#ifndef LCD_ST7365P_H
#define LCD_ST7365P_H

#include <stdint.h>

#include "esp_err.h"

#define LCD_CMD_SWRESET 0x01
#define LCD_CMD_SLPOUT  0x11
#define LCD_CMD_INVOFF  0x20
#define LCD_CMD_INVON   0x21
#define LCD_CMD_COLMOD  0x3A
#define LCD_CMD_MADCTL  0x36
#define LCD_CMD_CASET   0x2A
#define LCD_CMD_RASET   0x2B
#define LCD_CMD_RAMWR   0x2C
#define LCD_CMD_DISPON  0x29

#define LCD_DIR_VERTICAL_0     0
#define LCD_DIR_VERTICAL_180   1
#define LCD_DIR_HORIZONTAL_0   2
#define LCD_DIR_HORIZONTAL_180 3

#define LCD_MADCTL_MY  0x80
#define LCD_MADCTL_MX  0x40
#define LCD_MADCTL_MV  0x20
#define LCD_MADCTL_BGR 0x08

esp_err_t lcd_st7365p_init(void);
uint16_t lcd_st7365p_get_width(void);
uint16_t lcd_st7365p_get_height(void);
esp_err_t lcd_st7365p_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
esp_err_t lcd_st7365p_draw_image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *image_rgb565);
esp_err_t lcd_st7365p_draw_demo(void);

#endif