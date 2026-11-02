#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "usage: $0 /path/to/ndk/llvm/prebuilt/<host>/bin /path/to/v40-module.zip /path/to/output-directory" >&2
  exit 2
fi

ROOT=$(cd "$(dirname "$0")" && pwd)
BIN=$(realpath "$1")
BASE=$(realpath "$2")
OUT=$(realpath -m "$3")
CC="$BIN/aarch64-linux-android26-clang"
CXX="$BIN/aarch64-linux-android26-clang++"
[ -x "$CC" ] && [ -x "$CXX" ] || { echo "Android NDK r27 ARM64 compiler unavailable" >&2; exit 2; }
[ -f "$BASE" ] || { echo "v40 module unavailable" >&2; exit 2; }

printf '%s  %s\n' \
  f3d401aea814c27500f88d70235bf7e22d085ffe0abf1063fc3198347d2ee46e "$BASE" | sha256sum -c -
printf '%s  %s\n' \
  e6b83dae7673fa28a4981e41b5f2ef3ab94625ceddb0384fc5d907de7396b109 \
  "$ROOT/artifacts/v40-native-link-objects.tar.gz" | sha256sum -c -

mkdir -p "$OUT"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/jamesdsp-v40-diagnostic.XXXXXXXX")
trap 'rm -rf -- "$TMP"' EXIT
tar -xzf "$ROOT/artifacts/v40-native-link-objects.tar.gz" -C "$TMP"
unzip -p "$BASE" system/vendor/lib64/soundfx/libphonon.so > "$TMP/libphonon.so"
unzip -p "$BASE" system/vendor/lib64/soundfx/libjamesdsp_orbit.so > "$TMP/v40-original.so"
printf '%s  %s\n' \
  857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369 "$TMP/libphonon.so" | sha256sum -c -
printf '%s  %s\n' \
  34fd8f4102ed92c2c252c7fb1ddf5483a0b614b9839ef836ee8cb642439ee680 "$TMP/v40-original.so" | sha256sum -c -

mapfile -t OBJECTS < "$ROOT/link-objects.txt"
[ "${#OBJECTS[@]}" -eq 90 ] || { echo "wrong v40 object count" >&2; exit 1; }
LINK_OBJECTS=()
DIAG_OBJECTS=()
for rel in "${OBJECTS[@]}"; do
  [ -f "$TMP/$rel" ] || { echo "missing v40 object: $rel" >&2; exit 1; }
  LINK_OBJECTS+=("$TMP/$rel")
  if [ "$rel" = obj/jamesdsp.c.o ]; then
    DIAG_OBJECTS+=("$TMP/jamesdsp-diagnostic.o")
  else
    DIAG_OBJECTS+=("$TMP/$rel")
  fi
done

LINK_FLAGS=(-shared -Wl,-z,defs -Wl,--error-limit=0 -Wl,-rpath,/vendor/lib64/soundfx)
LIB_FLAGS=("$TMP/libphonon.so" -llog -lm -ldl)
"$CXX" "${LINK_FLAGS[@]}" -o "$TMP/v40-relinked.so" "${LINK_OBJECTS[@]}" "${LIB_FLAGS[@]}"
cmp -s "$TMP/v40-relinked.so" "$TMP/v40-original.so" || {
  echo "refusing: original v40 native library did not relink byte-identically" >&2; exit 1;
}

"$CC" -std=gnu11 -fPIC -O2 -DJAMESDSP_REFERENCE_IMPL \
  -I"$ROOT/source/headers" -c "$ROOT/source/jamesdsp.c" \
  -o "$TMP/jamesdsp-diagnostic.o"
"$CXX" "${LINK_FLAGS[@]}" -o "$OUT/libjamesdsp_orbit.so" \
  "${DIAG_OBJECTS[@]}" "${LIB_FLAGS[@]}"

readelf -h -d "$OUT/libjamesdsp_orbit.so" | \
  grep -E 'Class:|Machine:|RUNPATH|NEEDED'
sha256sum "$OUT/libjamesdsp_orbit.so"
echo "v40 baseline relink: byte-identical; DSP and Orbit objects reused"
