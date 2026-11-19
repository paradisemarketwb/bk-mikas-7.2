# IDF resolves REQUIRES before sdkconfig is available. Keep dependency discovery
# unconditional, and select only the backend source after Kconfig evaluation.
set(link_sources "bk_kline.c")
if(CONFIG_BK_TRANSPORT_USB_KKL)
 list(APPEND link_sources "bk_usb_kkl.c")
else()
 list(APPEND link_sources "bk_uart.c")
endif()
idf_component_register(SRCS ${link_sources} INCLUDE_DIRS "include"
 REQUIRES mikas esp_timer log esp_driver_uart esp_driver_gpio usb usb_host_ftdi_vcp usb_host_cdc_acm)
