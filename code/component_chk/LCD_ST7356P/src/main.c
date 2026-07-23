#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "dev/lcd_st7365p.h"
#include "image_assets.h"

#define TAG "image_slideshow"
#define SLIDE_INTERVAL_MS 333

static void stop_on_error(const char *operation, esp_err_t error)
{
    ESP_LOGE(TAG, "%s failed: %s", operation, esp_err_to_name(error));
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void draw_centered_image(const image_rgb565_t *image, uint16_t index)
{
    const uint16_t panel_width = lcd_st7365p_get_width();
    const uint16_t panel_height = lcd_st7365p_get_height();
    const uint16_t draw_x = (panel_width > image->width) ? (uint16_t)((panel_width - image->width) / 2) : 0;
    const uint16_t draw_y = (panel_height > image->height) ? (uint16_t)((panel_height - image->height) / 2) : 0;
    esp_err_t error;
    //  error = lcd_st7365p_fill_rect(0, 0, panel_width, panel_height, 0x0000);
    // if (error != ESP_OK)
    // {
    //     stop_on_error("lcd_st7365p_fill_rect", error);
    // }

    error = lcd_st7365p_draw_image(draw_x, draw_y, image->width, image->height, image->data);
    if (error != ESP_OK)
    {
        stop_on_error("lcd_st7365p_draw_image", error);
    }

    ESP_LOGI(TAG, "Display image %u/%u", (unsigned)(index + 1), (unsigned)IMAGE_ASSET_COUNT);
}

/*
 * Application entry point.
 * Initialize LCD, then display one image every second.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Image slideshow start");

    esp_err_t error = lcd_st7365p_init();
    if (error != ESP_OK)
    {
        stop_on_error("lcd_st7365p_init", error);
    }

    if (IMAGE_ASSET_COUNT == 0)
    {
        ESP_LOGE(TAG, "No images found in image_assets");
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    uint16_t image_index = 0;
    lcd_st7365p_fill_rect(0, 0, lcd_st7365p_get_width(), lcd_st7365p_get_height(), 0x0000);
    while (1)
    {
        draw_centered_image(&image_assets[image_index], image_index);

        image_index++;
        if (image_index >= IMAGE_ASSET_COUNT)
        {
            image_index = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(SLIDE_INTERVAL_MS));
    }
}