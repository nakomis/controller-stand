// Minimal INA226 driver: shunt current and bus voltage, continuous conversion.
#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    i2c_master_dev_handle_t dev;
    float shunt_milliohm;
} ina226_t;

esp_err_t ina226_init(ina226_t *ina, i2c_master_bus_handle_t bus, uint8_t addr, float shunt_milliohm);
esp_err_t ina226_read(ina226_t *ina, float *current_ma, float *bus_mv);
