#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
  echo "usage: $0 /path/to/Stage18/source equivalent|divergence [output-dir]" >&2
  exit 2
fi
SRC=$(realpath "$1")
EXPECTED=$2
[[ "$EXPECTED" == equivalent || "$EXPECTED" == divergence ]] || {
  echo "expected mode must be equivalent or divergence" >&2; exit 2;
}
OUT=${3:-"${TMPDIR:-/tmp}/jamesdsp-asrc-segmentation"}
mkdir -p "$OUT/core"
OUT=$(realpath "$OUT")
CC=${CC:-gcc}
CXX=${CXX:-g++}
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
OBJECTS=()
for rel in "${CORE_REL[@]}"; do
  [[ "$rel" == jamesdsp.c ]] && continue
  object="$OUT/core/$(echo "$rel" | tr '/.' '__').o"
  "$CC" -std=gnu11 -O1 -g -fsanitize=address,undefined -funsigned-char -fno-omit-frame-pointer \
    -w -Wno-error=incompatible-pointer-types -include stddef.h -I"$SRC" \
    -c "$SRC/$rel" -o "$object"
  OBJECTS+=("$object")
done
"$CXX" -std=c++17 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -Wall -Wextra -Werror -I"$SRC" \
  "$(cd "$(dirname "$0")/.." && pwd)/tests/asrc_segmentation_test.cpp" \
  "${OBJECTS[@]}" -lm -lpthread -o "$OUT/asrc_segmentation_test"
if [[ "$EXPECTED" == divergence ]]; then
  DEFAULT_UBSAN_OPTIONS=halt_on_error=0:print_stacktrace=0
else
  DEFAULT_UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
fi
ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1} \
  UBSAN_OPTIONS=${UBSAN_OPTIONS:-$DEFAULT_UBSAN_OPTIONS} \
  "$OUT/asrc_segmentation_test" "$EXPECTED" | tee "$OUT/result.txt"
