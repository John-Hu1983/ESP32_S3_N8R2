#include "lvgl_port.h"

#include <stddef.h>

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "config.h"
#include "lcd_st7365p.h"

#define TAG "lvgl_port"
#define LVGL_TICK_PERIOD_MS 1
#define LVGL_DRAW_BUF_ROWS LCD_DMA_ROWS

static lv_disp_draw_buf_t lvgl_draw_buffer;
static lv_disp_drv_t lvgl_disp_drv;
static lv_color_t *lvgl_buf_1;
static lv_color_t *lvgl_buf_2;
static esp_timer_handle_t lvgl_tick_timer;

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    const uint16_t width = (uint16_t)(area->x2 - area->x1 + 1);
    const uint16_t height = (uint16_t)(area->y2 - area->y1 + 1);
    const size_t byte_count = (size_t)width * height * sizeof(lv_color_t);

    const esp_err_t error = lcd_st7365p_draw_area_rgb565_be((uint16_t)area->x1,
                                                            (uint16_t)area->y1,
                                                            width,
                                                            height,
                                                            (const uint8_t *)color_p,
                                                            byte_count);
    if (error != ESP_OK)
    {
        ESP_LOGE(TAG, "flush failed: %s", esp_err_to_name(error));
    }

    lv_disp_flush_ready(disp_drv);
}

esp_err_t lvgl_port_init(void)
{
    lv_init();

    const size_t draw_pixels = (size_t)LCD_WIDTH * LVGL_DRAW_BUF_ROWS;
    const size_t draw_buf_bytes = draw_pixels * sizeof(lv_color_t);

    lvgl_buf_1 = heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    ESP_RETURN_ON_FALSE(lvgl_buf_1 != NULL, ESP_ERR_NO_MEM, TAG, "draw buffer 1 alloc failed");

    lvgl_buf_2 = heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (lvgl_buf_2 == NULL)
    {
        ESP_LOGW(TAG, "draw buffer 2 alloc failed, using single-buffer mode");
    }

    lv_disp_draw_buf_init(&lvgl_draw_buffer, lvgl_buf_1, lvgl_buf_2, draw_pixels);

    lv_disp_drv_init(&lvgl_disp_drv);
    lvgl_disp_drv.hor_res = LCD_WIDTH;
    lvgl_disp_drv.ver_res = LCD_HEIGHT;
    lvgl_disp_drv.flush_cb = lvgl_flush_cb;
    lvgl_disp_drv.draw_buf = &lvgl_draw_buffer;
    ESP_RETURN_ON_FALSE(lv_disp_drv_register(&lvgl_disp_drv) != NULL,
                        ESP_FAIL,
                        TAG,
                        "lv_disp_drv_register failed");

    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&tick_timer_args, &lvgl_tick_timer), TAG, "esp_timer_create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000U), TAG, "esp_timer_start_periodic failed");

    ESP_LOGI(TAG,
             "LVGL init: %dx%d draw_buf=%u bytes mode=%s",
             LCD_WIDTH,
             LCD_HEIGHT,
             (unsigned)draw_buf_bytes,
             lvgl_buf_2 != NULL ? "double" : "single");

    return ESP_OK;
}
