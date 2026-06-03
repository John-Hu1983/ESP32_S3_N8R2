#include "sc7a20htr.h"

#include <driver/i2c_master.h>

Sc7a20htr::Sc7a20htr(i2c_master_bus_handle_t i2c_bus, uint8_t addr)
    : I2cDevice(i2c_bus, addr), i2c_bus_(i2c_bus), addr_(addr) {
}

bool Sc7a20htr::SelfTest(int timeout_ms) {
    return i2c_master_probe(i2c_bus_, addr_, timeout_ms) == ESP_OK;
}
