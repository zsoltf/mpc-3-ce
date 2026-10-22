#!/usr/bin/env python3
"""Copy the full plugin's missing-library closure, keeping the native GPU/ABI."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("bundle", type=Path)
args = parser.parse_args()
native = {
    "libc.so.6", "libm.so.6", "libdl.so.2", "libpthread.so.0", "librt.so.1",
    "libstdc++.so.6", "libgcc_s.so.1", "ld-linux-armhf.so.3",
    "libEGL.so.1", "libGLESv2.so.2", "libgbm.so.1", "libarchive.so.13",
}
roots = [Path("/usr/lib/arm-linux-gnueabihf"), Path("/lib/arm-linux-gnueabihf"),
         Path("/usr/arm-linux-gnueabihf/lib")]
dest = args.bundle / "deps"
dest.mkdir()
queue = list(args.bundle.glob("*.so"))
copied = set()
report = []
while queue:
    binary = queue.pop(0)
    text = subprocess.check_output(["arm-linux-gnueabihf-readelf", "-d", binary], text=True)
    if binary.parent == args.bundle:
        if "[libGL.so.1]" in text or "[libGLESv2.so.2]" not in text:
            raise SystemExit("Native GLES drawing dependency is not explicit: " + str(binary))
    for needed in re.findall(r"\(NEEDED\).*\[(.*?)\]", text):
        report.append(f"{binary.name}: {needed} ({'native' if needed in native else 'bundled'})")
        if needed in native or needed in copied:
            continue
        source = next((root / needed for root in roots if (root / needed).is_file()), None)
        if source is None:
            raise SystemExit("Missing ARM runtime dependency: " + needed)
        target = dest / needed
        shutil.copyfile(source, target)
        copied.add(needed)
        queue.append(target)
# DT_RPATH is inherited for dependency-of-dependency lookup. Do not change the
# MPC process environment, its loader, or native GPU/standard C++ libraries.
for binary in args.bundle.glob("*.so"):
    subprocess.run(["patchelf", "--force-rpath", "--set-rpath", "$ORIGIN/deps", binary], check=True)
(args.bundle / "DEPENDENCIES.txt").write_text("\n".join(report) + "\n")
print(f"Packaged {len(copied)} scoped ARM libraries; native ABI/GPU dependencies remain explicit")
