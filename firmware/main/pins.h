// Pin map for the ESP32-S3 dev board. Avoids strapping pins (0, 3, 45, 46) and the
// USB pins (19, 20).
#pragma once

#include "driver/gpio.h"

// GC9A01 round display (SPI2)
#define PIN_LCD_SCLK GPIO_NUM_12
#define PIN_LCD_MOSI GPIO_NUM_11
#define PIN_LCD_CS   GPIO_NUM_10
#define PIN_LCD_DC   GPIO_NUM_9
#define PIN_LCD_RST  GPIO_NUM_14
#define PIN_LCD_BL   GPIO_NUM_13

// I2C to the two INA226s
#define PIN_I2C_SDA GPIO_NUM_8
#define PIN_I2C_SCL GPIO_NUM_18

// Per-bay load switch enable (active high) and fault flag (open drain, active low)
#define PIN_FRONT_EN    GPIO_NUM_4
#define PIN_FRONT_FAULT GPIO_NUM_5
#define PIN_BACK_EN     GPIO_NUM_6
#define PIN_BACK_FAULT  GPIO_NUM_7

#define INA226_ADDR_FRONT 0x40
#define INA226_ADDR_BACK  0x41
