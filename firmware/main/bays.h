// The two dock bays: load switches, presence detection and current sensing.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum { BAY_FRONT = 0, BAY_BACK = 1, BAY_COUNT } bay_id_t;

typedef struct {
    bool present;
    bool fault;         // load switch tripped its current limit
    float current_ma;
    float bus_mv;
} bay_sample_t;

esp_err_t bays_init(void);
// Call every ~250 ms. Handles probing empty bays and reads the INA226s.
void bays_poll(uint32_t now_ms, bay_sample_t out[BAY_COUNT]);
