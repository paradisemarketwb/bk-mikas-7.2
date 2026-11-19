#pragma once
#ifdef ESP_PLATFORM
#include "esp_err.h"
#endif
#include "bk_ui_config.h"
#include <stdbool.h>
#include <stdint.h>
#define BK_SETTINGS_VERSION 3
typedef struct {
  uint32_t version;
  uint8_t screen, brightness, dark, reserved;
  uint8_t selected[5], padding[3];
  bk_layout_t layout;
} bk_settings_t;
void bk_settings_defaults(bk_settings_t *out);
bool bk_settings_valid(const bk_settings_t *value);
#ifdef ESP_PLATFORM
esp_err_t bk_settings_load(bk_settings_t *out);
esp_err_t bk_settings_save(const bk_settings_t *value);
#endif
/* Caller frees JSON with cJSON_free. Import is all-or-nothing. */
char *bk_settings_to_json(const bk_settings_t *value);
bool bk_settings_from_json(const char *json, bk_settings_t *out);
