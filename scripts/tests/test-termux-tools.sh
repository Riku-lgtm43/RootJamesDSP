#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/bin"

source "$ROOT/scripts/termux-android-tools.sh"
source "$ROOT/scripts/termux-package-detection.sh"

# Models Termux zipalign: help is usage-only and exits 2, while alignment and
# verification calls perform their normal work successfully.
cat > "$TMP/bin/zipalign" <<'SH'
#!/usr/bin/env bash
case "$1" in
  -h) echo 'Usage: zipalign'; exit 2 ;;
  -f) cp "$4" "$5" ;;
  -c) [[ -s "$3" ]] ;;
  *) exit 2 ;;
esac
SH
chmod +x "$TMP/bin/zipalign"
PATH="$TMP/bin:$PATH"
export PATH
[[ $(find_android_tool zipalign) == "$TMP/bin/zipalign" ]]

cat > "$TMP/bin/dumpsys" <<'SH'
#!/usr/bin/env bash
cat "$DUMPSYS_FIXTURE"
exit "${DUMPSYS_STATUS:-0}"
SH
chmod +x "$TMP/bin/dumpsys"
cat > "$TMP/bin/su" <<'SH'
#!/usr/bin/env bash
cat "$SU_FIXTURE"
exit "${SU_STATUS:-1}"
SH
chmod +x "$TMP/bin/su"

DUMPSYS_FIXTURE="$TMP/packages.txt"
SU_FIXTURE="$TMP/root-packages.txt"
export DUMPSYS_FIXTURE SU_FIXTURE
printf 'Package [james.dsp.debug] (debug)\n' > "$DUMPSYS_FIXTURE"
[[ $(termux_package_status james.dsp) == absent ]]
printf 'Package [james.dsp] (release)\n' > "$DUMPSYS_FIXTURE"
[[ $(termux_package_status james.dsp) == installed ]]

# A failed unprivileged lookup falls back to root dumpsys. A failed lookup by
# both paths remains unknown rather than being misreported as package absence.
DUMPSYS_STATUS=1
printf 'Package [james.dsp] (release)\n' > "$SU_FIXTURE"
SU_STATUS=0
export DUMPSYS_STATUS SU_STATUS
[[ $(termux_package_status james.dsp) == installed ]]
SU_STATUS=1
export SU_STATUS
[[ $(termux_package_status james.dsp) == unknown ]]

echo 'Termux tool and package detection regressions passed.'
