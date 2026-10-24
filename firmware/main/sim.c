#include "sim.h"

#include <math.h>

#include "sdkconfig.h"

#ifdef CONFIG_GROGU_SIMULATE

#define SPEEDUP   CONFIG_GROGU_SIM_SPEEDUP
#define PEAK_MA   900.0f
#define CAP_MAH   1360.0f

typedef struct {
    float level;
    float taper_ma;
    uint32_t last_sim_ms;
} sim_ctl_t;

static sim_ctl_t s_front = {.level = 35.0f};
static uint32_t s_cycle_start_ms;

// Same toy charger as the host tests: CC to 80 %, then an exponential taper.
static float step(sim_ctl_t *c, uint32_t dt_ms)
{
    float ma;
    if (c->level < 80.0f) {
        ma = PEAK_MA * (0.86f + 0.14f * c->level / 80.0f);
        c->taper_ma = PEAK_MA;
        c->level += ma * (float)dt_ms / 3600000.0f / CAP_MAH * 100.0f;
    } else if (c->level < 100.0f) {
        c->taper_ma *= expf(-(float)dt_ms / (15.0f * 60000.0f));
        ma = c->taper_ma;
        c->level += 20.0f * (float)dt_ms / (45.0f * 60000.0f);
    } else {
        ma = 20.0f;
    }
    if (c->level > 100.0f) {
        c->level = 100.0f;
    }
    return ma;
}

void sim_poll(uint32_t now_ms, bay_sample_t out[BAY_COUNT])
{
    uint32_t sim_ms = now_ms * SPEEDUP;
    uint32_t in_cycle = sim_ms - s_cycle_start_ms;

    // A cycle is a full charge plus an hour docked full, then 20 s (real) empty.
    bool lifted = s_front.level >= 100.0f && in_cycle > 4u * 3600u * 1000u;
    if (lifted) {
        if (in_cycle > 4u * 3600u * 1000u + 20u * 1000u * SPEEDUP) {
            s_front = (sim_ctl_t){.level = 35.0f, .last_sim_ms = sim_ms};
            s_cycle_start_ms = sim_ms;
        }
        out[BAY_FRONT] = (bay_sample_t){.present = false};
    } else {
        uint32_t dt = sim_ms - s_front.last_sim_ms;
        s_front.last_sim_ms = sim_ms;
        out[BAY_FRONT] = (bay_sample_t){.present = true, .current_ma = step(&s_front, dt), .bus_mv = 5050.0f};
    }
    out[BAY_BACK] = (bay_sample_t){.present = true, .current_ma = 20.0f, .bus_mv = 5050.0f};
}

#else

void sim_poll(uint32_t now_ms, bay_sample_t out[BAY_COUNT])
{
    (void)now_ms;
    for (int i = 0; i < BAY_COUNT; i++) {
        out[i] = (bay_sample_t){0};
    }
}

#endif
