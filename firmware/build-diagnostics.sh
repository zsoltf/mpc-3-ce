#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
out=${1:-"$repo/build/usb-diagnostics"}
mkdir -p "$out"
out=$(CDPATH= cd -- "$out" && pwd -P)
profile=${MPCLEARN_FIRMWARE:-3.9.1}
case "$profile" in
 3.9.1) [ "$#" -le 1 ] || { echo 'A root filesystem requires explicit MPCLEARN_FIRMWARE=3.10' >&2; exit 2; } ;;
 3.10)
  base=${2:?The 3.10 collector requires its exact no-SSH root filesystem}
  base=$(cd "$(dirname "$base")" && pwd -P)/$(basename "$base")
  docker run --rm --network none -v "$repo:/src:ro" -v "$out:/out" -v "$base:/base.ext:ro" mpclearn-build:local sh -ec '
   python3 - <<"PY"
import hashlib, importlib.util, shutil, subprocess, tempfile
from pathlib import Path
out = Path("/out")
spec = importlib.util.spec_from_file_location("model310", "/src/controllers/program-observer/model-310.py")
model = importlib.util.module_from_spec(spec)
spec.loader.exec_module(model)
# Extract afresh: debugfs can return success when an image path is absent.
with tempfile.TemporaryDirectory() as td:
    extracted = Path(td)
    for name, path in [("MPC", "/usr/bin/MPC"),
                       ("command-observer.so", "/usr/share/mpclearn/mcu/command-observer.so"),
                       ("payload.sha256", "/usr/share/mpclearn/mcu/payload.sha256")]:
        subprocess.run(["debugfs", "-R", f"dump {path} {extracted / name}", "/base.ext"], check=True)
        if not (extracted / name).is_file():
            raise SystemExit(f"Missing diagnostic admission input: {path}")
    # Reuse the production CE admission and traced layout owner, without hooks.
    model.prepare(extracted / "MPC", out / "unused", mirror_sites=[], command=True)
    for name in ["MPC", "command-observer.so", "payload.sha256"]:
        shutil.copyfile(extracted / name, out / name)
digest = lambda name: hashlib.sha256((out / name).read_bytes()).hexdigest()
observer = digest("command-observer.so")
manifest = digest("payload.sha256")
manifest_lines = (out / "payload.sha256").read_text().splitlines()
assert f"{observer}  command-observer.so" in manifest_lines
(out / "diagnostics-profile.h").write_text(
    "#define MPCLEARN_DIAGNOSTIC_PROFILE 1\n"
    "#include \"model-310-identity.h\"\n"
    "#include \"model-310-layout.h\"\n"
    "#define CURSOR_SLOT_RVA MODEL_POINTER_STATE\n"
    f"#define EXPECTED_PAYLOAD_FILES {len(manifest_lines)}u\n"
    f"#define MPCLEARN_OBSERVER_SHA256 \"{observer}\"\n"
    f"#define MPCLEARN_PAYLOAD_MANIFEST_SHA256 \"{manifest}\"\n")
print("PASS: diagnostic admission derived from exact CE ELF and packaged observer/manifest")
PY
  '
  ;;
 *) echo "Unsupported diagnostic firmware: $profile" >&2; exit 2 ;;
esac
docker run --rm --network none -v "$repo:/src:ro" -v "$out:/out" mpclearn-controls-build sh -ec '
if [ "$1" = 3.10 ]; then set -- -include /out/diagnostics-profile.h; else set --; fi
arm-linux-gnueabihf-gcc -std=c11 -O2 -Wall -Wextra -Werror -marm "$@" /src/firmware/usb-diagnostics.c -o /out/usb-diagnostics
file /out/usb-diagnostics
arm-linux-gnueabihf-readelf -l /out/usb-diagnostics | sed -n "/interpreter/p"
arm-linux-gnueabihf-readelf -d /out/usb-diagnostics | sed -n "/NEEDED\|RPATH\|RUNPATH/p"
arm-linux-gnueabihf-readelf -A /out/usb-diagnostics
' sh "$profile"
sha256sum "$out/usb-diagnostics" >"$out/usb-diagnostics.sha256"
