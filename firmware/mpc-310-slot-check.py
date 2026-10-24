#!/usr/bin/env python3
"""Recheck traced 3.10 CE data-slot ownership against exact stock ELFs.

This read-only proof preserves the independently traced InfoTab and Az01
constructor/call evidence. It does not execute either native behavior.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('old', type=Path, help='pristine 3.9.1 MPC')
parser.add_argument('new', type=Path, help='pristine 3.10 MPC')
args = parser.parse_args()
HERE = Path(__file__).resolve().parent


class Image:
    def __init__(self, path, size, digest):
        self.raw = path.read_bytes()
        assert len(self.raw) == size
        assert hashlib.sha256(self.raw).hexdigest() == digest
        assert self.raw[:7] == b'\x7fELF\x01\x01\x01'
        assert struct.unpack_from('<HH', self.raw, 16) == (3, 40)
        phoff = struct.unpack_from('<I', self.raw, 28)[0]
        ent, count = struct.unpack_from('<HH', self.raw, 42)
        self.loads = [struct.unpack_from('<8I', self.raw, phoff + i * ent)
                      for i in range(count)]
        self.loads = [p for p in self.loads if p[0] == 1]

    def offset(self, va, size=4):
        return next(off + va - start for _, off, start, _, length, _, _, _
                    in self.loads if start <= va and va + size <= start + length)

    def at(self, va, size):
        off = self.offset(va, size)
        return self.raw[off:off + size]

    def word(self, va):
        return struct.unpack('<I', self.at(va, 4))[0]


old = Image(args.old, 112222004,
            'bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426')
new = Image(args.new, 116497964,
            '753f838ca362d42c8cc3f5c9c3fcc604ba076b5919212e0301923a3c11c93494')

# The old accepted data coordinates; these are not used as relocation deltas.
assert old.at(0x04b16ab4, 12) == b' Firmware\0\0\0'
assert old.offset(0x068a4260) == 109720160
assert old.word(0x068a4260) == 2
assert old.at(old.word(0x068a425c), 9) == b'ehci_hcd\0'

# InfoTab RTTI, primary vptr, constructor assignment and both literal references.
info_name = b'N16TouchPreferences7InfoTabE\0'
assert new.at(new.word(0x06dddd5c + 4), len(info_name)) == info_name
assert new.word(0x06dddcc4 - 4) == 0x06dddd5c
assert new.word(0x0392146c) + 0x039211f4 + 8 + 0x68 == 0x06dddcc4
assert new.word(0x0392148c) + 0x039214ec + 8 == 0x04dd8810
assert new.word(0x03921974) + 0x03921fc4 + 8 == 0x04dd8810
assert new.word(0x039213fc) == 0xe2889009  # add r9,r8,#9
assert new.word(0x039214cc) == 0xe2889009  # append length +9
assert new.word(0x039214f0) == 0xe1c200d0  # ldrd r0,[r2]
assert new.word(0x039214f4) == 0xe5d2e008  # ldrb lr,[r2,#8]
assert new.word(0x03921fc8) == 0xe5d3c008  # allocated branch: byte 8
assert new.word(0x03921fcc) == 0xe1c320d0  # allocated branch: first 8 bytes
assert new.raw.count(b' Firmware\0\0\0') == 1

# Az01 RTTI/vtable slot 5 establishes the policy's platform owner.
assert new.at(new.word(0x06d3322c + 4), 6) == b'4Az01\0'
assert new.word(0x06d33240 - 4) == 0x06d3322c
assert new.word(0x06d33240 + 5 * 4) == 0x022c87d0
base = new.word(0x022c8898) + 0x022c8804 + 8
assert base == 0x06cae0f8
assert new.word(0x022c8834) == 0xe285100c  # r1 = table +12
assert new.word(0x022c8808) == 0xe285602c  # r6 = table +44
call = new.word(0x022c8840)
assert call >> 24 == 0xeb
disp = call & 0xffffff
if disp & 0x800000:
    disp -= 1 << 24
assert 0x022c8840 + 8 + disp * 4 == 0x022f998c
assert new.word(base + 0x14) == 0x04d6e018
assert new.at(0x04d6e018, 9) == b'ehci_hcd\0'
assert new.word(base + 0x18) == 2
assert new.offset(base + 0x18) == 0x06cad110

# These verified PC-relative values identify the native writer's paths.
for literal, add, value in [
    (0x022fa9a0, 0x022f99cc, b'/proc/irq\0'),
    (0x022fa9a8, 0x022f9a0c, b'/proc/irq/\0'),
    (0x022fa9ac, 0x022f9a10, b'/smp_affinity_list\0'),
]:
    target = new.word(literal) + add + 8
    assert new.at(target, len(value)) == value
assert new.word(0x022f9ca8) == 0xe5988004  # CPU = matched row +4
assert new.word(0x022f9d8c) == 0xe1a01008  # integer formatting argument

spec = json.loads((HERE / 'mpc-ce-3.10.json').read_text())
assert spec['size'] == len(new.raw)
assert spec['input_sha256'] == hashlib.sha256(new.raw).hexdigest()
patched = bytearray(new.raw)
allowed = []
end = 0
for slot in spec['slots']:
    offset = slot['offset']
    before = bytes.fromhex(slot['before_hex'])
    after = bytes.fromhex(slot['after_hex'])
    assert before and len(before) == len(after)
    assert end <= offset <= len(new.raw) - len(before)
    assert new.raw[offset:offset + len(before)] == before
    if slot['unique']:
        assert new.raw.count(before) == 1
    patched[offset:offset + len(after)] = after
    allowed.extend(offset + i for i, (a, b) in enumerate(zip(before, after)) if a != b)
    end = offset + len(before)
assert patched[0x04dd8810:0x04dd8819] == b' CE 0.4.0'
assert patched[0x04dd8819:0x04dd881c] == b'\0\0\0'
changed = [i for i, (a, b) in enumerate(zip(new.raw, patched)) if a != b]
assert len(patched) == len(new.raw) and changed == allowed and len(changed) == 9
assert hashlib.sha256(patched).hexdigest() == spec['output_sha256']
print(json.dumps({'status': 'PASS: offline bytes and traced instruction contracts only',
                  'output_sha256': spec['output_sha256'], 'size': len(patched),
                  'changed_offsets': changed,
                  'native_display_irq_and_playback': 'unobserved'}, indent=2))
