#!/bin/sh
set -eu
cd /source/libpd
patch --fuzz=0 -p1 < /kit/libpd-embed.patch
cp -R /source/pd pure-data
mkdir -p /out/notices
cp /kit/notices/* /out/notices/
cp /kit/LICENSE /out/LICENSE
# Retain the patched Pd/libpd source before any compiler output is generated.
tar --sort=name --mtime=@0 --owner=0 --group=0 --numeric-owner -czf /out/libpd-source.tar.gz .
flags='-fvisibility=hidden -DLIBPD_EMBED_NO_SIGNALS -DPDINSTANCE -DPDTHREADS'
for target in local armhf; do
 if [ "$target" = armhf ]; then
  cc=arm-linux-gnueabihf-gcc
  arch='-march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard'
 else
  cc=gcc
  arch=''
 fi
 mkdir -p /out/$target
 make clean UTIL=false EXTRA=false >/dev/null
 if ! make -j4 libs/libpd.a CC="$cc" MULTI=true UTIL=false EXTRA=false SETLOCALE=false OPT_CFLAGS='-O2 -fno-strict-aliasing' ADDITIONAL_CFLAGS="$flags $arch" >/tmp/pd-durm-build.log 2>&1; then
  cat /tmp/pd-durm-build.log
  exit 1
 fi
 "$cc" -std=c11 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation -fPIC $flags $arch -Ilibpd_wrapper -Ipure-data/src -I/out -I/kit -shared /kit/plugin.c /kit/engine.c libs/libpd.a -o /out/$target/PdDurm.so -Wl,--version-script=/kit/exports.map -Wl,-Bsymbolic -lm -lpthread -ldl
 cp /out/durm.pd /out/$target/durm.pd
 if [ -f /kit/component-host.c ]; then
 "$cc" -std=c11 -O2 -Wall -Wextra -Werror -Wno-misleading-indentation $flags $arch -Ilibpd_wrapper -Ipure-data/src -I/out -I/kit /kit/component-host.c /kit/engine.c libs/libpd.a -o /out/$target/component-host -lm -lpthread -ldl
 fi
 cp libs/libpd.a /out/$target/libpd.a
 done
arm-linux-gnueabihf-readelf -A /out/armhf/PdDurm.so
arm-linux-gnueabihf-nm -D --defined-only /out/armhf/PdDurm.so
arm-linux-gnueabihf-readelf -d /out/armhf/PdDurm.so | sed -n '/NEEDED\|RPATH\|RUNPATH/p'
