#!/usr/bin/env python3
"""Apply the admitted CE label and Az01 EHCI CPU deltas to /usr/bin/MPC in an ext root image."""
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile

repo = pathlib.Path(__file__).resolve().parents[1]
rootfs, executable, expectation = map(pathlib.Path, sys.argv[1:])
spec = json.loads((repo / "firmware/mpc-ce.json").read_text())
slots = spec["slots"]


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


if rootfs.is_symlink() or not rootfs.is_file():
    raise SystemExit("root filesystem must be a regular image")
if executable.is_symlink() or not executable.is_file():
    raise SystemExit("CE MPC must be a regular file")
if executable.stat().st_size != spec["size"] or digest(executable) != spec["output_sha256"]:
    raise SystemExit("CE MPC does not match the exact admitted output")
candidate = executable.read_bytes()
for slot in slots:
    after = bytes.fromhex(slot["after_hex"])
    if candidate[slot["offset"]:slot["offset"] + len(after)] != after:
        raise SystemExit("CE MPC does not contain admitted bytes: " + slot["name"])

with tempfile.TemporaryDirectory() as td:
    temp = pathlib.Path(td)
    stock = temp / "MPC.stock"
    final = temp / "MPC.final"
    run("debugfs", "-R", f"dump /usr/bin/MPC {stock}", str(rootfs))
    if stock.stat().st_size != spec["size"] or digest(stock) != spec["input_sha256"]:
        raise SystemExit("root image does not contain the exact pristine MPC")
    original = stock.read_bytes()
    allowed = []
    end = 0
    for slot in slots:
        offset = slot["offset"]
        before = bytes.fromhex(slot["before_hex"])
        after = bytes.fromhex(slot["after_hex"])
        if not before or len(before) != len(after) or not end <= offset <= len(original) - len(before):
            raise SystemExit("invalid or overlapping MPC slot: " + slot["name"])
        if original[offset:offset + len(before)] != before:
            raise SystemExit("root MPC slot changed: " + slot["name"])
        if slot["unique"] and original.count(before) != 1:
            raise SystemExit("root MPC slot is not unique: " + slot["name"])
        allowed.extend(offset + i for i, (a, b) in enumerate(zip(before, after)) if a != b)
        end = offset + len(before)
    differences = [i for i, (a, b) in enumerate(zip(original, candidate)) if a != b]
    if differences != allowed:
        raise SystemExit("CE MPC has changes outside the admitted slots")

    stat_before = run("debugfs", "-R", "stat /usr/bin/MPC", str(rootfs))
    header = run("dumpe2fs", "-h", str(rootfs))
    match = re.search(r"^Block size:\s+(\d+)$", header, re.MULTILINE)
    if not match:
        raise SystemExit("cannot determine root filesystem block size")
    block_size = int(match.group(1))
    mapped_slots = []
    for slot in slots:
        offset = slot["offset"]
        before = bytes.fromhex(slot["before_hex"])
        after = bytes.fromhex(slot["after_hex"])
        logical, within = divmod(offset, block_size)
        if within + len(after) > block_size:
            raise SystemExit("admitted MPC slot unexpectedly crosses a filesystem block")
        mapped = run("debugfs", "-R", f"bmap /usr/bin/MPC {logical}", str(rootfs)).strip()
        if not mapped.isdigit() or int(mapped) <= 0:
            raise SystemExit("cannot map admitted MPC data block")
        mapped_slots.append((int(mapped) * block_size + within, before, after))

    applied = []
    try:
        with rootfs.open("r+b", buffering=0) as image:
            # Validate every mapped interval before the first write.
            for image_offset, before, after in mapped_slots:
                image.seek(image_offset)
                if image.read(len(before)) != before:
                    raise SystemExit("mapped root image bytes do not match the admitted MPC slot")
            for image_offset, before, after in mapped_slots:
                applied.append((image_offset, before))
                image.seek(image_offset)
                if image.write(after) != len(after):
                    raise RuntimeError("short root image write")
            image.flush()
            os.fsync(image.fileno())
        run("debugfs", "-R", f"dump /usr/bin/MPC {final}", str(rootfs))
        stat_after = run("debugfs", "-R", "stat /usr/bin/MPC", str(rootfs))
        if digest(final) != spec["output_sha256"] or final.read_bytes() != candidate:
            raise RuntimeError("root MPC differs from the exact admitted CE executable")
        if stat_after != stat_before:
            raise RuntimeError("root MPC inode metadata changed")
    except BaseException:
        if applied:
            with rootfs.open("r+b", buffering=0) as image:
                for image_offset, before in reversed(applied):
                    image.seek(image_offset)
                    if image.write(before) != len(before):
                        raise RuntimeError("short root image rollback write")
                image.flush()
                os.fsync(image.fileno())
        raise

result = {
    "version": 2,
    "path": "/usr/bin/MPC",
    "size": spec["size"],
    "input_sha256": spec["input_sha256"],
    "output_sha256": spec["output_sha256"],
    "slots": slots,
    "changed_offsets": allowed,
}
expectation.write_text(json.dumps(result, indent=2) + "\n")
print(f"PASS: /usr/bin/MPC is the exact CE executable; {len(allowed)} admitted bytes changed in two slots and inode metadata is unchanged")
