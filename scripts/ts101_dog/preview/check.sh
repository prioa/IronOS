#!/bin/bash
# Syntax-check UI sources against the host stubs: check.sh <file.cpp>...
cd "$(dirname "$0")/../../../source"
STUBS="$(cd ../scripts/ts101_dog/preview/stubs && pwd)"
for f in "$@"; do
  echo "== $f"
  clang++ -std=c++17 -fsyntax-only -DMODEL_TS101 -I"$STUBS" -I./Core/BSP -I./Core/BSP/Miniware -I./Core/Drivers \
    -I./Core/Drivers/usb-pd/include -I./Core/Inc -I./Core/Threads -I./Core/Threads/UI/drawing -I./Core/Threads/UI/logic \
    -I./Core/Threads/UI/logic/utils "$f" 2>&1 | grep -E "error" | head -8
done
