#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { TEL_RPM,TEL_COOLANT,TEL_VOLTS,TEL_THROTTLE,TEL_INTAKE,TEL_ADVANCE,TEL_AIR,TEL_FUEL,TEL_INJECTION,TEL_LAMBDA1,TEL_LAMBDA2,TEL_METRIC_COUNT } bk_tel_metric_t;
typedef enum { TEL_UNAVAILABLE,TEL_VALID,TEL_STALE,TEL_NO_ECU,TEL_UNSUPPORTED,TEL_NEEDS_DATA } bk_tel_state_t;
typedef struct {double value;uint64_t at_ms;bk_tel_state_t state;} bk_sample_t;
typedef struct {bk_sample_t samples[TEL_METRIC_COUNT];bool connected;uint8_t faults[256];unsigned fault_count;uint64_t faults_at_ms;bool faults_valid;} bk_snapshot_t;
typedef struct {uint8_t pid,width;double scale,offset;bool is_signed;uint32_t period_ms;} bk_field_t;
extern const bk_field_t bk_fields[TEL_METRIC_COUNT];
void bk_store_init(bk_snapshot_t *s);
bool bk_store_reply(bk_snapshot_t *s,bk_tel_metric_t metric,const uint8_t *body,unsigned len,uint64_t now);
bk_sample_t bk_store_get(const bk_snapshot_t *s,bk_tel_metric_t metric,uint64_t now);
