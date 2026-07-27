/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "time.h"
#include "sys/time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "driver/gpio.h"
#include "driver/spi_slave_hd.h"
#include "esp_system.h"
#include "esp_err.h"

#include "nvs_flash.h"
#include "nvs.h"
#include "sensor.h"
#include "sccb.h"
#include "esp_camera.h"
#include "xclk.h"

#if CONFIG_BF20A6_SUPPORT
#include "bf20a6.h"
#endif

#if CONFIG_BF30A2_SUPPORT
#include "bf30a2.h"
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define TAG ""
#else
#include "esp_log.h"
static const char *TAG = "spi_cam";
#endif

typedef enum {
    CAM_STATE_IDLE = 0,
    CAM_STATE_READ_BUF = 1, // work mode
} spi_cam_driver_state_t;

typedef struct trans_link_s {
    spi_slave_hd_data_t trans;
    struct trans_link_s *next;
    bool recycled;      //1: the current transaction descriptor is processed by the HW already, it is available and can be reused for new transaction
}trans_link_t;

typedef struct {
    sensor_t sensor;
    uint32_t fb_count;                   /* This is the num of bufs required by the driver, which is （1 + available in the application layer）. */
    camera_fb_t *fb_array;               // s_fb_buf_array[FRAME_NUM]
    spi_cam_driver_state_t driver_status;
    QueueHandle_t frame_buffer_queue;
    trans_link_t *rx_curr_trans;         /* Pointer to the current transaction */
    TaskHandle_t spi_cam_task_handle;
    uint32_t fb_size;                    /* frame buffer len */
} spi_cam_status_t;

// SPI CAM
/********************* 240 * 320 *****************/
#define IMAGE_LINE_APPEND_CHAR_LEN  12
#define SPI_DMA_MAX_LEN             4092
#define QUEUE_SIZE                  40      // number of DMA descriptors. If the data receiving cannot be continuous, try to increase QUEUE_SIZE
#define TOTAL_SIZE                  80640   // image size(including line header)
#define DMA_TRANS_LEN               3840    // DMA transfer size

#define RCV_HOST    SPI2_HOST

#define TEST_ESP_OK(ret) assert(ret == ESP_OK)

// volatile int recv_eof_block = 0;
static DRAM_ATTR spi_cam_status_t *s_state = NULL;

#define CAMERA_ENABLE_OUT_CLOCK(v) camera_enable_out_clock((v))
#define CAMERA_DISABLE_OUT_CLOCK() camera_disable_out_clock()

#define CAM_CHECK(a, str, ret) if (!(a)) {                                          \
        ESP_LOGE(TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str);                    \
        return (ret);                                                               \
        }

#define CAM_CHECK_GOTO(a, str, lab) if (!(a)) {                                     \
        ESP_LOGE(TAG,"%s(%d): %s", __FUNCTION__, __LINE__, str);                    \
        goto lab;                                                                   \
        }

typedef struct {
    int (*detect)(int slv_addr, sensor_id_t *id);
    int (*init)(sensor_t *sensor);
} sensor_func_t;

static const sensor_func_t g_sensors[] = {
#if CONFIG_BF20A6_SUPPORT
    {bf20a6_detect, bf20a6_init},
#endif
#if CONFIG_BF30A2_SUPPORT
    {bf30a2_detect, bf30a2_init},
#endif
};
static uint8_t *s_rx_buffer[QUEUE_SIZE];

#define SPI_CS_CTAL_DEBUG_ON 1
#if SPI_CS_CTAL_DEBUG_ON
#define SPI_CS_CTAL_PIN (2)     // Change to your CS control GPIO
void test_gpio_init(void)
{
    //Configuration for the handshake line
    gpio_config_t io_conf={
        .intr_type=GPIO_INTR_DISABLE,
        .mode=GPIO_MODE_OUTPUT,
        .pin_bit_mask=(1 << SPI_CS_CTAL_PIN)
    };

    //Configure handshake line as output
    gpio_config(&io_conf);
    gpio_set_level(SPI_CS_CTAL_PIN, 1);

    // // debug
    // io_conf.pin_bit_mask=(1 << 12);
    // gpio_config(&io_conf);
    // gpio_set_level(12, 0);
    // //
}

/*helper for debug. display buffer in hex on a line*/
static void spi_camera_disp_buf(uint8_t* buf, uint32_t len)
{
    int i;
    assert(buf != NULL);
    for (i = 0; i < len; i++) {
        printf("%02x ", buf[i]);//when finished the test, the printf() will be change to ESP_LOGD();
        if ((i + 1) % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}
#endif

static void spi_bus_default_config(spi_bus_config_t* bus_cfg, const camera_config_t *config)
{
    bus_cfg->mosi_io_num = config->pin_d0;
    bus_cfg->miso_io_num = -1;
    bus_cfg->sclk_io_num = config->pin_pclk;
    bus_cfg->quadwp_io_num = -1;
    bus_cfg->quadhd_io_num = -1;
    bus_cfg->max_transfer_sz = 4096 * QUEUE_SIZE;   // need how much DMA
}

static void spi_slot_default_config(spi_slave_hd_slot_config_t* slave_hd_cfg, const camera_config_t *config)
{
    slave_hd_cfg->spics_io_num = config->pin_cs;    // connect CS pin to GND
    slave_hd_cfg->flags |= SPI_SLAVE_HD_APPEND_MODE;
    slave_hd_cfg->mode = 1;
    slave_hd_cfg->queue_size = QUEUE_SIZE;
    slave_hd_cfg->dma_chan = SPI_DMA_CH_AUTO;
#if ESP_IDF_VERSION_MAJOR < 5
    slave_hd_cfg->ms_data_bitlen = 32 * 1024 * 8;   // intr trigger length, in bits.  set to max.
#endif
}

static void init_slave_hd(const camera_config_t *config)
{
    spi_bus_config_t bus_cfg = {};
    spi_bus_default_config(&bus_cfg, config);

    spi_slave_hd_slot_config_t slave_hd_cfg = {};
    spi_slot_default_config(&slave_hd_cfg, config);

    ESP_ERROR_CHECK(spi_slave_hd_init(RCV_HOST, &bus_cfg, &slave_hd_cfg));
}

//Create a link to the transaction descriptors, malloc the transaction buffers
static esp_err_t create_transaction_pool(uint8_t **data_buf, trans_link_t *trans_link, uint16_t times)
{
    for (int i = 0; i < times; i++) {
        //malloc data buffers for transaction
        // data_buf[i] = heap_caps_calloc(1, s_state->fb_transaction_len, MALLOC_CAP_DMA);
        data_buf[i] = heap_caps_calloc(1, DMA_TRANS_LEN, MALLOC_CAP_DMA);
        if (!data_buf[i]) {
            ESP_LOGI("Create pool:", "No enough memory");
            return ESP_ERR_NO_MEM;
        }
        // printf("size %d\n", s_state->fb_transaction_len);

        //attach data buffer and transaction descriptor
        trans_link[i].trans.data = data_buf[i];

        //link the recycling transaction descriptors
        if (i != QUEUE_SIZE - 1) {
            trans_link[i].next = &trans_link[i+1];
        } else {
            trans_link[i].next = &trans_link[0];
        }

        //init transaction descriptor as available
        trans_link[i].recycled = 1;
    }
    return ESP_OK;
}

static bool get_rx_transaction_descriptor(trans_link_t **out_trans)
{
    if (s_state->rx_curr_trans->recycled == 0) {
        return false;
    }
    // s_state->rx_curr_trans->trans.len = s_state->fb_transaction_len;
    s_state->rx_curr_trans->trans.len = DMA_TRANS_LEN;
    *out_trans = s_state->rx_curr_trans;
    s_state->rx_curr_trans = s_state->rx_curr_trans->next;
    return true;
}

void recvTask(void *arg)
{
    trans_link_t trans_link[QUEUE_SIZE] = {};
    trans_link_t *trans_for_recv;   //The transaction to receive data, should get from ``get_rx_transaction_descriptor``
    s_state->rx_curr_trans = trans_link;
    camera_fb_t *frame_buf_ptr = NULL;

    //
    // debug
    static int printf_cnt = 0;
    //

    // init slave driver
    // init_slave_hd();
    xQueueReset(s_state->frame_buffer_queue);

    ESP_ERROR_CHECK(create_transaction_pool(s_rx_buffer, trans_link, QUEUE_SIZE));
    //This variable is used to check if you're using transaction descriptors more than you prepared in the pool
    bool get_desc_success = false;

    // gpio_set_level(SPI_CS_CTAL_PIN, 1);

    for (int i = 0; i < QUEUE_SIZE; i++) {
        get_desc_success = get_rx_transaction_descriptor(&trans_for_recv);
        if (get_desc_success) {
            ESP_ERROR_CHECK(spi_slave_hd_append_trans(RCV_HOST, SPI_SLAVE_CHAN_RX, &trans_for_recv->trans, portMAX_DELAY));
        }
    }

    gpio_set_level(SPI_CS_CTAL_PIN, 0);

    // start camera
    s_state->sensor.set_special_effect(&s_state->sensor, 0xb0);  // enable output for BF30A2

    /**************Note: must delay, you can see this delay in dataline.**************
    Please adjust the delay size here based on the actual online situation.**************
    */
    ESP_LOGD(TAG, "begin spi recv task");
    vTaskDelay(50 / portTICK_PERIOD_MS);  // 150 - 250

    /****************Note: can't not use too mush printf() in while below.**************
    *****************Note: Delays must never be used in the following while.**************
    *use s_fb_buf_array[0] to recv data and when one complete image be recved, just copy to one framebuffer.
    */
    while (1) {
        spi_slave_hd_data_t *ret_trans;
        trans_link_t *ret_link;

        // Get the transaction descriptor that is already procecssed by the HW and can be recycled
        spi_slave_hd_get_append_trans_res(RCV_HOST, SPI_SLAVE_CHAN_RX, &ret_trans, portMAX_DELAY);
        // In append mode, always use the descriptor returned by the driver.
        ret_link = (trans_link_t *)ret_trans;
        ret_link->recycled = 1;

        // uint8_t *tmpp = ret_link->trans.data;
        // esp_rom_printf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", 
        //                 tmpp[0], tmpp[1], tmpp[2], tmpp[3], tmpp[4], tmpp[5], tmpp[6], tmpp[7], tmpp[8], tmpp[9], tmpp[10], tmpp[11], tmpp[12], tmpp[13], tmpp[14], tmpp[15]);

        size_t copy_len = ret_link->trans.trans_len;
        if (copy_len == 0) {
            goto append_next;
        }
        if (s_state->fb_array[0].len + copy_len > TOTAL_SIZE) {
            copy_len = TOTAL_SIZE - s_state->fb_array[0].len;
        }
        memcpy(s_state->fb_array[0].buf + s_state->fb_array[0].len, ret_link->trans.data, copy_len);
        s_state->fb_array[0].len += copy_len;

        if (s_state->fb_array[0].len >= TOTAL_SIZE) {
            s_state->fb_array[0].len = 0; // restart to recv another frame.
            for(int i=1; i<s_state->fb_count;i++) { // Note, s_state->fb_array[0] used to receive continuous data, index begin in 1.
                if(s_state->fb_array[i].en) {
                    memcpy(s_state->fb_array[i].buf, s_state->fb_array[0].buf, TOTAL_SIZE);
                    s_state->fb_array[i].en = 0;
                    frame_buf_ptr = &s_state->fb_array[i];
                    if(xQueueSend(s_state->frame_buffer_queue, (void *)&frame_buf_ptr, 0) != pdTRUE) {
                        ESP_LOGE(TAG, "send queue fail");
                    }
                    // just save to one buffer
                    break;
                }
            }
        }

        printf_cnt++;
        if (printf_cnt >= QUEUE_SIZE) {
            printf_cnt = 0;
        }

append_next:
        get_desc_success = get_rx_transaction_descriptor(&trans_for_recv);
        if (get_desc_success) {
            // ESP_LOGW(TAG, "append");
            ESP_ERROR_CHECK(spi_slave_hd_append_trans(RCV_HOST, SPI_SLAVE_CHAN_RX, &trans_for_recv->trans, portMAX_DELAY));
        } else {
            ESP_LOGE(TAG, "get desc fail, ind: %d\n", printf_cnt);
        }
    }
    // never come here    
    spi_slave_hd_deinit(RCV_HOST);
    vTaskDelete(NULL);
}

camera_fb_t *esp_spi_cam_take(TickType_t timeout)
{
    if (s_state == NULL) {
        return NULL;
    }
    camera_fb_t *frame_buf_ptr = NULL;
    xQueueReceive(s_state->frame_buffer_queue, (void *)&frame_buf_ptr, timeout);
    if (frame_buf_ptr) {
        frame_buf_ptr->len = s_state->fb_size;
        return frame_buf_ptr;
    } else {
        ESP_LOGW(TAG, "Failed to get the frame on time!");
    }
    return NULL;
}

void esp_spi_cam_give(camera_fb_t *cam_frame)
{
    if (s_state == NULL) {
        return;
    }

    if(cam_frame) {
        cam_frame->len = 0;
        cam_frame->en = 1;
    }
}

static esp_err_t camera_probe(const camera_config_t *config, camera_model_t *out_camera_model)
{
    *out_camera_model = CAMERA_NONE;
    if (s_state != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_state = (spi_cam_status_t *) calloc(sizeof(spi_cam_status_t), 1);
    if (!s_state) {
        return ESP_ERR_NO_MEM;
    }

    if (config->pin_pwdn >= 0) {
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << config->pin_pwdn;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        gpio_set_level(config->pin_pwdn, 0);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        ESP_LOGD(TAG, "Initializing PDN, set to 0");
    }

    if (config->pin_xclk >= 0) {
        ESP_LOGD(TAG, "Enabling XCLK output");
        CAMERA_ENABLE_OUT_CLOCK(config);
    }


    if (config->pin_sscb_sda != -1) {
        ESP_LOGD(TAG, "Initializing SSCB");
        SCCB_Init(config->pin_sscb_sda, config->pin_sscb_scl);
    }

    if (config->pin_pwdn >= 0) {
        // ESP_LOGW(TAG, "Resetting camera by power down line");
        // gpio_config_t conf = { 0 };
        // conf.pin_bit_mask = 1LL << config->pin_pwdn;
        // conf.mode = GPIO_MODE_OUTPUT;
        // gpio_config(&conf);

        // carefull, logic is inverted compared to reset pin
        ESP_LOGD(TAG, "PDN -> 1");
        gpio_set_level(config->pin_pwdn, 1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        ESP_LOGD(TAG, "PDN -> 0");
        gpio_set_level(config->pin_pwdn, 0);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    if (config->pin_reset >= 0) {
        ESP_LOGD(TAG, "Resetting camera");
        gpio_config_t conf = { 0 };
        conf.pin_bit_mask = 1LL << config->pin_reset;
        conf.mode = GPIO_MODE_OUTPUT;
        gpio_config(&conf);

        gpio_set_level(config->pin_reset, 0);
        vTaskDelay(10 / portTICK_PERIOD_MS);
        gpio_set_level(config->pin_reset, 1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    uint8_t slv_addr = SCCB_Probe();
    vTaskDelay(20 / portTICK_PERIOD_MS);

    if (slv_addr == 0) {
        ESP_LOGE(TAG, "camera_probe() -> SCCB_Probe() fail");
        CAMERA_DISABLE_OUT_CLOCK();
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Detected camera at address=0x%02x", slv_addr);
    s_state->sensor.slv_addr = slv_addr;
    s_state->sensor.xclk_freq_hz = config->xclk_freq_hz;

    /**
     * Read sensor ID and then initialize sensor
     * Attention: Some sensors have the same SCCB address. Therefore, several attempts may be made in the detection process
     */
    sensor_id_t *id = &s_state->sensor.id;
    for (size_t i = 0; i < sizeof(g_sensors) / sizeof(sensor_func_t); i++) {
        if (g_sensors[i].detect(slv_addr, id)) {
            camera_sensor_info_t *info = esp_camera_sensor_get_info(id);
            if (NULL != info) {
                *out_camera_model = info->model;
                ESP_LOGI(TAG, "Detected %s camera", info->name);
                g_sensors[i].init(&s_state->sensor);
                break;
            }
        }
    }

    if (CAMERA_NONE == *out_camera_model) { //If no supported sensors are detected
        CAMERA_DISABLE_OUT_CLOCK();
        ESP_LOGE(TAG, "Detected camera not supported.");
        return ESP_ERR_NOT_SUPPORTED;
    }

    ESP_LOGI(TAG, "Camera PID=0x%02x VER=0x%02x MIDL=0x%02x MIDH=0x%02x",
             id->PID, id->VER, id->MIDH, id->MIDL);

    ESP_LOGD(TAG, "Doing SW reset of sensor");
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    s_state->sensor.reset(&s_state->sensor);  // init regs

    return ESP_OK;
}

esp_err_t spi_camera_config(const camera_config_t *config)
{
    s_state->frame_buffer_queue = xQueueCreate(s_state->fb_count - 1, sizeof(camera_fb_t *));
    if(s_state->frame_buffer_queue == NULL) {
        ESP_LOGE(TAG, "Queue create fail");
        return ESP_FAIL;
    }

    // init slave driver
    init_slave_hd(config);
    ESP_LOGD(TAG, "init_slave_hd OK");

    xTaskCreate(recvTask, "recvTask", 4096, NULL, 15, &s_state->spi_cam_task_handle);

// #if CONFIG_CAMERA_CORE0
//     xTaskCreatePinnedToCore(recvTask, "recvTask", 4096, NULL, 10, &s_state->spi_cam_task_handle, 0);
// #elif CONFIG_CAMERA_CORE1
//     xTaskCreatePinnedToCore(recvTask, "recvTask", 4096, NULL, 10, &s_state->spi_cam_task_handle, 1);
// #else
//     xTaskCreate(recvTask, "recvTask", 4096, NULL, 10, &s_state->spi_cam_task_handle);
// #endif
    return ESP_OK;
}

esp_err_t esp_spi_cam_init(const camera_config_t *config)
{
    esp_err_t err;
    camera_model_t camera_model = CAMERA_NONE;
    uint32_t _caps = MALLOC_CAP_8BIT;
    uint8_t in_bytes_per_pixel = 1;

    test_gpio_init();
    // vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    // probe camera
    err = camera_probe(config, &camera_model);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera probe failed with error 0x%x(%s)", err, esp_err_to_name(err));
        goto fail;
    }

    framesize_t frame_size = (framesize_t) config->frame_size;
    pixformat_t pix_format = (pixformat_t) config->pixel_format;

    if (frame_size > camera_sensor[camera_model].max_size) {
        ESP_LOGW(TAG, "The frame size exceeds the maximum for this sensor, it will be forced to the maximum possible value");
        frame_size = camera_sensor[camera_model].max_size;
    }

    s_state->sensor.status.framesize = frame_size;
    s_state->sensor.pixformat = pix_format;
    ESP_LOGW(TAG, "Setting frame size to %dx%d", resolution[frame_size].width, resolution[frame_size].height);

    // To ensure real-time performance, an additional buffer is always used here to receive continuous data.
    s_state->fb_count = config->fb_count + 1;
    s_state->fb_size = TOTAL_SIZE;
    s_state->fb_array = (camera_fb_t *)heap_caps_calloc(1, s_state->fb_count * sizeof(camera_fb_t), MALLOC_CAP_DEFAULT);
    CAM_CHECK(s_state->fb_array != NULL, "frames malloc failed", ESP_FAIL);
    ESP_LOGD(TAG, "sizeof(camera_fb_t): %d", sizeof(camera_fb_t));
    for (size_t i = 0; i < s_state->fb_count; i++) {
        ESP_LOGD(TAG, "s_state->fb_array[%d] addr: %p", i, &(s_state->fb_array[i]));
    }

    if (CAMERA_FB_IN_DRAM == config->fb_location) {
        _caps |= MALLOC_CAP_INTERNAL;
    } else {
        _caps |= MALLOC_CAP_SPIRAM;
    }
    for (size_t i = 0; i < s_state->fb_count; i++) {
        // s_state->fb_array[i].buf = (uint8_t *)heap_caps_malloc(s_state->fb_size, _caps);
        s_state->fb_array[i].buf = (uint8_t *)heap_caps_malloc(TOTAL_SIZE, _caps);
        CAM_CHECK(s_state->fb_array[i].buf != NULL, "fb malloc failed", ESP_FAIL);
        ESP_LOGD(TAG, "fb_array[%d].data_addr=%p", i, s_state->fb_array[i].buf);
        s_state->fb_array[i].len = 0;
        s_state->fb_array[i].format = (pixformat_t) config->pixel_format;
        s_state->fb_array[i].height = resolution[frame_size].height;
        s_state->fb_array[i].width = resolution[frame_size].width;  // + line header
        s_state->fb_array[i].en = 1;
    }

    spi_camera_config(config);

    if (s_state->sensor.set_framesize(&s_state->sensor, frame_size) != 0) {
        ESP_LOGE(TAG, "Failed to set frame size");
        err = ESP_ERR_CAMERA_FAILED_TO_SET_FRAME_SIZE;
        goto fail;
    }
    ESP_LOGD(TAG, "set_framesize ok");
    s_state->sensor.set_pixformat(&s_state->sensor, pix_format);
    ESP_LOGD(TAG, "set_pixformat ok");

    s_state->sensor.init_status(&s_state->sensor);
    ESP_LOGD(TAG, "init_status ok");

    ESP_LOGI(TAG, "cam init OK");
    return ESP_OK;

fail:
    esp_spi_cam_deinit();
    return err;
}

esp_err_t esp_spi_cam_deinit(void)
{
    if (s_state == NULL) {
        return ESP_OK;
    }

    CAMERA_DISABLE_OUT_CLOCK();

    if (s_state->spi_cam_task_handle) {
        vTaskDelete(s_state->spi_cam_task_handle);
    }

    spi_slave_hd_deinit(RCV_HOST);

    if (s_state->frame_buffer_queue) {
        vQueueDelete(s_state->frame_buffer_queue);
    }
    
    for (int x = 0; x < QUEUE_SIZE; x++) {
        if (s_rx_buffer[x]) {
            free(s_rx_buffer[x]);
        }
    }

    if (s_state->fb_array) {
        for (int x = 0; x < s_state->fb_count; x++) {
            if (s_state->fb_array[x].buf) {
                free(s_state->fb_array[x].buf);
            }
        }   
        free(s_state->fb_array);
    }

    SCCB_Deinit();

    free(s_state);
    s_state = NULL;

    return ESP_OK;
}

sensor_t *esp_spi_cam_sensor_get(void)
{
    if (s_state == NULL) {
        return NULL;
    }
    return &s_state->sensor;
}
