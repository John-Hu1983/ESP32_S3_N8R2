#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "dev/lcd_st7365p.h"
#include "image/code/image_assets.h"

#define TAG "lcd_demo"

/*
 * Application entry point.
 * Initialize the LCD once, clear the screen, then loop through all generated
 * image assets and display each one for 3 seconds.
 */
void app_main(void)
{
    ESP_LOGI(TAG, "ST7365P LCD SPI DMA demo start");
    ESP_LOGI(TAG, "RS=IO%d SDA/MOSI=IO%d SCL/SCLK=IO%d CS=IO%d RESET=IO%d dir=%d %dx%d",
             LCD_GPIO_RS, LCD_GPIO_SDA, LCD_GPIO_SCL, LCD_GPIO_CS, LCD_GPIO_RESET,
             LCD_DIRECTION, lcd_st7365p_get_width(), lcd_st7365p_get_height());

    ESP_ERROR_CHECK(lcd_st7365p_init());
    ESP_ERROR_CHECK(lcd_st7365p_fill_rect(0, 0, lcd_st7365p_get_width(), lcd_st7365p_get_height(), 0x0000));

    while (1)
    {
        for (uint16_t index = 0; index < IMAGE_ASSET_COUNT; index++)
        {
            const image_rgb565_t *image = &image_assets[index];
            const uint16_t x = image->width < lcd_st7365p_get_width() ? (lcd_st7365p_get_width() - image->width) / 2 : 0;
            const uint16_t y = image->height < lcd_st7365p_get_height() ? (lcd_st7365p_get_height() - image->height) / 2 : 0;

            ESP_LOGI(TAG, "Display image %d: %dx%d", index + 1, image->width, image->height);
            ESP_ERROR_CHECK(lcd_st7365p_draw_image(x, y, image->width, image->height, image->data));
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
}