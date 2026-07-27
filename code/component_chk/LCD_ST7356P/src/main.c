#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "dev/lcd_st7365p.h"
#include "image_assets.h"

#define TAG "image_slideshow"
#define FRAME_INTERVAL_MS 50
#define GIF_GAP_MS 900

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

    ESP_LOGI(TAG, "Display frame %u", (unsigned)(index + 1));
}

static void clear_screen(void)
{
    esp_err_t error = lcd_st7365p_fill_rect(0, 0, lcd_st7365p_get_width(), lcd_st7365p_get_height(), 0x0000);
    if (error != ESP_OK)
    {
        stop_on_error("lcd_st7365p_fill_rect", error);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Image slideshow start");

    esp_err_t error = lcd_st7365p_init();
    if (error != ESP_OK)
    {
        stop_on_error("lcd_st7365p_init", error);
    }

    if (IMAGE_GIF_SET_COUNT == 0)
    {
        ESP_LOGE(TAG, "No GIF sets found in image_assets");
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    clear_screen();

    while (1)
    {
        for (uint16_t set_index = 0; set_index < IMAGE_GIF_SET_COUNT; set_index++)
        {
            const image_gif_set_t *set = &image_gif_sets[set_index];
            const char *set_name = (set->folder != NULL) ? set->folder : "unknown";

            if ((set->frames == NULL) || (set->frame_count == 0))
            {
                ESP_LOGW(TAG, "Skip GIF set %u/%u: invalid frame data", (unsigned)(set_index + 1), (unsigned)IMAGE_GIF_SET_COUNT);
                continue;
            }

            ESP_LOGI(
                TAG,
                "Play GIF set %u/%u: %s (%u frames)",
                (unsigned)(set_index + 1),
                (unsigned)IMAGE_GIF_SET_COUNT,
                set_name,
                (unsigned)set->frame_count);

            for (uint8_t times = 0; times < 3; times++)
            {
                ESP_LOGI(TAG, "Play GIF set %u/%u: %s (%u frames) - loop %u/2", (unsigned)(set_index + 1), (unsigned)IMAGE_GIF_SET_COUNT, set_name, (unsigned)set->frame_count, (unsigned)(times + 1));
                for (uint16_t frame_index = 0; frame_index < set->frame_count; frame_index++)
                {
                    draw_centered_image(&set->frames[frame_index], frame_index);
                    vTaskDelay(pdMS_TO_TICKS(FRAME_INTERVAL_MS));
                }
            }

            clear_screen();
            vTaskDelay(pdMS_TO_TICKS(GIF_GAP_MS));
        }
    }
}