#!/usr/bin/env bash
set -euo pipefail
REPO=$(cd "$(dirname "$0")/../../.." && pwd)
SOURCE="$REPO/experimental/enhanced-silence-v40/source/headers"
OUTPUT=${1:-${TMPDIR:-/tmp}/jamesdsp-signal-diagnostic-test}
cc -std=gnu11 -O1 -g -Wall -Wextra -fsanitize=address,undefined \
  -fno-omit-frame-pointer -pthread \
  -I"$REPO/experimental/enhanced-silence-v40/tests/include" \
  -I"$SOURCE" -I"$SOURCE/orbit" \
  "$REPO/experimental/enhanced-silence-v40/tests/signal_diagnostic_test.c" \
  -lm -o "$OUTPUT"
ASAN_OPTIONS=detect_leaks=1 "$OUTPUT"
