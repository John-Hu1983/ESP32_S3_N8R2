#ifndef SC7A20HTR_H
#define SC7A20HTR_H

#include "i2c_device.h"

#include <cstdint>

class Sc7a20htr : public I2cDevice {
public:
    static constexpr uint8_t kDefaultI2cAddress = 0x18;

    Sc7a20htr(i2c_master_bus_handle_t i2c_bus, uint8_t addr = kDefaultI2cAddress);

    bool SelfTest(int timeout_ms = 100);

private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    uint8_t addr_ = kDefaultI2cAddress;
};

#endif  // SC7A20HTR_H
