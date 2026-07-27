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
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"

#include "esp_camera.h"

static const char *TAG = "example:take_picture";

static esp_err_t camera_init(uint32_t xclk_freq_hz, pixformat_t pixel_format, framesize_t frame_size, uint8_t fb_count)
{
    camera_config_t camera_config = {
        // S3 LCD EV
        .pin_pwdn = 1,
        .pin_reset = -1,
        .pin_xclk = 4,
        .pin_sscb_sda = 17,
        .pin_sscb_scl = 18,
        .pin_d1 = -1,
        .pin_d0 = 13,
        .pin_vsync = -1,
        .pin_cs = 10,
        .pin_pclk = 14,

        .xclk_freq_hz = xclk_freq_hz,
        .ledc_timer = LEDC_TIMER_0, // // This is only valid on ESP32/ESP32-S2. ESP32-S3 use LCD_CAM interface.
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = pixel_format, //YUV422,GRAYSCALE,RGB565,JPEG
        .frame_size = frame_size,
        .fb_count = fb_count,       // For ESP32/ESP32-S2, if more than one, i2s runs in continuous mode. Use only with JPEG.
        .grab_mode = CAM_CONTINUOUS_FRAME_GRAB_WHEN_EMPTY_MODE,
        .fb_location = CAMERA_FB_IN_PSRAM
    };

    //initialize the camera
    esp_err_t ret = esp_spi_cam_init(&camera_config);
    // sensor_t *s = esp_camera_sensor_get();
    // s->set_vflip(s, 1);//flip it back

    return ret;
}

static bool camera_test_fps(uint16_t times, float *fps, uint32_t *size)
{
    *fps = 0.0f;
    *size = 0;
    uint32_t s = 0;
    uint32_t num = 0;
    uint64_t total_time = esp_timer_get_time();
    for (size_t i = 0; i < times; i++) {
        camera_fb_t *pic = esp_spi_cam_take(5000 / portTICK_PERIOD_MS);
        if (NULL == pic) {
            ESP_LOGW(TAG, "fb get failed");
            return 0;
        } else {
            s += pic->len;
            num++;
        }
        esp_spi_cam_give(pic);
    }
    total_time = esp_timer_get_time() - total_time;
    if (num) {
        *fps = num * 1000000.0f / total_time ;
        *size = s / num;
    }
    return 1;
}

static void __attribute__((noreturn)) task_fatal_error(void)
{
    ESP_LOGE(TAG, "Exiting task due to fatal error...");
    (void)vTaskDelete(NULL);

    while (1) {
        ;
    }
}

static void camera_performance_test_with_format(uint32_t xclk_freq, uint32_t pic_num, pixformat_t pixel_format, framesize_t frame_size)
{
    esp_err_t ret = ESP_OK;
    // //detect sensor information
    // TEST_ESP_OK(init_camera(10000000, pixel_format, frame_size, 2));
    // sensor_t *s = esp_camera_sensor_get();
    // camera_sensor_info_t *info = esp_camera_sensor_get_info(&s->id);
    // TEST_ASSERT_NOT_NULL(info);
    // TEST_ESP_OK(esp_camera_deinit());
    // vTaskDelay(500 / portTICK_RATE_MS);
    struct fps_result {
        float fps;
        uint32_t size;
    };
    struct fps_result results = {0};
    
    ret = camera_init(xclk_freq, pixel_format, frame_size, 2);
    vTaskDelay(100 / portTICK_RATE_MS);
    if (ESP_OK != ret) {
        ESP_LOGW(TAG, "Testing init failed :-(, skip this item");
        task_fatal_error();
    }
    camera_test_fps(pic_num, &results.fps, &results.size);
    if(esp_spi_cam_deinit() != ESP_OK) {
        ESP_LOGW(TAG, "deinit fail");
    }

    printf("FPS Result\n");
    printf("fps, size \n");
    
    printf("%5.2f,     %7d \n",
            results.fps, results.size);

    printf("----------------------------------------------------------------------------------------\n");
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

    camera_performance_test_with_format(40 * 1000000, 100, PIXFORMAT_GRAYSCALE, FRAMESIZE_QVGA);  // FRAMESIZE_QVGA  FRAMESIZE_200X240
}
