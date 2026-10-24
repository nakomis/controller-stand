// The 1.28" round GC9A01: outer arc for the front bay, inner arc for the back, and a
// line of text for each in the middle.
#pragma once

#include <stdbool.h>

#include "bays.h"
#include "charge_estimator.h"
#include "esp_err.h"

typedef struct {
    ce_state_t state;
    float pct;
    bool fault;
    char text[24];  // from ce_describe()
} bay_view_t;

esp_err_t display_init(void);
void display_update(const bay_view_t views[BAY_COUNT]);
void display_set_dim(bool dim);
