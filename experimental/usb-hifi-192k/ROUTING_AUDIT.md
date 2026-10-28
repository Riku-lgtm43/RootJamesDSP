# JamesDSP USB HiFi 192 kHz routing audit

Status: routing cause narrowed to the legacy output-mix path. The exact AxionOS policy decision or effect-chain move failure still needs one capture with routing logs. No audio configuration, vendor file, native library, APK, or device state was changed. No 192 kHz fix ZIP is produced until the active HiFi route is proven to accept the chosen attachment.

## Evidence from the device reports

The supplied Android 16 snapshot shows Apple Music's active PCM-float, stereo, 192 kHz client routed to the `hifi_playback` MIXER output. The output has no effect chains. JamesDSP and AxionFX are enabled on session 0 of the primary output. That proves the current global JamesDSP chain is not processing the active HiFi output. It does not show whether AudioPolicy selected the wrong destination, whether a move was rejected, or which effect/configuration caused a rejection.

The ordinary Lossless snapshot shows a 44.1 kHz Apple Music client mixed at 48 kHz on a DEEP_BUFFER output with one chain containing JamesDSP and AxionFX. The user's EQ A/B check confirmed that the effects were audible on that route. This establishes that the DAC and JamesDSP can work together on the ordinary route.

The extra `DUPLICATING` thread is an AudioFlinger fan-out/mirroring thread that writes a mixed stream to downstream output tracks. It is not the HiFi playback thread used by the active player. Android 16 explicitly rejects session-0 global effects on a DUPLICATING thread. The report must include its downstream/output mapping before assigning it a device role; the thread name and sample rate alone are insufficient.

## Code path in this fork

In root legacy mode, `RootAudioProcessorService.updateLegacyMode()` opens audio session 0. `RootSessionDatabase.addSessionByIntent()` constructs `JamesDspRemoteEngine` for that session. The engine calls the four-argument `AudioEffect` constructor with the session ID; this API does not accept a playback output I/O handle. The effect XML registers the library and UUID, but does not choose an output route.

The fork already includes a supported per-app path. With Legacy mode off and Enhanced processing on, `RootSessionDumpManager` discovers active playback sessions through system audio dumps and `AudioPlaybackCallback`; `RootSessionDatabase` creates one JamesDSP effect using each discovered session ID. For the reported Apple Music session (73 in that capture), this requests a session effect on the player's live thread instead of a global session-0 effect. The session number is transient and must be discovered each run. Whether the ROM allows and successfully configures that session effect at 192 kHz must still be confirmed on the device.

The native effect source accepts stereo float and PCM 32-bit configurations and passes the configured sample rate to the DSP core. The DSP core's rate-selection code internally resamples rates above 48 kHz to its 44.1/48 kHz processing rate and returns to the stream rate. That is software processing and is not bit-perfect. This is separate from the missing chain: no sample-rate or Orbit code can process audio while the active thread has zero chains.

## Android 16 framework trace

Source checked from AOSP `platform/frameworks/av` branch `android16-release`, commit `4b2111885ff934799e70eddae2527a9848c3be29`:

1. `AudioFlinger::createEffect()` treats session 0 as `AUDIO_SESSION_OUTPUT_MIX`. When no I/O handle is supplied, it asks `AudioSystem::getOutputForEffect()` for the target output.
2. `AudioPolicyManager::selectOutputForMusicEffects()` chooses among outputs matching media devices. It first considers active outputs, prioritizing offload, spatializer, deep buffer, primary, then the first matching output. AudioPolicy calls `moveEffects()` when the selected output changes.
3. `AudioFlinger::moveEffectChain_ll()` checks the entire chain against the destination thread and reconfigures each effect. If any chain member is incompatible or fails `EFFECT_CMD_SET_CONFIG`, the move is rolled back. Thus AxionFX could block a move even if JamesDSP alone is compatible; the supplied dump does not prove that this occurred.
4. A normal MIXER output with no FAST/RAW/DIRECT/BIT_PERFECT restriction can accept a software insert effect. The supplied `hifi_playback` flags are NONE and its thread type is MIXER, so AOSP does not reject a software effect solely because the rate is 192 kHz. AOSP configures mixer effects in float at the thread's sample rate. Vendor AudioPolicy changes and effect-specific config behavior remain possible.
5. A session-specific effect is located by its live session ID in the playback threads. It avoids the global music-effect output selection, but still requires successful effect creation/configuration and on-device A/B validation.

Relevant AOSP source:

- [AudioFlinger createEffect and effect-chain migration](https://android.googlesource.com/platform/frameworks/av/+/4b2111885ff934799e70eddae2527a9848c3be29/services/audioflinger/AudioFlinger.cpp)
- [Playback thread effect compatibility](https://android.googlesource.com/platform/frameworks/av/+/4b2111885ff934799e70eddae2527a9848c3be29/services/audioflinger/Threads.cpp)
- [AudioPolicy music-effect output selection](https://android.googlesource.com/platform/frameworks/av/+/4b2111885ff934799e70eddae2527a9848c3be29/services/audiopolicy/managerdefault/AudioPolicyManager.cpp)
- [Effect configuration and compatibility](https://android.googlesource.com/platform/frameworks/av/+/4b2111885ff934799e70eddae2527a9848c3be29/services/audioflinger/Effects.cpp)

## Root-cause conclusion and remaining evidence

Confirmed component boundary: the failure is at output-mix attachment/routing. The app creates session 0 in Legacy mode; the Android framework/ROM must select and move that global chain. It is not an Orbit DSP algorithm failure.

The current dumps do not identify whether AxionOS selects the primary output for global effects, whether its policy excludes the HiFi profile from the matching media outputs, or whether moving the combined JamesDSP/AxionFX chain fails compatibility/configuration. A source-only “reattach” or XML edit cannot distinguish those conditions and could create duplicate or broken chains. For that reason, this branch does not add speculative reconnection logic or modify audio policy configuration.

First supported correction to verify: switch the root app from deprecated Legacy mode to its existing Enhanced processing per-app mode. This is the fork's existing route-aware session attachment path and should target the active player's discovered session. It is not yet called a 192 kHz fix until the checks below pass on the Poco X6 Pro/AxionOS build.

AxionFX is a controlled diagnostic only. AudioFlinger checks all members of a global chain during migration. If temporarily disabling AxionFX allows the global chain to move, that is evidence of an AxionFX compatibility conflict; restore it immediately after the short test. Do not remove its module or edit its XML.

## Termux capture

Run the read-only script while music is actively playing. One run creates one timestamped report. Run it once during Apple Music ordinary Lossless and again during Hi-Res Lossless if a before/after comparison is needed. It captures the policy, AudioFlinger, active players, recent effect/routing logs, effect XML candidates, and readable ALSA USB stream files, with root-first access and user-shell fallback.

After this branch is pushed:

```sh
curl -fsSL 'https://raw.githubusercontent.com/Riku-lgtm43/RootJamesDSP/fix/usb-hifi-192k-effect-routing/experimental/usb-hifi-192k/tools/capture-routing-diagnostics.sh' -o "$HOME/capture-jamesdsp-usb-routing.sh" && bash "$HOME/capture-jamesdsp-usb-routing.sh"
```

If Termux has not been granted shared-storage access, run `termux-setup-storage` once and approve it, then rerun the command. The report is written under `/sdcard/Download/` by default. The script does not change audio settings, modules, permissions, effect configuration, or system files.

## Device validation guide

### Route check and supported per-app path

1. Keep the known-good Stage 31 ZIP and current KernelSU module available for rollback. Do not install a new module for this diagnostic.
2. In JamesDSP Settings → Audio processing, check Legacy mode. If it is enabled, switch Legacy mode off. Enable Enhanced processing and complete the app's system dump permission setup. This selects the existing per-app session path; it does not change Apple Music's audio quality setting.
3. Force-stop and reopen Apple Music after changing processing mode. Attach the JCALLY DAC and start Apple Music Hi-Res Lossless playback. Confirm the phone reports the USB HiFi output as active before capturing.
4. Run the capture command above. In `dumpsys audio`, identify the active Apple Music session. In AudioFlinger, find the active `hifi_playback` MIXER and verify a JamesDSP-enabled effect chain for that same session on that same thread. Confirm the player session is not merely present on the primary or DUPLICATING thread.
5. At low volume, use a pronounced but safe EQ A/B change in JamesDSP. Pass requires an audible change with continuous playback and no crackle, pop, dropout, silence, clipping, or channel swap. Capture a second report after reverting the EQ change.

If no session effect appears, inspect the app log and the new AudioPolicy/AudioFlinger log lines for effect creation, ownership/permission, format/configuration, and move errors. As a controlled test only, temporarily disable AxionFX in its own UI, restart Apple Music playback, capture again, then restore AxionFX. Do not disable it permanently or remove its files.

### Required route matrix

| Scenario | Pass evidence |
| --- | --- |
| Ordinary Lossless 44.1 kHz client / 48 kHz mixer | Active route has JamesDSP chain; EQ A/B audible; no artifact. Existing user capture already passes this route. |
| USB HiFi 96 kHz | Record player, mixer, USB stream rates independently; matching active mixer session has JamesDSP; EQ A/B audible without artifacts. |
| USB HiFi 192 kHz | Active player → active `hifi_playback` MIXER → enabled JamesDSP chain for the same session → USB output; report PCM format/sample rate at client, mixer, and ALSA where readable; EQ A/B audible. |
| USB unplug/replug and sample-rate change | One active chain after each transition, no stale/duplicate session and no restart-button interaction. |
| Apple Music restart, screen off, audio service restart, reboot | Processing returns after playback resumes; no service crash, duplicate chain, or unprocessed route. |
| AxionFX isolation | Compare only as a short diagnostic and restore AxionFX. If chain migration changes, include both captures and relevant logs. |
| Rollback | Disable Enhanced processing or re-enable Legacy mode to return to prior app behavior. If a later module candidate is installed, disable it in KernelSU, reboot, restore the saved known-good Stage 31 ZIP, and reboot. |

Do not call output “bit-perfect” with software DSP enabled. Do not force 192 kHz playback to 48 kHz. USB ALSA stream rate can differ from the player/client/mixer snapshot and must be recorded independently.

## Status matrix

| Area | Status |
| --- | --- |
| Ordinary Lossless, 44.1 → 48 kHz mixer | **User-confirmed pass** in supplied capture and EQ A/B listening. |
| Legacy session-0 effect on USB HiFi 192 kHz | **Fail** in supplied capture: active mixer has zero effect chains; global chain is on primary. |
| Enhanced per-app effect on USB HiFi 192 kHz | **Unverified**; existing supported path selected for first test. |
| 96 kHz LDAC Orbit | **User-confirmed on the previously tested build** as audible without crackling/popping. The later v36 artifact has not been installed/device-tested. |
| Signed APK, native library, module ID, rollback | **Preserved**; no package or device mutation on this branch. Existing module ID remains `jamesdsp_orbit_stage29`. |

## Why no KernelSU ZIP is included

There is not yet an evidence-supported system/module change. The existing Enhanced processing session path is the narrowest available app-level option to test; if it fails, the captured logs can identify whether AxionFX, ROM output selection, or a 192 kHz config response blocks attachment. Only after a specific supported change passes effect-chain and device listening checks should an experimental package be made.
