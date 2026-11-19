#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BK_RX_CAPACITY 2048
/* Caller supplies synchronization. A rejected push leaves the ring unchanged.
 */
typedef struct {
  uint8_t data[BK_RX_CAPACITY];
  size_t head, tail, count;
} bk_byte_ring_t;
static inline void bk_ring_clear(bk_byte_ring_t *r) {
  r->head = r->tail = r->count = 0;
}
static inline bool bk_ring_push(bk_byte_ring_t *r, const uint8_t *data,
                                size_t n) {
  if (n > BK_RX_CAPACITY - r->count)
    return false;
  for (size_t i = 0; i < n; i++) {
    r->data[r->head] = data[i];
    r->head = (r->head + 1) % BK_RX_CAPACITY;
  }
  r->count += n;
  return true;
}
static inline size_t bk_ring_read(bk_byte_ring_t *r, uint8_t *out, size_t n) {
  if (n > r->count)
    n = r->count;
  for (size_t i = 0; i < n; i++) {
    out[i] = r->data[r->tail];
    r->tail = (r->tail + 1) % BK_RX_CAPACITY;
  }
  r->count -= n;
  return n;
}
