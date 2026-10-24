// Grogu: the controller-stand's ESP32-S3. Reads each bay, runs the charge estimator and
// draws the result on the round display. Every second it also prints a CSV line per
// docked bay for calibration (CONT-9): "csv,<ms>,<bay>,<mA>,<mV>,<state>,<pct>".
#include <stdio.h>
#include <string.h>

#include "bays.h"
#include "charge_estimator.h"
#include "display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fleet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "sim.h"

#define POLL_MS 250
#define LOG_EVERY_MS 1000

static const char *TAG = "grogu";

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

void app_main(void)
{
    ESP_ERROR_CHECK(display_init());
    // Fleet OTA: returns at once and connects in the background. A new image is kept
    // once it reaches IoT Core, or rolled back after 120 s.
    fleet_start();
#ifndef CONFIG_GROGU_SIMULATE
    ESP_ERROR_CHECK(bays_init());
#else
    ESP_LOGW(TAG, "simulating bays (CONFIG_GROGU_SIMULATE)");
#endif

    ce_bay_t est[BAY_COUNT];
    ce_config_t cfg = ce_config_default();
    for (int i = 0; i < BAY_COUNT; i++) {
        ce_init(&est[i], &cfg);
    }

    bay_view_t views[BAY_COUNT] = {0};
    ce_state_t last_state[BAY_COUNT] = {CE_EMPTY, CE_EMPTY};
    uint32_t last_change_ms = now_ms();
    uint32_t last_log_ms = 0;
    bool dimmed = false;

    for (;;) {
        uint32_t now = now_ms();
        bay_sample_t samples[BAY_COUNT];
#ifdef CONFIG_GROGU_SIMULATE
        sim_poll(now, samples);
        // The estimator must see simulated time, or a sped-up charge looks like noise.
        uint32_t est_now = now * CONFIG_GROGU_SIM_SPEEDUP;
#else
        bays_poll(now, samples);
        uint32_t est_now = now;
#endif
        bool log_now = now - last_log_ms >= LOG_EVERY_MS;
        if (log_now) {
            last_log_ms = now;
        }

        for (int i = 0; i < BAY_COUNT; i++) {
            ce_update(&est[i], est_now, samples[i].present, samples[i].current_ma);
            views[i].state = ce_state(&est[i]);
            views[i].pct = ce_percent(&est[i]);
            views[i].fault = samples[i].fault;
            ce_describe(&est[i], views[i].text, sizeof(views[i].text));

            if (views[i].state != last_state[i]) {
                // Dock, undock, or charged: worth waking the screen for.
                last_state[i] = views[i].state;
                last_change_ms = now;
            }
            if (log_now && samples[i].present) {
                printf("csv,%lu,%d,%.1f,%.0f,%d,%.1f\n", (unsigned long)now, i, samples[i].current_ma,
                       samples[i].bus_mv, views[i].state, views[i].pct);
            }
        }

        display_update(views);

        bool want_dim = now - last_change_ms >= CONFIG_GROGU_DIM_AFTER_S * 1000u;
        if (want_dim != dimmed) {
            dimmed = want_dim;
            display_set_dim(dimmed);
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}
