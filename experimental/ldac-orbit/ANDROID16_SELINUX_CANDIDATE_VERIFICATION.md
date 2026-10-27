# Android 16 SELinux label candidate verification

## Device-log finding

The uploaded device log identifies a POCO `duchamp_global` Android 16 build.
At boot, `audioserver` repeatedly receives an SELinux `read` denial for the
module-overlay `audio_effects.xml`; the file is labeled
`u:object_r:vendor_file:s0`. Later, JamesDSP reports an abnormal sample rate,
reads PID `-1` for session 0, and receives `-3` (`ERROR_INVALID_OPERATION`)
while writing effect parameters. The supplied tombstone is for Termux `aapt2`
reading `Settings.apk`, not for JamesDSP or `audioserver`.

This indicates effect registration or loading failed. It does not establish a
native DSP segmentation fault. In the app source,
`JamesDspRemoteEngine.isSampleRateAbnormal` treats a nonpositive sample rate as
abnormal; that check can therefore report the observed toast when the effect
instance is unavailable.

## Correction in this candidate

Android 16 policy maps `/vendor/etc(/.*)` to `vendor_configs_file`. The prior
module customizer assigned `vendor_file` recursively to every file under
`/system/vendor`, which includes the effect registration XML and the CIPIC
configuration/HRTF data. This candidate retains `vendor_file` for sound-effect
libraries and assigns `vendor_configs_file` to `/system/vendor/etc` and its
contents. It adds no SELinux allow rule and changes no native DSP binary.

The `vendor_default_prop` read denials in the supplied log are unresolved:
the filtered records do not identify the property key or caller. This package
does not add broad property access. If those denials remain after the file
labels are confirmed, capture full audio-service log context before changing
policy.

## Offline checks

- Termux packager validates the base candidate SHA-256 and checks the ARM64 DSP
  library, signed APK, `libphonon.so`, `libc++_shared.so`, `audio_effects.xml`,
  and CIPIC SOFA hashes before packaging and after extraction.
- The generated ZIP is integrity-checked and its sidecar SHA-256 is verified.
- The customizer is syntax-checked and statically checked for both intended
  labels and the absence of an `audioserver` policy grant.
- No app, APK, native DSP, Steam Audio, or signing-certificate rebuild occurs.

## Device checks still required

This candidate has not been installed. After an opt-in in-place update and
reboot, first inspect labels with `ls -Z`:

```sh
su -c 'ls -lZ /vendor/etc/audio_effects.xml /vendor/etc/axionfx/hrtf/cipic_124.sofa /vendor/lib64/soundfx/libjamesdsp_orbit.so /vendor/lib64/soundfx/libphonon.so'
```

Confirm both `/vendor/etc` files use `vendor_configs_file`, both sound-effect
libraries use `vendor_file`, and the app reports a valid effect PID/sample
rate before trying playback. If registration remains unavailable, disable the
module and reboot. LDAC, AAC, USB DAC, effect combinations, and extended
stability remain pending on this Android 16 ROM.
