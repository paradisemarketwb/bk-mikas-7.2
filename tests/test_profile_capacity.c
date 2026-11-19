#include "bk_settings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void dense_cal(bk_calibration_t *c, bk_calibration_kind_t kind) {
  bk_calibration_defaults(c, kind);
  c->configured = true;
  c->method = BK_CAL_TABLE;
  c->lambda_kind =
      kind == BK_CAL_LAMBDA ? BK_LAMBDA_WIDEBAND : BK_LAMBDA_UNSPECIFIED;
  memset(c->profile_name, 'P', 31);
  c->profile_name[31] = 0;
  c->k = 1.234567891;
  c->divisor = 3.876543219;
  c->b = 2.345678912;
  c->raw_min = .0123456789;
  c->raw_max = kind == BK_CAL_MAF ? 65535.123456 : 255.123456;
  c->output_min = .00123456789;
  c->output_max = kind == BK_CAL_MAF ? 4999.12345678 : 5.12345678;
  c->point_count = 8;
  for (unsigned i = 0; i < 8; i++)
    c->points[i] = (bk_calibration_point_t){
        .raw = .123456789 + i * (kind == BK_CAL_MAF ? 5000.123456 : 30.123456),
        .value = .123456789 + i * (kind == BK_CAL_MAF ? 500.123456 : .5123456)};
  assert(bk_calibration_validate(c, NULL, 0));
}
int main(void) {
  bk_settings_t c;
  bk_settings_defaults(&c);
  unsigned char *blob = malloc(20000);
  assert(blob);
  printf("default valid=%u raw=%zu packed=%zu\n", bk_settings_valid(&c),
         sizeof(c), bk_settings_encode(&c, blob, 20000));
  c.layout.screen_count = 8;
  c.brightness = 99;
  c.dark = 1;
  for (unsigned z = 0; z < 5; z++)
    c.selected[z] = (uint8_t)(z + 1);
  for (unsigned i = 0; i < 8; i++) {
    bk_layout_screen_t *s = &c.layout.screens[i];
    memset(s, 0, sizeof(*s));
    memset(s->id, 'a' + i, 15);
    s->id[15] = 0;
    memset(s->title, 'T' + i, 63);
    s->title[63] = 0;
    s->background = 0x12abcd;
    s->enabled = true;
    s->show_title = true;
    s->style = BK_STYLE_REFERENCE;
    s->background_mode = BK_BACKGROUND_USER;
    s->background_id = 3;
    s->background_dim = 99;
    s->background_scale = 199;
    s->sequence_count = 8;
    s->sequence_index = 7;
    s->widget_count = 8;
    for (unsigned j = 0; j < 8; j++) {
      bk_metric_preset_t *p = &s->sequence[j];
      p->type = BK_WIDGET_NEEDLE;
      p->metric = (bk_metric_t)(j + 1);
      p->min = .1234567f;
      p->max = 34567.891f;
      p->warn_low = 1.234567f;
      p->warn_high = 23.45678f;
      p->flags = 3;
      p->precision = 3;
      memset(p->unit, 'A' + j, 15);
      p->unit[15] = 0;
      bk_widget_config_t *w = &s->widgets[j];
      w->type = BK_WIDGET_NEEDLE;
      w->metric = p->metric;
      w->x = 257;
      w->y = 257;
      w->w = 109;
      w->h = 111;
      w->min = p->min;
      w->max = p->max;
      w->warn_low = p->warn_low;
      w->warn_high = p->warn_high;
      w->color = 0x12abcd;
      w->flags = 3;
      w->font_size = 96;
      w->precision = 3;
      w->visible = true;
      w->primary = true;
      w->missing_policy = BK_MISSING_INACTIVE;
      memset(w->unit, 'J' + j, 15);
      w->unit[15] = 0;
    }
  }
  printf("8x8+8seq valid=%u packed=%zu\n", bk_settings_valid(&c),
         bk_settings_encode(&c, blob, 20000));
  assert(bk_settings_valid(&c));
  dense_cal(&c.maf, BK_CAL_MAF);
  dense_cal(&c.lambda, BK_CAL_LAMBDA);
  for (unsigned i = 0; i < BK_METRIC_COUNT; i++) {
    c.source_policies[i].preferred = BK_SOURCE_MODULE;
    c.source_policies[i].auto_fallback = true;
  }
  assert(bk_settings_valid(&c));
  printf("dense+2x8cal+policies valid=%u packed=%zu dedicated_nvs=65536\n",
         bk_settings_valid(&c), bk_settings_encode(&c, blob, 20000));
  bk_settings_t out;
  size_t size = bk_settings_encode(&c, blob, 20000);
  assert(bk_settings_decode(blob, size, &out));
  char *json = bk_settings_to_json(&c);
  assert(json);
  printf("dense JSON bytes=%zu\n", strlen(json));
  assert(bk_settings_from_json(json, &out));
  free(json);
  free(blob);
  return 0;
}
