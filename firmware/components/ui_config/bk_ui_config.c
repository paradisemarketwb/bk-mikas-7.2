#include "bk_ui_config.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
int bk_layout_view_to_screen(unsigned v) {
  return v < 5 ? (int)v : v >= 10 && v < 13 ? (int)v - 5 : -1;
}
unsigned bk_layout_screen_to_view(unsigned s) { return s < 5 ? s : s + 5; }
static bool terminated(const char *s, size_t n) {
  return memchr(s, 0, n) != NULL;
}
static bool inside(int x, int y) {
  int dx = x - 240, dy = y - 240;
  return dx * dx + dy * dy <= 232 * 232;
}
static bool preset_valid(const bk_metric_preset_t *p) {
  return (unsigned)p->metric < BK_METRIC_COUNT &&
         (unsigned)p->type < BK_WIDGET_COUNT && p->flags <= 3 &&
         p->precision <= 3 && isfinite(p->min) && isfinite(p->max) &&
         isfinite(p->warn_low) && isfinite(p->warn_high) && p->min < p->max &&
         fabsf(p->min) <= 1000000 && fabsf(p->max) <= 1000000 &&
         !((p->flags & 3) == 3 && p->warn_low >= p->warn_high) &&
         terminated(p->unit, sizeof(p->unit));
}
bool bk_layout_validate(const bk_layout_t *l) {
  if (!l || l->version != BK_LAYOUT_VERSION || l->screen_count < 1 ||
      l->screen_count > BK_LAYOUT_MAX_SCREENS)
    return false;
  unsigned enabled = 0;
  for (unsigned i = 0; i < l->screen_count; i++) {
    const bk_layout_screen_t *s = &l->screens[i];
    enabled += s->enabled;
    if (!terminated(s->id, sizeof(s->id)) || !s->id[0] ||
        !terminated(s->title, sizeof(s->title)) || s->background > 0xffffff ||
        s->widget_count > BK_LAYOUT_MAX_WIDGETS)
      return false;
    if ((unsigned)s->style >= BK_STYLE_COUNT ||
        (unsigned)s->background_mode > BK_BACKGROUND_USER ||
        s->background_dim > 100 || s->background_scale < 50 ||
        s->background_scale > 200)
      return false;
    if (s->sequence_count > BK_SEQUENCE_MAX ||
        (s->sequence_count && s->sequence_index >= s->sequence_count))
      return false;
    for (unsigned p = 0; p < s->sequence_count; p++)
      if (!preset_valid(&s->sequence[p]))
        return false;
    for (unsigned n = 0; s->id[n]; n++)
      if (!((s->id[n] >= 'a' && s->id[n] <= 'z') ||
            (s->id[n] >= 'A' && s->id[n] <= 'Z') ||
            (s->id[n] >= '0' && s->id[n] <= '9') || s->id[n] == '_' ||
            s->id[n] == '-'))
        return false;
    for (unsigned j = 0; j < i; j++)
      if (!strcmp(s->id, l->screens[j].id))
        return false;
    for (unsigned j = 0; j < s->widget_count; j++) {
      const bk_widget_config_t *w = &s->widgets[j];
      bk_metric_preset_t p = {.metric = w->metric,
                              .type = w->type,
                              .min = w->min,
                              .max = w->max,
                              .warn_low = w->warn_low,
                              .warn_high = w->warn_high,
                              .flags = w->flags,
                              .precision = w->precision};
      memcpy(p.unit, w->unit, sizeof(p.unit));
      if (!preset_valid(&p) ||
          (unsigned)w->missing_policy > BK_MISSING_INACTIVE)
        return false;
      if (w->x < 0 || w->y < 0 || w->w < 16 || w->h < 16 || w->x + w->w > 480 ||
          w->y + w->h >
              ((w->type == BK_WIDGET_ARC || w->type == BK_WIDGET_NEEDLE) ? 472
                                                                         : 444))
        return false;
      if ((w->type == BK_WIDGET_NUMBER &&
           (w->w < 100 ||
            w->h < (s->style == BK_STYLE_REFERENCE ? 64 : 110))) ||
          ((w->type == BK_WIDGET_CHART || w->type == BK_WIDGET_BAR ||
            w->type == BK_WIDGET_TEMPERATURE) &&
           (w->w < 64 || w->h < 48)) ||
          ((w->type == BK_WIDGET_DIGITAL || w->type == BK_WIDGET_ALARM) &&
           w->h < 48))
        return false;
      if (w->color > 0xffffff ||
          (w->font_size != 20 && w->font_size != 48 && w->font_size != 64 &&
           w->font_size != 96 && w->font_size != 128))
        return false;
      if (w->type == BK_WIDGET_ARC || w->type == BK_WIDGET_NEEDLE) {
        int cx = w->x + w->w / 2, cy = w->y + w->h / 2,
            r = (w->w < w->h ? w->w : w->h) / 2;
        double dx = cx - 240, dy = cy - 240;
        if (sqrt(dx * dx + dy * dy) + r > 232)
          return false;
      } else if (!inside(w->x, w->y) || !inside(w->x + w->w, w->y) ||
                 !inside(w->x, w->y + w->h) ||
                 !inside(w->x + w->w, w->y + w->h))
        return false;
    }
  }
  return enabled > 0;
}
void bk_metric_preset_defaults(bk_metric_t metric, bk_metric_preset_t *p) {
  static const float maximum[BK_METRIC_COUNT] = {
      6000, 120,   16,  100, 30,  200,   60,  20,   160, 800,
      1000, 86400, 160, 20,  255, 65535, 100, 1.2f, 30,  100};
  if (!p)
    return;
  memset(p, 0, sizeof(*p));
  p->metric = metric;
  p->type = BK_WIDGET_ARC;
  p->max = (unsigned)metric < BK_METRIC_COUNT ? maximum[metric] : 100;
  p->precision = metric == BK_VOLTAGE || metric == BK_FUEL_LPH ||
                         metric == BK_INJECTION ||
                         metric == BK_LAMBDA_VOLTAGE || metric == BK_FUEL_L100
                     ? 1
                     : 0;
  if (metric == BK_COOLANT) {
    p->flags = BK_WARN_HIGH;
    p->warn_high = 105;
  }
  if (metric == BK_RPM) {
    p->flags = BK_WARN_HIGH;
    p->warn_high = 5800;
  }
  if (metric == BK_VOLTAGE) {
    p->flags = BK_WARN_LOW;
    p->warn_low = 12;
  }
}
static bk_widget_config_t widget(bk_widget_type_t t, bk_metric_t m, int x,
                                 int y, int w, int h, float maximum) {
  bk_metric_preset_t p;
  bk_metric_preset_defaults(m, &p);
  bk_widget_config_t a = {.type = t,
                          .metric = m,
                          .x = x,
                          .y = y,
                          .w = w,
                          .h = h,
                          .min = 0,
                          .max = maximum,
                          .color = 0x126b48,
                          .font_size = 20,
                          .visible = true,
                          .flags = p.flags,
                          .warn_low = p.warn_low,
                          .warn_high = p.warn_high,
                          .precision = p.precision};
  if (t == BK_WIDGET_NUMBER)
    a.font_size = 48;
  return a;
}
static void sequence_defaults(bk_layout_screen_t *s, bk_metric_t primary) {
  const bk_metric_t common[] = {BK_RPM,           BK_COOLANT, BK_VOLTAGE,
                                BK_THROTTLE,      BK_MAF,     BK_INJECTION,
                                BK_LAMBDA_VOLTAGE};
  s->sequence_count = 8;
  bk_metric_preset_defaults(primary, &s->sequence[0]);
  for (unsigned i = 0; i < 7; i++)
    bk_metric_preset_defaults(common[i], &s->sequence[i + 1]);
}
void bk_layout_default_screen(bk_layout_screen_t *s, unsigned slot,
                              bk_style_t style) {
  const char *ids[] = {"main", "engine", "fuel", "sensors", "trip"};
  const char *titles[] = {"ГЛАВНЫЙ ПРИБОР", "ДВИГАТЕЛЬ", "ТОПЛИВО", "ДАТЧИКИ",
                          "ПОЕЗДКА"};
  const bk_metric_t main[] = {BK_SPEED, BK_COOLANT, BK_FUEL_LPH, BK_MAF,
                              BK_DISTANCE};
  const bk_metric_t left[] = {BK_RPM, BK_VOLTAGE, BK_RANGE, BK_LAMBDA,
                              BK_TRIP_SECONDS};
  const bk_metric_t right[] = {BK_VOLTAGE, BK_RPM, BK_INJECTION, BK_IGNITION,
                               BK_AVG_SPEED};
  const float maxima[] = {160, 120, 30, 200, 1000};
  unsigned kind = slot % 5;
  memset(s, 0, sizeof(*s));
  /* Keep formatting type-safe for custom IDs. */
  if (slot < 5)
    snprintf(s->id, sizeof(s->id), "%s", ids[kind]);
  else
    snprintf(s->id, sizeof(s->id), "custom%u", slot % BK_LAYOUT_MAX_SCREENS);
  const char *ref_titles[] = {"Главный прибор", "Двигатель", "Топливо",
                              "Датчики", "Поездка"};
  snprintf(s->title, sizeof(s->title), "%s",
           style == BK_STYLE_REFERENCE ? ref_titles[kind] : titles[kind]);
  s->enabled = true;
  s->show_title = true;
  s->style = style;
  s->background_scale = 100;
  s->background = style == BK_STYLE_REFERENCE ? 0xf5f6ef : 0xf7f7ef;
  s->widget_count = 4;
  s->widgets[0] =
      widget(BK_WIDGET_ARC, main[kind], 33, 33, 414, 414, maxima[kind]);
  s->widgets[0].primary = true;
  s->widgets[1] =
      widget(BK_WIDGET_NUMBER, main[kind], 85, 150, 310, 130, maxima[kind]);
  s->widgets[1].primary = true;
  s->widgets[2] = widget(BK_WIDGET_DIGITAL, left[kind], 70, 295, 165, 75, 6000);
  s->widgets[3] =
      widget(BK_WIDGET_DIGITAL, right[kind], 245, 295, 165, 75, 6000);
  if (style == BK_STYLE_REFERENCE) {
    if (kind == 0) {
      s->show_title = false;
      s->widgets[0] = widget(BK_WIDGET_ARC, BK_SPEED, 52, 52, 376, 376, 160);
      s->widgets[0].primary = true;
      s->widgets[1] =
          widget(BK_WIDGET_NUMBER, BK_SPEED, 90, 125, 300, 148, 160);
      s->widgets[1].font_size = 128;
      s->widgets[1].primary = true;
      s->widgets[2] = widget(BK_WIDGET_NUMBER, BK_RPM, 90, 335, 300, 76, 6000);
      s->widgets[2].font_size = 64;
      s->widget_count = 3;
    } else if (kind == 1) {
      s->widget_count = 3;
      s->widgets[0] =
          widget(BK_WIDGET_NUMBER, BK_COOLANT, 100, 115, 280, 126, 120);
      s->widgets[0].font_size = 96;
      s->widgets[0].primary = true;
      s->widgets[1] =
          widget(BK_WIDGET_TEMPERATURE, BK_COOLANT, 65, 245, 350, 56, 120);
      s->widgets[1].min = 40;
      s->widgets[1].primary = true;
      s->widgets[2] =
          widget(BK_WIDGET_NUMBER, BK_VOLTAGE, 100, 325, 280, 86, 16);
      s->widgets[2].font_size = 64;
    } else if (kind == 2) {
      s->widget_count = 3;
      s->widgets[0] =
          widget(BK_WIDGET_NUMBER, BK_FUEL_L100, 70, 125, 340, 100, 30);
      s->widgets[0].font_size = 64;
      s->widgets[0].primary = true;
      s->widgets[1] =
          widget(BK_WIDGET_NUMBER, BK_RANGE, 110, 270, 260, 100, 800);
      s->widgets[1].font_size = 64;
      s->widgets[2] =
          widget(BK_WIDGET_BAR, BK_FUEL_LEVEL, 105, 377, 270, 48, 100);
    } else if (kind == 3) {
      s->widget_count = 3;
      s->widgets[0] = widget(BK_WIDGET_DIGITAL, BK_MAF, 60, 130, 360, 68, 80);
      snprintf(s->widgets[0].unit, sizeof(s->widgets[0].unit), "г/с");
      s->widgets[0].precision = 1;
      s->widgets[0].primary = true;
      s->widgets[1] =
          widget(BK_WIDGET_DIGITAL, BK_LAMBDA_VOLTAGE, 60, 225, 360, 68, 1.2f);
      s->widgets[1].precision = 2;
      s->widgets[2] =
          widget(BK_WIDGET_DIGITAL, BK_IGNITION, 60, 318, 360, 68, 60);
      s->widgets[2].precision = 1;
    } else {
      s->widget_count = 3;
      s->widgets[0] =
          widget(BK_WIDGET_NUMBER, BK_DISTANCE, 135, 120, 260, 90, 1000);
      s->widgets[0].font_size = 64;
      s->widgets[0].precision = 1;
      s->widgets[0].primary = true;
      s->widgets[1] =
          widget(BK_WIDGET_NUMBER, BK_TRIP_SECONDS, 135, 220, 260, 90, 86400);
      s->widgets[1].font_size = 64;
      s->widgets[2] =
          widget(BK_WIDGET_NUMBER, BK_AVG_SPEED, 135, 320, 260, 90, 160);
      s->widgets[2].font_size = 64;
      s->widgets[2].precision = 1;
    }
    for (unsigned i = 0; i < s->widget_count; i++)
      s->widgets[i].color = 0x009d31;
  }
  bk_metric_t primary = main[kind];
  for (unsigned i = 0; i < s->widget_count; i++)
    if (s->widgets[i].primary) {
      primary = s->widgets[i].metric;
      break;
    }
  sequence_defaults(s, primary);
  for (unsigned i = 0; i < s->widget_count; i++)
    if (s->widgets[i].primary) {
      bk_widget_config_t *w = &s->widgets[i];
      bk_metric_preset_t *p = &s->sequence[0];
      p->metric = w->metric;
      p->min = w->min;
      p->max = w->max;
      p->warn_low = w->warn_low;
      p->warn_high = w->warn_high;
      p->flags = w->flags;
      p->precision = w->precision;
      memcpy(p->unit, w->unit, sizeof(p->unit));
      if (w->type == BK_WIDGET_ARC || w->type == BK_WIDGET_NEEDLE)
        p->type = w->type;
      break;
    }
}
void bk_layout_defaults(bk_layout_t *l) {
  if (!l)
    return;
  memset(l, 0, sizeof(*l));
  l->version = BK_LAYOUT_VERSION;
  l->screen_count = 5;
  for (unsigned i = 0; i < 5; i++)
    bk_layout_default_screen(&l->screens[i], i, BK_STYLE_CLASSIC);
}
void bk_layout_reference_defaults(bk_layout_t *l) {
  if (!l)
    return;
  memset(l, 0, sizeof(*l));
  l->version = BK_LAYOUT_VERSION;
  l->screen_count = 5;
  for (unsigned i = 0; i < 5; i++)
    bk_layout_default_screen(&l->screens[i], i, BK_STYLE_REFERENCE);
}
bool bk_layout_step_primary(bk_layout_screen_t *s, int direction) {
  if (!s || !s->sequence_count)
    return false;
  s->sequence_index =
      (s->sequence_index + (direction < 0 ? -1 : 1) + s->sequence_count) %
      s->sequence_count;
  bk_metric_preset_t *p = &s->sequence[s->sequence_index];
  for (unsigned i = 0; i < s->widget_count; i++)
    if (s->widgets[i].primary) {
      bk_widget_config_t *w = &s->widgets[i];
      w->metric = p->metric;
      w->min = p->min;
      w->max = p->max;
      w->warn_low = p->warn_low;
      w->warn_high = p->warn_high;
      w->flags = p->flags;
      w->precision = p->precision;
      memcpy(w->unit, p->unit, sizeof(w->unit));
      if (w->type == BK_WIDGET_ARC || w->type == BK_WIDGET_NEEDLE)
        w->type = p->type;
    }
  return true;
}
