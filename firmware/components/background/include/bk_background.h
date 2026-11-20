#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BK_BACKGROUND_SLOTS 4u
#define BK_BACKGROUND_BANKS 2u
#define BK_BACKGROUND_BANK_BYTES 131072u
#define BK_BACKGROUND_HEADER_BYTES 64u
#define BK_BACKGROUND_PARTITION_BYTES 1048576u
#define BK_BACKGROUND_PARTITION_OFFSET 0x610000u
#define BK_BACKGROUND_MAX_WIDTH 240u
#define BK_BACKGROUND_MAX_HEIGHT 240u
#define BK_BACKGROUND_MAX_PIXELS_BYTES (240u * 240u * 2u)
#define BK_BACKGROUND_FORMAT_RGB565_LE 1u

typedef enum {
  BK_BACKGROUND_OK,
  BK_BACKGROUND_NO_ASSET,
  BK_BACKGROUND_INVALID_ARGUMENT,
  BK_BACKGROUND_BUFFER_TOO_SMALL,
  BK_BACKGROUND_IO_ERROR,
  BK_BACKGROUND_NO_PARTITION
} bk_background_status_t;
typedef struct {
  void *context;
  size_t size;
  bool (*read)(void *context, size_t offset, void *data, size_t length);
  bool (*write)(void *context, size_t offset, const void *data, size_t length);
  bool (*erase)(void *context, size_t offset, size_t length);
} bk_background_io_t;
typedef struct {
  bk_background_io_t io;
} bk_background_t;
typedef struct {
  bool available;
  unsigned slot, bank;
  uint16_t width, height;
  uint32_t generation, format, bytes, crc32;
} bk_background_info_t;

/* Callers serialize access; the codec does not allocate memory or keep stale
 * caches. */
bk_background_status_t bk_background_init(bk_background_t *store,
                                          const bk_background_io_t *io);
/* pixels==NULL reads/validates metadata and the on-flash payload CRC without
 * copying. */
bk_background_status_t bk_background_load(bk_background_t *store, unsigned slot,
                                          bk_background_info_t *info,
                                          void *pixels, size_t capacity);
bk_background_status_t bk_background_slot_info(bk_background_t *store,
                                               unsigned slot,
                                               bk_background_info_t *info);
/* Fixed RGB565 little-endian format, exact width*height*2 bytes. Commit header
 * last. */
bk_background_status_t bk_background_store(bk_background_t *store,
                                           unsigned slot, uint16_t width,
                                           uint16_t height, const void *pixels,
                                           size_t length);
uint32_t bk_background_crc32(const void *data, size_t length);
const char *bk_background_status_name(bk_background_status_t status);
/* ESP backend finds the existing ui_assets partition, never creates/moves
 * partitions. */
bk_background_status_t bk_background_esp_open(bk_background_t *store);
