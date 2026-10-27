# Experimental bridge verification record

Date: 2026-10-27

## Source/build checks

- Patch applied cleanly to the uploaded Stage 18 source tree with `patch -p1`.
- Clean ARM64 NDK build completed for all manifest sources using NDK r27 /
  Clang 18.0.1, target API 26, C `-std=gnu11`, C++ `-std=c++17`,
  `-fPIC -O2 -DJAMESDSP_REFERENCE_IMPL`, strict undefined-symbol linking, and
  the exact Stage 31 packaged `libphonon.so`.
- Candidate SHA-256:
  `459fe1fe6e5510634a183c5b4972031fa2172cf1dc31ccc108fb494040f41795`.
- Stage 31 reference SHA-256:
  `025bf72e53ccaa600c07699b7650020dff651e1716bd200d2c6d9252bb01ce7d`.
- Both are AArch64 ELF64 with the same `NEEDED` entries and RUNPATH
  `/vendor/lib64/soundfx`; neither has a SONAME. Candidate has 1,025 dynamic
  defined symbols; every candidate export is present in the reference. The
  reference has two additional CPU resolver symbols.
- The candidate links the exact packaged Steam Audio library:
  `857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`.
- The NDK baseline build is not byte-identical to Stage 31. Its SHA-256 is
  `940b14d3db9633feb7adebeebd22acbd4dfc80a602ac6d3583011c47dcb763dc`.

## Offline callback harness

Command:

```sh
experimental/ldac-orbit/scripts/run-host-regression.sh \
  work/patch-check-final/source work/steamaudio_4.8.1/include \
  work/patch-check-final/frame_bridge_test
```

Compiler: Debian g++ 14.2.0 with `-Wall -Wextra -Werror`, ASan, UBSan, and
leak detection. Steam Audio calls are mocked at the API boundary; these
results validate bridge logic and memory safety, not the real HRTF algorithm.

| Rate | Peak | Max adjacent step | Left RMS | Right RMS | Stereo delta RMS | Mean aligned change | Result |
|---:|---:|---:|---:|---:|---:|---:|---|
| 44,100 Hz | 0.170958 | 0.0208415 | 0.0855313 | 0.0303110 | 0.0626755 | 0.0668018 | PASS |
| 48,000 Hz | 0.171172 | 0.0191211 | 0.0856018 | 0.0303193 | 0.0627221 | 0.0667804 | PASS |
| 96,000 Hz | 0.171172 | 0.00958492 | 0.0854162 | 0.0302458 | 0.0626539 | 0.0668695 | PASS |

At all three rates, rendering with fixed 128-frame callbacks and repeated
irregular callbacks (including 1, 64, 96, 127, 128, 129, 192, 240, 256, 480,
512, 882, 960, 1,024, 1,920, and 2,048 frames) produced maximum segmentation
difference 0. Canary checks, finite output, nonzero distinct stereo output,
per-128-frame wet-change checks, peak bound, and discontinuity threshold
passed. Initialization failure bypass/recovery, exact OFF passthrough,
disable transition, reset, 48-to-96 kHz reconfiguration, and 24 repeated
enable/disable transitions passed under sanitizers.

## Not tested

- Real Stage 31 Steam Audio processing on the device, including whether its
  HRTF initializes at 96 kHz.
- LDAC 44.1/48 crackling resolution, LDAC 96 kHz audibility, AAC/USB behavior,
  reboot persistence, or long-duration stability.
- Installation or rollback on a device. No installation was performed.
