#include "bk_uaz_widgets.h"
#include "bk_uaz_fonts.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool number_font(unsigned n) {
  switch (n) {
  case 146:
  case 74:
  case 72:
  case 60:
  case 58:
  case 54:
  case 52:
  case 46:
  case 38:
  case 34:
  case 30:
  case 26:
    return true;
  default:
    return false;
  }
}
static lv_obj_t *box(lv_obj_t *parent, int x, int y, int width, int height,
                     uint32_t color, int radius) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, width, height);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, 255, 0);
  lv_obj_set_style_radius(o, radius, 0);
  lv_obj_set_clickable(o, false);
  return o;
}
static void text(lv_obj_t *o, const char *value) {
  if (o && strcmp(lv_label_get_text(o), value))
    lv_label_set_text(o, value);
}
static void place(lv_obj_t *label, const char *value, const lv_font_t *font,
                  int x, int y, int width, int height, lv_text_align_t align) {
  text(label, value);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_align(label, align, 0);
  lv_obj_set_style_text_letter_space(label, 0, 0);
  lv_point_t size;
  lv_text_get_size(&size, value, font, 0, 0, 10000, LV_TEXT_FLAG_NONE);
  if (size.x > width) {
    const unsigned sizes[] = {146, 128, 96, 74, 72, 64, 60, 58, 54,
                              52,  48,  46, 38, 34, 30, 26, 20};
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
      const lv_font_t *candidate = sizes[i] == 128  ? &bk_font_reference_128
                                   : sizes[i] == 96 ? &bk_font_reference_engine
                                   : sizes[i] == 64 ? &bk_font_reference_64
                                   : sizes[i] == 48 ? &lv_font_montserrat_48
                                   : sizes[i] == 20 ? &bk_font_ru_20
                                                    : bk_uaz_font(sizes[i]);
      if (candidate->line_height > font->line_height)
        continue;
      lv_text_get_size(&size, value, candidate, 0, 0, 10000, LV_TEXT_FLAG_NONE);
      if (size.x <= width) {
        font = candidate;
        break;
      }
      lv_text_get_size(&size, value, candidate, -1, 0, 10000,
                       LV_TEXT_FLAG_NONE);
      if (size.x <= width) {
        font = candidate;
        lv_obj_set_style_text_letter_space(label, -1, 0);
        break;
      }
    }
    lv_obj_set_style_text_font(label, font, 0);
  }
  lv_font_glyph_dsc_t glyph;
  int top = 0, ink = font->line_height;
  if (lv_font_get_glyph_dsc(font, &glyph, '8', 0)) {
    ink = glyph.box_h;
    top = font->line_height - font->base_line - glyph.box_h - glyph.ofs_y;
  }
  lv_obj_set_pos(label, x, y + (height - ink) / 2 - top);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_CLIP);
}
bool bk_uaz_widget_create(bk_widget_t *w, lv_obj_t *parent,
                          const bk_widget_config_t *c, bool dark) {
  bool arc = c->type == BK_WIDGET_ARC && c->w > c->h;
  bool digital = c->type == BK_WIDGET_DIGITAL;
  bool number = c->type == BK_WIDGET_NUMBER && number_font(c->font_size);
  bool temp = c->type == BK_WIDGET_TEMPERATURE;
  bool fuel = c->type == BK_WIDGET_BAR && c->metric == BK_FUEL_LEVEL &&
              c->font_size == 26;
  if (!arc && !digital && !number && !temp && !fuel)
    return false;
  memset(w, 0, sizeof(*w));
  w->config = *c;
  w->dark = dark;
  w->style = BK_STYLE_REFERENCE;
  w->target = -1;
  w->root = lv_obj_create(parent);
  lv_obj_remove_style_all(w->root);
  lv_obj_set_scrollable(w->root, false);
  lv_obj_set_pos(w->root, c->x, c->y);
  lv_obj_set_size(w->root, c->w, c->h);
  uint32_t track = dark ? 0x34453b : 0xc7cac6;
  if (arc) {
    w->chart = lv_arc_create(w->root);
    lv_obj_remove_style(w->chart, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(w->chart, false);
    int side = c->w * 384 / 400;
    lv_obj_set_size(w->chart, side, side);
    lv_obj_set_pos(w->chart, (c->w - side) / 2, c->h * 2 / 3 - side / 2);
    lv_arc_set_rotation(w->chart, 170);
    lv_arc_set_bg_angles(w->chart, 0, 200);
    lv_arc_set_range(w->chart, 0, 1000);
    lv_obj_set_style_arc_width(w->chart, c->w * 24 / 400, LV_PART_MAIN);
    lv_obj_set_style_arc_width(w->chart, c->w * 24 / 400, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(w->chart, lv_color_hex(track), LV_PART_MAIN);
    lv_obj_set_style_arc_color(w->chart, lv_color_hex(c->color),
                               LV_PART_INDICATOR);
  } else if (digital) {
    int pill = c->w * 185 / 345, pw = c->w - pill;
    w->chart = bk_text(w->root, "", 0, 0, pill - 5);
    w->backdrop =
        box(w->root, pill, 0, pw, c->h, dark ? 0x173f2c : 0xcce5d0, 14);
    w->value = bk_text(w->root, "", pill + 8, 0, pw - 16);
    w->unit = bk_text(w->root, "", pill + 8, 0, pw - 16);
    lv_obj_set_hidden(w->unit, true);
  } else if (number) {
    w->value = bk_text(w->root, "", 0, 0, c->w);
    w->unit = bk_text(w->root, "", 0, 0, c->w);
    w->chart = bk_text(w->root, "", 0, 0, c->w);
  } else if (temp) {
    w->chart = lv_bar_create(w->root);
    lv_obj_set_clickable(w->chart, false);
    lv_obj_set_pos(w->chart, c->w * 38 / 436, 0);
    lv_obj_set_size(w->chart, c->w * 360 / 436, 28 * c->h / 68);
    lv_bar_set_range(w->chart, 0, 1000);
    lv_obj_set_style_bg_color(w->chart, lv_color_hex(track), 0);
    lv_obj_set_style_radius(w->chart, 14, 0);
    lv_obj_set_style_radius(w->chart, 14, LV_PART_INDICATOR);
    w->backdrop = box(w->root, 38, 0, 3, 28, 0x075532, 1);
    w->value = bk_text(w->root, "", 0, 36, 80);
    w->unit = bk_text(w->root, "", 203, 36, 80);
    w->needle = bk_text(w->root, "", 356, 36, 80);
  } else {
    for (unsigned i = 0; i < 6; i++)
      w->segments[i] = box(w->root, c->w * (32 + 36 * (int)i) / 292, 0,
                           c->w * 32 / 292, c->h * 28 / 32, track, 4);
    w->value = bk_text(w->root, "E", 0, 0, 30);
    w->unit = bk_text(w->root, "F", c->w - 30, 0, 30);
  }
  lv_obj_set_hidden(w->root, !c->visible);
  return true;
}
bool bk_uaz_widget_update(bk_widget_t *w, double value, bool valid,
                          const char *metric_name, const char *unit,
                          const char *missing, lv_color_t ink,
                          lv_color_t accent) {
  const bk_widget_config_t *c = &w->config;
  if (w->style != BK_STYLE_REFERENCE)
    return false;
  if (c->type == BK_WIDGET_NUMBER && number_font(c->font_size)) {
    char number[64], full[112];
    if (valid)
      snprintf(number, sizeof(number), "%.*f", c->precision, value);
    else
      snprintf(number, sizeof(number), "%s", missing);
    if (valid && c->metric == BK_TRIP_SECONDS)
      snprintf(number, sizeof(number), "%02u:%02u", (unsigned)value / 3600,
               ((unsigned)value / 60) % 60);
    const char *suffix = unit;
    if (!c->unit[0] && c->metric == BK_VOLTAGE)
      suffix = "V";
    const lv_font_t *font =
        valid ? (c->font_size == 20    ? &bk_font_ru_20
                 : c->font_size == 48  ? &lv_font_montserrat_48
                 : c->font_size == 64  ? &bk_font_reference_64
                 : c->font_size == 96  ? &bk_font_reference_engine
                 : c->font_size == 128 ? &bk_font_reference_128
                                       : bk_uaz_font(c->font_size))
              : &bk_font_ru_20;
    text(w->unit, "");
    text(w->chart, "");
    if (c->font_size == 146) {
      place(w->value, number, font, 0, 0, c->w, c->h * 150 / 190,
            LV_TEXT_ALIGN_CENTER);
      place(w->unit, suffix, &bk_uaz_font_34, c->w * 73 / 276, c->h * 150 / 190,
            c->w * 130 / 276, c->h * 40 / 190, LV_TEXT_ALIGN_CENTER);
    } else if (c->font_size == 72) {
      place(w->value, number, font, 0, 0, c->w * 170 / 341, c->h * 80 / 100,
            LV_TEXT_ALIGN_LEFT);
      place(w->unit, suffix, &bk_uaz_font_30, c->w * 171 / 341, c->h * 30 / 100,
            c->w * 170 / 341, c->h * 38 / 100, LV_TEXT_ALIGN_LEFT);
    } else {
      bool no_unit = c->metric == BK_TRIP_SECONDS;
      snprintf(full, sizeof(full), "%s%s%s", number,
               valid && !no_unit && suffix[0] && c->metric != BK_COOLANT ? " "
                                                                         : "",
               valid && !no_unit ? suffix : "");
      int height = c->font_size == 60   ? c->h * 75 / 108
                   : c->font_size == 46 ? c->h * 60 / 92
                                        : c->h;
      place(w->value, full, font, 0, 0, c->w, height, LV_TEXT_ALIGN_CENTER);
      if (c->metric == BK_RANGE && c->font_size == 60)
        place(w->chart, "запас хода", &bk_font_reference_header, 0,
              c->h * 74 / 108, c->w, c->h * 34 / 108, LV_TEXT_ALIGN_CENTER);
      if (c->metric == BK_AVG_SPEED && c->font_size == 46)
        place(w->chart, "средняя", &bk_font_reference_header, c->w * 27 / 250,
              c->h * 59 / 92, c->w * 180 / 250, c->h * 32 / 92,
              LV_TEXT_ALIGN_CENTER);
    }
    lv_obj_set_style_text_color(w->value, ink, 0);
    lv_obj_set_style_text_color(w->unit, ink, 0);
    lv_obj_set_style_text_color(w->chart, ink, 0);
    return true;
  }
  if (c->type == BK_WIDGET_DIGITAL) {
    int pill = c->w * 185 / 345, pw = c->w - pill;
    char full[120];
    if (valid)
      snprintf(full, sizeof(full), "%.*f%s%s", c->precision, value,
               unit[0] && c->metric != BK_IGNITION ? " " : "", unit);
    else
      snprintf(full, sizeof(full), "%s", missing);
    const char *name = c->metric == BK_MAF              ? "ДМРВ"
                       : c->metric == BK_LAMBDA_VOLTAGE ? "ЛЯМБДА"
                       : c->metric == BK_IGNITION       ? "УОЗ"
                                                        : metric_name;
    place(w->chart, name, &bk_uaz_font_34, 0, 2, c->w * 180 / 345, c->h - 10,
          LV_TEXT_ALIGN_LEFT);
    place(w->value, full,
          valid ? (c->font_size == 20    ? &bk_font_ru_20
                   : c->font_size == 48  ? &lv_font_montserrat_48
                   : c->font_size == 64  ? &bk_font_reference_64
                   : c->font_size == 96  ? &bk_font_reference_engine
                   : c->font_size == 128 ? &bk_font_reference_128
                                         : bk_uaz_font(c->font_size))
                : &bk_font_ru_20,
          pill + 8, 8, pw - 16, c->h - 16, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_text_color(w->value, ink, 0);
    lv_obj_set_style_text_color(w->chart, ink, 0);
    return true;
  }
  if (c->type == BK_WIDGET_TEMPERATURE) {
    double ratio = valid ? (value - c->min) / (c->max - c->min) : 0;
    ratio = fmax(0, fmin(1, ratio));
    lv_bar_set_value(w->chart, (int)(ratio * 1000), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(w->chart, accent, LV_PART_INDICATOR);
    lv_obj_set_hidden(w->backdrop, !valid);
    lv_obj_set_pos(w->backdrop,
                   c->w * 38 / 436 + (int)(c->w * 360 / 436 * ratio) - 2, 0);
    char a[32], b[32], d[32];
    const char *u = c->metric == BK_COOLANT ? "°" : "";
    double middle = c->metric == BK_COOLANT && c->min == 50 && c->max == 120
                        ? 90
                        : (c->min + c->max) / 2;
    snprintf(a, sizeof(a), "%.0f%s", c->min, u);
    snprintf(b, sizeof(b), "%.0f%s", middle, u);
    snprintf(d, sizeof(d), "%.0f%s", c->max, u);
    place(w->value, a, &bk_uaz_font_26, 0, c->h * 36 / 68, c->w * 80 / 436,
          c->h * 32 / 68, LV_TEXT_ALIGN_CENTER);
    place(w->unit, b, &bk_uaz_font_26, c->w * 203 / 436, c->h * 36 / 68,
          c->w * 80 / 436, c->h * 32 / 68, LV_TEXT_ALIGN_CENTER);
    place(w->needle, d, &bk_uaz_font_26, c->w * 356 / 436, c->h * 36 / 68,
          c->w * 80 / 436, c->h * 32 / 68, LV_TEXT_ALIGN_CENTER);
    return true;
  }
  if (c->type == BK_WIDGET_BAR && c->metric == BK_FUEL_LEVEL &&
      c->font_size == 26) {
    int count =
        valid ? (int)lround(
                    fmax(0, fmin(1, (value - c->min) / (c->max - c->min))) * 6)
              : 0;
    for (int i = 0; i < 6; i++)
      lv_obj_set_hidden(w->segments[i], !valid);
    if (!valid) {
      place(w->value, missing, &bk_font_ru_20, 0, 0, c->w, c->h,
            LV_TEXT_ALIGN_CENTER);
      text(w->unit, "");
      return true;
    }
    for (int i = 0; i < 6; i++)
      lv_obj_set_style_bg_color(
          w->segments[i],
          i < count ? accent : lv_color_hex(w->dark ? 0x34453b : 0xc7cac6), 0);
    place(w->value, "E", &bk_uaz_font_26, 0, 0, 30, c->h, LV_TEXT_ALIGN_CENTER);
    place(w->unit, "F", &bk_uaz_font_26, c->w - 30, 0, 30, c->h,
          LV_TEXT_ALIGN_CENTER);
    return true;
  }
  return false;
}
