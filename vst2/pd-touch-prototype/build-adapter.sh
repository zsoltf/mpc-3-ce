#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
repo=$(cd ../.. && pwd)
mkdir -p "$repo/build/pd-touch-prototype/libpd" "$repo/build/pd-touch-prototype/adapter"
tar -xzf ../pd-durm/package/libpd-source.tar.gz -C "$repo/build/pd-touch-prototype/libpd"
cp "$repo/vst2/pd-rack-prototype/editor-view.h" "$repo/build/pd-touch-prototype/adapter/"
cp "$repo/build/cardinal-touch-prototype/sources/dpf/dgl/src/nanovg/stb_truetype.h" "$repo/build/pd-touch-prototype/adapter/"
cp "$repo/build/pd-touch-prototype/libpd/pure-data/font/DejaVuSansMono.ttf" "$repo/build/pd-touch-prototype/adapter/DejaVuSans.ttf"
docker run --rm --network none -v "$PWD:/kit:ro" -v "$repo/build/pd-touch-prototype:/work" \
 -v "$repo/vst2/pd-durm/package/armhf/libpd.a:/libpd.a:ro" mpclearn-cardinal-rack-probe-build sh -ec '
 flags="-DPDINSTANCE -DPDTHREADS -DLIBPD_EMBED_NO_SIGNALS -fvisibility=hidden -O2 -fno-strict-aliasing -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard -Wall -Wextra -Werror -Wno-misleading-indentation -I/work/libpd/libpd_wrapper -I/work/libpd/pure-data/src -I/work/adapter -I/kit"
 arm-linux-gnueabihf-gcc -std=c11 -fPIC $flags -c /kit/pd-touch.c -o /work/adapter/pd-touch.o
 cp /libpd.a /work/adapter/libpd-touch.a
 arm-linux-gnueabihf-ar rcs /work/adapter/libpd-touch.a /work/adapter/pd-touch.o
 arm-linux-gnueabihf-gcc -std=c11 -fPIC $flags -c /kit/patch-files.c -o /work/adapter/patch-files.o
 arm-linux-gnueabihf-g++ -std=c++11 -fPIC $flags -Wno-unused-function -c /kit/bitmap-font.cc -o /work/adapter/bitmap-font.o
 arm-linux-gnueabihf-ar rcs /work/adapter/libpd-touch.a /work/adapter/patch-files.o /work/adapter/bitmap-font.o
 mkdir -p /tmp/mpclearn-pd-touch/assets
 cp /work/adapter/DejaVuSans.ttf /tmp/mpclearn-pd-touch/assets/
 arm-linux-gnueabihf-gcc -std=c11 $flags /kit/component.c /work/adapter/libpd-touch.a -o /work/adapter/component -lm -lpthread -ldl
 /lib/ld-linux-armhf.so.3 /work/adapter/component
 arm-linux-gnueabihf-g++ -std=c++11 -fPIC $flags -c /kit/pd-canvas.cc -o /work/adapter/pd-canvas.o
 arm-linux-gnueabihf-ar rcs /work/adapter/libpd-touch.a /work/adapter/pd-canvas.o
 arm-linux-gnueabihf-g++ -std=c++11 $flags /kit/canvas-component.cc /work/adapter/libpd-touch.a -o /work/adapter/canvas-component -lm -lpthread -ldl
 /lib/ld-linux-armhf.so.3 /work/adapter/canvas-component
 mkdir -p /tmp/mpclearn-pd-touch/library
 cp -R /work/libpd/pure-data/doc /work/libpd/pure-data/extra /tmp/mpclearn-pd-touch/library/
 arm-linux-gnueabihf-g++ -std=c++11 $flags /kit/real-patch-component.cc /work/adapter/libpd-touch.a -o /work/adapter/real-patch-component -lm -lpthread -ldl
 /lib/ld-linux-armhf.so.3 /work/adapter/real-patch-component
 arm-linux-gnueabihf-g++ -std=c++11 $flags /kit/controls-component.cc /work/adapter/libpd-touch.a -o /work/adapter/controls-component -lm -lpthread -ldl
 /lib/ld-linux-armhf.so.3 /work/adapter/controls-component
 arm-linux-gnueabihf-g++ -std=c++11 $flags /kit/numbers-component.cc /work/adapter/libpd-touch.a -o /work/adapter/numbers-component -lm -lpthread -ldl
 /lib/ld-linux-armhf.so.3 /work/adapter/numbers-component
 arm-linux-gnueabihf-g++ -std=c++11 $flags /kit/text-component.cc /work/adapter/libpd-touch.a -o /work/adapter/text-component -lm -lpthread -ldl
 /lib/ld-linux-armhf.so.3 /work/adapter/text-component
 cp /tmp/mpclearn-pd-touch/state/prototype.pd /work/adapter/text-created.pd
 arm-linux-gnueabihf-g++ -std=c++11 $flags /kit/widgets-component.cc /work/adapter/libpd-touch.a -o /work/adapter/widgets-component -lm -lpthread -ldl
 /lib/ld-linux-armhf.so.3 /work/adapter/widgets-component
 arm-linux-gnueabihf-g++ -std=c++11 $flags /kit/arrays-component.cc /work/adapter/libpd-touch.a -o /work/adapter/arrays-component -lm -lpthread -ldl
 /lib/ld-linux-armhf.so.3 /work/adapter/arrays-component
 mkdir -p /work/adapter/state
 cp /tmp/mpclearn-pd-touch/state/prototype.pd /work/adapter/state/
 '
python3 check-arrays.py "$repo/build/pd-touch-prototype/libpd/pure-data/doc/3.audio.examples" "$repo/build/pd-touch-prototype/adapter"
