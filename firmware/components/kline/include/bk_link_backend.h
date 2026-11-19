#pragma once
#include "esp_err.h"
#include "mikas_link.h"
esp_err_t bk_backend_init(void);
void bk_backend_disarm(void);
bool bk_backend_ready(void);
const mikas_stream_t *bk_backend_stream(void);
