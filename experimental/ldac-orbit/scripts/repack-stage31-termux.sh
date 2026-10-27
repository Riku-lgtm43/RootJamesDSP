#!/data/data/com.termux/files/usr/bin/bash
# Repackages the already-tested native candidate; does not compile native code.
set -euo pipefail

if [ "$#" -lt 1 ] || [ "$#" -gt 3 ]; then
  echo "usage: $0 /path/to/output/experimental.zip [candidate.zip] [original-stage31.zip]" >&2
  exit 2
fi
OUT_ARG=$1
OUT_DIR=$(dirname "$OUT_ARG")
OUT_NAME=$(basename "$OUT_ARG")
mkdir -p "$OUT_DIR"
OUT_DIR=$(cd "$OUT_DIR" && pwd)
OUT="$OUT_DIR/$OUT_NAME"
[ ! -e "$OUT" ] || { echo "output already exists: $OUT" >&2; exit 2; }

TMP_BASE=${TMPDIR:-${PREFIX:-/data/data/com.termux/files/usr}/tmp}
mkdir -p "$TMP_BASE"
TMP=$(mktemp -d "$TMP_BASE/jdsp-cipic-nolog.XXXXXXXX")
trap 'rm -rf -- "$TMP"' EXIT

RAW_BASE=https://raw.githubusercontent.com/Riku-lgtm43/RootJamesDSP/fix/ldac-orbit-native/experimental/ldac-orbit/artifacts
CANDIDATE_SHA=643357f02277556bc203cecf9e0b43a98f76e38bc85c9f40dde324645cffa19e
STAGE31_SHA=0799875e20dcf2a05fdf99d08379344d8795641f0d17c99e301e6a994da34b44
CANDIDATE="$TMP/candidate.zip"
STAGE31="$TMP/stage31.zip"

if [ "$#" -ge 2 ]; then
  cp "$2" "$CANDIDATE"
else
  curl -fL --retry 3 "$RAW_BASE/JamesDSP_Orbit_LDAC_CIPIC_HRTF_Experimental_KernelSU_NoLogs.zip" -o "$CANDIDATE"
fi
printf '%s  %s\n' "$CANDIDATE_SHA" "$CANDIDATE" | sha256sum -c -
if [ "$#" -ge 3 ]; then
  cp "$3" "$STAGE31"
else
  curl -fL --retry 3 \
    https://github.com/Riku-lgtm43/RootJamesDSP/releases/download/stage31-deployable-stress-test-pending/JamesDSP-Stage31-Combined-Deployable-Corrected.zip \
    -o "$STAGE31"
fi
printf '%s  %s\n' "$STAGE31_SHA" "$STAGE31" | sha256sum -c -
unzip -t "$CANDIDATE" >/dev/null
unzip -t "$STAGE31" >/dev/null

mkdir "$TMP/base" "$TMP/candidate"
unzip -q "$STAGE31" -d "$TMP/base"
unzip -q "$CANDIDATE" -d "$TMP/candidate"

# Confirm all carried-through payloads match the original release before staging.
for rel in \
  system/app/JamesDSP/JamesDSP.apk \
  system/vendor/etc/audio_effects.xml \
  system/vendor/lib64/soundfx/libphonon.so \
  system/vendor/lib64/soundfx/libc++_shared.so \
  customize.sh; do
  cmp -s "$TMP/base/$rel" "$TMP/candidate/$rel" || {
    echo "refusing: protected Stage 31 payload differs: $rel" >&2; exit 1;
  }
done

# Overlay every experimental candidate member except the payloads just proven
# byte-identical to Stage 31. This also carries new files such as the SOFA data,
# license notice, updated verification records, and current module metadata.
PROTECTED=$'system/app/JamesDSP/JamesDSP.apk\nsystem/vendor/etc/audio_effects.xml\nsystem/vendor/lib64/soundfx/libphonon.so\nsystem/vendor/lib64/soundfx/libc++_shared.so\ncustomize.sh'
while IFS= read -r rel; do
  [ -n "$rel" ] || continue
  if printf '%s\n' "$PROTECTED" | grep -Fxq "$rel"; then continue; fi
  mkdir -p "$TMP/base/$(dirname "$rel")"
  unzip -p "$CANDIDATE" "$rel" > "$TMP/base/$rel"
done < <(unzip -Z1 "$CANDIDATE" | sed '/\/$/d')

# Verify package members and their uncompressed bytes exactly match the committed candidate.
(cd "$TMP/base" && sha256sum -c reference/binary_checksums.txt)
CAND_MEMBERS=$(unzip -Z1 "$CANDIDATE" | sed '/\/$/d' | sort)
BASE_MEMBERS=$(cd "$TMP/base" && find . -type f | sed 's#^./##' | sort)
if [ "$CAND_MEMBERS" != "$BASE_MEMBERS" ]; then
  echo "member list differs from candidate:" >&2
  diff -u <(printf '%s\n' "$CAND_MEMBERS") <(printf '%s\n' "$BASE_MEMBERS") >&2 || true
  exit 1
fi
while IFS= read -r rel; do
  cmp -s <(unzip -p "$CANDIDATE" "$rel") "$TMP/base/$rel" || {
    echo "member differs from candidate: $rel" >&2; exit 1;
  }
done <<< "$CAND_MEMBERS"

# Build a fresh ZIP from the verified candidate payload; never overwrite an existing file.
(cd "$TMP/base" && zip -X -qr "$OUT" .)
zip -T "$OUT"
unzip -t "$OUT" >/dev/null
(cd "$OUT_DIR" && sha256sum "$OUT_NAME" > "$OUT_NAME.sha256" && sha256sum -c "$OUT_NAME.sha256")
echo "Repacked verified candidate: $OUT"
echo "ZIP checksum sidecar: $OUT.sha256"
