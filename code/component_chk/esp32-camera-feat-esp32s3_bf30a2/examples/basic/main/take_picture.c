#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include <unistd.h>
#include <dirent.h>
#include "esp_camera.h"

static const char *TAG = "[CAMERA]";

#define BF30A2_DEFAULT_SDA_IO   GPIO_NUM_5
#define BF30A2_DEFAULT_SCL_IO   GPIO_NUM_6
#define BF30A2_DEFAULT_VCLK_IO  GPIO_NUM_10
#define BF30A2_DEFAULT_SD_IO    GPIO_NUM_8
#define BF30A2_EXTERNAL_XCLK_HZ 24000000
/*
 * BF30A2 has no sensor-side CS pin.
 * This GPIO is the local CS input used by ESP32 SPI slave logic.
 * Keep it pulled low (board pull-down or direct GND strap).
 */
#define BF30A2_DEFAULT_LOCAL_CS_IO GPIO_NUM_9

static void prepare_local_cs_input(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BF30A2_DEFAULT_LOCAL_CS_IO),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "local CS input setup failed: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "local CS input GPIO=%d with internal pulldown", (int)BF30A2_DEFAULT_LOCAL_CS_IO);
}

static esp_err_t camera_init(pixformat_t pixel_format, framesize_t frame_size, uint8_t fb_count)
{
    camera_config_t camera_config = {
        // BF30A2 custom wiring (external 24MHz XCLK oscillator, PWDN tied to GND)
        .pin_pwdn = -1,
        .pin_reset = -1,
        .pin_xclk = -1, // XCLK is driven by external active oscillator
        .pin_sscb_sda = BF30A2_DEFAULT_SDA_IO,
        .pin_sscb_scl = BF30A2_DEFAULT_SCL_IO,
        .pin_d1 = -1,
        .pin_d0 = BF30A2_DEFAULT_SD_IO,
        .pin_vsync = -1,
        .pin_cs = BF30A2_DEFAULT_LOCAL_CS_IO,
        .pin_pclk = BF30A2_DEFAULT_VCLK_IO,

        .pixel_format = pixel_format,
        .frame_size = frame_size,
        .fb_count = fb_count,       // For ESP32/ESP32-S2, if more than one, i2s runs in continuous mode. Use only with JPEG.
        .grab_mode = CAM_CONTINUOUS_FRAME_GRAB_WHEN_EMPTY_MODE,
        .fb_location = CAMERA_FB_IN_PSRAM
    };

    //initialize the camera
    esp_err_t ret = esp_spi_cam_init(&camera_config);

    return ret;
}

#define FRAME_SIZES FRAMESIZE_QVGA
#define PIXFORMATS  PIXFORMAT_GRAYSCALE

void getPicTask(void *arg)
{
    // debug
    static int debug_cnt = 0;

    while (1) {
        // ESP_LOGI(TAG, "Taking picture...");
        camera_fb_t *pic = esp_spi_cam_take(2000 / portTICK_PERIOD_MS);

        // use pic->buf to access the image
        if(pic) {
            ESP_LOGI(TAG, "Picture taken! Cnt [%d], Its size was: %zu bytes, struct addr: %p, data addr: %p", debug_cnt, pic->len, pic, pic->buf);
            debug_cnt++;
            esp_spi_cam_give(pic);
        }
    }

    ESP_LOGW(TAG, "DELETE get task");
    vTaskDelete(NULL);
}

void app_main()
{
    printf("sys init\n");
    ESP_LOGI(TAG, "External XCLK source: %d Hz", BF30A2_EXTERNAL_XCLK_HZ);
    ESP_LOGI(TAG, "Internal free heap size: %d bytes", esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "PSRAM    free heap size: %d bytes", esp_get_free_heap_size() - esp_get_free_internal_heap_size());
    ESP_LOGI(TAG, "Total    free heap size: %d bytes", esp_get_free_heap_size());

    prepare_local_cs_input();

    if (ESP_OK != camera_init(PIXFORMATS, FRAME_SIZES, 2)) {
        ESP_LOGE(TAG, "init camrea sensor fail");
        return;
    }

    xTaskCreate(getPicTask, "getPicTask", 4096, NULL, 10, NULL);
}
