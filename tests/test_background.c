#include "bk_background.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned char flash[BK_BACKGROUND_PARTITION_BYTES],
    saved[BK_BACKGROUND_PARTITION_BYTES];
static long long budget = -1;
static unsigned writes, erases;
static bool read_failure;
static bool read_bytes(void *ctx, size_t off, void *data, size_t n) {
  (void)ctx;
  if (read_failure || off > sizeof(flash) || n > sizeof(flash) - off)
    return false;
  memcpy(data, flash + off, n);
  return true;
}
static size_t allowed(size_t n) {
  if (budget < 0)
    return n;
  if ((unsigned long long)budget < n) {
    size_t take = (size_t)budget;
    budget = 0;
    return take;
  }
  budget -= (long long)n;
  return n;
}
static bool write_bytes(void *ctx, size_t off, const void *data, size_t n) {
  (void)ctx;
  writes++;
  assert(off % 16 == 0 && n % 16 == 0);
  if (off > sizeof(flash) || n > sizeof(flash) - off)
    return false;
  size_t take = allowed(n);
  const unsigned char *src = data;
  for (size_t i = 0; i < take; i++) {
    assert((flash[off + i] & src[i]) == src[i]);
    flash[off + i] &= src[i];
  }
  return take == n;
}
static bool erase_bytes(void *ctx, size_t off, size_t n) {
  (void)ctx;
  erases++;
  assert(off % 4096 == 0 && n % 4096 == 0);
  if (off > sizeof(flash) || n > sizeof(flash) - off)
    return false;
  size_t take = allowed(n);
  memset(flash + off, 255, take);
  return take == n;
}
static bk_background_t start(void) {
  bk_background_t b;
  bk_background_io_t io = {.size = sizeof(flash),
                           .read = read_bytes,
                           .write = write_bytes,
                           .erase = erase_bytes};
  assert(bk_background_init(&b, &io) == BK_BACKGROUND_OK);
  return b;
}
static void fix_header_crc(size_t base) {
  unsigned char *h = flash + base;
  memset(h + 32, 0, 4);
  uint32_t crc = bk_background_crc32(h, 64);
  for (unsigned i = 0; i < 4; i++)
    h[32 + i] = (unsigned char)(crc >> (i * 8));
}
static void put32(size_t base, unsigned off, uint32_t value) {
  for (unsigned i = 0; i < 4; i++)
    flash[base + off + i] = (unsigned char)(value >> (i * 8));
}
int main(void) {
  assert(bk_background_crc32("123456789", 9) == 0xcbf43926u);
  assert(bk_background_crc32(NULL, 0) == 0);
  memset(flash, 255, sizeof(flash));
  bk_background_t b = start();
  bk_background_info_t info;
  unsigned char out[16];
  assert(bk_background_slot_info(&b, 0, &info) == BK_BACKGROUND_NO_ASSET &&
         !info.available);
  assert(bk_background_slot_info(&b, 4, &info) ==
         BK_BACKGROUND_INVALID_ARGUMENT);
  const unsigned char a[] = {0x00, 0xf8, 0xe0, 0x07, 0x1f, 0x00, 0xff, 0xff};
  const unsigned char next[] = {0x12, 0x34, 0x56, 0x78, 0x90, 0xab};
  assert(bk_background_store(&b, 0, 2, 2, a, sizeof(a)) == BK_BACKGROUND_OK);
  assert(bk_background_load(&b, 0, &info, out, sizeof(out)) ==
         BK_BACKGROUND_OK);
  assert(info.available && info.generation == 1 && info.bank == 0 &&
         info.width == 2 && info.height == 2 &&
         info.format == BK_BACKGROUND_FORMAT_RGB565_LE);
  assert(!memcmp(out, a, sizeof(a)));
  assert(bk_background_load(&b, 0, &info, out, 2) ==
             BK_BACKGROUND_BUFFER_TOO_SMALL &&
         info.available && info.bytes == 8);
  memcpy(saved, flash, sizeof(flash));
  const long long cuts[] = {0,      1,      4096,   131071, 131072, 131073,
                            131087, 131088, 131089, 131120, 131151, 131152};
  for (unsigned i = 0; i < sizeof(cuts) / sizeof(cuts[0]); i++) {
    memcpy(flash, saved, sizeof(flash));
    budget = cuts[i];
    bk_background_status_t st =
        bk_background_store(&b, 0, 3, 1, next, sizeof(next));
    budget = -1;
    assert(bk_background_load(&b, 0, &info, out, sizeof(out)) ==
           BK_BACKGROUND_OK);
    if (st == BK_BACKGROUND_OK) {
      assert(info.bank == 1 && info.generation == 2);
      assert(!memcmp(out, next, sizeof(next)));
    } else {
      assert(st == BK_BACKGROUND_IO_ERROR && info.bank == 0 &&
             info.generation == 1);
      assert(!memcmp(out, a, sizeof(a)));
    }
  }
  memcpy(flash, saved, sizeof(flash));
  unsigned before = writes + erases;
  assert(bk_background_store(&b, 4, 2, 2, a, 8) ==
         BK_BACKGROUND_INVALID_ARGUMENT);
  assert(bk_background_store(&b, 0, 0, 2, a, 8) ==
         BK_BACKGROUND_INVALID_ARGUMENT);
  assert(bk_background_store(&b, 0, 241, 1, a, 8) ==
         BK_BACKGROUND_INVALID_ARGUMENT);
  assert(bk_background_store(&b, 0, 2, 2, a, 7) ==
         BK_BACKGROUND_INVALID_ARGUMENT);
  assert(bk_background_store(&b, 0, 2, 2, NULL, 8) ==
         BK_BACKGROUND_INVALID_ARGUMENT);
  assert(before == writes + erases);
  assert(bk_background_store(&b, 0, 3, 1, next, 6) == BK_BACKGROUND_OK);
  flash[BK_BACKGROUND_BANK_BYTES + BK_BACKGROUND_HEADER_BYTES] ^= 1;
  assert(bk_background_load(&b, 0, &info, out, sizeof(out)) ==
             BK_BACKGROUND_OK &&
         info.generation == 1);
  assert(!memcmp(out, a, 8));
  memcpy(flash, saved, sizeof(flash));
  assert(bk_background_store(&b, 0, 3, 1, next, 6) == BK_BACKGROUND_OK);
  put32(BK_BACKGROUND_BANK_BYTES, 20, 99);
  fix_header_crc(BK_BACKGROUND_BANK_BYTES);
  assert(bk_background_slot_info(&b, 0, &info) == BK_BACKGROUND_OK &&
         info.generation == 1); /* unknown format despite valid header CRC */
  memcpy(flash, saved, sizeof(flash));
  put32(0, 24, UINT32_MAX);
  fix_header_crc(0);
  assert(bk_background_slot_info(&b, 0, &info) == BK_BACKGROUND_NO_ASSET &&
         !info.available); /* checked before read/allocation */
  memcpy(flash, saved, sizeof(flash));
  flash[16] = 241;
  fix_header_crc(0);
  assert(bk_background_slot_info(&b, 0, &info) == BK_BACKGROUND_NO_ASSET);
  memcpy(flash, saved, sizeof(flash));
  flash[12] = 1;
  fix_header_crc(0);
  assert(bk_background_slot_info(&b, 0, &info) ==
         BK_BACKGROUND_NO_ASSET); /* wrong slot */
  memcpy(flash, saved, sizeof(flash));
  flash[4] = 2;
  fix_header_crc(0);
  assert(bk_background_slot_info(&b, 0, &info) ==
         BK_BACKGROUND_NO_ASSET); /* unsupported version */
  memcpy(flash, saved, sizeof(flash));
  flash[32] ^= 1;
  assert(bk_background_slot_info(&b, 0, &info) ==
         BK_BACKGROUND_NO_ASSET); /* header CRC */
  memcpy(flash, saved, sizeof(flash));
  put32(0, 8, UINT32_MAX);
  fix_header_crc(0);
  assert(bk_background_store(&b, 0, 3, 1, next, 6) == BK_BACKGROUND_OK);
  assert(bk_background_slot_info(&b, 0, &info) == BK_BACKGROUND_OK &&
         info.generation == 0 && info.bank == 1); /* modular generation */
  memset(flash, 255, sizeof(flash));
  assert(bk_background_store(&b, 3, 2, 2, a, 8) == BK_BACKGROUND_OK);
  assert(bk_background_slot_info(&b, 0, &info) == BK_BACKGROUND_NO_ASSET);
  assert(bk_background_slot_info(&b, 3, &info) == BK_BACKGROUND_OK &&
         info.slot == 3);
  static unsigned char max_pixels[BK_BACKGROUND_MAX_PIXELS_BYTES];
  memset(max_pixels, 0x5a, sizeof(max_pixels));
  assert(bk_background_store(&b, 2, 240, 240, max_pixels, sizeof(max_pixels)) ==
         BK_BACKGROUND_OK);
  assert(bk_background_load(&b, 2, &info, max_pixels, sizeof(max_pixels)) ==
             BK_BACKGROUND_OK &&
         info.bytes == sizeof(max_pixels));
  read_failure = true;
  assert(bk_background_slot_info(&b, 2, &info) == BK_BACKGROUND_IO_ERROR &&
         !info.available);
  before = erases;
  assert(bk_background_store(&b, 2, 2, 2, a, 8) == BK_BACKGROUND_IO_ERROR &&
         before == erases);
  read_failure = false;
  bk_background_t absent = {0};
  assert(bk_background_slot_info(&absent, 0, &info) ==
             BK_BACKGROUND_NO_PARTITION &&
         !info.available);
  assert(bk_background_store(&absent, 0, 2, 2, a, 8) ==
         BK_BACKGROUND_NO_PARTITION);
  assert(bk_background_esp_open(&absent) == BK_BACKGROUND_NO_PARTITION);
  puts("background RGB565/CRC/two-bank powercut recovery: PASS");
  return 0;
}
