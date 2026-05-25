#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"

#define LCD_WIDTH  (480)
#define LCD_HEIGHT (272)

// Verify these GPIOs against the schematic before flashing.
#define PIN_LCD_PWR (7)
#define PIN_LCD_CS  (19)
#define PIN_LCD_DC  (8)
#define PIN_LCD_WR  (6)
#define PIN_LCD_RST (0)
#define PIN_LCD_RD  (-1)

#define PIN_LCD_D0 (15)
#define PIN_LCD_D1 (16)
#define PIN_LCD_D2 (38)
#define PIN_LCD_D3 (39)
#define PIN_LCD_D4 (40)
#define PIN_LCD_D5 (41)
#define PIN_LCD_D6 (42)
#define PIN_LCD_D7 (37)

#define LCD_CMD_COL_ADDR (0x2A)
#define LCD_CMD_ROW_ADDR (0x2B)
#define LCD_CMD_RAMWR    (0x2C)

static const char *TAG = "nv3041a_demo";

static esp_lcd_panel_io_handle_t s_io = NULL;

static void lcd_reset(void)
{
	if (PIN_LCD_RST < 0) {
		return;
	}

	gpio_set_level(PIN_LCD_RST, 0);
	vTaskDelay(pdMS_TO_TICKS(20));
	gpio_set_level(PIN_LCD_RST, 1);
	vTaskDelay(pdMS_TO_TICKS(120));
}

static void lcd_cmd(uint8_t cmd)
{
	ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_io, cmd, NULL, 0));
}

static void lcd_param(uint8_t cmd, const void *data, size_t len)
{
	ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_io, cmd, data, len));
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	uint8_t col_data[4] = {
		(uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
		(uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF),
	};
	uint8_t row_data[4] = {
		(uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
		(uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF),
	};

	lcd_param(LCD_CMD_COL_ADDR, col_data, sizeof(col_data));
	lcd_param(LCD_CMD_ROW_ADDR, row_data, sizeof(row_data));
}

static uint16_t color_swap(uint16_t color)
{
	return (uint16_t)((color << 8) | (color >> 8));
}

static void lcd_fill_rect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color)
{
	uint16_t x1 = (uint16_t)(x0 + w - 1);
	uint16_t y1 = (uint16_t)(y0 + h - 1);
	uint16_t *line = heap_caps_malloc(w * sizeof(uint16_t), MALLOC_CAP_DMA);
	if (!line) {
		ESP_LOGE(TAG, "line buffer alloc failed");
		return;
	}

	uint16_t swapped = color_swap(color);
	for (uint16_t x = 0; x < w; x++) {
		line[x] = swapped;
	}

	lcd_set_window(x0, y0, x1, y1);
	for (uint16_t y = 0; y < h; y++) {
		ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(s_io, LCD_CMD_RAMWR, line, w * 2));
	}

	free(line);
}

static void nv3041a_init_minimal(void)
{
	lcd_reset();

	lcd_cmd(0x11); // sleep out
	vTaskDelay(pdMS_TO_TICKS(120));

	uint8_t madctl = 0x00;
	lcd_param(0x36, &madctl, 1);

	uint8_t pix_fmt = 0x55; // RGB565
	lcd_param(0x3A, &pix_fmt, 1);

	lcd_cmd(0x29); // display on
	vTaskDelay(pdMS_TO_TICKS(20));
}

void app_main(void)
{
	if (PIN_LCD_PWR >= 0) {
		gpio_set_direction(PIN_LCD_PWR, GPIO_MODE_OUTPUT);
		gpio_set_level(PIN_LCD_PWR, 1);
		vTaskDelay(pdMS_TO_TICKS(10));
	}

	if (PIN_LCD_RST >= 0) {
		gpio_set_direction(PIN_LCD_RST, GPIO_MODE_OUTPUT);
		gpio_set_level(PIN_LCD_RST, 1);
	}

	esp_lcd_i80_bus_handle_t i80_bus = NULL;
	esp_lcd_i80_bus_config_t bus_config = {
		.clk_src = LCD_CLK_SRC_DEFAULT,
		.dc_gpio_num = PIN_LCD_DC,
		.wr_gpio_num = PIN_LCD_WR,
		.data_gpio_nums = {
			PIN_LCD_D0,
			PIN_LCD_D1,
			PIN_LCD_D2,
			PIN_LCD_D3,
			PIN_LCD_D4,
			PIN_LCD_D5,
			PIN_LCD_D6,
			PIN_LCD_D7,
		},
		.bus_width = 8,
		.max_transfer_bytes = LCD_WIDTH * 2,
	};
	ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

	esp_lcd_panel_io_i80_config_t io_config = {
		.cs_gpio_num = PIN_LCD_CS,
		.pclk_hz = 10 * 1000 * 1000,
		.trans_queue_depth = 10,
		.dc_levels = {
			.dc_idle_level = 0,
			.dc_cmd_level = 0,
			.dc_dummy_level = 0,
			.dc_data_level = 1,
		},
		.lcd_cmd_bits = 8,
		.lcd_param_bits = 8,
	};
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &s_io));

	nv3041a_init_minimal();

	uint16_t bar_h = LCD_HEIGHT / 6;
	lcd_fill_rect(0, 0 * bar_h, LCD_WIDTH, bar_h, 0xF800); // red
	lcd_fill_rect(0, 1 * bar_h, LCD_WIDTH, bar_h, 0x07E0); // green
	lcd_fill_rect(0, 2 * bar_h, LCD_WIDTH, bar_h, 0x001F); // blue
	lcd_fill_rect(0, 3 * bar_h, LCD_WIDTH, bar_h, 0xFFE0); // yellow
	lcd_fill_rect(0, 4 * bar_h, LCD_WIDTH, bar_h, 0xF81F); // magenta
	lcd_fill_rect(0, 5 * bar_h, LCD_WIDTH, LCD_HEIGHT - 5 * bar_h, 0x07FF); // cyan
}