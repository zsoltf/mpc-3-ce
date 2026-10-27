#!/bin/sh
set -eu
cd "$(dirname "$0")"
build_area=$(mktemp -d /tmp/mpclearn-pd-engine.XXXXXX)
trap 'rm -rf "$build_area"' EXIT INT TERM
python3 -B fetch-source.py "$build_area/source"
mkdir -p package
cp "$build_area/source/qubit/Instruments/Durm/durm.pd" package/durm-original.pd
python3 -B adapt-patch.py package/durm-original.pd package/durm.pd
python3 - <<'PY'
from pathlib import Path
import hashlib
p=Path('package');digest=hashlib.sha256((p/'durm.pd').read_bytes()).digest()
(p/'identity.h').write_text('static const unsigned char PATCH_ID[32]={'+','.join(str(n) for n in digest)+'};\n')
print('adapted patch SHA256',digest.hex())
PY
docker build -t mpclearn-pd-engine-build .
docker run --rm --network none --memory 1g --pids-limit 128 -v "$build_area/source:/source" -v "$PWD:/kit:ro" -v "$PWD/package:/out" mpclearn-pd-engine-build sh /kit/build-in-container.sh
