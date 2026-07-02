#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "../../config.h"

#define TOUCH_SPI_HOST SPI3_HOST
#define TOUCH_SPI_CLOCK (2 * 1000 * 1000)

static const char *TAG = "TSC2046";
static spi_device_handle_t touch_spi;

static uint16_t tsc2046_read_channel(uint8_t cmd)
{
    uint8_t tx[3] = {cmd, 0x00, 0x00};
    uint8_t rx[3] = {0};
    spi_transaction_t t = {
        .length = 24,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_transmit(touch_spi, &t);

    return (uint16_t)(((uint16_t)rx[1] << 8) | rx[2]) >> 4;
}

static bool tsc2046_read_xy(uint16_t *x, uint16_t *y)
{
    if (gpio_get_level(TOUCH_IRQ) != 0)
    {
        return false;
    }

    uint16_t x1 = tsc2046_read_channel(0xD0);
    uint16_t y1 = tsc2046_read_channel(0x90);
    uint16_t x2 = tsc2046_read_channel(0xD0);
    uint16_t y2 = tsc2046_read_channel(0x90);

    *x = (x1 + x2) / 2;
    *y = (y1 + y2) / 2;
    return true;
}

static void touch_task(void *arg)
{
    (void)arg;
    uint16_t x = 0;
    uint16_t y = 0;

    while (1)
    {
        if (tsc2046_read_xy(&x, &y))
        {
            ESP_LOGI(TAG, "Touch x=%u y=%u", x, y);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}

void tsc2046_init(void)
{
    spi_bus_config_t touch_bus_cfg = {
        .mosi_io_num = TOUCH_MOSI,
        .miso_io_num = TOUCH_MISO,
        .sclk_io_num = TOUCH_CLK,
    };
    spi_bus_initialize(TOUCH_SPI_HOST, &touch_bus_cfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t touch_dev_cfg = {
        .clock_speed_hz = TOUCH_SPI_CLOCK,
        .mode = 0,
        .spics_io_num = TOUCH_CS,
        .queue_size = 4,
    };
    spi_bus_add_device(TOUCH_SPI_HOST, &touch_dev_cfg, &touch_spi);

    gpio_config_t touch_irq_conf = {
        .pin_bit_mask = (1ULL << TOUCH_IRQ),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&touch_irq_conf);
}

void tsc2046_start_task(void)
{
    xTaskCreate(touch_task, "touch_task", 4096, NULL, 5, NULL);
}
