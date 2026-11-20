#pragma once
#ifdef ESP_PLATFORM
#include "esp_err.h"
#endif
#include "bk_sources.h"
#include "bk_ui_config.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BK_SETTINGS_VERSION 4
#define BK_PROFILE_JSON_MAX 65536u
typedef struct {
  uint32_t version;
  uint8_t screen, brightness, dark, reserved;
  uint8_t selected[5], padding[3];
  bk_layout_t layout;
  bk_calibration_t maf, lambda;
  bk_source_policy_t source_policies[BK_METRIC_COUNT];
} bk_settings_t;
void bk_settings_defaults(bk_settings_t *out);
bool bk_settings_valid(const bk_settings_t *value);
#ifdef ESP_PLATFORM
esp_err_t bk_settings_load(bk_settings_t *out);
esp_err_t bk_settings_save(const bk_settings_t *value);
#endif
/* Legacy v3 ABI and bounded RLE envelope; no flash erasure on migration. */
bool bk_settings_migrate_v3(const void *blob, size_t size, bk_settings_t *out);
size_t bk_settings_encode(const bk_settings_t *value, void *buffer,
                          size_t capacity);
bool bk_settings_decode(const void *buffer, size_t size, bk_settings_t *out);
void bk_settings_reset_screens(bk_settings_t *value);
void bk_settings_reset_calibrations(bk_settings_t *value);
/* Caller frees JSON with cJSON_free. Import is all-or-nothing. */
char *bk_settings_to_json(const bk_settings_t *value);
bool bk_settings_from_json(const char *json, bk_settings_t *out);
