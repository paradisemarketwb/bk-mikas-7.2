#pragma once
#include "bk_history.h"
#include "esp_err.h"
esp_err_t bk_journal_restore(bk_history_t *history);
esp_err_t bk_journal_save(const bk_history_t *history);
