#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BK_CALIBRATION_POINTS 8
#define BK_PROFILE_NAME_SIZE 32
#define BK_SOURCE_CAPACITY 64
#define BK_METRIC_ID_SIZE 48
#define BK_ACTIVITY_POINTS 32

typedef enum {
  BK_UNIT_RAW,
  BK_UNIT_G_S,
  BK_UNIT_KG_H,
  BK_UNIT_V,
  BK_UNIT_MV,
  BK_UNIT_LAMBDA_RATIO,
  BK_UNIT_AFR,
  BK_UNIT_NATIVE
} bk_unit_t;
typedef enum { BK_CAL_MAF, BK_CAL_LAMBDA } bk_calibration_kind_t;
typedef enum {
  BK_CAL_RAW,
  BK_CAL_LINEAR,
  BK_CAL_TABLE
} bk_calibration_method_t;
typedef enum {
  BK_LAMBDA_UNSPECIFIED,
  BK_LAMBDA_NARROWBAND,
  BK_LAMBDA_WIDEBAND
} bk_lambda_kind_t;
typedef struct {
  double raw, value;
} bk_calibration_point_t;
typedef struct {
  bk_calibration_kind_t kind;
  bool configured;
  bk_calibration_method_t method;
  bk_unit_t unit;
  bk_lambda_kind_t lambda_kind;
  char profile_name[BK_PROFILE_NAME_SIZE];
  double k, divisor, b, raw_min, raw_max, output_min, output_max;
  unsigned point_count;
  bk_calibration_point_t points[BK_CALIBRATION_POINTS];
} bk_calibration_t;
typedef enum {
  BK_CAL_OK,
  BK_CAL_NEEDS_CONFIG,
  BK_CAL_INVALID_CONFIG,
  BK_CAL_RAW_OUT_OF_RANGE,
  BK_CAL_OUTPUT_OUT_OF_RANGE,
  BK_CAL_INVALID_UNIT,
  BK_CAL_NONFINITE
} bk_calibration_result_t;
void bk_calibration_defaults(bk_calibration_t *c, bk_calibration_kind_t kind);
bool bk_calibration_validate(const bk_calibration_t *c, char *reason,
                             size_t cap);
bk_calibration_result_t bk_calibration_evaluate(const bk_calibration_t *c,
                                                double raw,
                                                bk_unit_t target_unit,
                                                double *out);
const char *bk_unit_name(bk_unit_t unit);

/* Adapters identify actual sources; registering one is not a validity claim. */
typedef enum {
  BK_SOURCE_NONE,
  BK_SOURCE_ECU,
  BK_SOURCE_GPS,
  BK_SOURCE_SPEED_SENSOR,
  BK_SOURCE_FUEL_SENSOR,
  BK_SOURCE_OUTSIDE_TEMP,
  BK_SOURCE_OIL_PRESSURE,
  BK_SOURCE_OIL_TEMP,
  BK_SOURCE_TPMS,
  BK_SOURCE_PARKING,
  BK_SOURCE_ANALOG,
  BK_SOURCE_DIGITAL,
  BK_SOURCE_FUEL_MODE,
  BK_SOURCE_DERIVED,
  BK_SOURCE_USER,
  BK_SOURCE_MODULE,
  BK_SOURCE_SIMULATION,
  BK_SOURCE_COUNT
} bk_source_id_t;
typedef enum {
  BK_SOURCE_NO_SIGNAL,
  BK_SOURCE_VALID,
  BK_SOURCE_STALE,
  BK_SOURCE_UNCONFIRMED,
  BK_SOURCE_OUT_OF_RANGE,
  BK_SOURCE_INVALID,
  BK_SOURCE_SIMULATION_BLOCKED
} bk_source_status_t;
typedef struct {
  double value, raw;
  bool raw_valid, confirmed, simulation, user_calibration;
  uint64_t at_ms;
  bk_unit_t unit;
  bk_source_id_t source;
  char profile_name[BK_PROFILE_NAME_SIZE];
} bk_source_sample_t;
typedef struct {
  char metric[BK_METRIC_ID_SIZE];
  bk_source_id_t source;
  int priority; /* higher number wins, source ID breaks ties */
  uint32_t fresh_ms;
  unsigned confirm_samples;
  double minimum, maximum;
  bk_unit_t unit;
} bk_source_config_t;
typedef struct {
  bk_source_config_t config;
  bk_source_sample_t sample;
  bk_source_status_t status;
  unsigned confirmations;
  bool used, published, selected;
} bk_source_entry_t;
typedef struct {
  bk_source_entry_t entries[BK_SOURCE_CAPACITY];
} bk_sources_t;
typedef struct {
  bk_source_id_t preferred; /* NONE = automatic initial selection */
  bool auto_fallback;       /* explicitly allow switching to another source */
  bool allow_simulation;    /* false for live operation */
} bk_source_policy_t;
typedef struct {
  bk_source_sample_t sample;
  bk_source_status_t status;
  const char *reason;
} bk_source_selection_t;
void bk_sources_init(bk_sources_t *registry);
bool bk_sources_register(bk_sources_t *registry,
                         const bk_source_config_t *config);
bool bk_sources_remove(bk_sources_t *registry, const char *metric,
                       bk_source_id_t source);
bool bk_sources_publish(bk_sources_t *registry, const char *metric,
                        const bk_source_sample_t *sample);
bk_source_selection_t bk_sources_select(bk_sources_t *registry,
                                        const char *metric,
                                        const bk_source_policy_t *policy,
                                        uint64_t now_ms);
const char *bk_source_name(bk_source_id_t source);
const char *bk_source_status_reason(bk_source_status_t state);
/* Explicit adapter for simulator samples: never impersonates ECU or a sensor.
 */
bool bk_sources_publish_simulation(bk_sources_t *registry, const char *metric,
                                   double value, bk_unit_t unit,
                                   uint64_t now_ms);

/* No smoothing: statistics use the original timestamped points in the window.
 */
typedef struct {
  double value;
  uint64_t at_ms;
} bk_signal_point_t;
typedef struct {
  bk_signal_point_t points[BK_ACTIVITY_POINTS];
  unsigned count, next;
} bk_signal_activity_t;
typedef enum {
  BK_ACTIVITY_NO_SIGNAL,
  BK_ACTIVITY_STATIC,
  BK_ACTIVITY_CHANGING,
  BK_ACTIVITY_STALE
} bk_activity_state_t;
typedef struct {
  bk_activity_state_t state;
  unsigned samples, changes, crossings;
  double minimum, maximum;
  uint64_t last_ms;
} bk_activity_result_t;
void bk_signal_activity_init(bk_signal_activity_t *activity);
bool bk_signal_activity_update(bk_signal_activity_t *activity, double value,
                               uint64_t at_ms);
bk_activity_result_t
bk_signal_activity_get(const bk_signal_activity_t *activity, uint64_t now_ms,
                       uint32_t window_ms, double epsilon,
                       double crossing_level);

typedef enum {
  BK_SECTION_DAILY,
  BK_SECTION_DIAGNOSTICS,
  BK_SECTION_SERVICE,
  BK_SECTION_SETTINGS
} bk_function_section_t;
typedef enum {
  BK_FUNCTION_AVAILABLE,
  BK_FUNCTION_NEEDS_CALIBRATION,
  BK_FUNCTION_NEEDS_SENSOR,
  BK_FUNCTION_NO_SIGNAL,
  BK_FUNCTION_NEEDS_PROTOCOL,
  BK_FUNCTION_PLANNED,
  BK_FUNCTION_UNSUPPORTED,
  BK_FUNCTION_DISABLED
} bk_function_status_t;
typedef struct {
  const char *id, *name, *unit, *sources, *reason;
  bk_function_section_t section;
  bk_function_status_t status;
} bk_function_t;
const bk_function_t *bk_function_catalog(size_t *count);
const bk_function_t *bk_function_find(const char *id);
const char *bk_function_status_name(bk_function_status_t status);
