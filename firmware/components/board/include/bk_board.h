#ifndef BK_BOARD_H
#define BK_BOARD_H
#include "esp_err.h"
/* Start display/touch, then call the UI only from its owning task.
 * Initialization failure keeps the backlight off; no automatic retries. */
esp_err_t bk_board_start(void);
#endif
