include(${CMAKE_CURRENT_LIST_DIR}/assets/uaz21/sources.cmake)
idf_component_register(SRCS ${BK_UAZ_ASSET_SOURCES} "bk_widgets.c" "bk_font_ru_20.c" "bk_font_reference_64.c" "bk_font_reference_128.c" "bk_font_reference_engine.c" "bk_font_reference_header.c"
 INCLUDE_DIRS "include" "assets/uaz21" REQUIRES lvgl ui_config)
