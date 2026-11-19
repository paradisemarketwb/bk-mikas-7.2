#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Single owner byte stream. USB packet headers must be removed by the VCP
 * driver. read: >=0 byte count, -1 disconnected/cancelled, -2 stream/line
 * error. */
typedef struct {
  void *context;
  bool (*flush)(void *context);
  bool (*write)(void *context, const uint8_t *data, size_t length,
                unsigned timeout_ms);
  int (*read)(void *context, uint8_t *data, size_t capacity,
              unsigned timeout_ms);
  uint64_t (*now_ms)(void *context);
  void (*delay_ms)(void *context, unsigned ms);
} mikas_stream_t;
typedef enum {
  MIKAS_LINK_OK,
  MIKAS_LINK_ARGUMENT,
  MIKAS_LINK_IO,
  MIKAS_LINK_DISCONNECTED,
  MIKAS_LINK_ECHO,
  MIKAS_LINK_FRAME,
  MIKAS_LINK_TIMEOUT
} mikas_link_result_t;
mikas_link_result_t mikas_stream_exchange(const mikas_stream_t *io,
                                          const uint8_t *command,
                                          size_t command_length, uint8_t *reply,
                                          size_t capacity, size_t *reply_length,
                                          unsigned timeout_ms);
