#include "lcd_st7365p.h"

#include <stddef.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"

#define TAG "lcd_st7365p"
#define LCD_YIELD_CHUNKS 64

static spi_device_handle_t lcd_spi;
static uint8_t *lcd_dma_buffer;

/*
 * Yield to FreeRTOS after several SPI chunks.
 * Long full-screen transfers can otherwise keep this task busy long enough to
 * starve lower-priority tasks or the idle task that feeds the watchdog.
 */
static void lcd_yield_if_needed(uint32_t *chunk_count)
{
	(*chunk_count)++;
	if (*chunk_count >= LCD_YIELD_CHUNKS)
	{
		*chunk_count = 0;
		taskYIELD();
	}
}

/*
 * Convert the selected display direction into the ST7365P MADCTL register value.
 * MADCTL controls row/column mirroring, row-column exchange, and RGB/BGR order.
 */
static uint8_t lcd_get_madctl(void)
{
    uint8_t madctl = 0;

#if LCD_DIRECTION == LCD_DIR_VERTICAL_0
	madctl = LCD_MADCTL_BGR;
#elif LCD_DIRECTION == LCD_DIR_VERTICAL_180
	madctl = LCD_MADCTL_MX | LCD_MADCTL_MY | LCD_MADCTL_BGR;
#elif LCD_DIRECTION == LCD_DIR_HORIZONTAL_0
	madctl = LCD_MADCTL_MV | LCD_MADCTL_MX | LCD_MADCTL_BGR;
#elif LCD_DIRECTION == LCD_DIR_HORIZONTAL_180
	madctl = LCD_MADCTL_MV | LCD_MADCTL_MY | LCD_MADCTL_BGR;
#else
#error "Unsupported LCD_DIRECTION"
#endif

#if LCD_MIRROR_X
	/* With MV set, logical X is controlled by MY; otherwise by MX. */
	madctl ^= (madctl & LCD_MADCTL_MV) ? LCD_MADCTL_MY : LCD_MADCTL_MX;
#endif

#if LCD_MIRROR_Y
	/* With MV set, logical Y is controlled by MX; otherwise by MY. */
	madctl ^= (madctl & LCD_MADCTL_MV) ? LCD_MADCTL_MX : LCD_MADCTL_MY;
#endif

	return madctl;
}

/*
 * Return the active display width after applying LCD_DIRECTION.
 * Horizontal directions swap the physical panel width and height.
 */
uint16_t lcd_st7365p_get_width(void)
{
	return LCD_WIDTH;
}

/*
 * Return the active display height after applying LCD_DIRECTION.
 * Use this instead of the physical panel height when drawing UI or images.
 */
uint16_t lcd_st7365p_get_height(void)
{
	return LCD_HEIGHT;
}

/*
 * Send one LCD command byte through SPI.
 * RS/DC is driven low so the panel treats the byte as a command, not pixel data.
 */
static esp_err_t lcd_write_command(uint8_t command)
{
	spi_transaction_t transaction = {
		.length = 8,
		.tx_buffer = &command,
	};

	gpio_set_level(LCD_GPIO_RS, 0);
	return spi_device_polling_transmit(lcd_spi, &transaction);
}

/*
 * Send raw data bytes through SPI.
 * RS/DC is driven high so the panel treats the bytes as command parameters or
 * frame memory data, depending on the command sent just before this call.
 */
static esp_err_t lcd_write_data(const void *data, size_t length)
{
	if (length == 0)
	{
		return ESP_OK;
	}

	spi_transaction_t transaction = {
		.length = length * 8,
		.tx_buffer = data,
	};

	gpio_set_level(LCD_GPIO_RS, 1);
	return spi_device_polling_transmit(lcd_spi, &transaction);
}

/*
 * Convenience wrapper for commands that take a single data byte.
 */
static esp_err_t lcd_write_data_byte(uint8_t data)
{
	return lcd_write_data(&data, sizeof(data));
}

/*
 * Configure non-SPI LCD control pins.
 * RS/DC selects command/data mode, and RESET performs the panel hardware reset.
 */
static esp_err_t lcd_gpio_init(void)
{
	const gpio_config_t output_config = {
		.pin_bit_mask = (1ULL << LCD_GPIO_RS) | (1ULL << LCD_GPIO_RESET),
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};

	ESP_RETURN_ON_ERROR(gpio_config(&output_config), TAG, "gpio_config failed");
	gpio_set_level(LCD_GPIO_RS, 1);
	gpio_set_level(LCD_GPIO_RESET, 1);

	return ESP_OK;
}

/*
 * Configure the ESP-IDF SPI master bus and attach the LCD as a write-only device.
 * SPI_DMA_CH_AUTO lets ESP-IDF choose a DMA channel for larger pixel transfers.
 */
static esp_err_t lcd_spi_init(void)
{
	ESP_LOGI(TAG, "SPI init: host=%d sclk=IO%d mosi=IO%d cs=IO%d clock=%luHz", LCD_SPI_HOST, LCD_GPIO_SCL, LCD_GPIO_SDA, LCD_GPIO_CS, (unsigned long)LCD_SPI_CLOCK_HZ);
	ESP_LOGI(TAG, "Heap before LCD buffers: internal DMA=%u PSRAM=%u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

	const spi_bus_config_t bus_config = {
		.mosi_io_num = LCD_GPIO_SDA,
		.miso_io_num = -1,
		.sclk_io_num = LCD_GPIO_SCL,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = LCD_DMA_PIXELS * 2,
	};
	const spi_device_interface_config_t device_config = {
		.clock_speed_hz = LCD_SPI_CLOCK_HZ,
		.mode = 0,
		.spics_io_num = LCD_GPIO_CS,
		.queue_size = 7,
	};

	ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO), TAG, "spi_bus_initialize failed");
	ESP_RETURN_ON_ERROR(spi_bus_add_device(LCD_SPI_HOST, &device_config, &lcd_spi), TAG, "spi_bus_add_device failed");

	lcd_dma_buffer = heap_caps_malloc(LCD_DMA_PIXELS * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
	if (lcd_dma_buffer == NULL)
	{
		ESP_LOGE(TAG, "failed to allocate %u-byte internal DMA buffer", (unsigned)(LCD_DMA_PIXELS * 2));
		return ESP_ERR_NO_MEM;
	}

	ESP_LOGI(TAG, "LCD DMA buffer: %u bytes internal", (unsigned)(LCD_DMA_PIXELS * 2));

	return ESP_OK;
}

/*
 * Hardware reset sequence for the panel.
 * The delays give the controller time to leave reset before commands are sent.
 */
static void lcd_reset(void)
{
	gpio_set_level(LCD_GPIO_RESET, 0);
	vTaskDelay(pdMS_TO_TICKS(20));
	gpio_set_level(LCD_GPIO_RESET, 1);
	vTaskDelay(pdMS_TO_TICKS(120));
}

/*
 * Select the drawing rectangle in LCD GRAM and prepare for pixel writes.
 * CASET sets the column range, RASET sets the row range, and RAMWR starts data.
 */
static esp_err_t lcd_set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	const uint8_t column_data[] = {
		x0 >> 8,
		x0 & 0xFF,
		x1 >> 8,
		x1 & 0xFF,
	};
	const uint8_t row_data[] = {
		y0 >> 8,
		y0 & 0xFF,
		y1 >> 8,
		y1 & 0xFF,
	};

	ESP_RETURN_ON_ERROR(lcd_write_command(LCD_CMD_CASET), TAG, "CASET failed");
	ESP_RETURN_ON_ERROR(lcd_write_data(column_data, sizeof(column_data)), TAG, "CASET data failed");
	ESP_RETURN_ON_ERROR(lcd_write_command(LCD_CMD_RASET), TAG, "RASET failed");
	ESP_RETURN_ON_ERROR(lcd_write_data(row_data, sizeof(row_data)), TAG, "RASET data failed");
	ESP_RETURN_ON_ERROR(lcd_write_command(LCD_CMD_RAMWR), TAG, "RAMWR failed");

	return ESP_OK;
}

/*
 * Fill a rectangle with one RGB565 color.
 * The rectangle is clipped to the active display size, then sent in DMA-sized
 * chunks so full-screen clears do not require a huge RAM buffer.
 */
esp_err_t lcd_st7365p_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
	if (width == 0 || height == 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT)
	{
		return ESP_OK;
	}

	if (x + width > LCD_WIDTH)
	{
		width = LCD_WIDTH - x;
	}
	if (y + height > LCD_HEIGHT)
	{
		height = LCD_HEIGHT - y;
	}

	const uint8_t color_hi = color >> 8;
	const uint8_t color_lo = color & 0xFF;
	for (uint32_t pixel = 0; pixel < LCD_DMA_PIXELS; pixel++)
	{
		lcd_dma_buffer[pixel * 2] = color_hi;
		lcd_dma_buffer[pixel * 2 + 1] = color_lo;
	}
	uint32_t pixels_left = (uint32_t)width * height;
	uint32_t chunk_count = 0;
	ESP_RETURN_ON_ERROR(lcd_set_address_window(x, y, x + width - 1, y + height - 1), TAG, "set window failed");
	while (pixels_left > 0)
	{
		const uint32_t chunk_pixels = pixels_left > LCD_DMA_PIXELS ? LCD_DMA_PIXELS : pixels_left;
		ESP_RETURN_ON_ERROR(lcd_write_data(lcd_dma_buffer, chunk_pixels * 2), TAG, "pixel data failed");
		pixels_left -= chunk_pixels;
		lcd_yield_if_needed(&chunk_count);
	}

	return ESP_OK;
}

/*
 * Draw an RGB565 image at x/y.
 * The input image is a uint16_t array in normal RGB565 form; this function packs
 * it into high-byte-first SPI bytes, clips right/bottom overflow, and batches
 * multiple rows into each DMA transfer for better refresh speed.
 */
esp_err_t lcd_st7365p_draw_image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *image_rgb565)
{
	if (image_rgb565 == NULL)
	{
		return ESP_ERR_INVALID_ARG;
	}
	if (width == 0 || height == 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT)
	{
		return ESP_OK;
	}

	const uint16_t source_width = width;
	uint16_t draw_width = width;
	uint16_t draw_height = height;

	if (x + draw_width > LCD_WIDTH)
	{
		draw_width = LCD_WIDTH - x;
	}
	if (y + draw_height > LCD_HEIGHT)
	{
		draw_height = LCD_HEIGHT - y;
	}

	ESP_RETURN_ON_ERROR(lcd_set_address_window(x, y, x + draw_width - 1, y + draw_height - 1), TAG, "set image window failed");

	const uint16_t rows_per_chunk = (LCD_DMA_PIXELS / draw_width) > 0 ? (LCD_DMA_PIXELS / draw_width) : 1;
	uint16_t row = 0;

	while (row < draw_height)
	{
		uint16_t chunk_rows = draw_height - row;
		if (chunk_rows > rows_per_chunk)
		{
			chunk_rows = rows_per_chunk;
		}

		uint8_t *destination = lcd_dma_buffer;
		for (uint16_t local_row = 0; local_row < chunk_rows; local_row++)
		{
			const uint16_t *source_row = image_rgb565 + ((uint32_t)(row + local_row) * source_width);
			for (uint16_t column = 0; column < draw_width; column++)
			{
				const uint16_t color = source_row[column];
				*destination++ = color >> 8;
				*destination++ = color & 0xFF;
			}
		}

		const uint32_t chunk_pixels = (uint32_t)chunk_rows * draw_width;
		ESP_RETURN_ON_ERROR(lcd_write_data(lcd_dma_buffer, chunk_pixels * 2), TAG, "image data failed");
		row += chunk_rows;
	}

	return ESP_OK;
}

/*
 * Draw a rectangle from already byte-swapped RGB565 data.
 * Input bytes must be in panel order (high byte first for each pixel), which
 * lets GUI stacks like LVGL flush directly without extra conversion.
 */
esp_err_t lcd_st7365p_draw_area_rgb565_be(uint16_t x,
						  uint16_t y,
						  uint16_t width,
						  uint16_t height,
						  const uint8_t *rgb565_be,
						  size_t byte_count)
{
	if (rgb565_be == NULL)
	{
		return ESP_ERR_INVALID_ARG;
	}
	if (width == 0 || height == 0)
	{
		return ESP_OK;
	}
	if (x >= LCD_WIDTH || y >= LCD_HEIGHT || (x + width) > LCD_WIDTH || (y + height) > LCD_HEIGHT)
	{
		return ESP_ERR_INVALID_ARG;
	}

	const size_t required_bytes = (size_t)width * height * 2;
	if (byte_count < required_bytes)
	{
		return ESP_ERR_INVALID_SIZE;
	}

	ESP_RETURN_ON_ERROR(lcd_set_address_window(x, y, x + width - 1, y + height - 1), TAG, "set window failed");

	const uint8_t *source = rgb565_be;
	size_t bytes_left = required_bytes;
	uint32_t chunk_count = 0;

	while (bytes_left > 0)
	{
		size_t chunk_bytes = bytes_left;
		if (chunk_bytes > (LCD_DMA_PIXELS * 2))
		{
			chunk_bytes = LCD_DMA_PIXELS * 2;
		}

		ESP_RETURN_ON_ERROR(lcd_write_data(source, chunk_bytes), TAG, "raw area data failed");
		source += chunk_bytes;
		bytes_left -= chunk_bytes;
		lcd_yield_if_needed(&chunk_count);
	}

	return ESP_OK;
}

/*
 * Initialize GPIO, SPI, reset the LCD, and send the basic ST7365P setup commands.
 * This must be called once before any drawing function is used.
 */
esp_err_t lcd_st7365p_init(void)
{
	const uint8_t madctl = lcd_get_madctl();

	ESP_LOGI(TAG, "init GPIO");
	ESP_RETURN_ON_ERROR(lcd_gpio_init(), TAG, "lcd_gpio_init failed");
	ESP_LOGI(TAG, "init SPI and buffers");
	ESP_RETURN_ON_ERROR(lcd_spi_init(), TAG, "lcd_spi_init failed");
	ESP_LOGI(TAG, "reset panel");
	lcd_reset();

	ESP_LOGI(TAG, "send init commands");
	ESP_RETURN_ON_ERROR(lcd_write_command(LCD_CMD_SWRESET), TAG, "SWRESET failed");
	vTaskDelay(pdMS_TO_TICKS(150));

	ESP_RETURN_ON_ERROR(lcd_write_command(LCD_CMD_SLPOUT), TAG, "SLPOUT failed");
	vTaskDelay(pdMS_TO_TICKS(120));

	ESP_RETURN_ON_ERROR(lcd_write_command(LCD_CMD_COLMOD), TAG, "COLMOD failed");
	ESP_RETURN_ON_ERROR(lcd_write_data_byte(0x05), TAG, "COLMOD data failed");

	ESP_RETURN_ON_ERROR(lcd_write_command(LCD_CMD_MADCTL), TAG, "MADCTL failed");
	ESP_RETURN_ON_ERROR(lcd_write_data_byte(madctl), TAG, "MADCTL data failed");

#if LCD_COLOR_INVERT
	ESP_RETURN_ON_ERROR(lcd_write_command(LCD_CMD_INVON), TAG, "INVON failed");
#else
	ESP_RETURN_ON_ERROR(lcd_write_command(LCD_CMD_INVOFF), TAG, "INVOFF failed");
#endif

	ESP_RETURN_ON_ERROR(lcd_write_command(LCD_CMD_DISPON), TAG, "DISPON failed");
	vTaskDelay(pdMS_TO_TICKS(120));

	return ESP_OK;
}

/*
 * Draw a simple color-bar test pattern.
 * This is useful for checking orientation, color order, and whether full-screen
 * writes cover the whole active display area.
 */
esp_err_t lcd_st7365p_draw_demo(void)
{
	static const uint16_t colors[] = {
		0xF800,
		0x07E0,
		0x001F,
		0xFFE0,
		0xF81F,
		0x07FF,
		0xFFFF,
		0x0000,
	};
	const uint16_t color_count = sizeof(colors) / sizeof(colors[0]);
	const uint16_t bar_width = LCD_WIDTH / color_count;

	for (uint16_t index = 0; index < color_count; index++)
	{
		const uint16_t x = index * bar_width;
		const uint16_t width = index == color_count - 1 ? LCD_WIDTH - x : bar_width;
		ESP_RETURN_ON_ERROR(lcd_st7365p_fill_rect(x, 0, width, LCD_HEIGHT, colors[index]), TAG, "fill bar failed");
	}

	return ESP_OK;
}