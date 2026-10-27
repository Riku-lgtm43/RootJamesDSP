#!/usr/bin/env bash
set -euo pipefail
if [ "$#" -ne 5 ]; then
  echo "usage: $0 /path/to/ndk/toolchains/llvm/prebuilt/host/bin /path/to/source /path/to/SteamAudio/include /path/to/exact/libphonon.so /path/to/output-dir" >&2
  exit 2
fi
BIN=$(realpath "$1")
SRC=$(realpath "$2")
PHONON_INC=$(realpath "$3")
PHONON_LIB=$(realpath "$4")
OUT=$(realpath -m "$5")
CC="$BIN/aarch64-linux-android26-clang"
CXX="$BIN/aarch64-linux-android26-clang++"
for f in "$SRC/Android.mk.disabled" "$PHONON_INC/phonon.h" "$PHONON_LIB"; do
  [ -f "$f" ] || { echo "missing input: $f" >&2; exit 2; }
done
mkdir -p "$OUT/obj"
mapfile -t CFILES < <(sed -n '/^LOCAL_SRC_FILES :=/,/^# terminator/p' "$SRC/Android.mk.disabled" | sed -n 's/^[[:space:]]*\([^[:space:]\\#]*\.c\)[[:space:]\\]*$/\1/p')
CFILES+=(legacy_orbit_c.cpp orbit/OrbitAdapter.cpp orbit/EffectParamOrbitBridge.cpp orbit/SteamSpatial.cpp)
OBJS=()
for f in "${CFILES[@]}"; do
  obj="$OUT/obj/${f//\//_}.o"
  if [[ "$f" == *.cpp ]]; then
    "$CXX" -std=c++17 -fPIC -O2 \
      -I"$SRC" -I"$SRC/orbit" -I"$PHONON_INC" -c "$SRC/$f" -o "$obj"
  else
    "$CC" -std=gnu11 -fPIC -O2 \
      -I"$SRC" -I"$SRC/orbit" -I"$PHONON_INC" -c "$SRC/$f" -o "$obj"
  fi
  OBJS+=("$obj")
done
"$CXX" -shared -Wl,-z,defs -Wl,--error-limit=0 \
  -Wl,-rpath,/vendor/lib64/soundfx -o "$OUT/libjamesdsp_orbit_experimental.so" \
  "${OBJS[@]}" "$PHONON_LIB" -llog -lm -ldl
sha256sum "$OUT/libjamesdsp_orbit_experimental.so"
readelf -h -d "$OUT/libjamesdsp_orbit_experimental.so" | \
  grep -E 'Class:|Machine:|SONAME|NEEDED|RUNPATH'
