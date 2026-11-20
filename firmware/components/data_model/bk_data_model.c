#include "bk_data_model.h"
#include <math.h>
#include <string.h>
static const char *ids[BK_METRIC_COUNT] = {
    "rpm",      "coolant",        "voltage",        "throttle",  "fuel_lph",
    "maf",      "ignition",       "injection",      "speed",     "range",
    "distance", "engine_seconds", "average_speed",  "dtc_count", "lambda_raw",
    "maf_raw",  "intake",         "lambda_voltage", "fuel_l100", "fuel_level"};
static const bk_metric_t map[TEL_METRIC_COUNT] = {
    BK_RPM,       BK_COOLANT,  BK_VOLTAGE,     BK_THROTTLE,
    BK_INTAKE,    BK_IGNITION, BK_MAF_RAW,     BK_FUEL_LPH,
    BK_INJECTION, BK_LAMBDA,   BK_METRIC_COUNT};
const char *bk_data_metric_id(bk_metric_t m) {
  return (unsigned)m < BK_METRIC_COUNT ? ids[m] : "unknown";
}
static bk_source_entry_t *entry(bk_data_model_t *m, bk_metric_t id,
                                bk_source_id_t source) {
  for (unsigned i = 0; i < BK_SOURCE_CAPACITY; i++) {
    bk_source_entry_t *e = &m->sources.entries[i];
    if (e->used && e->config.source == source &&
        !strcmp(e->config.metric, ids[id]))
      return e;
  }
  return NULL;
}
static void invalidate(bk_source_entry_t *e) {
  if (e) {
    e->published = false;
    e->confirmations = 0;
    e->status = BK_SOURCE_NO_SIGNAL;
  }
}
static bool ecu_derived(bk_metric_t id) {
  return id == BK_MAF || id == BK_LAMBDA_VOLTAGE;
}
void bk_data_model_init(bk_data_model_t *m) {
  memset(m, 0, sizeof(*m));
  bk_sources_init(&m->sources);
  for (unsigned i = 0; i < TEL_METRIC_COUNT; i++)
    if (map[i] != BK_METRIC_COUNT) {
      bk_source_config_t c = {.source = BK_SOURCE_ECU,
                              .priority = 100,
                              .fresh_ms = 3000,
                              .confirm_samples = 1,
                              .minimum = -1000000,
                              .maximum = 1000000,
                              .unit = BK_UNIT_NATIVE};
      strcpy(c.metric, ids[map[i]]);
      bk_sources_register(&m->sources, &c);
    }
  for (unsigned i = 0; i < 2; i++) {
    bk_source_config_t c = {.source = BK_SOURCE_DERIVED,
                            .priority = 100,
                            .fresh_ms = 3000,
                            .confirm_samples = 1,
                            .minimum = 0,
                            .maximum = 1000000,
                            .unit = i ? BK_UNIT_V : BK_UNIT_KG_H};
    strcpy(c.metric, ids[i ? BK_LAMBDA_VOLTAGE : BK_MAF]);
    bk_sources_register(&m->sources, &c);
  }
}
void bk_data_model_update(bk_data_model_t *m, const bk_snapshot_t *s,
                          const bk_settings_t *cfg, uint64_t now) {
  if (!m || !s || !cfg)
    return;
  if (!s->connected)
    for (unsigned n = 0; n < BK_SOURCE_CAPACITY; n++) {
      bk_source_entry_t *e = &m->sources.entries[n];
      if (e->used && (e->config.source == BK_SOURCE_ECU ||
                      (e->config.source == BK_SOURCE_DERIVED &&
                       (!strcmp(e->config.metric, ids[BK_MAF]) ||
                        !strcmp(e->config.metric, ids[BK_LAMBDA_VOLTAGE])))))
        invalidate(e);
    }
  for (unsigned i = 0; i < TEL_METRIC_COUNT; i++)
    if (map[i] != BK_METRIC_COUNT) {
      bk_sample_t v = bk_store_get(s, i, now);
      bk_source_entry_t *e = entry(m, map[i], BK_SOURCE_ECU);
      if (!e || !s->connected || now < v.at_ms ||
          now - v.at_ms > e->config.fresh_ms)
        continue;
      if (v.state != TEL_VALID && !(i == TEL_AIR && v.raw_valid))
        continue;
      /* A reconnect flag is not a new ECU measurement. Preserve the last time
       * even while unpublished, and wait for a strictly newer physical frame.
       */
      if ((e->sample.raw_valid || e->sample.confirmed) &&
          v.at_ms <= e->sample.at_ms)
        continue;
      bk_source_sample_t sample = {.value = i == TEL_AIR ? v.raw : v.value,
                                   .raw = v.raw,
                                   .raw_valid = v.raw_valid,
                                   .confirmed = true,
                                   .at_ms = v.at_ms,
                                   .unit = BK_UNIT_NATIVE,
                                   .source = BK_SOURCE_ECU};
      bk_sources_publish(&m->sources, ids[map[i]], &sample);
    }
  for (unsigned i = 0; i < 2; i++) {
    bk_metric_t metric = i ? BK_LAMBDA_VOLTAGE : BK_MAF;
    bk_source_entry_t *derived = entry(m, metric, BK_SOURCE_DERIVED);
    bk_source_entry_t *raw =
        entry(m, i ? BK_LAMBDA : BK_MAF_RAW, BK_SOURCE_ECU);
    const bk_calibration_t *c = i ? &cfg->lambda : &cfg->maf;
    if (!derived)
      continue;
    if (!s->connected || !c->configured ||
        !bk_calibration_validate(c, NULL, 0) || !raw || !raw->published ||
        raw->status != BK_SOURCE_VALID || !raw->sample.raw_valid) {
      invalidate(derived);
      continue;
    }
    /* Retain timestamp/metadata of an old derived value so freshness remains
     * STALE; do not refresh it on redraw or by recalculating an old frame. */
    if (now < raw->sample.at_ms ||
        now - raw->sample.at_ms > raw->config.fresh_ms)
      continue;
    double result;
    bk_unit_t unit = i ? BK_UNIT_V : BK_UNIT_KG_H;
    if (bk_calibration_evaluate(c, raw->sample.raw, unit, &result) !=
        BK_CAL_OK) {
      invalidate(derived);
      continue;
    }
    bk_source_sample_t sample = {.value = result,
                                 .raw = raw->sample.raw,
                                 .raw_valid = true,
                                 .confirmed = true,
                                 .user_calibration = true,
                                 .at_ms = raw->sample.at_ms,
                                 .unit = unit,
                                 .source = BK_SOURCE_DERIVED};
    strcpy(sample.profile_name, c->profile_name);
    if (derived->published && derived->sample.at_ms == sample.at_ms) {
      /* New coefficients can reinterpret the same frame immediately; this is
       * not an additional sample or confirmation and must not advance age. */
      derived->sample = sample;
    } else
      bk_sources_publish(&m->sources, ids[metric], &sample);
  }
  for (unsigned i = 0; i < BK_METRIC_COUNT; i++) {
    bk_source_selection_t selected =
        bk_sources_select(&m->sources, ids[i], &cfg->source_policies[i], now);
    bk_data_value_t *v = &m->values[i];
    v->value = selected.sample.value;
    v->sample = selected.sample;
    v->state = selected.status == BK_SOURCE_VALID         ? BK_VALUE_VALID
               : selected.status == BK_SOURCE_STALE       ? BK_VALUE_STALE
               : selected.status == BK_SOURCE_UNCONFIRMED ? BK_VALUE_UNCONFIRMED
                                                          : BK_VALUE_NO_SIGNAL;
    if (!s->connected &&
        (selected.sample.source == BK_SOURCE_ECU ||
         (selected.sample.source == BK_SOURCE_DERIVED && ecu_derived(i))))
      v->state = BK_VALUE_NO_SIGNAL;
    if (selected.status != BK_SOURCE_VALID) {
      bool local_cal =
          (cfg->source_policies[i].preferred == BK_SOURCE_NONE ||
           cfg->source_policies[i].preferred == BK_SOURCE_DERIVED) &&
          (selected.sample.source == BK_SOURCE_NONE ||
           selected.sample.source == BK_SOURCE_DERIVED);
      if (local_cal && ((i == BK_MAF && !cfg->maf.configured) ||
                        (i == BK_LAMBDA_VOLTAGE && !cfg->lambda.configured)))
        v->state = BK_VALUE_NEEDS_CALIBRATION;
      /* Registered stale/unconfirmed sources keep their true state. Missing
       * hardware is distinct from a present sensor that temporarily lost data.
       */
      if (selected.sample.source == BK_SOURCE_NONE) {
        if (i == BK_SPEED || i == BK_RANGE || i == BK_DISTANCE ||
            i == BK_AVG_SPEED || i == BK_FUEL_LEVEL)
          v->state = BK_VALUE_REQUIRES_SENSOR;
        if (i == BK_FUEL_L100)
          v->state = BK_VALUE_INSUFFICIENT;
      }
    }
    if (v->state != BK_VALUE_VALID)
      v->value = NAN;
  }
}
