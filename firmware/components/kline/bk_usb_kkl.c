#include "bk_byte_ring.h"
#include "bk_link_backend.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "usb/cdc_acm_host.h"
#include "usb/usb_host.h"
#include "usb/vcp_ftdi.h"
#include <stdatomic.h>

/* Native USB D-=GPIO19, D+=GPIO20. This backend MUST NOT configure GPIO19
 * as TX_ARM. The UART backend is excluded by CMake in this configuration. */
static cdc_acm_dev_hdl_t device;
static bk_byte_ring_t rx;
static portMUX_TYPE rx_mux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t reader;
static atomic_bool connected, paused, rx_fault, serial_fault;
static atomic_uint disconnect_epoch;
static bool installed;
static const char *TAG = "usb_kkl";
static uint64_t now(void *ctx) {
  (void)ctx;
  return esp_timer_get_time() / 1000;
}
static void delay(void *ctx, unsigned ms) {
  (void)ctx;
  vTaskDelay(pdMS_TO_TICKS(ms));
}
static void wake_reader(void) {
  if (reader)
    xTaskNotifyGive(reader);
}
static void lost(void) {
  atomic_store(&connected, false);
  atomic_fetch_add(&disconnect_epoch, 1);
  wake_reader();
}
static bool data_received(const uint8_t *data, size_t n, void *ctx) {
  (void)ctx;
  if (atomic_load(&paused) || !atomic_load(&connected))
    return true;
  /* FT232RL endpoints are full-speed MPS64. Espressif removes the two FTDI
   * status bytes; in_buffer_size=0 selects exactly that endpoint MPS (64), not
   * an RX FIFO. */
  portENTER_CRITICAL(&rx_mux);
  bool ok = n <= 62 && bk_ring_push(&rx, data, n);
  portEXIT_CRITICAL(&rx_mux);
  if (!ok)
    atomic_store(&rx_fault, true);
  wake_reader();
  return true;
}
static void device_event(const cdc_acm_host_dev_event_data_t *event,
                         void *ctx) {
  (void)ctx;
  switch (event->type) {
  case CDC_ACM_HOST_DEVICE_DISCONNECTED:
  case CDC_ACM_HOST_ERROR:
    lost();
    break;
  case CDC_ACM_HOST_SERIAL_STATE: {
    bool bad =
        event->data.serial_state.bFraming || event->data.serial_state.bParity ||
        event->data.serial_state.bOverRun || event->data.serial_state.bBreak;
    atomic_store(&serial_fault, bad);
    if (bad) {
      atomic_store(&rx_fault, true);
      wake_reader();
    }
    break;
  }
  default:
    break;
  }
}
static void host_events(void *arg) {
  (void)arg;
  for (;;) {
    uint32_t events = 0;
    esp_err_t e = usb_host_lib_handle_events(portMAX_DELAY, &events);
    if (e != ESP_OK) {
      lost();
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }
    if (events & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS)
      usb_host_device_free_all();
  }
}
void bk_backend_disarm(void) {
  /* This pauses application TX; it is not a physical K-Line disconnect inside
   * a third-party KKL adapter. No DTR/RTS toggles or GPIO19 writes on OTA. */
  atomic_store(&paused, true);
  wake_reader();
}
esp_err_t bk_backend_init(void) {
  if (installed)
    return ESP_ERR_INVALID_STATE;
  reader = xTaskGetCurrentTaskHandle();
  atomic_init(&connected, false);
  atomic_init(&paused, false);
  atomic_init(&rx_fault, false);
  atomic_init(&serial_fault, false);
  atomic_init(&disconnect_epoch, 0);
  bk_ring_clear(&rx);
  usb_host_config_t cfg = {.skip_phy_setup = false,
                           .intr_flags = ESP_INTR_FLAG_LEVEL1};
  esp_err_t e = usb_host_install(&cfg);
  if (e)
    return e;
  const cdc_acm_host_driver_config_t cdc = {.driver_task_stack_size = 4096,
                                            .driver_task_priority = 7,
                                            .xCoreID = tskNO_AFFINITY,
                                            .new_dev_cb = NULL};
  e = cdc_acm_host_install(&cdc);
  if (e) {
    usb_host_uninstall();
    return e;
  }
  if (xTaskCreate(host_events, "usb_host", 4096, NULL, 8, NULL) != pdPASS) {
    cdc_acm_host_uninstall();
    usb_host_uninstall();
    return ESP_ERR_NO_MEM;
  }
  installed = true;
  ESP_LOGI(TAG, "USB Host ready: FT232RL 0403:6001, external 5V VBUS required");
  return ESP_OK;
}
static bool ensure_open(void) {
  if (!installed || atomic_load(&paused))
    return false;
  if (device && !atomic_load(&connected)) {
    cdc_acm_host_close(device);
    device = NULL;
  }
  if (device)
    return true;
  atomic_store(&serial_fault, false);
  unsigned epoch = atomic_load(&disconnect_epoch);
  const cdc_acm_host_device_config_t config = {.connection_timeout_ms = 300,
                                               .out_buffer_size = 64,
                                               .in_buffer_size = 0,
                                               .event_cb = device_event,
                                               .data_cb = data_received,
                                               .user_arg = NULL};
  esp_err_t e = ftdi_vcp_open(FT232_PID, 0, &config, &device);
  if (e != ESP_OK) {
    device = NULL;
    return false;
  }
  cdc_acm_line_coding_t line = {
      .dwDTERate = 9600, .bCharFormat = 0, .bParityType = 0, .bDataBits = 8};
  e = cdc_acm_host_line_coding_set(device, &line);
  /* Disable inherited hardware/software flow control, as pyserial does. */
  if (!e)
    e = cdc_acm_host_send_custom_request(device, 0x40, 0x01, 0, 1, 0, NULL);
  /* FTDI SIO_MODEM_CTRL: enable masks 0x0100|0x0200, values 1|2.
   * Explicit request preserves pyserial DTR/RTS=True semantics.
   * Reference: Linux ftdi_sio.h FTDI_SIO_SET_{DTR,RTS}_HIGH. */
  if (!e)
    e = cdc_acm_host_send_custom_request(device, 0x40, 0x02, 0x0303, 1, 0,
                                         NULL);
  /* FTDI latency timer=2ms; send vendor command, not CDC control to a VCP. */
  if (!e)
    e = cdc_acm_host_send_custom_request(device, 0x40, 0x09, 2, 1, 0, NULL);
  if (e || atomic_load(&paused) || epoch != atomic_load(&disconnect_epoch)) {
    cdc_acm_host_close(device);
    device = NULL;
    atomic_store(&connected, false);
    return false;
  }
  atomic_store(&connected, true);
  if (epoch != atomic_load(&disconnect_epoch)) {
    atomic_store(&connected, false);
    return false;
  }
  ESP_LOGI(TAG, "FT232RL opened: 9600 8N1, echo required");
  return true;
}
static bool flush(void *ctx) {
  (void)ctx;
  if (!ensure_open())
    return false;
  /* FTDI SIO_RESET value1 purges RX. Never reset/bitbang the ECU line. */
  if (cdc_acm_host_send_custom_request(device, 0x40, 0x00, 1, 1, 0, NULL) !=
      ESP_OK) {
    lost();
    return false;
  }
  portENTER_CRITICAL(&rx_mux);
  bk_ring_clear(&rx);
  portEXIT_CRITICAL(&rx_mux);
  atomic_store(&rx_fault, false);
  ulTaskNotifyTake(pdTRUE, 0);
  return atomic_load(&connected) && !atomic_load(&paused) &&
         !atomic_load(&serial_fault);
}
static bool write_bytes(void *ctx, const uint8_t *data, size_t n,
                        unsigned timeout) {
  (void)ctx;
  if (atomic_load(&paused) || !atomic_load(&connected) || !device)
    return false;
  esp_err_t e = cdc_acm_host_data_tx_blocking(device, data, n, timeout);
  if (e != ESP_OK) {
    lost();
    return false;
  }
  return true;
}
static int read_bytes(void *ctx, uint8_t *out, size_t n, unsigned timeout) {
  (void)ctx;
  uint64_t end = now(NULL) + timeout;
  for (;;) {
    if (atomic_load(&paused) || !atomic_load(&connected))
      return -1;
    if (atomic_load(&rx_fault) || atomic_load(&serial_fault))
      return -2;
    portENTER_CRITICAL(&rx_mux);
    size_t got = bk_ring_read(&rx, out, n);
    portEXIT_CRITICAL(&rx_mux);
    if (got) {
      if (atomic_load(&rx_fault) || atomic_load(&serial_fault))
        return -2;
      if (atomic_load(&paused) || !atomic_load(&connected))
        return -1;
      return (int)got;
    }
    uint64_t time = now(NULL);
    if (time >= end)
      return 0;
    TickType_t wait = pdMS_TO_TICKS(end - time);
    ulTaskNotifyTake(pdTRUE, wait ? wait : 1);
  }
}
const mikas_stream_t *bk_backend_stream(void) {
  static const mikas_stream_t stream = {.flush = flush,
                                        .write = write_bytes,
                                        .read = read_bytes,
                                        .now_ms = now,
                                        .delay_ms = delay};
  return &stream;
}

bool bk_backend_ready(void) {
  return atomic_load(&connected) && !atomic_load(&paused);
}
