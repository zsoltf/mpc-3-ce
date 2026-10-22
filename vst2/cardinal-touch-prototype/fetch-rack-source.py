#!/usr/bin/env python3
"""Fetch only this finite component's immutable source closure into build scratch."""
import argparse
import hashlib
import tarfile
import urllib.request
from pathlib import Path

# Rack and DPF are the exact gitlinks of Cardinal 0a530b73273afc914ec71a78e9165cd28c53b599.
# The remaining revisions are Rack's gitlinks, not current upstream branches.
SOURCES = (
    ("rack", "VCVRack/Rack", "8c6f41b778b4bf8860b89b36d5503fd37924077f", "59e509468b53a23e73d560a8c5d0e3c2dae7f9505d4aea61e1429e1a214d723d"),
    ("dpf", "DISTRHO/DPF", "461ea6d45d842d273ae4d53ad5490e43459419cc", "b206dc10e34851a34695134713e4d9f00d0532c4db19477789a96cd1966a5d02"),
    ("filesystem", "gulrak/filesystem", "7e37433f318488ae4bc80f80e12df12a01579874", "15e1dacd2a52d7cf67afcc548cc92b218f88a2726488e50887922e86c1493f68"),
    ("nanosvg", "memononen/nanosvg", "25241c5a8f8451d41ab1b02ab2d865b01600d949", "d957259360bf108858388bb01686a8cb0fc31d90db2d996ddf65575b37bb20d4"),
    ("blendish", "VCVRack/oui-blendish", "2fc6405883f8451944ed080547d073c8f9f31898", "f5c5a814b3302ac865ab648ec69f586b67cc0e9d2e51f77bcd4f495e75af6930"),
    ("glfw", "VCVRack/glfw", "d46b57cdb16e521c8cf4290c9022278863a84022", "d8527b380bdd253a742813823758e4fdc5317519e15e3a2f379e27da94187cd2"),
    ("simde", "simd-everywhere/simde", "dd0b662fd8cf4b1617dbbb4d08aa053e512b08e4", "66660dff86095de353ba051312d893fbc9576372eb591388d21549ce6518fc9a"),
    ("tinyexpr", "codeplea/tinyexpr", "4e8cc0067a1e2378faae23eb2dfdd21e9e9907c2", "2d63d882e6b36f808e0ec739ad796fc44aa1595146e1914a4a80ce5670a2d2c0"),
    ("osdialog", "AndrewBelt/osdialog", "fd0beccddb7c6bb10acd04918a4c3b1ad2550057", "d1e9a2cd9c073ac680c56ada84027f495bb70ef2c9a5efb14077209e1bba06c1"),
)

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
for name, repo, revision, expected in SOURCES:
    archive = args.output / (name + ".tar.gz")
    if not archive.exists():
        with urllib.request.urlopen(f"https://codeload.github.com/{repo}/tar.gz/{revision}", timeout=60) as response:
            data = response.read(32 * 1024 * 1024 + 1)
        if len(data) > 32 * 1024 * 1024 or hashlib.sha256(data).hexdigest() != expected:
            raise SystemExit(f"Archive identity mismatch: {name}")
        archive.write_bytes(data)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != expected:
        raise SystemExit(f"Cached archive identity mismatch: {name}")
    target = args.output / name
    marker = target / ".probe-source-sha256"
    if not marker.exists() or marker.read_text().strip() != expected:
        target.mkdir(exist_ok=True)
        with tarfile.open(archive, "r:gz") as source:
            for member in source:
                parts = Path(member.name).parts[1:]
                if not parts:
                    continue
                member.name = str(Path(*parts))
                source.extract(member, target, filter="data")
        marker.write_text(expected + "\n")
    print(name, revision, expected)
