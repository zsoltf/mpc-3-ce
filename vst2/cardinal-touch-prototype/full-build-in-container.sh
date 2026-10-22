#!/bin/bash
set -euo pipefail
cd /work/source
export CC=arm-linux-gnueabihf-gcc CXX=arm-linux-gnueabihf-g++ AR=arm-linux-gnueabihf-ar
export PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig
export CFLAGS='-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard'
export CXXFLAGS="$CFLAGS"
common=(SYSDEPS=true USE_GLES2=true LINUX_EMBED=true)
mkdir -p deps/sysroot/include/ghc deps/sysroot/lib
cp -p dpf/dgl/src/nanovg/nanovg.h deps/sysroot/include/
cp -p src/Rack/dep/nanosvg/src/*.h src/Rack/dep/oui-blendish/*.h \
   src/Rack/dep/osdialog/*.h src/Rack/dep/pffft/*.h \
   src/Rack/dep/fuzzysearchdatabase/src/FuzzySearchDatabase.hpp \
   src/Rack/dep/tinyexpr/tinyexpr.h deps/sysroot/include/
cp -p src/Rack/dep/filesystem/include/ghc/*.hpp deps/sysroot/include/ghc/
if [[ ${1:-all} == rack ]]; then
    exec make -j4 "${common[@]}" -C src rack.a
fi
if [[ ${1:-all} == deps ]]; then
    make -j4 "${common[@]}" deps
    exec cmake --build deps/surge-build --target surge-common -j4
fi
make -j4 "${common[@]}" deps
# The outer dependency target does not track Surge's internal headers.
# Let its generated build check them after source preparation on reruns.
cmake --build deps/surge-build --target surge-common -j4
make -j4 "${common[@]}" carla
make -j4 "${common[@]}" -C plugins plugins.a
make -j4 "${common[@]}" -C src rack.a
make -j4 "${common[@]}" dgl
make -j4 "${common[@]}" -C src/CardinalSynth vst2 VST2_FILENAME=Cardinal.vst/CardinalSynth.so
make -j4 "${common[@]}" -C src/CardinalFX vst2 VST2_FILENAME=Cardinal.vst/CardinalFX.so
make "${common[@]}" resources
