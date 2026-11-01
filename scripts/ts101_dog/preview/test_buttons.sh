#!/bin/bash
# Build and run the host test of the button logic: test_buttons.sh [out dir]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../../../source"
OUT="${1:-$HERE/out}"
mkdir -p "$OUT"
eval "$(sed -n '/^FLAGS=/,/logic\/utils")/p' "$HERE/build.sh")"
eval "$(sed -n '/^UI=/p;/^SOURCES=/,/pre_render_assets.cpp")/p' "$HERE/build.sh")"
LOGIC="$SRC/Core/Threads/UI/logic"
SOURCES=("${SOURCES[@]/$HERE\/preview.cpp/$HERE/test_buttons.cpp}" "$SRC/Core/Drivers/Buttons.cpp" "$LOGIC/Soldering.cpp" "$LOGIC/HomeScreen.cpp" "$LOGIC/Sleep.cpp"
  "$LOGIC/DebugMenu.cpp" "$LOGIC/utils/SolderingCommon.cpp" "$LOGIC/utils/shouldDeviceSleep.cpp" "$LOGIC/utils/shouldDeviceShutdown.cpp" "$LOGIC/utils/min.cpp" "$HERE/test_stubs.cpp")
clang++ "${FLAGS[@]}" -DBUTTON_TEST "${SOURCES[@]}" -o "$OUT/test_buttons"
"$OUT/test_buttons"
