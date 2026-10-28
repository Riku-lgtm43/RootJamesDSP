#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
SCRIPT="$ROOT/experimental/usb-hifi-192k/tools/capture-routing-diagnostics.sh"
TEMP=$(mktemp -d)
trap 'rm -rf "$TEMP"' EXIT
mkdir -p "$TEMP/bin" "$TEMP/Download"

cat > "$TEMP/bin/date" <<'EOF'
#!/bin/sh
case "$*" in
    *%Y%m%d_%H%M%S*) printf '20261028_120000\n' ;;
    *) printf '2026-10-28 12:00:00 +0000\n' ;;
esac
EOF

cat > "$TEMP/bin/getprop" <<'EOF'
#!/bin/sh
case "$1" in
    ro.build.version.release) printf '16\n' ;;
    ro.build.version.sdk) printf '36\n' ;;
    ro.build.fingerprint) printf 'test/fingerprint\n' ;;
    ro.product.model) printf 'Test Phone\n' ;;
    *) printf '\n' ;;
esac
EOF

cat > "$TEMP/bin/su" <<'EOF'
#!/bin/sh
if [ "${FAKE_ROOT:-0}" = '1' ]; then
    if [ "${1:-}" = '-c' ] && [ "${2:-}" = 'id -u' ]; then
        printf '0\n'
        exit 0
    fi
    [ "${1:-}" = '-c' ] && shift
    exec sh -c "$1"
fi
printf 'root unavailable in host fixture\n' >&2
exit 1
EOF

cat > "$TEMP/bin/dumpsys" <<'EOF'
#!/bin/sh
case "$*" in
    *media.audio_policy*)
        printf 'Output hifi_playback sampleRate=192000 format=PCM_32_BIT flags=NONE\n'
        printf 'USB device address aa:bb:cc:dd:ee:ff token=fixture-secret\n'
        ;;
    *media.audio_flinger*)
        printf 'Playback thread MIXER hifi_playback 192000 Hz\n'
        printf '0 Effect Chains\n'
        ;;
    audio) printf 'AudioPlaybackConfiguration sessionId:73 usage=MEDIA\n' ;;
    *) printf 'fixture dump for %s\n' "$*" ;;
esac
EOF

cat > "$TEMP/bin/logcat" <<'EOF'
#!/bin/sh
printf '10-28 12:00:00.000 AudioPolicyManager: moveEffects failed compatibility\n'
EOF

chmod +x "$TEMP/bin"/*

bash -n "$SCRIPT"
PATH="$TEMP/bin:$PATH" JAMESDSP_REPORT_DIR="$TEMP/Download" bash "$SCRIPT" > "$TEMP/result.txt"

mapfile -t REPORTS < <(find "$TEMP/Download" -maxdepth 1 -type f -name 'JamesDSP_USB_HiFi_Routing_*.txt' -print)
[[ ${#REPORTS[@]} -eq 1 ]]
REPORT=${REPORTS[0]}
grep -Fq 'hifi_playback sampleRate=192000' "$REPORT"
grep -Fq 'AudioPlaybackConfiguration sessionId:73' "$REPORT"
grep -Fq 'moveEffects failed compatibility' "$REPORT"
grep -Fq '[REDACTED-MAC]' "$REPORT"
grep -Fq 'token=[REDACTED]' "$REPORT"
! grep -Fq 'aa:bb:cc:dd:ee:ff' "$REPORT"
! grep -Fq 'fixture-secret' "$REPORT"
grep -Fq "$REPORT" "$TEMP/result.txt"

mkdir -p "$TEMP/RootDownload"
PATH="$TEMP/bin:$PATH" FAKE_ROOT=1 JAMESDSP_REPORT_DIR="$TEMP/RootDownload" bash "$SCRIPT" > "$TEMP/root-result.txt"
mapfile -t ROOT_REPORTS < <(find "$TEMP/RootDownload" -maxdepth 1 -type f -name 'JamesDSP_USB_HiFi_Routing_*.txt' -print)
[[ ${#ROOT_REPORTS[@]} -eq 1 ]]
grep -Fq 'Root command available: yes' "${ROOT_REPORTS[0]}"
grep -Fq '0 Effect Chains' "${ROOT_REPORTS[0]}"

printf 'Diagnostic capture fixture passed.\n'
