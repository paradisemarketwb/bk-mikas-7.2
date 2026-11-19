#pragma once
#include "esp_err.h"
#include "bk_telemetry.h"
typedef void (*bk_snapshot_fn)(bk_snapshot_t *out);
esp_err_t bk_wifi_start(bk_snapshot_fn snapshot);
const char *bk_wifi_ssid(void);
const char *bk_wifi_password(void);
