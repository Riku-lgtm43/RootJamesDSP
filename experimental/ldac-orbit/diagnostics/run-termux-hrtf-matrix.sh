#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

EXPECTED_PHONON_SHA=857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PHONON=${1:?Usage: run-termux-hrtf-matrix.sh /path/to/extracted/libphonon.so [sofa-path]}
SOFA=${2:-/vendor/etc/axionfx/hrtf/sadie_h12.sofa}
ACTUAL=$(sha256sum "$PHONON" | awk '{print $1}')
if [[ "$ACTUAL" != "$EXPECTED_PHONON_SHA" ]]; then
  printf 'Refusing library with unexpected SHA-256.\nExpected: %s\nActual:   %s\n' "$EXPECTED_PHONON_SHA" "$ACTUAL" >&2
  exit 1
fi
command -v clang++ >/dev/null || { echo 'Install Termux clang first: pkg install clang' >&2; exit 1; }
TMP=$(mktemp -d "${TMPDIR:-$PREFIX/tmp}/jdsp-hrtf-matrix.XXXXXX")
trap 'rm -rf "$TMP"' EXIT
cp -- "$PHONON" "$TMP/libphonon.so"
clang++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -I "$SCRIPT_DIR/include" \
  "$SCRIPT_DIR/hrtf_init_matrix.cpp" \
  -L "$TMP" -Wl,-rpath,'$ORIGIN' -lphonon \
  -o "$TMP/hrtf_init_matrix"
printf 'Library SHA-256: %s\n' "$ACTUAL"
printf 'Library ELF/build identity:\n'
(command -v readelf >/dev/null && readelf -n "$PHONON" | sed -n '/Build ID/p') || true
printf 'Running default and readable SOFA matrix against the verified Stage 31 library.\n'
LD_LIBRARY_PATH="$TMP${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$TMP/hrtf_init_matrix" "$SOFA"
