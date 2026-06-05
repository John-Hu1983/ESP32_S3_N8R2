#include "tsc2046_touch.h"

#include "config.h"

#include <esp_err.h>
#include <esp_log.h>

#define TAG "TSC2046"

#ifndef TSC2046_X_MIN
#define TSC2046_X_MIN 200
#endif
#ifndef TSC2046_X_MAX
#define TSC2046_X_MAX 3900
#endif
#ifndef TSC2046_Y_MIN
#define TSC2046_Y_MIN 200
#endif
#ifndef TSC2046_Y_MAX
#define TSC2046_Y_MAX 3900
#endif
#ifndef TSC2046_SWAP_XY
#define TSC2046_SWAP_XY 0
#endif
#ifndef TSC2046_MIRROR_X
#define TSC2046_MIRROR_X 0
#endif
#ifndef TSC2046_MIRROR_Y
#define TSC2046_MIRROR_Y 0
#endif
#ifndef TSC2046_SPI_CLOCK_HZ
#define TSC2046_SPI_CLOCK_HZ (2 * 1000 * 1000)
#endif
#ifndef TSC2046_SAMPLES
#define TSC2046_SAMPLES 4
#endif

namespace {
constexpr uint8_t kCmdReadX = 0xD0;
constexpr uint8_t kCmdReadY = 0x90;
}  // namespace

bool Tsc2046Touch::Init(spi_host_device_t host, gpio_num_t clk, gpio_num_t mosi, gpio_num_t miso,
                        gpio_num_t cs, gpio_num_t irq, int width, int height) {
    if (width <= 0 || height <= 0) {
        ESP_LOGE(TAG, "Invalid display size: %d x %d", width, height);
        return false;
    }

    width_ = width;
    height_ = height;
    irq_ = irq;

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = mosi;
    buscfg.miso_io_num = miso;
    buscfg.sclk_io_num = clk;
    buscfg.quadwp_io_num = GPIO_NUM_NC;
    buscfg.quadhd_io_num = GPIO_NUM_NC;
    buscfg.max_transfer_sz = 0;

    esp_err_t ret = spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = TSC2046_SPI_CLOCK_HZ;
    devcfg.mode = 0;
    devcfg.spics_io_num = cs;
    devcfg.queue_size = 1;

    ret = spi_bus_add_device(host, &devcfg, &dev_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI add device failed: %s", esp_err_to_name(ret));
        return false;
    }

    if (irq_ != GPIO_NUM_NC) {
        gpio_config_t io_cfg = {};
        io_cfg.intr_type = GPIO_INTR_DISABLE;
        io_cfg.mode = GPIO_MODE_INPUT;
        io_cfg.pin_bit_mask = 1ULL << irq_;
        io_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        ESP_ERROR_CHECK(gpio_config(&io_cfg));
    }

    return true;
}

uint16_t Tsc2046Touch::ReadChannel(uint8_t cmd) {
    if (dev_ == nullptr) {
        return 0;
    }

    uint8_t tx[3] = {cmd, 0x00, 0x00};
    uint8_t rx[3] = {0};
    spi_transaction_t trans = {};
    trans.length = 8 * sizeof(tx);
    trans.rxlength = 8 * sizeof(rx);
    trans.tx_buffer = tx;
    trans.rx_buffer = rx;

    esp_err_t ret = spi_device_polling_transmit(dev_, &trans);
    if (ret != ESP_OK) {
        return 0;
    }

    uint16_t value = (static_cast<uint16_t>(rx[1]) << 8) | rx[2];
    return value >> 4;
}

uint16_t Tsc2046Touch::MapCoordinate(uint16_t raw, uint16_t min, uint16_t max, int size) const {
    if (size <= 1 || max <= min) {
        return 0;
    }

    int32_t clamped = raw;
    if (clamped < min) {
        clamped = min;
    } else if (clamped > max) {
        clamped = max;
    }

    int32_t mapped = (clamped - min) * (size - 1) / (max - min);
    if (mapped < 0) {
        mapped = 0;
    } else if (mapped >= size) {
        mapped = size - 1;
    }

    return static_cast<uint16_t>(mapped);
}

bool Tsc2046Touch::Read(uint16_t* x, uint16_t* y, bool* pressed) {
    if (x == nullptr || y == nullptr || pressed == nullptr) {
        return false;
    }

    *pressed = false;
    if (dev_ == nullptr) {
        return false;
    }

    if (irq_ != GPIO_NUM_NC && gpio_get_level(irq_) != 0) {
        return true;
    }

    uint32_t x_sum = 0;
    uint32_t y_sum = 0;
    int valid = 0;
    for (int i = 0; i < TSC2046_SAMPLES; ++i) {
        uint16_t raw_x = ReadChannel(kCmdReadX);
        uint16_t raw_y = ReadChannel(kCmdReadY);
        if (raw_x < TSC2046_X_MIN || raw_x > TSC2046_X_MAX || raw_y < TSC2046_Y_MIN ||
            raw_y > TSC2046_Y_MAX) {
            continue;
        }
        x_sum += raw_x;
        y_sum += raw_y;
        ++valid;
    }

    if (valid == 0) {
        return true;
    }

    uint16_t raw_x = static_cast<uint16_t>(x_sum / valid);
    uint16_t raw_y = static_cast<uint16_t>(y_sum / valid);
    last_raw_x_ = raw_x;
    last_raw_y_ = raw_y;

#if TSC2046_SWAP_XY
    uint16_t mapped_x = MapCoordinate(raw_y, TSC2046_Y_MIN, TSC2046_Y_MAX, width_);
    uint16_t mapped_y = MapCoordinate(raw_x, TSC2046_X_MIN, TSC2046_X_MAX, height_);
#else
    uint16_t mapped_x = MapCoordinate(raw_x, TSC2046_X_MIN, TSC2046_X_MAX, width_);
    uint16_t mapped_y = MapCoordinate(raw_y, TSC2046_Y_MIN, TSC2046_Y_MAX, height_);
#endif
#if TSC2046_MIRROR_X
    mapped_x = static_cast<uint16_t>(width_ - 1 - mapped_x);
#endif
#if TSC2046_MIRROR_Y
    mapped_y = static_cast<uint16_t>(height_ - 1 - mapped_y);
#endif

    *x = mapped_x;
    *y = mapped_y;
    *pressed = true;
    return true;
}
