#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

JKS="$HOME/.jamesdsp-signing/jamesdsp-release.jks"
ALIAS=jamesdsp_release
BUILD_TOOLS="${ANDROID_HOME:-$HOME/Android/Sdk}/build-tools/35.0.0"
INPUT="${1:-app/build/outputs/apk/rootFdroid/release/JamesDSP-v1.6.14-51-root-fdroid-universal-release-unsigned.apk}"
OUTPUT="${2:-${INPUT%-unsigned.apk}-signed.apk}"

for tool in keytool sha256sum; do command -v "$tool" >/dev/null || { echo "Missing $tool" >&2; exit 1; }; done
[[ -r "$JKS" ]] || { echo "Keystore unavailable: $JKS" >&2; exit 1; }
[[ -s "$INPUT" ]] || { echo "Unsigned APK unavailable: $INPUT" >&2; exit 1; }
[[ -x "$BUILD_TOOLS/zipalign" && -x "$BUILD_TOOLS/apksigner" ]] || {
  echo "Install Android build-tools 35.0.0 and set ANDROID_HOME" >&2; exit 1;
}

read -rsp 'Keystore password: ' STORE_PASS; printf '\n' >&2
read -rsp 'Key password: ' KEY_PASS; printf '\n' >&2
TMP=$(mktemp -d)
trap 'unset STORE_PASS KEY_PASS; rm -rf "$TMP"' EXIT
ALIGNED="$TMP/aligned.apk"
mkdir -p "$(dirname "$OUTPUT")"
"$BUILD_TOOLS/zipalign" -f -p 4 "$INPUT" "$ALIGNED"
printf '%s\n%s\n' "$STORE_PASS" "$KEY_PASS" | "$BUILD_TOOLS/apksigner" sign \
  --ks "$JKS" --ks-key-alias "$ALIAS" --ks-pass stdin --key-pass stdin \
  --out "$OUTPUT" "$ALIGNED"
"$BUILD_TOOLS/apksigner" verify --verbose --print-certs "$OUTPUT"
"$BUILD_TOOLS/aapt" dump badging "$OUTPUT" | sed -n '1,2p'
sha256sum "$OUTPUT"
