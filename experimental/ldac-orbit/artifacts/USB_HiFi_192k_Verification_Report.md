# JamesDSP Orbit USB HiFi 192 kHz — Experimental v37 Verification

Status: **experimental candidate; exact Android-library matrix and device playback remain pending.**

## Finding

The 2026-10-28 device capture shows an active JamesDSP effect chain on Apple
Music session 73 at `hifi_playback` I/O 253. AudioFlinger reports stereo-float
192 kHz input and output, so Enhanced mode has attached the effect to the
active USB playback session. This matches the user's report that the other
JamesDSP functions work in Enhanced mode.

The v36 Orbit selector chose bundled CIPIC SOFA only at 88.2/96 kHz. At
192 kHz, it fell through to Steam Audio's embedded default HRTF. The exact
Stage 31 library's prior matrix established that the embedded HRTF supports
24, 44.1, and 48 kHz, and rejects 88.2/96 kHz. Because the exact Android
library has not yet been tested at 192 kHz, the selector mismatch is the
strongest source-level explanation for Orbit not working, but its 192 kHz
initialization failure remains to be confirmed on-device.

The capture separately reports `Momentary freq = 48000 Hz` in the ALSA USB
descriptor while AudioFlinger reports 192 kHz. The mixer/client rate and USB
hardware rate are separate observations; this report makes no bit-perfect or
192 kHz hardware-output claim.

## Change

The selector now retains the embedded HRTF at 24/44.1/48 kHz and chooses
bundled CIPIC SOFA for all other rates, including 192 kHz. It also covers
8/16/32 kHz rates, where the embedded HRTF matrix fails. Steam Audio resamples
the SOFA data for the requested stream rate. The Orbit processing algorithm,
headroom control, app, module ID, and effect UUID are unchanged.

## Build and preserved payloads

- Module ID: `jamesdsp_orbit_stage29`; module version code: 37.
- New ARM64 native library SHA-256:
  `14bb402c9132f6e12a566cffe3c3a2ef8fafacfdf1fadd4581b1c52ce657e458`.
- New `orbit_SteamSpatial.cpp.o` SHA-256:
  `003785fb0e9230822dfd6a1524391f6dcedf33bae4696a9a6951caae3dfa64bc`.
- Only the Orbit spatial object was rebuilt. The other 89 object inputs were
  reused unchanged from the v36 base object set.
- ELF remains AArch64 with RUNPATH `/vendor/lib64/soundfx`; exported symbols,
  NEEDED libraries, and RUNPATH match v36.
- The signed JamesDSP APK/certificate, `libphonon.so`, `libc++_shared.so`, audio
  effects XML, CIPIC SOFA, installer, module ID, and license notice are
  byte-identical to v36. The original Stage 31 release and v36 ZIP are
  unchanged. No signing key or password is included.

## Tests

- ASan/UBSan mocked host callback regression passed at 8, 16, 24, 32, 44.1,
  48, 88.2, 96, 176.4, and 192 kHz. Regular and irregular callback sizes were
  segmentation-equivalent; the JamesDSP connection, Orbit parameter,
  configuration guard, failure bypass/recovery, continuity, and limiter checks
  passed.
- Real Steam Audio 4.8.1 Linux x86-64 HRTF tests passed at 44.1/48/96/192 kHz
  for tone and broadband signals. At 192 kHz, output remained finite, peaked at
  0.95, had no full-scale samples, and matched across regular/irregular
  callbacks. This validates the algorithm and SOFA import against the Linux
  library, not the Android ARM64 binary.
- A Steam Audio 4.8.1 Linux matrix created CIPIC HRTFs at all four tested frame
  sizes (128/256/512/1024) for 8 through 192 kHz. Embedded-default failures at
  8/16/32/88.2/96/176.4/192 kHz matched the selector's expanded rate set.
- `diagnostics/hrtf_init_matrix.cpp` now includes the expanded rates so the
  exact Android ARM64 `libphonon.so` can be checked from Termux. That device
  matrix has not yet been run at 192 kHz.
- The attached device dump recorded zero active-track underruns at capture
  time, but does not establish long-session Enhanced-mode stability. USB
  reconnect, Android 16 reboot, AAC/USB regression, and extended stress testing
  remain pending.

## Device test and rollback

Use only as an in-place experimental KernelSU update after backing up the
installed module, settings, and known-good Stage 31 ZIP. Test briefly at low
volume with Orbit OFF/ON at USB 192 kHz, then verify ordinary 44.1/48 kHz
playback and AAC/USB controls. Stop on crackle, pops, dropouts, silence,
abnormal balance, or volume. To roll back, disable the module in KernelSU,
reboot, restore the saved Stage 31 ZIP, and reboot again.

This candidate fixes Orbit's HRTF-rate selection. It does not change the
Enhanced-mode session service or resolve its reported stability concerns. It
is not a production-stable release.

## Package checksum

The final ZIP SHA-256 is published in the matching `.sha256` sidecar and
`SHA256SUMS`. The checksum is kept outside the ZIP to avoid a self-referential
archive hash.
