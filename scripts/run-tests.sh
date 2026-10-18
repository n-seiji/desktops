#!/bin/sh
# Builds and runs every unit test in bin/. Run through mise (CFLAGS comes from mise.toml).
# SAN adds sanitizer flags: `mise run test-san`.
set -eu
cd "$(dirname "$0")/.."
: "${CFLAGS:?run this through mise: mise run test}"
SAN="${SAN:-}"
mkdir -p bin

# run <name> <sources and libraries...>: builds tests/<name>.c with the given sources and runs it
run() {
  name=$1
  shift
  # shellcheck disable=SC2086
  clang $CFLAGS $SAN "tests/$name.c" "$@" -o "bin/$name"
  "bin/$name"
}

run title_test src/title.c -framework CoreFoundation
run config_test src/config.c src/config_write.c
run config_write_test src/config_write.c
run select_test src/select.c
run keyrules_test src/keyrules.c
run strip_test src/strip.c
run order_test src/order.c
run shown_test src/shown.c -framework ApplicationServices
run cardmath_test src/cardmath.c -framework ApplicationServices
run border_geom_test src/border_geom.c -framework ApplicationServices
