#include "ina226.h"

#define REG_CONFIG 0x00
#define REG_SHUNT  0x01
#define REG_BUS    0x02

// AVG = 16, VBUSCT = VSHCT = 1.1 ms, continuous shunt and bus. ~35 ms per reading.
#define CONFIG_VALUE 0x4527

#define TIMEOUT_MS 50

static esp_err_t write_reg(ina226_t *ina, uint8_t reg, uint16_t value)
{
    uint8_t buf[3] = {reg, value >> 8, value & 0xff};
    return i2c_master_transmit(ina->dev, buf, sizeof(buf), TIMEOUT_MS);
}

static esp_err_t read_reg(ina226_t *ina, uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];
    esp_err_t err = i2c_master_transmit_receive(ina->dev, &reg, 1, buf, sizeof(buf), TIMEOUT_MS);
    if (err == ESP_OK) {
        *value = (uint16_t)(buf[0] << 8 | buf[1]);
    }
    return err;
}

esp_err_t ina226_init(ina226_t *ina, i2c_master_bus_handle_t bus, uint8_t addr, float shunt_milliohm)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400000,
    };
    ina->shunt_milliohm = shunt_milliohm;
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &ina->dev);
    if (err != ESP_OK) {
        return err;
    }
    return write_reg(ina, REG_CONFIG, CONFIG_VALUE);
}

esp_err_t ina226_read(ina226_t *ina, float *current_ma, float *bus_mv)
{
    uint16_t shunt, bus;
    esp_err_t err = read_reg(ina, REG_SHUNT, &shunt);
    if (err == ESP_OK) {
        err = read_reg(ina, REG_BUS, &bus);
    }
    if (err != ESP_OK) {
        return err;
    }
    // Shunt LSB 2.5 uV; uV / mOhm = mA. Bus LSB 1.25 mV.
    *current_ma = (float)(int16_t)shunt * 2.5f / ina->shunt_milliohm;
    *bus_mv = (float)bus * 1.25f;
    return ESP_OK;
}
