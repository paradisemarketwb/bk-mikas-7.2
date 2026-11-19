#include "bk_settings.h"
#include <string.h>
void bk_settings_defaults(bk_settings_t *s) {
  memset(s, 0, sizeof(*s));
  s->version = BK_SETTINGS_VERSION;
  s->screen = 1;
  s->brightness = 80;
  s->dark = 0;
  const uint8_t selected[] = {8, 1, 4, 5, 10};
  memcpy(s->selected, selected, 5);
  bk_layout_defaults(&s->layout);
  bk_settings_reset_calibrations(s);
}
bool bk_settings_valid(const bk_settings_t *s) {
  if (!s || s->version != BK_SETTINGS_VERSION || s->brightness < 5 ||
      s->brightness > 100 || s->dark > 1 || !bk_layout_validate(&s->layout) ||
      !bk_calibration_validate(&s->maf, NULL, 0) ||
      !bk_calibration_validate(&s->lambda, NULL, 0))
    return false;
  if (s->screen > 9 &&
      (s->screen < 10 || s->screen >= 10 + s->layout.screen_count - 5))
    return false;
  for (unsigned i = 0; i < 5; i++)
    if (s->selected[i] >= BK_METRIC_COUNT)
      return false;
  for (unsigned i = 0; i < BK_METRIC_COUNT; i++)
    if ((unsigned)s->source_policies[i].preferred >= BK_SOURCE_COUNT ||
        s->source_policies[i].allow_simulation)
      return false;
  int slot = bk_layout_view_to_screen(s->screen);
  if (slot >= 0 &&
      (slot >= s->layout.screen_count || !s->layout.screens[slot].enabled))
    return false;
  return true;
}

void bk_settings_reset_calibrations(bk_settings_t *s) {
  bk_calibration_defaults(&s->maf, BK_CAL_MAF);
  bk_calibration_defaults(&s->lambda, BK_CAL_LAMBDA);
}
void bk_settings_reset_screens(bk_settings_t *s) {
  bk_layout_defaults(&s->layout);
  s->screen = 1;
  const uint8_t selected[] = {8, 1, 4, 5, 10};
  memcpy(s->selected, selected, 5);
}
