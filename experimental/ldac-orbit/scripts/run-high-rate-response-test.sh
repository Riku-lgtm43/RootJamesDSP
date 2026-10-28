#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 3 ]; then
  echo "usage: $0 /path/to/patched/source /path/to/SteamAudio-SDK /path/to/cipic_124.sofa" >&2
  exit 2
fi

SRC=$(realpath "$1")
SDK=$(realpath "$2")
SOFA=$(realpath "$3")
LIB="$SDK/lib/linux-x64/libphonon.so"
for file in "$SRC/orbit/SteamSpatial.cpp" "$SRC/orbit/SteamSpatial.h" \
            "$SDK/include/phonon.h" "$LIB" "$SOFA"; do
  [ -f "$file" ] || { echo "missing input: $file" >&2; exit 2; }
done
case "$SOFA" in
  *'"'*) echo "SOFA path cannot contain a quote character" >&2; exit 2 ;;
esac

TMP=$(mktemp -d "${TMPDIR:-/tmp}/jdsp-high-rate-response.XXXXXXXX")
printf '#define JAMESDSP_ORBIT_CIPIC_SOFA_PATH "%s"\n' "$SOFA" > "$TMP/sofa_override.h"
CXX=${CXX:-g++}
"$CXX" -std=c++17 -O2 -Wall -Wextra -Werror \
  -I"$SRC" -I"$SRC/orbit" -I"$SDK/include" \
  -include "$TMP/sofa_override.h" \
  "$SRC/orbit/SteamSpatial.cpp" \
  "$(dirname "$0")/../tests/orbit_high_rate_response_test.cpp" \
  "$LIB" -Wl,-rpath,"$(dirname "$LIB")" \
  -o "$TMP/orbit-high-rate-response-test"
"$TMP/orbit-high-rate-response-test"
