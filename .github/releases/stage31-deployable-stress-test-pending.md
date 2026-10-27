## Stage 31 combined KernelSU module — stress test pending

This pre-release contains one combined ARM64 KernelSU ZIP with the signed
JamesDSP app (`james.dsp` v1.6.14, versionCode 51) and the Stage 31 native DSP
payload. The existing module ID is preserved as `jamesdsp_orbit_stage29`.

**SHA-256:** `0799875e20dcf2a05fdf99d08379344d8795641f0d17c99e301e6a994da34b44`

### Supported scope and observed behavior

- Rooted ARM64 Android with KernelSU and systemless modules.
- Device/ROM-specific `audio_effects.xml`; this is not a universal ROM package.
- Native effect registration and basic Orbit control have limited observations.
- Audible Orbit playback has been observed over USB DAC. Bluetooth LDAC Orbit
  behavior remains unresolved.

### Installation precautions

Back up the current module ZIP and directory, app data/settings, and APK.
Confirm the included vendor XML is suitable for the device, disable the older
Stage23 experiment, resolve any `james.dsp` signature conflict, and stop
competing DSP services. Install through KernelSU Manager as an in-place update
of `jamesdsp_orbit_stage29`, then reboot. Verify package registration, mounted
libraries and XML, SELinux labels, effect registration, and playback.

### Rollback

If audio problems or instability occur, disable the updated module in
KernelSU Manager and reboot. If needed, restore the previous module ZIP and
reboot again. Resolve systemless APK removal or signature/data migration
carefully; retain app and module backups until validation is complete.

### Tests still pending

- Android 16 installation and reboot persistence.
- Extended stability and stress testing.
- Bluetooth LDAC Orbit behavior, including higher sample rates.
- Reversible upgrade and rollback on the target device.

This is a deployable test candidate, not a production-stable release. Project
license and attribution information is available in the repository `LICENSE`
and source notices. The release contains no private JKS, signing passwords,
or private signing keys. See `scripts/STAGE31-COMBINED-RELEASE.md` for the
reproduction and integrity-verification procedure.
