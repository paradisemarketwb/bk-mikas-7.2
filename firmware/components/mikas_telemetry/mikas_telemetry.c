#include "mikas_telemetry.h"
#include <math.h>
bool mikas_le16(const uint8_t *buf, size_t len, uint16_t *out) {
    if (!buf || !out || len < 2) return false;
    *out=(uint16_t)buf[0] | ((uint16_t)buf[1]<<8);
    return true;
}
double mikas_rpm(uint8_t raw) {return (double)raw*40.0;}
double mikas_voltage(uint8_t raw) {return (double)raw/10.0;}
double mikas_celsius(uint8_t raw) {return (double)raw-40.0;}
double mikas_ignition_deg(uint8_t raw) {return (double)(int8_t)raw/2.0;}
double mikas_injection_ms(uint16_t raw) {return (double)raw/125.0;}
double mikas_fuel_lph(uint16_t raw) {return (double)raw/10.0;}
bool mikas_fuel_l100(double fuel_lph, double speed_kmh, double *out) {
    if (!out || !isfinite(fuel_lph) || !isfinite(speed_kmh) ||
        fuel_lph < 0.0 || speed_kmh < 3.0) return false;
    *out=100.0*fuel_lph/speed_kmh;
    return isfinite(*out);
}
