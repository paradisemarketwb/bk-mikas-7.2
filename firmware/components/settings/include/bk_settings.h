#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct {uint32_t version;uint8_t screen;uint8_t brightness;uint8_t dark;uint8_t reserved;uint8_t selected[5];uint8_t padding[3];} bk_settings_t;
esp_err_t bk_settings_load(bk_settings_t *out);
esp_err_t bk_settings_save(const bk_settings_t *value);
