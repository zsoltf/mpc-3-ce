#!/bin/sh
# Requires the ordinary exact 3.10 command profile; diagnostic capture is optional.
set -eu
cd "$(dirname "$0")"
[ "$#" -eq 1 ] || { echo 'midi-mute-check.sh /absolute/new-scratch-directory' >&2; exit 2; }
case "$1" in /*) ;; *) exit 2;; esac
[ "$(sed -n 's/^#define MODEL_MIDI_MUTE //p' package/command/model-build-profile.h)" = 1 ] || { echo 'prepare the exact MIDI mute command profile first' >&2; exit 2; }
mkdir "$1"
docker run --rm --network none -v "$PWD:/src:ro" -v "$1:/out" mpclearn-controls-build sh -ec '
flags="-include /src/package/command/model-build-profile.h -I/src/package/command -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon"
arm-linux-gnueabihf-gcc $flags -DVOLUME_MIRROR -DMIRROR_COMMAND -DCOMMAND_COMPONENT -no-pie -Wl,-Ttext-segment=0x10000000 /src/midi-mute-component.c /src/component.S /src/gate.S /src/transport-queue.c -o /out/midi-mute-component -pthread -ldl -lm
arm-linux-gnueabihf-gcc $flags /src/midi-mute-accounting-test.c -o /out/midi-mute-accounting-test
arm-linux-gnueabihf-gcc $flags -DVOLUME_MIRROR -DMIRROR_COMMAND -fPIC -fvisibility=hidden -shared /src/observer.c /src/command-capture.c /src/patch.c /src/gate.S /src/transport-queue.c -o /out/command-observer.so -pthread -ldl -Wl,-z,now
arm-linux-gnueabihf-gcc $flags /src/mirror-read.c -o /out/mirror-read -lm
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /out/midi-mute-accounting-test
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /out/midi-mute-component /out/mirror.state
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /out/mirror-read /out/mirror.state > /out/mirror.json || [ "$?" -eq 1 ]
'
python3 - "$1/mirror.json" <<'PYTEST'
import json,sys
j=json.load(open(sys.argv[1]));m=j['midi_mute']
assert j['format']=='MMV24' and not m['snapshot_atomic']
assert m['capacity']==8192 and m['high_water']==8192 and m['slot_bytes']==328
assert m['error']==1 and m['unaccounted']>=2 and m['max_lookup']<=64
assert not m['active'] and not m['pending'] and m['ordering_lost']==2
print('PASS actual ARM mirror-read exposes native accounting fault, bounds and independent counters from substituted adapter state; physical output unobserved')
PYTEST
