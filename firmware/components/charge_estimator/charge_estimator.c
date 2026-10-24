#include "charge_estimator.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

ce_config_t ce_config_default(void)
{
    // Provisional. DualSense: 1560 mAh at 3.7 V nominal. Input side at 5 V with ~85 %
    // charger efficiency: 1560 * 3.7 / 5 / 0.85 ~= 1360 mAh.
    return (ce_config_t){
        .peak_ma = 900.0f,
        .cc_start_ratio = 0.86f,    // 3.6 V / 4.2 V
        .cc_end_pct = 80.0f,
        .taper_detect_ratio = 0.92f,
        .full_ma = 60.0f,
        .full_hold_ms = 60 * 1000,
        .capacity_mah = 1360.0f,
        .taper_minutes = 45.0f,
        .settle_ms = 30 * 1000,
        .filter_tau_ms = 5 * 1000.0f,
    };
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void ce_init(ce_bay_t *bay, const ce_config_t *cfg)
{
    memset(bay, 0, sizeof(*bay));
    bay->cfg = *cfg;
    bay->state = CE_EMPTY;
}

static void reset_dock(ce_bay_t *bay, uint32_t now_ms)
{
    ce_config_t cfg = bay->cfg;
    ce_init(bay, &cfg);
    bay->state = CE_SETTLING;
    bay->docked_at_ms = now_ms;
    bay->last_ms = now_ms;
}

// Level implied by the current alone, given whether we believe the taper has begun.
static float level_from_current(const ce_bay_t *bay)
{
    const ce_config_t *c = &bay->cfg;
    float peak = bay->in_taper ? bay->max_current_ma : c->peak_ma;
    if (peak <= c->full_ma) {
        return c->cc_end_pct;
    }
    if (bay->in_taper) {
        float frac = (bay->current_ma - c->full_ma) / (peak - c->full_ma);
        return clampf(100.0f - frac * (100.0f - c->cc_end_pct), c->cc_end_pct, 99.0f);
    }
    float ratio = bay->current_ma / peak;
    float frac = (ratio - c->cc_start_ratio) / (1.0f - c->cc_start_ratio);
    return clampf(frac * c->cc_end_pct, 0.0f, c->cc_end_pct);
}

static void estimate(ce_bay_t *bay)
{
    const ce_config_t *c = &bay->cfg;
    float coulomb_pct = bay->start_pct + bay->delivered_mah / c->capacity_mah * 100.0f;

    if (bay->in_taper) {
        // The taper's current is a far better guide than the running total, which
        // accumulates the error of the starting guess.
        bay->pct = level_from_current(bay);
    } else {
        // Still in CC: never above the CC ceiling, however much has gone in.
        bay->pct = clampf(coulomb_pct, 0.0f, c->cc_end_pct - 1.0f);
    }

    float taper_left = c->taper_minutes * (100.0f - bay->pct) / (100.0f - c->cc_end_pct);
    if (bay->in_taper) {
        bay->minutes_to_full = clampf(taper_left, 1.0f, c->taper_minutes);
    } else {
        float cc_mah = (c->cc_end_pct - bay->pct) / 100.0f * c->capacity_mah;
        float amps = bay->current_ma > 1.0f ? bay->current_ma : c->peak_ma;
        bay->minutes_to_full = cc_mah / amps * 60.0f + c->taper_minutes;
    }
}

void ce_update(ce_bay_t *bay, uint32_t now_ms, bool present, float current_ma)
{
    const ce_config_t *c = &bay->cfg;

    if (!present) {
        if (bay->state != CE_EMPTY) {
            ce_config_t cfg = *c; // ce_init clears bay, cfg included
            ce_init(bay, &cfg);
        }
        return;
    }
    if (bay->state == CE_EMPTY) {
        reset_dock(bay, now_ms);
    }

    uint32_t dt_ms = now_ms - bay->last_ms;
    bay->last_ms = now_ms;

    if (!bay->has_sample) {
        bay->current_ma = current_ma;
        bay->has_sample = true;
    } else {
        float alpha = 1.0f - expf(-(float)dt_ms / c->filter_tau_ms);
        bay->current_ma += alpha * (current_ma - bay->current_ma);
    }
    if (bay->current_ma > bay->max_current_ma) {
        bay->max_current_ma = bay->current_ma;
    }

    if (bay->state == CE_FULL) {
        return; // latched until undocked; top-up trickles don't count as charging
    }

    // Full detection.
    if (bay->current_ma < c->full_ma) {
        if (!bay->below_full) {
            bay->below_full = true;
            bay->below_full_since_ms = now_ms;
        } else if (now_ms - bay->below_full_since_ms >= c->full_hold_ms) {
            bay->state = CE_FULL;
            bay->pct = 100.0f;
            bay->minutes_to_full = 0.0f;
            return;
        }
    } else {
        bay->below_full = false;
    }

    // Taper detection: a sustained fall from the peak. A controller docked already
    // in its taper never shows a peak, so its absolute current, well below the
    // configured CC peak, also counts.
    if (!bay->in_taper && bay->max_current_ma > c->full_ma) {
        bool fell_from_peak = bay->current_ma < bay->max_current_ma * c->taper_detect_ratio;
        bool below_cc = bay->current_ma < c->peak_ma * c->cc_start_ratio * c->taper_detect_ratio;
        if (fell_from_peak || below_cc) {
            bay->in_taper = true;
            if (below_cc && !fell_from_peak) {
                bay->max_current_ma = c->peak_ma; // never saw the peak; assume config
            }
        }
    }

    if (bay->state == CE_SETTLING) {
        if (now_ms - bay->docked_at_ms < c->settle_ms) {
            return;
        }
        bay->start_pct = level_from_current(bay);
        bay->delivered_mah = 0.0f;
        bay->state = CE_CHARGING;
    } else {
        bay->delivered_mah += bay->current_ma * (float)dt_ms / 3600000.0f;
    }
    estimate(bay);
}

ce_state_t ce_state(const ce_bay_t *bay)
{
    return bay->state;
}

float ce_percent(const ce_bay_t *bay)
{
    return bay->pct;
}

float ce_minutes_to_full(const ce_bay_t *bay)
{
    return bay->minutes_to_full;
}

float ce_observed_peak_ma(const ce_bay_t *bay)
{
    // Only a peak followed by a taper is a real CC->CV transition.
    return bay->in_taper && bay->max_current_ma != bay->cfg.peak_ma ? bay->max_current_ma : 0.0f;
}

int ce_describe(const ce_bay_t *bay, char *buf, unsigned len)
{
    switch (bay->state) {
    case CE_EMPTY:
        return snprintf(buf, len, "empty");
    case CE_SETTLING:
        return snprintf(buf, len, "checking");
    case CE_FULL:
        return snprintf(buf, len, "100%%");
    case CE_CHARGING:
    default: {
        int minutes = (int)(bay->minutes_to_full + 0.5f);
        if (minutes >= 90) {
            return snprintf(buf, len, "~%d%%  %dh%02d", (int)(bay->pct + 0.5f), minutes / 60,
                            minutes % 60);
        }
        return snprintf(buf, len, "~%d%%  %d min", (int)(bay->pct + 0.5f), minutes < 1 ? 1 : minutes);
    }
    }
}
