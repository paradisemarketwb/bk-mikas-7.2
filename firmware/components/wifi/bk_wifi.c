#include "bk_wifi.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_image_format.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include <ctype.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char ssid[32], password[33], authorization[96];
static bk_wifi_hooks_t api;
static atomic_bool updating;
extern const unsigned char web_start[] asm("_binary_index_html_start");
extern const unsigned char web_end[] asm("_binary_index_html_end");
const char *bk_wifi_ssid(void) { return ssid; }
const char *bk_wifi_password(void) { return password; }
unsigned bk_wifi_clients(void) {
  wifi_sta_list_t list;
  return esp_wifi_ap_get_sta_list(&list) == ESP_OK ? list.num : 0;
}
static bool equal(const char *a, const char *b) {
  size_t n = strlen(a);
  if (n != strlen(b))
    return false;
  unsigned v = 0;
  for (size_t i = 0; i < n; i++)
    v |= (unsigned char)a[i] ^ (unsigned char)b[i];
  return v == 0;
}
static bool authorized(httpd_req_t *r, bool write) {
  char auth[128];
  if (httpd_req_get_hdr_value_str(r, "Authorization", auth, sizeof(auth)) !=
          ESP_OK ||
      !equal(auth, authorization)) {
    httpd_resp_set_status(r, "401 Unauthorized");
    httpd_resp_set_hdr(r, "WWW-Authenticate", "Basic realm=HUNTER");
    httpd_resp_sendstr(r, "Authentication required");
    return false;
  }
  if (write) {
    char intent[24];
    if (httpd_req_get_hdr_value_str(r, "X-BK-Intent", intent, sizeof(intent)) !=
            ESP_OK ||
        strcmp(intent, "configure")) {
      httpd_resp_send_err(r, HTTPD_403_FORBIDDEN,
                          "Explicit intent header required");
      return false;
    }
  }
  return true;
}
static esp_err_t json_send(httpd_req_t *r, cJSON *root) {
  char *s = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (!s)
    return httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR,
                               "Out of memory");
  httpd_resp_set_type(r, "application/json");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  esp_err_t e = httpd_resp_sendstr(r, s);
  cJSON_free(s);
  return e;
}
static esp_err_t index_get(httpd_req_t *r) {
  httpd_resp_set_type(r, "text/html; charset=utf-8");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
  return httpd_resp_send(r, (const char *)web_start, web_end - web_start);
}
static esp_err_t data_get(httpd_req_t *r) {
  if (!authorized(r, false))
    return ESP_OK;
  bk_snapshot_t s;
  api.snapshot(&s);
  uint64_t now = esp_timer_get_time() / 1000;
  cJSON *root = cJSON_CreateObject();
  if (!root)
    return ESP_ERR_NO_MEM;
  cJSON_AddBoolToObject(root, "connected", s.connected);
  cJSON_AddBoolToObject(root, "adapter_ready",
                        api.adapter_ready && api.adapter_ready());
  cJSON_AddStringToObject(root, "transport",
                          api.transport_name ? api.transport_name : "unknown");
  cJSON_AddNumberToObject(root, "clients", bk_wifi_clients());
  cJSON_AddStringToObject(root, "version", esp_app_get_description()->version);
  cJSON_AddBoolToObject(root, "maintenance", atomic_load(&updating));
  cJSON *a = cJSON_AddArrayToObject(root, "metrics");
  static const char *keys[] = {
      "rpm", "coolant",  "volts",        "throttle",    "intake",     "advance",
      "air", "fuel_lph", "injection_ms", "lambda1_raw", "lambda2_raw"};
  for (unsigned i = 0; i < TEL_METRIC_COUNT; i++) {
    bk_sample_t v = bk_store_get(&s, i, now);
    cJSON *m = cJSON_CreateObject();
    cJSON_AddStringToObject(m, "key", keys[i]);
    cJSON_AddNumberToObject(m, "state", v.state);
    cJSON_AddStringToObject(m, "source",
                            v.source == TEL_SOURCE_MIKAS ? "mikas-monitor-0b"
                                                         : "none");
    if (v.state == TEL_VALID)
      cJSON_AddNumberToObject(m, "value", v.value);
    else
      cJSON_AddNullToObject(m, "value");
    if (v.state == TEL_VALID || v.state == TEL_STALE)
      cJSON_AddNumberToObject(m, "age_ms", now - v.at_ms);
    cJSON_AddItemToArray(a, m);
  }
  cJSON_AddBoolToObject(root, "faults_valid",
                        s.connected && s.faults_valid &&
                            now - s.faults_at_ms <= 15000);
  cJSON *faults = cJSON_AddArrayToObject(root, "faults");
  for (unsigned i = 0; i < s.fault_count && i < 127; i++) {
    cJSON *f = cJSON_CreateObject();
    cJSON_AddNumberToObject(f, "code", s.faults[i * 2]);
    cJSON_AddNumberToObject(f, "flags", s.faults[i * 2 + 1]);
    cJSON_AddStringToObject(f, "description",
                            bk_dtc_description(s.faults[i * 2]));
    cJSON_AddItemToArray(faults, f);
  }
  return json_send(r, root);
}
static esp_err_t history_get(httpd_req_t *r) {
  if (!authorized(r, false))
    return ESP_OK;
  bk_history_t *h = malloc(sizeof(*h));
  if (!h)
    return ESP_ERR_NO_MEM;
  api.history(h);
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "observed_engine_seconds",
                          h->observed_run_ms / 1000);
  cJSON *a = cJSON_AddArrayToObject(root, "points");
  for (unsigned i = 0; i < h->count; i++) {
    bk_history_point_t *p =
        &h->points[(h->head + BK_LOG_POINTS - h->count + i) % BK_LOG_POINTS];
    cJSON *row = cJSON_CreateArray();
    cJSON_AddItemToArray(row, cJSON_CreateNumber(p->at_ms));
    float vals[] = {p->rpm, p->coolant, p->volts};
    for (unsigned j = 0; j < 3; j++)
      cJSON_AddItemToArray(row, (p->valid & (1u << j))
                                    ? cJSON_CreateNumber(vals[j])
                                    : cJSON_CreateNull());
    cJSON_AddItemToArray(a, row);
  }
  a = cJSON_AddArrayToObject(root, "events");
  for (unsigned i = 0; i < h->event_count; i++) {
    bk_dtc_event_t *e =
        &h->events[(h->event_head + BK_HISTORY_EVENTS - h->event_count + i) %
                   BK_HISTORY_EVENTS];
    cJSON *v = cJSON_CreateObject();
    cJSON_AddNumberToObject(v, "at_ms", e->at_ms);
    cJSON_AddNumberToObject(v, "boot_id", e->boot_id);
    cJSON_AddNumberToObject(v, "code", e->code);
    cJSON_AddNumberToObject(v, "flags", e->flags);
    cJSON_AddBoolToObject(v, "present", e->present);
    cJSON_AddStringToObject(v, "description", bk_dtc_description(e->code));
    cJSON_AddItemToArray(a, v);
  }
  free(h);
  return json_send(r, root);
}
static esp_err_t config_get(httpd_req_t *r) {
  if (!authorized(r, false))
    return ESP_OK;
  bk_settings_t *s = malloc(sizeof(*s));
  if (!s)
    return ESP_ERR_NO_MEM;
  api.config_get(s);
  char *json = bk_settings_to_json(s);
  free(s);
  if (!json)
    return ESP_ERR_NO_MEM;
  httpd_resp_set_type(r, "application/json");
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  esp_err_t e = httpd_resp_sendstr(r, json);
  cJSON_free(json);
  return e;
}
static char *body_read(httpd_req_t *r, unsigned max) {
  if (r->content_len <= 0 || r->content_len > max)
    return NULL;
  char *b = malloc(r->content_len + 1);
  if (!b)
    return NULL;
  size_t at = 0;
  unsigned timeouts = 0;
  while (at < r->content_len) {
    int n = httpd_req_recv(r, b + at, r->content_len - at);
    if (n == HTTPD_SOCK_ERR_TIMEOUT && timeouts++ < 3)
      continue;
    if (n <= 0) {
      free(b);
      return NULL;
    }
    at += n;
  }
  b[at] = 0;
  if (memchr(b, 0, at)) {
    free(b);
    return NULL;
  }
  return b;
}
static esp_err_t config_post(httpd_req_t *r) {
  if (!authorized(r, true))
    return ESP_OK;
  if (atomic_load(&updating))
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Update in progress");
  char *body = body_read(r, 32768);
  bk_settings_t *cfg = malloc(sizeof(*cfg));
  if (!body || !cfg) {
    free(body);
    free(cfg);
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                               "Invalid or oversized body");
  }
  bool valid = bk_settings_from_json(body, cfg);
  free(body);
  esp_err_t e = valid ? api.config_apply(cfg) : ESP_ERR_INVALID_ARG;
  free(cfg);
  if (e)
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                               "Profile rejected or cannot be saved");
  return httpd_resp_sendstr(r, "{\"ok\":true}");
}
static esp_err_t reset_post(httpd_req_t *r) {
  if (!authorized(r, true))
    return ESP_OK;
  char confirm[24];
  if (httpd_req_get_hdr_value_str(r, "X-BK-Confirm", confirm,
                                  sizeof(confirm)) ||
      strcmp(confirm, "RESET"))
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                               "RESET confirmation required");
  bk_settings_t *s = malloc(sizeof(*s));
  if (!s)
    return ESP_ERR_NO_MEM;
  bk_settings_defaults(s);
  esp_err_t e = api.config_apply(s);
  free(s);
  return e ? httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR,
                                 "Save failed")
           : httpd_resp_sendstr(r, "{\"ok\":true}");
}
static void restart_task(void *arg) {
  (void)arg;
  vTaskDelay(pdMS_TO_TICKS(1500));
  esp_restart();
}
static esp_err_t ota_post(httpd_req_t *r) {
  if (!authorized(r, true))
    return ESP_OK;
  char confirm[16], hex[65];
  if (httpd_req_get_hdr_value_str(r, "X-BK-Confirm", confirm,
                                  sizeof(confirm)) ||
      strcmp(confirm, "UPDATE") ||
      httpd_req_get_hdr_value_str(r, "X-BK-SHA256", hex, sizeof(hex)) ||
      strlen(hex) != 64)
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                               "UPDATE and SHA256 required");
  for (unsigned i = 0; i < 64; i++)
    if (!isxdigit((unsigned char)hex[i]))
      return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid hash");
  uint8_t expected[32];
  for (unsigned i = 0; i < 32; i++) {
    unsigned x;
    if (sscanf(hex + i * 2, "%2x", &x) != 1)
      return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid hash");
    expected[i] = x;
  }
  const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
  if (!target || r->content_len < 512 || r->content_len > target->size)
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Invalid image size");
  bool was = false;
  if (!atomic_compare_exchange_strong(&updating, &was, true))
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                               "Update already active");
  if (!api.ota_prepare()) {
    atomic_store(&updating, false);
    return httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                               "Stop engine before updating");
  }
  uint8_t *buf = malloc(4096);
  esp_ota_handle_t handle = 0;
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  esp_err_t e = buf ? ESP_OK : ESP_ERR_NO_MEM;
  bool started = false;
  size_t total = 0;
  unsigned timeouts = 0;
  mbedtls_sha256_starts(&sha, 0);
  while (!e && total < r->content_len) {
    size_t want = r->content_len - total;
    if (want > 4096)
      want = 4096;
    int n = httpd_req_recv(r, (char *)buf, want);
    if (n == HTTPD_SOCK_ERR_TIMEOUT && timeouts++ < 3)
      continue;
    if (n <= 0) {
      e = ESP_FAIL;
      break;
    }
    if (!started) {
      size_t have = n, need = sizeof(esp_image_header_t) +
                              sizeof(esp_image_segment_header_t) +
                              sizeof(esp_app_desc_t);
      while (have < need) {
        int more = httpd_req_recv(r, (char *)buf + have, need - have);
        if (more <= 0) {
          e = ESP_FAIL;
          break;
        }
        have += more;
      }
      n = have;
      if (e)
        break;
      esp_app_desc_t desc;
      memcpy(&desc,
             buf + sizeof(esp_image_header_t) +
                 sizeof(esp_image_segment_header_t),
             sizeof(desc));
      if (buf[0] != ESP_IMAGE_HEADER_MAGIC ||
          desc.magic_word != ESP_APP_DESC_MAGIC_WORD ||
          memcmp(desc.project_name, esp_app_get_description()->project_name,
                 sizeof(desc.project_name))) {
        e = ESP_ERR_INVALID_ARG;
        break;
      }
      e = esp_ota_begin(target, r->content_len, &handle);
      if (e)
        break;
      started = true;
    }
    mbedtls_sha256_update(&sha, buf, n);
    e = esp_ota_write(handle, buf, n);
    total += n;
  }
  uint8_t actual[32];
  mbedtls_sha256_finish(&sha, actual);
  mbedtls_sha256_free(&sha);
  free(buf);
  unsigned diff = 0;
  for (unsigned i = 0; i < 32; i++)
    diff |= actual[i] ^ expected[i];
  if (diff)
    e = ESP_ERR_INVALID_CRC;
  if (!e && total == r->content_len)
    e = esp_ota_end(handle);
  else if (started)
    esp_ota_abort(handle);
  if (!e)
    e = esp_ota_set_boot_partition(target);
  /* Keep K-Line disarmed after a failed update until a deliberate reboot. */
  if (e)
    return httpd_resp_send_err(
        r, HTTPD_400_BAD_REQUEST,
        "Image rejected; current firmware preserved. Reboot before retry.");
  esp_err_t sent = httpd_resp_sendstr(r, "{\"ok\":true,\"restarting\":true}");
  if (xTaskCreate(restart_task, "restart", 2048, NULL, 2, NULL) != pdPASS)
    esp_restart();
  return sent;
}
esp_err_t bk_wifi_start(const bk_wifi_hooks_t *hooks) {
  if (!hooks || !hooks->snapshot || !hooks->history || !hooks->config_get ||
      !hooks->config_apply || !hooks->ota_prepare)
    return ESP_ERR_INVALID_ARG;
  api = *hooks;
  nvs_handle_t h;
  esp_err_t e = nvs_open("bk_wifi", NVS_READWRITE, &h);
  if (e)
    return e;
  size_t size = sizeof(password);
  e = nvs_get_str(h, "password", password, &size);
  if (e == ESP_ERR_NVS_NOT_FOUND) {
    uint8_t random[16];
    esp_fill_random(random, sizeof(random));
    for (unsigned i = 0; i < 16; i++)
      snprintf(password + i * 2, 3, "%02x", random[i]);
    e = nvs_set_str(h, "password", password);
    if (!e)
      e = nvs_commit(h);
  }
  nvs_close(h);
  if (e)
    return e;
  char login[48];
  snprintf(login, sizeof(login), "hunter:%s", password);
  size_t encoded = 0;
  memcpy(authorization, "Basic ", 6);
  if (mbedtls_base64_encode((unsigned char *)authorization + 6,
                            sizeof(authorization) - 6, &encoded,
                            (const unsigned char *)login, strlen(login)))
    return ESP_FAIL;
  authorization[6 + encoded] = 0;
  uint8_t mac[6];
  e = esp_netif_init();
  if (e)
    return e;
  e = esp_event_loop_create_default();
  if (e)
    return e;
  if (!esp_netif_create_default_wifi_ap())
    return ESP_ERR_NO_MEM;
  wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
  e = esp_wifi_init(&init);
  if (e)
    return e;
  e = esp_wifi_get_mac(WIFI_IF_AP, mac);
  if (e)
    return e;
  snprintf(ssid, sizeof(ssid), "HUNTER-%02X%02X%02X", mac[3], mac[4], mac[5]);
  wifi_config_t cfg = {.ap = {.channel = 6,
                              .max_connection = 2,
                              .authmode = WIFI_AUTH_WPA2_PSK,
                              .pmf_cfg = {.capable = true}}};
  memcpy(cfg.ap.ssid, ssid, strlen(ssid));
  cfg.ap.ssid_len = strlen(ssid);
  memcpy(cfg.ap.password, password, strlen(password));
  e = esp_wifi_set_mode(WIFI_MODE_AP);
  if (e)
    return e;
  e = esp_wifi_set_config(WIFI_IF_AP, &cfg);
  if (e)
    return e;
  e = esp_wifi_start();
  if (e)
    return e;
  httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
  hc.max_uri_handlers = 8;
  hc.stack_size = 8192;
  httpd_handle_t server;
  e = httpd_start(&server, &hc);
  if (e)
    return e;
  const httpd_uri_t routes[] = {
      {.uri = "/", .method = HTTP_GET, .handler = index_get},
      {.uri = "/api/telemetry", .method = HTTP_GET, .handler = data_get},
      {.uri = "/api/history", .method = HTTP_GET, .handler = history_get},
      {.uri = "/api/config", .method = HTTP_GET, .handler = config_get},
      {.uri = "/api/config", .method = HTTP_POST, .handler = config_post},
      {.uri = "/api/reset", .method = HTTP_POST, .handler = reset_post},
      {.uri = "/api/ota", .method = HTTP_POST, .handler = ota_post}};
  for (unsigned i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
    e = httpd_register_uri_handler(server, &routes[i]);
    if (e)
      return e;
  }
  return ESP_OK;
}
