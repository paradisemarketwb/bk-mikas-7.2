#ifndef BK_WIDGETS_H
#define BK_WIDGETS_H
#include "bk_ui_config.h"
#include "lvgl.h"
LV_FONT_DECLARE(bk_font_ru_20);
LV_FONT_DECLARE(bk_font_reference_128);
LV_FONT_DECLARE(bk_font_reference_engine);
LV_FONT_DECLARE(bk_font_reference_header);
LV_FONT_DECLARE(bk_font_reference_64);
typedef struct {
  const char *id, *name;
} bk_widget_registry_entry_t;
extern const bk_widget_registry_entry_t bk_widget_registry[BK_WIDGET_COUNT];
typedef struct {
  lv_obj_t *root, *value, *unit, *needle, *chart, *backdrop;
  lv_chart_series_t *series;
  bk_widget_config_t config;
  bool dark, editing;
  bk_style_t style;
  int target;
  char tick_labels[7][20];
  const char *tick_ptrs[8];
} bk_widget_t;
lv_obj_t *bk_text(lv_obj_t *parent, const char *text, int x, int y, int width);
lv_obj_t *bk_gauge(lv_obj_t *parent, lv_color_t accent);
void bk_widget_create(bk_widget_t *widget, lv_obj_t *parent,
                      const bk_widget_config_t *config, bool dark);
void bk_widget_create_style(bk_widget_t *widget, lv_obj_t *parent,
                            const bk_widget_config_t *config, bool dark,
                            bk_style_t style);
void bk_reference_icon(lv_obj_t *parent, unsigned icon, int x, int y,
                       lv_color_t color);
void bk_widget_update(bk_widget_t *widget, double value, bk_value_state_t state,
                      bool connected, const char *name, const char *unit);
void bk_widget_history_reset(void);
void bk_widget_history_push(bk_metric_t metric, double value,
                            bk_value_state_t state);
size_t bk_widget_history_count(bk_metric_t metric);
void bk_widget_chart_refresh(bk_widget_t *widget);
#endif
