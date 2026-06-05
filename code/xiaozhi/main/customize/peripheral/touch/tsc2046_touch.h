#pragma once

#include <driver/gpio.h>
#include <driver/spi_master.h>
#include <cstdint>

class Tsc2046Touch {
public:
    bool Init(spi_host_device_t host, gpio_num_t clk, gpio_num_t mosi, gpio_num_t miso,
              gpio_num_t cs, gpio_num_t irq, int width, int height);
    bool Read(uint16_t* x, uint16_t* y, bool* pressed);
    uint16_t last_raw_x() const { return last_raw_x_; }
    uint16_t last_raw_y() const { return last_raw_y_; }

private:
    uint16_t ReadChannel(uint8_t cmd);
    uint16_t MapCoordinate(uint16_t raw, uint16_t min, uint16_t max, int size) const;

    spi_device_handle_t dev_ = nullptr;
    gpio_num_t irq_ = GPIO_NUM_NC;
    int width_ = 0;
    int height_ = 0;
    uint16_t last_raw_x_ = 0;
    uint16_t last_raw_y_ = 0;
};
