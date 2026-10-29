#!/usr/bin/env python3
"""Create an exact CE executable with its admitted label and Az01 EHCI CPU policy."""
import argparse
import hashlib
import json
import os
import pathlib

SPEC = json.loads((pathlib.Path(__file__).with_name("mpc-ce.json")).read_text())


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=pathlib.Path)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--mpc-3.10', dest='mpc_310', action='store_true',
                        help='explicit MPC 3.10 / CE 0.4.3 data slots; does not enable runtime admission')
    args = parser.parse_args()
    source, output = args.source, args.output
    spec = json.loads(pathlib.Path(__file__).with_name('mpc-ce-3.10.json').read_text()) if args.mpc_310 else SPEC
    if source.resolve() == output.resolve():
        raise SystemExit("output must not replace the pristine MPC executable")
    raw = source.read_bytes()
    if len(raw) != spec['size'] or digest(raw) != spec['input_sha256']:
        raise SystemExit("wrong pristine MPC executable")
    patched = bytearray(raw)
    allowed = []
    end = 0
    for slot in spec['slots']:
        offset = slot["offset"]
        before = bytes.fromhex(slot["before_hex"])
        after = bytes.fromhex(slot["after_hex"])
        if not before or len(before) != len(after) or not end <= offset <= len(raw) - len(before):
            raise SystemExit("invalid or overlapping MPC slot: " + slot["name"])
        if raw[offset:offset + len(before)] != before:
            raise SystemExit("MPC slot changed: " + slot["name"])
        if slot["unique"] and raw.count(before) != 1:
            raise SystemExit("MPC slot is not unique: " + slot["name"])
        patched[offset:offset + len(after)] = after
        allowed.extend(offset + i for i, (a, b) in enumerate(zip(before, after)) if a != b)
        end = offset + len(before)
    differences = [i for i, (a, b) in enumerate(zip(raw, patched)) if a != b]
    if differences != allowed or len(patched) != len(raw):
        raise SystemExit("unexpected executable delta")
    if digest(patched) != spec['output_sha256']:
        raise SystemExit("patched executable identity changed")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(output.name + ".next")
    temporary.write_bytes(patched)
    os.chmod(temporary, source.stat().st_mode & 0o777)
    temporary.replace(output)
    print(f"PASS: {len(differences)} bytes changed only in the {len(spec['slots'])} admitted MPC slots")
    print(digest(patched))


if __name__ == "__main__":
    main()
