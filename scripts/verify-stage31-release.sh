#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

ARCHIVE=${1:?Usage: verify-stage31-release.sh combined.zip [expected-sha256]}
EXPECTED_SHA=${2:-}
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
source "$SCRIPT_DIR/termux-android-tools.sh"
for tool in python unzip sha256sum; do command -v "$tool" >/dev/null || exit 1; done

ACTUAL_SHA=$(sha256sum "$ARCHIVE" | awk '{print $1}')
if [[ -n "$EXPECTED_SHA" && "$ACTUAL_SHA" != "$EXPECTED_SHA" ]]; then
  echo "SHA-256 mismatch: expected $EXPECTED_SHA, got $ACTUAL_SHA" >&2
  exit 1
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
python - "$ARCHIVE" "$TMP/module" <<'PY'
import pathlib, stat, sys, zipfile
archive, target = sys.argv[1:]
root = pathlib.Path(target).resolve()
with zipfile.ZipFile(archive) as z:
    bad = z.testzip()
    if bad:
        raise SystemExit(f"Corrupt ZIP member: {bad}")
    for item in z.infolist():
        path = pathlib.PurePosixPath(item.filename)
        mode = item.external_attr >> 16
        if path.is_absolute() or ".." in path.parts or stat.S_ISLNK(mode):
            raise SystemExit(f"Unsafe ZIP member: {item.filename}")
        if path.suffix.lower() in {".jks", ".keystore", ".p12", ".pfx", ".pem", ".key"}:
            raise SystemExit(f"Signing or private-key material must not be published: {item.filename}")
    names = set(z.namelist())
    required = {
        "module.prop", "README.txt",
        "system/app/JamesDSP/JamesDSP.apk",
        "system/vendor/etc/audio_effects.xml",
        "system/vendor/lib64/soundfx/libjamesdsp_orbit.so",
        "system/vendor/lib64/soundfx/libphonon.so",
        "system/vendor/lib64/soundfx/libc++_shared.so",
        "reference/binary_checksums.txt",
        "reference/vendor_audio_effects_current.xml",
    }
    missing = required - names
    if missing:
        raise SystemExit(f"Missing required ZIP members: {sorted(missing)}")
    z.extractall(root)
PY

MODULE="$TMP/module"
[[ $(sed -n 's/^id=//p' "$MODULE/module.prop") == jamesdsp_orbit_stage29 ]]
cmp "$SCRIPT_DIR/README-Stage31-Combined.txt" "$MODULE/README.txt"
(cd "$MODULE" && sha256sum -c reference/binary_checksums.txt)
cmp "$MODULE/system/vendor/etc/audio_effects.xml" "$MODULE/reference/vendor_audio_effects_current.xml"
python - "$MODULE/system/vendor/etc/audio_effects.xml" <<'PY'
import sys, xml.etree.ElementTree as ET
root = ET.parse(sys.argv[1]).getroot()
namespace = {"a": root.tag.split("}")[0].strip("{")}
libraries = {node.attrib["name"]: node.attrib["path"]
             for node in root.findall(".//a:libraries/a:library", namespace)}
effects = {node.attrib["name"]: node.attrib["library"]
           for node in root.findall(".//a:effects/a:effect", namespace)}
if libraries.get("jamesdsp_orbit") != "libjamesdsp_orbit.so":
    raise SystemExit("Orbit native library entry is missing or incorrect")
if effects.get("jamesdsp_orbit") != "jamesdsp_orbit":
    raise SystemExit("Orbit effect entry is missing or incorrect")
print("Audio effects XML parsed; Orbit library and effect entries are present.")
PY

APKSIGNER=$(find_android_tool apksigner)
AAPT=$(find_android_tool aapt)
APK="$MODULE/system/app/JamesDSP/JamesDSP.apk"
"$APKSIGNER" verify --verbose --print-certs "$APK"
BADGING=$("$AAPT" dump badging "$APK")
grep -q "package: name='james.dsp' versionCode='51' versionName='1.6.14'" <<< "$BADGING"
grep -q "native-code: 'arm64-v8a'" <<< "$BADGING"
printf '%s\n' "$BADGING" | sed -n '1,3p'
printf 'Combined ZIP verified. Size: %s bytes; SHA-256: %s\n' "$(wc -c < "$ARCHIVE" | tr -d ' ')" "$ACTUAL_SHA"
