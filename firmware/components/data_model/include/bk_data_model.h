#pragma once
#include "bk_settings.h"
#include "bk_telemetry.h"
typedef struct {
  double value;
  bk_value_state_t state;
  bk_source_sample_t sample;
} bk_data_value_t;
typedef struct {
  bk_sources_t sources;
  bk_data_value_t values[BK_METRIC_COUNT];
} bk_data_model_t;
void bk_data_model_init(bk_data_model_t *model);
/* External adapters register/publish into model->sources; they never call LVGL.
 */
void bk_data_model_update(bk_data_model_t *model, const bk_snapshot_t *snapshot,
                          const bk_settings_t *settings, uint64_t now_ms);
const char *bk_data_metric_id(bk_metric_t metric);
