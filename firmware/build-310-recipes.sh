#!/bin/bash
# Browser builder recipes for the MPC 3.10 v0.4.1 image: the candidate payload
# rebuilt with the placeholder key, then SSH, no-SSH and diagnostics byte recipes.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
cd "$repo"
candidate=${1:?Pass a build-310-candidate.sh output directory inside this checkout}
candidate=$(cd "$candidate" && pwd)
case "$candidate" in "$repo"/*) ;; *) echo 'Candidate must be inside this checkout' >&2; exit 2;; esac
rel=${candidate#"$repo"/}
out=$(mktemp -d build/310-recipes.XXXXXX)
mkdir -p "$out/inputs"
cp inputs/MPC-3.10.0-Gen1-update.img "$out/inputs/"
cp inputs/placeholder.pub "$out/inputs/owner.pub"
docker run --rm --network none -v "$repo:/work" -w /work \
 -v "$repo/$out/inputs:/inputs:ro" -v "$candidate/payload:/payload:ro" \
 -e MPC_FIRMWARE_VERSION=3.10 -e MPC_IMAGE_PROFILE=prerelease -e MPC_IMAGE_WORK="$out/image" \
 -e MPC_CE_EXECUTABLE="$rel/exec/MPC" \
 -e MPC_IMAGE_VERIFICATION="$out/image-verification.json" \
 -e MPC_IMAGE_OUTPUT="$out/placeholder-update.img" mpclearn-build:local sh firmware/build.sh
docker run --rm --network none -v "$repo:/work" -w /work -e RECIPE_OUTPUT="$out" mpclearn-build:local sh -ec '
out=$RECIPE_OUTPUT
python3 firmware/make-patch.py $out/image/rootfs.original.ext $out/image/rootfs.final.ext inputs/placeholder.pub $out/patch-ssh
python3 firmware/disable-ssh.py $out/image/rootfs.final.ext $out/rootfs.no-ssh.ext
e2fsck -fn $out/rootfs.no-ssh.ext
python3 firmware/make-patch.py --no-ssh $out/image/rootfs.original.ext $out/rootfs.no-ssh.ext inputs/placeholder.pub $out/patch-no-ssh
'
cp "$out/image/rootfs.original.ext" "$out/rootfs.original.ext"
MPCLEARN_FIRMWARE=3.10 sh firmware/build-diagnostic-root.sh "$repo/$out"
mkdir -p builder/resources/310/ssh builder/resources/310/no-ssh builder/resources/310/diagnostics
cp "$out/patch-ssh/manifest.json" "$out/patch-ssh/patch.bin" builder/resources/310/ssh/
cp "$out/patch-no-ssh/manifest.json" "$out/patch-no-ssh/patch.bin" builder/resources/310/no-ssh/
cp "$out/diagnostics/recipe/manifest.json" "$out/diagnostics/recipe/patch.bin" builder/resources/310/diagnostics/
echo "3.10 builder recipes: builder/resources/310 (source image and recipe inputs: $out)"
