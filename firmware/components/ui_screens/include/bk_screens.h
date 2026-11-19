#ifndef BK_SCREENS_H
#define BK_SCREENS_H
#include "bk_ui.h"
typedef struct { const char *title; bk_metric_t primary, left, right; } bk_screen_t;
extern const bk_screen_t bk_screens[5];
extern const char *const bk_metric_names[BK_METRIC_COUNT];
extern const char *const bk_metric_units[BK_METRIC_COUNT];
extern const double bk_metric_max[BK_METRIC_COUNT];
#endif
