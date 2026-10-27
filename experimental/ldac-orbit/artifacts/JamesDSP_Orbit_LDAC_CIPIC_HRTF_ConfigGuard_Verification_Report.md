# JamesDSP Orbit Stage 31 — config-guard experimental build

Status: experimental opt-in KernelSU update. The previous build was reported
clean on LDAC 44.1/48 kHz with Orbit audible at 96 kHz. This config-guard
rebuild has not yet been installed or tested on-device. Android 16 reboot,
AAC/USB playback, and extended stability/stress testing remain pending.

## Build and scope

- Module ID remains `jamesdsp_orbit_stage29`; effect UUID remains
  `f27317f4-c984-4de6-9a90-545759495bf2`.
- ARM64 ELF64 native library SHA-256:
  `764fce2c57d72069851b39505400e46b253822df412085996a9abf4484aff9ae`.
- Built with Android NDK r27 / Clang 18.0.1 for
  `aarch64-linux-android26`; strict undefined-symbol link.
- Steam Audio library SHA-256:
  `857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`.
- A same-path rebuild of the preceding no-logging source reproduced its
  native library hash exactly (`697f0caf67a4b84069c8c6c5ece80e242f254a5eb27b46cf90e4c3198d07b813`).
  Object comparison found only `jamesdsp.c.o` and `legacy_orbit_c.cpp.o`
  changed in this guard update. All 88 other objects are byte-identical,
  including the JamesDSP DSP core and Orbit audio-processing implementation.
- Dynamic exports, NEEDED dependencies, and RUNPATH
  `/vendor/lib64/soundfx` are unchanged. Orbit-specific diagnostic log strings
  are absent.

## JamesDSP connection and processing

The app creates the effect using the matching JamesDSP type/implementation
UUIDs and sends the five Orbit parameter IDs through the legacy effect command
interface. The native wrapper forwards non-Orbit IDs to JamesDSP. For each
audio callback, JamesDSP processes the input first; Orbit processes the result
only when JamesDSP succeeds, float mode is active, and input/output frame
counts match.

Previously, Orbit eligibility checked config field values but did not require
Android's corresponding config mask bits, and the wrapper treated command
dispatch success as config success without checking JamesDSP's reply status.
The update fixes both. It now requires successful transport and reply statuses,
explicit input/output rate/channel/format mask bits, matching positive rates,
stereo channels, and float PCM. The AIDL adapter applies the same rule and
requires matching input/output frame counts. Valid stereo-float audio keeps
the same DSP/HRTF algorithm and processing path.

## Regression and artifact checks

The ASan/UBSan host suite passed at 44.1, 48, 88.2, and 96 kHz for regular and
irregular callback segmentation. It checks finite/bounded output, stereo
separation, canaries, continuity, bypass, initialization failure/recovery,
SOFA routing, reset, reconfiguration, and repeated enable/disable cycles.
The integration test calls the real legacy effect entry points with a stub
JamesDSP core and verifies parameter set/readback, post-DSP Orbit routing,
config reply rejection, missing config masks, mismatched rates, non-stereo or
non-float formats, and `-ENODATA` bypass. Steam Audio calls are mocked in these
host tests; the separate HRTF compatibility matrix was run against the exact
packaged Steam Audio library.

The signed JamesDSP APK and certificate, `audio_effects.xml`,
`libc++_shared.so`, `libphonon.so`, CIPIC SOFA data/license, `customize.sh`,
and module ID are unchanged from the prior verified candidate. The candidate
ZIP passes full `unzip -t`; listed binary hashes, APK signature checks, and
protected-file comparisons pass. No JKS or signing credentials are included.

This is not a production-stable build. Use a backup and low-volume opt-in
testing. To roll back, disable the module in KernelSU, reboot, restore the
known-good Stage 31 module, and reboot again.


Complete package ZIP SHA-256: `c34d15b14ed560e5e19347681af83a1f97a4eb2f6dc1208dabb98db83ea163cc`. The matching checksum sidecar is
`JamesDSP_Orbit_LDAC_CIPIC_HRTF_Experimental_KernelSU_ConfigGuard.zip.sha256`.
