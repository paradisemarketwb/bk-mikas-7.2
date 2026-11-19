#include "bk_sources.h"
#include "bk_telemetry.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void near(double a, double b) { assert(fabs(a - b) < 1e-8); }
static bk_source_config_t config(const char *id, bk_source_id_t src,
                                 int priority, unsigned confirms) {
  bk_source_config_t c = {.source = src,
                          .priority = priority,
                          .fresh_ms = 1000,
                          .confirm_samples = confirms,
                          .minimum = 0,
                          .maximum = 300,
                          .unit = BK_UNIT_NATIVE};
  snprintf(c.metric, sizeof(c.metric), "%s", id);
  return c;
}
static bool publish(bk_sources_t *r, bk_source_id_t source, double value,
                    uint64_t at, bool confirmed) {
  bk_source_sample_t s = {.value = value,
                          .raw = value * 10,
                          .raw_valid = true,
                          .at_ms = at,
                          .source = source,
                          .unit = BK_UNIT_NATIVE,
                          .confirmed = confirmed,
                          .user_calibration = true};
  snprintf(s.profile_name, sizeof(s.profile_name), "test-profile");
  return bk_sources_publish(r, "speed", &s);
}
static void calibration(void) {
  bk_calibration_t c;
  char reason[120];
  double out = 0;
  bk_calibration_defaults(&c, BK_CAL_MAF);
  assert(bk_calibration_validate(&c, reason, sizeof(reason)));
  assert(bk_calibration_evaluate(&c, 100, BK_UNIT_G_S, &out) ==
             BK_CAL_NEEDS_CONFIG &&
         isnan(out));
  assert(bk_calibration_evaluate(&c, 100, BK_UNIT_RAW, &out) == BK_CAL_OK);
  near(out, 100);
  c.configured = true;
  c.method = BK_CAL_LINEAR;
  c.k = 2;
  c.divisor = 4;
  c.b = 3;
  assert(bk_calibration_evaluate(&c, 100, BK_UNIT_G_S, &out) == BK_CAL_OK);
  near(out, 53);
  assert(bk_calibration_evaluate(&c, 100, BK_UNIT_KG_H, &out) == BK_CAL_OK);
  near(out, 190.8);
  c.unit = BK_UNIT_KG_H;
  assert(bk_calibration_evaluate(&c, 100, BK_UNIT_G_S, &out) == BK_CAL_OK);
  near(out, 53 / 3.6);
  assert(bk_calibration_evaluate(&c, 100, BK_UNIT_V, &out) ==
             BK_CAL_INVALID_UNIT &&
         isnan(out));
  assert(bk_calibration_evaluate(&c, -1, c.unit, &out) ==
         BK_CAL_RAW_OUT_OF_RANGE);
  assert(bk_calibration_evaluate(&c, INFINITY, c.unit, &out) ==
         BK_CAL_NONFINITE);
  c.divisor = 0;
  assert(!bk_calibration_validate(&c, reason, sizeof(reason)) && reason[0]);
  c.divisor = 1;
  c.k = NAN;
  assert(!bk_calibration_validate(&c, NULL, 0));
  c.k = 1;
  c.output_max = 10;
  assert(bk_calibration_evaluate(&c, 100, c.unit, &out) ==
             BK_CAL_OUTPUT_OUT_OF_RANGE &&
         isnan(out));
  c.output_max = 5000;
  c.method = BK_CAL_TABLE;
  c.point_count = 4;
  c.points[0] = (bk_calibration_point_t){0, 0};
  c.points[1] = (bk_calibration_point_t){64, 10};
  c.points[2] = (bk_calibration_point_t){128, 30};
  c.points[3] = (bk_calibration_point_t){256, 20};
  assert(bk_calibration_evaluate(&c, 96, c.unit, &out) == BK_CAL_OK);
  near(out, 20);
  assert(bk_calibration_evaluate(&c, 256, c.unit, &out) == BK_CAL_OK);
  near(out, 20);
  assert(bk_calibration_evaluate(&c, 300, c.unit, &out) ==
         BK_CAL_RAW_OUT_OF_RANGE);
  c.points[2].raw = 64;
  assert(!bk_calibration_validate(&c, NULL, 0));
  c.points[2].raw = 128;
  c.point_count = 9;
  assert(!bk_calibration_validate(&c, NULL, 0));
  c.point_count = 4;
  memset(c.profile_name, 'a', sizeof(c.profile_name));
  assert(!bk_calibration_validate(&c, NULL, 0));
  bk_calibration_defaults(&c, BK_CAL_LAMBDA);
  c.configured = true;
  c.method = BK_CAL_LINEAR;
  assert(!bk_calibration_validate(&c, NULL,
                                  0)); /* sensor semantics are required */
  c.lambda_kind = BK_LAMBDA_NARROWBAND;
  c.unit = BK_UNIT_MV;
  c.k = 4;
  c.output_max = 1100;
  assert(bk_calibration_evaluate(&c, 200, BK_UNIT_V, &out) == BK_CAL_OK);
  near(out, .8);
  c.unit = BK_UNIT_AFR;
  assert(!bk_calibration_validate(&c, NULL, 0));
  c.lambda_kind = BK_LAMBDA_WIDEBAND;
  c.output_max = 100;
  c.k = .05;
  c.b = 10;
  assert(bk_calibration_evaluate(&c, 200, BK_UNIT_AFR, &out) == BK_CAL_OK);
  near(out, 20);
  assert(bk_calibration_evaluate(&c, 200, BK_UNIT_V, &out) ==
         BK_CAL_INVALID_UNIT);
  bk_calibration_defaults(&c, BK_CAL_LAMBDA);
  assert(!c.configured &&
         c.method == BK_CAL_RAW); /* reset removes user conversion */
}
static void registry(void) {
  bk_sources_t r;
  bk_sources_init(&r);
  bk_source_config_t gps = config("speed", BK_SOURCE_GPS, 20, 2),
                     wheel = config("speed", BK_SOURCE_SPEED_SENSOR, 10, 1);
  assert(bk_sources_register(&r, &gps));
  assert(!bk_sources_register(&r, &gps));
  assert(bk_sources_register(&r, &wheel));
  bk_source_policy_t p = {0};
  bk_source_selection_t sel = bk_sources_select(&r, "speed", &p, 0);
  assert(sel.status == BK_SOURCE_NO_SIGNAL && isnan(sel.sample.value));
  assert(publish(&r, BK_SOURCE_SPEED_SENSOR, 35, 100, true));
  sel = bk_sources_select(&r, "speed", &p, 100);
  assert(sel.status == BK_SOURCE_VALID &&
         sel.sample.source == BK_SOURCE_SPEED_SENSOR);
  near(sel.sample.value, 35);
  assert(publish(&r, BK_SOURCE_GPS, 50, 100, true));
  assert(publish(&r, BK_SOURCE_GPS, 51, 200, true));
  sel = bk_sources_select(&r, "speed", &p, 200);
  assert(sel.sample.source ==
         BK_SOURCE_SPEED_SENSOR); /* no silent priority switching */
  p.auto_fallback = true;
  sel = bk_sources_select(&r, "speed", &p, 200);
  assert(sel.sample.source == BK_SOURCE_GPS);
  near(sel.sample.raw, 510);
  assert(sel.sample.raw_valid && sel.sample.user_calibration &&
         !strcmp(sel.sample.profile_name, "test-profile"));
  assert(publish(&r, BK_SOURCE_SPEED_SENSOR, 36, 900, true));
  p.auto_fallback = false;
  sel = bk_sources_select(&r, "speed", &p, 1250);
  assert(sel.status == BK_SOURCE_STALE && sel.sample.source == BK_SOURCE_GPS &&
         isnan(sel.sample.value));
  assert(sel.sample.at_ms == 200);
  p.auto_fallback = true;
  sel = bk_sources_select(&r, "speed", &p, 1250);
  assert(sel.status == BK_SOURCE_VALID &&
         sel.sample.source == BK_SOURCE_SPEED_SENSOR);
  p.preferred = BK_SOURCE_GPS;
  p.auto_fallback = false;
  sel = bk_sources_select(&r, "speed", &p, 1250);
  assert(sel.status == BK_SOURCE_STALE && sel.sample.source == BK_SOURCE_GPS);
  assert(!publish(&r, BK_SOURCE_GPS, 70, 200,
                  true)); /* duplicate timestamps cannot confirm or refresh */
  assert(publish(&r, BK_SOURCE_GPS, 70, 1300, false));
  sel = bk_sources_select(&r, "speed", &p, 1300);
  assert(sel.status == BK_SOURCE_UNCONFIRMED);
  assert(publish(&r, BK_SOURCE_GPS, 70, 1400, true));
  sel = bk_sources_select(&r, "speed", &p, 1400);
  assert(sel.status == BK_SOURCE_UNCONFIRMED);
  assert(publish(&r, BK_SOURCE_GPS, 70, 1500, true));
  sel = bk_sources_select(&r, "speed", &p, 1500);
  assert(sel.status == BK_SOURCE_VALID);
  assert(!publish(&r, BK_SOURCE_GPS, 301, 1600, true));
  sel = bk_sources_select(&r, "speed", &p, 1600);
  assert(sel.status == BK_SOURCE_OUT_OF_RANGE && isnan(sel.sample.value));
  assert(!publish(&r, BK_SOURCE_GPS, NAN, 1700, true));
  sel = bk_sources_select(&r, "speed", &p, 1700);
  assert(sel.status == BK_SOURCE_INVALID);
  assert(publish(&r, BK_SOURCE_GPS, 30, 1800, true));
  assert(publish(&r, BK_SOURCE_GPS, 31, 1900, true));
  sel = bk_sources_select(&r, "speed", &p, 1899);
  assert(sel.status == BK_SOURCE_INVALID); /* future data */
  bk_source_config_t sim = config("speed", BK_SOURCE_SIMULATION, 100, 1);
  assert(bk_sources_register(&r, &sim));
  assert(bk_sources_publish_simulation(&r, "speed", 150, BK_UNIT_NATIVE, 2000));
  p.preferred = BK_SOURCE_SIMULATION;
  sel = bk_sources_select(&r, "speed", &p, 2000);
  assert(sel.status == BK_SOURCE_SIMULATION_BLOCKED && isnan(sel.sample.value));
  p.allow_simulation = true;
  sel = bk_sources_select(&r, "speed", &p, 2000);
  assert(sel.status == BK_SOURCE_VALID && sel.sample.simulation &&
         sel.sample.source == BK_SOURCE_SIMULATION);
  assert(bk_sources_remove(&r, "speed", BK_SOURCE_SIMULATION));
  assert(!bk_sources_remove(&r, "speed", BK_SOURCE_SIMULATION));
  p.preferred = BK_SOURCE_MODULE;
  p.auto_fallback = false;
  sel = bk_sources_select(&r, "speed", &p, 2000);
  assert(sel.status == BK_SOURCE_NO_SIGNAL && isnan(sel.sample.value));
  /* Physical source removal is not permission for automatic fallback. */
  p = (bk_source_policy_t){.preferred = BK_SOURCE_GPS};
  assert(publish(&r, BK_SOURCE_GPS, 60, 2100, true));
  assert(publish(&r, BK_SOURCE_GPS, 61, 2200, true));
  sel = bk_sources_select(&r, "speed", &p, 2200);
  assert(sel.status == BK_SOURCE_VALID);
  assert(bk_sources_remove(&r, "speed", BK_SOURCE_GPS));
  assert(publish(&r, BK_SOURCE_SPEED_SENSOR, 45, 2201, true));
  p.preferred = BK_SOURCE_NONE;
  sel = bk_sources_select(&r, "speed", &p, 2201);
  assert(sel.status == BK_SOURCE_NO_SIGNAL &&
         sel.sample.source == BK_SOURCE_GPS);
  assert(bk_sources_register(&r, &gps));
  assert(publish(&r, BK_SOURCE_GPS, 62, 2202, true));
  sel = bk_sources_select(&r, "speed", &p, 2202);
  assert(sel.status == BK_SOURCE_UNCONFIRMED);
  assert(publish(&r, BK_SOURCE_GPS, 63, 2203, true));
  sel = bk_sources_select(&r, "speed", &p, 2203);
  assert(sel.status == BK_SOURCE_VALID && sel.sample.source == BK_SOURCE_GPS);
  assert(bk_sources_remove(&r, "speed", BK_SOURCE_GPS));
  p.auto_fallback = true;
  sel = bk_sources_select(&r, "speed", &p, 2204);
  assert(sel.status == BK_SOURCE_VALID &&
         sel.sample.source == BK_SOURCE_SPEED_SENSOR);
  gps.minimum = NAN;
  assert(!bk_sources_register(&r, &gps));
}
static void activity(void) {
  bk_signal_activity_t a;
  bk_signal_activity_init(&a);
  bk_activity_result_t r = bk_signal_activity_get(&a, 100, 1000, .01, .45);
  assert(r.state == BK_ACTIVITY_NO_SIGNAL && isnan(r.minimum));
  assert(bk_signal_activity_update(&a, .1, 100));
  assert(bk_signal_activity_update(&a, .8, 200));
  assert(bk_signal_activity_update(&a, .2, 300));
  r = bk_signal_activity_get(&a, 300, 1000, .01, .45);
  assert(r.state == BK_ACTIVITY_CHANGING && r.samples == 3 && r.changes == 2 &&
         r.crossings == 2);
  near(r.minimum, .1);
  near(r.maximum, .8);
  assert(bk_signal_activity_update(&a, .2, 400));
  assert(!bk_signal_activity_update(&a, .4, 400));
  assert(!bk_signal_activity_update(&a, NAN, 500));
  r = bk_signal_activity_get(&a, 400, 150, .01, .45);
  assert(r.state == BK_ACTIVITY_STATIC && r.samples == 2 && r.changes == 0);
  r = bk_signal_activity_get(&a, 2000, 500, .01, .45);
  assert(r.state == BK_ACTIVITY_STALE);
  for (unsigned i = 0; i < 50; i++)
    assert(bk_signal_activity_update(&a, i, 500 + i));
  r = bk_signal_activity_get(&a, 549, 1000, .01, 25);
  assert(r.samples == BK_ACTIVITY_POINTS &&
         r.changes == BK_ACTIVITY_POINTS - 1);
}
static void catalog_check(void) {
  size_t count = 0;
  const bk_function_t *c = bk_function_catalog(&count);
  assert(c && count >= 70);
  for (size_t i = 0; i < count; i++) {
    assert(c[i].id[0] && c[i].name[0] && c[i].unit[0] && c[i].sources[0] &&
           c[i].reason[0]);
    assert(bk_function_find(c[i].id) == &c[i]);
    for (size_t j = 0; j < i; j++)
      assert(strcmp(c[i].id, c[j].id));
    if (c[i].section == BK_SECTION_SERVICE)
      assert(c[i].status == BK_FUNCTION_DISABLED);
  }
  assert(bk_function_find("air")->status == BK_FUNCTION_NEEDS_CALIBRATION);
  assert(bk_function_find("service.dtc_clear")->status == BK_FUNCTION_DISABLED);
  assert(!bk_function_find("invented"));
}
static void raw_retention(void) {
  bk_snapshot_t s;
  bk_store_init(&s);
  s.connected = true;
  uint8_t maf[] = {0x34, 0x12}, lambda[] = {200}, intake[] = {70};
  assert(bk_store_reply(&s, TEL_AIR, maf, 2, 123));
  assert(s.samples[TEL_AIR].raw_valid);
  near(s.samples[TEL_AIR].raw, 0x1234);
  assert(s.samples[TEL_AIR].state == TEL_NEEDS_DATA);
  assert(bk_store_reply(&s, TEL_LAMBDA1, lambda, 1, 124));
  near(s.samples[TEL_LAMBDA1].raw, 200);
  assert(bk_store_reply(&s, TEL_INTAKE, intake, 1, 125));
  near(s.samples[TEL_INTAKE].raw, 70);
  near(s.samples[TEL_INTAKE].value, 30);
  assert(s.samples[TEL_INTAKE].at_ms == 125);
}
int main(void) {
  calibration();
  registry();
  activity();
  catalog_check();
  raw_retention();
  puts("sources/calibration/catalog/activity/raw: PASS");
  return 0;
}
