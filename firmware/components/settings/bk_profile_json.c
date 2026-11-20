#include "bk_settings.h"
#include "cJSON.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static bool number(const cJSON *o, const char *k, double lo, double hi,
                   double *v) {
  const cJSON *n = cJSON_GetObjectItemCaseSensitive(o, k);
  if (!cJSON_IsNumber(n) || !isfinite(n->valuedouble) || n->valuedouble < lo ||
      n->valuedouble > hi)
    return false;
  *v = n->valuedouble;
  return true;
}
static bool integer(const cJSON *o, const char *k, int lo, int hi, int *v) {
  double n;
  if (!number(o, k, lo, hi, &n) || floor(n) != n)
    return false;
  *v = (int)n;
  return true;
}
static bool string(const cJSON *o, const char *k, char *out, size_t cap) {
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
  if (!cJSON_IsString(v) || strlen(v->valuestring) >= cap)
    return false;
  strcpy(out, v->valuestring);
  return true;
}
static bool boolean(const cJSON *o, const char *k, bool *out) {
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(o, k);
  if (!cJSON_IsBool(v))
    return false;
  *out = cJSON_IsTrue(v);
  return true;
}
static bool unique(const cJSON *v) {
  if (cJSON_IsObject(v)) {
    for (const cJSON *a = v->child; a; a = a->next)
      for (const cJSON *b = a->next; b; b = b->next)
        if (a->string && b->string && !strcmp(a->string, b->string))
          return false;
  }
  for (const cJSON *c = v->child; c; c = c->next)
    if (!unique(c))
      return false;
  return true;
}
#define NUM(o, k, v) cJSON_AddNumberToObject(o, k, v)
static void calibration_json(cJSON *root, const char *key,
                             const bk_calibration_t *c) {
  cJSON *o = cJSON_AddObjectToObject(root, key);
  NUM(o, "kind", c->kind);
  cJSON_AddBoolToObject(o, "configured", c->configured);
  NUM(o, "method", c->method);
  NUM(o, "unit", c->unit);
  NUM(o, "lambda_kind", c->lambda_kind);
  cJSON_AddStringToObject(o, "profile_name", c->profile_name);
  NUM(o, "k", c->k);
  NUM(o, "divisor", c->divisor);
  NUM(o, "b", c->b);
  NUM(o, "raw_min", c->raw_min);
  NUM(o, "raw_max", c->raw_max);
  NUM(o, "output_min", c->output_min);
  NUM(o, "output_max", c->output_max);
  cJSON *a = cJSON_AddArrayToObject(o, "points");
  for (unsigned i = 0; i < c->point_count; i++) {
    cJSON *v = cJSON_CreateObject();
    NUM(v, "raw", c->points[i].raw);
    NUM(v, "value", c->points[i].value);
    cJSON_AddItemToArray(a, v);
  }
}
static bool calibration_parse(const cJSON *o, bk_calibration_t *c,
                              bk_calibration_kind_t kind) {
  int n;
  if (!cJSON_IsObject(o) || !integer(o, "kind", kind, kind, &n))
    return false;
  c->kind = n;
  if (!boolean(o, "configured", &c->configured) ||
      !integer(o, "method", 0, BK_CAL_TABLE, &n))
    return false;
  c->method = n;
  if (!integer(o, "unit", 0, BK_UNIT_NATIVE, &n))
    return false;
  c->unit = n;
  if (!integer(o, "lambda_kind", 0, BK_LAMBDA_WIDEBAND, &n))
    return false;
  c->lambda_kind = n;
  if (!string(o, "profile_name", c->profile_name, sizeof(c->profile_name)))
    return false;
  if (!number(o, "k", -1e6, 1e6, &c->k) ||
      !number(o, "divisor", 1e-9, 1e9, &c->divisor) ||
      !number(o, "b", -1e6, 1e6, &c->b) ||
      !number(o, "raw_min", -1e9, 1e9, &c->raw_min) ||
      !number(o, "raw_max", -1e9, 1e9, &c->raw_max) ||
      !number(o, "output_min", -1e9, 1e9, &c->output_min) ||
      !number(o, "output_max", -1e9, 1e9, &c->output_max))
    return false;
  const cJSON *a = cJSON_GetObjectItemCaseSensitive(o, "points");
  if (!cJSON_IsArray(a) || cJSON_GetArraySize(a) > BK_CALIBRATION_POINTS)
    return false;
  c->point_count = cJSON_GetArraySize(a);
  for (unsigned i = 0; i < c->point_count; i++) {
    const cJSON *v = cJSON_GetArrayItem(a, i);
    if (!number(v, "raw", -1e9, 1e9, &c->points[i].raw) ||
        !number(v, "value", -1e9, 1e9, &c->points[i].value))
      return false;
  }
  return bk_calibration_validate(c, NULL, 0);
}
static void preset_json(cJSON *o, const bk_metric_preset_t *p) {
  NUM(o, "metric", p->metric);
  NUM(o, "type", p->type);
  NUM(o, "min", p->min);
  NUM(o, "max", p->max);
  NUM(o, "warn_low", p->warn_low);
  NUM(o, "warn_high", p->warn_high);
  NUM(o, "flags", p->flags);
  NUM(o, "precision", p->precision);
  cJSON_AddStringToObject(o, "unit", p->unit);
}
char *bk_settings_to_json(const bk_settings_t *s) {
  if (!bk_settings_valid(s))
    return NULL;
  cJSON *r = cJSON_CreateObject();
  if (!r)
    return NULL;
  NUM(r, "version", s->version);
  NUM(r, "screen", s->screen);
  NUM(r, "brightness", s->brightness);
  cJSON_AddBoolToObject(r, "dark", s->dark);
  cJSON *sel = cJSON_AddArrayToObject(r, "selected");
  for (unsigned i = 0; i < 5; i++)
    cJSON_AddItemToArray(sel, cJSON_CreateNumber(s->selected[i]));
  cJSON *policies = cJSON_AddArrayToObject(r, "source_policies");
  for (unsigned i = 0; i < BK_METRIC_COUNT; i++) {
    cJSON *p = cJSON_CreateObject();
    NUM(p, "preferred", s->source_policies[i].preferred);
    cJSON_AddBoolToObject(p, "auto_fallback",
                          s->source_policies[i].auto_fallback);
    cJSON_AddItemToArray(policies, p);
  }
  calibration_json(r, "maf", &s->maf);
  calibration_json(r, "lambda", &s->lambda);
  cJSON *l = cJSON_AddObjectToObject(r, "layout");
  NUM(l, "version", s->layout.version);
  cJSON *screens = cJSON_AddArrayToObject(l, "screens");
  for (unsigned i = 0; i < s->layout.screen_count; i++) {
    const bk_layout_screen_t *sc = &s->layout.screens[i];
    cJSON *o = cJSON_CreateObject();
    cJSON_AddItemToArray(screens, o);
    cJSON_AddStringToObject(o, "id", sc->id);
    cJSON_AddStringToObject(o, "title", sc->title);
    NUM(o, "background", sc->background);
    cJSON_AddBoolToObject(o, "enabled", sc->enabled);
    NUM(o, "style", sc->style);
    cJSON_AddBoolToObject(o, "show_title", sc->show_title);
    NUM(o, "background_mode", sc->background_mode);
    NUM(o, "background_id", sc->background_id);
    NUM(o, "background_dim", sc->background_dim);
    NUM(o, "background_scale", sc->background_scale);
    NUM(o, "sequence_index", sc->sequence_index);
    cJSON *seq = cJSON_AddArrayToObject(o, "sequence");
    for (unsigned j = 0; j < sc->sequence_count; j++) {
      cJSON *q = cJSON_CreateObject();
      preset_json(q, &sc->sequence[j]);
      cJSON_AddItemToArray(seq, q);
    }
    cJSON *ws = cJSON_AddArrayToObject(o, "widgets");
    for (unsigned j = 0; j < sc->widget_count; j++) {
      const bk_widget_config_t *w = &sc->widgets[j];
      cJSON *a = cJSON_CreateObject();
      cJSON_AddItemToArray(ws, a);
      NUM(a, "type", w->type);
      NUM(a, "metric", w->metric);
      NUM(a, "x", w->x);
      NUM(a, "y", w->y);
      NUM(a, "w", w->w);
      NUM(a, "h", w->h);
      NUM(a, "min", w->min);
      NUM(a, "max", w->max);
      NUM(a, "warn_low", w->warn_low);
      NUM(a, "warn_high", w->warn_high);
      NUM(a, "color", w->color);
      NUM(a, "flags", w->flags);
      NUM(a, "font_size", w->font_size);
      NUM(a, "precision", w->precision);
      cJSON_AddBoolToObject(a, "visible", w->visible);
      cJSON_AddBoolToObject(a, "primary", w->primary);
      NUM(a, "missing_policy", w->missing_policy);
      cJSON_AddStringToObject(a, "unit", w->unit);
    }
  }
  char *out = cJSON_PrintUnformatted(r);
  cJSON_Delete(r);
  return out;
}
#define INT(o, key, lo, hi, dst)                                               \
  do {                                                                         \
    int n_;                                                                    \
    if (!integer(o, key, lo, hi, &n_))                                         \
      goto bad;                                                                \
    dst = n_;                                                                  \
  } while (0)
#define FLOAT(o, key, dst)                                                     \
  do {                                                                         \
    double n_;                                                                 \
    if (!number(o, key, -1000000, 1000000, &n_))                               \
      goto bad;                                                                \
    dst = (float)n_;                                                           \
  } while (0)
static bool shallow_json(const char *json) {
  unsigned depth = 0;
  bool quoted = false, escaped = false;
  for (const char *p = json; *p; p++) {
    if (quoted) {
      if (escaped)
        escaped = false;
      else if (*p == '\\')
        escaped = true;
      else if (*p == '"')
        quoted = false;
    } else if (*p == '"')
      quoted = true;
    else if (*p == '{' || *p == '[') {
      if (++depth > 12)
        return false;
    } else if (*p == '}' || *p == ']') {
      if (!depth)
        return false;
      depth--;
    }
  }
  return depth == 0 && !quoted;
}
bool bk_settings_from_json(const char *json, bk_settings_t *out) {
  if (!json || !out || strlen(json) > BK_PROFILE_JSON_MAX ||
      !shallow_json(json))
    return false;
  const char *end = NULL;
  cJSON *r = cJSON_ParseWithOpts(json, &end, true);
  if (!r)
    return false;
  bk_settings_t *s = calloc(1, sizeof(*s));
  if (!s) {
    cJSON_Delete(r);
    return false;
  }
  if (!cJSON_IsObject(r) || !unique(r))
    goto bad;
  bk_settings_defaults(s);
  int input_version;
  INT(r, "version", 3, 4, input_version);
  if (input_version == 4 &&
      (!calibration_parse(cJSON_GetObjectItemCaseSensitive(r, "maf"), &s->maf,
                          BK_CAL_MAF) ||
       !calibration_parse(cJSON_GetObjectItemCaseSensitive(r, "lambda"),
                          &s->lambda, BK_CAL_LAMBDA)))
    goto bad;
  const cJSON *policies =
      cJSON_GetObjectItemCaseSensitive(r, "source_policies");
  if (input_version == 4 && policies) {
    if (!cJSON_IsArray(policies) ||
        cJSON_GetArraySize(policies) != BK_METRIC_COUNT)
      goto bad;
    for (unsigned i = 0; i < BK_METRIC_COUNT; i++) {
      const cJSON *p = cJSON_GetArrayItem(policies, i);
      INT(p, "preferred", 0, BK_SOURCE_COUNT - 1,
          s->source_policies[i].preferred);
      if (!boolean(p, "auto_fallback", &s->source_policies[i].auto_fallback))
        goto bad;
    }
  }
  INT(r, "screen", 0, 12, s->screen);
  INT(r, "brightness", 5, 100, s->brightness);
  bool dark;
  if (!boolean(r, "dark", &dark))
    goto bad;
  s->dark = dark;
  cJSON *sel = cJSON_GetObjectItemCaseSensitive(r, "selected");
  if (!cJSON_IsArray(sel) || cJSON_GetArraySize(sel) != 5)
    goto bad;
  for (unsigned i = 0; i < 5; i++) {
    cJSON *v = cJSON_GetArrayItem(sel, i);
    if (!cJSON_IsNumber(v) || v->valuedouble < 0 ||
        v->valuedouble >= BK_METRIC_COUNT ||
        floor(v->valuedouble) != v->valuedouble)
      goto bad;
    s->selected[i] = (uint8_t)v->valuedouble;
  }
  cJSON *l = cJSON_GetObjectItemCaseSensitive(r, "layout");
  if (!cJSON_IsObject(l))
    goto bad;
  int layout_version;
  INT(l, "version", 1, 2, layout_version);
  if ((input_version == 3) != (layout_version == 1))
    goto bad;
  s->layout.version = BK_LAYOUT_VERSION;
  cJSON *screens = cJSON_GetObjectItemCaseSensitive(l, "screens");
  int count = cJSON_GetArraySize(screens);
  if (!cJSON_IsArray(screens) || count < 1 || count > BK_LAYOUT_MAX_SCREENS)
    goto bad;
  s->layout.screen_count = count;
  for (int i = 0; i < count; i++) {
    cJSON *o = cJSON_GetArrayItem(screens, i);
    bk_layout_screen_t *sc = &s->layout.screens[i];
    bk_layout_default_screen(sc, i, BK_STYLE_CLASSIC);
    sc->sequence_count = 0;
    sc->sequence_index = 0;
    if (!string(o, "id", sc->id, sizeof(sc->id)) ||
        !string(o, "title", sc->title, sizeof(sc->title)) ||
        !boolean(o, "enabled", &sc->enabled))
      goto bad;
    INT(o, "background", 0, 0xffffff, sc->background);
    if (layout_version == 2) {
      INT(o, "style", 0, BK_STYLE_COUNT - 1, sc->style);
      if (!boolean(o, "show_title", &sc->show_title))
        goto bad;
      INT(o, "background_mode", 0, BK_BACKGROUND_USER, sc->background_mode);
      INT(o, "background_id", 0, 255, sc->background_id);
      INT(o, "background_dim", 0, 100, sc->background_dim);
      INT(o, "background_scale", 50, 200, sc->background_scale);
      INT(o, "sequence_index", 0, BK_SEQUENCE_MAX - 1, sc->sequence_index);
      const cJSON *seq = cJSON_GetObjectItemCaseSensitive(o, "sequence");
      if (!cJSON_IsArray(seq) || cJSON_GetArraySize(seq) > BK_SEQUENCE_MAX)
        goto bad;
      sc->sequence_count = cJSON_GetArraySize(seq);
      for (unsigned k = 0; k < sc->sequence_count; k++) {
        const cJSON *q = cJSON_GetArrayItem(seq, k);
        bk_metric_preset_t *p = &sc->sequence[k];
        INT(q, "metric", 0, BK_METRIC_COUNT - 1, p->metric);
        INT(q, "type", 0, BK_WIDGET_COUNT - 1, p->type);
        FLOAT(q, "min", p->min);
        FLOAT(q, "max", p->max);
        FLOAT(q, "warn_low", p->warn_low);
        FLOAT(q, "warn_high", p->warn_high);
        INT(q, "flags", 0, 3, p->flags);
        INT(q, "precision", 0, 3, p->precision);
        if (!string(q, "unit", p->unit, sizeof(p->unit)))
          goto bad;
      }
    }
    cJSON *ws = cJSON_GetObjectItemCaseSensitive(o, "widgets");
    int nw = cJSON_GetArraySize(ws);
    if (!cJSON_IsArray(ws) || nw < 0 || nw > BK_LAYOUT_MAX_WIDGETS)
      goto bad;
    sc->widget_count = nw;
    for (int j = 0; j < nw; j++) {
      cJSON *a = cJSON_GetArrayItem(ws, j);
      bk_widget_config_t *w = &sc->widgets[j];
      memset(w, 0, sizeof(*w));
      w->primary = (j == 0);
      if (layout_version == 2) {
        if (!boolean(a, "primary", &w->primary))
          goto bad;
        INT(a, "missing_policy", 0, BK_MISSING_INACTIVE, w->missing_policy);
      }
      INT(a, "type", 0, BK_WIDGET_COUNT - 1, w->type);
      INT(a, "metric", 0, BK_METRIC_COUNT - 1, w->metric);
      INT(a, "x", 0, 479, w->x);
      INT(a, "y", 0, 479, w->y);
      INT(a, "w", 1, 480, w->w);
      INT(a, "h", 1, 480, w->h);
      FLOAT(a, "min", w->min);
      FLOAT(a, "max", w->max);
      FLOAT(a, "warn_low", w->warn_low);
      FLOAT(a, "warn_high", w->warn_high);
      INT(a, "color", 0, 0xffffff, w->color);
      INT(a, "flags", 0, 3, w->flags);
      INT(a, "font_size", 20, 146, w->font_size);
      INT(a, "precision", 0, 3, w->precision);
      if (!boolean(a, "visible", &w->visible) ||
          !string(a, "unit", w->unit, sizeof(w->unit)))
        goto bad;
    }
  }
  if (input_version == 3)
    for (unsigned i = 0; i < s->layout.screen_count; i++)
      bk_layout_seed_legacy_sequence(&s->layout.screens[i]);
  if (input_version == 3 && s->screen == 9) {
    s->screen = 0;
    for (unsigned i = 0; i < s->layout.screen_count; i++)
      if (s->layout.screens[i].enabled) {
        s->screen = bk_layout_screen_to_view(i);
        break;
      }
  }
  if (!bk_settings_valid(s))
    goto bad;
  *out = *s;
  free(s);
  cJSON_Delete(r);
  return true;
bad:
  free(s);
  cJSON_Delete(r);
  return false;
}
