#!/bin/bash
# Host test of the v2 shake detector: test_shake.sh [out dir]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../../../source"
OUT="${1:-$HERE/out}"
mkdir -p "$OUT"
clang++ -std=c++17 -O0 -w -DMODEL_TS101 -include "$HERE/stubs/host_prelude.h" -I"$HERE/stubs" -I"$SRC/Core/BSP" -I"$SRC/Core/BSP/Miniware" -I"$SRC/Core/Inc" \
  -I"$SRC/Core/Threads/UI/drawing/mono_128x32" "$HERE/test_shake.cpp" "$SRC/Core/Threads/UI/drawing/mono_128x32/ts101_v2_shake.cpp" -o "$OUT/test_shake"
"$OUT/test_shake"
