#!/usr/bin/env python3
"""Overlay the verified v40 diagnostic native library onto the intact v40 ZIP."""

import hashlib
import pathlib
import sys
import zipfile

BASE_SHA = "f3d401aea814c27500f88d70235bf7e22d085ffe0abf1063fc3198347d2ee46e"
NATIVE_SHA = "c50b05d00a4688edef44e17feff17ad5ce8b31a8f738eb1a56ad7fe4054a6df9"
NATIVE_PATH = "system/vendor/lib64/soundfx/libjamesdsp_orbit.so"
CHECKS_PATH = "reference/binary_checksums.txt"
README_PATH = "README.txt"
NOTE_PATH = "reference/Enhanced_Silence_Diagnostic.txt"
PROP_PATH = "module.prop"
PAYLOADS = (
    "system/app/JamesDSP/JamesDSP.apk",
    "system/vendor/etc/audio_effects.xml",
    "system/vendor/lib64/soundfx/libc++_shared.so",
    NATIVE_PATH,
    "system/vendor/lib64/soundfx/libphonon.so",
    "system/vendor/etc/axionfx/hrtf/cipic_124.sofa",
)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: package.py v40-module.zip diagnostic-libjamesdsp_orbit.so output.zip")
    base_path, native_path, output_path = map(pathlib.Path, sys.argv[1:])
    if output_path.exists():
        raise SystemExit(f"output already exists: {output_path}")
    if sha256(base_path.read_bytes()) != BASE_SHA:
        raise SystemExit("refusing: v40 module SHA-256 differs")
    native = native_path.read_bytes()
    if sha256(native) != NATIVE_SHA:
        raise SystemExit("refusing: diagnostic native SHA-256 differs")
    note = pathlib.Path(__file__).with_name("README-DIAGNOSTIC.txt").read_bytes()

    with zipfile.ZipFile(base_path) as base:
        if base.testzip() is not None:
            raise SystemExit("v40 ZIP integrity check failed")
        infos = base.infolist()
        if len({i.filename for i in infos}) != len(infos):
            raise SystemExit("duplicate v40 ZIP entries")
        data = {i.filename: base.read(i.filename) for i in infos if not i.is_dir()}
        if set(PAYLOADS) - data.keys():
            raise SystemExit("v40 ZIP is missing required payload")
        prop = data[PROP_PATH].decode()
        prop = prop.replace(
            "name=JamesDSP Orbit Stage31 - USB HiFi 192 kHz HRTF Level Only (Experimental)",
            "name=JamesDSP Orbit v40 - Enhanced Silence Signal Diagnostic (Temporary)",
        ).replace("version=40-192k-hrtf-level-only", "version=41-v40-signal-diagnostic").replace(
            "versionCode=40", "versionCode=41"
        ).replace(
            "description=Experimental 192 kHz Orbit HRTF level correction; no Enhanced mode app or PCM fallback changes.",
            "description=Temporary v40 signal diagnostic; no audio algorithm or Enhanced mode app changes.",
        )
        if "versionCode=41" not in prop:
            raise SystemExit("unexpected v40 module.prop")
        data[PROP_PATH] = prop.encode()
        data[NATIVE_PATH] = native
        data[README_PATH] = note
        data[NOTE_PATH] = note
        data[CHECKS_PATH] = "".join(
            f"{sha256(data[path])}  {path}\n" for path in PAYLOADS
        ).encode()

        output_path.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(output_path, "x") as result:
            for info in infos:
                result.writestr(info, b"" if info.is_dir() else data[info.filename])
            new_info = zipfile.ZipInfo(NOTE_PATH, (2026, 11, 2, 0, 0, 0))
            new_info.compress_type = zipfile.ZIP_DEFLATED
            new_info.external_attr = 0o100644 << 16
            result.writestr(new_info, note)

    with zipfile.ZipFile(output_path) as result:
        if result.testzip() is not None:
            raise SystemExit("diagnostic ZIP integrity check failed")
        if set(result.namelist()) != {i.filename for i in infos} | {NOTE_PATH}:
            raise SystemExit("diagnostic ZIP members differ from v40 plus diagnostic note")
        for path, contents in data.items():
            if result.read(path) != contents:
                raise SystemExit(f"diagnostic ZIP content differs: {path}")
        for path in PAYLOADS:
            actual = sha256(result.read(path))
            print(f"{actual}  {path}")
    digest = sha256(output_path.read_bytes())
    output_path.with_suffix(output_path.suffix + ".sha256").write_text(
        f"{digest}  {output_path.name}\n"
    )
    print(f"ZIP SHA-256: {digest}")


if __name__ == "__main__":
    main()
