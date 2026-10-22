#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
kit=$PWD
repo=$(cd ../.. && pwd)
host=${1:?Pass the completed hosted-host-build stage}
test -f "$host/controllers/program-observer/package/native-preferences/runtime/command-observer.so"
stage=$(mktemp -d "$repo/build/pd-touch-prototype/hosted-package.XXXXXX")
mkdir -p "$stage/pd-package/"{library,assets,plugins} "$stage/source"
cp -R "$host/controllers/program-observer/package/native-preferences/"{runtime,tools} "$stage/pd-package/"
cp -R "$repo/build/pd-touch-prototype/libpd/pure-data/"{doc,extra} "$stage/pd-package/library/"
cp "$repo/build/pd-touch-prototype/adapter/DejaVuSans.ttf" "$stage/pd-package/assets/"
cp "$repo/build/pd-touch-prototype/libpd/pure-data/font/LICENSE" "$stage/pd-package/assets/DejaVuSans-LICENSE.txt"
cp "$repo/build/pd-touch-prototype/hosted/"*.so "$stage/pd-package/plugins/"
cp "$kit"/* "$stage/source/"
cp "$repo/build/pd-touch-prototype/adapter/stb_truetype.h" "$stage/source/"
cp "$repo/vst2/pd-durm/package/libpd-source.tar.gz" "$stage/source/"
docker run --rm --network none -v "$repo/vst2:/kit:ro" -v "$stage/pd-package:/out" mpclearn-cardinal-full-build bash -ec '
 arm-linux-gnueabihf-gcc -I/kit/pd-rack-prototype -std=c11 -O2 -Wall -Wextra -Werror /kit/pd-touch-prototype/hosted-component.c -o /out/tools/hosted-component -ldl -lm
 for module in /out/plugins/*.so; do
   arm-linux-gnueabihf-strip --strip-unneeded "$module"
   arm-linux-gnueabihf-readelf -dWsV "$module" > "$module.elf.txt"
 done
 du -sk /out/plugins /out/library /out/assets /out/runtime /out/tools
 ' > "$stage/package-build.log" 2>&1
(cd "$stage/pd-package" && find . -type f -exec shasum -a 256 {} + > "$stage/PACKAGE.sha256")
(cd "$stage/source" && find . -type f -exec shasum -a 256 {} + > "$stage/SOURCE.sha256")
echo "PD_HOSTED_PACKAGE=$stage"
