#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/tests
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
 -I firmware/components/mikas/include -I firmware/components/telemetry/include \
 firmware/components/mikas/mikas.c firmware/components/telemetry/bk_telemetry.c tests/test_monitor.c -o build/tests/monitor
build/tests/monitor
cc -std=c11 -Wall -Wextra -Werror -pedantic \
 -I firmware/components/mikas_telemetry/include firmware/components/mikas_telemetry/mikas_telemetry.c \
 firmware/tests/test_telemetry.c -lm -o build/tests/telemetry
build/tests/telemetry
if [[ -n "${CJSON_DIR:-}" ]]; then
 cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
  -I tests/host -I "$CJSON_DIR" -I firmware/components/sources/include -I firmware/components/settings/include \
  -I firmware/components/ui_config/include -I firmware/components/history/include -I firmware/components/telemetry/include \
  firmware/components/sources/bk_sources.c firmware/components/settings/bk_profile_storage.c \
  firmware/components/settings/bk_profile.c firmware/components/settings/bk_profile_json.c \
  firmware/components/ui_config/bk_ui_config.c firmware/components/history/bk_history.c \
  firmware/components/telemetry/bk_telemetry.c "$CJSON_DIR/cJSON.c" tests/test_profile_history.c \
  -lm -o build/tests/profile-history
 build/tests/profile-history build/tests/default-profile.json
fi
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
 -I firmware/components/mikas/include -I firmware/components/kline/include \
 firmware/components/mikas/mikas.c firmware/components/mikas/mikas_link.c tests/test_mikas_stream.c \
 -o build/tests/mikas-stream
build/tests/mikas-stream

cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
 -I firmware/components/sources/include -I firmware/components/telemetry/include \
 firmware/components/sources/bk_sources.c firmware/components/telemetry/bk_telemetry.c tests/test_sources.c \
 -lm -o build/tests/sources
build/tests/sources
if [[ -n "${CJSON_DIR:-}" ]]; then
 cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
  -I "$CJSON_DIR" -I firmware/components/settings/include -I firmware/components/ui_config/include \
  -I firmware/components/sources/include firmware/components/settings/bk_profile.c \
  firmware/components/settings/bk_profile_json.c firmware/components/settings/bk_profile_storage.c \
  firmware/components/sources/bk_sources.c firmware/components/ui_config/bk_ui_config.c \
  "$CJSON_DIR/cJSON.c" tests/test_profile_migration.c -lm -o build/tests/profile-migration
 build/tests/profile-migration
fi

cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
 -I firmware/components/background/include firmware/components/background/bk_background.c firmware/components/background/bk_background_esp.c \
 tests/test_background.c -o build/tests/background
build/tests/background
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
 -I firmware/components/data_model/include -I firmware/components/sources/include \
 -I firmware/components/settings/include -I firmware/components/ui_config/include \
 -I firmware/components/telemetry/include firmware/components/data_model/bk_data_model.c \
 firmware/components/sources/bk_sources.c firmware/components/settings/bk_profile.c \
 firmware/components/ui_config/bk_ui_config.c firmware/components/telemetry/bk_telemetry.c \
 tests/test_data_model.c -lm -o build/tests/data-model
build/tests/data-model

if [[ -n "${CJSON_DIR:-}" ]]; then
 cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
  -I "$CJSON_DIR" -I firmware/components/settings/include -I firmware/components/ui_config/include \
  -I firmware/components/sources/include firmware/components/settings/bk_profile.c \
  firmware/components/settings/bk_profile_json.c firmware/components/settings/bk_profile_storage.c \
  firmware/components/sources/bk_sources.c firmware/components/ui_config/bk_ui_config.c \
  "$CJSON_DIR/cJSON.c" tests/test_profile_capacity.c -lm -o build/tests/profile-capacity
 build/tests/profile-capacity
fi
