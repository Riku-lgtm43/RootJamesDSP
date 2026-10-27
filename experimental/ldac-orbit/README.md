# LDAC Orbit frame bridge candidate

This is an opt-in experimental native update for the existing Stage 31
KernelSU module. It keeps module ID `jamesdsp_orbit_stage29` and effect UUID
`f27317f4-c984-4de6-9a90-545759495bf2`. It does not alter the published
Stage 31 ZIP or install anything automatically.

## Evidence and scope

The user confirmed Orbit crackle/pops on LDAC 44.1 kHz and harsher pops at
48 kHz, while Orbit is inaudible on LDAC 96 kHz. The supplied device dump
shows an active stereo-float JamesDSP effect at 96 kHz with 1,920-frame
buffers, and the LDAC codec state also reports 96 kHz. It does not report the
Orbit parameter state or Steam Audio initialization result, so the high-rate
silence cause remains unconfirmed.

In the supplied Stage 18 archival source, `SteamSpatial::process()` used
fixed 128-frame Steam Audio blocks and passed a shorter callback tail through
dry. This aligns with 44.1 kHz 882-frame and 48 kHz 960-frame callback sizes,
but the user dump does not contain a per-callback trace. Treat this as a
confirmed source behavior and a plausible crackle mechanism, not proof of the
device root cause.

The patch adds a preallocated per-instance 128-frame bridge, a bounded 128
frame processing latency, a short fade when the first processed block becomes
available, and a bounded fade back to current dry audio when Orbit is turned
off. It also logs accepted/rejected effect configuration and Steam Audio
initialization results outside the per-buffer processing loop. It does not
change JamesDSP's algorithms, parameter IDs, app, APK, Steam Audio library,
other native dependencies, effect UUID, or audio XML.

## Provenance and build

The patch targets the uploaded `JamesDSP_Stage18_Full_Link` source archive,
not an asserted exact Stage 31 source checkout. Apply it from that source
tree's root, where `source/` is present:

```sh
patch -p1 < stage18-frame-bridge.patch
```

The candidate was built for ARM64 with Android NDK r27 / Clang 18.0.1,
`-std=c++17 -fPIC -O2 -DJAMESDSP_REFERENCE_IMPL`, target API 26, strict
undefined-symbol linking, and the exact `libphonon.so` extracted from the
published Stage 31 package (SHA-256
`857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`). It
retains `RUNPATH=/vendor/lib64/soundfx` and Stage 31's `NEEDED` library list.
Its dynamic exports introduce no new symbols relative to the Stage 31
reference; the two Stage 31-only CPU resolver symbols are absent from both
the baseline rebuild and candidate.

The supplied Stage 18 and Stage 31 reference have compatible observed effect
exports, but the Stage 18 source is archival and the local NDK baseline is
not byte-identical to the Stage 31 ELF. The user-reported historical
byte-equality result after RUNPATH patching was not independently reproduced
here.

## Offline regression test

The deterministic host harness mocks only the Steam Audio API boundary; it
does not claim to model the real HRTF output. Run it with the patched source
and the Steam Audio headers:

```sh
experimental/ldac-orbit/scripts/run-host-regression.sh \
  /path/to/JamesDSP_Stage18_Full_Link/source \
  /path/to/steamaudio/include
```

It exercises 44.1/48/96 kHz, divisible and irregular callbacks including
1, 64, 96, 127, 128, 129, 192, 240, 256, 480, 512, 882, 960, 1,024, 1,920,
and 2,048 frames, callback segmentation equivalence, canaries, finite output,
stereo separation, peak and discontinuity bounds, disabled passthrough,
initialization failure/recovery, disable transition, reset, and rate change.
The harness was run with ASan and UBSan and passed. Hardware LDAC, AAC, and USB
playback, actual Steam Audio HRTF output, and stress testing remain unverified.

## Device diagnostics and rollback

This candidate is for a short, controlled opt-in test only. Back up the
installed module and the known-good Stage 31 ZIP first. Leave Orbit off on
LDAC until testing. In KernelSU Manager, update the existing module with the
experimental ZIP; do not install it as a second module. If the candidate
causes audio problems, disable it in KernelSU Manager and reboot, then restore
the backed-up Stage 31 ZIP and reboot. Keep the current signed app, module
backup, and settings backup. Never flash the ZIP from recovery on an unknown
ROM.

Check `logcat -s JamesDSPOrbit AxionFxSpatial` for route configuration and
HRTF initialization status. The logs do not prove that the spatial output is
audible. Test short playback first: Orbit OFF then ON at LDAC 44.1, 48, and
96 kHz, followed by AAC and USB controls. Stop a route immediately on
crackle, popping, dropout, or abnormal level. Do not perform extended playback
until every short test is clean. Do not call LDAC fixed until the user reports
device results.

## 96 kHz HRTF initialization investigation

The supplied device diagnostic reports that the experimental library
initialized Steam Audio at 48 kHz, then `iplHRTFCreate` returned error 3 at
96 kHz. Inspection of the exact Stage 31 `libphonon.so` confirms its embedded
default HRTF database lists only 24, 44.1, and 48 kHz. Steam Audio 4.8.1 source
shows that its default HRTF map throws `Status::Initialization` when an exact
sampling-rate entry is absent; `CContext::createHRTF` returns that as
`IPL_STATUS_INITIALIZATION`. The HRTF map is constructed before the FFT
objects, so changing `frameSize` cannot make the default 96 kHz HRTF initialize.
This identifies the failing condition for the default profile and matches the
reported error. The Stage 18 source used for the candidate initializes
`mHrtfProfile` to 0, and `setHrtfProfile` has no caller in that source tree, so
the shipped path defaults to the embedded database. A device-provided SADIE
SOFA file would be a separate path; its actual availability and use still need
device confirmation. The official header documents `frameSize` as samples per
channel and does not restrict it to particular positive sizes. The exact
Stage 31 `libphonon.so` has SHA-256
`857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`, build ID
`020c11ef0d34b1431c84615ccf48973fd417e224`, and NEEDED dependencies `liblog`,
`libm`, `libdl`, and `libc`. The embedded HRTF resource and sample-rate lookup
function are byte-identical in the packaged library and Steam Audio 4.8.1 SDK
library. This source/binary evidence establishes why the default profile
fails, but the requested real-library Termux matrix has not been run on the
device. No 96 kHz workaround or replacement package has been made.

The diagnostic source in `diagnostics/` builds a standalone Termux executable
which calls the original library directly. It tests default HRTF creation at
44.1, 48, 88.2, and 96 kHz with frame sizes 128, 256, 512, and 1024. If the
exact SADIE H12 path is readable, it repeats the matrix using that SOFA file.
It does not alter vendor files or install a module. Run this complete block in
Termux. It installs Termux-only build utilities, copies the installed library
read-only into Termux private storage, downloads the diagnostic source and
headers from this branch, and saves the matrix output to shared Downloads:

```sh
set -euo pipefail
pkg install -y clang binutils coreutils curl
termux-setup-storage
mkdir -p "$HOME/jdsp-hrtf-matrix/include" /sdcard/Download
BASE=https://raw.githubusercontent.com/Riku-lgtm43/RootJamesDSP/fix/ldac-orbit-native/experimental/ldac-orbit/diagnostics
curl -fL "$BASE/hrtf_init_matrix.cpp" -o "$HOME/jdsp-hrtf-matrix/hrtf_init_matrix.cpp"
curl -fL "$BASE/run-termux-hrtf-matrix.sh" -o "$HOME/jdsp-hrtf-matrix/run-termux-hrtf-matrix.sh"
curl -fL "$BASE/include/phonon.h" -o "$HOME/jdsp-hrtf-matrix/include/phonon.h"
curl -fL "$BASE/include/phonon_version.h" -o "$HOME/jdsp-hrtf-matrix/include/phonon_version.h"
su -c 'sha256sum /vendor/lib64/soundfx/libphonon.so'
su -c 'cat /vendor/lib64/soundfx/libphonon.so' > "$HOME/libphonon-stage31.so"
bash "$HOME/jdsp-hrtf-matrix/run-termux-hrtf-matrix.sh" \
  "$HOME/libphonon-stage31.so" \
  /vendor/etc/axionfx/hrtf/sadie_h12.sofa 2>&1 \
  | tee /sdcard/Download/JamesDSP_HRTF_Compatibility_Matrix.txt
```

The runner refuses any library whose SHA-256 differs from the known Stage 31
copy. It records context creation status separately from the full numeric and
named HRTF error, and releases every created HRTF/context. A readable SOFA path
is tested directly; an unavailable one is explicitly skipped. Return
`/sdcard/Download/JamesDSP_HRTF_Compatibility_Matrix.txt` and the device's `sha256sum` for the installed
`/vendor/lib64/soundfx/libphonon.so` before choosing a code change. No source
patch or replacement ZIP has been made for the 96 kHz issue.

## Termux package reassembly

The exact tested candidate ZIP and its checksum are committed under
`artifacts/` on branch `fix/ldac-orbit-native`. Download the [exact ZIP from
the branch](https://raw.githubusercontent.com/Riku-lgtm43/RootJamesDSP/fix/ldac-orbit-native/experimental/ldac-orbit/artifacts/JamesDSP_Orbit_LDAC_FrameBridge_Experimental_KernelSU.zip)
if an in-chat attachment cannot be opened. Its SHA-256 is
`44388287d08151490dac520c24038eb25a83e6a1aefe04479adc5c6778a90064`.
The Termux helper
reassembles the same uncompressed package members over the original Stage 31
ZIP using the already-tested Orbit library from the committed candidate. It
does not compile or alter native DSP. It checks both input ZIP hashes,
confirms the signed APK, support libraries, audio XML, and installer match
Stage 31, compares every final member byte-for-byte with the committed
candidate, then writes and verifies an output checksum sidecar. ZIP container
metadata can make the reassembled ZIP's outer SHA differ from the committed
ZIP while all uncompressed member bytes remain identical.

Install the Termux utilities if needed, then fetch and run the helper directly:

```sh
pkg install curl zip unzip coreutils findutils diffutils
curl -fL \
  https://raw.githubusercontent.com/Riku-lgtm43/RootJamesDSP/fix/ldac-orbit-native/experimental/ldac-orbit/scripts/repack-stage31-termux.sh \
  -o "$HOME/repack-stage31-termux.sh"
bash "$HOME/repack-stage31-termux.sh" \
  "$HOME/storage/downloads/JamesDSP_Orbit_LDAC_FrameBridge_Experimental_KernelSU.zip"
```

The script downloads the candidate from the branch and the original Stage 31
release ZIP. It also accepts local inputs:

```sh
bash experimental/ldac-orbit/scripts/repack-stage31-termux.sh \
  "$HOME/storage/downloads/JamesDSP_Orbit_LDAC_FrameBridge_Experimental_KernelSU.zip" \
  experimental/ldac-orbit/artifacts/JamesDSP_Orbit_LDAC_FrameBridge_Experimental_KernelSU.zip \
  "$HOME/storage/downloads/JamesDSP-Stage31-Combined-Deployable-Corrected.zip"
```
