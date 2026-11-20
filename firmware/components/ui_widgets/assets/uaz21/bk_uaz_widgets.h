#pragma once
#include "bk_widgets.h"
bool bk_uaz_widget_create(bk_widget_t *w, lv_obj_t *parent,
                          const bk_widget_config_t *c, bool dark);
bool bk_uaz_widget_update(bk_widget_t *w, double value, bool valid,
                          const char *name, const char *unit,
                          const char *missing, lv_color_t ink,
                          lv_color_t accent);
