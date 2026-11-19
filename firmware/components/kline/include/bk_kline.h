#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
esp_err_t bk_kline_init(void);
void bk_kline_disarm(void);
esp_err_t bk_kline_exchange(const uint8_t *command,size_t n,uint8_t *reply,size_t cap,size_t *reply_n,unsigned timeout_ms);
