#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
  echo "usage: $0 /path/to/patched/Stage18/source [output-dir] [iterations]" >&2
  exit 2
fi

SRC=$(realpath "$1")
OUT=${2:-"${TMPDIR:-/tmp}/jamesdsp-orbit-engine-benchmark"}
ITERATIONS=${3:-100}
mkdir -p "$OUT/core"
OUT=$(realpath "$OUT")
HERE=$(cd "$(dirname "$0")/.." && pwd)
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
  [[ "$rel" == "jamesdsp.c" ]] && continue
  object="$OUT/core/$(echo "$rel" | tr '/.' '__').o"
  "$CC" -std=gnu11 -O2 -DNDEBUG -funsigned-char -w -Wno-error=incompatible-pointer-types \
    -include stddef.h -I"$SRC" -c "$SRC/$rel" -o "$object"
  OBJECTS+=("$object")
done

"$CXX" -std=c++17 -O2 -DNDEBUG -I"$SRC" \
  "$HERE/tests/engine_callback_bench.cpp" "${OBJECTS[@]}" \
  -lm -lpthread -o "$OUT/engine_callback_bench"
RAW_LOG="$OUT/benchmark.log"
{
  for rate in 44100 48000 88200 96000; do
    "$OUT/engine_callback_bench" "$rate" "$ITERATIONS"
  done
} > "$RAW_LOG"
awk 'BEGIN { FS="," } /^rate,/ || /^[0-9]+,/ { print }' "$RAW_LOG" \
  | tee "$OUT/benchmark.csv"
warning_count=$(grep -c '^Latency must not smaller than 64$' "$RAW_LOG" || true)
printf 'latency_warning_lines=%s\n' "$warning_count" > "$OUT/diagnostics.txt"
printf '%s\n' "latency_warning_lines=$warning_count"
