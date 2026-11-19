idf_component_register(SRCS "bk_settings.c" "bk_profile_json.c" "bk_profile.c" "bk_profile_storage.c" INCLUDE_DIRS "include" REQUIRES esp_partition nvs_flash ui_config json sources)
