#pragma once
#include "bk_background.h"
#include "bk_history.h"
#include "bk_settings.h"
#include "bk_telemetry.h"
#include "esp_err.h"
typedef void (*bk_snapshot_fn)(bk_snapshot_t *out);
typedef struct {
  bk_snapshot_fn snapshot;
  const char *transport_name;
  bool (*adapter_ready)(void);
  void (*history)(bk_history_t *out);
  void (*config_get)(bk_settings_t *out);
  esp_err_t (*config_apply)(const bk_settings_t *value);
  bool (*ota_prepare)(void);
  bool (*background_ready)(void);
  esp_err_t (*background_load)(unsigned slot, bk_background_info_t *info,
                               void *pixels, size_t capacity);
  esp_err_t (*background_store)(unsigned slot, uint16_t width, uint16_t height,
                                const void *pixels, size_t length);
} bk_wifi_hooks_t;
esp_err_t bk_wifi_start(const bk_wifi_hooks_t *hooks);
const char *bk_wifi_ssid(void);
const char *bk_wifi_password(void);
unsigned bk_wifi_clients(void);
