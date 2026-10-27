# Root JamesDSP Termux release flow

Designation: **Stage 31 Deployable — Stress Test Pending**.

Sign the unsigned APK downloaded from the `root-fdroid-release-unsigned` Actions artifact. Install Android build-tools 35.0.0 and set `ANDROID_HOME`. The JKS stays at `$HOME/.jamesdsp-signing/jamesdsp-release.jks`, alias `jamesdsp_release`.

```sh
bash scripts/sign-release-termux.sh path/to/unsigned.apk path/to/JamesDSP.apk
```

The signer prompts silently, zipaligns, signs, runs apksigner verification, and prints certificate fingerprint, app metadata, and SHA-256. Compare the signer fingerprint with your saved certificate record; retain the signed APK and keystore privately.

Package the supplied Stage 31 archive:

```sh
bash scripts/package-stage31-termux.sh path/to/JamesDSP_Orbit_Stage31_Deployable_Stress_Test_Pending.zip path/to/JamesDSP.apk path/to/JamesDSP-Stage31-Deployable-Stress-Test-Pending.zip
```

The script checks ZIP CRC, traversal/symlink safety, expected module ID and required files, native/config hashes from the archive manifest, APK signature and package `james.dsp`, and preserves checked files while adding the APK. It refuses a detected user-installed `james.dsp` conflict. It cannot compare installed module bytes from this host.

Before flashing, back up the current module ZIP/directory and app data, disable the separate Orbit controller, and ensure only one module provides the effect registration. Resolve any existing `james.dsp` signature conflict deliberately. Flash through KernelSU and reboot; test launch, effect registration, all native parameter readbacks and playback. For rollback, disable/remove only this module update and restore the backed-up ZIP in KernelSU. Do not remove unrelated modules or alter SELinux/audio routing. Device testing remains required before production designation.

## Termux ARM64 tool compatibility

The signing and packaging scripts prefer executable Termux-native `zipalign`, `apksigner`, and `aapt`, then try the matching SDK build-tools. Each candidate must execute a help/version probe; incompatible Linux-host SDK binaries are rejected with installation guidance. Typical native setup is `pkg install zipalign apksigner aapt`; verify package names against current Termux repositories. The package script refuses packaging while `james.dsp` is already installed, preventing an accidental systemless duplicate or unresolved signature replacement. Back up app data and make the existing installation transition deliberately before retrying.
