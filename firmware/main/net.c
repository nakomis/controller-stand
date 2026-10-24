#include "net.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "sdkconfig.h"

#define RETRY_US (5 * 1000 * 1000)

static const char *TAG = "net";
static esp_timer_handle_t s_retry;
static volatile bool s_have_ip;
static char s_ssid[33];

static void retry(void *arg)
{
    esp_wifi_connect();
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_have_ip = false;
        esp_timer_start_once(s_retry, RETRY_US);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        s_have_ip = true;
        ESP_LOGI(TAG, "joined %s as " IPSTR, s_ssid, IP2STR(&e->ip_info.ip));
    }
}

esp_err_t net_start(const char *ssid, const char *password)
{
    char pass[65];
    /* The prov partition's network if there is one, else sdkconfig.secrets' (development builds). */
    strlcpy(s_ssid, ssid && ssid[0] ? ssid : CONFIG_GROGU_WIFI_SSID, sizeof s_ssid);
    strlcpy(pass, ssid && ssid[0] ? password : CONFIG_GROGU_WIFI_PASSWORD, sizeof pass);
    if (!s_ssid[0]) {
        ESP_LOGW(TAG, "no network configured (prov partition or sdkconfig.secrets): no Wi-Fi");
        return ESP_ERR_NOT_FOUND;
    }
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(sta, CONFIG_GROGU_HOSTNAME);

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), TAG, "wifi");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL), TAG, "events");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL), TAG, "events");
    esp_timer_create_args_t t = {.callback = retry, .name = "wifi-retry"};
    ESP_RETURN_ON_ERROR(esp_timer_create(&t, &s_retry), TAG, "timer");

    wifi_config_t sta_cfg = {0};
    strlcpy((char *)sta_cfg.sta.ssid, s_ssid, sizeof sta_cfg.sta.ssid);
    strlcpy((char *)sta_cfg.sta.password, pass, sizeof sta_cfg.sta.password);
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg), TAG, "config");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");
    memset(pass, 0, sizeof pass);
    return ESP_OK;
}

bool net_up(void)
{
    return s_have_ip;
}
