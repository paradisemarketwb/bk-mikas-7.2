#ifndef BK_UI_H
#define BK_UI_H
#include "lvgl.h"
#include <stdbool.h>
/* All calls must run on the LVGL task, under the display lock. Values are
 * supplied by telemetry; the UI never initiates ECU requests. */
#include "bk_ui_config.h"
#include <stddef.h>
typedef struct { unsigned screen; bool dark; bk_metric_t selected[5]; uint8_t brightness; } bk_ui_config_t;
void bk_ui_get_config(bk_ui_config_t *out);
bool bk_ui_set_config(const bk_ui_config_t *config);
void bk_ui_create(lv_obj_t *parent);
void bk_ui_get_layout(bk_layout_t *out);
bool bk_ui_set_layout(const bk_layout_t *layout);
void bk_ui_set_dtcs(const uint8_t *codes,size_t count,bool valid);
void bk_ui_set_dtc_lookup(const char *(*lookup)(uint8_t code));
void bk_ui_set_brightness_callback(void (*callback)(uint8_t,void *),void *context);
void bk_ui_boot_start(unsigned next_view); /* 600ms nonblocking */
void bk_ui_set_wifi_status(const char *status);
void bk_ui_set_sample(bk_metric_t metric, double value, bk_value_state_t state);
void bk_ui_set_connected(bool connected);
void bk_ui_set_theme(bool dark);
void bk_ui_select_screen(unsigned index);
unsigned bk_ui_screen(void);
/* Supply only the current AP credentials; QR is never a hard-coded password. */
void bk_ui_set_wifi(const char *ssid, const char *password, const char *url);
#endif
