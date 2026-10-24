/* The Fleet client (from Katooni's, itself from tinycrypt's notifier.c): MQTT to AWS IoT Core with this
 * Grogu's own certificate, taking commands on controller-stand/<thing>/commands and answering on
 * controller-stand/<thing>/responses. ota.c does the work.
 *
 * main.c needs only fleet_start(). */
#pragma once

#include <stdbool.h>

/* One call, once, from app_main (after the display is up, which is the slow part of boot):
 *   1. initialises NVS (the default partition, tolerating main having done so),
 *   2. loads the prov partition (Wi-Fi, IoT endpoint, Thing, certificate),
 *   3. runs the self-test (NVS, and the charge estimator on a canned sequence) and, if this image is
 *      on probation, rolls back at once should it fail,
 *   4. starts Wi-Fi and the MQTT client.
 * Returns immediately: connecting happens in the background. An image on probation is kept only once
 * it has reached IoT Core (within OTA_VERIFY_DEADLINE_S), otherwise it is rolled back, so nothing else
 * has to be marked healthy. An unprovisioned board (no prov partition) does everything but 3's
 * rollback decision and 4: it is kept immediately and runs without Fleet.
 * Returns true if Fleet was started. */
bool fleet_start(void);

/* Queue a JSON response for Fleet; dropped (false) while disconnected. */
bool fleet_respond(const char *json);

/* Publish a status document on controller-stand/<thing>/status (QoS 0, not retained). A no-op
 * returning false while MQTT is down or before fleet_start(). */
bool fleet_publish_status(const char *json);
