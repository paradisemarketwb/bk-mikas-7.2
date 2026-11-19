#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MIKAS_MAX_PAYLOAD 256
#define MIKAS_MAX_WIRE (2*(MIKAS_MAX_PAYLOAD+1)+1)
/* Own monitor 0B protocol, 9600 8N1, NOT OBD-II. */
size_t mikas_encode(const uint8_t *body, size_t len, uint8_t *wire, size_t cap);
bool mikas_decode(const uint8_t *wire, size_t len, uint8_t *body, size_t cap, size_t *out_len);
bool mikas_read_allowed(const uint8_t *body, size_t len);
bool mikas_faults_valid(const uint8_t *body, size_t len);
