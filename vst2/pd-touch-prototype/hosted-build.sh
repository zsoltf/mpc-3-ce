#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
repo=$(cd ../.. && pwd)
bash build-adapter.sh
mkdir -p "$repo/build/pd-touch-prototype/hosted"
docker run --rm --network none -v "$repo/vst2:/kit:ro" \
 -v "$repo/build/pd-touch-prototype:/work" mpclearn-cardinal-full-build bash -ec '
 flags="-std=c++14 -O2 -fPIC -fvisibility=hidden -fno-strict-aliasing -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -Wall -Wextra -Werror -Wno-misleading-indentation"
 for variant in synth fx; do
   inputs=0; test "$variant" != fx || inputs=2
   arm-linux-gnueabihf-g++ -I/kit/pd-rack-prototype $flags -DPD_HOSTED_INPUTS=$inputs /kit/pd-touch-prototype/hosted-plugin.cc \
     /work/adapter/libpd-touch.a -shared -o /work/hosted/pd-sketch-$variant.so \
     -lm -lpthread -ldl -Wl,--no-undefined,--exclude-libs,ALL,-z,now,--version-script=/kit/pd-touch-prototype/hosted-exports.map
   arm-linux-gnueabihf-readelf -dWs /work/hosted/pd-sketch-$variant.so > /work/hosted/pd-sketch-$variant.elf.txt
 done
 '
