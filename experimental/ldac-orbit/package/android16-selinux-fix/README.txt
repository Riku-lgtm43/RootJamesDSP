JamesDSP Orbit Stage 31 — Android 16 SELinux Label Candidate
Experimental KernelSU module update; not production stable
==========================================================

WHY THIS CANDIDATE EXISTS
- The uploaded POCO Android 16 log records audioserver SELinux denials while
  reading the module-overlay /vendor/etc/audio_effects.xml, labeled
  vendor_file. The app then reports an invalid effect PID and a zero/invalid
  sample rate. This is an effect registration/loading failure in the supplied
  evidence; the log does not show a native DSP SIGSEGV.
- This candidate gives /vendor/etc and its contents the standard Android
  vendor_configs_file SELinux type. Native sound-effect libraries retain the
  vendor_file type. It adds no SELinux allow rule and does not relax policy.
- This is a proposed correction based on the supplied audit log. It has not
  yet been installed or validated on the phone.

ENGINE CONTENT
- The ARM64 DSP library is byte-identical to the Engine Audit candidate.
- The signed JamesDSP APK, certificate, libphonon.so, libc++_shared.so,
  audio_effects.xml, CIPIC data, and native module ID are unchanged.
- KernelSU module ID: jamesdsp_orbit_stage29.
- Android 16 LDAC, AAC, USB DAC, effect combinations, and stress behavior
  remain unverified.

INSTALL AND ROLLBACK
1. Back up the current module and known-good Stage 31 ZIP.
2. Install only as an in-place update through KernelSU Manager; do not enable a
   second copy of the same module.
3. Reboot. Before playback, verify with `ls -Z` that
   /vendor/etc/audio_effects.xml and /vendor/etc/axionfx/hrtf/cipic_124.sofa
   are labeled vendor_configs_file, and the soundfx libraries are vendor_file.
4. If registration still fails, stop testing. Disable the module in KernelSU
   Manager and reboot; restore the saved Stage 31 ZIP if needed.

No installation or reboot is performed by the packaging workflow.
