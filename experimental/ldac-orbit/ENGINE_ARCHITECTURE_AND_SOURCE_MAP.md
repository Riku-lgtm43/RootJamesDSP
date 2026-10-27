# JamesDSP Stage 31 Orbit engine source map

Audit date: 2026-10-27. This map describes the experimental native source used
for this branch's ARM64 candidate. It does not claim that the archival Stage 18
source is the exact source revision used to produce the published Stage 31
binary.

## Provenance and build inputs

- Repository branch at audit start: `fix/ldac-orbit-native`, commit
  `ed4175e832d99cdbfcc87bf16fa13e6053c68d51`; origin pointed to the same SHA.
- Stage 18 source archive:
  `JamesDSP_Stage18_Full_Link.zip`, SHA-256
  `52da7707c0e7bc5819597895fbef8bc59e5dcdca72076fec78b28cd8ede5ee5b`.
- Apply the existing `stage18-frame-bridge.patch` first and then
  `stage18-engine-audit.patch` from the archive root containing `source/`.
- Reference Stage 31 combined ZIP SHA-256:
  `0799875e20dcf2a05fdf99d08379344d8795641f0d17c99e301e6a994da34b44`.
- Packaged `libphonon.so` SHA-256:
  `857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`.
- NDK build: Android NDK r27 (`27.0.12077973`), Clang 18.0.1, API 26,
  GNU C11 / C++17, `-O2 -fPIC`, `-Wl,-z,defs`, and
  `RUNPATH=/vendor/lib64/soundfx`. The reproducible script builds the native
  library only; it does not build or sign the APK.
- ARM64 ELF dependencies: `libphonon.so`, `liblog.so`, `libm.so`, `libdl.so`,
  `libc++_shared.so`, and `libc.so`.

## Runtime path

```text
AudioFlinger / effect HAL
  -> audio_effects.xml resolves the JamesDSP type/implementation UUID
  -> legacy EffectCreate() or AIDL createEffect()
  -> EffectDSPMainConstructor() creates the JamesDSP state
  -> EFFECT_CMD_SET_CONFIG converts the negotiated input/output contract
  -> EffectDSPMainProcess() converts interleaved PCM to the core's planar float
  -> optional ASRC to the 44.1/48 kHz core rate
  -> JamesDSPProcess() runs enabled DSP stages in order
  -> output gain and stateful limiter
  -> ASRC back to the negotiated rate and PCM format conversion
  -> Orbit processes the successful stereo-float output through its persistent
     128-frame Steam Audio bridge
  -> AudioFlinger output path, then route/codec/player processing
```

The concrete legacy path is `jamesdsp.c:EffectCreate`,
`jamesdsp.c:generic_command`, and `jamesdsp.c:generic_process`. It owns one
`EffectDSPMain` and one Orbit context per effect instance. Configuration first
reaches the core; Orbit eligibility is set only when the command transport and
core reply both succeed. A process callback calls `EffectDSPMainProcess` first
and calls Orbit only when the core returns success, the selected format is
float, and input/output frame counts match.

The AIDL path is present in `jamesdsp_aidl.cpp` and `jamesdsp_aidl.h`.
`JamesDSPAIDL::createContext` converts the AIDL common configuration to the
legacy `effect_config_t`, initializes the same core, and
`JamesDSPAIDLContext::process` calls the same `EffectDSPMainProcess`. It gates
Orbit to matching stereo-float configuration and processes the core output.
The Android NDK host build and wrapper integration test exercise the legacy
entry point; AIDL is source-reviewed but not built against Android platform
headers or runtime-tested here.

The app-side connection in `app/src/main/java/.../JamesDspRemoteEngine.kt`
creates `AudioEffectHidden` for the existing effect UUID and session, sends
native parameter IDs, and reads Orbit values back before caching successful
writes. A reboot clears the cache. This audit does not change the app or its
settings persistence. Device session attachment and service restart behavior
remain outside host verification.

The app-to-native command map was reviewed without changing it:

| App control | Native command IDs / data |
|---|---|
| Orbit | `0x1000` enable, `0x1001` width, `0x1003` strength, `0x1005` decay seconds, `0x1006` bass; writes are read back before entering the app's `orbitApplied` cache. |
| Compressor/compander | `115` parameter array / `1200` enable; `112` bass boost / `1201` enable. |
| Multimodal EQ and reverb | `116` filter/interpolation/bands / `1202` enable; `128` preset / `1203` enable. |
| Stereo and tube | `137` stereo width / `1204` enable; `150` tube drive / `1206` enable. |
| Convolver, crossfeed, graphic EQ | `12000` impulse data plus CRC commit `25003`, then enable `1205`; `188` mode / `1208` enable; graphic EQ data `12001` subcommand `10006` plus CRC `25000`, then `1210` enable. |
| DDC and live program | `12001` subcommand `10009` + CRC `25001`, then DDC enable `1212`; live script subcommand `10010` + CRC `25002`, then enable `1213`. |

These commands are invoked from the existing preference-sync path. Orbit writes
avoid redundant updates when a prior value was successfully set and read back;
the engine clears the cache when the effect instance is recreated. Host tests
exercise Orbit parameter round-trips through the real legacy wrapper, while
the app service and persisted profile lifecycle are source-reviewed only.

## Core processing and ownership

`jdsp_impl.c:EffectDSPMainProcess` validates configured state, non-null buffers,
equal positive frame counts, supported stereo PCM format, and arithmetic
bounds. Its format dispatch supports PCM16, float32, signed PCM32, packed
PCM24, and 8.24 PCM. `jdspController.c` deinterleaves to two planar work
buffers. For input rates outside the core's 44.1/48 kHz operating points, two
per-channel ASRC handlers feed and drain the DSP-rate FIFO. Persistent filter,
convolver, reverb, resampler, Orbit, and transition state belongs to the effect
instance; the caller retains ownership of callback buffers.

The actual `JamesDSPProcess` stage order is:

1. analogue/tube modelling;
2. compressor;
3. bass boost;
4. multimodal EQ;
5. arbitrary magnitude EQ;
6. convolver;
7. DDC;
8. live programmable DSP;
9. crossfeed;
10. stereo enhancement;
11. reverb;
12. output gain and linked stereo limiter.

The shared mutex is acquired around convolver, DDC, and live-program processing.
This is relevant real-time blocking risk if control changes contend with an
audio callback. The patch does not redesign parameter synchronization because
no Android contention trace or reproducing race was available. Buffer growth
can still allocate on the callback when a larger-than-seen frame count arrives;
the patch makes that growth safe but does not claim allocation-free callbacks.

Orbit configuration, HRTF loading, and effect object creation occur on the
control/configuration path. The callback consumes already-created state and
the fixed-frame bridge. The bridge adds up to 128 samples of Orbit latency.
The 88.2/96 kHz path uses the licensed bundled CIPIC SOFA because the exact
packaged Steam Audio library's embedded default HRTF has no exact entries at
those rates; the default is retained at 44.1/48 kHz.

## Scope limits

The native host suite exercises all supported input sample formats and the
real DSP core plus legacy wrapper. It mocks only the Steam Audio API boundary,
so it does not certify real HRTF sound quality. The source-level AIDL adapter
is not exercised by this Android NDK build. No host result establishes AAC,
USB, Android 16 installation/reboot, concurrent app updates, Bluetooth route
stability, or long-duration playback. LDAC and other downstream audio stages
can change the signal independently of JamesDSP.
