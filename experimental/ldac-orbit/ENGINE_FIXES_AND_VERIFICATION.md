# Engine audit fixes and verification

## Changes

The audit uses the Stage 18 source archive already used for the experimental
native build, after the existing Orbit bridge patch. It does not assert that
this archival source is the exact source revision used to build the published
Stage 31 binary. The published Stage 31 ZIP and all older experimental ZIPs are
unchanged.

| Finding | Root-cause change | Validation |
|---|---|---|
| ASRC FIFO state was lost when callback storage grew. | Allocate the new work/ring block first, preserve queued stereo samples in order, rebase ring cursors, and only then release the old block. Check size arithmetic and copy/bypass safely if allocation fails. | Baseline: exact at 44.1/48 kHz but max difference 0.530578226 at 88.2 kHz and 0.473923266 at 96 kHz for identical streams split differently. Candidate: exact at all rates. ASan/UBSan candidate run passes. |
| Disabled convolution/crossfeed/EQ refresh ran when callback frame size changed, with an unguarded partition message. | Defer disabled refresh to the enable path; initialize advanced crossfeed convolution only when selected and enabled. Remove the callback-time `printf`. | Same-toolchain host transition benchmark: 128-frame p50 falls from 1,789.949/1,856.348/1,811.251/1,891.410 µs to 0.621/0.621/9.204/9.204 µs at 44.1/48/88.2/96 kHz. Baseline emits 416 warning lines; candidate emits zero. These are host transition timings, not device steady-state performance. |
| Release flags could select a delayed benchmark thread and callback wrapper. | Remove `JAMESDSP_REFERENCE_IMPL` from the release Soong/legacy configuration and NDK script. Release selects the direct DSP processor; the reference benchmark code remains behind the opt-in macro. | Final NDK build has no macro define and produces expected ARM64 ELF. Soong was source-reviewed, not built with Android platform tools. |
| Signed packed PCM24 conversion invoked undefined signed shifts. | Use defined unsigned assembly/emission and explicit sign extension. | UBSan and PCM24 endpoint/round-trip tests in both byte orders pass. |
| PFB signed coefficients depended on implementation-defined plain `char`. | Use `int8_t` for signed filter coefficients/workspace and `uint8_t` for lookup codes. | Host regression compiles C as `-funsigned-char`; negative coefficient and active stereo-enhancement tests pass. NDK ARM64 build passes. |
| NSEEL pointer and PFB workspace alignment was insufficient. | Align pointer tables to `sizeof(char *)` and PFB offsets/storage to the members' alignment. | Baseline UBSan reports misaligned pointer/PFB accesses; candidate `-funsigned-char` ASan/UBSan host regression passes with halt-on-error. |
| Config and parameter packet failures could mutate stale state or overrun declared data. | Validate the initial complete config, paired input/output fields, supported stereo PCM format, command payload size/padding, reply capacity, buffers, frame counts, and arithmetic before processing. | Real core tests exercise supported PCM formats and invalid configs/packets/frame counts; real legacy wrapper test checks rejected config and Orbit eligibility. |
| Partial allocation failure in crossfeed/convolver could publish incomplete state. | Check object and IR allocations, clean partial crossfeed construction, and replace saved IR transactionally; defer disabled refresh. | Active/disabled crossfeed tests and sanitizers pass. Explicit allocator fault injection and full IR playback remain unavailable. |

No PEQ, compressor, DDC, reverb, FIR algorithm, gain setting, Orbit parameter,
signed APK, Steam Audio library, SOFA data, app, or module identity change was
justified by the measured host evidence. These areas are not claimed to have
newly measured sound-quality improvements.

## Reproduction commands

After applying both source patches, run:

```sh
bash -n experimental/ldac-orbit/scripts/*.sh
experimental/ldac-orbit/scripts/run-engine-host-regression.sh \
  /path/to/patched/Stage18/source /path/to/steamaudio/include
experimental/ldac-orbit/scripts/run-host-regression.sh \
  /path/to/patched/Stage18/source /path/to/steamaudio/include
experimental/ldac-orbit/scripts/run-engine-callback-benchmark.sh \
  /path/to/patched/Stage18/source /tmp/jdsp-benchmark 100
```

To demonstrate the ASRC defect, run the segmentation test against the source
tree before and after the engine patch:

```sh
experimental/ldac-orbit/scripts/run-asrc-segmentation-regression.sh \
  /path/to/source-before-engine-audit divergence /tmp/jdsp-before
experimental/ldac-orbit/scripts/run-asrc-segmentation-regression.sh \
  /path/to/source-after-engine-audit equivalent /tmp/jdsp-after
```

The before mode allows UBSan to continue after it reports baseline alignment
errors so it can report the high-rate stream difference. The after mode halts
on the first ASan/UBSan error.

Build the candidate native library with:

```sh
experimental/ldac-orbit/scripts/build-arm64-ndk.sh \
  "$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin" \
  /path/to/patched/Stage18/source \
  /path/to/steamaudio/include \
  /path/to/exact/libphonon.so \
  /path/to/output
```

The final library is SHA-256
`3cbbe92a885e2533c4f14cf7b6a3ff570efa4e0e557a9db493b5d72672ed95bb`.
`readelf` reports ELF64 AArch64, `RUNPATH=/vendor/lib64/soundfx`, and
`NEEDED` dependencies `libphonon.so`, `liblog.so`, `libm.so`, `libdl.so`,
`libc++_shared.so`, and `libc.so`.

## Package verification

The separate candidate ZIP is built from the preceding ConfigGuard package as
a payload base and replaces the experimental native library plus its version
and audit documentation. Verification compares protected product payloads
(APK, certificate, module ID, audio-effects XML, libc++ runtime, Steam Audio
library, SOFA data, license notice, and install scripts) to the original Stage
31 release. Other candidate package members are checked against the ConfigGuard
base, with changes limited to the native library and audit/version files. The
APK signer certificate SHA-256 is
`b3180a2a9460dcdeafe8d1d983a82e14e3725e6b1b8a245af7e4357a425ff77b`.
The final artifact checksum is published in its adjacent `.sha256` sidecar and
`SHA256SUMS` after the archive is created.

## Limits and release status

Host tests use the real JamesDSP core and legacy wrapper. Only the Steam Audio
API calls are mocked. The AIDL adapter is source-reviewed but not built against
Android platform headers or tested through FMQ. This run does not establish
Android 16 installation/reboot behavior, device callback timing, hardware HRTF
fidelity, Bluetooth codec transitions, AAC, USB DAC, all effect combinations,
or extended stability. Earlier clean 44.1/48 kHz and audible 96 kHz playback
reports came from the preceding candidate. This package has not been installed;
no user-system changes were made.
