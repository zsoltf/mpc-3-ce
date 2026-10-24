"""Exact 3.10 Native Preferences preparation, called by the existing owner.

The coordinates are manually traced data, not relocation guesses. Runtime
admission still belongs to native-preferences-patch.c and the command owner.
"""
import hashlib
import json
import re
import struct
import sys
from pathlib import Path


def prepare(current_path, stock_path, output):
    here = Path(__file__).resolve().parent
    sys.path.insert(0, str(here.parents[1] / 'scripts'))
    from firmware_reanchor import ELF
    current, stock = ELF(current_path), ELF(stock_path)
    identity = json.loads((here.parents[1] / 'firmware/mpc-ce-3.10.json').read_text())
    for elf, digest, label in ((current, identity['output_sha256'], 'CE'),
                               (stock, identity['input_sha256'], 'stock')):
        if len(elf.raw) != identity['size'] or hashlib.sha256(elf.raw).hexdigest() != digest:
            raise SystemExit('wrong exact 3.10 ' + label + ' MPC')
    spec = json.loads((here / 'native-preferences-310-coordinates.json').read_text())
    if spec['stock_sha256'] != identity['input_sha256']:
        raise SystemExit('native Preferences coordinate source pin differs')
    ranges, sites, pointers = spec['ranges'], spec['sites'], spec['pointers']
    if len(sites) != 4:
        raise SystemExit('native Preferences requires its four existing hook owners')

    def address(value):
        return int(value, 16) if isinstance(value, str) else value

    def word(at):
        return struct.unpack('<I', current.at(address(at), 4))[0]

    def branch(at):
        at = address(at)
        value = word(at)
        if value & 0x0e000000 != 0x0a000000 or value >> 28 == 15:
            raise SystemExit(f'not an ARM branch at {at:#x}')
        displacement = value & 0xffffff
        if displacement & 0x800000:
            displacement -= 1 << 24
        return (at + 8 + 4 * displacement) & 0xffffffff

    for item in ranges:
        at, length = address(item['address']), item['length']
        data = current.at(at, length)
        if data != stock.at(at, length) or hashlib.sha256(data).hexdigest() != item['sha256']:
            raise SystemExit('native Preferences traced range differs: ' + item['name'])
    for item in sites:
        if current.at(address(item['address']), 8).hex() != item['bytes']:
            raise SystemExit('native Preferences hook bytes differ: ' + item['name'])
    for item in pointers:
        if word(item['slot']) != address(item['target']):
            raise SystemExit('native Preferences pointer differs: ' + item['name'])
    # These retain the existing preparation's caller/field/dispatch proof at
    # exact 3.10 instruction locations, in addition to complete range hashes.
    for item in spec['words']:
        if word(item['address']) != address(item['word']):
            raise SystemExit('native Preferences semantic word differs: ' + item['name'])
    for item in spec['branches']:
        if branch(item['address']) != address(item['target']):
            raise SystemExit('native Preferences semantic branch differs: ' + item['name'])

    for item in spec.get('veneers', []):
        at = address(item['address'])
        first, second, literal = struct.unpack('<3I', current.at(at, 12))
        target = (at + 12 + literal) & 0xffffffff
        if (first, second) != (0xe59fc000, 0xe08ff00c) or target != address(item['target']):
            raise SystemExit('native Preferences inter-range veneer differs: ' + item['name'])

    interiors = {address(item['address']) + 4: item['name'] for item in sites}
    for typ, offset, va, pa, size, mem, flags, align in current.loads:
        if typ != 1 or not flags & 1:
            continue
        for delta in range(0, size - 3, 4):
            value = struct.unpack_from('<I', current.raw, offset + delta)[0]
            if value & 0x0e000000 != 0x0a000000 or value >> 28 == 15:
                continue
            displacement = value & 0xffffff
            if displacement & 0x800000:
                displacement -= 1 << 24
            target = va + delta + 8 + 4 * displacement
            if target in interiors:
                raise SystemExit(f'direct ARM branch enters {interiors[target]} hook interior at {va+delta:#x}')

    def byte_list(data):
        return ','.join(hex(value) for value in data)

    lines = ['#ifndef NATIVE_PREFERENCES_PINNED_H', '#define NATIVE_PREFERENCES_PINNED_H',
             '#include <stdint.h>', '#define NATIVE_PREFERENCES_TAB_VTABLE_BYTES 32u',
             '#define NATIVE_PREFERENCES_BUTTON_VTABLE_BYTES 212u',
             '#define NATIVE_PREFERENCES_SITE_COUNT 4u',
             'typedef struct {uint32_t address,length;const unsigned char *bytes;} NativePreferencesRange;',
             'typedef struct {uint32_t address;unsigned char bytes[8];} NativePreferencesSite;',
             'typedef struct {uint32_t slot,target;} NativePreferencesPointer;',
             'static const NativePreferencesSite native_preferences_sites[NATIVE_PREFERENCES_SITE_COUNT]={']
    for item in sites:
        lines.append(f' {{0x{address(item["address"]):08x}u,{{{byte_list(bytes.fromhex(item["bytes"]))}}}}},')
    lines.append('};')
    runtime = []
    for item in ranges:
        at, length = address(item['address']), item['length']
        if item.get('relocated_table', False):
            continue
        if at == 0x223d974:
            if length != 328 or current.at(at, 8).hex() != '34119fe534219fe5':
                raise SystemExit('shared signal destructor extent/prefix differs')
            at, length = at + 8, length - 8
        runtime.append((at, length))
    for index, (at, length) in enumerate(runtime):
        lines.append(f'static const unsigned char native_preferences_range_{index}[{length}]={{{byte_list(current.at(at,length))}}};')
    lines += [f'#define NATIVE_PREFERENCES_RANGE_COUNT {len(runtime)}u',
              'static const NativePreferencesRange native_preferences_ranges[NATIVE_PREFERENCES_RANGE_COUNT]={']
    for index, (at, length) in enumerate(runtime):
        lines.append(f' {{0x{at:08x}u,{length}u,native_preferences_range_{index}}},')
    lines += ['};', f'#define NATIVE_PREFERENCES_POINTER_COUNT {len(pointers)}u',
              'static const NativePreferencesPointer native_preferences_pointers[NATIVE_PREFERENCES_POINTER_COUNT]={']
    for item in pointers:
        lines.append(f' {{0x{address(item["slot"]):08x}u,0x{address(item["target"]):08x}u}},')
    lines += ['};', '#endif', '']
    output = Path(output)
    output.write_text('\n'.join(lines))
    layout = ['#ifndef NATIVE_PREFERENCES_310_LAYOUT_H', '#define NATIVE_PREFERENCES_310_LAYOUT_H']
    for name, value in spec['constants'].items():
        if not re.fullmatch(r'[A-Z][A-Z0-9_]*', name):
            raise SystemExit('invalid native Preferences coordinate name')
        layout.append(f'#define NP_{name} {address(value):#x}u')
    layout += ['#endif', '']
    output.with_name('native-preferences-310-layout.h').write_text('\n'.join(layout))
    print(f'PASS exact 3.10 stock/CE native Preferences: {len(ranges)} traced ranges, '
          f'{len(runtime)} live ranges, {len(pointers)} relocated pointers, {len(sites)} hooks; '
          'semantic words/branches and no direct ARM branch into hook second words')
