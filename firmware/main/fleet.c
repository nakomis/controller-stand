#include "fleet.h"

#include <stdio.h>
#include <string.h>

#include "charge_estimator.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "mqtt_client.h"
#include "net.h"
#include "nvs_flash.h"
#include "ota.h"
#include "prov.h"
#include "sdkconfig.h"

/* Fleet's commands can carry a presigned URL of ~1.5 KB (longer ones arrive in chunks). */
#define MQTT_BUFFER_SIZE 4096
#define FLEET_PROJECT "controller-stand"

extern const char root_ca_start[] asm("_binary_AmazonRootCA1_pem_start");

static const char *TAG = "fleet";
static prov_t s_prov;
static char s_commands[sizeof(((prov_t *)0)->thing_name) + 32];
static char s_responses[sizeof(((prov_t *)0)->thing_name) + 32];
static char s_status[sizeof(((prov_t *)0)->thing_name) + 32];
static int s_subscribe_id = -1;
static esp_mqtt_client_handle_t s_mqtt;
static volatile bool s_connected;

/* ---- self-test ---------------------------------------------------------------------------- */

/* The charge estimator on a canned sequence: a controller docks drawing 400 mA, so the bay leaves
 * "empty"; the current then falls to nothing for long enough that it reads "full" at 100 %; undocking
 * returns it to "empty". Deterministic, and needs no hardware. */
static bool estimator_ok(void)
{
    ce_bay_t bay;
    const ce_config_t cfg = ce_config_default();
    ce_init(&bay, &cfg);
    uint32_t t = 1000;
    for (int i = 0; i < 20; i++, t += 100) ce_update(&bay, t, true, 400.0f);
    if (ce_state(&bay) == CE_EMPTY || ce_state(&bay) == CE_FULL) return false;
    const uint32_t end = t + cfg.full_hold_ms + 5000;
    for (; t <= end; t += 100) ce_update(&bay, t, true, 0.0f);
    if (ce_state(&bay) != CE_FULL || ce_percent(&bay) < 99.9f) return false;
    ce_update(&bay, t, false, 0.0f);
    return ce_state(&bay) == CE_EMPTY;
}

/* What a freshly updated image must manage before it's kept. No hardware: a board with a missing part
 * must still be updatable. */
static bool selftest(void)
{
#if CONFIG_GROGU_OTA_FAIL_SELFTEST
    ESP_LOGE(TAG, "TEST BUILD: failing the self-test on purpose");
    return false;
#endif
    nvs_stats_t stats;
    bool nvs_ok = nvs_get_stats(NULL, &stats) == ESP_OK;
    bool est_ok = estimator_ok();
    if (!nvs_ok || !est_ok) ESP_LOGE(TAG, "self-test failed: nvs %s, estimator %s", nvs_ok ? "ok" : "BAD", est_ok ? "ok" : "BAD");
    return nvs_ok && est_ok;
}

/* ---- MQTT --------------------------------------------------------------------------------- */

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t e = data;
    switch ((esp_mqtt_event_id_t)id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "connected to IoT Core as %s", s_prov.thing_name);
        s_subscribe_id = esp_mqtt_client_subscribe(s_mqtt, s_commands, 1);   /* clean session: every time */
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "disconnected");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        if (e->msg_id != s_subscribe_id) break;
        if (e->error_handle->error_type == MQTT_ERROR_TYPE_SUBSCRIBE_FAILED) {
            ESP_LOGE(TAG, "subscription to %s refused", s_commands);
        } else {
            ESP_LOGI(TAG, "listening for Fleet on %s", s_commands);
            ota_on_connected();
        }
        break;
    case MQTT_EVENT_DATA:
        if (e->current_data_offset != 0 || e->data_len != e->total_data_len) {
            ESP_LOGW(TAG, "command of %d bytes doesn't fit the %d-byte buffer", e->total_data_len, MQTT_BUFFER_SIZE);
            if (e->current_data_offset == 0) fleet_respond("{\"type\":\"error\",\"message\":\"Command too long\"}");
            break;
        }
        ota_handle_command(e->data, (size_t)e->data_len);
        break;
    case MQTT_EVENT_ERROR:
        if (e->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
            ESP_LOGW(TAG, "transport error: tls 0x%x, stack 0x%x, errno %d", e->error_handle->esp_tls_last_esp_err,
                     e->error_handle->esp_tls_stack_err, e->error_handle->esp_transport_sock_errno);
        else
            ESP_LOGW(TAG, "error type %d, connect return code %d", e->error_handle->error_type,
                     e->error_handle->connect_return_code);
        break;
    default:
        break;
    }
}

static bool mqtt_start(void)
{
    snprintf(s_commands, sizeof s_commands, FLEET_PROJECT "/%s/commands", s_prov.thing_name);
    snprintf(s_responses, sizeof s_responses, FLEET_PROJECT "/%s/responses", s_prov.thing_name);
    snprintf(s_status, sizeof s_status, FLEET_PROJECT "/%s/status", s_prov.thing_name);

    /* TLS needs the time to check certificates. */
    esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&sntp);

    static char uri[sizeof s_prov.iot_endpoint + 16];
    snprintf(uri, sizeof uri, "mqtts://%s:8883", s_prov.iot_endpoint);
    const esp_mqtt_client_config_t config = {
        .broker = {.address.uri = uri, .verification.certificate = root_ca_start},
        .credentials = {
            .client_id = s_prov.thing_name,
            .authentication = {.certificate = s_prov.device_cert, .key = s_prov.device_key},
        },
        .session.keepalive = 60,
        .network.reconnect_timeout_ms = 5000,
        .buffer.size = MQTT_BUFFER_SIZE,
    };
    s_mqtt = esp_mqtt_client_init(&config);
    if (!s_mqtt || esp_mqtt_client_register_event(s_mqtt, ESP_EVENT_ANY_ID, on_event, NULL) != ESP_OK ||
        esp_mqtt_client_start(s_mqtt) != ESP_OK) {
        ESP_LOGE(TAG, "MQTT client failed to start: no Fleet");
        return false;
    }
    ESP_LOGI(TAG, "starting: IoT Core %s, %s", s_prov.iot_endpoint, s_commands);
    return true;
}

bool fleet_start(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    bool nvs_ok = err == ESP_OK;
    if (!nvs_ok) ESP_LOGE(TAG, "NVS: %s", esp_err_to_name(err));

    bool provisioned = prov_load(&s_prov);
    bool fleet = provisioned && CONFIG_GROGU_FLEET;
    ota_boot(nvs_ok && selftest(), fleet);

    /* Without the prov partition, Wi-Fi from sdkconfig.secrets (if any) still comes up, but there is
     * no certificate, so no Fleet. */
    if (net_start(provisioned ? s_prov.wifi_ssid : NULL, provisioned ? s_prov.wifi_password : NULL) != ESP_OK) {
        ESP_LOGW(TAG, "no network");
        return false;
    }
    return fleet && mqtt_start();
}

bool fleet_respond(const char *json)
{
    if (!s_mqtt || !s_connected) {
        ESP_LOGW(TAG, "not connected: response dropped: %s", json);
        return false;
    }
    return esp_mqtt_client_enqueue(s_mqtt, s_responses, json, 0, 1, 0, true) >= 0;
}

bool fleet_publish_status(const char *json)
{
    if (!s_mqtt || !s_connected || !json) return false;
    return esp_mqtt_client_enqueue(s_mqtt, s_status, json, 0, 0, 0, false) >= 0;
}
