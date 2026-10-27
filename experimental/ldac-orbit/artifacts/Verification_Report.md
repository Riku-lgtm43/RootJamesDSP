# JamesDSP Orbit LDAC Frame Bridge Experimental — Verification Report

Status: **Experimental, offline verified only. Not production stable.**

## Artifact integrity

- Asset: `JamesDSP_Orbit_LDAC_FrameBridge_Experimental_KernelSU.zip`
- SHA-256: `44388287d08151490dac520c24038eb25a83e6a1aefe04479adc5c6778a90064`
- Module ID: `jamesdsp_orbit_stage29` (in-place opt-in update)
- Effect UUID: `f27317f4-c984-4de6-9a90-545759495bf2`
- Experimental Orbit library SHA-256: `459fe1fe6e5510634a183c5b4972031fa2172cf1dc31ccc108fb494040f41795`
- Unmodified Stage 31 Orbit library SHA-256: `025bf72e53ccaa600c07699b7650020dff651e1716bd200d2c6d9252bb01ce7d`
- Stage 31 Steam Audio dependency SHA-256: `857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369` (preserved unchanged)
- Packaged JamesDSP APK SHA-256: `e37ee0373e0fbb43147dbe10e7af617d45ab53203339c3904d86c8fba2d96eb2`; APK signature verified, unchanged from Stage 31.

## Changes and evidence

The native patch adds a fixed 128-frame per-instance callback bridge, bounded transition fades, and non-real-time logs for effect configuration eligibility and Steam Audio initialization. The bridge adds 128 frames of latency while Orbit is enabled. Existing JamesDSP algorithms, parameter IDs, APK, signing certificate, Steam Audio library, C++ runtime, audio XML, installer script, and module identity are unchanged.

The user confirmed crackling/popping with Orbit ON at LDAC 44.1 kHz, harsher crackling at 48 kHz, and no audible Orbit change at 96 kHz while other JamesDSP effects work. The uploaded diagnostic shows a 96 kHz stereo-float JamesDSP effect configuration with 1,920-frame buffers. Its Orbit initialization result and callback trace are absent. The Stage 18 archival source passes callback tails shorter than 128 frames dry; this is a plausible 44.1/48 kHz mechanism, not proven device root cause. The 96 kHz cause remains unresolved; this candidate adds logs to identify config rejection or HRTF initialization failure.

## Verification performed

- Full ARM64 build using Android NDK r27 / Clang 18.0.1, target API 26, with the exact Stage 31 `libphonon.so`; strict undefined-symbol link passed.
- ELF is AArch64 ELF64, RUNPATH `/vendor/lib64/soundfx`, same NEEDED library list as Stage 31. Candidate adds no dynamic exports versus Stage 31; Stage 31 has two additional CPU resolver symbols.
- ASan/UBSan host regression harness with mocked Steam Audio passed at 44.1/48/96 kHz. Fixed 128-frame and irregular callbacks (1, 64, 96, 127, 128, 129, 192, 240, 256, 480, 512, 882, 960, 1,024, 1,920, 2,048 frames) were segmentation-equivalent (maximum output difference 0). Canaries, finite output, distinct stereo, per-block wet change, peak/discontinuity bounds, failure bypass/recovery, OFF passthrough, transitions, reset, rate changes, and 24 enable/disable cycles passed.
- ZIP integrity and embedded SHA-256 manifest passed. APK signature verified. Payload changes are limited to the Orbit library, module metadata, README, reference checksum list, and experimental build provenance note; APK, signing identity, Steam Audio, C++ runtime, audio XML, and installer script match the original Stage 31 archive.
- The candidate archive also contains two harmless root-level verification logs, `apk-verify.log` and `zip-test.log`. They are retained by the Termux reassembly helper so every member remains byte-identical to the committed candidate.

The harness mocks the Steam Audio API. It does not establish real HRTF output, LDAC behavior, AAC/USB regression, reboot persistence, or long-run stability. No device installation or playback test was performed.

## Installation precautions and rollback

For rooted ARM64 KernelSU devices only; the inherited audio XML is device/ROM-specific. Back up the installed module, app data/settings, and original Stage 31 ZIP before testing. This ZIP uses the same module ID and must be installed as an in-place opt-in update through KernelSU Manager, not as a second module or through recovery. Do not proceed if the vendor audio XML is not suitable for the device.

Test briefly at low volume. Stop immediately on crackling, pops, dropouts, silence, or abnormal level. Test LDAC 44.1/48/96 separately, then AAC and USB controls. Leave extended playback until short tests are clean. LDAC is not considered fixed until the user reports hardware results; the 96 kHz issue is specifically unresolved.

Rollback: disable the experimental module in KernelSU Manager and reboot; restore the saved Stage 31 ZIP and reboot. Preserve app data if a signing/package conflict appears. No automatic installation was performed.
