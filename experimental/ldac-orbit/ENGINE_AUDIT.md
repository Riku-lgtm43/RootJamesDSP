# JamesDSP Stage 31 Orbit engine audit

Audit date: 2026-10-27

Detailed deliverables are split into the [architecture and source map](ENGINE_ARCHITECTURE_AND_SOURCE_MAP.md),
[baseline and benchmarks](DSP_ENGINE_BASELINE_AND_BENCHMARKS.md),
[discrepancy register](ENGINE_DISCREPANCY_REGISTER.md), and
[fixes and verification report](ENGINE_FIXES_AND_VERIFICATION.md). They
separate measured host behavior, source review, and pending device tests.

## Scope

The changes apply to the experimental Stage 18 source build used by this
branch's Orbit module. The source archive SHA-256 is
`52da7707c0e7bc5819597895fbef8bc59e5dcdca72076fec78b28cd8ede5ee5b`.
Apply `stage18-engine-audit.patch` after `stage18-frame-bridge.patch`, from the
archive root containing `source/`. The published Stage 31 ZIP, its signed APK,
certificate, vendor libraries, effects XML, and module ID are not modified.
The existing experimental ZIPs also remain unchanged; this audit produces a
separate candidate.

## Audio path reviewed

The legacy Android effect entry point creates the JamesDSP instance and routes
commands through `generic_command`. `EFFECT_CMD_SET_CONFIG` configures the
JamesDSP core first, then sets Orbit eligibility from the transport result and
the core's reply status. For an audio callback, `generic_process` calls
`EffectDSPMainProcess` first. The core validates the configured format, frame
counts, buffer pointers, and frame-size limits; converts interleaved PCM to
planar float; optionally resamples through its 44.1 or 48 kHz core; processes
the enabled JamesDSP stages; applies output gain and limiting; and converts
back to the requested PCM format. Orbit then processes the core's float output
only after a successful core callback and only for matching frame counts.

The DSP stage order is analogue modelling, compressor, bass boost, multimodal
EQ, arbitrary response EQ, DDC, live programmable processing, convolution,
crossfeed, stereo enhancement, reverb, output gain, and limiter. The AIDL
adapter uses the same core processing entry point and gates Orbit on an
explicit stereo-float configuration. This host audit links and exercises the
actual legacy wrapper. The AIDL source is reviewed but is not built against
platform headers or exercised through FMQ here. The Steam Audio API itself is
mocked in wrapper tests; the separate device HRTF matrix remains the evidence
for the packaged Steam Audio library.

For rates above the engine's native range, the core uses ASRC around its
44.1/48 kHz DSP path. Orbit's 128-frame bridge collects the result for Steam
Audio's fixed-size processing blocks. At 88.2/96 kHz the module selects bundled
CIPIC SOFA data because the packaged Steam Audio default HRTF has no entries at
those rates.

## Findings and changes

| Finding | Change | Evidence |
| --- | --- | --- |
| Growing the process buffer freed the ASRC FIFO backing arrays but retained its cursors. A later high-rate callback could read uninitialized ring samples. | Reallocation now preserves queued left/right FIFO samples in order, rebases cursors, and commits the new allocation only after it succeeds. On allocation failure, processing safely copies input to output. | Fixed-size 128-frame and irregular callback streams match exactly at 44.1, 48, 88.2, and 96 kHz. |
| Changing callback size rebuilt disabled crossfeed, EQ, and convolver state. The crossfeed path could build advanced convolution objects even when crossfeed was off. | Disabled effects now defer refresh work until enabled. Advanced crossfeed allocates only on activation and handles partial allocation failures without dereferencing null objects. | Real-engine callback benchmark and disabled/active crossfeed regression. |
| The host build selected a callback wrapper that checked a benchmark completion flag on every callback. Production build flags also enabled a 30-second delayed calibration thread that ran 20 seconds of CPU benchmarks and could refresh convolution state from an audio callback. | Production Soong, legacy make, and NDK build flags no longer define `JAMESDSP_REFERENCE_IMPL`; the normal process function is selected directly in that build. Default partition estimates remain available. | Build flag inspection and an NDK ARM64 rebuild. Active convolution timing on the phone remains pending. |
| Packed PCM24 conversion left-shifted signed bytes and right-shifted negative integers, which is undefined in C. | Conversion now assembles and emits bytes using defined unsigned bit operations and explicit sign extension. | Endpoint tests for both byte orders and packed PCM24 stream round trips under UBSan. |
| The stereo enhancement PFB stored signed coefficients in plain `char`. The ARM64 compiler treats plain `char` as unsigned, so negative-coefficient branches were unreachable. | PFB coefficient and workspace values use `int8_t`; lookup indexes use `uint8_t`. An unused expression in convolution refresh was also removed. | NDK warning disappeared; regression verifies negative generated coefficients and processes the active stereo enhancement path. |
| PFB workspace and NSEEL pointer tables were not sufficiently aligned for their pointer and double members. | Workspace storage and offset alignment are corrected in the incremental patch. | ASan/UBSan host engine and active stereo enhancement processing. |
| Invalid or partial audio configuration could leave a zero/stale rate or let input and output use different masks, channel layouts, or formats. Malformed parameter packets and mismatched frame counts were not rejected consistently. | The first config must define both sides; subsequent partial updates must be symmetric. Rates, stereo channels, PCM formats, parameter packet lengths, buffers, and equal frame counts are validated before processing. Rejected configs do not partially mutate the engine. | Malformed config/parameter tests and actual wrapper tests for rejected routes. |
| Crossfeed and convolver setup could continue after allocation failure. | Crossfeed partial allocations are checked and cleaned up. Convolver impulse replacement now checks sizes and allocation results, preserves saved IR data transactionally, and defers refresh when disabled. | ASan/UBSan tests exercise disabled and enabled advanced crossfeed; full convolver IR playback is not covered by the host harness. |

## Regression coverage

Run the full real-engine host suite with:

```sh
experimental/ldac-orbit/scripts/run-engine-host-regression.sh \
  /path/to/Stage18/source \
  /path/to/steamaudio/include
```

The runner compiles the C core listed in `Android.mk.disabled`, links the real
`EffectDSPMain` API and real legacy `EffectCreate` wrapper, then mocks only the
Steam Audio API boundary. ASan and UBSan halt on the first error. Coverage
includes:

- 44.1, 48, 88.2, and 96 kHz; 1 through 2,048 frame callbacks, including the
  common 882, 960, and 1,920 frame sizes.
- Fixed 128-frame versus irregular segmentation over 16,384 frames per rate.
- Float, signed 16-bit, signed 32-bit, packed 24-bit, and 8.24 PCM paths.
- Parameter packet bounds, initial and partial configs, unsupported formats,
  mismatched frame counts, and poisoned constructor storage.
- Live legacy wrapper command routing, real core processing before Orbit,
  Orbit output at all four rates, and bypass after a rejected route.
- Disabled and active advanced crossfeed; active stereo enhancement/PFB.
- Finite output, PCM24 signed full-scale endpoints, and ASan/UBSan memory checks.

The existing Orbit bridge and command-connection host suite remains available:

```sh
experimental/ldac-orbit/scripts/run-host-regression.sh \
  /path/to/Stage18/source \
  /path/to/steamaudio/include
```

The release build uses the exact Stage 31 `libphonon.so`, SHA-256
`857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`, and
Android NDK r27. Build it with:

```sh
experimental/ldac-orbit/scripts/build-arm64-ndk.sh \
  "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin" \
  /path/to/Stage18/source \
  /path/to/steamaudio/include \
  /path/to/exact/libphonon.so \
  /path/to/output
```

`build-arm64-ndk.sh` builds for `aarch64-linux-android26`, links with
undefined-symbol checks, and preserves `/vendor/lib64/soundfx` RUNPATH. It does
not build or sign the APK.

## Host performance comparison

`run-engine-callback-benchmark.sh` measures the disabled-effects callback path
for repeated transitions among the frame sizes above. It builds both versions
with host GCC `-O2` and no sanitizers. Results are environment-specific and do
not predict Android device timing. The recorded comparison and final candidate
checksum are in the verification report packaged with the separate candidate.

## Device and release status

The user previously reported clean short LDAC playback at 44.1/48 kHz and
audible Orbit at 96 kHz on an earlier candidate. The new engine-audit candidate
has not been installed. Hardware Steam Audio output, Android 16 install/reboot,
LDAC transition stress, AAC, USB DAC, all effect combinations, and extended
stability testing remain pending. No installation is performed by this work.
Treat the package as experimental, back up the existing module, and keep a
known-good rollback ZIP available.
