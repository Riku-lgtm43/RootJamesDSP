# JamesDSP Orbit Stage 31 — High-Rate CIPIC HRTF Experimental Verification

Status: **Experimental KernelSU package. HRTF initialization verified; audio behavior remains device-unverified.**

## Artifact

- Asset: `JamesDSP_Orbit_LDAC_CIPIC_HRTF_Experimental_KernelSU.zip`
- SHA-256: `3e65077f40c83a7cff404d3837d3b5ea98997cb57b9586e5ff3adbf22b9a967e`.
- Module ID: `jamesdsp_orbit_stage29` (in-place opt-in update)
- Effect UUID: `f27317f4-c984-4de6-9a90-545759495bf2`

## Root cause and repair

The exact Stage 31 `libphonon.so` contains embedded default HRTF data only at
24, 44.1, and 48 kHz. The user's direct Termux matrix confirmed default HRTF
creation fails with `IPL_STATUS_INITIALIZATION` at 88.2/96 kHz for frame sizes
128, 256, 512, and 1024. This happens before frame-size-dependent FFT setup.
The same exact library created HRTFs from the included CIPIC 124 SOFA at all
four rates and all four frame sizes.

The candidate retains the embedded default HRTF at 44.1/48 kHz and selects the
bundled CIPIC SOFA file at 88.2/96 kHz. The matrix tests HRTF object creation;
it does not verify audible output through Android's audio effect chain.

## Build and payload integrity

- Source: supplied JamesDSP Stage 18 archive plus the committed
  `experimental/ldac-orbit/stage18-frame-bridge.patch`.
- ARM64 build: Android NDK r27 / Clang 18.0.1; target API 26; strict undefined
  symbol link; exact Stage 31 `libphonon.so` used as the dependency.
- Experimental Orbit library SHA-256:
  `e00daf0a466ed52660fba93fa4a93c61b96982bdbb92259ff2d967f26b163dfa`.
- Bundled `cipic_124.sofa` SHA-256:
  `c28ff4a874ac889ec0c5885ca524762a70d56984232ff7aadcd9c15d32e1cfb6`.
- `libphonon.so` remains byte-identical to Stage 31:
  `857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`.
- JamesDSP APK SHA-256 and certificate are unchanged. APK v2/v3 signatures
  verify; certificate SHA-256:
  `b3180a2a9460dcdeafe8d1d983a82e14e3725e6b1b8a245af7e4357a425ff77b`.
- `libc++_shared.so`, `audio_effects.xml`, `customize.sh`, module ID, effect
  UUID, and every other non-allowlisted payload member match the prior
  frame-bridge package byte-for-byte. No files were deleted.
- Orbit ELF remains AArch64 ELF64 with the same RUNPATH and NEEDED entries.
  No new dynamic exports are introduced versus Stage 31. Object comparison with
  the prior bridge build found only `orbit_SteamSpatial.cpp.o` changed; the
  other 89 objects were byte-identical.
- CIPIC redistribution and attribution notice is included at
  `licenses/CIPIC-HRTF-NOTICE.txt`.

Expected package payload differences from the prior frame-bridge ZIP are only:

- Orbit library, updated to select CIPIC SOFA at 88.2/96 kHz.
- New CIPIC 124 SOFA data and its attribution notice.
- `module.prop`, README, checksum/build references, verification matrices, and
  the updated ZIP integrity log.

## Tests

- User-provided exact-library default and CIPIC HRTF matrices are included in
  this package under `reference/`. CIPIC HRTF creation succeeded at 44.1, 48,
  88.2, and 96 kHz for frame sizes 128/256/512/1024; default data fails only at
  the two high rates.
- Host ASan/UBSan callback regression suite passed at 44.1/48/88.2/96 kHz,
  including irregular callback sizes, frame boundaries, channel balance,
  finite output, sample continuity, failure bypass/recovery, and repeated
  transitions.
  Full sanitizer output is preserved in `reference/host-regression.log`.
  Steam Audio is mocked in this host harness; the exact-library result comes
  from the separate user-run Termux matrix.
- ARM64 cross-build, APK signature verification, required-file hashes, ZIP
  integrity, and file-by-file comparison completed successfully.

## Outstanding device checks and rollback

No module installation or audio playback was performed. Test LDAC 44.1/48/88.2/96,
then AAC and USB briefly at low volume. Confirm the mounted CIPIC SOFA file is
readable and logcat shows high-rate initialization before extended playback.
Android 16 install/reboot behavior, high-rate audibility, AAC/USB regression,
and extended stability/stress testing remain pending. Stop on crackle, pops,
dropouts, silence, or abnormal output.

Rollback: disable the module in KernelSU Manager and reboot, then restore the
saved known-good Stage 31/frame-bridge package and reboot. This package is not a
production-stable release. No automatic installation or system change was made.
