#include "bk_background.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_partition.h"
static bool part_read(void *ctx, size_t off, void *data, size_t n) {
  return esp_partition_read(ctx, off, data, n) == ESP_OK;
}
static bool part_write(void *ctx, size_t off, const void *data, size_t n) {
  return esp_partition_write(ctx, off, data, n) == ESP_OK;
}
static bool part_erase(void *ctx, size_t off, size_t n) {
  return esp_partition_erase_range(ctx, off, n) == ESP_OK;
}
bk_background_status_t bk_background_esp_open(bk_background_t *s) {
  if (!s)
    return BK_BACKGROUND_INVALID_ARGUMENT;
  memset(s, 0, sizeof(*s));
  const esp_partition_t *part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "ui_assets");
  if (!part || part->address != BK_BACKGROUND_PARTITION_OFFSET ||
      part->size < BK_BACKGROUND_PARTITION_BYTES)
    return BK_BACKGROUND_NO_PARTITION;
  bk_background_io_t io = {.context = (void *)part,
                           .size = part->size,
                           .read = part_read,
                           .write = part_write,
                           .erase = part_erase};
  return bk_background_init(s, &io);
}
#else
bk_background_status_t bk_background_esp_open(bk_background_t *s) {
  if (!s)
    return BK_BACKGROUND_INVALID_ARGUMENT;
  memset(s, 0, sizeof(*s));
  return BK_BACKGROUND_NO_PARTITION;
}
#endif
