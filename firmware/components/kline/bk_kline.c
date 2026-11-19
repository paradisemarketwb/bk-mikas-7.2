#include "bk_kline.h"
#include "bk_link_backend.h"
#include "esp_log.h"
#include "mikas_link.h"
#include "sdkconfig.h"
#include <stdatomic.h>
esp_err_t bk_kline_init(void) { return bk_backend_init(); }
void bk_kline_disarm(void) { bk_backend_disarm(); }
esp_err_t bk_kline_exchange(const uint8_t *cmd, size_t n, uint8_t *reply,
                            size_t cap, size_t *out, unsigned timeout) {
  mikas_link_result_t result = mikas_stream_exchange(
      bk_backend_stream(), cmd, n, reply, cap, out, timeout);
  static atomic_int last_result = MIKAS_LINK_OK;
  int previous = atomic_exchange(&last_result, result);
  if (result != MIKAS_LINK_OK && previous != (int)result) {
    static const char *errors[] = {"ok",
                                   "command/argument rejected",
                                   "stream I/O error",
                                   "adapter disconnected or paused",
                                   "echo mismatch",
                                   "invalid frame/checksum",
                                   "reply timeout"};
    ESP_LOGW("ecu_link", "%s: %s", bk_kline_transport_name(), errors[result]);
  }
  switch (result) {
  case MIKAS_LINK_OK:
    return ESP_OK;
  case MIKAS_LINK_ARGUMENT:
    return ESP_ERR_INVALID_ARG;
  case MIKAS_LINK_TIMEOUT:
    return ESP_ERR_TIMEOUT;
  case MIKAS_LINK_DISCONNECTED:
    return ESP_ERR_INVALID_STATE;
  case MIKAS_LINK_ECHO:
  case MIKAS_LINK_FRAME:
    return ESP_ERR_INVALID_RESPONSE;
  default:
    return ESP_FAIL;
  }
}
const char *bk_kline_transport_name(void) {
#if CONFIG_BK_TRANSPORT_USB_KKL
  return "USB-KKL FT232RL (0403:6001)";
#else
  return "UART / L9637D";
#endif
}
bool bk_kline_adapter_ready(void) {
#if CONFIG_BK_ENABLE_KLINE
  return bk_backend_ready();
#else
  return false;
#endif
}
