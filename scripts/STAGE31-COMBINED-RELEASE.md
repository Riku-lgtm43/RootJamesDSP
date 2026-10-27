# Stage 31 combined KernelSU release

The release asset is one combined KernelSU module ZIP containing the existing
Stage 31 module payload and its owner-signed `james.dsp` ARM64 APK. Do not put
the two split-upload ZIPs in a release; they are transport pieces, not module
packages.

## Reproduce a combined package

Use the verified Stage 31 module ZIP and the signed APK as inputs. The signing
JKS remains local and is never an input to the packaging script.

```sh
bash scripts/package-stage31-termux.sh \
  path/to/JamesDSP_Orbit_Stage31_Deployable_Stress_Test_Pending.zip \
  path/to/JamesDSP-v1.6.14-51-root-fdroid-arm64-v8a-release.apk \
  path/to/JamesDSP-Stage31-Combined-Deployable-Corrected.zip
bash scripts/verify-stage31-release.sh \
  path/to/JamesDSP-Stage31-Combined-Deployable-Corrected.zip
```

The packager preserves the module ID `jamesdsp_orbit_stage29`, copies the
tracked `README-Stage31-Combined.txt` into the root as `README.txt`, verifies
the native-library checksums, APK signature, and `james.dsp` package name, and
keeps all other module entries. The verifier checks ZIP safety and CRCs, the
module ID and README, native hashes, the XML reference and Orbit registrations,
the APK signature, package/version, and ARM64 metadata. Pass an expected
SHA-256 as the verifier's second argument when checking a published artifact.

The corrected combined asset reconstructed for the Stage 31 pre-release is
23,654,654 bytes, SHA-256
`0799875e20dcf2a05fdf99d08379344d8795641f0d17c99e301e6a994da34b44`.
Reconstruction used the first payload followed by the second and did not
repack or alter the combined ZIP.

## Publication and device precautions

Publish a combined ZIP as a GitHub Release asset and mark the release as a
pre-release until installation, reboot, and extended stress testing pass.
The asset is device/ROM-specific because it contains a vendor audio effects
XML snapshot. It is intended for a rooted ARM64 KernelSU device with
systemless module support and upgrades the existing module ID in place.

Before installation, back up the currently working module ZIP and directory,
app data/settings, and signed APK; confirm the vendor XML is suitable; disable
the older Stage23 experiment; resolve any `james.dsp` signing conflict; and
stop competing DSP services. Install with KernelSU Manager, reboot, and verify
package registration, mounted libraries/XML, SELinux labels, and playback.
For rollback, disable the updated module in KernelSU Manager and reboot; if
needed, restore the previous module ZIP. Resolve removal or restoration of the
systemless APK carefully to avoid losing app data.

Outstanding validation includes Android 16 installation and reboot, extended
stability and stress runs, and Bluetooth LDAC Orbit behavior. USB DAC playback
and basic Orbit control have limited prior observations only. Do not call this
release production-stable.

## Licensing and signing material

Keep license and attribution files supplied with module inputs; the packager
does not remove them. See the repository's `LICENSE` and source notices for
project licensing and attribution. Do not infer that one repository license
covers every third-party component. The signed APK and native payload are
copied as supplied. Never add a private JKS, signing password, private key, or
credential file to the archive or release.
