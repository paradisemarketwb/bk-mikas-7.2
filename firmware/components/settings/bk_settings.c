#include "bk_settings.h"
#include "nvs.h"
#include <stdlib.h>
#include <string.h>
esp_err_t bk_settings_load(bk_settings_t *s) {
  bk_settings_defaults(s);
  nvs_handle_t h;
  esp_err_t e = nvs_open("bk_settings", NVS_READONLY, &h);
  if (e == ESP_ERR_NVS_NOT_FOUND)
    return ESP_OK;
  if (e)
    return e;
  bk_settings_t *v = malloc(sizeof(*v));
  if (!v) {
    nvs_close(h);
    return ESP_ERR_NO_MEM;
  }
  size_t n = sizeof(*v);
  e = nvs_get_blob(h, "profile3", v, &n);
  nvs_close(h);
  if (e == ESP_OK) {
    if (n != sizeof(*v) || !bk_settings_valid(v))
      e = ESP_ERR_INVALID_STATE;
    else
      *s = *v;
  }
  free(v);
  return e == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : e;
}
esp_err_t bk_settings_save(const bk_settings_t *s) {
  if (!bk_settings_valid(s))
    return ESP_ERR_INVALID_ARG;
  nvs_handle_t h;
  esp_err_t e = nvs_open("bk_settings", NVS_READWRITE, &h);
  if (e)
    return e;
  bk_settings_t *old = malloc(sizeof(*old));
  size_t n = sizeof(*old);
  if (old && nvs_get_blob(h, "profile3", old, &n) == ESP_OK &&
      n == sizeof(*old) && !memcmp(old, s, n)) {
    free(old);
    nvs_close(h);
    return ESP_OK;
  }
  free(old);
  e = nvs_set_blob(h, "profile3", s, sizeof(*s));
  if (!e)
    e = nvs_commit(h);
  nvs_close(h);
  return e;
}
