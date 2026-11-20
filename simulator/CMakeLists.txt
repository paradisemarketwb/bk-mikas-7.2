cmake_minimum_required(VERSION 3.20)
project(bk_lvgl_simulator LANGUAGES C CXX)
set(CMAKE_C_STANDARD 11)
include(FetchContent)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
set(LV_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git
 GIT_TAG 80ca777e37a2b176770726a02e07a6fb79ef0b39 GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(lvgl)
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "Use native MSVC flags" FORCE)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "" FORCE)
set(CJSON_OVERRIDE_BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
set(CJSON_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(cjson GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
 GIT_TAG c859b25da02955fef659d658b8f324b5cde87be3 GIT_SHALLOW FALSE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(cjson)
set(BK_COMPONENTS ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/components)
include(${BK_COMPONENTS}/ui_widgets/assets/uaz21/sources.cmake)
add_executable(bk_sim main.c ${BK_UAZ_ASSET_SOURCES}
 ${BK_COMPONENTS}/settings/bk_profile_storage.c
 ${BK_COMPONENTS}/sources/bk_sources.c
 ${BK_COMPONENTS}/settings/bk_profile.c
 ${BK_COMPONENTS}/settings/bk_profile_json.c
 ${BK_COMPONENTS}/ui_config/bk_ui_config.c
 ${BK_COMPONENTS}/ui_core/bk_ui.c
 ${BK_COMPONENTS}/ui_widgets/bk_widgets.c
 ${BK_COMPONENTS}/ui_widgets/bk_font_ru_20.c
 ${BK_COMPONENTS}/ui_widgets/bk_font_reference_64.c
 ${BK_COMPONENTS}/ui_widgets/bk_font_reference_128.c
 ${BK_COMPONENTS}/ui_widgets/bk_font_reference_engine.c
 ${BK_COMPONENTS}/ui_widgets/bk_font_reference_header.c
 ${BK_COMPONENTS}/ui_screens/bk_screens.c)
target_include_directories(bk_sim PRIVATE ${cjson_SOURCE_DIR} ${BK_COMPONENTS}/sources/include ${BK_COMPONENTS}/settings/include ${BK_COMPONENTS}/ui_config/include ${BK_COMPONENTS}/ui_core/include
 ${BK_COMPONENTS}/ui_widgets/include ${BK_COMPONENTS}/ui_widgets/assets/uaz21 ${BK_COMPONENTS}/ui_screens/include)
target_link_libraries(bk_sim PRIVATE lvgl::lvgl cjson)
if(NOT MSVC)
 target_link_libraries(bk_sim PRIVATE m)
 target_compile_options(bk_sim PRIVATE -Wall -Wextra)
endif()
option(BK_REQUIRE_SDL "Fail when interactive SDL2 is missing" OFF)
option(BK_WITH_SDL "Build interactive window when SDL2 is available" ON)
if(BK_WITH_SDL)
 find_package(SDL2 QUIET)
 if(SDL2_FOUND)
  target_link_libraries(bk_sim PRIVATE SDL2::SDL2)
  target_compile_definitions(bk_sim PRIVATE BK_HAVE_SDL=1 SDL_MAIN_HANDLED=1)
 elseif(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
  target_include_directories(bk_sim PRIVATE ${SDL2_INCLUDE_DIR})
  target_link_libraries(bk_sim PRIVATE ${SDL2_LIBRARY})
  target_compile_definitions(bk_sim PRIVATE BK_HAVE_SDL=1 SDL_MAIN_HANDLED=1)
 else()
  if(BK_REQUIRE_SDL)
   message(FATAL_ERROR "SDL2 required. Install sdl2:x64-windows via vcpkg.")
  endif()
  message(STATUS "SDL2 development files unavailable: headless simulator only")
 endif()
endif()
enable_testing()
add_test(NAME render_all_screens COMMAND bk_sim --headless ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME profile_export COMMAND bk_sim --export ${CMAKE_CURRENT_BINARY_DIR}/profile.json --render ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME profile_import COMMAND bk_sim --import ${CMAKE_CURRENT_BINARY_DIR}/profile.json --export ${CMAKE_CURRENT_BINARY_DIR}/profile-roundtrip.json --render ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(profile_import PROPERTIES DEPENDS profile_export)

add_test(NAME profile_exact_roundtrip COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_BINARY_DIR}/profile.json ${CMAKE_CURRENT_BINARY_DIR}/profile-roundtrip.json)
set_tests_properties(profile_exact_roundtrip PROPERTIES DEPENDS profile_import)
add_test(NAME calibrated_profile_export COMMAND bk_sim --import ${CMAKE_CURRENT_SOURCE_DIR}/profiles/reference-calibrated.json --export ${CMAKE_CURRENT_BINARY_DIR}/calibrated-roundtrip.json --render ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME calibrated_profile_exact_roundtrip COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_CURRENT_SOURCE_DIR}/profiles/reference-calibrated.json ${CMAKE_CURRENT_BINARY_DIR}/calibrated-roundtrip.json)
set_tests_properties(calibrated_profile_exact_roundtrip PROPERTIES DEPENDS calibrated_profile_export)

if(BK_WITH_SDL AND (SDL2_FOUND OR (SDL2_INCLUDE_DIR AND SDL2_LIBRARY)))
 add_test(NAME sdl_window_texture_smoke COMMAND bk_sim --reference --smoke-window --no-persist)
 set_tests_properties(sdl_window_texture_smoke PROPERTIES TIMEOUT 20)
endif()
