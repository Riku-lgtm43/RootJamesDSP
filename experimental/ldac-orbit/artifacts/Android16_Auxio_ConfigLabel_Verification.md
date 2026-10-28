# Android 16 Auxio silence: config-label candidate v38

Status: **experimental packaging candidate; reboot and device verification are
still required.** This is based on the user's Auxio master-on log. It does not
claim to have confirmed that the SELinux denial is the only cause of silence.

## Device evidence

- At boot, `audioserver` receives repeated SELinux `read` denials for the
  overlaid `audio_effects.xml`, whose target context is `vendor_file`.
- During Auxio playback, the log maps PID 7848 to session 297 and records
  `AS.MusicFxHelper` opening session 297 for `org.oxycblt.auxio`.
- The filtered log does not record successful JamesDSP effect creation for
  session 297 or the negotiated track format/channel mask. It cannot establish
  whether the missing sound follows from effect registration, session
  attachment, or the native processing configuration.
- Android 16 labels files under `/vendor/etc` as `vendor_configs_file`; the
  package's current customizer instead applies `vendor_file` to all of
  `/system/vendor`. The observed denial matches that labeling error.

## Package change

Version 38 applies `vendor_file` to the overlay payload, then reapplies
`vendor_configs_file` to `/system/vendor/etc` and its files. This includes
`audio_effects.xml`, `vendor_audio_effects_current.xml`, and the bundled HRTF
configuration. Sound-effect libraries under `system/vendor/lib64/soundfx`
remain `vendor_file`.

The change adds no SELinux allow rule and performs no live system write. It
changes only `customize.sh`, module metadata, and documentation. The v37 Orbit
library, signed APK/certificate, Steam Audio libraries, audio effects XML,
CIPIC SOFA, module ID, and effect UUID are unchanged. No native code is rebuilt.

## Offline checks

- `bash -n` passes for the v38 customizer.
- Static checks confirm the config and library label assignments are ordered
  correctly and no SELinux allow rule was added.
- The v38 package compares byte-for-byte with v37 for the APK, DSP library,
  `libphonon.so`, `libc++_shared.so`, audio effects XML, CIPIC SOFA, and module
  installer payload files other than `customize.sh`.
- The complete ZIP integrity check and external SHA-256 sidecar pass.
- No JKS, keystore, password, or signing secret is present.

## Required device verification

Install only as an in-place opt-in update after backing up the current module.
After reboot, check labels:

```sh
su -c 'ls -lZ /vendor/etc/audio_effects.xml /vendor/etc/vendor_audio_effects_current.xml /vendor/etc/axionfx/hrtf/cipic_124.sofa /vendor/lib64/soundfx/libjamesdsp_orbit.so /vendor/lib64/soundfx/libphonon.so'
```

The files under `/vendor/etc` should show `vendor_configs_file`; the soundfx
libraries should show `vendor_file`. Then test Auxio with the master switch on
and off. Verify an active JamesDSP session/effect in `dumpsys media.audio_flinger`
before concluding that playback is repaired. USB hardware rate, Orbit
initialization, Android 16 reboot behavior, and Enhanced-mode stability remain
separate device checks. To roll back, disable the module in KernelSU, reboot,
and restore the backed-up Stage 31 package.
