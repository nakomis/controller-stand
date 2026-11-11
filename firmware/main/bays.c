#include "bays.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "ina226.h"
#include "pins.h"
#include "sdkconfig.h"

// Presence detection is provisional until CONT-1 pins down the sense line. For now an
// empty bay is probed: its switch is turned on briefly and a controller shows up as
// current. A docked bay stays powered until its current stays near zero, which assumes
// a full controller still draws a little standby current. If it doesn't, the sense pin
// has to take over.
#define ABSENT_MA      2.0f
#define ABSENT_HOLD_MS 5000
#define FAULT_BACKOFF_MS 10000

static const char *TAG = "bays";

typedef enum { ST_IDLE, ST_PROBING, ST_DOCKED, ST_FAULT } bay_phase_t;

typedef struct {
    gpio_num_t en;
    gpio_num_t fault;
    uint8_t addr;
    ina226_t ina;
    bool ina_ok;
    bay_phase_t phase;
    uint32_t phase_since_ms;
    uint32_t low_since_ms;
    bool low;
} bay_hw_t;

static bay_hw_t s_bays[BAY_COUNT] = {
    [BAY_FRONT] = {.en = PIN_FRONT_EN, .fault = PIN_FRONT_FAULT, .addr = INA226_ADDR_FRONT},
    [BAY_BACK] = {.en = PIN_BACK_EN, .fault = PIN_BACK_FAULT, .addr = INA226_ADDR_BACK},
};

static void set_phase(bay_hw_t *b, bay_phase_t phase, uint32_t now_ms)
{
    b->phase = phase;
    b->phase_since_ms = now_ms;
    gpio_set_level(b->en, phase == ST_PROBING || phase == ST_DOCKED);
}

esp_err_t bays_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &bus), TAG, "i2c bus");

    for (int i = 0; i < BAY_COUNT; i++) {
        bay_hw_t *b = &s_bays[i];
        gpio_config_t out = {.pin_bit_mask = 1ULL << b->en, .mode = GPIO_MODE_OUTPUT};
        gpio_config(&out);
        gpio_set_level(b->en, 0);
        gpio_config_t in = {
            .pin_bit_mask = 1ULL << b->fault,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
        };
        gpio_config(&in);

        esp_err_t err = ina226_init(&b->ina, bus, b->addr, (float)CONFIG_GROGU_SHUNT_MILLIOHM);
        b->ina_ok = err == ESP_OK;
        if (!b->ina_ok) {
            ESP_LOGE(TAG, "INA226 0x%02x: %s", b->addr, esp_err_to_name(err));
        }
        set_phase(b, ST_IDLE, 0);
    }
    return ESP_OK;
}

static void poll_one(bay_hw_t *b, uint32_t now_ms, bay_sample_t *out)
{
    *out = (bay_sample_t){0};
    if (b->ina_ok) {
        if (ina226_read(&b->ina, &out->current_ma, &out->bus_mv) != ESP_OK) {
            out->current_ma = 0.0f;
        }
    }
    uint32_t in_phase = now_ms - b->phase_since_ms;

    if (b->phase != ST_FAULT && b->phase != ST_IDLE && gpio_get_level(b->fault) == 0) {
        ESP_LOGW(TAG, "bay %u: load switch fault, backing off", b->addr);
        set_phase(b, ST_FAULT, now_ms);
    }

    switch (b->phase) {
    case ST_IDLE:
        if (in_phase >= CONFIG_GROGU_PROBE_PERIOD_MS) {
            set_phase(b, ST_PROBING, now_ms);
        }
        break;
    case ST_PROBING:
        if (in_phase >= CONFIG_GROGU_PROBE_ON_MS) {
            if (out->current_ma >= CONFIG_GROGU_PRESENT_MA) {
                b->low = false;
                set_phase(b, ST_DOCKED, now_ms);
            } else {
                set_phase(b, ST_IDLE, now_ms);
            }
        }
        break;
    case ST_DOCKED:
        if (out->current_ma < ABSENT_MA) {
            if (!b->low) {
                b->low = true;
                b->low_since_ms = now_ms;
            } else if (now_ms - b->low_since_ms >= ABSENT_HOLD_MS) {
                set_phase(b, ST_IDLE, now_ms);
            }
        } else {
            b->low = false;
        }
        break;
    case ST_FAULT:
        out->fault = true;
        if (in_phase >= FAULT_BACKOFF_MS) {
            set_phase(b, ST_IDLE, now_ms);
        }
        break;
    }
    out->present = b->phase == ST_DOCKED;
}

void bays_poll(uint32_t now_ms, bay_sample_t out[BAY_COUNT])
{
    for (int i = 0; i < BAY_COUNT; i++) {
        poll_one(&s_bays[i], now_ms, &out[i]);
    }
}
