#include "bk_background.h"
#include <string.h>

#define BG_MAGIC 0x47424b42u /* bytes BKBG */
#define BG_VERSION 1u
static uint16_t get16(const uint8_t *p) {
  return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}
static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 |
         (uint32_t)p[3] << 24;
}
static void put16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}
static void put32(uint8_t *p, uint32_t v) {
  for (unsigned i = 0; i < 4; i++)
    p[i] = (uint8_t)(v >> (i * 8));
}
static uint32_t crc_update(uint32_t crc, const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++) {
    crc ^= p[i];
    for (unsigned j = 0; j < 8; j++)
      crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
  }
  return crc;
}
uint32_t bk_background_crc32(const void *data, size_t n) {
  if (!data && n)
    return 0;
  return crc_update(0xffffffffu, data, n) ^ 0xffffffffu;
}
static bool ready(const bk_background_t *s) {
  return s && s->io.read && s->io.write && s->io.erase &&
         s->io.size >= BK_BACKGROUND_PARTITION_BYTES;
}
static size_t offset(unsigned slot, unsigned bank) {
  return ((size_t)slot * BK_BACKGROUND_BANKS + bank) * BK_BACKGROUND_BANK_BYTES;
}
static bool dimensions(uint16_t w, uint16_t h, size_t n) {
  return w && h && w <= BK_BACKGROUND_MAX_WIDTH &&
         h <= BK_BACKGROUND_MAX_HEIGHT && n == (size_t)w * h * 2;
}
bk_background_status_t bk_background_init(bk_background_t *s,
                                          const bk_background_io_t *io) {
  if (!s)
    return BK_BACKGROUND_INVALID_ARGUMENT;
  memset(s, 0, sizeof(*s));
  if (!io || !io->read || !io->write || !io->erase ||
      io->size < BK_BACKGROUND_PARTITION_BYTES)
    return BK_BACKGROUND_INVALID_ARGUMENT;
  s->io = *io;
  return BK_BACKGROUND_OK;
}
static bk_background_status_t bank_info(bk_background_t *s, unsigned slot,
                                        unsigned bank,
                                        bk_background_info_t *info) {
  uint8_t header[BK_BACKGROUND_HEADER_BYTES];
  size_t base = offset(slot, bank);
  if (!s->io.read(s->io.context, base, header, sizeof(header)))
    return BK_BACKGROUND_IO_ERROR;
  if (get32(header) != BG_MAGIC || get16(header + 4) != BG_VERSION ||
      get16(header + 6) != BK_BACKGROUND_HEADER_BYTES ||
      get32(header + 12) != slot ||
      get32(header + 20) != BK_BACKGROUND_FORMAT_RGB565_LE)
    return BK_BACKGROUND_NO_ASSET;
  uint16_t w = get16(header + 16), h = get16(header + 18);
  uint32_t n = get32(header + 24);
  if (!dimensions(w, h, n) ||
      n > BK_BACKGROUND_BANK_BYTES - BK_BACKGROUND_HEADER_BYTES)
    return BK_BACKGROUND_NO_ASSET;
  for (unsigned i = 36; i < BK_BACKGROUND_HEADER_BYTES; i++)
    if (header[i] != 0)
      return BK_BACKGROUND_NO_ASSET;
  uint32_t expected_header = get32(header + 32);
  put32(header + 32, 0);
  if (bk_background_crc32(header, sizeof(header)) != expected_header)
    return BK_BACKGROUND_NO_ASSET;
  uint32_t expected = get32(header + 28), crc = 0xffffffffu;
  uint8_t chunk[512];
  for (size_t done = 0; done < n;) {
    size_t take = n - done;
    if (take > sizeof(chunk))
      take = sizeof(chunk);
    if (!s->io.read(s->io.context, base + BK_BACKGROUND_HEADER_BYTES + done,
                    chunk, take))
      return BK_BACKGROUND_IO_ERROR;
    crc = crc_update(crc, chunk, take);
    done += take;
  }
  if ((crc ^ 0xffffffffu) != expected)
    return BK_BACKGROUND_NO_ASSET;
  *info = (bk_background_info_t){.available = true,
                                 .slot = slot,
                                 .bank = bank,
                                 .width = w,
                                 .height = h,
                                 .generation = get32(header + 8),
                                 .format = BK_BACKGROUND_FORMAT_RGB565_LE,
                                 .bytes = n,
                                 .crc32 = expected};
  return BK_BACKGROUND_OK;
}
static bool newer(uint32_t a, uint32_t b) {
  return a != b && (uint32_t)(a - b) < 0x80000000u;
}
static bk_background_status_t newest(bk_background_t *s, unsigned slot,
                                     bk_background_info_t *info) {
  bk_background_info_t a = {0}, b = {0};
  bk_background_status_t sa = bank_info(s, slot, 0, &a),
                         sb = bank_info(s, slot, 1, &b);
  if (sa == BK_BACKGROUND_IO_ERROR || sb == BK_BACKGROUND_IO_ERROR)
    return BK_BACKGROUND_IO_ERROR;
  if (sa != BK_BACKGROUND_OK && sb != BK_BACKGROUND_OK)
    return BK_BACKGROUND_NO_ASSET;
  *info = sa == BK_BACKGROUND_OK &&
                  (sb != BK_BACKGROUND_OK || !newer(b.generation, a.generation))
              ? a
              : b;
  return BK_BACKGROUND_OK;
}
bk_background_status_t bk_background_load(bk_background_t *s, unsigned slot,
                                          bk_background_info_t *info,
                                          void *pixels, size_t cap) {
  if (!info)
    return BK_BACKGROUND_INVALID_ARGUMENT;
  memset(info, 0, sizeof(*info));
  info->slot = slot;
  if (!s || slot >= BK_BACKGROUND_SLOTS)
    return BK_BACKGROUND_INVALID_ARGUMENT;
  if (!ready(s))
    return BK_BACKGROUND_NO_PARTITION;
  bk_background_status_t st = newest(s, slot, info);
  if (st != BK_BACKGROUND_OK)
    return st;
  if (!pixels)
    return BK_BACKGROUND_OK;
  if (cap < info->bytes)
    return BK_BACKGROUND_BUFFER_TOO_SMALL;
  if (!s->io.read(s->io.context,
                  offset(slot, info->bank) + BK_BACKGROUND_HEADER_BYTES, pixels,
                  info->bytes) ||
      bk_background_crc32(pixels, info->bytes) != info->crc32) {
    info->available = false;
    return BK_BACKGROUND_IO_ERROR;
  }
  return BK_BACKGROUND_OK;
}
bk_background_status_t bk_background_slot_info(bk_background_t *s,
                                               unsigned slot,
                                               bk_background_info_t *info) {
  return bk_background_load(s, slot, info, NULL, 0);
}
bk_background_status_t bk_background_store(bk_background_t *s, unsigned slot,
                                           uint16_t w, uint16_t h,
                                           const void *pixels, size_t n) {
  if (!s || slot >= BK_BACKGROUND_SLOTS || !pixels || !dimensions(w, h, n))
    return BK_BACKGROUND_INVALID_ARGUMENT;
  if (!ready(s))
    return BK_BACKGROUND_NO_PARTITION;
  bk_background_info_t old = {0};
  bk_background_status_t st = newest(s, slot, &old);
  if (st != BK_BACKGROUND_OK && st != BK_BACKGROUND_NO_ASSET)
    return st;
  unsigned bank = old.available ? 1u - old.bank : 0;
  uint32_t generation = old.available ? old.generation + 1u : 1u;
  size_t base = offset(slot, bank);
  if (!s->io.erase(s->io.context, base, BK_BACKGROUND_BANK_BYTES))
    return BK_BACKGROUND_IO_ERROR;
  /* All write offsets/sizes are at least16-byte aligned; payload length is
   * exact in header. */
  const uint8_t *input = pixels;
  uint32_t scratch_words[128];
  uint8_t *scratch = (uint8_t *)scratch_words;
  for (size_t done = 0; done < n;) {
    size_t take = n - done;
    if (take > sizeof(scratch_words))
      take = sizeof(scratch_words);
    size_t padded = (take + 15u) & ~(size_t)15u;
    memset(scratch, 0xff, padded);
    memcpy(scratch, input + done, take);
    if (!s->io.write(s->io.context, base + BK_BACKGROUND_HEADER_BYTES + done,
                     scratch, padded))
      return BK_BACKGROUND_IO_ERROR;
    done += take;
  }
  uint32_t header_words[BK_BACKGROUND_HEADER_BYTES / 4] = {0};
  uint8_t *header = (uint8_t *)header_words;
  put32(header, BG_MAGIC);
  put16(header + 4, BG_VERSION);
  put16(header + 6, BK_BACKGROUND_HEADER_BYTES);
  put32(header + 8, generation);
  put32(header + 12, slot);
  put16(header + 16, w);
  put16(header + 18, h);
  put32(header + 20, BK_BACKGROUND_FORMAT_RGB565_LE);
  put32(header + 24, (uint32_t)n);
  put32(header + 28, bk_background_crc32(pixels, n));
  put32(header + 32, bk_background_crc32(header, BK_BACKGROUND_HEADER_BYTES));
  /* No header is committed until every pixel write succeeded. The active bank
   * is never erased. */
  if (!s->io.write(s->io.context, base, header, BK_BACKGROUND_HEADER_BYTES))
    return BK_BACKGROUND_IO_ERROR;
  bk_background_info_t check = {0};
  st = bank_info(s, slot, bank, &check);
  return st == BK_BACKGROUND_OK && check.generation == generation
             ? BK_BACKGROUND_OK
             : BK_BACKGROUND_IO_ERROR;
}
const char *bk_background_status_name(bk_background_status_t s) {
  switch (s) {
  case BK_BACKGROUND_OK:
    return "ok";
  case BK_BACKGROUND_NO_ASSET:
    return "no-asset";
  case BK_BACKGROUND_INVALID_ARGUMENT:
    return "invalid-argument";
  case BK_BACKGROUND_BUFFER_TOO_SMALL:
    return "buffer-too-small";
  case BK_BACKGROUND_IO_ERROR:
    return "io-error";
  case BK_BACKGROUND_NO_PARTITION:
    return "no-partition";
  default:
    return "unknown";
  }
}
