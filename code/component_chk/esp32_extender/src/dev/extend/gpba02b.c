#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "../../config.h"
#include "gpba02b.h"

#define TAG "GPBA02B"

#define GPBA02B_REG_BUFA 0x00
#define GPBA02B_REG_BUFB 0x01
#define GPBA02B_REG_BUFC 0x02
#define GPBA02B_REG_DIRA 0x04
#define GPBA02B_REG_DIRB 0x05
#define GPBA02B_REG_DIRC 0x06

#define GPBA02B_WRITE_CMD(reg) (0x80 | ((reg) & 0x3F))

static void gpba02b_spi_delay(void)
{
    esp_rom_delay_us(1);
}

static void gpba02b_write_byte(uint8_t data)
{
    for (int bit = 7; bit >= 0; bit--)
    {
        gpio_set_level(EXTEND_SPI_MOSI, (data >> bit) & 0x01);
        gpba02b_spi_delay();
        gpio_set_level(EXTEND_SPI_CLK, 1);
        gpba02b_spi_delay();
        gpio_set_level(EXTEND_SPI_CLK, 0);
        gpba02b_spi_delay();
    }
}

static esp_err_t gpba02b_write_reg(uint8_t reg, uint8_t value)
{
    gpio_set_level(EXTEND_SPI_CS, 0);
    gpba02b_spi_delay();

    gpba02b_write_byte(GPBA02B_WRITE_CMD(reg));
    gpba02b_write_byte(value);

    gpba02b_spi_delay();
    gpio_set_level(EXTEND_SPI_CS, 1);
    gpba02b_spi_delay();

    return ESP_OK;
}

esp_err_t gpba02b_write_all(uint8_t value)
{
    ESP_RETURN_ON_ERROR(gpba02b_write_reg(GPBA02B_REG_BUFA, value), TAG, "write BUFA failed");
    ESP_RETURN_ON_ERROR(gpba02b_write_reg(GPBA02B_REG_BUFB, value), TAG, "write BUFB failed");
    ESP_RETURN_ON_ERROR(gpba02b_write_reg(GPBA02B_REG_BUFC, value), TAG, "write BUFC failed");

    return ESP_OK;
}

esp_err_t gpba02b_init(void)
{
    gpio_config_t output_conf = {
        .pin_bit_mask = (1ULL << EXTEND_SPI_CS) | (1ULL << EXTEND_SPI_MOSI) | (1ULL << EXTEND_SPI_CLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&output_conf), TAG, "output gpio config failed");

    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << EXTEND_SPI_MISO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&input_conf), TAG, "input gpio config failed");

    gpio_set_level(EXTEND_SPI_CS, 1);
    gpio_set_level(EXTEND_SPI_CLK, 0);
    gpio_set_level(EXTEND_SPI_MOSI, 0);

    ESP_RETURN_ON_ERROR(gpba02b_write_reg(GPBA02B_REG_DIRA, 0xFF), TAG, "write DIRA failed");
    ESP_RETURN_ON_ERROR(gpba02b_write_reg(GPBA02B_REG_DIRB, 0xFF), TAG, "write DIRB failed");
    ESP_RETURN_ON_ERROR(gpba02b_write_reg(GPBA02B_REG_DIRC, 0xFF), TAG, "write DIRC failed");
    ESP_RETURN_ON_ERROR(gpba02b_write_all(0x00), TAG, "clear outputs failed");

    ESP_LOGI(TAG, "GPBA02B all IO configured as outputs");
    return ESP_OK;
}

static void gpba02b_toggle_task(void *arg)
{
    (void)arg;
    uint8_t value = 0x00;

    while (1)
    {
        value = (uint8_t)~value;
        gpba02b_write_all(value);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void gpba02b_start_toggle_task(void)
{
    xTaskCreate(gpba02b_toggle_task, "gpba02b_toggle", 2048, NULL, 5, NULL);
}