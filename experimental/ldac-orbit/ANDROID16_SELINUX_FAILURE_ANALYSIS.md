# Android 16 engine startup failure: SELinux evidence

## Device evidence

The supplied filtered log identifies a POCO `duchamp_global` Android 16 build.
At boot, `audioserver` receives repeated SELinux `read` denials for the
overlaid `audio_effects.xml`; the target context in each denial is
`u:object_r:vendor_file:s0`. The same log later shows the JamesDSP app reading
PID `-1`, reporting an abnormal/zero sample rate, and receiving `-3`
(`ERROR_INVALID_OPERATION`) for effect parameter writes. The generic tombstone
previously supplied is for Termux `aapt2` reading `Settings.apk`, not
`audioserver` or JamesDSP.

This points to an effect registration/loading failure. It does not show that
the native DSP crashed with SIGSEGV. The exact transition from the SELinux
denial to failure to instantiate the effect cannot be proven from filtered
logcat alone, but the denial and invalid effect state align in time.

## Packaging discrepancy

The prior `customize.sh` recursively labeled every file beneath
`/system/vendor`, including `/system/vendor/etc/audio_effects.xml`, as
`vendor_file`. Android 16 policy maps `/vendor/etc(/.*)` to
`vendor_configs_file`; the uploaded device log confirms the overlay did not
have that context. The same directory contains the CIPIC SOFA data used by the
effect, so this candidate labels the full `/vendor/etc` subtree consistently.

The new candidate keeps native sound-effect libraries at `vendor_file` and
sets `/vendor/etc` and its descendants to `vendor_configs_file`. It does not
add an `allow audioserver vendor_file` rule, since that would grant broader
access than correcting the file label.

The log also contains reads denied for `vendor_default_prop`. The filtered
records do not identify the queried property or its caller, so no property
policy rule is added based on this evidence. Capture unfiltered audio-service
logs after confirming the corrected labels if this denial remains relevant.

## Validation status

The packaging test checks that the customizer assigns the two intended SELinux
types and that the native DSP binary and protected Stage 31 payload hashes
remain unchanged. Only an on-device reboot and `ls -Z`/effect-registration
check can establish that this correction works on the target ROM. The package
is experimental and must not be described as device-verified until that check
passes.
