/* BK Mikas 7.2: read-only, ECU-independent raw field decoders.
 * Does NOT perform K-Line transactions or ECU control.
 */
#ifndef MIKAS_TELEMETRY_H
#define MIKAS_TELEMETRY_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool mikas_le16(const uint8_t *buf, size_t len, uint16_t *out);
double mikas_rpm(uint8_t raw);          /* FREQ = raw * 40 rpm */
double mikas_voltage(uint8_t raw);      /* UACC = raw / 10 volts */
double mikas_celsius(uint8_t raw);      /* TWAT/TAIR = raw - 40 C */
double mikas_ignition_deg(uint8_t raw); /* signed(UOZ) / 2 */
double mikas_injection_ms(uint16_t raw);/* INJ = raw / 125 */
double mikas_fuel_lph(uint16_t raw);    /* JQT = raw / 10 */
bool mikas_fuel_l100(double fuel_lph, double speed_kmh, double *out);
#ifdef __cplusplus
}
#endif
#endif
