#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "usage: $0 /path/to/patched/Stage18/source /path/to/steamaudio/include [output-dir]" >&2
  exit 2
fi

SRC=$(realpath "$1")
SDK_INC=$(realpath "$2")
OUT=${3:-"${TMPDIR:-/tmp}/jamesdsp-orbit-engine-regression"}
mkdir -p "$OUT"
OUT=$(realpath "$OUT")
HERE=$(cd "$(dirname "$0")/.." && pwd)
CC=${CC:-gcc}
CXX=${CXX:-g++}

# This source manifest is the same native C inventory used by Android.mk.
mapfile -t CORE_REL < <(
  awk '
    /LOCAL_SRC_FILES :=/ { active = 1; next }
    /# terminator/ { active = 0 }
    active {
      gsub(/\\/, "")
      gsub(/[[:space:]]+/, " ")
      sub(/^ /, "")
      sub(/ $/, "")
      if (length($0)) print $0
    }
  ' "$SRC/Android.mk.disabled"
)
CORE=()
for rel in "${CORE_REL[@]}"; do
  [[ "$rel" == "jamesdsp.c" ]] && continue
  CORE+=("$SRC/$rel")
done

SAN=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
mkdir -p "$OUT/core"
OBJECTS=()
for source in "${CORE[@]}"; do
  object="$OUT/core/$(echo "${source#"$SRC/"}" | tr '/.' '__').o"
  "$CC" -std=gnu11 "${SAN[@]}" -funsigned-char -w -Wno-error=incompatible-pointer-types \
    -include stddef.h -I"$SRC" -c "$source" -o "$object"
  OBJECTS+=("$object")
done

"$CXX" -std=c++17 "${SAN[@]}" -Wall -Wextra -Werror -I"$SRC" \
  "$HERE/tests/engine_host_test.cpp" "${OBJECTS[@]}" -lm -lpthread -o "$OUT/engine_host_test"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} \
  UBSAN_OPTIONS=${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1} \
  "$OUT/engine_host_test"

# Link the actual AudioEffect wrapper and actual engine. Only Steam Audio's C API
# is mocked so the test can check the native command/process connection offline.
"$CC" -std=gnu11 "${SAN[@]}" -funsigned-char -w -Wno-error=incompatible-pointer-types \
  -include stddef.h -I"$SRC" \
  -c "$SRC/jamesdsp.c" -o "$OUT/jamesdsp.o"
"$CXX" -std=c++17 "${SAN[@]}" -Wall -Wextra -Werror \
  -I"$SRC" -I"$SRC/orbit" -I"$SDK_INC" \
  "$SRC/legacy_orbit_c.cpp" \
  "$SRC/orbit/OrbitAdapter.cpp" \
  "$SRC/orbit/EffectParamOrbitBridge.cpp" \
  "$SRC/orbit/SteamSpatial.cpp" \
  "$HERE/tests/mock_phonon.cpp" \
  "$HERE/tests/native_engine_integration_test.cpp" \
  "$OUT/jamesdsp.o" "${OBJECTS[@]}" -lm -lpthread -o "$OUT/native_engine_integration_test"
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} \
  UBSAN_OPTIONS=${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1} \
  "$OUT/native_engine_integration_test"
