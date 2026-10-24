#!/usr/bin/env python3
"""Offline ARM ELF discovery. Suggestions never change runtime admission/layouts."""
import argparse
import ast
from bisect import bisect_right
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import struct

REPO = Path(__file__).resolve().parents[1]


def sha(data):
    return hashlib.sha256(data).hexdigest()


class ELF:
    def __init__(self, path):
        self.raw = Path(path).read_bytes()
        if self.raw[:7] != b'\x7fELF\x01\x01\x01' or struct.unpack_from('<HH', self.raw, 16) != (3, 40):
            raise ValueError('expected ELF32 little-endian ARM ET_DYN')
        phoff = self.word(28)
        ent, count = struct.unpack_from('<HH', self.raw, 42)
        self.loads = [struct.unpack_from('<8I', self.raw, phoff + i * ent) for i in range(count)]
        self.loads = [s for s in self.loads if s[0] == 1]
        shoff = self.word(32)
        ent, count, strings = struct.unpack_from('<HHH', self.raw, 46)
        sections = [struct.unpack_from('<10I', self.raw, shoff + i * ent) for i in range(count)]
        names = sections[strings][4]
        self.sections = {}
        for s in sections:
            name = self.raw[names + s[0]:self.raw.index(b'\0', names + s[0])].decode('ascii')
            self.sections[name] = s
        s = self.sections['.text']
        self.text_start, self.text_end = s[3], s[3] + s[5]
        s = self.sections['.ARM.exidx']
        starts = []
        for off in range(0, s[5], 8):
            value = self.word(s[4] + off) & 0x7fffffff
            if value & 0x40000000:
                value -= 0x80000000
            address = s[3] + off + value
            if self.text_start <= address < self.text_end:
                starts.append(address)
        self.starts = sorted(set(starts)) + [self.text_end]
        self.vtables = self.read_vtables()
        self.slots = defaultdict(list)
        for key, tables in self.vtables.items():
            for address, slots in tables:
                for index, function in enumerate(slots):
                    self.slots[function].append((key, index, address, len(slots)))

    def word(self, offset):
        return struct.unpack_from('<I', self.raw, offset)[0]

    def at(self, address, length):
        for _, off, va, _, size, _, _, _ in self.loads:
            if va <= address and address + length <= va + size:
                return self.raw[off + address - va:off + address - va + length]
        raise ValueError(f'not file backed: {address:#x}+{length:#x}')

    def mapped(self, address):
        return any(va <= address < va + size for _, _, va, _, size, _, _, _ in self.loads)

    def executable(self, address):
        return any(s[2] & 4 and s[3] <= address < s[3] + s[5] for s in self.sections.values())

    def bounds(self, address):
        i = bisect_right(self.starts, address) - 1
        if 0 <= i < len(self.starts) - 1:
            return self.starts[i], self.starts[i + 1]
        return None

    def read_vtables(self):
        result = defaultdict(list)
        for section_name in ('.data.rel.ro', '.data.rel.ro.local'):
            if section_name not in self.sections:
                continue
            s = self.sections[section_name]
            data = self.raw[s[4]:s[4] + s[5]]
            for offset in range(0, len(data) - 12, 4):
                top, info, first = struct.unpack_from('<iII', data, offset)
                if not -0x100000 <= top <= 0 or not self.executable(first & ~1):
                    continue
                try:
                    nameptr = struct.unpack('<II', self.at(info, 8))[1]
                    name = self.at(nameptr, 256).split(b'\0', 1)[0]
                except ValueError:
                    continue
                if not re.fullmatch(rb'(?:N?\d|Z)[A-Za-z0-9_.$]+', name):
                    continue
                slots = []
                for pos in range(offset + 8, min(offset + 8 + 2048, len(data) - 3), 4):
                    value = struct.unpack_from('<I', data, pos)[0]
                    if not self.executable(value & ~1):
                        break
                    slots.append(value)
                result[(name.decode('ascii'), top)].append((s[3] + offset, slots))
        return result


def normalize(data, elf):
    """Mask ARM direct branch displacement and file-backed pointer literals only.

    This is a discovery heuristic, never a proof of callee or object layout.
    """
    out = bytearray(data)
    for offset in range(0, len(out) - 3, 4):
        word = struct.unpack_from('<I', out, offset)[0]
        if word & 0x0e000000 == 0x0a000000:
            word &= 0xff000000
        elif word >= 0x10000 and elf.mapped(word):
            word = 0
        struct.pack_into('<I', out, offset, word)
    return bytes(out)


class Matcher:
    def __init__(self, old, new):
        self.old, self.new = old, new
        self.exact = defaultdict(list)
        self.normal = defaultdict(list)
        for start, end in zip(new.starts, new.starts[1:]):
            data = new.at(start, end - start)
            self.exact[(len(data), sha(data))].append(start)
            self.normal[(len(data), sha(normalize(data, new)))].append(start)
        self.cache = {}
        self.old_slot_functions = sorted(old.slots)

    def locate(self, address):
        if address in self.cache:
            return self.cache[address]
        value = self._locate(address)
        self.cache[address] = value
        return value

    def _locate(self, address):
        old, new = self.old, self.new
        result = {'status': 'needs-trace', 'confidence': 'none', 'candidates': [],
                  'layout_status': 'unresolved'}
        bounds = old.bounds(address)
        if bounds:
            start, end = bounds
            data = old.at(start, end - start)
            delta = address - start
            result['old_unwind'] = [hex(start), hex(end)]
            result['offset_in_unwind'] = delta
            identities = []
            semantic = set()
            # Unwind entries can fold several virtual functions. Only an exact
            # slot function is meaningful, never the first function in a fold.
            for key, slot, _, size in old.slots.get(address, []):
                old_tables, new_tables = old.vtables[key], new.vtables.get(key, [])
                if len(old_tables) == len(new_tables) == 1:
                    table, functions = new_tables[0]
                    if len(functions) == size and slot < len(functions):
                        semantic.add(functions[slot])
                        identities.append({'type': key[0], 'offset_to_top': key[1],
                                           'slot': slot, 'new_vtable': hex(table)})
            result['rtti'] = identities
            owners = []
            # A detour may lie inside a virtual function or folded unwind.
            # Emit the candidate owner, without pretending its old interior
            # offset remains valid in changed code.
            owner_index = bisect_right(self.old_slot_functions, address) - 1
            if owner_index >= 0 and self.old_slot_functions[owner_index] >= start:
                owner = self.old_slot_functions[owner_index]
                for key, slot, _, size in old.slots[owner]:
                    tables = new.vtables.get(key, [])
                    if len(old.vtables[key]) == len(tables) == 1 and len(tables[0][1]) == size:
                        owners.append({'type': key[0], 'slot': slot,
                                       'old_function': hex(owner),
                                       'new_function': hex(tables[0][1][slot])})
            result['rtti_owner_candidates'] = owners
            matches = self.exact.get((len(data), sha(data)), [])
            method = 'unique-exact-unwind'
            if len(matches) != 1:
                matches = self.normal.get((len(data), sha(normalize(data, old))), [])
                method = 'unique-normalized-unwind'
            candidates = {v + delta for v in matches}
            if len(candidates) == 1 and len(data) >= 16:
                target = next(iter(candidates))
                if semantic and semantic != {target}:
                    result.update(reason='signature disagrees with RTTI slot',
                                  candidates=[hex(v) for v in sorted(candidates | semantic)])
                    return result
                result.update(status='auto', confidence='high' if semantic or method == 'unique-exact-unwind' else 'medium',
                              method=method, candidates=[hex(target)],
                              reason='address suggestion only; validate branches, ABI and fields before admission')
            else:
                result.update(candidates=[hex(v) for v in sorted(semantic or candidates)],
                              confidence='medium' if len(semantic) == 1 else 'low' if candidates else 'none',
                              reason='RTTI slot needs body trace' if semantic else 'changed, short or ambiguous unwind body')
            return result
        # Report a vtable coordinate by exact type identity and subobject. This
        # does not promote a whole table whose slots or ABI may have changed.
        for key, tables in old.vtables.items():
            for table, slots in tables:
                if table <= address < table + 8 + len(slots) * 4:
                    nt = new.vtables.get(key, [])
                    if len(tables) == len(nt) == 1 and len(nt[0][1]) == len(slots):
                        result.update(confidence='medium', candidates=[hex(nt[0][0] + address - table)],
                                      reason='RTTI vtable coordinate; slot semantics require trace',
                                      rtti=[{'type': key[0], 'offset_to_top': key[1]}])
                        return result
        result['reason'] = 'data, PLT or non-unwind coordinate needs trace'
        return result


def inventory(repo, old):
    paths = [repo / 'controllers/program-observer/model-prepare.py',
             repo / 'controllers/program-observer/native-preferences-prepare.py',
             repo / 'ui/private/mpc-3.9.1-profile.h']
    paths += sorted((repo / 'vst2').rglob('*prepare*.py'))
    items, coverage, fields = [], [], []
    for path in paths:
        source = path.read_text()
        rel = str(path.relative_to(repo))
        entries = {}
        tree = ast.parse(source) if path.suffix == '.py' else None
        if tree:
            for node in ast.walk(tree):
                if not isinstance(node, ast.Tuple) or len(node.elts) < 2:
                    continue
                a, b = node.elts[:2]
                if not isinstance(a, ast.Constant) or type(a.value) is not int or a.value < 0x10000 or not old.mapped(a.value):
                    continue
                if isinstance(b, ast.Constant) and isinstance(b.value, str) and re.fullmatch('[0-9a-f]{16}', b.value):
                    entries[(node.lineno, a.value)] = ('detour', 8, b.value)
                elif isinstance(b, ast.Constant) and type(b.value) is int and 0 < b.value < 0x100000:
                    label = node.elts[2].value if len(node.elts) > 2 and isinstance(node.elts[2], ast.Constant) else ''
                    entries[(node.lineno, a.value)] = ('range', b.value, str(label))
        # Includes literals inside staged C/Python source strings, not comments.
        # AST extracts hook/range labels; token scan accounts for remaining
        # constants, macro RVAs and provenance. Each source coordinate is kept.
        for lineno, line in enumerate(source.splitlines(), 1):
            if path.suffix == '.h' and not line.startswith('#define MPC_UI_RVA_'):
                if line.startswith('#define MPC_UI_') and not any(x in line for x in ('SHA256', 'PROFILE_')):
                    fields.append({'source': rel, 'line': lineno, 'definition': line, 'status': 'unresolved'})
                continue
            if line.lstrip().startswith(('# ', '//', '/*', '*')):
                continue
            for match in re.finditer(r'\b0x([0-9a-fA-F]{4,8})(?:[uUlL]*)\b', line):
                address = int(match[1], 16)
                if address >= 0x10000 and old.mapped(address):
                    entries.setdefault((lineno, address), ('address', 0, line.strip()[:140]))
        for (line, address), (kind, length, label) in sorted(entries.items()):
            items.append({'source': rel, 'line': line, 'old_address': hex(address),
                          'kind': kind, 'length': length, 'label': label})
        coverage.append({'source': rel, 'sha256': sha(source.encode()), 'coordinates': len(entries)})
    return items, coverage, fields


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--old', type=Path, required=True, help='exact stock or CE 3.9.1 ELF')
    parser.add_argument('--new', type=Path, required=True, help='exact stock 3.10 ELF')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--image', type=Path, help='optionally verify the official 3.10 AZ01 image and decoded root pin')
    args = parser.parse_args()
    old, new = ELF(args.old), ELF(args.new)
    legacy = json.loads((REPO / 'firmware/mpc-ce.json').read_text())
    pin = json.loads((REPO / 'firmware/mpc-3.10.json').read_text())
    if len(old.raw) != legacy['size'] or sha(old.raw) not in (legacy['input_sha256'], legacy['output_sha256']):
        raise SystemExit('wrong 3.9.1 baseline ELF')
    if len(new.raw) != pin['executable']['size'] or sha(new.raw) != pin['executable']['sha256']:
        raise SystemExit('wrong 3.10 candidate ELF')
    image_report = None
    if args.image:
        import image_format
        image = args.image.read_bytes()
        if len(image) != pin['image']['size'] or sha(image) != pin['image']['sha256']:
            raise SystemExit('wrong 3.10 image')
        del image
        # Reuse the existing structural decoder in this offline process only;
        # production image_format defaults and admission remain unchanged.
        image_format.ROOT_SIZE = pin['rootfs']['size']
        image_report = image_format.decode(args.image)
        if image_report['rootfs_sha256'] != pin['rootfs']['sha256']:
            raise SystemExit('wrong 3.10 rootfs')
    items, coverage, fields = inventory(REPO, old)
    matcher = Matcher(old, new)
    for item in items:
        if item['kind'] == 'detour' and old.at(int(item['old_address'], 16), 8).hex() != item['label']:
            raise SystemExit(f"stale baseline detour recipe: {item['source']}:{item['line']}")
        item.update(matcher.locate(int(item['old_address'], 16)))
        if item['kind'] == 'range' and item['length'] and 'old_unwind' in item:
            end = int(item['old_unwind'][1], 16)
            if int(item['old_address'], 16) + item['length'] > end:
                item.update(status='needs-trace', confidence='low', reason='range crosses unwind boundary; only first coordinate located')
    counts = defaultdict(Counter)
    hook_counts = defaultdict(Counter)
    unique = set()
    for item in items:
        counts[item['source']][item['status']] += 1
        key = (item['source'], item['kind'], item['old_address'], item['length'])
        if key not in unique:
            unique.add(key)
            hook_counts[item['source']][item['kind'] + ':' + item['status']] += 1
    report = {'schema_version': 1, 'purpose': 'offline discovery; no runtime admission or field-layout validation',
              'generator_sha256': sha(Path(__file__).read_bytes()),
              'old_path': str(args.old), 'new_path': str(args.new),
              'old_sha256': sha(old.raw), 'new_sha256': sha(new.raw),
              'image_verification': image_report,
              'rtti_vtable_keys': {'old': len(old.vtables), 'new': len(new.vtables), 'shared': len(old.vtables.keys() & new.vtables.keys())},
              'counts': {k: dict(v) for k, v in counts.items()},
              'distinct_coordinate_counts': {k: dict(v) for k, v in hook_counts.items()}, 'coverage': coverage,
              'incomplete_coverage': ['Numeric source coordinates only; computed addresses, imported/runtime include field offsets and assembly are not exhaustively inventoried.',
                                      'VST source transformation scripts with no ELF coordinates are listed with zero coverage; this does not validate their generated consumers.',
                                      'PLT/GOT, RTTI table layout, changed/folded bodies and all object fields require separate validation.',
                                      'No native execution, audio, controller, reconnect or release compatibility is established.'],
              'unresolved_profile_fields': fields, 'items': items}
    args.out.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'items': len(items), 'counts': report['counts'], 'distinct_coordinate_counts': report['distinct_coordinate_counts'], 'rtti_vtable_keys': report['rtti_vtable_keys']}, indent=2))


if __name__ == '__main__':
    main()
