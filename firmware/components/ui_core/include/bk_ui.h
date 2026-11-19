#ifndef BK_UI_H
#define BK_UI_H
#include "lvgl.h"
#include <stdbool.h>
/* All calls must run on the LVGL task, under the display lock. Values are
 * supplied by telemetry; the UI never initiates ECU requests. */
typedef enum { BK_RPM, BK_COOLANT, BK_VOLTAGE, BK_THROTTLE, BK_FUEL_LPH,
 BK_MAF, BK_IGNITION, BK_INJECTION, BK_SPEED, BK_RANGE, BK_DISTANCE,
 BK_TRIP_SECONDS, BK_AVG_SPEED, BK_DTC_COUNT, BK_LAMBDA, BK_METRIC_COUNT } bk_metric_t;
typedef enum { BK_VALUE_UNKNOWN, BK_VALUE_VALID, BK_VALUE_STALE,
 BK_VALUE_UNSUPPORTED, BK_VALUE_INSUFFICIENT } bk_value_state_t;
typedef struct { unsigned screen; bool dark; bk_metric_t selected[5]; } bk_ui_config_t;
void bk_ui_get_config(bk_ui_config_t *out);
bool bk_ui_set_config(const bk_ui_config_t *config);
void bk_ui_create(lv_obj_t *parent);
void bk_ui_set_sample(bk_metric_t metric, double value, bk_value_state_t state);
void bk_ui_set_connected(bool connected);
void bk_ui_set_theme(bool dark);
void bk_ui_select_screen(unsigned index);
unsigned bk_ui_screen(void);
/* Supply only the current AP credentials; QR is never a hard-coded password. */
void bk_ui_set_wifi(const char *ssid, const char *password, const char *url);
#endif
