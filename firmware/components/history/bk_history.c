#include "bk_history.h"
#include <string.h>
void bk_history_init(bk_history_t *h, uint32_t boot) {
  memset(h, 0, sizeof(*h));
  h->boot_id = boot;
}
void bk_history_sample(bk_history_t *h, const bk_snapshot_t *s, uint64_t now) {
  bk_sample_t rpm = bk_store_get(s, TEL_RPM, now);
  if (rpm.state == TEL_VALID && rpm.value >= 480) {
    if (h->last_running && now - h->last_running <= 3000)
      h->observed_run_ms += now - h->last_running;
    h->last_running = now;
  } else
    h->last_running = 0;
  if (h->count && now - h->last_point < 1000)
    return;
  h->last_point = now;
  bk_history_point_t p = {.at_ms = now};
  bk_sample_t vals[] = {rpm, bk_store_get(s, TEL_COOLANT, now),
                        bk_store_get(s, TEL_VOLTS, now)};
  for (unsigned i = 0; i < 3; i++)
    if (vals[i].state == TEL_VALID)
      p.valid |= (1u << i);
  p.rpm = vals[0].value;
  p.coolant = vals[1].value;
  p.volts = vals[2].value;
  h->points[h->head] = p;
  h->head = (h->head + 1) % BK_LOG_POINTS;
  if (h->count < BK_LOG_POINTS)
    h->count++;
}
static bool contains(const uint8_t *pairs, unsigned n, uint8_t code,
                     uint8_t flags) {
  for (unsigned i = 0; i < n; i++)
    if (pairs[i * 2] == code && pairs[i * 2 + 1] == flags)
      return true;
  return false;
}
static void event(bk_history_t *h, uint8_t code, uint8_t flags, bool present,
                  uint64_t now) {
  h->events[h->event_head] = (bk_dtc_event_t){.at_ms = now,
                                              .boot_id = h->boot_id,
                                              .code = code,
                                              .flags = flags,
                                              .present = present};
  h->event_head = (h->event_head + 1) % BK_HISTORY_EVENTS;
  if (h->event_count < BK_HISTORY_EVENTS)
    h->event_count++;
  h->event_revision++;
}
void bk_history_faults(bk_history_t *h, const bk_snapshot_t *s, uint64_t now) {
  if (!s->faults_valid || s->fault_count > 127)
    return;
  for (unsigned i = 0; i < s->fault_count; i++)
    if (!h->previous_valid || !contains(h->previous_faults, h->previous_count,
                                        s->faults[2 * i], s->faults[2 * i + 1]))
      event(h, s->faults[2 * i], s->faults[2 * i + 1], true, now);
  if (h->previous_valid)
    for (unsigned i = 0; i < h->previous_count; i++)
      if (!contains(s->faults, s->fault_count, h->previous_faults[2 * i],
                    h->previous_faults[2 * i + 1]))
        event(h, h->previous_faults[2 * i], h->previous_faults[2 * i + 1],
              false, now);
  memcpy(h->previous_faults, s->faults, s->fault_count * 2);
  h->previous_count = s->fault_count;
  h->previous_valid = true;
}
const char *bk_dtc_description(uint8_t code) {
  static const struct {
    uint8_t code;
    const char *text;
  } descriptions[] = {
#include "bk_dtc_codes.inc"
  };
  for (unsigned i = 0; i < sizeof(descriptions) / sizeof(descriptions[0]); i++)
    if (descriptions[i].code == code)
      return descriptions[i].text;
  return "Код Микас: сверьте диагностическую таблицу ЭБУ";
}
