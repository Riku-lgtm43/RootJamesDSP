# LDAC Orbit frame bridge candidate

This is an opt-in experimental native update for the existing Stage 31
KernelSU module. It keeps module ID `jamesdsp_orbit_stage29` and effect UUID
`f27317f4-c984-4de6-9a90-545759495bf2`. It does not alter the published
Stage 31 ZIP or install anything automatically.

## Evidence and scope

The user confirmed Orbit crackle/pops on LDAC 44.1 kHz and harsher pops at
48 kHz, while Orbit is inaudible on LDAC 96 kHz. The supplied device dump
shows an active stereo-float JamesDSP effect at 96 kHz with 1,920-frame
buffers, and the LDAC codec state also reports 96 kHz. It does not report the
Orbit parameter state or Steam Audio initialization result, so the high-rate
silence cause remains unconfirmed.

In the supplied Stage 18 archival source, `SteamSpatial::process()` used
fixed 128-frame Steam Audio blocks and passed a shorter callback tail through
dry. This aligns with 44.1 kHz 882-frame and 48 kHz 960-frame callback sizes,
but the user dump does not contain a per-callback trace. Treat this as a
confirmed source behavior and a plausible crackle mechanism, not proof of the
device root cause.

The patch adds a preallocated per-instance 128-frame bridge, a bounded 128
frame processing latency, a short fade when the first processed block becomes
available, and a bounded fade back to current dry audio when Orbit is turned
off. It also logs accepted/rejected effect configuration and Steam Audio
initialization results outside the per-buffer processing loop. It does not
change JamesDSP's algorithms, parameter IDs, app, APK, Steam Audio library,
other native dependencies, effect UUID, or audio XML.

## Provenance and build

The patch targets the uploaded `JamesDSP_Stage18_Full_Link` source archive,
not an asserted exact Stage 31 source checkout. Apply it from that source
tree's root, where `source/` is present:

```sh
patch -p1 < stage18-frame-bridge.patch
```

The candidate was built for ARM64 with Android NDK r27 / Clang 18.0.1,
`-std=c++17 -fPIC -O2 -DJAMESDSP_REFERENCE_IMPL`, target API 26, strict
undefined-symbol linking, and the exact `libphonon.so` extracted from the
published Stage 31 package (SHA-256
`857612e669414165daabd95423d6fcf5fc51e57a8e2a0d07e0b39eadaacb0369`). It
retains `RUNPATH=/vendor/lib64/soundfx` and Stage 31's `NEEDED` library list.
Its dynamic exports introduce no new symbols relative to the Stage 31
reference; the two Stage 31-only CPU resolver symbols are absent from both
the baseline rebuild and candidate.

The supplied Stage 18 and Stage 31 reference have compatible observed effect
exports, but the Stage 18 source is archival and the local NDK baseline is
not byte-identical to the Stage 31 ELF. The user-reported historical
byte-equality result after RUNPATH patching was not independently reproduced
here.

## Offline regression test

The deterministic host harness mocks only the Steam Audio API boundary; it
does not claim to model the real HRTF output. Run it with the patched source
and the Steam Audio headers:

```sh
experimental/ldac-orbit/scripts/run-host-regression.sh \
  /path/to/JamesDSP_Stage18_Full_Link/source \
  /path/to/steamaudio/include
```

It exercises 44.1/48/96 kHz, divisible and irregular callbacks including
1, 64, 96, 127, 128, 129, 192, 240, 256, 480, 512, 882, 960, 1,024, 1,920,
and 2,048 frames, callback segmentation equivalence, canaries, finite output,
stereo separation, peak and discontinuity bounds, disabled passthrough,
initialization failure/recovery, disable transition, reset, and rate change.
The harness was run with ASan and UBSan and passed. Hardware LDAC, AAC, and USB
playback, actual Steam Audio HRTF output, and stress testing remain unverified.

## Device diagnostics and rollback

This candidate is for a short, controlled opt-in test only. Back up the
installed module and the known-good Stage 31 ZIP first. Leave Orbit off on
LDAC until testing. In KernelSU Manager, update the existing module with the
experimental ZIP; do not install it as a second module. If the candidate
causes audio problems, disable it in KernelSU Manager and reboot, then restore
the backed-up Stage 31 ZIP and reboot. Keep the current signed app, module
backup, and settings backup. Never flash the ZIP from recovery on an unknown
ROM.

Check `logcat -s JamesDSPOrbit AxionFxSpatial` for route configuration and
HRTF initialization status. The logs do not prove that the spatial output is
audible. Test short playback first: Orbit OFF then ON at LDAC 44.1, 48, and
96 kHz, followed by AAC and USB controls. Stop a route immediately on
crackle, popping, dropout, or abnormal level. Do not perform extended playback
until every short test is clean. Do not call LDAC fixed until the user reports
device results.
