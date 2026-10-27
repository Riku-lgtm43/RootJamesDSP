JamesDSP Orbit Stage 31 — Engine Audit Candidate
Experimental KernelSU module update; not production stable
============================================================

STATUS
- This is a separate opt-in build. It has not been installed on the phone.
- Android 16 install/reboot, LDAC route changes, AAC, USB DAC, all original
  effect combinations, and extended stability tests remain pending.
- Earlier user playback reports (clean short LDAC 44.1/48 kHz and audible
  Orbit at 96 kHz) apply to the preceding candidate, not this build.
- No system modification or automatic installation is performed by this work.

WHAT CHANGED
- Preserves queued ASRC samples when the engine grows callback storage, fixing
  high-rate output dependence on callback segmentation.
- Defers convolution, EQ, and advanced crossfeed refresh for disabled effects;
  removes the synchronous callback-time partition warning.
- Removes the delayed reference benchmark selection from the release build.
- Defines packed PCM24 conversion, PFB signed coefficients, and workspace
  alignment safely on ARM64.
- Checks config, parameter, frame-count, and allocation-failure boundaries.
- Does not change the JamesDSP effect algorithms, Orbit controls, signed APK,
  Steam Audio library, CIPIC data, or app settings.

IDENTITY AND PRESERVED FILES
- KernelSU module ID: jamesdsp_orbit_stage29.
- Effect implementation UUID:
  f27317f4-c984-4de6-9a90-545759495bf2.
- Signed JamesDSP APK and signing certificate are preserved byte-for-byte from
  the published Stage 31 package. This module ZIP is not an APK signing build.
- libphonon.so, libc++_shared.so, audio_effects.xml, customize.sh, CIPIC SOFA,
  and its redistribution notice are preserved from the verified package.
- Orbit's persistent 128-frame bridge remains. It adds up to 128 samples of
  processing latency when enabled.

INSTALL AND ROLLBACK
1. Back up the installed module, app settings, and known-good Stage 31 ZIP.
2. Install through KernelSU Manager only as an in-place update; do not add a
   second copy of the module.
3. Start at low volume. Test short sections at negotiated LDAC rates with Orbit
   OFF, then ON. Stop immediately for crackle, pops, dropouts, silence, or
   unexpected loudness. Test AAC and USB separately only after normal behavior.
4. To roll back, disable the module in KernelSU Manager and reboot. Restore the
   saved Stage 31 package through KernelSU Manager and reboot again.

VERIFICATION
See reference/Verification_Report.md and the four audit documents in the
repository. Host tests use the real JamesDSP core and legacy wrapper; only the
Steam Audio API boundary is mocked. Hardware HRTF quality, Android framework
behavior, and long-duration playback are not certified by these tests.
The CIPIC attribution and license notice is at licenses/CIPIC-HRTF-NOTICE.txt.
No JKS, signing password, or signing secret is included.
