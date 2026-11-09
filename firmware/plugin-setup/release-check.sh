#!/bin/sh
# Real-kernel check of `plugin-setup release`: the admitted bundle is mounted
# through a loop device from a card filesystem, as on the MPC; release must
# leave the card unheld (view unmounted, loop detached, receipt gone, card
# unmounted through edisksctl), also when the card has died under the view.
# The card is an ext4 image on a loop device; edisksctl is a substitute that
# unmounts like edisksd. Kills the card by truncating its image, so reads under
# the view fail as after a USB removal. Needs a privileged container:
#
#   GOOS=linux GOARCH=arm GOARM=7 CGO_ENABLED=0 go build -o /tmp/ps/plugin-setup ./firmware/plugin-setup
#   docker run --rm --privileged --network none -v /tmp/ps:/ps:ro \
#     -v "$PWD/firmware/plugin-setup/release-check.sh:/release-check.sh:ro" \
#     -v BUNDLE:/bundle.squashfs:ro mpclearn-build:local sh /release-check.sh /ps/plugin-setup /bundle.squashfs
set -eu
exe=$1 bundle=$2
[ -f /.dockerenv ] && [ "$(id -u)" = 0 ]
work=$(mktemp -d)
cp "$exe" "$work/plugin-setup"; chmod 700 "$work/plugin-setup"
mkdir -p "$work/bin" /run/mpclearn /media
printf '%s\n' '#!/bin/sh' '[ "$1" = unmount ] || exit 2' 'echo "edisksctl unmount $2" >>'"$work"'/edisksctl.log' \
 'awk -v d="$2" '\''$1==d{print $2}'\'' /proc/mounts | sed "s/\\\\040/ /g" | while IFS= read -r m;do umount "$m";done' >"$work/bin/edisksctl"
chmod 700 "$work/bin/edisksctl"
export PATH="$work/bin:$PATH"
card_path='/media/MPC Card'
setup(){
 truncate -s 0 "$work/card.img";truncate -s 220M "$work/card.img";mkfs.ext4 -q -F "$work/card.img"
 card=$(losetup -f --show "$work/card.img")
 mkdir -p "$card_path" /run/mpclearn/plugins
 mount "$card" "$card_path";mkdir "$card_path/MPCLEARN";cp "$bundle" "$card_path/MPCLEARN/plugins.squashfs"
 mount -t squashfs -o loop,ro,nosuid,nodev "$card_path/MPCLEARN/plugins.squashfs" /run/mpclearn/plugins
 python3 - "$card_path" <<'PY' >/run/mpclearn/plugin-mount.json
import json,sys
for line in open('/proc/self/mountinfo'):
    left,right=line.split(' - ');l=left.split();r=right.split()
    m={'ID':l[0],'Device':l[2],'Source':r[1],'Path':l[4].replace('\\040',' '),'Type':r[0],'Options':l[5]}
    if m['Path']==sys.argv[1]:card=m
    if m['Path']=='/run/mpclearn/plugins':view=m
print(json.dumps({'Version':1,'SHA256':'0'*64,'Card':card,'Bundle':sys.argv[1]+'/MPCLEARN/plugins.squashfs','File':{},'View':view}))
PY
 ls /run/mpclearn/plugins >/dev/null
}
bundle_loops(){ for f in /sys/block/loop*/loop/backing_file;do case "$(cat "$f" 2>/dev/null)" in *MPCLEARN/plugins.squashfs*) echo "$f";; esac;done; }
released(){
 ! grep -q ' /run/mpclearn/plugins ' /proc/self/mountinfo || { echo 'view still mounted';return 1; }
 [ -z "$(bundle_loops)" ] || { echo "loop still attached: $(bundle_loops)";return 1; }
 [ ! -e /run/mpclearn/plugin-mount.json ] || { echo 'receipt kept';return 1; }
 ! grep -q " $(echo "$card_path" | sed 's/ /\\\\040/') " /proc/self/mountinfo || { echo 'card still mounted';return 1; }
}
cleanup(){ umount -l /run/mpclearn/plugins 2>/dev/null || :;umount -l "$card_path" 2>/dev/null || :;for l in $(losetup -j "$work/card.img" | cut -d: -f1);do losetup -d "$l" 2>/dev/null || :;done; }
trap cleanup EXIT

setup
out=$("$work/plugin-setup" release);echo "$out" | grep -qx 'Pd/Cardinal SD bundle released'
released;losetup -d "$card";grep -q 'edisksctl unmount /dev/loop' "$work/edisksctl.log"
echo 'PASS live card: view unmounted, loop detached, receipt removed, card unmounted through edisksctl'

setup
# The card dies under the view: its image is emptied, caches dropped, and the
# view now fails to read, as after MPC switched the card reader off USB.
truncate -s 0 "$work/card.img";sync;echo 3 >/proc/sys/vm/drop_caches
if find /run/mpclearn/plugins -type f -exec cat {} + >/dev/null 2>"$work/read.err";then echo 'dead view still readable';exit 1;fi
grep -q -i -E 'error|invalid' "$work/read.err"
out=$("$work/plugin-setup" release);echo "$out" | grep -qx 'Pd/Cardinal SD bundle released'
released;losetup -d "$card" 2>/dev/null || :
echo 'PASS dead card: reads under the view failed; release still unmounted the view, detached its loop, removed the receipt and unmounted the card'

out=$("$work/plugin-setup" release);echo "$out" | grep -qx 'Pd/Cardinal SD bundle released'
echo 'PASS release with nothing admitted is a no-op'
