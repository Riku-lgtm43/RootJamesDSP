# Experimental LDAC bridge and high-rate HRTF verification

Date: 2026-10-27

## Source and ARM64 build

- The complete patch applies cleanly to the supplied Stage 18 source tree with
  `patch -p1` and reproduces the working source in `work/ldac-candidate/source`.
- Built ARM64 with Android NDK r27 / Clang 18.0.1, target API 26, C `-std=gnu11`,
  C++ `-std=c++17`, `-fPIC -O2 -DJAMESDSP_REFERENCE_IMPL`, strict undefined
  symbol linking, and the exact Stage 31 `libphonon.so`.
- Prior Orbit library SHA-256:
  `e00daf0a466ed52660fba93fa4a93c61b96982bdbb92259ff2d967f26b163dfa`.
- No-diagnostic-logging Orbit library SHA-256:
  `697f0caf67a4b84069c8c6c5ece80e242f254a5eb27b46cf90e4c3198d07b813`.
- Unmodified Stage 31 reference Orbit library SHA-256:
  `025bf72e53ccaa600c07699b7650020dff651e1716bd200d2c6d9252bb01ce7d`.
- Stage 31 `libphonon.so` SHA-256:
  `857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`.
- New library is AArch64 ELF64, retains RUNPATH `/vendor/lib64/soundfx`, and
  has the same `NEEDED` library list and dynamic exports as the prior bridge
  build. It introduces no exports absent from Stage 31.
- Object comparison against the immediately preceding CIPIC build found only
  `legacy_orbit_c.cpp.o` and `orbit_SteamSpatial.cpp.o` changed; the other 88
  objects are byte-identical. The Orbit changes remove logs and harden startup,
  disable, reset, and failed-HRTF retry paths. The earlier comparison against
  the bridge-only build is historical and not the relevant final diff.
- CIPIC 124 SOFA SHA-256:
  `c28ff4a874ac889ec0c5885ca524762a70d56984232ff7aadcd9c15d32e1cfb6`.
  Its Regents of the University of California copyright and use notice are
  included in the package.

## Exact-library Android HRTF initialization matrix

The user supplied Termux results in
`reference/cipic-hrtf-init-matrix.txt`; the preceding default-profile matrix is
in `reference/default-hrtf-init-matrix.txt`. Both were run against the exact
Stage 31 Steam Audio library hash above, API 4.8.1, build ID
`020c11ef0d34b1431c84615ccf48973fd417e224`.

- Default embedded HRTF: 44.1/48 kHz succeeded at frame sizes 128, 256, 512,
  and 1024. At 88.2/96 kHz it returned
  `IPL_STATUS_INITIALIZATION` for all four sizes.
- CIPIC SOFA HRTF: succeeded at 44.1, 48, 88.2, and 96 kHz for all four frame
  sizes. This validates HRTF object creation with the exact library and data;
  it does not test audible output through AudioFlinger.
- The candidate therefore retains the embedded default profile at 44.1/48 kHz
  and selects the bundled CIPIC SOFA only at 88.2/96 kHz.

## Host callback regressions

Command:

```sh
experimental/ldac-orbit/scripts/run-host-regression.sh \
  work/ldac-candidate/source work/steamaudio_4.8.1/include \
  work/frame_bridge_cipic_test
```

Compiler: Debian g++ 14.2.0 with `-Wall -Wextra -Werror`, ASan, UBSan, and leak
checking. Steam Audio calls are mocked at the API boundary. The mock models the
measured unsupported default rates and verifies SOFA selection/path at 88.2/96.
It validates bridge control flow and memory safety, not actual HRTF output.

| Rate | Peak | Max adjacent step | Left RMS | Right RMS | Stereo delta RMS | Mean aligned change | Result |
|---:|---:|---:|---:|---:|---:|---:|---|
| 44,100 Hz | 0.170958 | 0.0208415 | 0.0855313 | 0.0303110 | 0.0626755 | 0.0668018 | PASS |
| 48,000 Hz | 0.171172 | 0.0191211 | 0.0856018 | 0.0303193 | 0.0627221 | 0.0667804 | PASS |
| 88,200 Hz | 0.171235 | 0.0104319 | 0.0854257 | 0.0302564 | 0.0626546 | 0.0668758 | PASS |
| 96,000 Hz | 0.171172 | 0.00958492 | 0.0854162 | 0.0302458 | 0.0626539 | 0.0668695 | PASS |

At every rate, fixed 128-frame and irregular callback sequences (1, 64, 96,
127, 128, 129, 192, 240, 256, 480, 512, 882, 960, 1,024, 1,920, and 2,048
frames) were segmentation-equivalent (maximum difference 0). Canaries, finite
output, stereo balance, nonzero wet change, peak/discontinuity bounds, failure
bypass/recovery (including high-rate SOFA failure), OFF passthrough,
transitions, reset, rate changes, and 24 repeated enable/disable cycles passed.

## Remaining device checks

The user reported a short playback pass with no crackling/popping at LDAC 44.1
and 48 kHz and audible Orbit at the actual negotiated 96 kHz rate; the capture
phase labeled 90 kHz negotiated 96 kHz. Rooted dumpsys showed active JamesDSP
sessions at the tested rates, and captured logs from the preceding diagnostic
build showed successful 96 kHz CIPIC initialization. The final no-logging
build adds startup/quick-disable/reset edge-case fixes and has not been
separately installed. AAC/USB, Android 16 reboot/install, and extended
stability/stress testing remain pending.

## JamesDSP connection guard follow-up

The legacy effect entry point registers the same JamesDSP implementation UUID
used by the app. The app sends Orbit parameter IDs `0x1000`, `0x1001`,
`0x1003`, `0x1005`, and `0x1006`; the native bridge handles those IDs and
forwards all others to the original JamesDSP command handler. `process()` runs
JamesDSP first, then applies Orbit to its float output only after JamesDSP
returns success and the buffers describe the same stereo frame count.

The follow-up guard now requires accepted `SET_CONFIG` transport and reply
status plus explicit input/output sample-rate, channel, and format mask bits.
It also requires matching positive rates, stereo channel masks, and float PCM.
The AIDL adapter mirrors the same eligibility rule and frame-count check. An
ASan/UBSan integration regression invokes the real legacy `EffectCreate`,
`command`, and `process` functions with a stub JamesDSP core. It verifies
parameter set/readback, post-DSP Orbit changes, rejected-config bypass,
missing-mask bypass, rate/channel/format rejection, and no Orbit processing
when JamesDSP returns `-ENODATA`.

An ARM64 same-path rebuild comparison against the immediately prior source
reproduced the prior library hash exactly (`697f0caf...`). Applying this guard
changed only `jamesdsp.c.o` and `legacy_orbit_c.cpp.o`; all 88 other objects,
including the JamesDSP DSP core and Orbit audio-processing implementation,
remained byte-identical. The new library keeps the same exports, dependencies,
and RUNPATH. The existing experimental ZIP and Stage 31 release are unchanged;
the config-guard package is a separate artifact and still needs device
confirmation.
