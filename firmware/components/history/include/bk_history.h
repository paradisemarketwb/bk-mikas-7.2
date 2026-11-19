#pragma once
#include "bk_telemetry.h"
#include <stddef.h>
#define BK_LOG_POINTS 180
#define BK_HISTORY_EVENTS 64
typedef struct {
  uint64_t at_ms;
  float rpm, coolant, volts;
  uint8_t valid;
} bk_history_point_t;
typedef struct {
  uint64_t at_ms;
  uint32_t boot_id;
  uint8_t code, flags, present;
} bk_dtc_event_t;
typedef struct {
  bk_history_point_t points[BK_LOG_POINTS];
  unsigned head, count;
  bk_dtc_event_t events[BK_HISTORY_EVENTS];
  unsigned event_head, event_count;
  uint64_t last_point, last_running, observed_run_ms;
  uint32_t boot_id, event_revision;
  uint8_t previous_faults[256];
  unsigned previous_count;
  bool previous_valid;
} bk_history_t;
void bk_history_init(bk_history_t *h, uint32_t boot_id);
void bk_history_sample(bk_history_t *h, const bk_snapshot_t *s, uint64_t now);
void bk_history_faults(bk_history_t *h, const bk_snapshot_t *s, uint64_t now);
const char *bk_dtc_description(uint8_t code);
