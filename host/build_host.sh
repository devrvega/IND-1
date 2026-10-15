#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the IND-1 host simulator (the whole app on this machine) into build/host/ind_host.
# Same sources as the firmware, -ffp-contract=off like the device (no fused multiply-add).
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
mkdir -p build/gen build/host
[ -f build/gen/felucca_font.h ] || python3 tools/gen_font.py build/gen/felucca_font.h >/dev/null
$CC -O2 -ffp-contract=off -std=c99 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
    -DEN_HOST -Ifirmware/src -Ifirmware/src/dsp -Ibuild/gen -o build/host/ind_host host/ind_host.c \
    firmware/src/dsp/ind.c -lm
echo "host: build/host/ind_host"
