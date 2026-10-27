# Stage 31 engine-audit KernelSU package verification

This is an experimental opt-in update for the existing module. It is not
production stable and has not been installed on a device. Android 16 install
and reboot, LDAC switching, AAC, USB DAC, complete effect combinations, and
extended stability remain pending.

## Source and build identity

- Stage18 source archive SHA-256:
  `52da7707c0e7bc5819597895fbef8bc59e5dcdca72076fec78b28cd8ede5ee5b`.
- Patch order: existing `stage18-frame-bridge.patch`, then
  `stage18-engine-audit.patch`.
- Build: Android NDK r27 (`27.0.12077973`), Clang 18.0.1, API 26, ARM64,
  C GNU11 / C++17, `-O2 -fPIC`, undefined-symbol checks, vendor soundfx
  RUNPATH. No APK build or signing operation.
- Experimental `libjamesdsp_orbit.so` SHA-256:
  `3cbbe92a885e2533c4f14cf7b6a3ff570efa4e0e557a9db493b5d72672ed95bb`.
- ELF is 64-bit AArch64, RUNPATH `/vendor/lib64/soundfx`, and NEEDED entries
  are `libphonon.so`, `liblog.so`, `libm.so`, `libdl.so`, `libc++_shared.so`,
  and `libc.so`.

## Preserved package identity and payloads

- KernelSU module ID: `jamesdsp_orbit_stage29`.
- Orbit implementation UUID:
  `f27317f4-c984-4de6-9a90-545759495bf2`.
- Original Stage 31 combined ZIP SHA-256 remains
  `0799875e20dcf2a05fdf99d08379344d8795641f0d17c99e301e6a994da34b44`.
- Embedded APK SHA-256:
  `e37ee0373e0fbb43147dbe10e7af617d45ab53203339c3904d86c8fba2d96eb2`.
- APK signer certificate SHA-256:
  `b3180a2a9460dcdeafe8d1d983a82e14e3725e6b1b8a245af7e4357a425ff77b`.
- `libphonon.so` SHA-256:
  `857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`.
- `libc++_shared.so` SHA-256:
  `e09c2f45cf4cf8ae574f94b6c2650d99ead0d332d5396f6613f062a2d2d73540`.
- `audio_effects.xml` SHA-256:
  `cec5bcba028986b982825ae43f7d86686689bcbf76481cffa5954680df475c1d`.
- CIPIC 124 SOFA and its license notice match the prior ConfigGuard package;
  the SOFA SHA-256 is
  `c28ff4a874ac889ec0c5885ca524762a70d56984232ff7aadcd9c15d32e1cfb6`.
- No JKS, signing password, alias, or signing secret is included.

## Offline verification

- Patch applies after the frame-bridge patch and reproduces the patched source
  tree byte-for-byte.
- `bash -n` passes for the packaging and regression scripts.
- ASan/UBSan real-engine host suite passes with `-funsigned-char`, including
  exact bypass, callback segmentation, PCM formats, malformed config/parameter
  handling, active crossfeed, active stereo enhancement, and legacy wrapper
  processing before Orbit.
- Baseline ASRC segmentation mismatch is reproduced at 88.2/96 kHz; patched
  output is identical for the fixed/irregular callback schedules at all four
  sample rates.
- Callback-size transition benchmark records mean, p50, p95, p99, and max for
  16 frame sizes at 44.1/48/88.2/96 kHz. The baseline emitted 416 synchronous
  partition warnings; the candidate emitted none. Measurements are
  host-specific and not Android performance claims.
- `apksigner verify --print-certs` passes on the embedded signed APK; the
  certificate fingerprint matches the original Stage 31 package.
- The Steam Audio API is mocked in host tests. Actual HRTF signal output and
  Android audio framework behavior are not certified here.

## Installation and rollback

Back up the installed module, settings, and known-good Stage 31 ZIP before an
opt-in test. Install only as an in-place update through KernelSU Manager. Stop
immediately if there is crackle, popping, dropouts, silence, or abnormal
loudness. To roll back, disable the module in KernelSU Manager and reboot, then
restore the saved Stage 31 ZIP through KernelSU Manager and reboot again.

The candidate ZIP SHA-256 is supplied in the adjacent `.sha256` sidecar and in
the repository `SHA256SUMS` file. The outer archive checksum is deliberately
not duplicated inside its own ZIP.
