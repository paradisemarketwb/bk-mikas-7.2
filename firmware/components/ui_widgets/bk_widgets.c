#include "bk_widgets.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
const bk_widget_registry_entry_t bk_widget_registry[BK_WIDGET_COUNT] = {
    {"number", "Большое число"}, {"digital", "Цифровой показатель"},
    {"arc", "Круговая шкала"},   {"needle", "Стрелочный прибор"},
    {"bar", "Индикатор уровня"}, {"temperature", "Температурная шкала"},
    {"status", "Статус связи"},  {"alarm", "Предупреждение"},
    {"chart", "Мини-график"}};
static float history[BK_METRIC_COUNT][BK_HISTORY_POINTS];
static uint8_t heads[BK_METRIC_COUNT], counts[BK_METRIC_COUNT];
static void text(lv_obj_t *o, const char *s) {
  if (o && strcmp(lv_label_get_text(o), s))
    lv_label_set_text(o, s);
}
lv_obj_t *bk_text(lv_obj_t *parent, const char *s, int x, int y, int width) {
  lv_obj_t *o = lv_label_create(parent);
  lv_label_set_text(o, s);
  lv_obj_set_width(o, width);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_style_text_font(o, &bk_font_ru_20, 0);
  lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
  return o;
}
lv_obj_t *bk_gauge(lv_obj_t *parent, lv_color_t color) {
  lv_obj_t *o = lv_arc_create(parent);
  lv_obj_set_size(o, 414, 414);
  lv_obj_set_pos(o, 33, 16);
  lv_arc_set_rotation(o, 135);
  lv_arc_set_bg_angles(o, 0, 270);
  lv_arc_set_range(o, 0, 1000);
  lv_arc_set_value(o, 0);
  lv_obj_set_style_arc_width(o, 12, LV_PART_MAIN);
  lv_obj_set_style_arc_width(o, 12, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(o, color, LV_PART_INDICATOR);
  lv_obj_remove_style(o, NULL, LV_PART_KNOB);
  lv_obj_set_clickable(o, false);
  return o;
}
static void classic_create(bk_widget_t *w, lv_obj_t *parent,
                           const bk_widget_config_t *c, bool dark) {
  memset(w, 0, sizeof(*w));
  w->config = *c;
  w->dark = dark;
  w->target = -1;
  lv_color_t color =
      lv_color_hex(dark && c->color == 0x126b48 ? 0x72d6aa : c->color);
  if (c->type == BK_WIDGET_ARC) {
    w->root = bk_gauge(parent, color);
  } else if (c->type == BK_WIDGET_NEEDLE) {
    w->root = lv_scale_create(parent);
    lv_scale_set_mode(w->root, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(w->root, 0, 1000);
    lv_scale_set_total_tick_count(w->root, 31);
    lv_scale_set_major_tick_every(w->root, 5);
    lv_scale_set_label_show(w->root, true);
    for (unsigned i = 0; i < 7; i++) {
      snprintf(w->tick_labels[i], sizeof(w->tick_labels[i]), "%.*f",
               c->precision, c->min + (c->max - c->min) * i / 6.0);
      w->tick_ptrs[i] = w->tick_labels[i];
    }
    w->tick_ptrs[7] = NULL;
    lv_scale_set_text_src(w->root, w->tick_ptrs);
    lv_obj_set_style_text_font(w->root, &bk_font_ru_20, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(
        w->root, lv_color_hex(dark ? 0xe9f1ed : 0x112b20), LV_PART_INDICATOR);
    lv_scale_set_angle_range(w->root, 270);
    lv_scale_set_rotation(w->root, 135);
    lv_obj_set_style_line_color(w->root, color, LV_PART_MAIN);
    lv_obj_set_style_line_color(w->root, color, LV_PART_ITEMS);
    lv_obj_set_style_line_color(w->root, color, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(w->root, 2, LV_PART_MAIN);
    lv_obj_set_style_line_width(w->root, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_width(w->root, 3, LV_PART_INDICATOR);
    lv_obj_set_style_length(w->root, 6, LV_PART_ITEMS);
    lv_obj_set_style_length(w->root, 12, LV_PART_INDICATOR);
    w->needle = lv_line_create(w->root);
    lv_obj_set_style_line_width(w->needle, 4, 0);
    lv_obj_set_style_line_color(w->needle, color, 0);
  } else {
    w->root = lv_obj_create(parent);
    lv_obj_remove_style_all(w->root);
    lv_obj_set_scrollable(w->root, false);
    if (c->type == BK_WIDGET_CHART) {
      w->value = bk_text(w->root, "", 0, 0, c->w);
      w->chart = lv_chart_create(w->root);
      lv_obj_set_pos(w->chart, 5, 28);
      lv_obj_set_size(w->chart, c->w - 10, c->h - 32);
      lv_obj_set_style_bg_color(w->chart,
                                lv_color_hex(dark ? 0x172b20 : 0xffffff), 0);
      lv_obj_set_style_line_color(
          w->chart, lv_color_hex(dark ? 0x385242 : 0xdce5dd), LV_PART_MAIN);
      lv_chart_set_type(w->chart, LV_CHART_TYPE_LINE);
      lv_chart_set_point_count(w->chart, BK_HISTORY_POINTS);
      lv_chart_set_axis_range(w->chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
      lv_chart_set_div_line_count(w->chart, 3, 3);
      lv_obj_set_style_size(w->chart, 0, 0, LV_PART_INDICATOR);
      w->series = lv_chart_add_series(w->chart, color, LV_CHART_AXIS_PRIMARY_Y);
    } else if (c->type == BK_WIDGET_BAR || c->type == BK_WIDGET_TEMPERATURE) {
      w->value = bk_text(w->root, "", 0, 0, c->w);
      w->chart = lv_bar_create(w->root);
      lv_obj_set_pos(w->chart, 4, c->h - 20);
      lv_obj_set_size(w->chart, c->w - 8, 16);
      lv_bar_set_range(w->chart, 0, 1000);
      lv_obj_set_style_bg_color(w->chart, color, LV_PART_INDICATOR);
    } else if (c->type == BK_WIDGET_NUMBER) {
      w->value = bk_text(w->root, "", 0, 30, c->w);
      w->unit = bk_text(w->root, "", 0, c->h - 27, c->w);
      w->chart = bk_text(w->root, "", 0, 0,
                         c->w); /* title label, not a chart object */
    } else
      w->value = bk_text(w->root, "", 0, 0, c->w);
  }
  lv_obj_set_pos(w->root, c->x, c->y);
  lv_obj_set_size(w->root, c->w, c->h);
  if (c->type == BK_WIDGET_NEEDLE) {
    int side = c->w < c->h ? c->w : c->h;
    lv_obj_set_size(w->root, side, side);
    lv_obj_set_pos(w->root, c->x + (c->w - side) / 2, c->y + (c->h - side) / 2);
  }
  lv_obj_set_hidden(w->root, !c->visible);
}

static void reference_create(bk_widget_t *w, lv_obj_t *parent,
                             const bk_widget_config_t *c, bool dark) {
  if (c->type == BK_WIDGET_NEEDLE || c->type == BK_WIDGET_CHART ||
      c->type == BK_WIDGET_ALARM || c->type == BK_WIDGET_STATUS) {
    classic_create(w, parent, c, dark);
    w->style = BK_STYLE_REFERENCE;
    return;
  }
  memset(w, 0, sizeof(*w));
  w->config = *c;
  w->dark = dark;
  w->style = BK_STYLE_REFERENCE;
  w->target = -1;
  if (c->type == BK_WIDGET_ARC) {
    w->root = bk_gauge(parent, lv_color_hex(c->color));
    lv_arc_set_rotation(w->root, 170);
    lv_arc_set_bg_angles(w->root, 0, 200);
    lv_obj_set_style_arc_width(w->root, 32, LV_PART_MAIN);
    lv_obj_set_style_arc_width(w->root, 32, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(
        w->root, lv_color_hex(dark ? 0x34453b : 0xd9ded7), LV_PART_MAIN);
  } else {
    w->root = lv_obj_create(parent);
    lv_obj_remove_style_all(w->root);
    lv_obj_set_scrollable(w->root, false);
    if (c->type == BK_WIDGET_NUMBER) {
      w->value = bk_text(w->root, "", 0, -5, c->w);
      w->unit = bk_text(w->root, "", 0, c->h - 24, c->w);
      lv_obj_set_style_text_font(w->value,
                                 c->font_size >= 128 ? &bk_font_reference_128
                                 : c->font_size >= 96
                                     ? &bk_font_reference_engine
                                 : c->font_size >= 64 ? &bk_font_reference_64
                                                      : &lv_font_montserrat_48,
                                 0);
    } else if (c->type == BK_WIDGET_DIGITAL) {
      w->chart = bk_text(w->root, "", 0, c->h / 2 - 17, c->w * 45 / 100);
      lv_obj_set_style_text_font(w->chart, &bk_font_reference_header, 0);
      lv_obj_t *pill = lv_obj_create(w->root);
      lv_obj_set_pos(pill, c->w * 47 / 100, 0);
      lv_obj_set_size(pill, c->w * 53 / 100, c->h);
      lv_obj_set_style_radius(pill, 14, 0);
      lv_obj_set_style_bg_color(pill, lv_color_hex(dark ? 0x163d29 : 0xdfead7),
                                0);
      lv_obj_set_style_border_width(pill, 0, 0);
      lv_obj_set_scrollable(pill, false);
      w->value = bk_text(w->root, "", c->w * 47 / 100, -12, c->w * 53 / 100);
      lv_obj_set_style_text_font(w->value, &lv_font_montserrat_48, 0);
      w->unit =
          bk_text(w->root, "", c->w * 47 / 100, c->h - 23, c->w * 53 / 100);
    } else {
      w->chart = lv_bar_create(w->root);
      lv_obj_set_pos(w->chart, 0, 0);
      lv_obj_set_size(w->chart, c->w, 15);
      lv_bar_set_range(w->chart, 0, 1000);
      lv_obj_set_style_radius(w->chart, 7, 0);
      lv_obj_set_style_bg_color(w->chart,
                                lv_color_hex(dark ? 0x34453b : 0xd9ded7), 0);
      lv_obj_set_style_bg_color(w->chart, lv_color_hex(c->color),
                                LV_PART_INDICATOR);
      w->value = bk_text(w->root, "", 0, 22, c->w);
    }
  }
  lv_obj_set_pos(w->root, c->x, c->y);
  lv_obj_set_size(w->root, c->w, c->h);
  lv_obj_set_hidden(w->root, !c->visible);
}
static void indicator_exec(void *var, int32_t n) {
  bk_widget_t *w = var;
  const bk_widget_config_t *c = &w->config;
  if (c->type == BK_WIDGET_ARC)
    lv_arc_set_value(w->root, n);
  else if (c->type == BK_WIDGET_NEEDLE)
    lv_scale_set_line_needle_value(w->root, w->needle,
                                   (c->w < c->h ? c->w : c->h) / 2 - 14, n);
  else if (c->type == BK_WIDGET_BAR || c->type == BK_WIDGET_TEMPERATURE)
    lv_bar_set_value(w->chart, n, LV_ANIM_OFF);
}
static void widget_delete(lv_event_t *e) {
  bk_widget_t *w = lv_event_get_user_data(e);
  lv_anim_delete(w, indicator_exec);
}
void bk_widget_create_style(bk_widget_t *w, lv_obj_t *parent,
                            const bk_widget_config_t *c, bool dark,
                            bk_style_t style) {
  if (style == BK_STYLE_REFERENCE)
    reference_create(w, parent, c, dark);
  else
    classic_create(w, parent, c, dark);
  lv_obj_add_event_cb(w->root, widget_delete, LV_EVENT_DELETE, w);
}
void bk_widget_create(bk_widget_t *w, lv_obj_t *parent,
                      const bk_widget_config_t *c, bool dark) {
  bk_widget_create_style(w, parent, c, dark, BK_STYLE_CLASSIC);
}
static void animate_indicator(bk_widget_t *w, int n) {
  if (n == w->target)
    return;
  int from = w->target < 0                     ? n
             : w->config.type == BK_WIDGET_ARC ? lv_arc_get_value(w->root)
                                               : w->target;
  w->target = n;
  lv_anim_delete(w, indicator_exec);
  if (w->editing) {
    indicator_exec(w, n);
    return;
  }
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, w);
  lv_anim_set_values(&a, from, n);
  lv_anim_set_duration(&a, 150);
  lv_anim_set_exec_cb(&a, indicator_exec);
  lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
  lv_anim_start(&a);
}
void bk_reference_icon(lv_obj_t *parent, unsigned icon, int x, int y,
                       lv_color_t color) {
  static const lv_point_precise_t paths[][16] = {
      {{2, 8},
       {6, 8},
       {6, 5},
       {17, 5},
       {17, 8},
       {22, 8},
       {22, 19},
       {5, 19},
       {5, 16},
       {2, 16},
       {2, 8}},
      {{10, 3},
       {10, 15},
       {7, 19},
       {8, 23},
       {13, 24},
       {17, 21},
       {16, 17},
       {14, 15},
       {14, 3},
       {10, 3}},
      {{2, 7},
       {21, 7},
       {21, 20},
       {2, 20},
       {2, 7},
       {8, 7},
       {8, 3},
       {15, 3},
       {15, 7}},
      {{4, 23},
       {4, 3},
       {17, 3},
       {17, 23},
       {4, 23},
       {4, 7},
       {17, 7},
       {22, 10},
       {22, 20},
       {19, 20},
       {19, 12}},
      {{1, 13}, {5, 13}, {8, 5}, {13, 22}, {17, 10}, {20, 13}, {25, 13}},
      {{3, 24},
       {8, 2},
       {17, 2},
       {23, 24},
       {3, 24},
       {12, 20},
       {12, 15},
       {13, 10},
       {13, 6}},
      {{13, 2},
       {21, 5},
       {25, 13},
       {21, 21},
       {13, 25},
       {5, 21},
       {1, 13},
       {5, 5},
       {13, 2},
       {13, 7},
       {13, 13},
       {18, 16}},
      {{3, 2},
       {21, 2},
       {21, 24},
       {3, 24},
       {3, 2},
       {3, 19},
       {21, 19},
       {12, 21},
       {12, 22}},
      {{2, 8},
       {6, 5},
       {12, 3},
       {18, 5},
       {23, 8},
       {18, 12},
       {15, 10},
       {12, 9},
       {9, 10},
       {6, 12},
       {10, 17},
       {12, 15},
       {15, 17}},
      {{2, 2},
       {10, 2},
       {10, 10},
       {2, 10},
       {2, 2},
       {16, 2},
       {24, 2},
       {24, 10},
       {16, 10},
       {16, 2}},
      {{13, 1},
       {17, 5},
       {21, 5},
       {21, 10},
       {25, 14},
       {21, 18},
       {21, 23},
       {17, 23},
       {13, 27},
       {9, 23},
       {4, 23},
       {4, 18},
       {0, 14},
       {4, 10},
       {4, 5},
       {9, 5}}};
  static const uint8_t count[] = {11, 10, 9, 10, 7, 9, 12, 9, 13, 10, 16};
  unsigned n = icon % (sizeof(paths) / sizeof(paths[0]));
  lv_obj_t *line = lv_line_create(parent);
  lv_obj_set_pos(line, x, y);
  lv_line_set_points(line, paths[n], count[n]);
  lv_obj_set_style_transform_pivot_x(line, 0, 0);
  lv_obj_set_style_transform_pivot_y(line, 0, 0);
  lv_obj_set_style_transform_scale_x(line, 512, 0);
  lv_obj_set_style_transform_scale_y(line, 512, 0);
  lv_obj_set_style_line_width(line, 2, 0);
  lv_obj_set_style_line_rounded(line, true, 0);
  lv_obj_set_style_line_color(line, color, 0);
}
void bk_widget_update(bk_widget_t *w, double value, bk_value_state_t state,
                      bool connected, const char *name, const char *unit) {
  const bk_widget_config_t *c = &w->config;
  if (c->metric == BK_MAF &&
      (!strcmp(c->unit, "г/с") || !strcmp(c->unit, "g/s")))
    value /= 3.6;
  if ((c->metric == BK_VOLTAGE || c->metric == BK_LAMBDA_VOLTAGE) &&
      (!strcmp(c->unit, "мВ") || !strcmp(c->unit, "mV")))
    value *= 1000;
  bool valid = state == BK_VALUE_VALID && isfinite(value),
       warning =
           valid && (((c->flags & BK_WARN_LOW) && value < c->warn_low) ||
                     ((c->flags & BK_WARN_HIGH) && value >= c->warn_high));
  lv_color_t color = lv_color_hex(warning                           ? 0xc74732
                                  : w->dark && c->color == 0x126b48 ? 0x72d6aa
                                                                    : c->color);
  lv_color_t fg = lv_color_hex(w->dark                          ? 0xe9f1ed
                               : w->style == BK_STYLE_REFERENCE ? 0x171d1b
                                                                : 0x112b20);
  bool missing = !valid;
  lv_obj_set_hidden(
      w->root,
      !c->visible ||
          (missing && c->missing_policy == BK_MISSING_HIDE && !w->editing));
  lv_obj_set_style_opa(
      w->root,
      missing && c->missing_policy == BK_MISSING_INACTIVE && !w->editing ? 120
                                                                         : 255,
      0);
  double ratio = valid ? (value - c->min) / (c->max - c->min) : 0;
  if (ratio < 0)
    ratio = 0;
  if (ratio > 1)
    ratio = 1;
  int n = (int)(ratio * 1000);
  const char *u = c->unit[0] ? c->unit : unit, *state_text = "--";
  if (state == BK_VALUE_NEEDS_CALIBRATION)
    state_text = "Калибровка";
  else if (state == BK_VALUE_REQUIRES_SENSOR)
    state_text = "Нет датчика";
  else if (state == BK_VALUE_NO_SIGNAL)
    state_text = "Нет сигнала";
  else if (state == BK_VALUE_UNCONFIRMED)
    state_text = "Не подтверждено";
  else if (state == BK_VALUE_PLANNED)
    state_text = "Запланировано";
  else if (state == BK_VALUE_STALE)
    state_text = "Устарело";
  else if (state == BK_VALUE_UNSUPPORTED)
    state_text = "Недоступно";
  else if (state == BK_VALUE_INSUFFICIENT)
    state_text = "Нет данных";
  char number[64], buffer[192];
  if (valid)
    snprintf(number, sizeof(number), "%.*f", c->precision, value);
  else
    snprintf(number, sizeof(number), "%s", state_text);
  if (w->style == BK_STYLE_REFERENCE &&
      (c->type == BK_WIDGET_NUMBER || c->type == BK_WIDGET_DIGITAL)) {
    if (valid && c->metric == BK_TRIP_SECONDS)
      snprintf(number, sizeof(number), "%02u:%02u", (unsigned)value / 3600,
               ((unsigned)value / 60) % 60);
    text(w->value, number);
    text(w->unit, c->metric == BK_TRIP_SECONDS               ? "ч:мин"
                  : c->metric == BK_RANGE && !c->unit[0]     ? "км · запас хода"
                  : c->metric == BK_AVG_SPEED && !c->unit[0] ? "км/ч · средняя"
                                                             : u);
    lv_obj_set_style_text_font(
        w->value,
        valid ? (c->type == BK_WIDGET_DIGITAL ? &bk_font_reference_64
                 : c->font_size >= 128        ? &bk_font_reference_128
                 : c->font_size >= 96         ? &bk_font_reference_engine
                 : c->font_size >= 64         ? &bk_font_reference_64
                                              : &lv_font_montserrat_48)
              : &bk_font_ru_20,
        0);
    if (valid) {
      const lv_font_t *font =
          lv_obj_get_style_text_font(w->value, LV_PART_MAIN);
      lv_point_t size;
      lv_text_get_size(&size, number, font, 0, 0, 10000, LV_TEXT_FLAG_NONE);
      int available = c->type == BK_WIDGET_DIGITAL ? c->w * 53 / 100 : c->w;
      if (size.x > available) {
        font = c->font_size >= 128 ? &bk_font_reference_engine
                                   : &lv_font_montserrat_48;
        lv_text_get_size(&size, number, font, 0, 0, 10000, LV_TEXT_FLAG_NONE);
        if (size.x > available)
          font = &bk_font_ru_20;
        lv_obj_set_style_text_font(w->value, font, 0);
      }
    }
    lv_obj_set_style_text_color(w->value, warning ? color : fg, 0);
    if (c->type == BK_WIDGET_DIGITAL)
      text(w->chart, c->metric == BK_MAF              ? "ДМРВ"
                     : c->metric == BK_LAMBDA_VOLTAGE ? "ЛЯМБДА"
                                                      : name);
    return;
  }
  if (w->style == BK_STYLE_REFERENCE &&
      (c->type == BK_WIDGET_BAR || c->type == BK_WIDGET_TEMPERATURE)) {
    animate_indicator(w, n);
    lv_obj_set_style_bg_color(w->chart, color, LV_PART_INDICATOR);
    if (c->type == BK_WIDGET_TEMPERATURE) {
      snprintf(buffer, sizeof(buffer), "%.0f°          %.0f°          %.0f°",
               c->min, (c->min + c->max) / 2, c->max);
      if (c->metric == BK_COOLANT)
        text(w->value, "50°           90°          120°");
      else
        text(w->value, buffer);
    } else
      text(w->value, valid ? "E                       F" : "Нет данных");
    return;
  }
  switch (c->type) {
  case BK_WIDGET_ARC:
    animate_indicator(w, n);
    lv_obj_set_style_arc_color(w->root, color, LV_PART_INDICATOR);
    break;
  case BK_WIDGET_NEEDLE:
    animate_indicator(w, n);
    lv_obj_set_style_line_color(w->needle, color, 0);
    break;
  case BK_WIDGET_NUMBER:
    text(w->value, number);
    text(w->unit, u);
    text(w->chart, name);
    lv_obj_set_style_text_font(
        w->value,
        valid && c->font_size == 48 ? &lv_font_montserrat_48 : &bk_font_ru_20,
        0);
    lv_obj_set_style_text_color(w->value, warning ? color : fg, 0);
    break;
  case BK_WIDGET_STATUS:
    lv_obj_set_style_text_color(w->value, fg, 0);
    text(w->value, connected ? "ЭБУ: подключён" : "ЭБУ: нет связи");
    break;
  case BK_WIDGET_ALARM:
    snprintf(buffer, sizeof(buffer),
             warning ? "ВНИМАНИЕ\n%s: %s %s" : "%s\n%s %s", name, number, u);
    text(w->value, buffer);
    lv_obj_set_style_text_color(w->value, warning ? color : fg, 0);
    break;
  case BK_WIDGET_CHART:
    snprintf(buffer, sizeof(buffer), "%s · 64 с", name);
    text(w->value, buffer);
    lv_obj_set_style_text_color(w->value, fg, 0);
    break;
  default:
    snprintf(buffer, sizeof(buffer), "%s\n%s %s", name, number, u);
    text(w->value, buffer);
    lv_obj_set_style_text_color(w->value, warning ? color : fg, 0);
    if (c->type == BK_WIDGET_BAR || c->type == BK_WIDGET_TEMPERATURE) {
      lv_bar_set_value(w->chart, n, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(w->chart, color, LV_PART_INDICATOR);
    }
    break;
  }
}
void bk_widget_history_reset(void) {
  memset(heads, 0, sizeof(heads));
  memset(counts, 0, sizeof(counts));
}
void bk_widget_history_push(bk_metric_t metric, double value,
                            bk_value_state_t state) {
  if ((unsigned)metric >= BK_METRIC_COUNT)
    return;
  history[metric][heads[metric]] =
      state == BK_VALUE_VALID && isfinite(value) ? (float)value : NAN;
  heads[metric] = (heads[metric] + 1) % BK_HISTORY_POINTS;
  if (counts[metric] < BK_HISTORY_POINTS)
    counts[metric]++;
}
size_t bk_widget_history_count(bk_metric_t metric) {
  return (unsigned)metric < BK_METRIC_COUNT ? counts[metric] : 0;
}
void bk_widget_chart_refresh(bk_widget_t *w) {
  if (w->config.type != BK_WIDGET_CHART)
    return;
  unsigned m = w->config.metric;
  int32_t data[BK_HISTORY_POINTS];
  unsigned count = counts[m],
           start = (heads[m] + BK_HISTORY_POINTS - count) % BK_HISTORY_POINTS;
  for (unsigned i = 0; i < BK_HISTORY_POINTS; i++) {
    data[i] = LV_CHART_POINT_NONE;
    if (i >= BK_HISTORY_POINTS - count) {
      float v = history[m][(start + i - (BK_HISTORY_POINTS - count)) %
                           BK_HISTORY_POINTS];
      if (isfinite(v)) {
        double ratio = (v - w->config.min) / (w->config.max - w->config.min);
        if (ratio < 0)
          ratio = 0;
        if (ratio > 1)
          ratio = 1;
        data[i] = (int32_t)(ratio * 1000);
      }
    }
  }
  lv_chart_set_series_values(w->chart, w->series, data, BK_HISTORY_POINTS);
  lv_chart_refresh(w->chart);
}
