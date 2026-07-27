/* SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */

#pragma once

#include "esp_err.h"
#include "driver/ledc.h"
#include "sensor.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sys/time.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration structure for camera initialization
 */
typedef enum {
    CAM_CONTINUOUS_FRAME_GRAB_WHEN_EMPTY_MODE = 0, /*!< Fills buffers when they are empty. Less resources but first 'fb_count' frames might be old */
    CAM_CONTINUOUS_FRAME_GRAB_LATEST,              /*!< Except when 1 frame buffer is used, queue will always contain the last 'fb_count' frames */
    CAM_SINGLE_FRAME_GRAB,
} camera_grab_mode_t;

/**
 * @brief Camera frame buffer location 
 */
typedef enum {
    CAMERA_FB_IN_PSRAM,         /*!< Frame buffer is placed in external PSRAM */
    CAMERA_FB_IN_DRAM           /*!< Frame buffer is placed in internal DRAM */
} camera_fb_location_t;

/**
 * @brief Configuration structure for camera initialization
 */
typedef struct {
    int pin_pwdn;                   /*!< GPIO pin for camera power down line */
    int pin_reset;                  /*!< GPIO pin for camera reset line */
    int pin_xclk;                   /*!< GPIO pin for camera XCLK line */
    int pin_sscb_sda;               /*!< GPIO pin for camera SDA line */
    int pin_sscb_scl;               /*!< GPIO pin for camera SCL line */

    int pin_d1;                     /*!< GPIO pin for camera D1 line */
    int pin_d0;                     /*!< GPIO pin for camera D0 line */
    int pin_vsync;                  /*!< GPIO pin for camera VSYNC line */
    int pin_cs;                     /*!< GPIO pin for camera SPI CS line */
    int pin_pclk;                   /*!< GPIO pin for camera PCLK line */

    int xclk_freq_hz;               /*!< Frequency of XCLK signal, in Hz. EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode */

    ledc_timer_t ledc_timer;        /*!< LEDC timer to be used for generating XCLK  */
    ledc_channel_t ledc_channel;    /*!< LEDC channel to be used for generating XCLK  */

    pixformat_t pixel_format;       /*!< Format of the pixel data: PIXFORMAT_ + YUV422|GRAYSCALE|RGB565|JPEG  */
    framesize_t frame_size;         /*!< Size of the output image: FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA  */

    size_t fb_count;                /*!< Number of frame buffers to be allocated. If more than one, then each frame will be acquired (double speed)  */
    camera_fb_location_t fb_location; /*!< The location where the frame buffer will be allocated */
    camera_grab_mode_t grab_mode;   /*!< When buffers should be filled */
} camera_config_t;

/**
 * @brief Data structure of camera frame buffer
 */
typedef struct {
    uint8_t * buf;              /*!< Pointer to the pixel data */
    size_t len;                 /*!< Length of the buffer in bytes */
    size_t width;               /*!< Width of the buffer in pixels */
    size_t height;              /*!< Height of the buffer in pixels */
    pixformat_t format;         /*!< Format of the pixel data */
    struct timeval timestamp;   /*!< Timestamp since boot of the first DMA buffer of the frame */
    volatile uint8_t en;                 /*!< Can be used for copying data */
} camera_fb_t;

#define ESP_ERR_CAMERA_BASE 0x20000
#define ESP_ERR_CAMERA_NOT_DETECTED             (ESP_ERR_CAMERA_BASE + 1)
#define ESP_ERR_CAMERA_FAILED_TO_SET_FRAME_SIZE (ESP_ERR_CAMERA_BASE + 2)
#define ESP_ERR_CAMERA_FAILED_TO_SET_OUT_FORMAT (ESP_ERR_CAMERA_BASE + 3)
#define ESP_ERR_CAMERA_NOT_SUPPORTED            (ESP_ERR_CAMERA_BASE + 4)

// typedef struct {
//     uint8_t *data;
//     uint32_t data_len; // recv data len
//     volatile  uint8_t en; // can be used for copying data
// } custom_cam_frame_t;

esp_err_t esp_spi_cam_init(const camera_config_t *config);
camera_fb_t *esp_spi_cam_take(TickType_t timeout);
void esp_spi_cam_give(camera_fb_t *cam_frame);
sensor_t *esp_spi_cam_sensor_get();
esp_err_t esp_spi_cam_deinit(void);

#ifdef __cplusplus
}
#endif

#include "img_converters.h"

