#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "../../config.h"

#define LCD_WIDTH 480
#define LCD_HEIGHT 272

#define LCD_SPI_HOST SPI2_HOST
#define LCD_SPI_CLOCK (80 * 1000 * 1000)

static spi_device_handle_t lcd_spi;

static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(LCD_DC, 0);
    spi_transaction_t t = {.length = 8, .tx_buffer = &cmd};
    spi_device_transmit(lcd_spi, &t);
}

static void lcd_data(uint8_t data)
{
    gpio_set_level(LCD_DC, 1);
    spi_transaction_t t = {.length = 8, .tx_buffer = &data};
    spi_device_transmit(lcd_spi, &t);
}

static void lcd_reset(void)
{
    gpio_set_level(LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
}

static void nv3041_vendor_init(void)
{
    lcd_reset();

    lcd_cmd(0xff);
    lcd_data(0xa5);
    lcd_cmd(0xE7);
    lcd_data(0x10);
    lcd_cmd(0x35);
    lcd_data(0x00);
    lcd_cmd(0x36);
    lcd_data(0xc0);
    lcd_cmd(0x3A);
    lcd_data(0x55);
    lcd_cmd(0x41);
    lcd_data(0x03);
    lcd_cmd(0x44);
    lcd_data(0x15);
    lcd_cmd(0x45);
    lcd_data(0x15);
    lcd_cmd(0x7d);
    lcd_data(0x03);

    lcd_cmd(0xc1);
    lcd_data(0xbb);
    lcd_cmd(0xc2);
    lcd_data(0x05);
    lcd_cmd(0xc3);
    lcd_data(0x10);
    lcd_cmd(0xc6);
    lcd_data(0x3e);
    lcd_cmd(0xc7);
    lcd_data(0x25);
    lcd_cmd(0xc8);
    lcd_data(0x11);
    lcd_cmd(0x7a);
    lcd_data(0x5f);
    lcd_cmd(0x6f);
    lcd_data(0x44);
    lcd_cmd(0x78);
    lcd_data(0x70);
    lcd_cmd(0xc9);
    lcd_data(0x00);
    lcd_cmd(0x67);
    lcd_data(0x21);

    lcd_cmd(0x51);
    lcd_data(0x0a);
    lcd_cmd(0x52);
    lcd_data(0x76);
    lcd_cmd(0x53);
    lcd_data(0x0a);
    lcd_cmd(0x54);
    lcd_data(0x76);

    lcd_cmd(0x46);
    lcd_data(0x0a);
    lcd_cmd(0x47);
    lcd_data(0x2a);
    lcd_cmd(0x48);
    lcd_data(0x0a);
    lcd_cmd(0x49);
    lcd_data(0x1a);

    lcd_cmd(0x56);
    lcd_data(0x43);
    lcd_cmd(0x57);
    lcd_data(0x42);
    lcd_cmd(0x58);
    lcd_data(0x3c);
    lcd_cmd(0x59);
    lcd_data(0x64);
    lcd_cmd(0x5a);
    lcd_data(0x41);
    lcd_cmd(0x5b);
    lcd_data(0x3c);
    lcd_cmd(0x5c);
    lcd_data(0x02);
    lcd_cmd(0x5d);
    lcd_data(0x3c);
    lcd_cmd(0x5e);
    lcd_data(0x1f);

    lcd_cmd(0x60);
    lcd_data(0x80);
    lcd_cmd(0x61);
    lcd_data(0x3f);
    lcd_cmd(0x62);
    lcd_data(0x21);
    lcd_cmd(0x63);
    lcd_data(0x07);
    lcd_cmd(0x64);
    lcd_data(0xe0);
    lcd_cmd(0x65);
    lcd_data(0x02);

    lcd_cmd(0xca);
    lcd_data(0x20);
    lcd_cmd(0xcb);
    lcd_data(0x52);
    lcd_cmd(0xcc);
    lcd_data(0x10);
    lcd_cmd(0xcd);
    lcd_data(0x42);
    lcd_cmd(0xd0);
    lcd_data(0x20);
    lcd_cmd(0xd1);
    lcd_data(0x52);
    lcd_cmd(0xd2);
    lcd_data(0x10);
    lcd_cmd(0xd3);
    lcd_data(0x42);
    lcd_cmd(0xd4);
    lcd_data(0x0a);
    lcd_cmd(0xd5);
    lcd_data(0x32);

    lcd_cmd(0xf8);
    lcd_data(0x03);
    lcd_cmd(0xf9);
    lcd_data(0x20);

    lcd_cmd(0x80);
    lcd_data(0x00);
    lcd_cmd(0xA0);
    lcd_data(0x00);
    lcd_cmd(0x81);
    lcd_data(0x05);
    lcd_cmd(0xA1);
    lcd_data(0x05);
    lcd_cmd(0x82);
    lcd_data(0x04);
    lcd_cmd(0xA2);
    lcd_data(0x03);
    lcd_cmd(0x86);
    lcd_data(0x25);
    lcd_cmd(0xA6);
    lcd_data(0x1c);
    lcd_cmd(0x87);
    lcd_data(0x2a);
    lcd_cmd(0xA7);
    lcd_data(0x2a);
    lcd_cmd(0x83);
    lcd_data(0x1d);
    lcd_cmd(0xA3);
    lcd_data(0x1d);
    lcd_cmd(0x84);
    lcd_data(0x1e);
    lcd_cmd(0xA4);
    lcd_data(0x1e);
    lcd_cmd(0x85);
    lcd_data(0x3f);
    lcd_cmd(0xA5);
    lcd_data(0x3f);

    lcd_cmd(0x88);
    lcd_data(0x0b);
    lcd_cmd(0xA8);
    lcd_data(0x0b);
    lcd_cmd(0x89);
    lcd_data(0x14);
    lcd_cmd(0xA9);
    lcd_data(0x13);
    lcd_cmd(0x8a);
    lcd_data(0x1a);
    lcd_cmd(0xAa);
    lcd_data(0x1a);
    lcd_cmd(0x8b);
    lcd_data(0x0a);
    lcd_cmd(0xAb);
    lcd_data(0x0a);
    lcd_cmd(0x8c);
    lcd_data(0x1c);
    lcd_cmd(0xAc);
    lcd_data(0x0c);
    lcd_cmd(0x8d);
    lcd_data(0x1f);
    lcd_cmd(0xAd);
    lcd_data(0x0b);
    lcd_cmd(0x8e);
    lcd_data(0x1f);
    lcd_cmd(0xAe);
    lcd_data(0x0a);
    lcd_cmd(0x8f);
    lcd_data(0x1f);
    lcd_cmd(0xAf);
    lcd_data(0x07);
    lcd_cmd(0x90);
    lcd_data(0x06);
    lcd_cmd(0xB0);
    lcd_data(0x06);
    lcd_cmd(0x91);
    lcd_data(0x0d);
    lcd_cmd(0xB1);
    lcd_data(0x0d);
    lcd_cmd(0x92);
    lcd_data(0x17);
    lcd_cmd(0xB2);
    lcd_data(0x17);

    lcd_cmd(0xff);
    lcd_data(0x00);

    lcd_cmd(0x21);
    lcd_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(200));
    lcd_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    lcd_cmd(0x2A);
    lcd_data(x1 >> 8);
    lcd_data(x1 & 0xFF);
    lcd_data(x2 >> 8);
    lcd_data(x2 & 0xFF);

    lcd_cmd(0x2B);
    lcd_data(y1 >> 8);
    lcd_data(y1 & 0xFF);
    lcd_data(y2 >> 8);
    lcd_data(y2 & 0xFF);

    lcd_cmd(0x2C);
}

static void lcd_fill_fast(uint16_t color)
{
    lcd_set_window(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    gpio_set_level(LCD_DC, 1);

    // LCD expects MSB first; swap byte order for SPI
    color = (uint16_t)((color << 8) | (color >> 8));

    uint16_t line_buf[LCD_WIDTH];
    for (int i = 0; i < LCD_WIDTH; i++)
    {
        line_buf[i] = color;
    }

    for (int y = 0; y < LCD_HEIGHT; y++)
    {
        spi_transaction_t t = {
            .length = LCD_WIDTH * 16,
            .tx_buffer = line_buf,
        };
        spi_device_transmit(lcd_spi, &t);
    }
}

void lcd_init(void)
{
    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << LCD_PWR) | (1ULL << LCD_RST) | (1ULL << LCD_DC),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&conf);

    gpio_set_level(LCD_PWR, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_MOSI,
        .miso_io_num = LCD_MISO,
        .sclk_io_num = LCD_SCK,
    };
    spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = LCD_SPI_CLOCK,
        .mode = 0,
        .spics_io_num = LCD_CS,
        .queue_size = 16,
    };
    spi_bus_add_device(LCD_SPI_HOST, &dev_cfg, &lcd_spi);

    nv3041_vendor_init();
}

void lcd_fill_color(uint16_t color)
{
    lcd_fill_fast(color);
}
