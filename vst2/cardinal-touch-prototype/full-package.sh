#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
kit=$PWD
repo=$(cd ../.. && pwd)
host=${1:?Pass the completed full-host-build stage path}
work=${2:-$repo/build/cardinal-full-prototype}
test -f "$host/controllers/program-observer/package/native-preferences/runtime/command-observer.so"
stage=$(mktemp -d "$repo/build/cardinal-full-prototype/package.XXXXXX")
mkdir "$stage/cardinal-package" "$stage/source-adapter"
cp -R "$host/controllers/program-observer/package/native-preferences/"{runtime,tools} "$stage/cardinal-package/"
cp "$kit"/full-* "$kit"/prepare-full* "$kit"/prepare-example-patches.py "$kit"/fetch-full-source.py "$kit"/touch-gpu.hpp "$kit"/overlay-api.h "$kit"/overlay-gpu.hpp "$kit"/Dockerfile.full "$kit"/Dockerfile.resources "$stage/source-adapter/"
cp "$kit/FULL-NATIVE.md" "$kit/LIFETIME.md" "$stage/"
cp "$work/source-manifest.json" "$stage/"
docker run --rm --network none \
 -v "$work:/work" -v "$repo/vst2:/kit:ro" \
 -v "$stage/cardinal-package:/out" mpclearn-cardinal-full-build bash -ec '
 test -f /work/source/bin/Cardinal.vst/CardinalSynth.so
 test -f /work/source/bin/Cardinal.vst/CardinalFX.so
 cp -RL /work/source/bin/Cardinal.vst /out/
 test -f /out/Cardinal.vst/resources/patches/templates/synth.vcv
 python3 /kit/cardinal-touch-prototype/prepare-example-patches.py /out/Cardinal.vst/resources
 arm-linux-gnueabihf-strip --strip-unneeded /out/Cardinal.vst/CardinalSynth.so /out/Cardinal.vst/CardinalFX.so
 python3 /kit/cardinal-touch-prototype/full-package-deps.py /out/Cardinal.vst
 arm-linux-gnueabihf-gcc -I/kit/pd-rack-prototype -std=c11 -O2 -Wall -Wextra -Werror /kit/cardinal-touch-prototype/full-component.c -o /out/tools/full-component -ldl
 for module in /out/Cardinal.vst/*.so; do
   arm-linux-gnueabihf-readelf -dWsV "$module" > "$module.elf.txt"
   /lib/ld-linux-armhf.so.3 /out/tools/full-component "$module" --load-only
 done
 for path in /out/Cardinal.vst /out/Cardinal.vst/resources /out/Cardinal.vst/deps /out/runtime /out/tools; do
   du -sk "$path"
 done
 ' > "$stage/package-build.log" 2>&1
cp "$work/source/LICENSE" "$stage/cardinal-package/Cardinal-LICENSE"
(cd "$stage/cardinal-package" && find . -type f -exec shasum -a 256 {} + > "$stage/PACKAGE.sha256")
(cd "$stage/source-adapter" && find . -type f -exec shasum -a 256 {} + > "$stage/SOURCE-ADAPTER.sha256")
echo "CARDINAL_FULL_PACKAGE=$stage"
