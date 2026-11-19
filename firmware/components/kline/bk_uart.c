#include "bk_link_backend.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#define PORT UART_NUM_1
#define ARM GPIO_NUM_19
static atomic_bool armed;
void bk_backend_disarm(void) {
  atomic_store(&armed, false);
  gpio_set_level(ARM, 0);
}
esp_err_t bk_backend_init(void) {
  gpio_set_level(ARM, 0);
  gpio_set_direction(ARM, GPIO_MODE_OUTPUT);
  uart_config_t cfg = {.baud_rate = 9600,
                       .data_bits = UART_DATA_8_BITS,
                       .parity = UART_PARITY_DISABLE,
                       .stop_bits = UART_STOP_BITS_1,
                       .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                       .source_clk = UART_SCLK_DEFAULT};
  esp_err_t e = uart_param_config(PORT, &cfg);
  if (e != ESP_OK)
    return e;
  e = uart_set_pin(PORT, 43, 44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (e != ESP_OK)
    return e;
  e = uart_driver_install(PORT, 2048, 0, 0, NULL, 0);
  if (e != ESP_OK)
    return e;
  gpio_set_level(ARM, 1);
  atomic_store(&armed, true);
  return ESP_OK;
}
static bool flush(void *ctx) {
  (void)ctx;
  return atomic_load(&armed) && uart_flush_input(PORT) == ESP_OK;
}
static bool write_bytes(void *ctx, const uint8_t *data, size_t n,
                        unsigned timeout) {
  (void)ctx;
  return atomic_load(&armed) && uart_write_bytes(PORT, data, n) == (int)n &&
         uart_wait_tx_done(PORT, pdMS_TO_TICKS(timeout)) == ESP_OK;
}
static int read_bytes(void *ctx, uint8_t *data, size_t n, unsigned timeout) {
  (void)ctx;
  if (!atomic_load(&armed))
    return -1;
  return uart_read_bytes(PORT, data, n, pdMS_TO_TICKS(timeout));
}
static uint64_t now(void *ctx) {
  (void)ctx;
  return esp_timer_get_time() / 1000;
}
static void delay(void *ctx, unsigned ms) {
  (void)ctx;
  vTaskDelay(pdMS_TO_TICKS(ms));
}
const mikas_stream_t *bk_backend_stream(void) {
  static const mikas_stream_t stream = {.flush = flush,
                                        .write = write_bytes,
                                        .read = read_bytes,
                                        .now_ms = now,
                                        .delay_ms = delay};
  return &stream;
}

bool bk_backend_ready(void) { return atomic_load(&armed); }
