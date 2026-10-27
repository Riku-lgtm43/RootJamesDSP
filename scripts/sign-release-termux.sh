#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

JKS="$HOME/.jamesdsp-signing/jamesdsp-release.jks"
ALIAS=jamesdsp_release
source "$(dirname "$0")/termux-android-tools.sh"
INPUT="${1:-app/build/outputs/apk/rootFdroid/release/JamesDSP-v1.6.14-51-root-fdroid-universal-release-unsigned.apk}"
OUTPUT="${2:-${INPUT%-unsigned.apk}-signed.apk}"

for tool in keytool sha256sum; do command -v "$tool" >/dev/null || { echo "Missing $tool; install it in Termux" >&2; exit 1; }; done
[[ -r "$JKS" ]] || { echo "Keystore unavailable: $JKS" >&2; exit 1; }
[[ -s "$INPUT" ]] || { echo "Unsigned APK unavailable: $INPUT" >&2; exit 1; }
ZIPALIGN=$(find_android_tool zipalign)
APKSIGNER=$(find_android_tool apksigner)
AAPT=$(find_android_tool aapt)

read -rsp 'Keystore password: ' STORE_PASS; printf '\n' >&2
read -rsp 'Key password: ' KEY_PASS; printf '\n' >&2
TMP=$(mktemp -d)
trap 'unset STORE_PASS KEY_PASS; rm -rf "$TMP"' EXIT
ALIGNED="$TMP/aligned.apk"
mkdir -p "$(dirname "$OUTPUT")"
"$ZIPALIGN" -f -p 4 "$INPUT" "$ALIGNED"
printf '%s\n%s\n' "$STORE_PASS" "$KEY_PASS" | "$APKSIGNER" sign \
  --ks "$JKS" --ks-key-alias "$ALIAS" --ks-pass stdin --key-pass stdin \
  --out "$OUTPUT" "$ALIGNED"
"$APKSIGNER" verify --verbose --print-certs "$OUTPUT"
"$AAPT" dump badging "$OUTPUT" | sed -n '1,2p'
sha256sum "$OUTPUT"
