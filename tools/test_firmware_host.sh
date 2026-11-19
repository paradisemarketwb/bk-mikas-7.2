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
