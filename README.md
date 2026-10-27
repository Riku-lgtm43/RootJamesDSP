# JamesDSP Orbit — RootJamesDSP Fork

A rooted Android fork of JamesDSP with an integrated Orbit HRTF preference card and a companion KernelSU systemless DSP module. This is distinct from the non-root RootlessJamesDSP app it descends from.

> **Pre-release · Stage 31 Deployable — Stress Test Pending.** This is a device-specific test candidate, not a stable or production-ready release.

**[Download the signed Stage 31 combined KernelSU ZIP](https://github.com/Riku-lgtm43/RootJamesDSP/releases/download/stage31-deployable-stress-test-pending/JamesDSP-Stage31-Combined-Deployable-Corrected.zip)** · [Release notes and precautions](https://github.com/Riku-lgtm43/RootJamesDSP/releases/tag/stage31-deployable-stress-test-pending) · [Orbit integration source](https://github.com/Riku-lgtm43/RootJamesDSP/tree/orbit-integration)

SHA-256:

```text
0799875e20dcf2a05fdf99d08379344d8795641f0d17c99e301e6a994da34b44
```

## What this fork contains

This fork keeps the existing JamesDSP interface and processing tools, including equalizer and graphic EQ, convolver, bass, compander, crossfeed, reverb, stereo widening, tube/analog modelling, presets, and device profiles. The Orbit addition is a root-only preference card with on/off, stereo width (0–200%), HRTF strength (0–100%), full-rotation period (5–60 seconds), bass preservation (0–100%), and **Restore orbit defaults**. Reset leaves Orbit's on/off state unchanged.

Feature availability in source is not proof of complete regression testing. Only limited Orbit testing has been reported: audible playback over a USB DAC. With Bluetooth LDAC, DSP/effect attachment may appear without an audible Orbit difference; LDAC behavior remains unresolved. Other device routes, codecs, apps, and sample rates are not verified.

## Compatibility

The combined release is intended for a rooted **ARM64 Android** device using **KernelSU systemless modules**. It includes a device/ROM-specific `audio_effects.xml`; it is **not a universal flashable package**. Compare that file with the target device's current vendor audio configuration and confirm compatibility before installing. Android 16 installation and reboot testing are still pending.

The release ZIP embeds an owner-signed `james.dsp` APK (version 1.6.14, versionCode 51) and updates the existing module ID `jamesdsp_orbit_stage29` in place. An installed `james.dsp` signed with another certificate can conflict. Do not remove an existing app or its data until you have confirmed the signing identity and made a backup. `james.dsp.debug` is a separate package ID.

## Install and rollback

Before installing:

1. Read the [release precautions](https://github.com/Riku-lgtm43/RootJamesDSP/blob/orbit-integration/scripts/STAGE31-COMBINED-RELEASE.md).
2. Back up the current module ZIP and installed module directory, app data/settings, and APK.
3. Verify that the included vendor XML suits the ROM; resolve any `james.dsp` signature conflict.
4. Disable the older Stage23 experiment and competing DSP/Orbit controller services so only one module registers the native effect.
5. Install or update through KernelSU Manager, reboot, then verify the systemless app, mounted XML/libraries, effect registration, and playback.

For rollback, disable the updated module in KernelSU Manager and reboot. If needed, restore the previous module ZIP and app backup. Handle system app signature or data migration deliberately; keep backups until testing is complete.

## Outstanding tests

- Android 16 installation, reboot, and persistence.
- Extended stability and stress testing.
- Bluetooth LDAC Orbit behavior, including high sample rates.
- Reversible upgrade and rollback on the target device.
- Regression coverage for existing JamesDSP features.

Do not treat the current observations as proof of production stability or universal audio processing.

## Builds and development

The actual fork changes are on [`orbit-integration`](https://github.com/Riku-lgtm43/RootJamesDSP/tree/orbit-integration); the repository default branch remains the upstream-derived `master` branch with this landing-page documentation. The workflows produce different artifacts:

- [Root F-Droid debug workflow](https://github.com/Riku-lgtm43/RootJamesDSP/actions/workflows/root-fdroid-debug.yml) builds debug APKs and runs unit tests.
- [Root F-Droid unsigned release workflow](https://github.com/Riku-lgtm43/RootJamesDSP/actions/workflows/root-fdroid-release-unsigned.yml) produces unsigned CI APKs. They are not the signed app in the published combined ZIP.
- The published combined ZIP already contains the owner-signed APK. It is not an unsigned CI artifact.
- [Termux signing and packaging guide](https://github.com/Riku-lgtm43/RootJamesDSP/blob/orbit-integration/scripts/RELEASE-TERMUX.md) describes local signing with an owner-managed JKS. [Combined package reproduction and verification](https://github.com/Riku-lgtm43/RootJamesDSP/blob/orbit-integration/scripts/STAGE31-COMBINED-RELEASE.md) covers the module ZIP. Keep JKS files, passwords, private keys, and credentials local; never commit or upload them.

## Credits and license

This is an unofficial fork of [ShadoV90/RootJamesDSP](https://github.com/ShadoV90/RootJamesDSP), in the lineage of [ThePBone/RootlessJamesDSP](https://github.com/ThePBone/RootlessJamesDSP). JamesDSP's `libjamesdsp` is by [James Fung (@james34602)](https://github.com/james34602), whose project is [JamesDSPManager](https://github.com/james34602/JamesDSPManager). The source also retains upstream and third-party contributions and notices; see the [upstream project](https://github.com/ShadoV90/RootJamesDSP) and [RootlessJamesDSP contributors](https://github.com/ThePBone/RootlessJamesDSP/graphs/contributors).

The repository's license is [GNU GPL v3](LICENSE). Third-party components and packaged binaries may have separate terms; their notices and ownership are not changed by this fork. This project is not affiliated with or endorsed by the upstream projects.
