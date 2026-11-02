JamesDSP v40 Enhanced mode signal diagnostic (temporary)
========================================================

This is an instrumented v40 KernelSU module for locating app-specific silence.
It is not an audio fix and is not intended for regular listening. It keeps the
v40 module ID, signed APK, certificate, JamesDSP DSP core objects, Orbit objects,
Steam Audio library, HRTF data, audio effects XML, and installer unchanged. Only
the native effect entry point has aggregate signal counters and diagnostic logs.

Install manually as an in-place update to v40, reboot, and keep the original v40
ZIP available for rollback. Do not install it alongside another JamesDSP module.
If audio is unexpectedly loud, distorted, or unstable, stop playback and restore
v40 in KernelSU Manager, then reboot.

Each JamesDSP-Signal log record identifies the effect session and shows:
  calls, frames: processing activity in the preceding reporting interval
  nz: nonzero sample counts at input / after JamesDSP core / after Orbit
  peak_u: corresponding absolute peaks in millionths of full scale
  nonfinite: NaN or infinity sample count across all three points
  errors: native process errors; unmetered: non-float or oversized callbacks
  enabled, rate, format: most recent effect control state

No PCM samples, music, media titles, passwords, or private signing material are
recorded. The aggregate monitor logs from a worker thread about once a second.

Suggested short test (USB DAC, unchanged volume and profile):
  1. Start a root logcat capture filtered to JamesDSP-Signal and JamesDSP-Test.
  2. With the master ON, play a TikTok clip known to be silent for five seconds.
  3. With the master still ON, play audible Apple Music for five seconds.
  4. With TikTok playing, turn only the master OFF, then play five seconds.
  5. If possible, repeat TikTok with master ON but Orbit OFF for five seconds.
  6. Stop capture and restore the original v40 ZIP after testing.

For each phase, mark it in Termux using:
  su -c '/system/bin/log -t JamesDSP-Test "TIKTOK_MASTER_ON"'
  su -c '/system/bin/log -t JamesDSP-Test "APPLE_MASTER_ON"'
  su -c '/system/bin/log -t JamesDSP-Test "TIKTOK_MASTER_OFF"'
  su -c '/system/bin/log -t JamesDSP-Test "TIKTOK_ORBIT_OFF_MASTER_ON"'

Capture command:
  su -c "/system/bin/logcat -v year,threadtime -b main -b system -s JamesDSP-Signal:I JamesDSP-Test:I '*:S'" > "$HOME/storage/shared/Download/JamesDSP_v40_Signal_Diagnostic.txt" &
  CAPTURE_PID=$!

After the phases, run: kill "$CAPTURE_PID"

Capture an AudioFlinger session map while TikTok is playing and master is ON:
  su -c '/system/bin/dumpsys media.audio_flinger' > "$HOME/storage/shared/Download/JamesDSP_v40_Flinger_TikTok.txt"

The recorder commands do not install or alter modules. The separate module ZIP
must be installed manually in KernelSU Manager to enable these counters.
