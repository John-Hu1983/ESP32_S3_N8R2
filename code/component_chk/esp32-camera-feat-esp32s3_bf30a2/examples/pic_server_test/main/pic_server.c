// Copyright 2020-2022 Espressif Systems (Shanghai) PTE LTD
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
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_camera.h"

#include "esp_http_server.h"
#include "img_converters.h"
#include "sdkconfig.h"
#include "esp_log.h"

static httpd_handle_t pic_httpd = NULL;

static const char *TAG = "pic_s";
#if PIC_SERVER_DEBUG_ON
static uint8_t image[96*96*2] = {
    0x7e, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xf7, 0x80, 0xef, 0x80, 0xe5, 0x7c, 0x5d, 0x80, 0xd5, 0x7e, 0x4d, 0x81, 0xca, 0x7e, 0x3f, 0x80, 0xbc, 0x7b, 0x37, 0x7e, 0x32, 0x7e, 0x2e, 0x7d, 0x2a, 0x7f, 0x2a, 0x7e, 0x07, 0x79, 0x23, 0x7f, 0x22, 0x7b, 0x22, 0x7e, 0x21, 0x7b, 0x1f, 0x7d, 0x1c, 0x7c, 0x1d, 0x7d, 0x1d, 0x80, 0x9c, 0x7c, 0x1c, 0x7e, 
0x18, 0x7d, 0x1a, 0x7c, 0x1a, 0x7c, 0x1a, 0x7d, 0x16, 0x7a, 0x16, 0x7b, 0x16, 0x7c, 0x13, 0x7b, 0x14, 0x7c, 0x10, 0x78, 0x11, 0x7e, 0x0f, 0x79, 0x10, 0x7e, 0x0d, 0x7a, 0x0f, 0x7a, 0x0c, 0x7a, 0x0c, 0x7c, 0x10, 0x7a, 0x1e, 0x7f, 0x1f, 0x7c, 0x1e, 0x81, 0x9e, 0x7a, 0x17, 0x83, 0x97, 0x77, 0x1c, 0x83, 0x9b, 0x76, 0x21, 0x81, 0xa3, 0x75, 0x29, 0x85, 0xac, 0x75, 0x2f, 0x86, 0xb4, 0x72, 0x36, 0x87, 0xaf, 0x70, 0x35, 0x8f, 0xb7, 0x70, 0x62, 0x87, 0xeb, 0x76, 0x7e, 0x81, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80, 0xfe, 0x80};


void disp_image_buf(void* buf, uint32_t width, uint32_t total_size)
{
    int i;
    assert(buf != NULL);
    uint8_t *disp_buf = (uint8_t *)buf;
    
    ESP_LOGI(TAG, "The buffer data is as follows:");
    for (i = 0; i < total_size; i++) {
        printf("0x%02x ", disp_buf[i]);
        if ((i + 1) % width == 0) {
            printf("\n");
        }
    }
    printf("\n");
}
#endif

/* Handler to download a file kept on the server */
static esp_err_t pic_get_handler(httpd_req_t *req)
{
    camera_fb_t *frame = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    frame = esp_spi_cam_take(3000 / portTICK_PERIOD_MS);
    //
    if (frame == NULL) {
        return ESP_FAIL;
    }
    //

    if(frame) {
        // disp_image_buf(frame->buf, 32, 128);
    }

    if (frame->buf) {
        if (frame->format == PIXFORMAT_JPEG) {
            _jpg_buf = frame->buf;
            _jpg_buf_len = frame->len;
        } else if (!frame2jpg(frame, 50, &_jpg_buf, &_jpg_buf_len)) {
            ESP_LOGE(TAG, "JPEG compression failed");
            res = ESP_FAIL;
        }
    } else {
        res = ESP_FAIL;
    }

    if (res == ESP_OK) {
        res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        if (frame->format != PIXFORMAT_JPEG) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
    }

    ESP_LOGI(TAG, "pic len %d", _jpg_buf_len);
    esp_spi_cam_give(frame);

    if (res != ESP_OK) {
        ESP_LOGW(TAG, "exit pic server");
        return ESP_FAIL;
    }

    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

esp_err_t start_pic_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 5120;

    httpd_uri_t pic_uri = {
        .uri = "/pic",
        .method = HTTP_GET,
        .handler = pic_get_handler,
        .user_ctx = NULL
    };

    ESP_LOGI(TAG, "Starting pic server on port: '%d'", config.server_port);
    if (httpd_start(&pic_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(pic_httpd, &pic_uri);
        return ESP_OK;
    }
    
    return ESP_FAIL;
}