#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail
MODULE_ZIP=${1:?Usage: package-stage31-termux.sh STAGE31.zip signed-root.apk [output.zip]}
SIGNED_APK=${2:?Usage: package-stage31-termux.sh STAGE31.zip signed-root.apk [output.zip]}
OUTPUT=${3:-JamesDSP-Stage31-Deployable-Stress-Test-Pending.zip}
EXPECTED_ID=jamesdsp_orbit_stage29
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
for tool in python unzip zip sha256sum; do command -v "$tool" >/dev/null || exit 1; done
python - "$MODULE_ZIP" "$TMP/module" <<'PY'
import pathlib,stat,sys,zipfile
archive,target=sys.argv[1:]; root=pathlib.Path(target).resolve()
with zipfile.ZipFile(archive) as z:
 bad=z.testzip()
 if bad: raise SystemExit(f"Corrupt ZIP: {bad}")
 for i in z.infolist():
  p=pathlib.PurePosixPath(i.filename); mode=i.external_attr>>16
  if p.is_absolute() or '..' in p.parts or stat.S_ISLNK(mode): raise SystemExit(f"Unsafe ZIP entry: {i.filename}")
 z.extractall(root)
PY
M="$TMP/module"
[[ $(sed -n 's/^id=//p' "$M/module.prop") == "$EXPECTED_ID" ]]
for p in system/vendor/lib64/soundfx/libjamesdsp_orbit.so system/vendor/lib64/soundfx/libphonon.so system/vendor/lib64/soundfx/libc++_shared.so system/vendor/etc/audio_effects.xml; do [[ -s "$M/$p" ]]; done
(cd "$M" && sha256sum -c reference/binary_checksums.txt)
SDK=${ANDROID_HOME:-$HOME/Android/Sdk}; BT="$SDK/build-tools/35.0.0"
"$BT/apksigner" verify --verbose --print-certs "$SIGNED_APK"
"$BT/aapt" dump badging "$SIGNED_APK" | grep -m1 "package: name='james.dsp'"
if command -v pm >/dev/null && pm list packages -3 2>/dev/null | grep -q '^package:james\.dsp$'; then echo 'Conflicting user-installed james.dsp; resolve upgrade/signature before package install.' >&2; exit 1; fi
mkdir -p "$M/system/app/JamesDSP"; cp "$SIGNED_APK" "$M/system/app/JamesDSP/JamesDSP.apk"; chmod 0644 "$M/system/app/JamesDSP/JamesDSP.apk"
(cd "$M" && sha256sum -c reference/binary_checksums.txt && zip -qr "$TMP/out.zip" .)
mkdir -p "$(dirname "$OUTPUT")"; cp "$TMP/out.zip" "$OUTPUT"; unzip -t "$OUTPUT"
sha256sum "$M/system/vendor/lib64/soundfx/libjamesdsp_orbit.so" "$M/system/vendor/lib64/soundfx/libphonon.so" "$M/system/vendor/etc/audio_effects.xml" "$SIGNED_APK" "$OUTPUT"
echo 'Stage 31 Deployable — Stress Test Pending'; echo "Preserved module ID: $EXPECTED_ID"
