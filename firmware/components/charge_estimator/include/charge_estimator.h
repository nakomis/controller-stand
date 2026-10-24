// Charge-state estimator for one DualSense dock bay.
//
// The dock pads carry no data, so the controller's battery level can't be read. This
// estimates it from the bay's 5 V input current, sampled by an INA226:
//
//   CC phase   The charger pushes a constant current into the battery. The *input*
//              current at 5 V still rises gently, because it scales with battery
//              voltage (P_in = V_bat * I_chg / efficiency), and V_bat climbs from
//              ~3.6 V to 4.2 V across the phase. So the ratio of input current to the
//              peak input current (reached as CV starts) gives a rough level, 0-80 %.
//   CV taper   Battery voltage is held at 4.2 V and the current decays. Where the
//              current sits between peak and the full threshold gives 80-99 %.
//   Full       Current below the full threshold for a while: 100 %.
//
// Once a starting level has been settled, mAh delivered since docking carries the
// estimate forward, so it moves smoothly rather than jumping with noise.
//
// All constants in ce_config_default() are PROVISIONAL until CONT-9 fits them to real
// charge logs.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CE_EMPTY = 0,   // no controller docked
    CE_SETTLING,    // docked, gathering readings before the first estimate
    CE_CHARGING,
    CE_FULL,
} ce_state_t;

typedef struct {
    float peak_ma;              // input current at the CC->CV transition
    float cc_start_ratio;       // input/peak ratio at ~0 % (battery at ~3.6 V)
    float cc_end_pct;           // level at the CC->CV transition
    float taper_detect_ratio;   // current below peak*this means the taper has begun
    float full_ma;              // below this (for full_hold_ms) means full
    uint32_t full_hold_ms;
    float capacity_mah;         // input-side mAh from flat to full
    float taper_minutes;        // typical duration of the whole CV taper
    uint32_t settle_ms;         // readings to gather before the first estimate
    float filter_tau_ms;        // EMA time constant for the current
} ce_config_t;

typedef struct {
    ce_config_t cfg;
    ce_state_t state;
    uint32_t docked_at_ms;
    uint32_t last_ms;
    uint32_t below_full_since_ms;
    bool below_full;
    bool in_taper;
    bool has_sample;
    float current_ma;           // filtered
    float max_current_ma;       // highest filtered current seen this dock
    float start_pct;            // level settled at the end of CE_SETTLING
    float delivered_mah;        // since the end of CE_SETTLING
    float pct;
    float minutes_to_full;
} ce_bay_t;

ce_config_t ce_config_default(void);

void ce_init(ce_bay_t *bay, const ce_config_t *cfg);

// Feed one sample. present comes from presence detection (sense pin or probe);
// current_ma is the bay's 5 V input current. now_ms must be monotonic.
void ce_update(ce_bay_t *bay, uint32_t now_ms, bool present, float current_ma);

ce_state_t ce_state(const ce_bay_t *bay);
// 0-100. Meaningful in CE_CHARGING and CE_FULL.
float ce_percent(const ce_bay_t *bay);
// Meaningful in CE_CHARGING; 0 when full.
float ce_minutes_to_full(const ce_bay_t *bay);

// Short status text for the display: "~60%  40 min", "~92%  9 min", "100%",
// "empty" or "checking". Returns the number of characters written (as snprintf).
int ce_describe(const ce_bay_t *bay, char *buf, unsigned len);

// Peak current observed at this dock's CC->CV transition, or 0 if not seen. The
// firmware can persist it and feed it back in as cfg.peak_ma, so the CC-phase estimate
// learns each controller's (well, each bay's) real peak.
float ce_observed_peak_ma(const ce_bay_t *bay);

#ifdef __cplusplus
}
#endif
