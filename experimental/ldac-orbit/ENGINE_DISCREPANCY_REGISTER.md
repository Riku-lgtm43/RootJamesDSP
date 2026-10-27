# JamesDSP engine discrepancy register

This register separates demonstrated source defects from platform questions.
Priority order is signal corruption/memory safety, real-time deadline risk,
then avoidable work. “Fixed” means the source patch contains the repair;
device and effect-on verification still have the limits listed below.

| ID / priority | Scope and observed failure | Root cause and evidence | Repair and regression | Status / remaining risk |
|---|---|---|---|---|
| D1 — critical | High-rate ASRC stream output depended on callback segmentation when process storage grew. | `JamesDSPReallocateBlock` replaced FIFO storage while preserving old read/write cursors. After growth, those cursors addressed newly allocated, uninitialized ring slots. The before/after ASRC segmentation reproducer runs identical 16,384-frame stereo streams as fixed 128-frame and irregular callbacks at 44.1/48/88.2/96 kHz. Baseline and patched results are archived with this audit. | Growth is now transactional; queued left/right samples are copied in order into the new ring and cursors are rebased. Capacity and conversion arithmetic are checked. | Fixed when the paired baseline/candidate test results pass. Android long-stream/rate-transition behavior remains pending. |
| D2 — high | Ordinary callback-size changes spent time rebuilding disabled convolution/crossfeed/EQ state and printed `Latency must not smaller than 64`. | A callback buffer growth path refreshed partitioned convolution even when the effect was disabled; advanced crossfeed convolution could also initialize while disabled. The baseline O2 callback benchmark records the synchronous diagnostic output and rebuild cost. | Disabled effect refresh is deferred until enable; crossfeed advanced convolution initializes only on activation; the unguarded print is removed. | Host callback cost improves substantially in the measured no-effects workload. Android CPU/underrun and active convolution rebuild timing have not been measured. |
| D3 — high | A production build could select the reference wrapper and delayed calibration machinery. | `JAMESDSP_REFERENCE_IMPL` selects a callback wrapper that reads benchmark state; its guarded initialization starts a benchmark after a 30-second delay and performs about 20 seconds of calibration. In that path, coefficient refresh may be requested from callback activity. | Removed that define from the Stage 18 Soong/legacy build settings and the reproducible NDK release script. Production uses `JamesDSPProcess` directly; default convolution partition estimates remain. | The final NDK library was built without the macro. The Soong target was source-reviewed but not built in an Android platform tree. |
| D4 — high | Negative packed PCM24 samples used signed left shift and implementation-defined negative right shift. | Signed shifts in PCM packing are undefined in C and are hit by ordinary negative audio samples. | Byte assembly and emission use unsigned integer operations and explicit sign extension. | UBSan host tests cover negative full-scale endpoints in little- and big-endian helpers and PCM24 stream round-trip. |
| D5 — high | Stereo PFB could treat negative coefficients as positive on ARM64. | The coefficient and workspace values used plain `char`; AArch64's default plain `char` is unsigned. A negative coefficient comparison therefore had different behavior across targets. | Coefficients/workspace use `int8_t`, and lookup codes use `uint8_t`. | Host PFB test verifies negative coefficients; final ARM64 build and active PFB host path pass. Device stereo-width quality is not measured. |
| D6 — medium | PFB workspace and NSEEL pointer table alignment could be insufficient for member types. | The original alignment calculation assumed float-sized/pointer-sized alignment rather than the actual stored pointer/double alignment. | Align PFB storage offsets and allocate the pointer table at `sizeof(char *)` alignment. | ASan/UBSan host suite passes active PFB processing. No ARM device sanitizer run. |
| D7 — high | Partial/mismatched configs and short parameter packets could leave stale state or parse beyond declared data. | Config updates could set one input/output field, and packet length arithmetic did not consistently include payload padding and declared value sizes. Buffer frame counts could differ. | Validate the complete initial contract; require paired input/output updates; reject unsupported/mismatched formats, masks, frames, and short packets before mutation. Check reply capacity and frame arithmetic. | Host tests cover valid PCM formats, malformed packets/configs, and mismatch rejection. Non-stereo routes are intentionally rejected by this core; whether a target route ever negotiates one remains a device question. |
| D8 — medium | Crossfeed/convolver setup could dereference or publish state after allocation failure. | Partial object construction and impulse replacement were not consistently transactional. | Check allocations, clean partial crossfeed objects, and replace IR state transactionally. Disabled convolver refresh is lazy. | Static failure paths reviewed and active/disabled host cases pass. Allocator fault injection and a long-IR playback suite are not present. |
| D9 — low | ARM build emitted mismatched signedness/const warnings in helper APIs. | Local array types did not match `pinv`/`geninv`; interpolation and window helpers discarded const; text/base64 byte conversions were implicit. | Match each helper's declared signedness and use const-correct signatures/explicit byte-string casts. | Final ARM64 build completes without these warnings. No intended sample-processing change. |

## Important no-change findings

- The main JamesDSP algorithms and stage order were not changed. The scope does
  not establish new EQ, compressor, reverb, DDC, or convolution transfer
  function measurements, so there is no claim of improved tonal fidelity.
- No evidence justified changing gain staging, limiter design, coefficient
  formulas outside the PFB signedness defect, Orbit parameters, saved profiles,
  PCM formats, AAC/USB routing, APK, or Steam Audio library.
- The 128-frame Orbit bridge and bundled CIPIC high-rate HRTF are retained.
- The app already caches only Orbit values whose set succeeded and read back
  exactly; it clears that cache when it recreates the effect. No app-side change
  was warranted by source review.

## Open discrepancies / evidence gaps

1. The callback may allocate when a previously unseen large frame size grows
   the core work buffers. Growth is safe after this patch but not allocation
   free; the observed 2,048-frame synthetic callback has not been confirmed as
   an actual device callback size under every route.
2. `jdsp_lock` protects convolver/DDC/live-program processing on the callback.
   A control/audio contention trace is unavailable, so blocking duration and
   parameter-update race behavior remain unknown.
3. The AIDL adapter is source-reviewed only. The NDK test links the real legacy
   AudioEffect wrapper; it does not build Android AIDL headers, exercise FMQ,
   or race `setCommon` with the worker thread.
4. Effect-on frequency response, impulse response, true peak, distortion,
   clipping, and all original-effect combinations were not measured. The host
   test verifies selected pathways and output finiteness, not a complete
   product certification.
5. Steam Audio calls are mocked in host integration tests. Actual HRTF output,
   Android 16, LDAC transitions, AAC, USB, reboots, and extended stability need
   device testing. Earlier user playback reports apply to an earlier candidate,
   not this engine-audit package.
