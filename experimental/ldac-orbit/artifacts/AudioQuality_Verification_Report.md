# JamesDSP Orbit Stage 31 — Audio Quality Guard Verification

Status: experimental opt-in candidate. Android device playback has not yet been
performed for this build.

## Cause and fix

Real Steam Audio 4.8.1 host measurements of the v35 Orbit code exceeded float
full scale. At 48 kHz, a 0.95 peak tone reached 1.100 and deterministic stereo
broadband input reached 1.993. At 96 kHz, those tests reached 1.617 and 2.590.
The 96 kHz HRTF used the packaged CIPIC SOFA. These peaks could be clipped by a
later audio stage and create harsh distortion.

This build enables Steam Audio's RMS HRTF normalization and applies a linked
gain to each completed 128-frame stereo block. The gain attacks immediately to
the measured 0.95 ceiling and releases over 60 ms. Non-finite output is replaced
with the corresponding original input sample. It adds no allocations, changes
no Orbit controls, and leaves JamesDSP's DSP core unchanged.

## Build provenance

- Module ID: `jamesdsp_orbit_stage29`; version code 36.
- Only `orbit_SteamSpatial.cpp.o` was rebuilt. The other 89 objects were reused
  byte-for-byte from the v35 ConfigGuard candidate.
- The v35 `orbit_SteamSpatial.cpp.o` was independently rebuilt from commit
  `cf06a9d` with Android NDK r27 / Clang 18.0.1 and matched the packaged object
  exactly: `8d688b7e190fbc39dfe2c7b1f4d09da5fd8d8dcc32572f79e8dd6bda4b3c890e`.
- New Orbit object SHA-256:
  `f67d92da547f699abbae3536d74ee182b8ac903ddb37c464f623fbb407ac7784`.
- New ARM64 shared library SHA-256:
  `1e10b178b652ac312f3af805015acbd85830d17e5e89f1c04c952d4d36445a5d`.
- ELF remains AArch64 with RUNPATH `/vendor/lib64/soundfx`. NEEDED entries and
  dynamic exports match v35.
- Signed APK, certificate, `audio_effects.xml`, `libc++_shared.so`,
  `libphonon.so`, `customize.sh`, CIPIC SOFA, and license notice match v35.

## Tests

The ASan/UBSan callback and effect-connection regressions passed. A synthetic
5x HRTF gain regression verified the stereo-linked limiter's peak ceiling and
finite output. A NaN HRTF injection verified dry fallback and recovery of the
filter state when the HRTF output became finite again.

The real-HRTF host test linked Steam Audio 4.8.1 Linux x86-64 and used the
packaged CIPIC data. At 44.1, 48, and 96 kHz, tone and deterministic broadband
stereo signals produced no samples at or above full scale; peaks remained at or
below 0.95. Regular 1,920-frame callbacks and irregular callbacks were exactly
equivalent in all six cases.

The Linux Steam Audio library is the same SDK version but is not the exact
Android ARM64 binary. These are offline signal tests, not device listening
results. LDAC 44.1/48/96, AAC, USB, Android 16 install/reboot, and extended
stability tests remain pending.

## Package checksum

- ZIP SHA-256: `2be9ef3b34f0c4201d3d78d0dcc9463072e0886e50a3ac1fa55c2948d067c3fa`
- Native Orbit library SHA-256: `1e10b178b652ac312f3af805015acbd85830d17e5e89f1c04c952d4d36445a5d`
