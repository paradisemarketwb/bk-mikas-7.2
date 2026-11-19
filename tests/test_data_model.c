#include "bk_data_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bk_source_entry_t *owned(bk_data_model_t *m, const char *id,
                                bk_source_id_t source) {
  for (unsigned i = 0; i < BK_SOURCE_CAPACITY; i++)
    if (m->sources.entries[i].used &&
        m->sources.entries[i].config.source == source &&
        !strcmp(m->sources.entries[i].config.metric, id))
      return &m->sources.entries[i];
  assert(0);
  return NULL;
}
static void refresh_ecu(bk_snapshot_t *s, uint64_t at) {
  assert(bk_store_reply(s, TEL_AIR, (uint8_t[]){110, 0}, 2, at));
  assert(bk_store_reply(s, TEL_RPM, (uint8_t[]){26}, 1, at));
}
static void selection_regressions(void) {
  bk_data_model_t *m = malloc(sizeof(*m));
  assert(m);
  bk_data_model_init(m);
  bk_settings_t cfg;
  bk_settings_defaults(&cfg);
  cfg.maf.configured = true;
  cfg.maf.method = BK_CAL_LINEAR;
  bk_snapshot_t s;
  bk_store_init(&s);
  s.connected = true;
  bk_source_entry_t *d = owned(m, "maf", BK_SOURCE_DERIVED);
  d->config.confirm_samples = 2;
  refresh_ecu(&s, 100);
  bk_data_model_update(m, &s, &cfg, 100);
  assert(d->confirmations == 1 &&
         m->values[BK_MAF].state == BK_VALUE_UNCONFIRMED);
  bk_data_model_update(m, &s, &cfg, 101);
  bk_data_model_update(m, &s, &cfg, 102);
  assert(d->confirmations == 1 && d->sample.at_ms == 100 &&
         m->values[BK_MAF].state == BK_VALUE_UNCONFIRMED);
  cfg.maf.k = 2;
  bk_data_model_update(m, &s, &cfg, 103);
  assert(d->confirmations == 1 && d->sample.at_ms == 100 &&
         fabs(d->sample.value - 792) < 1e-8);
  refresh_ecu(&s, 200);
  bk_data_model_update(m, &s, &cfg, 200);
  assert(d->confirmations == 2 && m->values[BK_MAF].state == BK_VALUE_VALID);
  cfg.maf.k = 3;
  bk_data_model_update(m, &s, &cfg, 201);
  assert(fabs(m->values[BK_MAF].value - 1188) < 1e-8 &&
         m->values[BK_MAF].sample.at_ms == 200 && d->confirmations == 2);
  bk_data_model_update(m, &s, &cfg, 3201);
  assert(m->values[BK_MAF].state == BK_VALUE_STALE &&
         isnan(m->values[BK_MAF].value) &&
         m->values[BK_MAF].sample.at_ms == 200);
  cfg.maf.configured = false;
  bk_data_model_update(m, &s, &cfg, 3202);
  assert(m->values[BK_MAF].state == BK_VALUE_NEEDS_CALIBRATION);
  cfg.maf.configured = true;
  refresh_ecu(&s, 4000);
  bk_data_model_update(m, &s, &cfg, 4000);
  refresh_ecu(&s, 4100);
  bk_data_model_update(m, &s, &cfg, 4100);
  assert(m->values[BK_MAF].state == BK_VALUE_VALID);
  bk_source_config_t external = {.source = BK_SOURCE_DERIVED,
                                 .priority = 50,
                                 .fresh_ms = 1000,
                                 .confirm_samples = 1,
                                 .minimum = 0,
                                 .maximum = 300,
                                 .unit = BK_UNIT_NATIVE};
  strcpy(external.metric, "speed");
  assert(bk_sources_register(&m->sources, &external));
  bk_source_sample_t point = {.source = BK_SOURCE_DERIVED,
                              .value = 44,
                              .unit = BK_UNIT_NATIVE,
                              .confirmed = true,
                              .at_ms = 4101};
  assert(bk_sources_publish(&m->sources, "speed", &point));
  s.connected = false;
  bk_data_model_update(m, &s, &cfg, 4101);
  assert(m->values[BK_SPEED].state == BK_VALUE_VALID &&
         m->values[BK_RPM].state == BK_VALUE_NO_SIGNAL);
  s.connected = true;
  bk_data_model_update(m, &s, &cfg, 4102);
  assert(m->values[BK_RPM].state == BK_VALUE_NO_SIGNAL &&
         m->values[BK_MAF].state ==
             BK_VALUE_NO_SIGNAL); /* old session is not new data */
  refresh_ecu(&s, 4200);
  bk_data_model_update(m, &s, &cfg, 4200);
  assert(m->values[BK_RPM].state == BK_VALUE_VALID && d->confirmations == 1);
  bk_data_model_update(m, &s, &cfg, 4201);
  assert(d->confirmations == 1);
  refresh_ecu(&s, 4300);
  bk_data_model_update(m, &s, &cfg, 4300);
  assert(m->values[BK_MAF].state == BK_VALUE_VALID);
  bk_source_config_t gps = external;
  gps.source = BK_SOURCE_GPS;
  gps.priority = 100;
  gps.confirm_samples = 2;
  assert(bk_sources_register(&m->sources, &gps));
  point.source = BK_SOURCE_GPS;
  point.at_ms = 4400;
  assert(bk_sources_publish(&m->sources, "speed", &point));
  cfg.source_policies[BK_SPEED].preferred = BK_SOURCE_GPS;
  bk_data_model_update(m, &s, &cfg, 4400);
  assert(m->values[BK_SPEED].state == BK_VALUE_UNCONFIRMED);
  point.at_ms = 4450;
  assert(bk_sources_publish(&m->sources, "speed", &point));
  bk_data_model_update(m, &s, &cfg, 4450);
  assert(m->values[BK_SPEED].state == BK_VALUE_VALID);
  bk_data_model_update(m, &s, &cfg, 5501);
  assert(m->values[BK_SPEED].state == BK_VALUE_STALE &&
         isnan(m->values[BK_SPEED].value));
  cfg.source_policies[BK_SPEED].preferred = BK_SOURCE_MODULE;
  bk_data_model_update(m, &s, &cfg, 5502);
  assert(m->values[BK_SPEED].state == BK_VALUE_REQUIRES_SENSOR);
  cfg.source_policies[BK_SPEED].auto_fallback = true;
  point.source = BK_SOURCE_DERIVED;
  point.at_ms = 5600;
  assert(bk_sources_publish(&m->sources, "speed", &point));
  s.connected = false;
  bk_data_model_update(m, &s, &cfg, 5600);
  assert(m->values[BK_SPEED].state == BK_VALUE_VALID &&
         m->values[BK_SPEED].sample.source == BK_SOURCE_DERIVED);
  bk_source_config_t module = external;
  module.source = BK_SOURCE_MODULE;
  module.unit = BK_UNIT_KG_H;
  strcpy(module.metric, "maf");
  assert(bk_sources_register(&m->sources, &module));
  point.source = BK_SOURCE_MODULE;
  point.at_ms = 5700;
  point.unit = BK_UNIT_KG_H;
  assert(bk_sources_publish(&m->sources, "maf", &point));
  cfg.maf.configured = false;
  cfg.source_policies[BK_MAF].preferred = BK_SOURCE_MODULE;
  bk_data_model_update(m, &s, &cfg, 5700);
  assert(m->values[BK_MAF].state == BK_VALUE_VALID);
  bk_data_model_update(m, &s, &cfg, 9001);
  assert(m->values[BK_MAF].state == BK_VALUE_STALE);
  assert(bk_sources_remove(&m->sources, "maf", BK_SOURCE_MODULE));
  bk_data_model_update(m, &s, &cfg, 9002);
  assert(m->values[BK_MAF].state == BK_VALUE_NO_SIGNAL);
  free(m);
}
int main(void) {
  bk_data_model_t *m = malloc(sizeof(*m));
  bk_settings_t c;
  bk_settings_defaults(&c);
  bk_data_model_init(m);
  bk_snapshot_t s;
  bk_store_init(&s);
  s.connected = true;
  assert(bk_store_reply(&s, TEL_AIR, (uint8_t[]){100, 0}, 2, 100));
  assert(bk_store_reply(&s, TEL_LAMBDA1, (uint8_t[]){128}, 1, 100));
  assert(bk_store_reply(&s, TEL_RPM, (uint8_t[]){25}, 1, 100));
  bk_data_model_update(m, &s, &c, 100);
  assert(m->values[BK_RPM].state == BK_VALUE_VALID &&
         m->values[BK_RPM].value == 1000);
  assert(m->values[BK_MAF].state == BK_VALUE_NEEDS_CALIBRATION &&
         m->values[BK_MAF_RAW].value == 100);
  assert(m->values[BK_SPEED].state == BK_VALUE_REQUIRES_SENSOR &&
         m->values[BK_LAMBDA].value == 128);
  c.maf.configured = true;
  c.maf.method = BK_CAL_LINEAR;
  c.maf.k = .1;
  c.maf.b = 2;
  c.lambda.configured = true;
  c.lambda.method = BK_CAL_LINEAR;
  c.lambda.lambda_kind = BK_LAMBDA_NARROWBAND;
  c.lambda.k = .005;
  bk_data_model_update(m, &s, &c, 101);
  assert(m->values[BK_MAF].state == BK_VALUE_VALID &&
         fabs(m->values[BK_MAF].value - 43.2) < 1e-8);
  assert(fabs(m->values[BK_LAMBDA_VOLTAGE].value - .64) < 1e-8 &&
         m->values[BK_LAMBDA_VOLTAGE].sample.user_calibration);
  c.maf.k = .2;
  bk_data_model_update(m, &s, &c, 102);
  assert(fabs(m->values[BK_MAF].value - 79.2) < 1e-8);
  c.maf.configured = false;
  bk_data_model_update(m, &s, &c, 103);
  assert(m->values[BK_MAF].state == BK_VALUE_NEEDS_CALIBRATION);
  bk_source_config_t gps = {.source = BK_SOURCE_GPS,
                            .priority = 80,
                            .fresh_ms = 1000,
                            .confirm_samples = 2,
                            .minimum = 0,
                            .maximum = 300,
                            .unit = BK_UNIT_NATIVE};
  strcpy(gps.metric, "speed");
  assert(bk_sources_register(&m->sources, &gps));
  bk_source_sample_t fix = {.value = 64,
                            .confirmed = true,
                            .at_ms = 200,
                            .unit = BK_UNIT_NATIVE,
                            .source = BK_SOURCE_GPS};
  assert(bk_sources_publish(&m->sources, "speed", &fix));
  bk_data_model_update(m, &s, &c, 200);
  assert(m->values[BK_SPEED].state != BK_VALUE_VALID);
  fix.at_ms = 201;
  assert(bk_sources_publish(&m->sources, "speed", &fix));
  bk_data_model_update(m, &s, &c, 201);
  assert(m->values[BK_SPEED].state == BK_VALUE_VALID &&
         m->values[BK_SPEED].sample.source == BK_SOURCE_GPS);
  s.connected = false;
  bk_data_model_update(m, &s, &c, 202);
  assert(m->values[BK_RPM].state != BK_VALUE_VALID &&
         m->values[BK_SPEED].state == BK_VALUE_VALID);
  bk_data_model_update(m, &s, &c, 1500);
  assert(m->values[BK_SPEED].state != BK_VALUE_VALID);
  fix.at_ms = 1501;
  assert(bk_sources_publish(&m->sources, "speed", &fix));
  fix.at_ms = 1502;
  assert(bk_sources_publish(&m->sources, "speed", &fix));
  bk_data_model_update(m, &s, &c, 1502);
  assert(m->values[BK_SPEED].state == BK_VALUE_VALID);
  c.source_policies[BK_SPEED].preferred = BK_SOURCE_SPEED_SENSOR;
  bk_data_model_update(m, &s, &c, 1503);
  assert(m->values[BK_SPEED].state != BK_VALUE_VALID);
  free(m);
  selection_regressions();
  puts("data model: real raw, immediate calibration changes, confirmed source "
       "arrival/loss/recovery and manual selection: PASS");
}
