# v40 Enhanced mode silence diagnostic

This separate KernelSU ZIP measures the signal entering JamesDSP, leaving the
JamesDSP core, and leaving Orbit. It does not change audio algorithms or the
Enhanced mode app. It is a temporary diagnostic for the Android 16 report that
TikTok and other apps are silent with the master switch on while Apple Music is
audible. The installed module during the supplied capture was v40.

Download [JamesDSP_Orbit_v40_Enhanced_Silence_Diagnostic_KernelSU.zip](artifacts/JamesDSP_Orbit_v40_Enhanced_Silence_Diagnostic_KernelSU.zip).

SHA-256: `555131c1415257d5f065a936423b079a5ea7df4a40dd4c27bcd87996fb02bcbc`

The ZIP uses the existing module ID `jamesdsp_orbit_stage29` and installs as an
in-place update. Installation and reboot are manual. Keep the [original v40
ZIP](../ldac-orbit/artifacts/JamesDSP_Orbit_USB_HiFi_192k_HRTF_LevelOnly_NoEnhanced_KernelSU_v40.zip)
for rollback. The diagnostic ZIP should be restored to v40 after the short
capture. No APK, certificate, Steam Audio library, HRTF data, audio effects
configuration, or installer changed.

[README-DIAGNOSTIC.txt](README-DIAGNOSTIC.txt) gives the short phone capture
steps and the meaning of each counter. [CAPTURE_FINDINGS.md](CAPTURE_FINDINGS.md)
records what the passive log proves and what it cannot identify. The attached
passive log was reviewed locally and is not published in this branch.

## Rebuild and checks

`build-native.sh` verifies the exact v40 ZIP and the committed v40 object set,
relinks the original native library byte for byte, compiles only the diagnostic
entry point, and links it with the other 89 original objects. It requires Android
NDK r27 and the already published v40 ZIP:

```bash
bash experimental/enhanced-silence-v40/build-native.sh \
  /path/to/android-ndk-r27/toolchains/llvm/prebuilt/linux-x86_64/bin \
  experimental/ldac-orbit/artifacts/JamesDSP_Orbit_USB_HiFi_192k_HRTF_LevelOnly_NoEnhanced_KernelSU_v40.zip \
  /tmp/jdsp-v40-diagnostic
bash experimental/enhanced-silence-v40/tests/run.sh
```

`package.py` copies all v40 ZIP members, replaces only the diagnostic library,
module metadata, README, and binary checksum list, and adds the diagnostic
note. It rejects an altered base ZIP or native library. The committed object
archive contains the exact 90 v40 link inputs; the original `jamesdsp.c` is
included as `source/jamesdsp-base.c` for a readable diff against the diagnostic
source. Native headers needed to compile that source are in `source/headers`.
The corresponding JamesDSP source and license are in this repository; original
JamesDSP and Steam Audio attribution remains applicable.
