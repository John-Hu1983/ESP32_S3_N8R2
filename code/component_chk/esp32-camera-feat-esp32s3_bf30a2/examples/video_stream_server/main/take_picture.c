// Copyright 2020-2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "protocol_examples_common.h"
#include "esp_camera.h"

#define TEST_ESP_OK(ret) assert(ret == ESP_OK)
#define TEST_ASSERT_NOT_NULL(ret) assert(ret != NULL)

static QueueHandle_t xQueueIFrame = NULL;

static const char *TAG = "video s_server";

esp_err_t start_stream_server(const QueueHandle_t frame_i, const bool return_fb);

esp_err_t start_pic_server(void);

static esp_err_t camera_init(uint32_t xclk_freq_hz, pixformat_t pixel_format, framesize_t frame_size, uint8_t fb_count)
{
    camera_config_t camera_config = {
        // S3 LCD EV
        .pin_pwdn = 1,
        .pin_reset = -1,
        .pin_xclk = 4,
        .pin_sscb_sda = 8,
        .pin_sscb_scl = 18,
        .pin_d1 = -1,
        .pin_d0 = 13,
        .pin_vsync = -1,
        .pin_cs = 10, // CS, connect to GND
        .pin_pclk = 14,

        .xclk_freq_hz = xclk_freq_hz,
        .ledc_timer = LEDC_TIMER_0, // // This is only valid on ESP32/ESP32-S2. ESP32-S3 use LCD_CAM interface.
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = pixel_format, //YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = frame_size,
        .fb_count = fb_count,       // don't less 1.
        .grab_mode = CAM_CONTINUOUS_FRAME_GRAB_WHEN_EMPTY_MODE,
        .fb_location = CAMERA_FB_IN_PSRAM
    };

    //initialize the camera
    esp_err_t ret = esp_spi_cam_init(&camera_config);
    // sensor_t *s = esp_camera_sensor_get();
    // s->set_vflip(s, 1);//flip it back

    return ret;
}

void app_main()
{
    // turn on/off led, camera power
    gpio_config_t io_config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = ((1ULL << GPIO_NUM_11) | (1ULL << GPIO_NUM_18) | (1ULL << GPIO_NUM_0)),
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_config);
    gpio_set_level(GPIO_NUM_11, 0);
    gpio_set_level(GPIO_NUM_18, 0);
    gpio_set_level(GPIO_NUM_0, 0);

    camera_fb_t *frame;
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    xQueueIFrame = xQueueCreate(2, sizeof(camera_fb_t *));

    /* It is recommended to use a camera sensor with JPEG compression to maximize the speed */
    TEST_ESP_OK(camera_init(20000000, PIXFORMAT_GRAYSCALE, FRAMESIZE_QVGA, 2));

    TEST_ESP_OK(start_stream_server(xQueueIFrame, true));

    ESP_LOGI(TAG, "Begin capture frame");
    
    while (true) {
        frame = esp_spi_cam_take(3000 / portTICK_PERIOD_MS);
        if (frame) {
            xQueueSend(xQueueIFrame, &frame, portMAX_DELAY);
        } 
    }
}
