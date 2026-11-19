#include "bk_ui.h"
#include "bk_screens.h"
#include "bk_sources.h"
#include "bk_widgets.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DTC_LIMIT 127
static struct {
  lv_obj_t *root, *body, *status, *editor_error, *brightness_label,
      *clock_label;
  bk_layout_t layout;
  bk_widget_t widgets[BK_LAYOUT_MAX_WIDGETS];
  unsigned rendered;
  double values[BK_METRIC_COUNT];
  bk_value_state_t states[BK_METRIC_COUNT];
  unsigned screen, last_instrument;
  bool dark, connected;
  uint8_t brightness;
  char ssid[33], password[65], url[96], wifi_status[64], clock[6];
  const void *background_images[4];
  uint32_t connection_changed;
  bool nav_pressed;
  int nav_axis;
  lv_point_t nav_start;
  bool drag_active, drag_moved, drag_resize, grid;
  unsigned selected_widget;
  lv_point_t drag_start;
  bk_widget_config_t drag_origin;
  bk_layout_screen_t canvas_backup;
  unsigned category;
  bool service_confirmed, reset_pending;
  uint8_t dtcs[DTC_LIMIT];
  size_t dtc_count;
  bool dtcs_valid;
  const char *(*dtc_lookup)(uint8_t);
  void (*brightness_callback)(uint8_t, void *);
  void *brightness_context;
  lv_timer_t *history_timer;
  unsigned edit_mode, edit_screen, edit_widget;
  bool delete_pending;
  bk_widget_config_t draft;
  lv_obj_t *edit_values[17];
} ui;
static void show(void);
static void show_editor(void);
static void canvas_begin(lv_event_t *e);
static lv_color_t fg(void) {
  bool reference =
      ui.layout.screen_count &&
      ui.layout.screens[ui.last_instrument].style == BK_STYLE_REFERENCE;
  return lv_color_hex(ui.dark ? 0xe9f1ed : reference ? 0x171d1b : 0x112b20);
}
static lv_color_t accent(void) {
  bool reference =
      ui.layout.screen_count &&
      ui.layout.screens[ui.last_instrument].style == BK_STYLE_REFERENCE;
  return lv_color_hex(ui.dark ? 0x72d6aa : reference ? 0x00592d : 0x126b48);
}
static void label(lv_obj_t *o, const char *s) {
  if (o && strcmp(lv_label_get_text(o), s))
    lv_label_set_text(o, s);
}
static bool instrument(unsigned view) {
  int s = bk_layout_view_to_screen(view);
  return s >= 0 && (unsigned)s < ui.layout.screen_count &&
         ui.layout.screens[s].enabled;
}
static void refresh(void) {
  if (!ui.root)
    return;
  bool waiting = !ui.connected;
  lv_obj_set_hidden(ui.status, !waiting);
  if (waiting)
    label(ui.status, lv_tick_get() - ui.connection_changed < 700
                         ? "Подключение к ЭБУ"
                         : "Нет связи с ЭБУ");
  for (unsigned i = 0; i < ui.rendered; i++) {
    bk_widget_t *w = &ui.widgets[i];
    bk_metric_t m = w->config.metric;
    bk_widget_update(w, ui.values[m], ui.states[m], ui.connected,
                     bk_metric_names[m], bk_metric_units[m]);
  }
}
static void navigate(lv_event_t *e) {
  bk_ui_select_screen((unsigned)(uintptr_t)lv_event_get_user_data(e));
}
static lv_obj_t *button_on(lv_obj_t *parent, const char *s, int x, int y,
                           int width, lv_event_cb_t cb, uintptr_t value) {
  lv_obj_t *o = lv_button_create(parent);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, width, 44);
  lv_obj_set_style_bg_color(o, accent(), 0);
  lv_obj_set_style_radius(o, 14, 0);
  lv_obj_t *t = bk_text(o, s, 0, 0, width - 12);
  lv_obj_center(t);
  lv_obj_set_style_text_color(t, lv_color_hex(ui.dark ? 0x112b20 : 0xffffff),
                              0);
  if (cb)
    lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, (void *)value);
  return o;
}
static lv_obj_t *single_line(const char *text, int x, int y, int width) {
  lv_obj_t *o = bk_text(ui.body, text, x, y, width);
  lv_label_set_long_mode(o, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
  lv_obj_set_height(o, 26);
  return o;
}
static lv_obj_t *button(const char *s, int x, int y, int width,
                        unsigned target) {
  return button_on(ui.body, s, x, y, width, navigate, target);
}
static void theme_event(lv_event_t *e) {
  (void)e;
  bk_ui_set_theme(!ui.dark);
}
static void next_instrument(int direction) {
  int current = bk_layout_view_to_screen(ui.screen);
  if (current < 0)
    current = (int)ui.last_instrument;
  for (unsigned i = 0; i < ui.layout.screen_count; i++) {
    current = (current + direction + (int)ui.layout.screen_count) %
              (int)ui.layout.screen_count;
    if (ui.layout.screens[current].enabled) {
      bk_ui_select_screen(bk_layout_screen_to_view(current));
      return;
    }
  }
}

static void pointer_navigation(lv_event_t *e) {
  if (ui.edit_mode || !instrument(ui.screen))
    return;
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *input = lv_indev_active();
  if (!input)
    return;
  lv_point_t point;
  lv_indev_get_point(input, &point);
  if (code == LV_EVENT_PRESSED) {
    if (!ui.nav_pressed) {
      ui.nav_pressed = true;
      ui.nav_start = point;
      ui.nav_axis = 0;
    }
    return;
  }
  if (!ui.nav_pressed)
    return;
  int dx = point.x - ui.nav_start.x, dy = point.y - ui.nav_start.y;
  if (code == LV_EVENT_PRESSING && ui.nav_axis == 0 &&
      (abs(dx) >= 24 || abs(dy) >= 24)) {
    if (abs(dx) > abs(dy) * 1.2f)
      ui.nav_axis = 1;
    else if (abs(dy) > abs(dx) * 1.2f)
      ui.nav_axis = 2;
  }
  if (code == LV_EVENT_RELEASED) {
    ui.nav_pressed = false;
    if (ui.nav_axis == 2 && abs(dy) >= 40)
      next_instrument(dy < 0 ? 1 : -1);
    else if (ui.nav_axis == 1 && abs(dx) >= 40) {
      int slot = bk_layout_view_to_screen(ui.screen);
      if (slot >= 0 &&
          bk_layout_step_primary(&ui.layout.screens[slot], dx < 0 ? 1 : -1))
        show();
    }
  }
}
static void widget_pointer(lv_event_t *e) {
  unsigned n = (unsigned)(uintptr_t)lv_event_get_user_data(e);
  if (n >= ui.rendered)
    return;
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *input = lv_indev_active();
  if (!input)
    return;
  lv_point_t point;
  lv_indev_get_point(input, &point);
  if (ui.edit_mode != 3) {
    if (code == LV_EVENT_LONG_PRESSED && ui.nav_axis == 0) {
      ui.selected_widget = n;
      bk_ui_begin_edit();
    }
    return;
  }
  bk_layout_screen_t *screen = &ui.layout.screens[ui.edit_screen];
  bk_widget_config_t *w = &screen->widgets[n];
  if (code == LV_EVENT_PRESSED) {
    ui.selected_widget = n;
    ui.drag_active = true;
    ui.drag_moved = false;
    ui.drag_start = point;
    ui.drag_origin = *w;
    ui.drag_resize = point.x >= w->x + w->w - 22 && point.y >= w->y + w->h - 22;
    for (unsigned k = 0; k < ui.rendered; k++) {
      lv_obj_set_style_outline_width(ui.widgets[k].root, k == n ? 3 : 1, 0);
    }
  } else if (code == LV_EVENT_PRESSING && ui.drag_active &&
             ui.selected_widget == n) {
    int dx = point.x - ui.drag_start.x, dy = point.y - ui.drag_start.y;
    if (abs(dx) + abs(dy) < 6 && !ui.drag_moved)
      return;
    ui.drag_moved = true;
    bk_widget_config_t old = *w;
    *w = ui.drag_origin;
    if (ui.drag_resize) {
      w->w += dx;
      w->h += dy;
    } else {
      w->x += dx;
      w->y += dy;
    }
    if (ui.grid) {
      w->x = (w->x + 2) / 4 * 4;
      w->y = (w->y + 2) / 4 * 4;
      w->w = (w->w + 2) / 4 * 4;
      w->h = (w->h + 2) / 4 * 4;
    }
    if (!bk_layout_validate(&ui.layout)) {
      *w = old;
      label(ui.editor_error, "Граница круга или размер");
      return;
    }
    ui.widgets[n].config = *w;
    lv_obj_set_pos(ui.widgets[n].root, w->x, w->y);
    lv_obj_set_size(ui.widgets[n].root, w->w, w->h);
    bool overlap = false;
    for (unsigned k = 0; k < screen->widget_count; k++)
      if (k != n && screen->widgets[k].visible) {
        bk_widget_config_t *q = &screen->widgets[k];
        if (w->x < q->x + q->w && w->x + w->w > q->x && w->y < q->y + q->h &&
            w->y + w->h > q->y)
          overlap = true;
      }
    label(ui.editor_error,
          overlap ? "Пересечение элементов" : "Переместите угол для размера");
  } else if (code == LV_EVENT_RELEASED) {
    ui.drag_active = false;
    if (ui.drag_moved)
      show();
  }
}
static void editor_list(lv_event_t *e) {
  (void)e;
  ui.edit_screen = ui.last_instrument;
  ui.edit_mode = 1;
  show_editor();
}
static lv_obj_t *scroll_panel(void) {
  lv_obj_t *p = lv_obj_create(ui.body);
  lv_obj_set_pos(p, 75, 113);
  lv_obj_set_size(p, 330, 247);
  lv_obj_set_style_text_color(p, fg(), 0);
  lv_obj_set_style_bg_color(p, lv_color_hex(ui.dark ? 0x172b20 : 0xf7f7ef), 0);
  lv_obj_set_style_border_width(p, 0, 0);
  lv_obj_set_style_pad_all(p, 6, 0);
  lv_obj_set_scroll_dir(p, LV_DIR_VER);
  return p;
}
static void edit_pick(lv_event_t *e) {
  unsigned n = (unsigned)(uintptr_t)lv_event_get_user_data(e);
  bk_layout_screen_t *s = &ui.layout.screens[ui.edit_screen];
  ui.edit_widget = n;
  ui.draft = n < s->widget_count
                 ? s->widgets[n]
                 : (bk_widget_config_t){.type = BK_WIDGET_NUMBER,
                                        .metric = BK_RPM,
                                        .x = 100,
                                        .y = 150,
                                        .w = 280,
                                        .h = 130,
                                        .min = 0,
                                        .max = 6000,
                                        .color = 0x126b48,
                                        .visible = true,
                                        .font_size = 48};
  ui.edit_mode = 2;
  show_editor();
}
static void editor_exit(lv_event_t *e) {
  (void)e;
  ui.edit_mode = 0;
  show();
}
static void editor_back(lv_event_t *e) {
  (void)e;
  ui.edit_mode = 1;
  show_editor();
}
static void edit_screen_change(lv_event_t *e) {
  (void)e;
  ui.edit_screen = (ui.edit_screen + 1) % ui.layout.screen_count;
  show_editor();
}
static void edit_delete(lv_event_t *e) {
  if (!ui.delete_pending) {
    ui.delete_pending = true;
    label(ui.editor_error, "Нажмите ещё раз для удаления");
    lv_obj_t *b = lv_event_get_target_obj(e);
    label(lv_obj_get_child(b, 0), "Подтвердить удаление");
    lv_obj_set_style_bg_color(b, lv_color_hex(0xc74732), 0);
    return;
  }
  bk_layout_screen_t *s = &ui.layout.screens[ui.edit_screen];
  if (ui.edit_widget < s->widget_count) {
    for (unsigned n = ui.edit_widget + 1; n < s->widget_count; n++)
      s->widgets[n - 1] = s->widgets[n];
    s->widget_count--;
  }
  ui.edit_mode = 1;
  show_editor();
}
static void edit_save(lv_event_t *e) {
  (void)e;
  bk_layout_screen_t *s = &ui.layout.screens[ui.edit_screen];
  bool add = ui.edit_widget == s->widget_count;
  bk_widget_config_t before = s->widgets[ui.edit_widget];
  s->widgets[ui.edit_widget] = ui.draft;
  if (add)
    s->widget_count++;
  if (!bk_layout_validate(&ui.layout)) {
    s->widgets[ui.edit_widget] = before;
    if (add)
      s->widget_count--;
    label(ui.editor_error, "Проверьте размеры и диапазон");
    return;
  }
  ui.edit_mode = 1;
  show_editor();
}
static const uint32_t palette[] = {0x126b48, 0x2484bf, 0xc74732,
                                   0xb87516, 0x8059b4, 0x72d6aa};
static void update_editor_labels(void) {
  char b[96];
  bk_widget_config_t *w = &ui.draft;
  snprintf(b, sizeof(b), "Вид: %s", bk_widget_registry[w->type].name);
  label(ui.edit_values[0], b);
  snprintf(b, sizeof(b), "Параметр: %s", bk_metric_names[w->metric]);
  label(ui.edit_values[1], b);
  const double v[] = {w->x,         w->y,        w->w,        w->h,
                      w->min,       w->max,      w->warn_low, w->warn_high,
                      w->precision, w->font_size};
  const char *names[] = {"X",        "Y",     "Ширина",     "Высота",
                         "Мин.",     "Макс.", "Порог низ.", "Порог верх.",
                         "Точность", "Шрифт"};
  for (unsigned i = 0; i < 10; i++) {
    snprintf(b, sizeof(b), "%s: %.1f", names[i], v[i]);
    label(ui.edit_values[i + 2], b);
  }
  snprintf(b, sizeof(b), "Цвет: #%06lx", (unsigned long)w->color);
  label(ui.edit_values[12], b);
  snprintf(b, sizeof(b), "Пороги: %s%s", w->flags & BK_WARN_LOW ? "низ " : "",
           w->flags & BK_WARN_HIGH ? "верх" : "выкл");
  label(ui.edit_values[13], b);
  label(ui.edit_values[14], w->visible ? "Видимость: да" : "Видимость: нет");
  snprintf(b, sizeof(b), "Единицы: %s", w->unit[0] ? w->unit : "авто");
  label(ui.edit_values[15], b);
}
static void edit_step(lv_event_t *e) {
  unsigned code = (unsigned)(uintptr_t)lv_event_get_user_data(e),
           field = code / 2;
  int d = (code % 2) ? 1 : -1;
  bk_widget_config_t *w = &ui.draft;
  switch (field) {
  case 0:
    w->type = (w->type + d + BK_WIDGET_COUNT) % BK_WIDGET_COUNT;
    break;
  case 1:
    w->metric = (w->metric + d + BK_METRIC_COUNT) % BK_METRIC_COUNT;
    w->min = 0;
    w->max = bk_metric_max[w->metric];
    w->unit[0] = 0;
    w->flags = 0;
    w->precision = w->metric == BK_VOLTAGE || w->metric == BK_FUEL_LPH ||
                           w->metric == BK_INJECTION
                       ? 1
                       : 0;
    if (w->metric == BK_COOLANT) {
      w->flags = BK_WARN_HIGH;
      w->warn_high = 105;
    }
    if (w->metric == BK_RPM) {
      w->flags = BK_WARN_HIGH;
      w->warn_high = 5800;
    }
    if (w->metric == BK_VOLTAGE) {
      w->flags = BK_WARN_LOW;
      w->warn_low = 12;
    }
    break;
  case 2:
    if (w->x + d * 4 >= 0 && w->x + d * 4 <= 464)
      w->x += d * 4;
    break;
  case 3:
    if (w->y + d * 4 >= 0 && w->y + d * 4 <= 416)
      w->y += d * 4;
    break;
  case 4:
    if (w->w + d * 4 >= 16 && w->w + d * 4 <= 480)
      w->w += d * 4;
    break;
  case 5:
    if (w->h + d * 4 >= 16 && w->h + d * 4 <= 416)
      w->h += d * 4;
    break;
  case 6:
    w->min += d;
    break;
  case 7:
    w->max += d;
    break;
  case 8:
    w->warn_low += d;
    break;
  case 9:
    w->warn_high += d;
    break;
  case 10:
    w->precision = (w->precision + d + 4) % 4;
    break;
  case 11:
    w->font_size = w->font_size == 20 ? 48 : 20;
    break;
  case 12: {
    unsigned p = 0;
    while (p < 6 && palette[p] != w->color)
      p++;
    w->color = palette[(p + d + 6) % 6];
    break;
  }
  case 13:
    w->flags = (w->flags + d + 4) % 4;
    break;
  case 14:
    w->visible = !w->visible;
    break;
  case 15:
    if (w->unit[0])
      w->unit[0] = 0;
    else
      snprintf(w->unit, sizeof(w->unit), "%s", bk_metric_units[w->metric]);
    break;
  default:
    break;
  }
  update_editor_labels();
  label(ui.editor_error, "");
}
static void show_editor(void) {
  ui.rendered = 0;
  lv_obj_clean(ui.body);
  ui.editor_error = NULL;
  ui.delete_pending = false;
  memset(ui.edit_values, 0, sizeof(ui.edit_values));
  bk_text(ui.body, ui.edit_mode == 1 ? "РЕДАКТОР ЭКРАНА" : "НАСТРОЙКА ВИДЖЕТА",
          60, 66, 360);
  lv_obj_t *panel = scroll_panel();
  bk_layout_screen_t *s = &ui.layout.screens[ui.edit_screen];
  if (ui.edit_mode == 1) {
    button_on(panel, s->title, 0, 0, 300, edit_screen_change, 0);
    for (unsigned i = 0; i < s->widget_count; i++) {
      char b[80];
      snprintf(b, sizeof(b), "%u. %s", i + 1,
               bk_widget_registry[s->widgets[i].type].name);
      button_on(panel, b, 0, 54 + (int)i * 52, 300, edit_pick, i);
    }
    if (s->widget_count < BK_LAYOUT_MAX_WIDGETS)
      button_on(panel, "+ Добавить виджет", 0, 54 + (int)s->widget_count * 52,
                300, edit_pick, s->widget_count);
    button_on(ui.body, "Двигать", 90, 381, 140, canvas_begin, 0);
    button_on(ui.body, "Готово", 250, 381, 140, editor_exit, 0);
  } else {
    for (unsigned f = 0; f < 16; f++) {
      int y = (int)f * 66;
      ui.edit_values[f] = bk_text(panel, "", 40, y, 220);
      button_on(panel, "-", 0, y + 12, 42, edit_step, f * 2);
      button_on(panel, "+", 266, y + 12, 42, edit_step, f * 2 + 1);
    }
    button_on(panel, "Удалить виджет", 36, 1070, 240, edit_delete, 0);
    ui.editor_error = bk_text(ui.body, "", 65, 362, 350);
    lv_obj_set_style_text_color(ui.editor_error, lv_color_hex(0xc74732), 0);
    button_on(ui.body, "Отмена", 95, 391, 138, editor_back, 0);
    button_on(ui.body, "Сохранить", 245, 391, 145, edit_save, 0);
    update_editor_labels();
  }
}
static void brightness_changed(lv_event_t *e) {
  unsigned n = (unsigned)lv_slider_get_value(lv_event_get_target_obj(e));
  ui.brightness = n;
  char text[40];
  snprintf(text, sizeof(text), "Яркость %u%%", ui.brightness);
  label(ui.brightness_label, text);
  if (ui.brightness_callback)
    ui.brightness_callback(ui.brightness, ui.brightness_context);
}

static void divider(int y) {
  lv_obj_t *o = lv_obj_create(ui.body);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, 65, y);
  lv_obj_set_size(o, 350, 1);
  lv_obj_set_style_bg_opa(o, 255, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(ui.dark ? 0x385044 : 0xd2d7cf), 0);
}
static void reference_header(const char *title, unsigned icon) {
  bool reference =
      ui.layout.screens[ui.last_instrument].style == BK_STYLE_REFERENCE;
  const lv_font_t *font =
      reference ? &bk_font_reference_header : &bk_font_ru_20;
  lv_point_t size;
  lv_text_get_size(&size, title, font, 0, 0, 480, LV_TEXT_FLAG_NONE);
  if (size.x > 240) {
    font = &bk_font_ru_20;
    lv_text_get_size(&size, title, font, 0, 0, 480, LV_TEXT_FLAG_NONE);
  }
  int total = size.x + 60, x = 240 - total / 2;
  bk_reference_icon(ui.body, icon, x, 52, accent());
  lv_obj_t *t = bk_text(ui.body, title, x + 60, 59, size.x + 2);
  lv_obj_set_style_text_font(t, font, 0);
}
static void draw_background(const bk_layout_screen_t *screen) {
  if (screen->background_mode == BK_BACKGROUND_USER &&
      screen->background_id < 4 &&
      ui.background_images[screen->background_id]) {
    lv_obj_t *image = lv_image_create(ui.body);
    lv_image_set_src(image, ui.background_images[screen->background_id]);
    const lv_image_dsc_t *source = ui.background_images[screen->background_id];
    unsigned edge = source->header.w < source->header.h ? source->header.w
                                                        : source->header.h;
    lv_image_set_scale(image, edge ? 480U * 256U * screen->background_scale /
                                         (edge * 100U)
                                   : 256);
    lv_obj_center(image);
  } else if (screen->background_mode == BK_BACKGROUND_BUILTIN) {
    /* Lightweight built-in contour/grid texture drawn by LVGL, no guessed
     * uploaded pixels. */
    for (unsigned i = 0; i < 7; i++) {
      lv_obj_t *ring = lv_obj_create(ui.body);
      lv_obj_remove_style_all(ring);
      int size = 130 + (int)i * 54;
      lv_obj_set_size(ring, size, size);
      lv_obj_center(ring);
      lv_obj_set_style_radius(ring, size / 2, 0);
      lv_obj_set_style_border_width(ring, 1, 0);
      lv_obj_set_style_border_color(
          ring, lv_color_hex(ui.dark ? 0x2b4636 : 0xdbe4d5), 0);
    }
  }
  if (screen->background_dim) {
    lv_obj_t *overlay = lv_obj_create(ui.body);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, 480, 480);
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0), 0);
    lv_obj_set_style_bg_opa(overlay, screen->background_dim * 255 / 100, 0);
    lv_obj_set_clickable(overlay, false);
  }
}
static void reference_rim(void) {
  lv_obj_t *rim = lv_obj_create(ui.body);
  lv_obj_remove_style_all(rim);
  lv_obj_set_size(rim, 480, 480);
  lv_obj_set_style_radius(rim, 240, 0);
  lv_obj_set_style_border_width(rim, 10, 0);
  lv_obj_set_style_border_color(rim,
                                lv_color_hex(ui.dark ? 0x52665a : 0x171d1b), 0);
  lv_obj_set_clickable(rim, false);
}
static void ref_decor(const bk_layout_screen_t *screen) {
  if (!strcmp(screen->id, "main")) {
    ui.clock_label = bk_text(ui.body, ui.clock, 190, 22, 100);
    if (ui.ssid[0])
      bk_reference_icon(ui.body, 8, 337, 43, accent());
    divider(325);
    bk_reference_icon(ui.body, 0, 65, 355, accent());
  } else if (!strcmp(screen->id, "engine")) {
    divider(310);
    bk_reference_icon(ui.body, 1, 59, 173, accent());
    bk_reference_icon(ui.body, 2, 64, 350, accent());
  } else if (!strcmp(screen->id, "fuel")) {
    divider(240);
    bk_reference_icon(ui.body, 3, 67, 292, accent());
  } else if (!strcmp(screen->id, "trip")) {
    bk_reference_icon(ui.body, 5, 72, 151, accent());
    bk_reference_icon(ui.body, 6, 72, 247, accent());
    bk_reference_icon(ui.body, 0, 72, 344, accent());
  }
  for (unsigned n = 0; n < ui.layout.screen_count; n++) {
    lv_obj_t *dot = lv_obj_create(ui.body);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 5, 5);
    lv_obj_set_pos(dot, 240 - (int)ui.layout.screen_count * 6 + (int)n * 12,
                   416);
    lv_obj_set_style_radius(dot, 3, 0);
    lv_obj_set_style_bg_opa(dot, 255, 0);
    lv_obj_set_style_bg_color(
        dot, lv_color_hex(n == ui.last_instrument ? 0x009d31 : 0xc3cfc3), 0);
  }
}
static void canvas_action(lv_event_t *e) {
  unsigned action = (unsigned)(uintptr_t)lv_event_get_user_data(e);
  if (action == 0)
    bk_ui_end_edit(false);
  else if (action == 1)
    bk_ui_end_edit(true);
  else if (action == 2)
    bk_ui_reset_screen();
  else {
    ui.grid = !ui.grid;
    show();
  }
}
static void canvas_begin(lv_event_t *e) {
  (void)e;
  ui.edit_mode = 0;
  ui.screen = bk_layout_screen_to_view(ui.edit_screen);
  bk_ui_begin_edit();
}
static void set_category(lv_event_t *e) {
  ui.category = (unsigned)(uintptr_t)lv_event_get_user_data(e);
  ui.service_confirmed = false;
  bk_ui_select_screen(9);
}
static void service_confirm(lv_event_t *e) {
  (void)e;
  ui.service_confirmed = true;
  show();
}
static void factory_reference(lv_event_t *e) {
  (void)e;
  ui.layout.screens[ui.last_instrument].style = BK_STYLE_REFERENCE;
  bk_ui_reset_screen();
}
static void style_toggle(lv_event_t *e) {
  (void)e;
  bk_ui_set_style(ui.layout.screens[ui.last_instrument].style ==
                          BK_STYLE_CLASSIC
                      ? BK_STYLE_REFERENCE
                      : BK_STYLE_CLASSIC);
}
static void title_toggle(lv_event_t *e) {
  (void)e;
  ui.layout.screens[ui.last_instrument].show_title =
      !ui.layout.screens[ui.last_instrument].show_title;
  show();
}
static void catalog_page(void) {
  reference_header(ui.category == BK_SECTION_SERVICE ? "Сервисный режим"
                                                     : "Диагностика",
                   ui.category == BK_SECTION_SERVICE ? 10 : 0);
  if (ui.category == BK_SECTION_SERVICE && !ui.service_confirmed) {
    bk_text(ui.body,
            "Только при остановленном\nавтомобиле. Команды управления\nв этой "
            "версии отключены.",
            65, 155, 350);
    button_on(ui.body, "Подтвердить вход", 100, 285, 280, service_confirm, 0);
  } else {
    lv_obj_t *p = scroll_panel();
    int y = 0;
    if (ui.category == BK_SECTION_DIAGNOSTICS) {
      button_on(p, "Ошибки ЭБУ", 0, y, 300, navigate, 7);
      y += 55;
      char text[120];
      char maf[32], lambda[32];
      if (ui.states[BK_MAF_RAW] == BK_VALUE_VALID)
        snprintf(maf, sizeof(maf), "%.0f", ui.values[BK_MAF_RAW]);
      else
        snprintf(maf, sizeof(maf), "--");
      if (ui.states[BK_LAMBDA] == BK_VALUE_VALID)
        snprintf(lambda, sizeof(lambda), "%.0f", ui.values[BK_LAMBDA]);
      else
        snprintf(lambda, sizeof(lambda), "--");
      snprintf(text, sizeof(text), "Связь: %s\nДМРВ raw: %s\nЛямбда raw: %s",
               ui.connected ? "есть" : "нет", maf, lambda);
      bk_text(p, text, 0, y, 300);
      y += 100;
    }
    size_t count = 0;
    const bk_function_t *functions = bk_function_catalog(&count);
    for (size_t i = 0; i < count; i++)
      if (functions[i].section == (bk_function_section_t)ui.category) {
        char text[300];
        snprintf(text, sizeof(text), "%s\n%s\n%s", functions[i].name,
                 ui.category == BK_SECTION_SERVICE
                     ? "Команда отключена"
                     : bk_function_status_name(functions[i].status),
                 functions[i].reason ? functions[i].reason : "");
        lv_obj_t *row = bk_text(p, text, 0, y, 300);
        lv_obj_update_layout(row);
        y += lv_obj_get_height(row) + 18;
      }
  }
  button("Назад", 180, 391, 120, 5);
}
static void menu_tile(const char *title, unsigned icon, int x, int y,
                      lv_event_cb_t cb, uintptr_t arg, bool active) {
  lv_obj_t *o = button_on(ui.body, "", x, y, 180, cb, arg);
  lv_obj_set_size(o, 180, 116);
  lv_obj_set_style_bg_color(o,
                            lv_color_hex(active    ? 0x009d31
                                         : ui.dark ? 0x283c30
                                                   : 0xe5e9df),
                            0);
  lv_obj_t *text = bk_text(o, title, 2, 71, 160);
  lv_obj_set_style_text_color(text,
                              lv_color_hex(active    ? 0xffffff
                                           : ui.dark ? 0xe9f1ed
                                                     : 0x171d1b),
                              0);
  bk_reference_icon(o, icon, 69, 23,
                    lv_color_hex(active ? 0xffffff : 0x00592d));
}
static void show(void) {
  if (!ui.root)
    return;
  if (ui.edit_mode == 1 || ui.edit_mode == 2) {
    show_editor();
    return;
  }
  ui.rendered = 0;
  lv_obj_clean(ui.body);
  ui.brightness_label = NULL;
  ui.clock_label = NULL;
  ui.editor_error = NULL;
  int si = bk_layout_view_to_screen(ui.screen);
  bk_layout_screen_t *screen = si >= 0 && (unsigned)si < ui.layout.screen_count
                                   ? &ui.layout.screens[si]
                                   : &ui.layout.screens[ui.last_instrument];
  bool reference = screen->style == BK_STYLE_REFERENCE;
  lv_obj_set_style_bg_color(
      ui.root, lv_color_hex(ui.dark ? 0x101c17 : screen->background), 0);
  lv_obj_set_style_text_color(ui.root, fg(), 0);
  lv_obj_set_style_border_width(ui.root, 0, 0);
  lv_obj_set_style_border_color(ui.root,
                                lv_color_hex(ui.dark ? 0x52665a : 0x171d1b), 0);
  lv_obj_set_pos(ui.status, 100, reference ? 91 : 438);
  lv_obj_set_width(ui.status, 280);
  if (reference && !instrument(ui.screen))
    reference_rim();
  if (instrument(ui.screen)) {
    ui.last_instrument = (unsigned)si;
    draw_background(screen);
    if (reference)
      reference_rim();
    if (reference) {
      ref_decor(screen);
      if (screen->show_title) {
        unsigned icon = !strcmp(screen->id, "engine")    ? 0
                        : !strcmp(screen->id, "fuel")    ? 3
                        : !strcmp(screen->id, "sensors") ? 4
                                                         : 5;
        reference_header(screen->title, icon);
      }
    } else if (screen->show_title)
      bk_text(ui.body, screen->title, 85, 76, 310);
    for (unsigned n = 0; n < screen->widget_count; n++) {
      bk_widget_create_style(&ui.widgets[n], ui.body, &screen->widgets[n],
                             ui.dark, screen->style);
      ui.widgets[n].editing = ui.edit_mode == 3;
      if (screen->widgets[n].visible) {
        lv_obj_set_clickable(ui.widgets[n].root, true);
        lv_obj_set_event_bubble(ui.widgets[n].root, true);
        lv_obj_add_event_cb(ui.widgets[n].root, widget_pointer, LV_EVENT_ALL,
                            (void *)(uintptr_t)n);
      }
      if (ui.edit_mode == 3) {
        lv_obj_set_style_outline_color(ui.widgets[n].root, accent(), 0);
        lv_obj_set_style_outline_width(ui.widgets[n].root,
                                       n == ui.selected_widget ? 3 : 1, 0);
        lv_obj_t *handle = lv_obj_create(ui.widgets[n].root);
        lv_obj_remove_style_all(handle);
        lv_obj_set_size(handle, 14, 14);
        lv_obj_set_pos(handle, screen->widgets[n].w - 15,
                       screen->widgets[n].h - 15);
        lv_obj_set_style_bg_opa(handle, 255, 0);
        lv_obj_set_style_bg_color(handle, accent(), 0);
        lv_obj_set_clickable(handle, false);
      }
    }
    ui.rendered = screen->widget_count;
    if (ui.edit_mode == 3) {
      ui.editor_error =
          bk_text(ui.body, "Перетащите элемент или его угол", 65, 87, 350);
      const char *names[] = {"Нет", "OK", "Сброс",
                             ui.grid ? "Сетка" : "Без сетки"};
      for (unsigned n = 0; n < 4; n++) {
        lv_obj_t *b = button_on(ui.body, names[n], 104 + (int)n * 70, 398, 68,
                                canvas_action, n);
        lv_obj_set_height(b, 32);
      }
    } else if (reference) {
      lv_obj_t *b = button("Меню", 196, 431, 88, 5);
      lv_obj_set_height(b, 34);
    } else
      button("Меню", 180, 381, 120, 5);
  } else if (ui.screen == 5) {
    if (reference) {
      reference_header("Меню", 9);
      menu_tile("Показания", 0, 55, 125, navigate,
                bk_layout_screen_to_view(ui.last_instrument), true);
      menu_tile("Диагностика", 0, 245, 125, set_category,
                BK_SECTION_DIAGNOSTICS, false);
      menu_tile("Сервис", 10, 55, 257, set_category, BK_SECTION_SERVICE, false);
      menu_tile("Настройки", 10, 245, 257, navigate, 8, false);
    } else {
      bk_text(ui.body, "HUNTER · МЕНЮ", 80, 65, 320);
      button("Показания", 115, 113, 250,
             bk_layout_screen_to_view(ui.last_instrument));
      button_on(ui.body, "Диагностика", 115, 167, 250, set_category,
                BK_SECTION_DIAGNOSTICS);
      button_on(ui.body, "Сервисный режим", 115, 221, 250, set_category,
                BK_SECTION_SERVICE);
      button("Настройки", 115, 275, 250, 8);
      button("Подключение", 140, 329, 200, 6);
    }
  } else if (ui.screen == 6) {
    reference_header("Подключение", 7);
    if (ui.ssid[0]) {
      char data[240], ss[67], pw[131];
      size_t s = 0, p = 0;
      for (size_t i = 0; ui.ssid[i]; i++) {
        if (strchr("\\;,:\"", ui.ssid[i]))
          ss[s++] = '\\';
        ss[s++] = ui.ssid[i];
      }
      ss[s] = 0;
      for (size_t i = 0; ui.password[i]; i++) {
        if (strchr("\\;,:\"", ui.password[i]))
          pw[p++] = '\\';
        pw[p++] = ui.password[i];
      }
      pw[p] = 0;
      snprintf(data, sizeof(data), "WIFI:T:%s;S:%s;P:%s;;",
               ui.password[0] ? "WPA" : "nopass", ss, pw);
      lv_obj_t *qr = lv_qrcode_create(ui.body);
      lv_qrcode_set_size(qr, 180);
      lv_qrcode_set_dark_color(qr, lv_color_hex(0));
      lv_qrcode_set_light_color(qr, lv_color_hex(0xffffff));
      lv_obj_set_pos(qr, 150, 110);
      lv_qrcode_update(qr, data, strlen(data));
      single_line(ui.ssid, 65, 303, 350);
      char b[100];
      snprintf(b, sizeof(b), "Пароль: %s", ui.password);
      single_line(b, 60, 333, 360);
      single_line(ui.url, 65, 362, 350);
    } else
      bk_text(ui.body, "Точка доступа выключена\nQR появится после запуска AP",
              70, 185, 340);
    button("Назад", 180, 411, 120, 5);
  } else if (ui.screen == 7) {
    reference_header("Ошибки ЭБУ", 0);
    lv_obj_t *p = scroll_panel();
    if (!ui.dtcs_valid)
      bk_text(p, "Список ошибок недоступен", 0, 20, 300);
    else if (!ui.dtc_count)
      bk_text(p, "Ошибок не обнаружено", 0, 20, 300);
    else {
      int y = 0;
      for (size_t n = 0; n < ui.dtc_count; n++) {
        char b[180];
        const char *description =
            ui.dtc_lookup ? ui.dtc_lookup(ui.dtcs[n]) : NULL;
        snprintf(b, sizeof(b), "Код ЭБУ 0x%02X\n%s", ui.dtcs[n],
                 description ? description : "Описание не подтверждено");
        lv_obj_t *row = bk_text(p, b, 0, y, 300);
        lv_obj_update_layout(row);
        y += lv_obj_get_height(row) + 18;
      }
    }
    button("Назад", 180, 391, 120, 5);
  } else if (ui.screen == 8) {
    reference_header("Настройки", 10);
    lv_obj_t *p = scroll_panel();
    if (reference) {
      char b[64];
      snprintf(b, sizeof(b), "Яркость: %u%%", ui.brightness);
      ui.brightness_label = bk_text(p, b, 0, 0, 310);
      lv_obj_set_style_text_font(ui.brightness_label, &bk_font_reference_header,
                                 0);
      lv_obj_t *slider = lv_slider_create(p);
      lv_obj_set_pos(slider, 28, 52);
      lv_obj_set_size(slider, 260, 18);
      lv_slider_set_range(slider, 5, 100);
      lv_slider_set_value(slider, ui.brightness, LV_ANIM_OFF);
      lv_obj_set_style_bg_color(slider, accent(), LV_PART_INDICATOR);
      lv_obj_set_style_bg_color(slider, accent(), LV_PART_KNOB);
      lv_obj_add_event_cb(slider, brightness_changed, LV_EVENT_VALUE_CHANGED,
                          NULL);
      lv_obj_t *row =
          button_on(p, ui.dark ? "Тема: Тёмная  >" : "Тема: Светлая  >", 0, 89,
                    310, theme_event, 0);
      lv_obj_set_height(row, 60);
      lv_obj_set_style_bg_opa(row, 0, 0);
      lv_obj_set_style_shadow_width(row, 0, 0);
      lv_obj_t *t = lv_obj_get_child(row, 0);
      lv_obj_set_style_text_color(t, fg(), 0);
      lv_obj_set_style_text_font(t, &bk_font_reference_header, 0);
      row = button_on(p, ui.ssid[0] ? "Wi-Fi: Вкл.  >" : "Wi-Fi: Выкл.  >", 0,
                      163, 310, navigate, 6);
      lv_obj_set_height(row, 60);
      lv_obj_set_style_bg_opa(row, 0, 0);
      lv_obj_set_style_shadow_width(row, 0, 0);
      t = lv_obj_get_child(row, 0);
      lv_obj_set_style_text_color(t, fg(), 0);
      lv_obj_set_style_text_font(t, &bk_font_reference_header, 0);
      button_on(p, "Редактор экранов", 0, 250, 300, editor_list, 0);
      button_on(p, "Стиль: Reference", 0, 304, 300, style_toggle, 0);
      button_on(p, "Reference: заводской вид", 0, 358, 300, factory_reference,
                0);
      button_on(p, screen->show_title ? "Название: видно" : "Название: скрыто",
                0, 412, 300, title_toggle, 0);
    } else {
      button_on(p, ui.dark ? "Тема: тёмная" : "Тема: светлая", 0, 0, 300,
                theme_event, 0);
      button_on(p, "Редактор экранов", 0, 54, 300, editor_list, 0);
      button_on(p, "Стиль: Classic R2", 0, 108, 300, style_toggle, 0);
      button_on(p, "Reference: заводской вид", 0, 162, 300, factory_reference,
                0);
      button_on(p, screen->show_title ? "Название: видно" : "Название: скрыто",
                0, 216, 300, title_toggle, 0);
      char b[50];
      snprintf(b, sizeof(b), "Яркость %u%%", ui.brightness);
      ui.brightness_label = bk_text(p, b, 0, 279, 300);
      lv_obj_t *slider = lv_slider_create(p);
      lv_obj_set_pos(slider, 35, 330);
      lv_obj_set_size(slider, 230, 20);
      lv_slider_set_range(slider, 5, 100);
      lv_slider_set_value(slider, ui.brightness, LV_ANIM_OFF);
      lv_obj_add_event_cb(slider, brightness_changed, LV_EVENT_VALUE_CHANGED,
                          NULL);
      button_on(p, "Подключение Wi-Fi", 0, 380, 300, navigate, 6);
    }
    button("Назад", 180, 391, 120, 5);
  } else if (ui.screen == 9)
    catalog_page();
  else {
    bk_text(ui.body, "Экран отключён", 80, 200, 320);
    button("Меню", 180, 381, 120, 5);
  }
  refresh();
}
static void history_tick(lv_timer_t *timer) {
  (void)timer;
  refresh();
  for (unsigned m = 0; m < BK_METRIC_COUNT; m++)
    bk_widget_history_push(m, ui.values[m], ui.states[m]);
  if (!ui.edit_mode)
    for (unsigned n = 0; n < ui.rendered; n++)
      bk_widget_chart_refresh(&ui.widgets[n]);
}
void bk_ui_create(lv_obj_t *parent) {
  memset(&ui, 0, sizeof(ui));
  bk_layout_defaults(&ui.layout);
  bk_widget_history_reset();
  ui.brightness = 75;
  ui.screen = 1;
  ui.last_instrument = 1;
  ui.grid = true;
  snprintf(ui.clock, sizeof(ui.clock), "--:--");
  ui.connection_changed = lv_tick_get();
  ui.root = lv_obj_create(parent);
  lv_obj_set_size(ui.root, 480, 480);
  lv_obj_center(ui.root);
  lv_obj_set_scrollable(ui.root, false);
  lv_obj_set_style_pad_all(ui.root, 0, 0);
  lv_obj_set_style_border_width(ui.root, 0, 0);
  lv_obj_set_style_radius(ui.root, 240, 0);
  lv_obj_set_style_clip_corner(ui.root, true, 0);
  ui.body = lv_obj_create(ui.root);
  lv_obj_remove_style_all(ui.body);
  lv_obj_set_size(ui.body, 480, 480);
  lv_obj_set_scrollable(ui.body, false);
  lv_obj_add_event_cb(ui.body, pointer_navigation, LV_EVENT_ALL, NULL);
  ui.status = bk_text(ui.root, "", 100, 438, 280);
  ui.history_timer = lv_timer_create(history_tick, 1000, NULL);
  show();
}
void bk_ui_set_sample(bk_metric_t m, double value, bk_value_state_t state) {
  if ((unsigned)m >= BK_METRIC_COUNT)
    return;
  bk_value_state_t next = isfinite(value) ? state : BK_VALUE_UNKNOWN;
  if (ui.states[m] == next &&
      (ui.values[m] == value || (!isfinite(ui.values[m]) && !isfinite(value))))
    return;
  ui.values[m] = value;
  ui.states[m] = next;
  refresh();
}
void bk_ui_set_connected(bool connected) {
  if (ui.connected == connected)
    return;
  ui.connected = connected;
  ui.connection_changed = lv_tick_get();
  refresh();
}
void bk_ui_set_theme(bool dark) {
  ui.dark = dark;
  show();
}
void bk_ui_select_screen(unsigned n) {
  if (n >= 13)
    return;
  if (ui.edit_mode == 3)
    ui.layout.screens[ui.edit_screen] = ui.canvas_backup;
  ui.edit_mode = 0;
  ui.screen = n;
  show();
}
unsigned bk_ui_screen(void) { return ui.screen; }
void bk_ui_set_wifi(const char *ssid, const char *password, const char *url) {
  snprintf(ui.ssid, sizeof(ui.ssid), "%s", ssid ? ssid : "");
  snprintf(ui.password, sizeof(ui.password), "%s", password ? password : "");
  snprintf(ui.url, sizeof(ui.url), "%s", url ? url : "");
  if (ui.screen == 6)
    show();
}
void bk_ui_set_wifi_status(const char *status) {
  if (!strcmp(ui.wifi_status, status ? status : ""))
    return;
  snprintf(ui.wifi_status, sizeof(ui.wifi_status), "%s", status ? status : "");
  if (ui.screen == 6)
    show();
}
void bk_ui_set_dtcs(const uint8_t *codes, size_t count, bool valid) {
  if (count > DTC_LIMIT)
    count = DTC_LIMIT;
  valid = valid && (!count || codes);
  if (ui.dtcs_valid == valid && ui.dtc_count == count &&
      (!count || (codes && !memcmp(ui.dtcs, codes, count))))
    return;
  ui.dtcs_valid = valid;
  ui.dtc_count = count;
  if (codes && count)
    memcpy(ui.dtcs, codes, count);
  if (ui.screen == 7)
    show();
}
void bk_ui_set_dtc_lookup(const char *(*lookup)(uint8_t)) {
  ui.dtc_lookup = lookup;
  if (ui.screen == 7)
    show();
}
void bk_ui_set_brightness_callback(void (*cb)(uint8_t, void *), void *context) {
  ui.brightness_callback = cb;
  ui.brightness_context = context;
  if (cb)
    cb(ui.brightness, context);
}
void bk_ui_get_config(bk_ui_config_t *out) {
  if (!out)
    return;
  out->screen = ui.screen;
  out->dark = ui.dark;
  out->brightness = ui.brightness;
  for (unsigned i = 0; i < 5; i++) {
    out->selected[i] = BK_RPM;
    if (i >= ui.layout.screen_count)
      continue;
    bk_layout_screen_t *s = ui.edit_mode == 3 && i == ui.edit_screen
                                ? &ui.canvas_backup
                                : &ui.layout.screens[i];
    for (unsigned j = 0; j < s->widget_count; j++)
      if (s->widgets[j].type == BK_WIDGET_NUMBER) {
        out->selected[i] = s->widgets[j].metric;
        break;
      }
  }
}
bool bk_ui_set_config(const bk_ui_config_t *config) {
  if (!config || config->screen >= 13 || config->brightness > 100)
    return false;
  for (unsigned i = 0; i < 5; i++)
    if ((unsigned)config->selected[i] >= BK_METRIC_COUNT)
      return false;
  ui.screen = config->screen;
  ui.dark = config->dark;
  ui.brightness = config->brightness ? config->brightness : 75;
  for (unsigned i = 0; i < 5 && i < ui.layout.screen_count; i++) {
    bk_layout_screen_t *s = &ui.layout.screens[i];
    for (unsigned j = 0; j < s->widget_count; j++)
      if (s->widgets[j].type == BK_WIDGET_NUMBER ||
          s->widgets[j].type == BK_WIDGET_ARC)
        s->widgets[j].metric = config->selected[i];
  }
  if (ui.brightness_callback)
    ui.brightness_callback(ui.brightness, ui.brightness_context);
  show();
  return true;
}
void bk_ui_get_layout(bk_layout_t *out) {
  if (out) {
    *out = ui.layout;
    if (ui.edit_mode == 3)
      out->screens[ui.edit_screen] = ui.canvas_backup;
  }
}
bool bk_ui_set_layout(const bk_layout_t *layout) {
  if (!bk_layout_validate(layout))
    return false;
  ui.layout = *layout;
  if (ui.last_instrument >= ui.layout.screen_count ||
      !ui.layout.screens[ui.last_instrument].enabled) {
    for (unsigned n = 0; n < ui.layout.screen_count; n++)
      if (ui.layout.screens[n].enabled) {
        ui.last_instrument = n;
        break;
      }
  }
  if (bk_layout_view_to_screen(ui.screen) >= 0 && !instrument(ui.screen)) {
    for (unsigned n = 0; n < ui.layout.screen_count; n++)
      if (ui.layout.screens[n].enabled) {
        ui.screen = bk_layout_screen_to_view(n);
        break;
      }
  }
  ui.edit_mode = 0;
  show();
  return true;
}

void bk_ui_boot_start(unsigned next_view) {
  bk_ui_select_screen(next_view == 9
                          ? bk_layout_screen_to_view(ui.last_instrument)
                          : next_view);
}
void bk_ui_set_clock(const char *hhmm) {
  if (!hhmm || strlen(hhmm) != 5 || !strcmp(hhmm, ui.clock))
    return;
  snprintf(ui.clock, sizeof(ui.clock), "%s", hhmm);
  if (ui.clock_label)
    label(ui.clock_label, ui.clock);
}
bool bk_ui_set_background_image(uint8_t id, const void *image) {
  if (id >= 4)
    return false;
  ui.background_images[id] = image;
  show();
  return true;
}
void bk_ui_begin_edit(void) {
  if (ui.edit_mode == 3)
    return;
  int slot = bk_layout_view_to_screen(ui.screen);
  if (slot < 0)
    slot = ui.last_instrument;
  ui.edit_screen = slot;
  ui.screen = bk_layout_screen_to_view(slot);
  ui.canvas_backup = ui.layout.screens[slot];
  ui.edit_mode = 3;
  ui.drag_active = false;
  ui.nav_pressed = false;
  show();
}
void bk_ui_end_edit(bool save) {
  if (ui.edit_mode != 3)
    return;
  if (!save)
    ui.layout.screens[ui.edit_screen] = ui.canvas_backup;
  ui.edit_mode = 0;
  ui.drag_active = false;
  show();
}
void bk_ui_reset_screen(void) {
  unsigned slot = ui.edit_mode == 3 ? ui.edit_screen : ui.last_instrument;
  bk_layout_screen_t *current = &ui.layout.screens[slot];
  char id[16];
  memcpy(id, current->id, sizeof(id));
  unsigned kind = slot;
  const char *ids[] = {"main", "engine", "fuel", "sensors", "trip"};
  for (unsigned i = 0; i < 5; i++)
    if (!strcmp(current->id, ids[i]))
      kind = i;
  bk_style_t style = current->style;
  bk_layout_default_screen(current, kind, style);
  memcpy(current->id, id, sizeof(id));
  show();
}
void bk_ui_set_style(bk_style_t style) {
  if ((unsigned)style >= BK_STYLE_COUNT)
    return;
  ui.layout.screens[ui.last_instrument].style = style;
  show();
}
