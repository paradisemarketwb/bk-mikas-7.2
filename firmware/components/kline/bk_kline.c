#include "bk_kline.h"
#include "mikas.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#define PORT UART_NUM_1
#define ARM GPIO_NUM_19
void bk_kline_disarm(void){gpio_set_level(ARM,0);}
esp_err_t bk_kline_init(void){
 gpio_set_level(ARM,0);gpio_set_direction(ARM,GPIO_MODE_OUTPUT);
 uart_config_t cfg={.baud_rate=9600,.data_bits=UART_DATA_8_BITS,.parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};
 esp_err_t e=uart_param_config(PORT,&cfg);if(e!=ESP_OK)return e;
 e=uart_set_pin(PORT,43,44,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);if(e!=ESP_OK)return e;
 e=uart_driver_install(PORT,2048,0,0,NULL,0);if(e!=ESP_OK)return e;
 gpio_set_level(ARM,1);return ESP_OK;
}
esp_err_t bk_kline_exchange(const uint8_t *cmd,size_t n,uint8_t *reply,size_t cap,size_t *out,unsigned timeout){
 if(out)*out=0;
 if(!reply||!out||!mikas_read_allowed(cmd,n))return ESP_ERR_INVALID_ARG;
 uint8_t tx[MIKAS_MAX_WIRE],echo[MIKAS_MAX_WIRE],rx[MIKAS_MAX_WIRE];
 size_t len=mikas_encode(cmd,n,tx,sizeof(tx));if(!len)return ESP_ERR_INVALID_SIZE;
 vTaskDelay(pdMS_TO_TICKS(50));uart_flush_input(PORT);
 int64_t deadline=esp_timer_get_time()+(int64_t)timeout*1000;
 if(uart_write_bytes(PORT,tx,len)!=(int)len)return ESP_FAIL;
 if(uart_wait_tx_done(PORT,pdMS_TO_TICKS(timeout))!=ESP_OK)return ESP_ERR_TIMEOUT;
 size_t have=0;
 while(have<len&&esp_timer_get_time()<deadline){int r=uart_read_bytes(PORT,echo+have,len-have,pdMS_TO_TICKS(10));if(r>0)have+=(size_t)r;}
 if(have!=len||memcmp(tx,echo,len))return ESP_ERR_INVALID_RESPONSE;
 have=0;
 while(have<sizeof(rx)&&esp_timer_get_time()<deadline){
  int r=uart_read_bytes(PORT,rx+have,1,pdMS_TO_TICKS(10));if(r<=0)continue;
  if(rx[have++]==0x0d)return mikas_decode(rx,have,reply,cap,out)?ESP_OK:ESP_ERR_INVALID_RESPONSE;
 }
 return ESP_ERR_TIMEOUT;
}
