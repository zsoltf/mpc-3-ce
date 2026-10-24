#!/bin/sh
# Container entrypoint. Inputs mounted read-only at /inputs and /payload.
set -eu
cd /work
output=${MPC_IMAGE_OUTPUT:-artifacts/MPC-3.9.1-Gen1-CE-v0.2.6-PLACEHOLDER-update.img}
ce_mpc=${MPC_CE_EXECUTABLE:-build/v0_2_6/MPC}
firmware_version=${MPC_FIRMWARE_VERSION:-3.9.1}
case "$firmware_version" in
 3.9.1) stock_image=/inputs/MPC-3.9.1-Gen1-update.img; firmware_option=;;
 3.10) stock_image=/inputs/MPC-3.10.0-Gen1-update.img; firmware_option=--mpc-3.10
   : "${MPC_CE_EXECUTABLE:?3.10 requires its explicit composed CE executable}"
   : "${MPC_IMAGE_OUTPUT:?3.10 requires its explicit output image path}";;
 *) echo 'Unsupported firmware version' >&2; exit 2;;
esac
work=${MPC_IMAGE_WORK:-build}
verification=${MPC_IMAGE_VERIFICATION:-artifacts/image-verification.json}
profile=${MPC_IMAGE_PROFILE:-release}
case "$profile" in release) verify_option=;; prerelease) verify_option=--prerelease;; *) exit 2;; esac
[ ! -e "$output" ] || { echo "Output already exists: $output" >&2; exit 1; }
mkdir -p "$work" "$(dirname "$output")" "$(dirname "$verification")"
if [ "$firmware_version" = 3.10 ]; then
 python3 scripts/image_format.py "$stock_image" "$work/rootfs.original.ext" --mpc-3.10 > "$work/stock-image.json"
else
 python3 - "$work/rootfs.original.ext" <<'PY_STOCK'
import sys
from scripts.image_format import decode, OFFICIAL_SHA
assert decode('/inputs/MPC-3.9.1-Gen1-update.img',sys.argv[1])['image_sha256']==OFFICIAL_SHA
PY_STOCK
fi
ssh-keygen -l -f /inputs/owner.pub > "$work/owner-key-fingerprint.txt"
python3 scripts/patch_rootfs.py "$work/rootfs.original.ext" "$work/rootfs.ssh.ext" /inputs/owner.pub
cp "$work/rootfs.ssh.ext" "$work/rootfs.custom.ext"
python3 firmware/patch.py "$work/rootfs.custom.ext" /payload "$work/mcu-files.json"
python3 firmware/patch-mpc-root.py "$work/rootfs.custom.ext" "$ce_mpc" "$work/mpc-ce-files.json" $firmware_option
e2fsck -fn "$work/rootfs.custom.ext"
gcc -O2 -Wno-deprecated-declarations -o "$work/mpcimg2" upstream/mpcimg2.c -llzma -lcrypto
gcc -O2 -Wno-deprecated-declarations -o "$work/fs_inventory" scripts/fs_inventory.c -lext2fs -lcom_err -lcrypto
python3 - <<'PY'
import hashlib,pathlib
assert pathlib.Path('upstream/COMMIT').read_text().strip()=='9be3e63bf03d3467cf06478b48671894dcc634f4'
assert hashlib.sha256(pathlib.Path('upstream/mpcimg2.c').read_bytes()).hexdigest()=='e28fd9a2c1e4c760483e06dea31450f9d3d1e90cfb0c3604f3a1e66829853acc'
PY
"$work/mpcimg2" -m "$stock_image" "$work/rootfs.custom.ext" "$output" > "$work/repack.log"
python3 scripts/image_format.py "$output" "$work/rootfs.final.ext" "$stock_image" $firmware_option > "$verification"
cmp "$work/rootfs.custom.ext" "$work/rootfs.final.ext"
e2fsck -fn "$work/rootfs.final.ext"
"$work/fs_inventory" "$work/rootfs.original.ext" > "$work/original.inventory"
"$work/fs_inventory" "$work/rootfs.ssh.ext" > "$work/ssh.inventory"
"$work/fs_inventory" "$work/rootfs.final.ext" > "$work/mcu.inventory"
python3 scripts/verify_delta.py "$work/original.inventory" "$work/ssh.inventory" > "$work/ssh-delta.txt"
python3 firmware/verify.py $verify_option $firmware_option "$work/ssh.inventory" "$work/mcu.inventory" "$work/mcu-files.json" "$work/rootfs.final.ext" "$work/mpc-ce-files.json"
sha256sum "$output" > "$output.sha256"
