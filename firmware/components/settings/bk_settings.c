#include "bk_settings.h"
#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdlib.h>
#include <string.h>
#define PROFILE_CAPACITY 16384u
#define LEGACY_FLASH_BUDGET 6144u
/* New partition keeps full layouts away from the shared legacy AP/journal NVS.
 * OTA with an old partition table retains bounded legacy storage, never erases
 * it. */
static esp_err_t profile_open(nvs_open_mode_t mode, nvs_handle_t *h,
                              bool *dedicated) {
  const esp_partition_t *part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "ui_profiles");
  *dedicated = part != NULL;
  if (part) {
    if (part->address != 0x710000 || part->size < 0x10000)
      return ESP_ERR_INVALID_STATE;
    esp_err_t e = nvs_flash_init_partition("ui_profiles");
    if (e)
      return e;
    return nvs_open_from_partition("ui_profiles", "bk_settings", mode, h);
  }
  return nvs_open("bk_settings", mode, h);
}
static esp_err_t profile_read(nvs_handle_t h, const char *key,
                              bk_settings_t *s) {
  size_t n = 0;
  esp_err_t e = nvs_get_blob(h, key, NULL, &n);
  if (e)
    return e;
  if (!n || n > PROFILE_CAPACITY)
    return ESP_ERR_INVALID_SIZE;
  void *data = malloc(n);
  if (!data)
    return ESP_ERR_NO_MEM;
  e = nvs_get_blob(h, key, data, &n);
  if (!e && !bk_settings_decode(data, n, s))
    e = ESP_ERR_INVALID_STATE;
  free(data);
  return e;
}
esp_err_t bk_settings_load(bk_settings_t *s) {
  bk_settings_defaults(s);
  nvs_handle_t h;
  bool dedicated;
  esp_err_t e = profile_open(NVS_READONLY, &h, &dedicated);
  if (!e) {
    e = profile_read(h, dedicated ? "profile4" : "profile3", s);
    nvs_close(h);
  }
  if (dedicated && e == ESP_ERR_NVS_NOT_FOUND) {
    /* Dedicated space is new: read existing v3/v4 from the old NVS unchanged.
     */
    e = nvs_open("bk_settings", NVS_READONLY, &h);
    if (!e) {
      e = profile_read(h, "profile3", s);
      nvs_close(h);
    }
  }
  return e == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : e;
}
esp_err_t bk_settings_save(const bk_settings_t *s) {
  if (!bk_settings_valid(s))
    return ESP_ERR_INVALID_ARG;
  unsigned char *data = malloc(PROFILE_CAPACITY);
  if (!data)
    return ESP_ERR_NO_MEM;
  size_t n = bk_settings_encode(s, data, PROFILE_CAPACITY);
  if (!n) {
    free(data);
    return ESP_ERR_INVALID_SIZE;
  }
  nvs_handle_t h;
  bool dedicated;
  esp_err_t e = profile_open(NVS_READWRITE, &h, &dedicated);
  if (e) {
    free(data);
    return e;
  }
  if (!dedicated && n > LEGACY_FLASH_BUDGET) {
    free(data);
    nvs_close(h);
    return ESP_ERR_INVALID_SIZE;
  }
  const char *key = dedicated ? "profile4" : "profile3";
  size_t old_n = 0;
  if (nvs_get_blob(h, key, NULL, &old_n) == ESP_OK && old_n == n) {
    void *old = malloc(n);
    if (old && nvs_get_blob(h, key, old, &old_n) == ESP_OK &&
        !memcmp(old, data, n)) {
      free(old);
      free(data);
      nvs_close(h);
      return ESP_OK;
    }
    free(old);
  }
  e = nvs_set_blob(h, key, data, n);
  if (!e)
    e = nvs_commit(h);
  free(data);
  nvs_close(h);
  return e;
}
