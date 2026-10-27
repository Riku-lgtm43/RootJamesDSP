#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -lt 2 ]; then
  echo "usage: $0 /path/to/patched/Stage18/source /path/to/SteamAudio/include [output-binary]" >&2
  exit 2
fi
SRC=$(realpath "$1")
SDK_INC=$(realpath "$2")
OUT=${3:-${TMPDIR:-/tmp}/jamesdsp-orbit-frame-bridge-test}
mkdir -p "$(dirname "$OUT")"
CXX=${CXX:-g++}
CC=${CC:-gcc}
"$CXX" -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I"$SRC" -I"$SRC/orbit" -I"$SDK_INC" \
  "$SRC/orbit/SteamSpatial.cpp" \
  "$(dirname "$0")/../tests/mock_phonon.cpp" \
  "$(dirname "$0")/../tests/frame_bridge_test.cpp" -o "$OUT"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "$OUT"
"$CC" -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
  -fno-omit-frame-pointer -include stddef.h -I"$SRC" \
  -c "$SRC/jamesdsp.c" -o "$OUT.jamesdsp.o"
"$CC" -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
  -fno-omit-frame-pointer -include stddef.h -I"$SRC" \
  -c "$(dirname "$0")/../tests/effect_dsp_stubs.c" -o "$OUT.effect_dsp_stubs.o"
"$CXX" -std=c++17 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I"$SRC" -I"$SRC/orbit" -I"$SDK_INC" \
  "$SRC/legacy_orbit_c.cpp" \
  "$SRC/orbit/OrbitAdapter.cpp" \
  "$SRC/orbit/EffectParamOrbitBridge.cpp" \
  "$SRC/orbit/SteamSpatial.cpp" \
  "$(dirname "$0")/../tests/mock_phonon.cpp" \
  "$(dirname "$0")/../tests/effect_connection_test.cpp" \
  "$OUT.jamesdsp.o" "$OUT.effect_dsp_stubs.o" -o "$OUT.connection"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} "$OUT.connection"
