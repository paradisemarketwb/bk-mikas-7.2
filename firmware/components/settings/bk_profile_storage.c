#include "bk_settings.h"
#include <stdlib.h>
#include <string.h>
/* Frozen version-3 ESP32/host ABI. Never replace these with current types. */
typedef struct {
  bk_widget_type_t type;
  bk_metric_t metric;
  int16_t x, y, w, h;
  float min, max, warn_low, warn_high;
  uint32_t color;
  uint8_t flags, font_size, precision;
  uint8_t visible;
  char unit[16];
} legacy_widget_t;
typedef struct {
  char id[16], title[64];
  uint32_t background;
  uint8_t enabled;
  uint8_t widget_count;
  legacy_widget_t widgets[8];
} legacy_screen_t;
typedef struct {
  uint16_t version;
  uint8_t screen_count;
  legacy_screen_t screens[8];
} legacy_layout_t;
typedef struct {
  uint32_t version;
  uint8_t screen, brightness, dark, reserved;
  uint8_t selected[5], padding[3];
  legacy_layout_t layout;
} legacy_profile_t;
_Static_assert(sizeof(legacy_widget_t) == 56, "legacy v3 widget ABI");
_Static_assert(sizeof(legacy_profile_t) == 4308, "legacy v3 profile ABI");
bool bk_settings_migrate_v3(const void *blob, size_t size, bk_settings_t *out) {
  if (!blob || !out || size != sizeof(legacy_profile_t))
    return false;
  legacy_profile_t *old = malloc(sizeof(*old));
  bk_settings_t *s = malloc(sizeof(*s));
  if (!old || !s) {
    free(old);
    free(s);
    return false;
  }
  memcpy(old, blob, size);
  bool ok = false;
  if (old->version != 3 || old->layout.version != 1 ||
      old->layout.screen_count < 1 || old->layout.screen_count > 8)
    goto done;
  bk_settings_defaults(s);
  s->screen = old->screen;
  s->brightness = old->brightness;
  s->dark = old->dark;
  memcpy(s->selected, old->selected, 5);
  s->layout.screen_count = old->layout.screen_count;
  for (unsigned i = 0; i < s->layout.screen_count; i++) {
    const legacy_screen_t *a = &old->layout.screens[i];
    bk_layout_screen_t *b = &s->layout.screens[i];
    bk_layout_default_screen(b, i, BK_STYLE_CLASSIC);
    memcpy(b->id, a->id, sizeof(b->id));
    memcpy(b->title, a->title, sizeof(b->title));
    b->background = a->background;
    b->enabled = a->enabled;
    b->widget_count = a->widget_count;
    if (a->enabled > 1 || b->widget_count > 8)
      goto done;
    b->sequence_count = 0;
    b->sequence_index = 0;
    for (unsigned j = 0; j < b->widget_count; j++) {
      const legacy_widget_t *w = &a->widgets[j];
      bk_widget_config_t *v = &b->widgets[j];
      if (w->visible > 1)
        goto done;
      memset(v, 0, sizeof(*v));
      v->type = w->type;
      v->metric = w->metric;
      v->x = w->x;
      v->y = w->y;
      v->w = w->w;
      v->h = w->h;
      v->min = w->min;
      v->max = w->max;
      v->warn_low = w->warn_low;
      v->warn_high = w->warn_high;
      v->color = w->color;
      v->flags = w->flags;
      v->font_size = w->font_size;
      v->precision = w->precision;
      v->visible = w->visible;
      v->primary = (j == 0);
      memcpy(v->unit, w->unit, sizeof(v->unit));
    }
  }
  for (unsigned i = 0; i < s->layout.screen_count; i++)
    bk_layout_seed_legacy_sequence(&s->layout.screens[i]);
  if (s->screen == 9) {
    s->screen = 0;
    for (unsigned i = 0; i < s->layout.screen_count; i++)
      if (s->layout.screens[i].enabled) {
        s->screen = bk_layout_screen_to_view(i);
        break;
      }
  }
  if (!bk_settings_valid(s))
    goto done;
  *out = *s;
  ok = true;
done:
  free(old);
  free(s);
  return ok;
}
/* Zero-run packing bounds the NVS payload; whole blob is still replaced
 * atomically. Envelope version + exact decoded length + CRC detect
 * torn/corrupt/incompatible data. Existing partition table and Wi-Fi/journal
 * namespaces remain untouched. */
static uint32_t crc32(const unsigned char *p, size_t n) {
  uint32_t c = ~0u;
  while (n--) {
    c ^= *p++;
    for (unsigned b = 0; b < 8; b++)
      c = (c >> 1) ^ ((0u - (c & 1u)) & 0xedb88320u);
  }
  return ~c;
}
static void put32(unsigned char *p, uint32_t v) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (unsigned char)(v >> (8 * i));
}
static uint32_t get32(const unsigned char *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
size_t bk_settings_encode(const bk_settings_t *s, void *buffer, size_t cap) {
  if (!bk_settings_valid(s) || !buffer || cap < 12)
    return 0;
  unsigned char *d = buffer;
  const unsigned char *p = (const unsigned char *)s;
  memcpy(d, "BK4R", 4);
  put32(d + 4, sizeof(*s));
  put32(d + 8, crc32(p, sizeof(*s)));
  size_t at = 12, i = 0;
  while (i < sizeof(*s)) {
    bool zero = p[i] == 0;
    size_t n = 1;
    while (n < 127 && i + n < sizeof(*s) && (p[i + n] == 0) == zero)
      n++;
    if (at + 1 + (zero ? 0 : n) > cap)
      return 0;
    d[at++] = (unsigned char)(n | (zero ? 0 : 128));
    if (!zero) {
      memcpy(d + at, p + i, n);
      at += n;
    }
    i += n;
  }
  return at;
}
bool bk_settings_decode(const void *buffer, size_t size, bk_settings_t *out) {
  if (!buffer || !out)
    return false;
  const unsigned char *p = buffer;
  if (size < 12 || memcmp(p, "BK4R", 4))
    return bk_settings_migrate_v3(buffer, size, out);
  if (get32(p + 4) != sizeof(*out))
    return false;
  bk_settings_t *s = calloc(1, sizeof(*s));
  if (!s)
    return false;
  unsigned char *d = (unsigned char *)s;
  size_t i = 12, at = 0;
  bool ok = false;
  while (i < size) {
    unsigned c = p[i++], n = c & 127u;
    if (!n || at + n > sizeof(*s))
      goto done;
    if (c & 128u) {
      if (i + n > size)
        goto done;
      memcpy(d + at, p + i, n);
      i += n;
    }
    at += n;
  }
  if (at != sizeof(*s) || crc32(d, sizeof(*s)) != get32(p + 8) ||
      !bk_settings_valid(s))
    goto done;
  *out = *s;
  ok = true;
done:
  free(s);
  return ok;
}
