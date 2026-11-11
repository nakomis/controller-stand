idf_component_register(SRCS main.c ina226.c bays.c display.c sim.c
                            fleet.c net.c ota.c ota_cmd.c prov.c
                       INCLUDE_DIRS "."
                       REQUIRES charge_estimator esp_driver_i2c esp_driver_gpio esp_driver_spi esp_lcd esp_timer
                       PRIV_REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_app_format app_update
                                     esp_http_client mqtt json mbedtls
                       # Amazon Root CA 1 (public), for IoT Core and the update download. The device's own
                       # certificate, key and Wi-Fi are never in the image: they're in the prov partition.
                       EMBED_TXTFILES ${COMPONENT_DIR}/../AmazonRootCA1.pem)
