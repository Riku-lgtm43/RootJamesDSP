JamesDSP Orbit Stage31 — Combined APK + KernelSU Module
======================================================

RELEASE STATUS
- Stage: 31
- Classification: DEPLOYABLE TEST CANDIDATE — STRESS TEST PENDING.
- This COMBINED KernelSU module contains the native JamesDSP Orbit DSP
  payload AND the signed JamesDSP Android app at:
  system/app/JamesDSP/JamesDSP.apk
- APK package: james.dsp; version 1.6.14 (versionCode 51); ARM64 build.
- APK signing: owner-managed release key. The private JKS and signing
  passwords are NOT included in this archive.
- Stage31 is the integration/testing milestone. The native DSP libraries
  and vendor audio configuration are unchanged from the supplied working
  Stage31 deployable baseline (which carries Stage30-repackage binaries).
- Limited observed testing: native effect registration, basic control,
  and audible Orbit over USB DAC. Bluetooth LDAC Orbit was reported
  inaudible; its behavior remains unresolved, particularly at 96 kHz.
- No complete installation, reboot-persistence, rollback, or long-duration
  stress test has been verified for this combined ZIP.

COMPATIBILITY AND RISKS
- Rooted ARM64 Android device using KernelSU with systemless module support.
- This archive includes a DEVICE/ROM-SPECIFIC vendor audio_effects.xml
  snapshot. It is NOT a universal flashable package for other ROMs/devices.
  Compare with the currently installed vendor XML before deployment.
- The release app is packaged as a systemless system app via
  system/app/JamesDSP/JamesDSP.apk. Successful Android package scanning
  after reboot is NOT yet verified. Its package name is james.dsp.
- An installed james.dsp app with a different signature can conflict with
  this release APK. Resolve such conflicts with a data backup and confirmed
  signing identity BEFORE installing; do not blindly uninstall app data.
- james.dsp.debug has a distinct application ID and may coexist, but stop
  competing DSP app/services during functional testing.
- Does not directly modify physical vendor/system partitions or set
  SELinux permissive. Existing Stage23 experimental module must be disabled.
- The internal module ID is intentionally jamesdsp_orbit_stage29 to upgrade
  the existing Stage29/30/31 module IN PLACE, not create a second DSP effect.
  An update can overwrite the previously installed module files.

CONTENTS
- module.prop and customize.sh (original, unchanged)
- system/vendor/etc/audio_effects.xml (original, unchanged)
- system/vendor/lib64/soundfx/libjamesdsp_orbit.so (unchanged)
- system/vendor/lib64/soundfx/libphonon.so (unchanged)
- system/vendor/lib64/soundfx/libc++_shared.so (unchanged)
- system/app/JamesDSP/JamesDSP.apk (owner-signed ARM64 release)
- reference/binary_checksums.txt (original three native-library checksums)
- reference/vendor_audio_effects_current.xml (original snapshot)

CONTROLLED INSTALLATION — KERNELSU MANAGER, NOT RECOVERY
1. Back up the current working KernelSU module ZIP and installed module
   directory, current app data/settings, and the signed APK as applicable.
2. Confirm exact device/ROM suitability, including vendor XML overlay
   compatibility and your rollback route. Disable older Stage23 if present.
3. Confirm the intended in-place module ID jamesdsp_orbit_stage29 and
   resolve any release package james.dsp/signature conflicts first.
4. Stop the separate Orbit controller or other competing DSP managers.
5. Install this ZIP using KernelSU Manager and reboot. Do not install it
   as an additional module under a different ID.
6. Confirm APK registration, version and signing certificate, the mounted
   effect XML/libs, SELinux labels, and the native effect registration.
7. Check enable/disable, every Orbit control and its native readback,
   original JamesDSP features, USB audio playback, and Bluetooth LDAC.
8. Record findings before calling this a stable or generally deployable
   release. Stress tests and device rollback remain mandatory.

POST-INSTALL CHECKS (TERMUX)
su -c 'cat /data/adb/modules/jamesdsp_orbit_stage29/module.prop'
su -c 'ls -lZ /vendor/etc/audio_effects.xml /vendor/lib64/soundfx/libjamesdsp_orbit.so /vendor/lib64/soundfx/libphonon.so /vendor/lib64/soundfx/libc++_shared.so'
su -c 'grep -n jamesdsp /vendor/etc/audio_effects.xml'
su -c 'dumpsys package james.dsp | grep -E "Package \[|codePath=|versionCode=|User 0:"'

ROLLBACK
- If audio loss, severe distortion or instability appears, disable the
  updated module in KernelSU Manager and reboot. If necessary, restore the
  PREVIOUS working module ZIP and reboot again.
- Restoring the previous module may require removing the newly systemless
  release app and resolving app signature/data migration carefully.
- Keep previous module and app backups until all stability gates pass.
- Never delete an original installation, disable security protections,
  or modify physical partitions merely to make this ZIP install.

STRESS TEST GATE — NOT PASSED
- Extended USB DAC and Bluetooth LDAC playback, especially 48/96 kHz
- Orbit ON/OFF; four-parameter sweeps; clipping, crackling and silence
- 44.1/48/96 kHz and stereo/mono content; original JamesDSP features
- App/audio-service restarts, media switching, reboot persistence
- Calls, notifications, sleep/wake, route changes, reconnects
- Battery/CPU/thermal stability, native tombstones and unexpected crashes
- Reversible KernelSU upgrade/rollback on the actual device

INTEGRITY AND CHANGE POLICY
- The embedded signed APK and all original module payload files remain
  byte-for-byte identical to the audited input combined ZIP.
- Only this README.txt was corrected to match the combined distribution.
- Integrity checking is not a substitute for device testing or proof that
  the module is compatible with another device or Android build.
