# Passive capture findings and verification

## Observed on 2 November 2026

The user supplied `JamesDSP_Passive_20261102_211112.tar.gz` from a rooted Poco
X6 Pro on Android 16 with the v40 KernelSU module installed. Its AudioFlinger
snapshots were taken at 21:11:15, 21:12:15, and 21:13:15 local time. The
21:13:15 audio policy snapshot identifies the USB headset output.

At 21:12:15, TikTok (`com.zhiliaoapp.musically`, PID 21409, UID 10301) has an
active, unmuted 44.1 kHz PCM16 track in session 329 on the 48 kHz USB mixer.
The track has 5,655 frames ready and no reported underruns in that snapshot.
JamesDSP effect ID 243 is attached to that session, enabled, and nonsuspended.
Its effect buffers are 48 kHz stereo float.

At 21:13:15, Apple Music (`com.apple.android`, PID 3571, UID 10308) has an
active, unmuted 44.1 kHz PCM float track in session 81 on the same USB output.
JamesDSP effect ID 331 is enabled and nonsuspended with 48 kHz stereo float
effect buffers. The USB output's signal history shows nonzero levels around
21:13:10–21:13:15.

The differing source PCM types do not establish a format bug: both effects are
configured for stereo float by AudioFlinger. The capture has no native input,
post-core, or post-Orbit signal measurements. It also has no full AudioFlinger
snapshot after the user's reported master-switch-OFF action. The exact switch
time is absent from app logs, and the logcat launcher exited unexpectedly at
21:12:15. The USB HAL signal history does not isolate individual app tracks.
These facts do not support a specific audio algorithm or routing fix yet.

## Diagnostic boundary

The temporary native build reports aggregate nonzero counts and peaks once per
second for each effect session. `nz=A/B/C` and `peak_u=A/B/C` refer to native
input, post-JamesDSP core, and post-Orbit output. For a silent app with master
ON, the short controlled test can distinguish:

| Observation | Next area to inspect |
| --- | --- |
| No native callbacks for an active track | Effect-chain attachment or scheduling |
| Input near zero | App source, mixer input, or conversion before JamesDSP |
| Input nonzero, core output near zero | JamesDSP core configuration or processing |
| Core output nonzero, Orbit output near zero | Orbit processing |
| Final output nonzero but no audible audio | Downstream mixer, route, USB HAL, or device |

The counters show levels, not audio content. Nonzero output alone cannot prove
that a listener heard the signal. The app package for a session must still be
confirmed in the accompanying AudioFlinger snapshot.

## Package verification

- Original v40 ZIP SHA-256: `f3d401aea814c27500f88d70235bf7e22d085ffe0abf1063fc3198347d2ee46e`.
- Original v40 native library SHA-256: `34fd8f4102ed92c2c252c7fb1ddf5483a0b614b9839ef836ee8cb642439ee680`.
- Original 90-object relink matched the v40 native library byte for byte.
- Only `jamesdsp.c.o` was replaced in the diagnostic link. The remaining 89
  objects, including the JamesDSP core and Orbit C++ objects, were reused.
- Diagnostic native library SHA-256: `c50b05d00a4688edef44e17feff17ad5ce8b31a8f738eb1a56ad7fe4054a6df9`.
- ZIP SHA-256: `555131c1415257d5f065a936423b079a5ea7df4a40dd4c27bcd87996fb02bcbc`.
- ZIP CRC and SHA-256 sidecar passed. Repack produced an identical ZIP checksum.
- The original 35 entries remain; only `module.prop`, `README.txt`, the binary
  checksum list, and the native library changed. One diagnostic note was added.
- The signed APK, `audio_effects.xml`, `libphonon.so`, `libc++_shared.so`, CIPIC
  SOFA, and `customize.sh` match v40 byte for byte. Module ID remains
  `jamesdsp_orbit_stage29`.
- APK signature verification passed for v2/v3, with the preserved signer
  certificate SHA-256 `b3180a2a9460dcdeafe8d1d983a82e14e3725e6b1b8a245af7e4357a425ff77b`.
- The native library is AArch64 ELF64 with the original dependency set and
  `/vendor/lib64/soundfx` RUNPATH. All 1,025 dynamic exports match v40.
- The host test covers normal signal, core mute, Orbit mute, and effect
  configuration/enable controls. Device installation and listening remain
  pending; the user must install the diagnostic manually if they choose to test.
