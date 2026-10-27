# JamesDSP Orbit Stage 31 — no diagnostic logging CIPIC HRTF build

Status: **Experimental KernelSU update. Short user-reported LDAC playback passed; extended testing remains pending.**

## Candidate

- Module ID: `jamesdsp_orbit_stage29`
- Effect UUID: `f27317f4-c984-4de6-9a90-545759495bf2`
- Native Orbit library SHA-256:
  `697f0caf67a4b84069c8c6c5ece80e242f254a5eb27b46cf90e4c3198d07b813`
- Bundled CIPIC SOFA SHA-256:
  `c28ff4a874ac889ec0c5885ca524762a70d56984232ff7aadcd9c15d32e1cfb6`
- Exact Stage 31 Steam Audio library SHA-256:
  `857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`

## Source and change scope

Built from the supplied archival Stage 18 tree plus the committed patch. The change removes active Orbit route/configuration/HRTF diagnostic logging
and the associated status field. It retains the 128-frame bridge, HRTF routing,
and failure-safe bypass; startup remains dry until a full block is ready, then
crossfades to wet, while quick disable carries the current fade gain. Reset and
profile changes clear buffered bridge audio. At high rates a failed CIPIC HRTF
is no longer retried against the known-unsupported embedded default. Object
comparison with the immediately preceding CIPIC build found only
`legacy_orbit_c.cpp.o` and `orbit_SteamSpatial.cpp.o` changed; the other 88 of
90 objects are byte-identical. Steady-state DSP algorithms, APK, Steam Audio
library, and certificate were not changed.

Built with Android NDK r27 / Clang 18.0.1 for `aarch64-linux-android26`, strict
undefined-symbol linking. ELF checks: AArch64 ELF64, RUNPATH
`/vendor/lib64/soundfx`, unchanged NEEDED entries and dynamic exports. The
existing `liblog.so` dependency remains because other Stage 31 source files
use Android logging; the Orbit-specific diagnostic strings and calls are absent
from this build.

## Device evidence

The user's rooted capture identified the actual negotiated LDAC configurations
as 44.1 kHz, 48 kHz, and 96 kHz. The phase labeled 90 kHz negotiated 96 kHz.
AudioFlinger showed the JamesDSP effect active at those routes. Captured Orbit
logs from the preceding diagnostic build showed successful Steam Audio
initialization at 44.1/48 kHz and CIPIC SOFA selection plus initialization at
96 kHz. The user then reported: no crackling or popping at 44.1/48 kHz and
Orbit audible at 96 kHz.

Those playback results apply to the same processing/HRTF code before diagnostic
log removal. The final binary also changes startup and quick-disable transition handling,
clears stale bridge buffers on reset/profile changes, and skips a known-invalid
high-rate default-HRTF retry. The user's listening report predates these small
edge-case refinements; this exact ZIP has not been installed or listened to on-device. AAC/USB,
Android 16 update/reboot behavior, and extended stability/stress testing remain
pending.

## Regression and package verification

The ASan/UBSan host regression suite passed at 44.1, 48, 88.2, and 96 kHz for
128-frame and irregular callback segmentation. It checked callback canaries,
finite and bounded output, stereo separation, segmentation equivalence,
initialization failure and recovery, exact high-rate SOFA routing, bypass,
startup/quick-disable continuity, bridge reset, reconfiguration, and 24 repeated
transitions.
Steam Audio calls are mocked in this host test; real HRTF initialization was
verified separately against the exact packaged library and supplied matrices.

Package checks confirm:

- Signed JamesDSP APK and certificate are unchanged; APK v2/v3 signatures
  verify.
- `audio_effects.xml`, `libc++_shared.so`, `libphonon.so`, `customize.sh`,
  CIPIC SOFA, and the license notice match their verified source hashes.
- Module ID and effect UUID remain unchanged.
- ZIP integrity and all declared payload hashes pass.
- No JKS, signing passwords, or signing credentials are packaged.

The signed Stage 31 release and the earlier experimental ZIP remain unchanged.

## Precautions and rollback

Back up the installed module and known-good Stage 31 ZIP. Install only as an
in-place opt-in update through KernelSU Manager at low volume. Stop on any
abnormal audio. Roll back by disabling the module, rebooting, restoring the
saved package, and rebooting again. Do not describe this candidate as
production-stable.
