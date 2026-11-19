#ifndef BK_UI_CONFIG_H
#define BK_UI_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
#define BK_LAYOUT_VERSION 1
#define BK_LAYOUT_MAX_SCREENS 8
#define BK_LAYOUT_MAX_WIDGETS 8
#define BK_HISTORY_POINTS 64
typedef enum { BK_RPM, BK_COOLANT, BK_VOLTAGE, BK_THROTTLE, BK_FUEL_LPH,
 BK_MAF, BK_IGNITION, BK_INJECTION, BK_SPEED, BK_RANGE, BK_DISTANCE,
 BK_TRIP_SECONDS, BK_AVG_SPEED, BK_DTC_COUNT, BK_LAMBDA, BK_METRIC_COUNT } bk_metric_t;
typedef enum { BK_VALUE_UNKNOWN, BK_VALUE_VALID, BK_VALUE_STALE,
 BK_VALUE_UNSUPPORTED, BK_VALUE_INSUFFICIENT } bk_value_state_t;
typedef enum { BK_WIDGET_NUMBER, BK_WIDGET_DIGITAL, BK_WIDGET_ARC,
 BK_WIDGET_NEEDLE, BK_WIDGET_BAR, BK_WIDGET_TEMPERATURE, BK_WIDGET_STATUS,
 BK_WIDGET_ALARM, BK_WIDGET_CHART, BK_WIDGET_COUNT } bk_widget_type_t;
enum { BK_WARN_LOW=1, BK_WARN_HIGH=2 };
typedef struct {
 bk_widget_type_t type;
 bk_metric_t metric;
 int16_t x,y,w,h;
 float min,max,warn_low,warn_high;
 uint32_t color;
 uint8_t flags,font_size,precision;
 bool visible;
 char unit[16];
} bk_widget_config_t;
typedef struct {
 char id[16],title[64];
 uint32_t background;
 bool enabled;
 uint8_t widget_count;
 bk_widget_config_t widgets[BK_LAYOUT_MAX_WIDGETS];
} bk_layout_screen_t;
typedef struct {
 uint16_t version;
 uint8_t screen_count;
 bk_layout_screen_t screens[BK_LAYOUT_MAX_SCREENS];
} bk_layout_t;
void bk_layout_defaults(bk_layout_t *out);
bool bk_layout_validate(const bk_layout_t *layout);
/* View IDs0..4 instruments,5menu,6WiFi,7DTC,8settings,9boot,
 * IDs10..12 additional instruments5..7. */
int bk_layout_view_to_screen(unsigned view);
unsigned bk_layout_screen_to_view(unsigned screen);
#endif
