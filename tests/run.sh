#!/usr/bin/env bash
# Native simulation only: no network, target device, or ESPHome installation needed.
# Optional compiler flags: bash tests/run.sh -fsanitize=address,undefined
set -euo pipefail
test_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "$test_dir/.." && pwd)"
test_binary="$(mktemp "${TMPDIR:-/tmp}/nrf52-one-wire-test.XXXXXX")"
trap 'rm -f -- "$test_binary"' EXIT
"${CXX:-c++}" "$@" -std=c++20 -Wall -Wextra -Werror -fmax-errors=3 \
  -I"$test_dir/stubs" -I"$root/components/nrf52_one_wire" -I"$test_dir/upstream" \
  "$root/components/nrf52_one_wire/nrf52_one_wire.cpp" \
  "$test_dir/upstream/esphome/components/one_wire/one_wire_bus.cpp" \
  "$test_dir/test.cpp" -o "$test_binary"
"$test_binary"
