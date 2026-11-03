#!/bin/bash
# Build the host preview and render all scenes to PNG: build.sh [out dir]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../../../source"
OUT="${1:-$HERE/out}"
mkdir -p "$OUT"
FLAGS=(-std=c++17 -O0 -w -DMODEL_TS101 -include "$HERE/stubs/host_prelude.h" -I"$HERE" -I"$HERE/stubs" -I"$SRC" -I"$SRC/Core/BSP" -I"$SRC/Core/BSP/Miniware" -I"$SRC/Core/Drivers"
  -I"$SRC/Core/Drivers/usb-pd/include" -I"$SRC/Core/Inc" -I"$SRC/Core/Threads" -I"$SRC/Core/Threads/UI/drawing" -I"$SRC/Core/Threads/UI/drawing/mono_128x32"
  -I"$SRC/Core/Threads/UI/logic" -I"$SRC/Core/Threads/UI/logic/utils")
UI="$SRC/Core/Threads/UI/drawing/mono_128x32"
SOURCES=("$HERE/preview.cpp" "$HERE/preview_state.cpp" "$SRC/Core/Drivers/OLED.cpp" "$SRC/Core/Src/Translation.cpp" "$SRC/Core/Gen/Translation.${LANG_CODE:-DE}.cpp"
  "$UI/ts101_dog_screens.cpp" "$UI/ts101_dog_ui.cpp" "$UI/ts101_dog_sprites.cpp" "$UI/ts101_dog_fonts.cpp" "$UI/ts101_dog_keys.cpp" "$UI/ts101_dog_home.cpp" "$UI/ts101_dog_home_assets.cpp" "$UI/ts101_v2_grid.cpp" "$UI/ts101_v2_solder.cpp" "$UI/ts101_v2_home.cpp" "$UI/ts101_v2_sleep.cpp" "$UI/ts101_v2_shake.cpp" "$UI/ts101_v2_fonts.cpp" "$SRC/Core/Threads/UI/logic/TemperatureAdjust.cpp" "$SRC/Core/Threads/UI/logic/SettingsMenuDog.cpp" "$UI/draw_homescreen_detailed.cpp"
  "$UI/draw_tip_temperature.cpp" "$UI/draw_power_source_icon.cpp" "$UI/print_voltage.cpp" "$UI/show_warning.cpp"
  "$UI/pre_render_assets.cpp")
clang++ "${FLAGS[@]}" "${SOURCES[@]}" -o "$OUT/preview"
cd "$OUT" && ./preview
