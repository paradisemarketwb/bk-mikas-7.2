#include "mikas_link.h"
#include "mikas.h"
#include <string.h>
static unsigned remaining(const mikas_stream_t *s, uint64_t end) {
  uint64_t now = s->now_ms(s->context);
  return now < end ? (unsigned)(end - now) : 0;
}
mikas_link_result_t mikas_stream_exchange(const mikas_stream_t *s,
                                          const uint8_t *cmd, size_t length,
                                          uint8_t *reply, size_t cap,
                                          size_t *out, unsigned timeout) {
  if (out)
    *out = 0;
  if (!s || !s->flush || !s->write || !s->read || !s->now_ms || !s->delay_ms ||
      !reply || !out || !timeout || !mikas_read_allowed(cmd, length))
    return MIKAS_LINK_ARGUMENT;
  uint8_t tx[MIKAS_MAX_WIRE], echo[MIKAS_MAX_WIRE], frame[MIKAS_MAX_WIRE];
  size_t n = mikas_encode(cmd, length, tx, sizeof(tx));
  if (!n)
    return MIKAS_LINK_ARGUMENT;
  s->delay_ms(s->context, 50);
  if (!s->flush(s->context))
    return MIKAS_LINK_DISCONNECTED;
  uint64_t end = s->now_ms(s->context) + timeout;
  if (!s->write(s->context, tx, n, timeout))
    return MIKAS_LINK_IO;
  size_t have = 0;
  while (have < n) {
    unsigned left = remaining(s, end);
    if (!left)
      return MIKAS_LINK_TIMEOUT;
    int count =
        s->read(s->context, echo + have, n - have, left > 10 ? 10 : left);
    if (count < 0)
      return count == -1 ? MIKAS_LINK_DISCONNECTED : MIKAS_LINK_IO;
    if ((size_t)count > n - have)
      return MIKAS_LINK_IO;
    have += (size_t)count;
  }
  if (memcmp(tx, echo, n))
    return MIKAS_LINK_ECHO;
  have = 0;
  while (have < sizeof(frame)) {
    unsigned left = remaining(s, end);
    if (!left)
      return MIKAS_LINK_TIMEOUT;
    int count = s->read(s->context, frame + have, 1, left > 10 ? 10 : left);
    if (count < 0)
      return count == -1 ? MIKAS_LINK_DISCONNECTED : MIKAS_LINK_IO;
    if (count > 1)
      return MIKAS_LINK_IO;
    if (count && frame[have++] == 0x0d)
      return mikas_decode(frame, have, reply, cap, out) ? MIKAS_LINK_OK
                                                        : MIKAS_LINK_FRAME;
  }
  return MIKAS_LINK_FRAME;
}
