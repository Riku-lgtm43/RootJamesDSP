# DSP engine baseline and benchmark results

Date: 2026-10-27. The baseline is the verified Stage 18 archive after the
existing Orbit frame-bridge patch and before the engine-audit patch. The
candidate applies `stage18-engine-audit.patch` afterward. Archive SHA-256 is
`52da7707c0e7bc5819597895fbef8bc59e5dcdca72076fec78b28cd8ede5ee5b`.

## Signal correctness and segmentation

The real core test processes stereo signals through `EffectDSPMainProcess` at
44.1, 48, 88.2, and 96 kHz. At 44.1/48 kHz, float bypass with all optional
effects disabled has exact identity for the synthetic 128-frame stimulus:
maximum and RMS error are both zero. Float PCM has no rate conversion at these
rates. Integer format tests allow one LSB for PCM16 and 1,024 LSB for PCM32
conversion; packed PCM24 round-trips exactly, including signed endpoints in
both byte orders.

The ASRC regression renders the same deterministic stereo 16,384-frame stream
as fixed 128-frame callbacks and as the repeating irregular schedule
`1, 63, 96, 127, 129, 240, 480, 512, 882, 960, 1024, 2048`. At 44.1/48 kHz the
baseline matches exactly. At 88.2/96 kHz, baseline maximum absolute difference
is `0.530578226` / `0.473923266` and RMS difference is `0.00384082149` /
`0.00310114516`. The patched build is exact (`max_abs_diff=0`, `rms_diff=0`) at
all four rates. Baseline UBSan also reports misaligned `WarpedPFB` and NSEEL
pointer accesses; strict UBSan stops during initialization. The baseline
reproduction uses non-halting UBSan only so it can finish and quantify the
segmentation difference. The patched regression uses `halt_on_error=1` and
reports no ASan/UBSan errors. Data is archived in
[`benchmarks/asrc-segmentation.csv`](benchmarks/asrc-segmentation.csv).

## Callback-size transition benchmark

The benchmark compiles the actual C core listed by `Android.mk.disabled` with
GCC 14, `-O2 -DNDEBUG -funsigned-char`, then processes two low-level tones
(997 Hz left, 733 Hz right, amplitude 0.2) at each of these frame counts:
1, 64, 96, 127, 128, 129, 192, 240, 256, 480, 512, 882, 960, 1,024, 1,920,
and 2,048. It performs four untimed warm-up cycles, then records 100 callback
times per size while transitioning among sizes on every cycle. Results include
mean, p50, p95, p99, and max. This is a size-change/reconfiguration workload,
not a steady-state playback benchmark. The baseline emitted 416 synchronous
`Latency must not smaller than 64` messages during the run; the patched build
emitted zero.

| Rate | 128-frame baseline p50 | 128-frame patched p50 | Baseline p95 | Patched p95 |
|---:|---:|---:|---:|---:|
| 44.1 kHz | 1,789.949 µs | 0.621 µs | 1,916.408 µs | 0.631 µs |
| 48 kHz | 1,856.348 µs | 0.621 µs | 1,933.143 µs | 0.631 µs |
| 88.2 kHz | 1,811.251 µs | 9.204 µs | 1,882.066 µs | 9.224 µs |
| 96 kHz | 1,891.410 µs | 9.204 µs | 2,298.067 µs | 9.224 µs |

These host measurements show the disabled-effect refresh/logging cost removed
from the size-change path and the remaining high-rate ASRC cost. They include
the baseline's synchronous diagnostic formatting and I/O. They do not predict
Android callback timing, identify phone CPU load, or measure active FIR/HRTF
processing. Full before/after distributions are in
[`benchmarks/callback-before.csv`](benchmarks/callback-before.csv) and
[`benchmarks/callback-after.csv`](benchmarks/callback-after.csv); each has a
warning-count companion file.

## Integration and selected effect coverage

`run-engine-host-regression.sh` builds the real core and real legacy
`EffectCreate`/command/process wrapper with ASan and UBSan. C compilation uses
`-funsigned-char` to model the ARM64 plain-char default. Steam Audio API calls
are mocked only at the library boundary. Results:

- Core float bypass is exactly transparent at 44.1/48 kHz.
- Core fixed/irregular segmentation has zero difference at 44.1/48/88.2/96 kHz.
- PCM16, PCM32, packed PCM24, float, and 8.24 paths are exercised.
- Rejected configs/packets and mismatched frame counts fail without changing
  live state or writing output.
- Disabled and active advanced crossfeed and the active stereo-enhancement PFB
  run through the real core.
- The real legacy wrapper processes JamesDSP before Orbit. With mocked Steam
  Audio, Orbit changes the output at every tested rate; observed maximum deltas
  are 0.162057173, 0.29915978, 0.357021242, and 0.335587427 respectively.
- Final ARM64 build is an AArch64 ELF with the Stage 31 dependency list and
  `RUNPATH=/vendor/lib64/soundfx`.

## What this baseline does not establish

This is not a full sound-quality certification. The harness does not measure
each EQ/compressor/reverb/DDC/convolver transfer function, true peak, THD+N,
frequency response, long-IR tails, loudness, callback allocation counts, peak
memory, Bluetooth codec output, or Android underruns. It does not model the
real Steam Audio HRTF, AudioFlinger, AxionFx, player normalization, AAC, USB,
or LDAC route changes. User-reported crackle-free 44.1/48 kHz and audible
96 kHz Orbit results came from an earlier candidate and must not be attributed
to this uninstalled build.
