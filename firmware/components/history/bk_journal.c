#include "bk_journal.h"
#include "nvs.h"
#include <string.h>
typedef struct {
  uint32_t version;
  unsigned head, count;
  bk_dtc_event_t events[BK_HISTORY_EVENTS];
} persisted_t;
esp_err_t bk_journal_restore(bk_history_t *h) {
  nvs_handle_t n;
  esp_err_t e = nvs_open("bk_journal", NVS_READONLY, &n);
  if (e == ESP_ERR_NVS_NOT_FOUND)
    return ESP_OK;
  if (e)
    return e;
  persisted_t p;
  size_t size = sizeof(p);
  e = nvs_get_blob(n, "events", &p, &size);
  nvs_close(n);
  if (e == ESP_ERR_NVS_NOT_FOUND)
    return ESP_OK;
  if (e)
    return e;
  if (size != sizeof(p) || p.version != 1 || p.head >= BK_HISTORY_EVENTS ||
      p.count > BK_HISTORY_EVENTS)
    return ESP_ERR_INVALID_STATE;
  memcpy(h->events, p.events, sizeof(p.events));
  h->event_head = p.head;
  h->event_count = p.count;
  return ESP_OK;
}
esp_err_t bk_journal_save(const bk_history_t *h) {
  persisted_t p = {
      .version = 1, .head = h->event_head, .count = h->event_count};
  memcpy(p.events, h->events, sizeof(p.events));
  nvs_handle_t n;
  esp_err_t e = nvs_open("bk_journal", NVS_READWRITE, &n);
  if (e)
    return e;
  e = nvs_set_blob(n, "events", &p, sizeof(p));
  if (!e)
    e = nvs_commit(n);
  nvs_close(n);
  return e;
}
