#include "bk_settings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void *read_file(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  assert(f);
  assert(!fseek(f, 0, SEEK_END));
  long n = ftell(f);
  assert(n > 0);
  rewind(f);
  unsigned char *b = calloc(1, (size_t)n + 1);
  assert(b && fread(b, 1, n, f) == (size_t)n);
  fclose(f);
  *size = n;
  return b;
}
int main(void) {
  bk_settings_t *s = malloc(sizeof(*s)), *t = malloc(sizeof(*t));
  assert(s && t);
  size_t n;
  void *old = read_file("tests/fixtures/classic-v3-nvs.bin", &n);
  assert(bk_settings_decode(old, n, s));
  assert(s->version == 4 && s->layout.version == 2 &&
         s->layout.screen_count == 5);
  assert(!s->maf.configured && !s->lambda.configured);
  size_t json_n;
  char *json = read_file("simulator/profiles/default.json", &json_n);
  assert(bk_settings_from_json(json, t));
  char *a = bk_settings_to_json(s), *b = bk_settings_to_json(t);
  assert(a && b && !strcmp(a, b));
  free(a);
  free(b);
  free(json);
  for (unsigned i = 0; i < s->layout.screen_count; i++)
    assert(s->layout.screens[i].style == BK_STYLE_CLASSIC);
  assert(s->layout.screens[0].widgets[0].x == 33 &&
         s->layout.screens[0].widgets[0].y == 16);
  ((unsigned char *)old)[4] = 9;
  assert(bk_settings_decode(old, n, t) && t->screen == 0);
  free(old);
  unsigned char packed[16384];
  size_t bytes = bk_settings_encode(s, packed, sizeof(packed));
  assert(bytes && bk_settings_decode(packed, bytes, t));
  assert(!memcmp(s, t, sizeof(*s)));
  packed[bytes - 1] ^= 1;
  t->brightness = 77;
  assert(!bk_settings_decode(packed, bytes, t) && t->brightness == 77);
  bk_layout_reference_defaults(&s->layout);
  assert(bk_settings_valid(s));
  s->maf.configured = true;
  s->maf.method = BK_CAL_LINEAR;
  s->maf.unit = BK_UNIT_G_S;
  s->maf.k = .1;
  s->maf.b = 2;
  strcpy(s->maf.profile_name, "user MAF");
  assert(bk_settings_valid(s));
  s->layout.screens[0].show_title = false;
  s->layout.screens[0].widgets[1].x += 4;
  a = bk_settings_to_json(s);
  assert(a && bk_settings_from_json(a, t));
  b = bk_settings_to_json(t);
  assert(b && !strcmp(a, b));
  free(a);
  free(b);
  FILE *reference=fopen("build/tests/reference-profile.json","wb");assert(reference);
  a=bk_settings_to_json(s);assert(a&&fputs(a,reference)>=0);fclose(reference);free(a);
  bk_calibration_t calibration = s->maf;
  bk_layout_screen_t other = s->layout.screens[1];
  bk_layout_default_screen(&s->layout.screens[0], 0, BK_STYLE_REFERENCE);
  assert(!memcmp(&calibration, &s->maf, sizeof(calibration)) &&
         !memcmp(&other, &s->layout.screens[1], sizeof(other)));
  bk_settings_reset_screens(s);
  assert(!memcmp(&calibration, &s->maf, sizeof(calibration)));
  bk_layout_reference_defaults(&s->layout);
  bytes = bk_settings_encode(s, packed, sizeof(packed));
  assert(bytes && bytes <= 6144);
  printf("v3 binary/JSON→Classic migration, boot removal, CRC/atomic decode, "
         "Reference/calibration roundtrip and scoped reset: PASS (%zu-byte "
         "struct, %zu-byte packed default)\n",
         sizeof(*s), bytes);
  free(s);
  free(t);
  return 0;
}
