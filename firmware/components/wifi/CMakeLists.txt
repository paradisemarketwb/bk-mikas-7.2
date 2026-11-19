idf_component_register(SRCS "bk_wifi.c" INCLUDE_DIRS "include" REQUIRES telemetry esp_wifi esp_netif esp_event esp_http_server esp_timer nvs_flash json esp_hw_support)
