#ifndef GPBA02B_H
#define GPBA02B_H

#include <stdint.h>
#include "esp_err.h"

esp_err_t gpba02b_init(void);
esp_err_t gpba02b_write_all(uint8_t value);
void gpba02b_start_toggle_task(void);

#endif