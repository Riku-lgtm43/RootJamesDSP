# JamesDSP Orbit v39 — 192 kHz quality and Enhanced PCM fallback

**Status:** Experimental candidate. Not installed on the user's device.

## Changes

- At exactly 192,000 Hz, Steam Audio's HRTF volume is set to 0.25. The
  44.1/48/96 kHz paths and the 176.4 kHz path keep their previous volume.
  This is a targeted compensation for the measured CIPIC level increase at
  192 kHz; it does not change the Orbit blend, bass preservation, dry signal,
  or JamesDSP DSP algorithms.
- When the JamesDSP stereo DSP core rejects a configuration whose matching
  input/output rates, PCM format, and channel masks can be safely copied, the
  effect now acknowledges the configuration and copies audio byte-for-byte.
  JamesDSP and Orbit processing are bypassed for that unsupported stream, so a
  rejected mono or multichannel PCM route can continue playing instead of
  losing audio. Supported stereo routes keep the existing processing path.

The second change addresses a concrete failure mode in Enhanced mode: the DSP
core accepts stereo layouts only, while Enhanced mode can attach its effect to
other session layouts. It is a defensive compatibility fix; the supplied logs
do not include TikTok/Facebook stream configurations, so their device behavior
still needs confirmation.

## Build and protected payloads

- Reconstructed from the supplied Stage 18 source using
  `stage18-frame-bridge.patch` followed by `high-rate-hrtf-level.patch`. Both
  patches apply cleanly; the resulting Orbit source and effect wrapper match
  the tested source.
- Android NDK r27, Clang, target API 26, and the exact packaged Stage 31
  `libphonon.so` (`857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`).
  A relink with the prior objects and prior Orbit object reproduced the v38
  native library hash exactly before applying the two changes.
- The ARM64 rebuild changes only `jamesdsp.c.o` (effect wrapper) and
  `orbit_SteamSpatial.cpp.o`. The other 88 of 90 native objects, including the
  JamesDSP DSP core and its effect algorithms, are byte-identical to the v38
  object set.
- The output is AArch64 ELF64. Its exported symbols, `NEEDED` libraries, and
  `/vendor/lib64/soundfx` RUNPATH match v38.
- Module ID stays `jamesdsp_orbit_stage29`. The signed APK, signing
  certificate, `libphonon.so`, `libc++_shared.so`, audio effects XML, CIPIC
  SOFA, license notice, and installer are preserved from v38. No JKS or
  signing secret is included. The Stage 31 release and v38 ZIP are unchanged.

## Verification

- `run-host-regression.sh` passes under ASan/UBSan for 8, 16, 24, 32, 44.1,
  48, 88.2, 96, 176.4, and 192 kHz, including callback segmentation,
  initialization recovery, enable/disable transitions, and JamesDSP-to-Orbit
  routing.
- The effect connection regression verifies safe pass-through after rejected
  stereo, mono-float, and 5.1 PCM16 configurations. It checks byte-exact
  preservation for mono and multichannel streams.
- The real-HRTF suite passes at 44.1, 48, 96, and 192 kHz for tones and
  broadband input, with finite output, no clipping, a 0.95 peak ceiling, and
  identical output across callback segmentation.
- The 8/12/16/18 kHz response sweep compares both inputs and outputs against
  the 48 kHz reference. At 192 kHz, the maximum difference is 1.34 dB. The
  uncorrected v38 Orbit source fails the same check at 11.53 dB on the first
  probe.
- `apksigner` verifies the preserved APK with v2 and v3 signatures; its
  certificate SHA-256 is
  `b3180a2a9460dcdeafe8d1d983a82e14e3725e6b1b8a245af7e4357a425ff77b`.
- New native library SHA-256:
  `f0d87dc6ecb67c6888890b274e10283a02dbcf1d5fe727a0500e534a1ff7d4b7`.

Host tests use Steam Audio 4.8.1 Linux x86-64. They do not replace listening
through the Android ARM64 library and the actual DAC.

## Device checks still required

No device was connected for this build. After reviewing the ZIP, test with low
volume and the master switch on/off:

1. Apple Music at negotiated USB 192 kHz with Orbit enabled; compare the same
   track against v38 and report whether the harsh/bright character is reduced.
2. TikTok and Facebook, each with a brief 3–5 second video. Confirm sound
   returns with the master switch on. Mono/multichannel routes may intentionally
   run without JamesDSP effects through the safe bypass.
3. Ordinary 44.1/48 kHz playback, AAC, LDAC, USB routing, and Android 16 reboot.
4. Longer stability and stress testing.

The candidate is not production-stable. If audio becomes silent or distorted,
disable the module in KernelSU Manager, reboot, restore the backed-up known-good
module, and reboot again. Install only as an in-place update; do not install a
second module with the same ID.
