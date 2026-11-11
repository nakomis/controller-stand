/* Wi-Fi: join the network from the prov partition (or, for development builds, sdkconfig.secrets),
 * retrying until it works. Station only: Grogu has no web page and no access point. */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

/* ssid/password: from the prov partition, or NULL to use sdkconfig.secrets'. */
esp_err_t net_start(const char *ssid, const char *password);

/* True once the network has given us an address. */
bool net_up(void);
