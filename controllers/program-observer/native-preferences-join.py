#!/usr/bin/env python3
"""Check the one shared command/native Preferences destructor boundary."""
import hashlib
import re
import sys
from pathlib import Path

if len(sys.argv) != 4:
    raise SystemExit('native-preferences-join.py build-profile.h command-pinned.h native-pinned.h')
profile, command, native = (Path(p).read_text() for p in sys.argv[1:])
is310 = bool(re.search(r'^#define MODEL_FIRMWARE_310 1$', profile, re.M))
address = 0x223d974 if is310 else 0x1efc91c
tail_sha = ('cea18b3f0a7face3e668b1a4d0c9e82ea5a97f57399a4d66683a14610e3f905c'
            if is310 else
            '4765bc2aef6224b6eb4e2d63db1070112702565dd96e872a5273d030912e6c9f')

def table(name):
    match = re.search(r'\b' + name + r'\[[^;=]+?=\{(.*?)\};', command, re.S)
    if not match:
        raise SystemExit('missing generated command table: ' + name)
    return [int(n, 16) for n in re.findall(r'0x[0-9a-fA-F]+', match[1])]

anchors, expected, relocated = table('anchor'), table('expected'), table('relocated')
if anchors.count(address) != 1 or len(expected) != 8 * len(anchors) or len(relocated) != 2 * len(anchors):
    raise SystemExit('missing or ambiguous shared command destructor anchor')
index = anchors.index(address)
if bytes(expected[index * 8:index * 8 + 8]).hex() != '34119fe534219fe5':
    raise SystemExit('shared destructor displaced bytes differ')
literal_values = [0x04c7a2fc, 0xfffe0f48] if is310 else [0x04ba936c, 0xfffe3b74]
if relocated[index * 2:index * 2 + 2] != literal_values:
    raise SystemExit('shared destructor relocation literals differ')
ranges = [(int(a, 16), int(n), label) for a, n, label in re.findall(
    r'\{0x([0-9a-fA-F]+)u,(\d+)u,(native_preferences_range_\d+)\}', native)]
if not ranges or any(a < address + 8 and address < a + n for a, n, _ in ranges):
    raise SystemExit('native Preferences live guard overlaps the installed command detour')
tails = [label for a, n, label in ranges if (a, n) == (address + 8, 320)]
if len(tails) != 1:
    raise SystemExit('missing exact shared destructor live tail')
match = re.search(r'\b' + tails[0] + r'\[320\]=\{([^}]+)\};', native)
data = bytes(int(n, 16) for n in re.findall(r'0x[0-9a-fA-F]+', match[1])) if match else b''
if hashlib.sha256(data).hexdigest() != tail_sha:
    raise SystemExit('shared destructor live tail bytes differ')
print(f'PASS selected shared destructor {address:#x}: exact command pair/literals and untouched 320-byte native UI tail')
