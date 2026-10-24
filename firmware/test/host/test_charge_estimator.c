// Host tests for the charge estimator. Each test drives the estimator with a simple
// model of a DualSense charging from a given level, then checks what it reports.
#include "charge_estimator.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int checks;

#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            failures++;                                                    \
            printf("  FAIL %s:%d: %s — ", __FILE__, __LINE__, #cond);      \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)

#define STEP_MS 1000u

// --- A toy charger -------------------------------------------------------------------
// CC to 80 %, with input current rising from 0.86*peak to peak as battery voltage
// climbs; then an exponential CV taper to the full threshold over ~45 min; then a
// small standby draw.

typedef struct {
    float level;        // true level, 0-100
    float peak_ma;
    float capacity_mah;
    float taper_ma;     // current during the taper, decaying
    float noise_ma;     // peak-to-peak uniform noise
} charger_t;

static float noise(float amplitude)
{
    return amplitude * ((float)rand() / (float)RAND_MAX - 0.5f);
}

static float charger_step(charger_t *ch, uint32_t dt_ms)
{
    float ma;
    if (ch->level < 80.0f) {
        ma = ch->peak_ma * (0.86f + 0.14f * ch->level / 80.0f);
        ch->taper_ma = ch->peak_ma;
    } else if (ch->level < 100.0f) {
        // Decay so that 80 -> 100 takes roughly 45 minutes.
        ch->taper_ma *= expf(-(float)dt_ms / (15.0f * 60000.0f));
        ma = ch->taper_ma;
    } else {
        ma = 20.0f;
    }
    if (ch->level < 100.0f) {
        if (ch->level < 80.0f) {
            ch->level += ma * (float)dt_ms / 3600000.0f / ch->capacity_mah * 100.0f;
        } else {
            // Taper levels are driven by time so the model ends where we expect.
            ch->level += 20.0f * (float)dt_ms / (45.0f * 60000.0f);
        }
        if (ch->level > 100.0f) ch->level = 100.0f;
    }
    return ma + noise(ch->noise_ma);
}

static charger_t charger(float level)
{
    charger_t ch = {.level = level, .peak_ma = 900.0f, .capacity_mah = 1360.0f, .noise_ma = 0.0f};
    if (level >= 80.0f) {
        ch.taper_ma = 900.0f * expf(-(level - 80.0f) / 20.0f * 3.0f);
    }
    return ch;
}

// Run the charger and estimator together for `seconds`.
static void run(ce_bay_t *bay, charger_t *ch, uint32_t *now, uint32_t seconds)
{
    for (uint32_t i = 0; i < seconds; i++) {
        *now += STEP_MS;
        ce_update(bay, *now, true, charger_step(ch, STEP_MS));
    }
}

static ce_bay_t fresh_bay(void)
{
    ce_bay_t bay;
    ce_config_t cfg = ce_config_default();
    ce_init(&bay, &cfg);
    return bay;
}

// --- Tests -----------------------------------------------------------------------------

static void test_empty_when_absent(void)
{
    ce_bay_t bay = fresh_bay();
    for (uint32_t t = 0; t < 10000; t += STEP_MS) {
        ce_update(&bay, t, false, 0.0f);
    }
    CHECK(ce_state(&bay) == CE_EMPTY, "state %d", ce_state(&bay));
}

static void test_settles_before_estimating(void)
{
    ce_bay_t bay = fresh_bay();
    charger_t ch = charger(40.0f);
    uint32_t now = 0;
    run(&bay, &ch, &now, 10);
    CHECK(ce_state(&bay) == CE_SETTLING, "state %d after 10 s", ce_state(&bay));
    run(&bay, &ch, &now, 30);
    CHECK(ce_state(&bay) == CE_CHARGING, "state %d after 40 s", ce_state(&bay));
}

static void test_cc_phase_estimate(void)
{
    const float starts[] = {5.0f, 20.0f, 40.0f, 60.0f};
    for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); i++) {
        ce_bay_t bay = fresh_bay();
        charger_t ch = charger(starts[i]);
        uint32_t now = 0;
        run(&bay, &ch, &now, 60);
        float err = fabsf(ce_percent(&bay) - ch.level);
        CHECK(err < 8.0f, "start %.0f%%: estimate %.1f vs true %.1f", starts[i], ce_percent(&bay),
              ch.level);
        CHECK(ce_minutes_to_full(&bay) > 45.0f, "start %.0f%%: %.1f min", starts[i],
              ce_minutes_to_full(&bay));
    }
}

static void test_full_charge_tracks_and_latches(void)
{
    ce_bay_t bay = fresh_bay();
    charger_t ch = charger(30.0f);
    uint32_t now = 0;
    float last_pct = 0.0f, last_min = 1e9f;
    int pct_drops = 0, min_rises = 0;
    bool saw_taper = false;

    for (int minute = 0; minute < 240 && ce_state(&bay) != CE_FULL; minute++) {
        run(&bay, &ch, &now, 60);
        if (ce_state(&bay) != CE_CHARGING) continue;
        if (ce_percent(&bay) + 0.5f < last_pct) pct_drops++;
        if (ce_minutes_to_full(&bay) > last_min + 2.0f) min_rises++;
        last_pct = ce_percent(&bay);
        last_min = ce_minutes_to_full(&bay);
        if (ch.level > 85.0f && ch.level < 98.0f) {
            saw_taper = true;
            CHECK(fabsf(ce_percent(&bay) - ch.level) < 10.0f, "taper: estimate %.1f vs true %.1f",
                  ce_percent(&bay), ch.level);
        }
    }
    CHECK(saw_taper, "never observed the taper");
    CHECK(pct_drops == 0, "percentage went backwards %d times", pct_drops);
    CHECK(min_rises <= 1, "minutes-to-full rose %d times", min_rises);
    CHECK(ce_state(&bay) == CE_FULL, "state %d after 4 h", ce_state(&bay));
    CHECK(ce_percent(&bay) == 100.0f, "pct %.1f", ce_percent(&bay));
    CHECK(ce_minutes_to_full(&bay) == 0.0f, "min %.1f", ce_minutes_to_full(&bay));
    CHECK(ce_observed_peak_ma(&bay) > 850.0f, "observed peak %.0f", ce_observed_peak_ma(&bay));

    // A brief top-up draw doesn't drop it out of full.
    for (int i = 0; i < 120; i++) {
        now += STEP_MS;
        ce_update(&bay, now, true, 300.0f);
    }
    CHECK(ce_state(&bay) == CE_FULL, "state %d after top-up", ce_state(&bay));
}

static void test_docked_mid_taper(void)
{
    ce_bay_t bay = fresh_bay();
    charger_t ch = charger(90.0f);
    uint32_t now = 0;
    run(&bay, &ch, &now, 60);
    CHECK(ce_state(&bay) == CE_CHARGING, "state %d", ce_state(&bay));
    CHECK(ce_percent(&bay) >= 80.0f && ce_percent(&bay) < 100.0f, "pct %.1f", ce_percent(&bay));
    CHECK(ce_minutes_to_full(&bay) < 45.0f, "min %.1f", ce_minutes_to_full(&bay));
    CHECK(ce_observed_peak_ma(&bay) == 0.0f, "peak %.0f should be unobserved",
          ce_observed_peak_ma(&bay));
}

static void test_docked_already_full(void)
{
    ce_bay_t bay = fresh_bay();
    uint32_t now = 0;
    for (int i = 0; i < 120; i++) {
        now += STEP_MS;
        ce_update(&bay, now, true, 20.0f);
    }
    CHECK(ce_state(&bay) == CE_FULL, "state %d", ce_state(&bay));
}

static void test_undock_resets(void)
{
    ce_bay_t bay = fresh_bay();
    charger_t ch = charger(50.0f);
    uint32_t now = 0;
    run(&bay, &ch, &now, 120);
    CHECK(ce_state(&bay) == CE_CHARGING, "state %d", ce_state(&bay));

    now += STEP_MS;
    ce_update(&bay, now, false, 0.0f);
    CHECK(ce_state(&bay) == CE_EMPTY, "state %d after undock", ce_state(&bay));
    CHECK(ce_percent(&bay) == 0.0f, "pct %.1f after undock", ce_percent(&bay));

    charger_t other = charger(10.0f);
    run(&bay, &other, &now, 5);
    CHECK(ce_state(&bay) == CE_SETTLING, "state %d after redock", ce_state(&bay));
    run(&bay, &other, &now, 60);
    CHECK(fabsf(ce_percent(&bay) - other.level) < 8.0f, "redock estimate %.1f vs %.1f",
          ce_percent(&bay), other.level);
}

static void test_noisy_samples(void)
{
    srand(42);
    ce_bay_t bay = fresh_bay();
    charger_t ch = charger(45.0f);
    ch.noise_ma = 120.0f;
    uint32_t now = 0;
    run(&bay, &ch, &now, 60);
    float prev = ce_percent(&bay);
    int drops = 0;
    for (int i = 0; i < 30; i++) {
        run(&bay, &ch, &now, 60);
        if (ce_percent(&bay) + 0.5f < prev) drops++;
        prev = ce_percent(&bay);
    }
    CHECK(ce_state(&bay) == CE_CHARGING, "state %d", ce_state(&bay));
    CHECK(drops == 0, "noisy estimate went backwards %d times", drops);
    CHECK(fabsf(ce_percent(&bay) - ch.level) < 10.0f, "noisy estimate %.1f vs %.1f",
          ce_percent(&bay), ch.level);
}

static void test_describe(void)
{
    char buf[32];
    ce_bay_t bay = fresh_bay();
    ce_describe(&bay, buf, sizeof(buf));
    CHECK(strcmp(buf, "empty") == 0, "'%s'", buf);

    bay.state = CE_SETTLING;
    ce_describe(&bay, buf, sizeof(buf));
    CHECK(strcmp(buf, "checking") == 0, "'%s'", buf);

    bay.state = CE_CHARGING;
    bay.pct = 59.6f;
    bay.minutes_to_full = 40.2f;
    ce_describe(&bay, buf, sizeof(buf));
    CHECK(strcmp(buf, "~60%  40 min") == 0, "'%s'", buf);

    bay.minutes_to_full = 125.0f;
    ce_describe(&bay, buf, sizeof(buf));
    CHECK(strcmp(buf, "~60%  2h05") == 0, "'%s'", buf);

    bay.minutes_to_full = 0.2f;
    ce_describe(&bay, buf, sizeof(buf));
    CHECK(strcmp(buf, "~60%  1 min") == 0, "'%s'", buf);

    bay.state = CE_FULL;
    ce_describe(&bay, buf, sizeof(buf));
    CHECK(strcmp(buf, "100%") == 0, "'%s'", buf);
}

int main(void)
{
    struct {
        const char *name;
        void (*fn)(void);
    } tests[] = {
        {"empty when absent", test_empty_when_absent},
        {"settles before estimating", test_settles_before_estimating},
        {"CC phase estimate", test_cc_phase_estimate},
        {"full charge tracks and latches", test_full_charge_tracks_and_latches},
        {"docked mid-taper", test_docked_mid_taper},
        {"docked already full", test_docked_already_full},
        {"undock resets", test_undock_resets},
        {"noisy samples", test_noisy_samples},
        {"describe", test_describe},
    };
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        int before = failures;
        tests[i].fn();
        printf("%s %s\n", failures == before ? "ok  " : "FAIL", tests[i].name);
    }
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
