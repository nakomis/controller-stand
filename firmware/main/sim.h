// Simulated bays for UI work before the power board exists (CONFIG_GROGU_SIMULATE).
// Front: a controller docked at ~35 % and charging. Back: docks full. Every so often
// the front is lifted off and docked again, so the empty and settling states show too.
#pragma once

#include "bays.h"

void sim_poll(uint32_t now_ms, bay_sample_t out[BAY_COUNT]);
