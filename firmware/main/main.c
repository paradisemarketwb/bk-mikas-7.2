#include "bk_board.h"
#include "bk_history.h"
#include "bk_journal.h"
#include "bk_kline.h"
#include "bk_settings.h"
#include "bk_telemetry.h"
#include "bk_ui.h"
#include "bk_wifi.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mikas.h"
#include "nvs_flash.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *TAG = "bk";
static bk_snapshot_t store;
static bk_history_t history_store;
static bk_settings_t current_config;
static uint32_t config_revision = 1;
static SemaphoreHandle_t store_lock, history_lock, config_lock;
static QueueHandle_t settings_queue;
static atomic_bool maintenance;
typedef struct {
  uint32_t revision;
  bk_settings_t value;
} save_request_t;
static uint64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static void snapshot(bk_snapshot_t *s) {
  xSemaphoreTake(store_lock, portMAX_DELAY);
  *s = store;
  xSemaphoreGive(store_lock);
}
static void history_snapshot(bk_history_t *h) {
  xSemaphoreTake(history_lock, portMAX_DELAY);
  *h = history_store;
  xSemaphoreGive(history_lock);
}
#if CONFIG_BK_ENABLE_WIFI
static void config_get(bk_settings_t *s) {
  xSemaphoreTake(config_lock, portMAX_DELAY);
  *s = current_config;
  xSemaphoreGive(config_lock);
}
static esp_err_t config_apply(const bk_settings_t *s) {
  if (atomic_load(&maintenance) || !bk_settings_valid(s))
    return ESP_ERR_INVALID_STATE;
  xSemaphoreTake(config_lock, portMAX_DELAY);
  esp_err_t e = bk_settings_save(s);
  if (!e) {
    current_config = *s;
    config_revision++;
  }
  xSemaphoreGive(config_lock);
  return e;
}
static bool ota_prepare(void) {
  bk_snapshot_t s;
  snapshot(&s);
  bk_sample_t rpm = bk_store_get(&s, TEL_RPM, now_ms());
  if (s.connected && (rpm.state != TEL_VALID || rpm.value > 0))
    return false;
  atomic_store(&maintenance, true);
#if CONFIG_BK_ENABLE_KLINE
  bk_kline_disarm();
#endif
  xSemaphoreTake(store_lock, portMAX_DELAY);
  store.connected = false;
  store.faults_valid = false;
  xSemaphoreGive(store_lock);
  return true;
}
#endif
static void storage_task(void *arg) {
  (void)arg;
  save_request_t *request = malloc(sizeof(*request));
  bk_history_t *h = malloc(sizeof(*h));
  if (!request || !h) {
    free(request);
    free(h);
    vTaskDelete(NULL);
    return;
  }
  uint64_t last_save = now_ms();
  uint32_t saved_revision = 0;
  for (;;) {
    if (xQueueReceive(settings_queue, request, pdMS_TO_TICKS(1000)) == pdTRUE) {
      xSemaphoreTake(config_lock, portMAX_DELAY);
      if (request->revision == config_revision) {
        esp_err_t e = bk_settings_save(&request->value);
        if (e)
          ESP_LOGW(TAG, "Profile save failed: %s", esp_err_to_name(e));
      }
      xSemaphoreGive(config_lock);
    }
    if (now_ms() - last_save >= 300000) {
      history_snapshot(h);
      if (h->event_revision != saved_revision) {
        esp_err_t e = bk_journal_save(h);
        if (!e)
          saved_revision = h->event_revision;
        else
          ESP_LOGW(TAG, "Journal save failed: %s", esp_err_to_name(e));
      }
      last_save = now_ms();
    }
  }
}
static void history_task(void *arg) {
  (void)arg;
  for (;;) {
    bk_snapshot_t s;
    snapshot(&s);
    uint64_t now = now_ms();
    xSemaphoreTake(history_lock, portMAX_DELAY);
    bk_history_sample(&history_store, &s, now);
    if (s.connected && now - s.faults_at_ms <= 15000)
      bk_history_faults(&history_store, &s, now);
    xSemaphoreGive(history_lock);
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
#if CONFIG_BK_ENABLE_KLINE
static void ecu_task(void *arg) {
  (void)arg;
  uint8_t reply[256];
  size_t n;
  if (bk_kline_init() != ESP_OK) {
    bk_kline_disarm();
    ESP_LOGE(TAG, "K-Line init failed");
    vTaskDelete(NULL);
    return;
  }
  for (;;) {
    if (atomic_load(&maintenance)) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    uint8_t version = 1;
    if (bk_kline_exchange(&version, 1, reply, sizeof(reply), &n, 600) !=
            ESP_OK ||
        n != 1 || reply[0] != 0x0b) {
      vTaskDelay(pdMS_TO_TICKS(1250));
      continue;
    }
    xSemaphoreTake(store_lock, portMAX_DELAY);
    bk_store_init(&store);
    store.connected = true;
    xSemaphoreGive(store_lock);
    uint64_t due[TEL_METRIC_COUNT] = {0}, last_good = now_ms(),
             fault_due = now_ms() + 10000;
    unsigned cursor = 0;
    while (now_ms() - last_good < 3000 && !atomic_load(&maintenance)) {
      uint64_t now = now_ms();
      int selected = -1;
      /* Round-robin among overdue fields: bounded queue of one, no concurrent
       * UART users. */
      for (unsigned i = 0; i < TEL_METRIC_COUNT; i++) {
        unsigned m = (cursor + i) % TEL_METRIC_COUNT;
        if (now >= due[m]) {
          selected = m;
          cursor = (m + 1) % TEL_METRIC_COUNT;
          break;
        }
      }
      if (selected >= 0) {
        const bk_field_t *f = &bk_fields[selected];
        uint8_t cmd[] = {0x61, f->pid};
        due[selected] = now + f->period_ms;
        if (bk_kline_exchange(cmd, 2, reply, sizeof(reply), &n, 600) ==
                ESP_OK &&
            n == f->width) {
          last_good = now_ms();
          xSemaphoreTake(store_lock, portMAX_DELAY);
          bk_store_reply(&store, selected, reply, n, last_good);
          xSemaphoreGive(store_lock);
        }
      } else
        vTaskDelay(pdMS_TO_TICKS(10));
      if (now_ms() >= fault_due) {
        uint8_t cmd = 2;
        fault_due = now_ms() + 10000;
        bool ok = bk_kline_exchange(&cmd, 1, reply, sizeof(reply), &n, 600) ==
                      ESP_OK &&
                  mikas_faults_valid(reply, n);
        xSemaphoreTake(store_lock, portMAX_DELAY);
        store.faults_valid = ok;
        if (ok) {
          store.fault_count = reply[0];
          memcpy(store.faults, reply + 1, n - 1);
          store.faults_at_ms = now_ms();
        }
        xSemaphoreGive(store_lock);
      }
    }
    xSemaphoreTake(store_lock, portMAX_DELAY);
    store.connected = false;
    store.faults_valid = false;
    xSemaphoreGive(store_lock);
  }
}
#endif
static void brightness(uint8_t value, void *context) {
  (void)context;
  bk_board_set_brightness(value);
}
static void ui_task(void *arg) {
  (void)arg;
  bk_settings_t *cfg = malloc(sizeof(*cfg));
  save_request_t *save = malloc(sizeof(*save));
  if (!cfg || !save) {
    free(cfg);
    free(save);
    vTaskDelete(NULL);
    return;
  }
  uint32_t seen = 0;
  uint64_t changed_at = 0, started = now_ms(), last = started, status_at = 0;
  bool dirty = false;
  bk_ui_create(lv_screen_active());
  bk_ui_set_brightness_callback(brightness, NULL);
  bk_ui_set_dtc_lookup(bk_dtc_description);
  bk_ui_set_sample(BK_SPEED, 0, BK_VALUE_UNSUPPORTED);
  bk_ui_set_sample(BK_RANGE, 0, BK_VALUE_UNSUPPORTED);
  bk_ui_set_sample(BK_DISTANCE, 0, BK_VALUE_UNSUPPORTED);
  bk_ui_set_sample(BK_AVG_SPEED, 0, BK_VALUE_UNSUPPORTED);
#if CONFIG_BK_ENABLE_WIFI
  bk_ui_set_wifi(bk_wifi_ssid(), bk_wifi_password(), "http://192.168.4.1/");
#endif
  static const bk_metric_t map[TEL_METRIC_COUNT] = {
      BK_RPM,          BK_COOLANT,  BK_VOLTAGE,     BK_THROTTLE,
      BK_METRIC_COUNT, BK_IGNITION, BK_MAF,         BK_FUEL_LPH,
      BK_INJECTION,    BK_LAMBDA,   BK_METRIC_COUNT};
  for (;;) {
    uint64_t now = now_ms();
    lv_tick_inc(now - last);
    last = now;
    bool applied = false;
    xSemaphoreTake(config_lock, portMAX_DELAY);
    if (seen != config_revision) {
      *cfg = current_config;
      seen = config_revision;
      applied = true;
    }
    xSemaphoreGive(config_lock);
    if (applied) {
      bk_ui_config_t c = {.screen = cfg->screen,
                          .dark = cfg->dark,
                          .brightness = cfg->brightness};
      for (unsigned i = 0; i < 5; i++)
        c.selected[i] = cfg->selected[i];
      bk_ui_set_config(&c);
      bk_ui_set_layout(&cfg->layout);
      if (now - started < 100)
        bk_ui_boot_start(cfg->screen);
      dirty = false;
    }
    bk_snapshot_t s;
    snapshot(&s);
    bk_ui_set_connected(s.connected);
    for (unsigned i = 0; i < TEL_METRIC_COUNT; i++)
      if (map[i] != BK_METRIC_COUNT) {
        bk_sample_t v = bk_store_get(&s, i, now);
        bk_value_state_t state = BK_VALUE_UNKNOWN;
        if (v.state == TEL_VALID)
          state = BK_VALUE_VALID;
        else if (v.state == TEL_STALE)
          state = BK_VALUE_STALE;
        else if (v.state == TEL_NEEDS_DATA)
          state = BK_VALUE_INSUFFICIENT;
        bk_ui_set_sample(map[i], v.value, state);
      }
    bool dtc_valid =
        s.connected && s.faults_valid && now - s.faults_at_ms <= 15000;
    uint8_t codes[127];
    for (unsigned i = 0; i < s.fault_count && i < 127; i++)
      codes[i] = s.faults[i * 2];
    bk_ui_set_dtcs(codes, s.fault_count < 128 ? s.fault_count : 0, dtc_valid);
    xSemaphoreTake(history_lock, portMAX_DELAY);
    double runtime = history_store.observed_run_ms / 1000.;
    xSemaphoreGive(history_lock);
    bk_ui_set_sample(BK_TRIP_SECONDS, runtime, BK_VALUE_VALID);
    if (now - status_at >= 1000) {
      char status[80];
      status_at = now;
#if CONFIG_BK_ENABLE_WIFI
      snprintf(status, sizeof(status), "Телефонов: %u", bk_wifi_clients());
#else
      snprintf(status, sizeof(status), "Точка доступа выключена");
#endif
      bk_ui_set_wifi_status(status);
    }
    lv_timer_handler();
    if (now - started > 1000) {
      bk_ui_config_t c;
      bk_ui_get_config(&c);
      save->value = *cfg;
      save->value.screen = c.screen;
      save->value.dark = c.dark;
      save->value.brightness = c.brightness;
      for (unsigned i = 0; i < 5; i++)
        save->value.selected[i] = c.selected[i];
      bk_ui_get_layout(&save->value.layout);
      if (memcmp(cfg, &save->value, sizeof(*cfg)) &&
          bk_settings_valid(&save->value)) {
        xSemaphoreTake(config_lock, portMAX_DELAY);
        if (seen == config_revision) {
          *cfg = save->value;
          current_config = *cfg;
          seen = ++config_revision;
          changed_at = now;
          dirty = true;
        }
        xSemaphoreGive(config_lock);
      }
      if (dirty && now - changed_at >= 2000 && settings_queue) {
        save->value = *cfg;
        save->revision = seen;
        xQueueOverwrite(settings_queue, save);
        dirty = false;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
void app_main(void) {
  atomic_init(&maintenance, false);
  esp_err_t e = nvs_flash_init();
  if (e) {
    ESP_LOGE(TAG, "NVS failed; settings preserved for recovery");
    return;
  }
  store_lock = xSemaphoreCreateMutex();
  history_lock = xSemaphoreCreateMutex();
  config_lock = xSemaphoreCreateMutex();
  settings_queue = xQueueCreate(1, sizeof(save_request_t));
  if (!store_lock || !history_lock || !config_lock || !settings_queue) {
    ESP_LOGE(TAG, "Insufficient memory");
    return;
  }
  bk_store_init(&store);
  bk_history_init(&history_store, esp_random());
  e = bk_journal_restore(&history_store);
  if (e)
    ESP_LOGW(TAG, "Journal restore failed");
  e = bk_settings_load(&current_config);
  if (e)
    ESP_LOGW(TAG, "Invalid profile; factory layout loaded");
  esp_err_t display = bk_board_start();
#if CONFIG_BK_ENABLE_KLINE
  if (xTaskCreate(ecu_task, "mikas", 6144, NULL, 6, NULL) != pdPASS)
    ESP_LOGE(TAG, "ECU worker failed");
#endif
  if (xTaskCreate(history_task, "history", 4096, NULL, 2, NULL) != pdPASS)
    ESP_LOGE(TAG, "History worker failed");
  if (xTaskCreate(storage_task, "storage", 6144, NULL, 2, NULL) != pdPASS)
    ESP_LOGE(TAG, "Storage worker failed");
#if CONFIG_BK_ENABLE_WIFI
  const bk_wifi_hooks_t hooks = {.snapshot = snapshot,
                                 .history = history_snapshot,
                                 .config_get = config_get,
                                 .config_apply = config_apply,
                                 .ota_prepare = ota_prepare};
  e = bk_wifi_start(&hooks);
  if (e)
    ESP_LOGE(TAG, "SoftAP failed: %s", esp_err_to_name(e));
#endif
  if (display == ESP_OK) {
    if (xTaskCreate(ui_task, "lvgl", 8192, NULL, 3, NULL) != pdPASS)
      ESP_LOGE(TAG, "UI worker failed");
  } else
    ESP_LOGW(TAG, "Display unavailable: %s", esp_err_to_name(display));
}
