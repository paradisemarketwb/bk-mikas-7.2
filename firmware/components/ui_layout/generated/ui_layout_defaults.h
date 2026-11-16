/* GENERATED FROM ui/screens.json BY tools/generate_layout.py
   Data only; generic ESP32-S3 UI renderer reads these descriptors. */
#ifndef BK_UI_LAYOUT_DEFAULTS_H
#define BK_UI_LAYOUT_DEFAULTS_H
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    const char *id, *title, *layout, *accent;
    const char *main_metric, *left_metric, *right_metric, *caption;
    bool enabled;
} bk_screen_desc_t;
static const bk_screen_desc_t bk_default_screens[] = {
    {"coolant", "ДВИГАТЕЛЬ", "arc3", "#76d7b1", "coolant_c", "ecu_voltage_v", "rpm", "Температура охлаждающей жидкости", true},
    {"tachometer", "ТАХОМЕТР", "arc3", "#64c9f6", "rpm", "ignition_deg", "injection_ms", "Обороты коленвала", true},
    {"trip", "ПОЕЗДКА", "arc3", "#eec078", "fuel_lph", "rpm", "ecu_voltage_v", "Расход в л/ч; скорость при наличии источника", true},
    {"sensors", "ДАТЧИКИ", "arc3", "#ba9df5", "air_mass_kgh", "throttle_pct", "injection_ms", "Расход воздуха требует сверки с OpenDiag", true},
    {"diagnostics", "ДИАГНОСТИКА", "arc3", "#83d6b4", "dtc_count", "ecu_voltage_v", "rpm", "Только просмотр ошибок", true},
};
static const size_t bk_default_screens_len =
    sizeof(bk_default_screens)/sizeof(bk_default_screens[0]);
#define BK_DEFAULT_START_SCREEN "coolant"
#endif
