#include "bk_board.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <string.h>
#include "st7701_init.h"
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t expander,touch;
static esp_lcd_panel_handle_t panel;
static spi_device_handle_t spi;
static lv_display_t *display;
static void *draw_buffer;
static uint8_t outputs=0x07; /* LCD reset high, touch reset high, CS high; buzzer low. */
static const char *TAG="bk_board";
static esp_err_t write_reg(i2c_master_dev_handle_t dev,uint8_t reg,uint8_t value){uint8_t b[]={reg,value};return i2c_master_transmit(dev,b,2,30);}
static esp_err_t read_reg(i2c_master_dev_handle_t dev,uint8_t reg,void *data,size_t length){return i2c_master_transmit_receive(dev,&reg,1,data,length,20);}
static esp_err_t exio(unsigned bit,bool high){
 uint8_t next=high?(outputs|(1U<<bit)):(outputs&~(1U<<bit));
 esp_err_t e=write_reg(expander,1,next);if(e==ESP_OK)outputs=next;return e;
}
static esp_err_t spi_byte(bool data,uint8_t value){
 spi_transaction_t transaction={.cmd=data?1:0,.addr=value};
 return spi_device_transmit(spi,&transaction);
}
static esp_err_t lcd_registers(void){
 esp_err_t e=exio(0,false);if(e)return e;vTaskDelay(pdMS_TO_TICKS(10));
 e=exio(0,true);if(e)return e;vTaskDelay(pdMS_TO_TICKS(50));
 e=exio(2,false);if(e)return e;vTaskDelay(pdMS_TO_TICKS(50));
 for(unsigned i=0;i<sizeof(init_commands)/sizeof(init_commands[0]);i++){
  e=spi_byte(false,init_commands[i].command);if(e)return e;
  for(unsigned j=0;j<init_commands[i].length;j++){e=spi_byte(true,init_commands[i].data[j]);if(e)return e;}
  if(init_commands[i].delay_ms)vTaskDelay(pdMS_TO_TICKS(init_commands[i].delay_ms));
 }
 return exio(2,true);
}
static void flush(lv_display_t *d,const lv_area_t *area,uint8_t *pixels){
 /* RGB driver synchronously copies the partial rectangle into its framebuffer.
  * Flush-ready must not precede this copy. Single-FB mode can tear: bench test. */
 esp_err_t e=esp_lcd_panel_draw_bitmap(panel,area->x1,area->y1,area->x2+1,area->y2+1,pixels);
 if(e!=ESP_OK)ESP_LOGE(TAG,"LCD copy: %s",esp_err_to_name(e));
 lv_display_flush_ready(d);
}
static void touch_read(lv_indev_t *indev,lv_indev_data_t *data){
 (void)indev;uint8_t count=0,point[4];data->state=LV_INDEV_STATE_RELEASED;
 if(read_reg(touch,0x02,&count,1)!=ESP_OK || !(count&0x0f))return;
 if((count&0x0f)>2 || read_reg(touch,0x03,point,sizeof(point))!=ESP_OK)return;
 unsigned x=((point[0]&15)<<8)|point[1],y=((point[2]&15)<<8)|point[3];
 if(x>=480||y>=480)return;
 data->point.x=x;data->point.y=y;data->state=LV_INDEV_STATE_PRESSED;
}
static void cleanup(void){
 gpio_set_level(6,0);
 if(display){lv_display_delete(display);display=NULL;}
 if(draw_buffer){heap_caps_free(draw_buffer);draw_buffer=NULL;}
 if(panel){esp_lcd_panel_del(panel);panel=NULL;}
 if(spi){spi_bus_remove_device(spi);spi=NULL;}
 /* SPI and I2C bus handles are retained after failed init; retry is disallowed.
  * This avoids touching unrelated peripherals while diagnosis remains active. */
}
esp_err_t bk_board_start(void){
 static bool attempted;
 if(attempted)return ESP_ERR_INVALID_STATE;
 attempted=true;
 esp_err_t e;
 gpio_config_t backlight={.pin_bit_mask=1ULL<<6,.mode=GPIO_MODE_OUTPUT};
 e=gpio_config(&backlight);if(e)return e;gpio_set_level(6,0);
 i2c_master_bus_config_t bc={.i2c_port=I2C_NUM_0,.sda_io_num=15,.scl_io_num=7,
  .clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,.flags.enable_internal_pullup=true};
 e=i2c_new_master_bus(&bc,&bus);if(e)goto fail;
 i2c_device_config_t dc={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=0x20,.scl_speed_hz=400000};
 e=i2c_master_bus_add_device(bus,&dc,&expander);if(e)goto fail;
 /* Set safe outputs BEFORE enabling output direction. Only LCD/touch reset,
  * LCD CS and buzzer pins are outputs; other expander pins stay inputs. */
 e=write_reg(expander,1,outputs);if(e)goto fail;
 e=write_reg(expander,3,0x78);if(e)goto fail;
 e=exio(1,false);if(e)goto fail;vTaskDelay(pdMS_TO_TICKS(10));
 e=exio(1,true);if(e)goto fail;vTaskDelay(pdMS_TO_TICKS(50));
 dc.device_address=0x15;e=i2c_master_bus_add_device(bus,&dc,&touch);if(e)goto fail;
 /* Touch absent is a board error, rather than displaying an unusable menu. */
 e=i2c_master_probe(bus,0x15,30);if(e)goto fail;
 spi_bus_config_t sc={.mosi_io_num=1,.miso_io_num=-1,.sclk_io_num=2,
  .quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=64};
 e=spi_bus_initialize(SPI2_HOST,&sc,SPI_DMA_CH_AUTO);if(e)goto fail;
 spi_device_interface_config_t sp={.command_bits=1,.address_bits=8,.mode=0,
  .clock_speed_hz=10000000,.spics_io_num=-1,.queue_size=1};
 e=spi_bus_add_device(SPI2_HOST,&sp,&spi);if(e)goto fail;
 e=lcd_registers();if(e)goto fail;
 esp_lcd_rgb_panel_config_t pc={
  .clk_src=LCD_CLK_SRC_DEFAULT,.data_width=16,.bits_per_pixel=16,.num_fbs=1,
  .bounce_buffer_size_px=480*10,.dma_burst_size=64,
  .hsync_gpio_num=38,.vsync_gpio_num=39,.de_gpio_num=40,.pclk_gpio_num=41,.disp_gpio_num=-1,
  .data_gpio_nums={5,45,48,47,21,14,13,12,11,10,9,46,3,8,18,17},
  .timings={.pclk_hz=16000000,.h_res=480,.v_res=480,
   .hsync_back_porch=10,.hsync_front_porch=50,.hsync_pulse_width=8,
   .vsync_back_porch=8,.vsync_front_porch=8,.vsync_pulse_width=3,
   .flags.pclk_active_neg=false},.flags.fb_in_psram=true};
 e=esp_lcd_new_rgb_panel(&pc,&panel);if(e)goto fail;
 e=esp_lcd_panel_reset(panel);if(e)goto fail;
 e=esp_lcd_panel_init(panel);if(e)goto fail;
 draw_buffer=heap_caps_aligned_alloc(64,480*40*2,MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
 if(!draw_buffer){e=ESP_ERR_NO_MEM;goto fail;}
 lv_init();display=lv_display_create(480,480);if(!display){e=ESP_ERR_NO_MEM;goto fail;}
 lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
 lv_display_set_buffers(display,draw_buffer,NULL,480*40*2,LV_DISPLAY_RENDER_MODE_PARTIAL);
 lv_display_set_flush_cb(display,flush);
 lv_indev_t *input=lv_indev_create();if(!input){e=ESP_ERR_NO_MEM;goto fail;}
 lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,touch_read);
 lv_indev_set_display(input,display);
 gpio_set_level(6,1);
 ESP_LOGI(TAG,"Waveshare LCD 480x480 RGB565 16MHz + CST820 initialized; hardware validation pending");
 return ESP_OK;
fail:
 ESP_LOGE(TAG,"Display init failed: %s",esp_err_to_name(e));cleanup();return e;
}
