#!/bin/sh
set -eu
cd "$(dirname "$0")"
docker run --rm --network none --memory 512m --pids-limit 64 -v "$PWD/package:/out:ro" mpclearn-pd-engine-build /out/local/component-host /out/local/PdDurm.so /out/local
docker run --rm --network none --memory 512m --pids-limit 64 -v "$PWD/package:/out:ro" mpclearn-pd-engine-build qemu-arm -L /usr/arm-linux-gnueabihf /out/armhf/component-host /out/armhf/PdDurm.so /out/armhf
