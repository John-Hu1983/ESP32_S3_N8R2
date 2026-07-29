#ifndef CONFIG_H
#define CONFIG_H

#include "driver/spi_master.h"

#define LCD_GPIO_RS      2
#define LCD_GPIO_SDA     14
#define LCD_GPIO_SCL     19
#define LCD_GPIO_CS      1
#define LCD_GPIO_RESET   0

#define LCD_PANEL_WIDTH  320
#define LCD_PANEL_HEIGHT 480
#define LCD_DIRECTION    LCD_DIR_HORIZONTAL_0
#define LCD_MIRROR_X     1
#define LCD_MIRROR_Y     0
#define LCD_COLOR_INVERT 1

#ifndef LCD_BOOT_BG_COLOR
#define LCD_BOOT_BG_COLOR 0x0000
#endif

#if LCD_DIRECTION == LCD_DIR_HORIZONTAL_0 || LCD_DIRECTION == LCD_DIR_HORIZONTAL_180
#define LCD_WIDTH        LCD_PANEL_HEIGHT
#define LCD_HEIGHT       LCD_PANEL_WIDTH
#else
#define LCD_WIDTH        LCD_PANEL_WIDTH
#define LCD_HEIGHT       LCD_PANEL_HEIGHT
#endif

#define LCD_SPI_HOST     SPI2_HOST
#define LCD_SPI_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_DISPLAY_SIZE (LCD_WIDTH * LCD_HEIGHT * 2)
#define LCD_DMA_ROWS     20
#define LCD_DMA_PIXELS   (LCD_WIDTH * LCD_DMA_ROWS)

#endif