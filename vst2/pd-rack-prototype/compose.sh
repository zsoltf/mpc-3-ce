#!/bin/bash
# Build the release observer with the Pd/Cardinal touch editors from a source
# tree: by default this checkout (or an unpacked source ZIP). It copies the
# tree into a fresh stage, composes the editor consumers, derives the CE MPC
# from the stock build/MPC and runs the Native Preferences build there.
# Needs the docker images mpclearn-controls-build (controllers/Dockerfile) and
# mpclearn-editor-build (this folder's Dockerfile).
set -euo pipefail
cd "$(dirname "$0")"
kit=$PWD
repo=$(cd ../.. && pwd)
location=${1:-/data/mpclearn/dev}
source=${2:-$repo}
[ "$#" -le 2 ] || { echo 'compose.sh [/data/mpclearn/dev|/usr/share/mpclearn/mcu] [SOURCE_TREE]' >&2; exit 2; }
source=$(cd "$source" && pwd)
sh "$repo/controllers/program-observer/device-location.sh" "$location" release
mkdir -p "$repo/build/pd-rack-prototype"
stage=$(mktemp -d "$repo/build/pd-rack-prototype/host.XXXXXX")
# Sources only: no Git data, local inputs, build outputs or packages.
tar -C "$source" --exclude=./.git --exclude=./.claude --exclude=./build --exclude=./artifacts --exclude=./inputs \
 --exclude='*/package' --exclude=./builder/dist -cf - . | tar -C "$stage" -xf -
# Record the exact source inputs before composition rewrites their copies.
# Build only from the staged tree, including when SOURCE_TREE is an export.
python3 "$stage/firmware/candidate-source.py" record "$stage" "$stage/source-inputs.json"
kit="$stage/vst2/pd-rack-prototype"
cp "$kit/consumer.h" "$stage/ui/examples/toolkit-example.h"
cp "$kit/consumer.cc" "$stage/ui/examples/toolkit-example.cc"
(cd "$stage" && patch -p1 < "$stage/vst2/cardinal-touch-prototype/full-host-build.patch")
python3 "$kit/prepare.py" "$stage" --track
# The shipped CE executable, derived and hash-checked from the stock MPC of
# the selected profile (firmware/mpc-ce*.json pin both); never a release copy.
case "${MPCLEARN_FIRMWARE:-3.9.1}" in
 3.9.1) stock="$repo/build/MPC";patch_flag=;;
 3.10) stock="$repo/build/mpc310/MPC";patch_flag=--mpc-3.10;;
 *) echo 'Unsupported explicit firmware profile' >&2;exit 2;;
esac
python3 -B "$stage/firmware/patch-mpc-ce.py" $patch_flag "$stock" "$stage/ce-MPC"
MPCLEARN_NATIVE_UI_EXAMPLE=0 sh "$stage/controllers/program-observer/native-preferences-build.sh" "$stage/ce-MPC" "$stock" "$location"
echo "PD_RACK_HOST_STAGE=$stage"
