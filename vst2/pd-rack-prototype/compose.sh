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
tar -C "$source" --exclude=./.git --exclude=./build --exclude=./artifacts --exclude=./inputs \
 --exclude='*/package' --exclude=./builder/dist -cf - . | tar -C "$stage" -xf -
cp "$kit/consumer.h" "$stage/ui/examples/toolkit-example.h"
cp "$kit/consumer.cc" "$stage/ui/examples/toolkit-example.cc"
(cd "$stage" && patch -p1 < "$repo/vst2/cardinal-touch-prototype/full-host-build.patch")
python3 "$kit/prepare.py" "$stage" --track
# The shipped CE executable, derived and hash-checked from the stock 3.9.1 MPC
# (firmware/mpc-ce.json pins both); never an older release's CE copy.
python3 -B "$repo/firmware/patch-mpc-ce.py" "$repo/build/MPC" "$stage/ce-MPC"
MPCLEARN_NATIVE_UI_EXAMPLE=0 sh "$stage/controllers/program-observer/native-preferences-build.sh" "$stage/ce-MPC" "$repo/build/MPC" "$location"
echo "PD_RACK_HOST_STAGE=$stage"
