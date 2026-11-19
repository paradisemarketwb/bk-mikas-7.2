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
  if (!json || !out || strlen(json) > 32768 || !shallow_json(json))
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
  INT(r, "version", 3, 3, s->version);
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
  INT(l, "version", 1, 1, s->layout.version);
  cJSON *screens = cJSON_GetObjectItemCaseSensitive(l, "screens");
  int count = cJSON_GetArraySize(screens);
  if (!cJSON_IsArray(screens) || count < 1 || count > BK_LAYOUT_MAX_SCREENS)
    goto bad;
  s->layout.screen_count = count;
  for (int i = 0; i < count; i++) {
    cJSON *o = cJSON_GetArrayItem(screens, i);
    bk_layout_screen_t *sc = &s->layout.screens[i];
    if (!string(o, "id", sc->id, sizeof(sc->id)) ||
        !string(o, "title", sc->title, sizeof(sc->title)) ||
        !boolean(o, "enabled", &sc->enabled))
      goto bad;
    INT(o, "background", 0, 0xffffff, sc->background);
    cJSON *ws = cJSON_GetObjectItemCaseSensitive(o, "widgets");
    int nw = cJSON_GetArraySize(ws);
    if (!cJSON_IsArray(ws) || nw < 0 || nw > BK_LAYOUT_MAX_WIDGETS)
      goto bad;
    sc->widget_count = nw;
    for (int j = 0; j < nw; j++) {
      cJSON *a = cJSON_GetArrayItem(ws, j);
      bk_widget_config_t *w = &sc->widgets[j];
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
      INT(a, "font_size", 20, 48, w->font_size);
      INT(a, "precision", 0, 3, w->precision);
      if (!boolean(a, "visible", &w->visible) ||
          !string(a, "unit", w->unit, sizeof(w->unit)))
        goto bad;
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
