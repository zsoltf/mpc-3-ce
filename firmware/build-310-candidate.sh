#!/bin/bash
# Private MPC 3.10 development candidate (v0.4.3), for device trials only.
# Same canonical image patch/verification path as build-prerelease.sh; the
# runtime manifest is the candidate's own, written beside its payload.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo"
host=${1:?Pass the image-location 3.10 Pd/Cardinal host stage (MPCLEARN_FIRMWARE=3.10 compose.sh)}
bundle=${2:?Pass the frozen complete Pd/Cardinal plugin SquashFS}
out=${3:?Pass a new output directory inside this checkout}
source_commit=${4:?Pass the exact committed source SHA of the composed host}
host=$(cd "$host" && pwd)
bundle=$(cd "$(dirname "$bundle")" && pwd)/$(basename "$bundle")
case "$host" in "$repo"/*) ;; *) echo 'Host stage must be inside this checkout' >&2; exit 2;; esac
grep -Fqx '#define MODEL_FIRMWARE_310 1' "$host/controllers/program-observer/package/mirror-input/model-build-profile.h"
python3 -B - "$host/ce-MPC" <<'PY'
import hashlib,json,pathlib,sys
p=pathlib.Path(sys.argv[1]); s=json.loads(pathlib.Path('firmware/mpc-ce-3.10.json').read_text())
assert p.stat().st_size==s['size'] and hashlib.sha256(p.read_bytes()).hexdigest()==s['output_sha256'], 'host stage CE MPC is not the admitted 3.10 executable'
PY
test ! -e "$out"
mkdir -p "$out"
out=$(cd "$out" && pwd)
case "$out" in "$repo"/*) ;; *) echo 'Output must be inside this checkout' >&2; exit 2;; esac
relative=${out#"$repo"/}
python3 - "$bundle" <<'PY'
import hashlib,json,pathlib,sys
p=pathlib.Path(sys.argv[1]); s=json.loads(pathlib.Path('firmware/prerelease-bundle.json').read_text())
assert p.is_file() and not p.is_symlink() and p.stat().st_size==s['bytes']
assert hashlib.file_digest(p.open('rb'),'sha256').hexdigest()==s['sha256']
PY
cp "$host/source-inputs.json" "$out/source-inputs.json"
mkdir "$out/runtime"
cp "$host/controllers/program-observer/package/native-preferences/runtime/"* "$out/runtime/"
(cd firmware/plugin-setup && GOOS=linux GOARCH=arm GOARM=7 CGO_ENABLED=0 \
 go build -buildvcs=false -trimpath -ldflags='-s -w' -o "$out/runtime/plugin-setup" .)
cp firmware/prerelease-bundle.json "$out/runtime/plugin-bundle.json"
(cd "$out/runtime" && for f in command-observer.so command-client mirror-input mirror-read config.h mcu-session.sh mcu mpclearn-controls main-button plugin-setup plugin-bundle.json; do
 printf '%s  %s\n' "$(shasum -a 256 "$f" | cut -d' ' -f1)" "$f"; done) > "$out/candidate-runtime.sha256"
cp "$out/candidate-runtime.sha256" "$out/runtime/session-package.sha256"
python3 firmware/package.py --prerelease --candidate-310 "$out/candidate-runtime.sha256" --source-commit "$source_commit" --source-inputs "$host/source-inputs.json" "$out/runtime" "$out/payload"
mkdir -p "$out/MPCLEARN"
cp "$bundle" "$out/MPCLEARN/plugins.squashfs"
cp firmware/plugin-setup/README.md "$out/README.md"
mkdir "$out/exec"
cp "$host/ce-MPC" "$out/exec/MPC"
image="$relative/MPC-3.10.0-Gen1-CE-v0.4.3-candidate-update.img"
docker run --rm --network none -v "$repo:/work" -w /work \
 -v "$repo/inputs:/inputs:ro" -v "$out/payload:/payload:ro" \
 -e MPC_FIRMWARE_VERSION=3.10 -e MPC_IMAGE_PROFILE=prerelease -e MPC_IMAGE_WORK="$relative/image" \
 -e MPC_CE_EXECUTABLE="$relative/exec/MPC" \
 -e MPC_IMAGE_VERIFICATION="$relative/image-verification.json" \
 -e MPC_IMAGE_OUTPUT="$image" mpclearn-build:local sh firmware/build.sh
docker run --rm --network none --tmpfs /provision-root/run \
 -v "$repo:/work" -w /work mpclearn-build:local \
 python3 firmware/provision-check.py "$relative/image/rootfs.final.ext"
printf 'MPC 3.10 candidate image: %s\nSD-card folder: %s/MPCLEARN\n' "$image" "$out"
