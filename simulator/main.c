#include "bk_settings.h"
#include "bk_uaz_fonts.h"
#include "bk_uaz_layout.h"
#include "bk_ui.h"
#include "bk_widgets.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef BK_HAVE_SDL
#include <SDL.h>
#endif
/* All values in this executable are explicitly simulated. No production source
 * is impersonated: the external adapter publishes BK_SOURCE_SIMULATION. */
static uint16_t framebuffer[480 * 480], draw_buffer[480 * 40];
static bk_settings_t profile_seed;
static bk_sources_t sources;
static bool connected = true, gps = true, paused;
static double rpm = 2450, temp = 87, voltage = 13.9, throttle = 18, fuel = 4.1;
static uint8_t dtc_codes[] = {0x12, 0x34, 0x56};
static size_t dtc_count;
static int mouse_x, mouse_y;
static bool mouse_down;
static const char *save_path = "hunter-profile.json";
static double frame_total_ms, frame_max_ms;
static unsigned frames;
static void flush(lv_display_t *d, const lv_area_t *area, uint8_t *pixels) {
  const uint16_t *src = (const uint16_t *)pixels;
  for (int y = area->y1; y <= area->y2; y++) {
    memcpy(framebuffer + y * 480 + area->x1, src,
           (size_t)(area->x2 - area->x1 + 1) * 2);
    src += area->x2 - area->x1 + 1;
  }
  lv_display_flush_ready(d);
}
static void pointer_read(lv_indev_t *i, lv_indev_data_t *data) {
  (void)i;
  data->point.x = mouse_x;
  data->point.y = mouse_y;
  data->state = mouse_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void tick(lv_indev_t *pointer, unsigned ms) {
  lv_tick_inc(ms);
  if (pointer)
    lv_indev_read(pointer);
  lv_timer_handler();
}
static void swipe(lv_indev_t *p, int x, int y, int ex, int ey) {
  mouse_x = x;
  mouse_y = y;
  mouse_down = false;
  for (unsigned i = 0; i < 3; i++)
    tick(p, 20);
  mouse_down = true;
  tick(p, 20);
  for (int i = 1; i <= 8; i++) {
    mouse_x = x + (ex - x) * i / 8;
    mouse_y = y + (ey - y) * i / 8;
    tick(p, 20);
  }
  mouse_down = false;
  for (unsigned i = 0; i < 4; i++)
    tick(p, 20);
}
static const char *demo_dtc(uint8_t code) {
  switch (code) {
  case 0x12:
    return "Симуляция: датчик температуры";
  case 0x34:
    return "Симуляция: датчик кислорода";
  default:
    return "Симуляция диагностического события";
  }
}
static void fixture_calibrations(void) {
  bk_calibration_defaults(&profile_seed.maf, BK_CAL_MAF);
  profile_seed.maf.configured = true;
  profile_seed.maf.method = BK_CAL_LINEAR;
  profile_seed.maf.k = .1;
  profile_seed.maf.unit = BK_UNIT_G_S;
  bk_calibration_defaults(&profile_seed.lambda, BK_CAL_LAMBDA);
  profile_seed.lambda.configured = true;
  profile_seed.lambda.method = BK_CAL_LINEAR;
  profile_seed.lambda.divisor = 255;
  profile_seed.lambda.lambda_kind = BK_LAMBDA_NARROWBAND;
}
static bk_source_selection_t simulated_speed(void) {
  if (gps) {
    bk_source_config_t c = {.source = BK_SOURCE_SIMULATION,
                            .fresh_ms = 1500,
                            .confirm_samples = 1,
                            .minimum = 0,
                            .maximum = 240,
                            .unit = BK_UNIT_NATIVE};
    snprintf(c.metric, sizeof(c.metric), "vehicle.speed");
    bk_sources_register(&sources, &c);
    bk_sources_publish_simulation(&sources, "vehicle.speed", 64, BK_UNIT_NATIVE,
                                  lv_tick_get());
  } else
    bk_sources_remove(&sources, "vehicle.speed", BK_SOURCE_SIMULATION);
  bk_source_policy_t policy = {.allow_simulation = true};
  return bk_sources_select(&sources, "vehicle.speed", &policy, lv_tick_get());
}
static void sample(void) {
  bk_ui_set_connected(connected);
  bk_value_state_t state = connected ? BK_VALUE_VALID : BK_VALUE_STALE;
  bk_ui_set_sample(BK_RPM, rpm, state);
  bk_ui_set_sample(BK_COOLANT, temp, state);
  bk_ui_set_sample(BK_VOLTAGE, voltage, state);
  bk_ui_set_sample(BK_THROTTLE, throttle, state);
  bk_ui_set_sample(BK_FUEL_LPH, fuel, state);
  bk_ui_set_sample(BK_IGNITION, 12.5, state);
  bk_ui_set_sample(BK_INJECTION, 4.2, state);
  bk_ui_set_sample(BK_INTAKE, 23, state);
  bk_ui_set_sample(BK_DTC_COUNT, (double)dtc_count, state);
  bk_ui_set_dtcs(dtc_codes, dtc_count, connected);
  bk_ui_set_sample(BK_MAF_RAW, 123, state);
  bk_ui_set_sample(BK_LAMBDA, 222, state);
  double value = 0;
  bk_calibration_result_t r =
      bk_calibration_evaluate(&profile_seed.maf, 123, BK_UNIT_KG_H, &value);
  bk_ui_set_sample(BK_MAF, value,
                   !connected       ? BK_VALUE_STALE
                   : r == BK_CAL_OK ? BK_VALUE_VALID
                                    : BK_VALUE_NEEDS_CALIBRATION);
  r = bk_calibration_evaluate(&profile_seed.lambda, 222, BK_UNIT_V, &value);
  bk_ui_set_sample(BK_LAMBDA_VOLTAGE, value,
                   !connected       ? BK_VALUE_STALE
                   : r == BK_CAL_OK ? BK_VALUE_VALID
                                    : BK_VALUE_NEEDS_CALIBRATION);
  bk_source_selection_t speed = simulated_speed();
  bool available = speed.status == BK_SOURCE_VALID;
  bk_ui_set_sample(BK_SPEED, speed.sample.value,
                   available ? BK_VALUE_VALID : BK_VALUE_NO_SIGNAL);
  bk_ui_set_sample(BK_RANGE, 328,
                   gps ? BK_VALUE_VALID : BK_VALUE_REQUIRES_SENSOR);
  bk_ui_set_sample(BK_DISTANCE, 56.8,
                   gps ? BK_VALUE_VALID : BK_VALUE_NO_SIGNAL);
  bk_ui_set_sample(BK_AVG_SPEED, 40.6,
                   gps ? BK_VALUE_VALID : BK_VALUE_NO_SIGNAL);
  bk_ui_set_sample(BK_FUEL_L100, 12.4,
                   gps ? BK_VALUE_VALID : BK_VALUE_INSUFFICIENT);
  bk_ui_set_sample(BK_FUEL_LEVEL, 66,
                   gps ? BK_VALUE_VALID : BK_VALUE_REQUIRES_SENSOR);
  bk_ui_set_sample(BK_TRIP_SECONDS, 5040, BK_VALUE_VALID);
  bk_ui_set_clock("12:34");
}
static int ppm(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    perror(path);
    return 1;
  }
  fprintf(f, "P6\n480 480\n255\n");
  for (unsigned i = 0; i < 480 * 480; i++) {
    uint16_t c = framebuffer[i];
    uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31),
                      (uint8_t)(((c >> 5) & 63) * 255 / 63),
                      (uint8_t)((c & 31) * 255 / 31)};
    if (fwrite(rgb, 1, 3, f) != 3) {
      fclose(f);
      return 1;
    }
  }
  return fclose(f) != 0;
}
static void render(lv_display_t *d) {
  clock_t begin = clock();
  for (unsigned i = 0; i < 10; i++)
    tick(NULL, 20);
  lv_refr_now(d);
  double ms = 1000.0 * (clock() - begin) / CLOCKS_PER_SEC;
  frame_total_ms += ms;
  if (ms > frame_max_ms)
    frame_max_ms = ms;
  frames++;
}
static bool import_profile(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    perror(path);
    return false;
  }
  char *json = malloc(32770);
  if (!json) {
    fclose(f);
    return false;
  }
  size_t n = fread(json, 1, 32769, f);
  bool io = ferror(f) != 0;
  fclose(f);
  if (io || n > 32768) {
    free(json);
    return false;
  }
  json[n] = 0;
  bk_settings_t *s = malloc(sizeof(*s));
  if (!s) {
    free(json);
    return false;
  }
  bool valid = bk_settings_from_json(json, s);
  free(json);
  if (!valid) {
    fprintf(stderr, "Invalid profile: no UI changes\n");
    free(s);
    return false;
  }
  bk_ui_config_t c = {
      .screen = s->screen, .dark = s->dark, .brightness = s->brightness};
  for (unsigned i = 0; i < 5; i++)
    c.selected[i] = s->selected[i];
  valid = bk_ui_set_config(&c) && bk_ui_set_layout(&s->layout);
  if (valid)
    profile_seed = *s;
  free(s);
  sample();
  return valid;
}
static bool export_profile(const char *path) {
  /* Preserve calibration and source policies imported from the same full
   * profile. */
  bk_settings_t *s = malloc(sizeof(*s));
  if (!s)
    return false;
  *s = profile_seed;
  bk_ui_config_t c;
  bk_ui_get_config(&c);
  s->screen = c.screen;
  s->dark = c.dark;
  s->brightness = c.brightness;
  bk_ui_get_layout(&s->layout);
  /* Legacy selected[] is redundant in v4. Preserve imported bytes when only
   * rendering; update it after an actual layout edit. */
  if (memcmp(&profile_seed.layout, &s->layout, sizeof(s->layout)))
    for (unsigned i = 0; i < 5; i++)
      s->selected[i] = c.selected[i];
  char *json = bk_settings_to_json(s);
  free(s);
  if (!json)
    return false;
  size_t pathlen = strlen(path);
  char *temporary = malloc(pathlen + 5);
  if (!temporary) {
    cJSON_free(json);
    return false;
  }
  snprintf(temporary, pathlen + 5, "%s.tmp", path);
  FILE *f = fopen(temporary, "wb");
  if (!f) {
    perror(temporary);
    free(temporary);
    cJSON_free(json);
    return false;
  }
  size_t n = strlen(json);
  bool ok = fwrite(json, 1, n, f) == n;
  if (fclose(f))
    ok = false;
  cJSON_free(json);
  if (ok) {
#ifdef _WIN32
    /* Windows rename cannot replace an existing target; the temporary file was
     * fully written first. */
    remove(path);
#endif
    ok = rename(temporary, path) == 0;
  }
  if (!ok)
    perror(path);
  free(temporary);
  return ok;
}
static void keyboard_navigation(bool vertical, int direction) {
  bk_layout_t layout;
  bk_ui_get_layout(&layout);
  int slot = bk_layout_view_to_screen(bk_ui_screen());
  if (slot < 0)
    return;
  if (vertical) {
    for (unsigned i = 0; i < layout.screen_count; i++) {
      slot = (slot + direction + layout.screen_count) % layout.screen_count;
      if (layout.screens[slot].enabled) {
        bk_ui_select_screen(bk_layout_screen_to_view(slot));
        break;
      }
    }
  } else {
    bk_layout_step_primary(&layout.screens[slot], direction);
    bk_ui_set_layout(&layout);
  }
}
#ifdef BK_HAVE_SDL
static int interactive(bool smoke) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)) {
    fprintf(stderr, "%s\n", SDL_GetError());
    return 1;
  }
  SDL_Window *window = SDL_CreateWindow(
      "HUNTER SIMULATION | arrows: gestures | 1 Classic / 2 Reference | E edit",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 480, 480,
      smoke ? SDL_WINDOW_HIDDEN : 0);
  SDL_Renderer *renderer =
      window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : NULL;
  SDL_Texture *texture =
      renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                   SDL_TEXTUREACCESS_STREAMING, 480, 480)
               : NULL;
  if (!texture) {
    fprintf(stderr, "%s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }
  lv_indev_t *pointer = lv_indev_create();
  lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(pointer, pointer_read);
  bk_ui_config_t initial_config;
  bk_ui_get_config(&initial_config);
  bool running = true, dark = initial_config.dark, editing = false;
  unsigned smoke_frames = 0;
  uint32_t previous = SDL_GetTicks(), last = previous;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT)
        running = false;
      if (e.type == SDL_MOUSEMOTION) {
        mouse_x = e.motion.x;
        mouse_y = e.motion.y;
      }
      if (e.type == SDL_MOUSEBUTTONDOWN) {
        mouse_x = e.button.x;
        mouse_y = e.button.y;
        mouse_down = true;
      }
      if (e.type == SDL_MOUSEBUTTONUP) {
        mouse_x = e.button.x;
        mouse_y = e.button.y;
        mouse_down = false;
      }
      if (e.type == SDL_KEYDOWN) {
        bool ctrl = (e.key.keysym.mod & KMOD_CTRL) != 0;
        switch (e.key.keysym.sym) {
        case SDLK_ESCAPE:
          if (editing) {
            bk_ui_end_edit(false);
            editing = false;
          } else
            running = false;
          break;
        case SDLK_UP:
          keyboard_navigation(true, 1);
          break;
        case SDLK_DOWN:
          keyboard_navigation(true, -1);
          break;
        case SDLK_LEFT:
          keyboard_navigation(false, -1);
          break;
        case SDLK_RIGHT:
          keyboard_navigation(false, 1);
          break;
        case SDLK_1:
        case SDLK_2: {
          bk_layout_t layout;
          if (e.key.keysym.sym == SDLK_1)
            bk_layout_defaults(&layout);
          else
            bk_layout_reference_defaults(&layout);
          bk_ui_set_layout(&layout);
          bk_ui_select_screen(0);
          break;
        }
        case SDLK_t:
          dark = !dark;
          bk_ui_set_theme(dark);
          break;
        case SDLK_c:
          connected = !connected;
          sample();
          break;
        case SDLK_g:
          gps = !gps;
          sample();
          break;
        case SDLK_m:
          profile_seed.maf.configured = !profile_seed.maf.configured;
          if (profile_seed.maf.configured) {
            profile_seed.maf.method = BK_CAL_LINEAR;
            profile_seed.maf.k = .1;
            profile_seed.maf.unit = BK_UNIT_G_S;
          }
          sample();
          break;
        case SDLK_l:
          profile_seed.lambda.configured = !profile_seed.lambda.configured;
          if (profile_seed.lambda.configured) {
            profile_seed.lambda.method = BK_CAL_LINEAR;
            profile_seed.lambda.divisor = 255;
            profile_seed.lambda.lambda_kind = BK_LAMBDA_NARROWBAND;
          }
          sample();
          break;
        case SDLK_k:
          profile_seed.maf.k *= 1.1;
          sample();
          break;
        case SDLK_j:
          profile_seed.maf.k /= 1.1;
          sample();
          break;
        case SDLK_e:
          bk_ui_begin_edit();
          editing = true;
          break;
        case SDLK_RETURN:
          bk_ui_end_edit(true);
          editing = false;
          break;
        case SDLK_r:
          bk_ui_reset_screen();
          break;
        case SDLK_q:
          bk_ui_select_screen(5);
          break;
        case SDLK_F12:
          ppm("hunter-screenshot.ppm");
          break;
        case SDLK_SPACE:
          paused = !paused;
          break;
        case SDLK_PLUS:
        case SDLK_EQUALS:
          rpm += 200;
          sample();
          break;
        case SDLK_MINUS:
          rpm = rpm > 200 ? rpm - 200 : 0;
          sample();
          break;
        case SDLK_w:
          temp += 2;
          sample();
          break;
        case SDLK_s:
          if (ctrl)
            export_profile(save_path);
          else {
            temp -= 2;
            sample();
          }
          break;
        case SDLK_v:
          voltage = voltage > 12 ? 11.5 : 13.9;
          sample();
          break;
        case SDLK_d:
          dtc_count = dtc_count ? 0 : 3;
          sample();
          bk_ui_select_screen(7);
          break;
        default:
          break;
        }
      }
    }
    uint32_t now = SDL_GetTicks();
    lv_tick_inc(now - previous);
    previous = now;
    if (!paused && now - last >= 500) {
      last = now;
      sample();
    }
    lv_timer_handler();
    SDL_UpdateTexture(texture, NULL, framebuffer, 480 * 2);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    if (smoke && ++smoke_frames >= 8)
      running = false;
    SDL_Delay(10);
  }
  bk_ui_end_edit(false);
  lv_indev_delete(pointer);
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
#endif
static lv_obj_t *find_class(lv_obj_t *o, const lv_obj_class_t *type) {
  if (lv_obj_check_type(o, type))
    return o;
  for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) {
    lv_obj_t *found = find_class(lv_obj_get_child(o, i), type);
    if (found)
      return found;
  }
  return NULL;
}
static bool contains_label(lv_obj_t *o, const char *text) {
  if (lv_obj_check_type(o, &lv_label_class) &&
      strstr(lv_label_get_text(o), text))
    return true;
  for (unsigned i = 0; i < lv_obj_get_child_count(o); i++)
    if (contains_label(lv_obj_get_child(o, i), text))
      return true;
  return false;
}
#define CHECK(test, message)                                                   \
  do {                                                                         \
    if (!(test)) {                                                             \
      fprintf(stderr, "FAIL: %s\n", message);                                  \
      return 1;                                                                \
    }                                                                          \
  } while (0)
static int run_checks(lv_display_t *display, const char *dir) {
  char path[1024];
  bk_layout_t classic, reference, readback;
  bk_layout_defaults(&classic);
  bk_layout_reference_defaults(&reference);
  CHECK(bk_layout_validate(&classic), "Classic defaults valid");
  CHECK(bk_layout_validate(&reference), "Reference defaults valid");
  /* Regression guard: refresh must preserve the right-hand pill and the
   * Russian left caption through values and missing-source transitions. */
  bk_widget_config_t digital = reference.screens[3].widgets[0];
  bk_widget_t probe;
  bk_widget_create_style(&probe, lv_screen_active(), &digital, false,
                         BK_STYLE_REFERENCE);
  for (unsigned i = 0; i < 8; i++) {
    bk_widget_update(&probe, 44.28 + i,
                     i == 3 ? BK_VALUE_NO_SIGNAL : BK_VALUE_VALID, true,
                     "Расход воздуха", "кг/ч");
    lv_obj_update_layout(probe.root);
    CHECK(lv_obj_get_x(probe.backdrop) == digital.w * 185 / 345,
          "DIGITAL pill stays right on refresh");
    CHECK(lv_obj_get_x(probe.value) == digital.w * 185 / 345 + 8 &&
              lv_obj_is_hidden(probe.unit),
          "DIGITAL value and unit stay in pill");
    CHECK(!strcmp(lv_label_get_text(probe.chart), "ДМРВ") &&
              lv_obj_get_x(probe.chart) == 0,
          "DIGITAL left caption survives refresh");
  }
  lv_obj_delete(probe.root);
  CHECK(reference.screens[0].widgets[0].metric == BK_RPM &&
            !reference.screens[0].widgets[0].primary,
        "Reference arc is auxiliary RPM, independent of speed");
  bk_widget_create_style(&probe, lv_screen_active(),
                         &reference.screens[0].widgets[0], false,
                         BK_STYLE_REFERENCE);
  bk_widget_update(&probe, 3000, BK_VALUE_VALID, true, "Обороты", "об/мин");
  for (unsigned i = 0; i < 12; i++)
    tick(NULL, 20);
  CHECK(lv_arc_get_value(probe.chart) == 500,
        "3000 RPM renders half of 0..6000 RPM arc");
  lv_obj_delete(probe.root);

  bk_widget_create_style(&probe, lv_screen_active(),
                         &reference.screens[2].widgets[2], false,
                         BK_STYLE_REFERENCE);
  bk_widget_update(&probe, 0, BK_VALUE_REQUIRES_SENSOR, true, "Уровень топлива",
                   "%");
  for (unsigned i = 0; i < 6; i++)
    CHECK(lv_obj_is_hidden(probe.segments[i]),
          "Missing fuel source cannot look like an empty measured tank");
  CHECK(strstr(lv_label_get_text(probe.value), "датчика"),
        "Missing tank sensor shows explicit state");
  bk_widget_update(&probe, 66, BK_VALUE_VALID, true, "Уровень топлива", "%");
  for (unsigned i = 0; i < 6; i++)
    CHECK(!lv_obj_is_hidden(probe.segments[i]),
          "Fuel segments restore when source returns");
  lv_obj_delete(probe.root);
  lv_font_glyph_dsc_t glyph;
  CHECK(lv_font_get_glyph_dsc(&bk_uaz_font_54, &glyph, 'r', 0),
        "Custom/raw units retain readable glyphs through text-font fallback");
  fixture_calibrations();
  gps = true;
  connected = true;
  sample();
  bk_ui_set_wifi("HUNTER-SIMULATION", "SIMULATOR-ONLY", "http://192.168.4.1");
  for (unsigned style = 0; style < 2; style++)
    for (unsigned dark = 0; dark < 2; dark++) {
      CHECK(bk_ui_set_layout(style ? &reference : &classic), "Apply style");
      bk_ui_set_theme(dark != 0);
      for (unsigned view = 0; view < 10; view++) {
        if (view == 7) {
          dtc_count = 3;
          sample();
        }
        bk_ui_select_screen(view);
        render(display);
        snprintf(path, sizeof(path), "%s/%sscreen-%02u-%s.ppm", dir,
                 style ? "reference-" : "", view, dark ? "dark" : "light");
        CHECK(!ppm(path), "Framebuffer export");
      }
    }
  bk_ui_set_layout(&classic);
  bk_ui_set_theme(false);
  bk_ui_select_screen(1);
  sample();
  lv_indev_t *pointer = lv_indev_create();
  lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(pointer, pointer_read);
  swipe(pointer, 240, 300, 240, 150);
  CHECK(bk_ui_screen() == 2, "Vertical swipe next screen");
  swipe(pointer, 240, 150, 240, 300);
  CHECK(bk_ui_screen() == 1, "Vertical swipe previous screen");
  bk_ui_get_layout(&readback);
  unsigned index = readback.screens[1].sequence_index;
  swipe(pointer, 330, 200, 150, 200);
  bk_ui_get_layout(&readback);
  CHECK(bk_ui_screen() == 1, "Horizontal swipe stays on screen");
  CHECK(readback.screens[1].sequence_index != index,
        "Horizontal swipe switches primary");
  CHECK(bk_ui_set_layout(&classic), "Reset baseline");
  bk_ui_select_screen(1);
  bk_ui_select_screen(8);
  swipe(pointer, 240, 195, 240, 195);
  swipe(pointer, 240, 195, 240, 195);
  render(display);
  snprintf(path, sizeof(path), "%s/widget-editor.ppm", dir);
  CHECK(!ppm(path), "Widget editor screenshot");
  bk_ui_select_screen(1);
  bk_ui_begin_edit();
  swipe(pointer, 100, 320, 108, 304);
  bk_ui_get_layout(&readback);
  CHECK(!memcmp(&readback.screens[1], &classic.screens[1],
                sizeof(bk_layout_screen_t)),
        "Draft drag not persisted before save");
  render(display);
  snprintf(path, sizeof(path), "%s/layout-editor.ppm", dir);
  CHECK(!ppm(path), "Editor screenshot");
  bk_ui_end_edit(false);
  bk_ui_get_layout(&readback);
  CHECK(!memcmp(&readback.screens[1], &classic.screens[1],
                sizeof(bk_layout_screen_t)),
        "Cancel drag restores original");
  bk_ui_begin_edit();
  swipe(pointer, 100, 320, 108, 304);
  bk_ui_end_edit(true);
  bk_ui_get_layout(&readback);
  CHECK(readback.screens[1].widgets[2].x != classic.screens[1].widgets[2].x,
        "Save drag applies position");
  CHECK(bk_layout_validate(&readback), "Dragged layout inside circle");
  bk_ui_reset_screen();
  bk_ui_get_layout(&readback);
  CHECK(!memcmp(&readback.screens[1], &classic.screens[1],
                sizeof(bk_layout_screen_t)),
        "Reset single screen");
  gps = false;
  sample();
  CHECK(simulated_speed().status != BK_SOURCE_VALID, "Source disappearance");
  gps = true;
  sample();
  CHECK(simulated_speed().status == BK_SOURCE_VALID, "Source reappearance");
  double calculated;
  CHECK(bk_calibration_evaluate(&profile_seed.maf, 123, BK_UNIT_KG_H,
                                &calculated) == BK_CAL_OK &&
            fabs(calculated - 44.28) < .01,
        "MAF user calibration");
  profile_seed.maf.k *= 2;
  CHECK(bk_calibration_evaluate(&profile_seed.maf, 123, BK_UNIT_KG_H,
                                &calculated) == BK_CAL_OK &&
            fabs(calculated - 88.56) < .01,
        "Changed MAF coefficient applied");
  profile_seed.maf.k /= 2;
  CHECK(bk_calibration_evaluate(&profile_seed.lambda, 222, BK_UNIT_V,
                                &calculated) == BK_CAL_OK &&
            fabs(calculated - .870588) < .001,
        "Lambda user calibration");
  CHECK(bk_ui_set_layout(&reference),
        "Assets reference for real pointer actions");
  bk_ui_set_theme(false);
  bk_ui_select_screen(8);
  bk_ui_config_t prior, next;
  bk_ui_get_config(&prior);
  swipe(pointer, 350, 248, 350, 248);
  bk_ui_get_config(&next);
  CHECK(next.dark != prior.dark, "Reference theme row remains functional");
  swipe(pointer, 350, 248, 350, 248);
  bk_ui_select_screen(8);
  swipe(pointer, 350, 166, 350, 166);
  CHECK(find_class(lv_screen_active(), &lv_slider_class) != NULL,
        "Reference brightness row opens real slider");
  bk_ui_get_config(&prior);
  swipe(pointer, 120, 254, 360, 254);
  bk_ui_get_config(&next);
  CHECK(next.brightness != prior.brightness,
        "Reference brightness slider changes saved config");
  swipe(pointer, 240, 461, 240, 461);
  CHECK(find_class(lv_screen_active(), &lv_slider_class) == NULL,
        "Brightness back returns to settings rows");
  swipe(pointer, 240, 397, 240, 397);
  swipe(pointer, 220, 141, 220, 141);
  CHECK(contains_label(lv_screen_active(), "РЕДАКТОР ЭКРАНА"),
        "Reference More still reaches screen constructor");
  CHECK(bk_ui_set_layout(&reference), "Leave constructor safely");
  bk_ui_select_screen(7);
  sample();
  swipe(pointer, 240, 178, 240, 178);
  CHECK(contains_label(lv_screen_active(), "Код ЭБУ 0x12"),
        "Reference real numeric DTC row opens details");
  swipe(pointer, 240, 461, 240, 461);
  CHECK(!contains_label(lv_screen_active(), "Код ЭБУ 0x12"),
        "DTC back returns to list");
  bk_ui_select_screen(6);
  render(display);
  lv_obj_t *qr = find_class(lv_screen_active(), &lv_qrcode_class);
  CHECK(qr && lv_obj_get_width(qr) == 264,
        "Real Wi-Fi QR uses archive264px allocation");
  bk_layout_t custom = classic;
  custom.screen_count = 1;
  custom.screens[0].widget_count = 1;
  bk_widget_config_t *w = &custom.screens[0].widgets[0];
  *w = (bk_widget_config_t){.metric = BK_RPM,
                            .x = 100,
                            .y = 120,
                            .w = 280,
                            .h = 230,
                            .min = 0,
                            .max = 6000,
                            .color = 0x126b48,
                            .visible = true,
                            .font_size = 48};
  for (unsigned kind = 0; kind < BK_WIDGET_COUNT; kind++) {
    w->type = kind;
    CHECK(bk_ui_set_layout(&custom), "Widget layout");
    bk_ui_select_screen(0);
    for (unsigned k = 0; k < 70; k++) {
      bk_ui_set_sample(BK_RPM, 1000 + (k % 10) * 350, BK_VALUE_VALID);
      tick(NULL, 1000);
    }
    render(display);
    snprintf(path, sizeof(path), "%s/widget-%02u.ppm", dir, kind);
    CHECK(!ppm(path), "Widget render");
  }
  CHECK(bk_widget_history_count(BK_RPM) == BK_HISTORY_POINTS,
        "Bounded history");
  w->type = BK_WIDGET_NUMBER;
  w->x = 0;
  w->y = 0;
  CHECK(!bk_ui_set_layout(&custom), "Off-circle rejected");
  CHECK(bk_ui_set_layout(&classic), "Restore Classic");
  bk_ui_select_screen(1);
  bk_ui_boot_start(1);
  CHECK(bk_ui_screen() == 1, "No boot page or delay");
  snprintf(path, sizeof(path), "%s/edited-profile.json", dir);
  profile_seed.maf.k = .234567;
  profile_seed.lambda.k = .876543;
  CHECK(export_profile(path), "Persist profile");
  bk_layout_t saved;
  bk_ui_get_layout(&saved);
  CHECK(import_profile(path), "Restore saved profile");
  CHECK(fabs(profile_seed.maf.k - .234567) < 1e-12 &&
            fabs(profile_seed.lambda.k - .876543) < 1e-12,
        "User calibration coefficients preserved");
  bk_ui_get_layout(&readback);
  CHECK(!memcmp(&saved, &readback, sizeof(saved)), "Exact layout restored");
  lv_indev_delete(pointer);
  printf("PASS: Classic+Reference, vertical/horizontal pointer gestures, "
         "drag/save/cancel/reset, calibration, source loss/recovery, JSON "
         "preservation, no boot.\n");
  return 0;
}
int main(int argc, char **argv) {
  const char *input = NULL, *output = NULL, *render_dir = NULL,
             *headless = NULL;
  bool reference = false, classic = false, no_persist = false, demo = false,
       smoke_window = false;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--import") && i + 1 < argc)
      input = argv[++i];
    else if (!strcmp(argv[i], "--export") && i + 1 < argc)
      output = argv[++i];
    else if (!strcmp(argv[i], "--profile") && i + 1 < argc)
      save_path = argv[++i];
    else if (!strcmp(argv[i], "--no-persist"))
      no_persist = true;
    else if (!strcmp(argv[i], "--smoke-window"))
      smoke_window = true;
    else if (!strcmp(argv[i], "--demo"))
      demo = true;
    else if (!strcmp(argv[i], "--reference"))
      reference = true;
    else if (!strcmp(argv[i], "--classic"))
      classic = true;
    else if (!strcmp(argv[i], "--render") && i + 1 < argc)
      render_dir = argv[++i];
    else if (!strcmp(argv[i], "--headless"))
      headless = i + 1 < argc ? argv[++i] : ".";
    else {
      fprintf(stderr,
              "bk_sim [--classic|--reference] [--profile file] [--import file] "
              "[--export file] [--render DIR|--headless DIR]\n");
      return 2;
    }
  }
  bk_settings_defaults(&profile_seed);
  bk_sources_init(&sources);
  lv_init();
  lv_display_t *display = lv_display_create(480, 480);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(display, draw_buffer, NULL, sizeof(draw_buffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, flush);
  bk_ui_create(lv_screen_active());
  bk_ui_set_dtc_lookup(demo_dtc);
  if (!headless && !render_dir && !input && !no_persist) {
    FILE *f = fopen(save_path, "rb");
    if (f) {
      fclose(f);
      if (!import_profile(save_path))
        return 1;
    }
  }
  if (input && !import_profile(input))
    return 1;
  if (reference || classic) {
    bk_layout_t layout;
    if (reference)
      bk_layout_reference_defaults(&layout);
    else
      bk_layout_defaults(&layout);
    CHECK(bk_ui_set_layout(&layout), "Selected style defaults");
    bk_ui_select_screen(0);
  }
  if (demo) {
    if (input) {
      fprintf(stderr, "--demo cannot replace imported calibration; use a "
                      "separate fixture profile\n");
      return 2;
    }
    fixture_calibrations();
  }
  sample();
  if (render_dir) {
    unsigned saved = bk_ui_screen();
    char path[1024];
    for (unsigned view = 0; view < 13; view++) {
      if (view == 7) {
        dtc_count = 3;
        sample();
      }
      bk_ui_select_screen(view);
      render(display);
      snprintf(path, sizeof(path), "%s/custom-%02u.ppm", render_dir, view);
      if (ppm(path))
        return 1;
    }
    bk_ui_select_screen(saved);
    if (output && !export_profile(output))
      return 1;
    lv_display_delete(display);
    lv_deinit();
    printf("Measured render CPU batch avg %.2f ms / max %.2f ms (%u batches). "
           "Not a hardware FPS claim.\n",
           frame_total_ms / frames, frame_max_ms, frames);
    return 0;
  }
  if (headless) {
    if (output && !export_profile(output))
      return 1;
    int result = run_checks(display, headless);
    lv_display_delete(display);
    lv_deinit();
    if (frames)
      printf("Render CPU batch avg %.2f ms / max %.2f ms (%u batches).\n",
             frame_total_ms / frames, frame_max_ms, frames);
    return result;
  }
#ifdef BK_HAVE_SDL
  int result = interactive(smoke_window);
  if (output && !export_profile(output))
    result = 1;
  if (!no_persist && !export_profile(save_path))
    result = 1;
  lv_display_delete(display);
  lv_deinit();
  return result;
#else
  (void)smoke_window;
  fprintf(stderr, "SDL2 unavailable; use --render DIR or --headless DIR.\n");
  return 2;
#endif
}
