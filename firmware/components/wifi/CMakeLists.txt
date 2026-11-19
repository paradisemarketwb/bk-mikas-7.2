idf_component_register(SRCS "bk_wifi.c" INCLUDE_DIRS "include"
 REQUIRES background telemetry history settings esp_wifi esp_netif esp_event esp_http_server esp_timer nvs_flash json esp_hw_support app_update esp_app_format mbedtls
 EMBED_FILES "index.html")
