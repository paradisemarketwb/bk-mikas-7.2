#ifndef BK_WIDGETS_H
#define BK_WIDGETS_H
#include "lvgl.h"
LV_FONT_DECLARE(bk_font_ru_20);
lv_obj_t *bk_text(lv_obj_t *parent, const char *text, int x, int y, int width);
lv_obj_t *bk_gauge(lv_obj_t *parent, lv_color_t accent);
#endif
