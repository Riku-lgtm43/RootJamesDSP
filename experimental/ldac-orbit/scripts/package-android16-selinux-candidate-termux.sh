#!/data/data/com.termux/files/usr/bin/bash
# Repackage the committed engine-audit candidate with corrected vendor config
# labels. This script does not build native code or install the module.
set -euo pipefail

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  echo "usage: $0 /path/to/output.zip [engine-audit-candidate.zip]" >&2
  exit 2
fi

OUT_ARG=$1
OUT_DIR=$(dirname "$OUT_ARG")
OUT_NAME=$(basename "$OUT_ARG")
mkdir -p "$OUT_DIR"
OUT_DIR=$(cd "$OUT_DIR" && pwd)
OUT="$OUT_DIR/$OUT_NAME"
[ ! -e "$OUT" ] || { echo "output already exists: $OUT" >&2; exit 2; }

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
PACKAGE_DIR="$ROOT_DIR/package/android16-selinux-fix"
BASE_SHA=2cc9c73689844fc88e43c0df99330825acd916416158ad3e8372148c68e41228
BASE_URL=https://raw.githubusercontent.com/Riku-lgtm43/RootJamesDSP/fix/ldac-orbit-native/experimental/ldac-orbit/artifacts/JamesDSP_Orbit_LDAC_CIPIC_HRTF_Experimental_KernelSU_EngineAudit.zip

if [ -n "${TMPDIR:-}" ]; then
  TMP_BASE=$TMPDIR
elif [ -n "${PREFIX:-}" ]; then
  TMP_BASE="$PREFIX/tmp"
else
  TMP_BASE=/tmp
fi
mkdir -p "$TMP_BASE"
TMP=$(mktemp -d "$TMP_BASE/jdsp-android16-selinux.XXXXXXXX")
trap 'rm -rf -- "$TMP"' EXIT
BASE="$TMP/engine-audit.zip"

if [ "$#" -eq 2 ]; then
  cp "$2" "$BASE"
else
  curl -fL --retry 3 "$BASE_URL" -o "$BASE"
fi
printf '%s  %s\n' "$BASE_SHA" "$BASE" | sha256sum -c -
unzip -tq "$BASE"

mkdir "$TMP/module"
unzip -q "$BASE" -d "$TMP/module"
M="$TMP/module"

[[ $(sed -n 's/^id=//p' "$M/module.prop") == jamesdsp_orbit_stage29 ]]
for item in \
  'system/vendor/lib64/soundfx/libjamesdsp_orbit.so 3cbbe92a885e2533c4f14cf7b6a3ff570efa4e0e557a9db493b5d72672ed95bb' \
  'system/vendor/lib64/soundfx/libphonon.so 857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369' \
  'system/vendor/lib64/soundfx/libc++_shared.so e09c2f45cf4cf8ae574f94b6c2650d99ead0d332d5396f6613f062a2d2d73540' \
  'system/app/JamesDSP/JamesDSP.apk e37ee0373e0fbb43147dbe10e7af617d45ab53203339c3904d86c8fba2d96eb2' \
  'system/vendor/etc/audio_effects.xml cec5bcba028986b982825ae43f7d86686689bcbf76481cffa5954680df475c1d' \
  'system/vendor/etc/axionfx/hrtf/cipic_124.sofa c28ff4a874ac889ec0c5885ca524762a70d56984232ff7aadcd9c15d32e1cfb6'; do
  read -r rel expected <<< "$item"
  actual=$(sha256sum "$M/$rel" | cut -d' ' -f1)
  [ "$actual" = "$expected" ] || { echo "protected payload changed: $rel" >&2; exit 1; }
done

cp "$PACKAGE_DIR/customize.sh" "$M/customize.sh"
cp "$PACKAGE_DIR/module.prop" "$M/module.prop"
cp "$PACKAGE_DIR/README.txt" "$M/README.txt"
cp "$ROOT_DIR/ANDROID16_SELINUX_FAILURE_ANALYSIS.md" "$M/reference/ANDROID16_SELINUX_FAILURE_ANALYSIS.md"
cp "$ROOT_DIR/ANDROID16_SELINUX_CANDIDATE_VERIFICATION.md" "$M/reference/ANDROID16_SELINUX_CANDIDATE_VERIFICATION.md"

grep -Fq 'u:object_r:vendor_file:s0' "$M/customize.sh"
grep -Fq 'u:object_r:vendor_configs_file:s0' "$M/customize.sh"
grep -Fq 'set_perm_recursive "$MODPATH/system/vendor/etc"' "$M/customize.sh"
! grep -Fq 'allow audioserver vendor_file' "$M/customize.sh"

find "$M" -type f -exec touch -t 202610270000 {} +
(cd "$M" && find . -type f -print | LC_ALL=C sort | sed 's#^\./##' > "$TMP/files.list")
(cd "$M" && zip -X -q -9 "$OUT" -@ < "$TMP/files.list")
unzip -tq "$OUT"

# Recheck every payload whose bytes must remain identical to Stage 31 / Engine Audit.
mkdir "$TMP/verify"
unzip -q "$OUT" -d "$TMP/verify"
for item in \
  'system/vendor/lib64/soundfx/libjamesdsp_orbit.so 3cbbe92a885e2533c4f14cf7b6a3ff570efa4e0e557a9db493b5d72672ed95bb' \
  'system/vendor/lib64/soundfx/libphonon.so 857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369' \
  'system/vendor/lib64/soundfx/libc++_shared.so e09c2f45cf4cf8ae574f94b6c2650d99ead0d332d5396f6613f062a2d2d73540' \
  'system/app/JamesDSP/JamesDSP.apk e37ee0373e0fbb43147dbe10e7af617d45ab53203339c3904d86c8fba2d96eb2' \
  'system/vendor/etc/audio_effects.xml cec5bcba028986b982825ae43f7d86686689bcbf76481cffa5954680df475c1d' \
  'system/vendor/etc/axionfx/hrtf/cipic_124.sofa c28ff4a874ac889ec0c5885ca524762a70d56984232ff7aadcd9c15d32e1cfb6'; do
  read -r rel expected <<< "$item"
  actual=$(sha256sum "$TMP/verify/$rel" | cut -d' ' -f1)
  [ "$actual" = "$expected" ] || { echo "output payload changed: $rel" >&2; exit 1; }
done

(cd "$OUT_DIR" && sha256sum "$OUT_NAME" > "$OUT_NAME.sha256" && sha256sum -c "$OUT_NAME.sha256")
echo "Created experimental, uninstalled package: $OUT"
echo "ZIP checksum sidecar: $OUT.sha256"
