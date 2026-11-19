#include "bk_byte_ring.h"
#include "mikas.h"
#include "mikas_link.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct {
  uint64_t now;
  uint8_t in[2048];
  size_t used, pos, chunk;
  int disconnect;
  bool flushed, fail_write;
  unsigned writes;
} fake_t;
static uint64_t now(void *c) { return ((fake_t *)c)->now; }
static void delay(void *c, unsigned ms) { ((fake_t *)c)->now += ms; }
static bool flush(void *c) {
  ((fake_t *)c)->flushed = true;
  return true;
}
static bool write_bytes(void *c, const uint8_t *data, size_t n,
                        unsigned timeout) {
  fake_t *f = c;
  (void)timeout;
  f->writes++;
  assert(f->flushed);
  assert(n == 4 && !memcmp(data, (uint8_t[]){0x61, 0x29, 0x76, 0x0d}, 4));
  return !f->fail_write;
}
static int read_bytes(void *c, uint8_t *out, size_t capacity,
                      unsigned timeout) {
  fake_t *f = c;
  f->now += timeout;
  if (f->disconnect >= 0 && (int)f->pos >= f->disconnect)
    return -1;
  size_t n = f->used - f->pos;
  if (n > capacity)
    n = capacity;
  if (n > f->chunk)
    n = f->chunk;
  memcpy(out, f->in + f->pos, n);
  f->pos += n;
  return (int)n;
}
static void seed(fake_t *f) {
  memset(f, 0, sizeof(*f));
  f->chunk = 2;
  f->disconnect = -1;
  uint8_t request[] = {0x61, 0x29}, reply[] = {20};
  f->used = mikas_encode(request, 2, f->in, sizeof(f->in));
  f->used += mikas_encode(reply, 1, f->in + f->used, sizeof(f->in) - f->used);
}
int main(void) {
  fake_t f;
  seed(&f);
  mikas_stream_t stream = {&f, flush, write_bytes, read_bytes, now, delay};
  uint8_t out[256];
  size_t n;
  uint8_t command[] = {0x61, 0x29};
  assert(mikas_stream_exchange(&stream, command, 2, out, sizeof(out), &n,
                               100) == MIKAS_LINK_OK &&
         n == 1 && out[0] == 20);
  seed(&f);
  f.chunk = 1;
  assert(mikas_stream_exchange(&stream, command, 2, out, sizeof(out), &n,
                               100) == MIKAS_LINK_OK);
  seed(&f);
  f.in[0] ^= 1;
  assert(mikas_stream_exchange(&stream, command, 2, out, sizeof(out), &n,
                               100) == MIKAS_LINK_ECHO &&
         n == 0);
  seed(&f);
  f.in[f.used - 2] ^= 1;
  assert(mikas_stream_exchange(&stream, command, 2, out, sizeof(out), &n,
                               100) == MIKAS_LINK_FRAME &&
         n == 0);
  seed(&f);
  f.disconnect = 2;
  assert(mikas_stream_exchange(&stream, command, 2, out, sizeof(out), &n,
                               100) == MIKAS_LINK_DISCONNECTED);
  seed(&f);
  f.used = 0;
  assert(mikas_stream_exchange(&stream, command, 2, out, sizeof(out), &n, 35) ==
         MIKAS_LINK_TIMEOUT);
  seed(&f);
  f.fail_write = true;
  assert(mikas_stream_exchange(&stream, command, 2, out, sizeof(out), &n,
                               100) == MIKAS_LINK_IO);
  seed(&f);
  assert(mikas_stream_exchange(&stream, (uint8_t[]){0x62, 0x0e, 8}, 3, out,
                               sizeof(out), &n, 100) == MIKAS_LINK_ARGUMENT &&
         f.writes == 0 && !f.flushed);
  bk_byte_ring_t ring = {0};
  uint8_t a[BK_RX_CAPACITY], b[BK_RX_CAPACITY];
  for (unsigned i = 0; i < sizeof(a); i++)
    a[i] = (uint8_t)i;
  assert(bk_ring_push(&ring, a, sizeof(a)));
  size_t head = ring.head;
  assert(!bk_ring_push(&ring, a, 1) && head == ring.head &&
         ring.count == sizeof(a));
  assert(bk_ring_read(&ring, b, 17) == 17 && !memcmp(a, b, 17));
  assert(bk_ring_push(&ring, a, 17));
  assert(bk_ring_read(&ring, b, sizeof(b)) == sizeof(b));
  assert(!memcmp(b, a + 17, sizeof(a) - 17) &&
         !memcmp(b + sizeof(a) - 17, a, 17));
  assert(ring.count == 0);
  bk_ring_clear(&ring);
  puts("mikas stream: fragmented echo/reply, disconnect, timeout, corrupt "
       "frames, read-only guard; USB RX ring wrap/overflow: PASS");
}
