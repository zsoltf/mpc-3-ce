"""Pinned, manually traced 3.10 coordinates for explicit preparation.

Finite model/mirror components use stock 3.10; the command profile requires the
exact composed CE ELF. Existing prepare/build owners retain admission authority.
No coordinate is admitted through similarity or runtime version detection.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys


def prepare(path, output, mirror_sites=None, command=False):
    here = Path(__file__).resolve().parent
    sys.path.insert(0, str(here.parents[1] / 'scripts'))
    from firmware_reanchor import ELF
    elf = ELF(path)
    pin = json.loads((here.parents[1] / 'firmware/mpc-3.10.json').read_text())['executable']
    if command:
        ce = json.loads((here.parents[1] / 'firmware/mpc-ce-3.10.json').read_text())
        pin = {'size': ce['size'], 'sha256': ce['output_sha256']}
        for slot in ce['slots']:
            offset = slot['offset']
            expected = bytes.fromhex(slot['after_hex'])
            if elf.raw[offset:offset+len(expected)] != expected:
                raise SystemExit('3.10 CE patch slot mismatch')
    if len(elf.raw) != pin['size'] or hashlib.sha256(elf.raw).hexdigest() != pin['sha256']:
        raise SystemExit('wrong exact 3.10 CE ELF' if command else 'wrong stock 3.10 ELF for offline model component')
    spec = json.loads((here / 'model-310-coordinates.json').read_text())
    if mirror_sites is not None:
        extension = json.loads((here / 'mirror-310-coordinates.json').read_text())
        spec['sites'] += extension['sites']
        spec['functions'] += extension['functions']
        spec['constants'].update(extension['constants'])
        # Traced shared command foundations supply fields/guards only here.
        # Their pending hooks are not selected by the finite mirror profile.
        command_extension = json.loads((here / 'command-310-coordinates.json').read_text())
        spec['functions'] += command_extension['functions']
        spec['constants'].update(command_extension['constants'])
        if command:
            spec['sites'] += extension['pending_command_sites'] + command_extension['sites']
        by_old = {int(site['old'], 16): site for site in spec['sites']}
        spec['sites'] = [by_old[a] for a, _ in mirror_sites]
    if 'EXECUTABLE_END' in spec['constants'] and spec['constants']['EXECUTABLE_END'] != next(x[2] + x[5] for x in elf.loads if x[6] & 1):
        raise SystemExit('selected executable segment bound changed')
    if 'WRITABLE_START' in spec['constants']:
        rw = next(x for x in elf.loads if x[0] == 1 and x[6] & 2)
        if (spec['constants']['WRITABLE_START'], spec['constants']['WRITABLE_END']) != (rw[2], rw[2]+rw[5]):
            raise SystemExit('selected writable memory segment bounds changed')
    for f in spec['functions']:
        a, n = int(f['start'], 16), f['length']
        if elf.bounds(a) != (a, a + n) or hashlib.sha256(elf.at(a, n)).hexdigest() != f['sha256']:
            raise SystemExit(f'3.10 traced function changed: {a:x}')
    sites = []
    for site in spec['sites']:
        a = int(site['address'], 16)
        if elf.at(a, 8).hex() != site['bytes'] or list(elf.bounds(a)) != [int(x, 16) for x in site['unwind']]:
            raise SystemExit(f"3.10 traced hook changed: {site.get('name', site.get('id'))}")
        sites.append((a, site['bytes']))
    data_words = set()
    if command:
        # This exact float literal aliases a conditional BL opcode into a meter
        # patch. Its unique VFP load, bounded branch table and complete local
        # control flow were traced independently; the full fold is guarded.
        if elf.at(0x3fbcadc,4).hex() != '52769f3b' or elf.at(0x3fbc8bc,4).hex() != '868a9fed':
            raise SystemExit('qualified input literal changed')
        if struct.unpack('<3I',elf.at(0x3fbca84,12)) != (0xe595319c,0xe353000a,0x908ff103):
            raise SystemExit('qualified literal pool switch changed')
        for a in range(0x3fbca90,0x3fbcac0,4):
            w = struct.unpack('<I',elf.at(a,4))[0]
            if w >> 24 != 0xea:
                raise SystemExit('literal pool boundary is no longer an unconditional branch table')
        data_words.add(0x3fbcadc)
    interiors = {a + 4 for a, _ in sites}
    text = elf.at(elf.text_start, elf.text_end - elf.text_start)
    for offset, (w,) in enumerate(struct.iter_unpack('<I', text[:len(text)//4*4])):
        if elf.text_start + 4*offset in data_words:
            continue
        if w & 0x0e000000 != 0x0a000000:
            continue
        d = w & 0xffffff
        if d & 0x800000:
            d -= 1 << 24
        if elf.text_start + 4*offset + 8 + 4*d in interiors:
            raise SystemExit(f'direct ARM branch at {elf.text_start + 4*offset:x} enters second 3.10 patch word {elf.text_start + 4*offset + 8 + 4*d:x}')
    for name, vptr, destructor in (('9MpcEngine', 0x6ce5650, 0xffccac),
                                    ('7Project', 0x6d501c4, 0x2917658),
                                    ('9TrackPool', 0x6cb7528, 0x11eab68),
                                    ('5Track', 0x6d51624, 0x29cd4a4)):
        if not any(a + 8 == vptr and slots[0] == destructor for a, slots in elf.vtables.get((name, 0), [])):
            raise SystemExit(f'3.10 primary owner RTTI changed: {name}')
    # The selected queue must call the shared Program body, not its repeated
    # byte-identical closure for another consumer.
    def call(a, target, tail=False):
        w = struct.unpack('<I', elf.at(a, 4))[0]
        d = w & 0xffffff
        if d & 0x800000:
            d -= 1 << 24
        if w >> 24 != (0xea if tail else 0xeb) or a + 8 + 4*d != target:
            raise SystemExit(f'3.10 call provenance changed: {a:x}')
    for a, target in ((0x1d9cea8, 0xfec1f0), (0x1d9d0d0, 0x29771e0),
                      (0x1d9d0f4, 0x290fb30), (0x2916410, 0x2937070),
                      (0x28d77c0, 0x28d74bc), (0x28d4af4, 0x27a7494),
                      (0x27a74d4, 0x27a71c8), (0x27a7228, 0x27a5ecc),
                      (0x27a5f6c, 0x268d4f8), (0x28d4220, 0x27a7940),
                      (0x27a7958, 0x27a75ac), (0x27a75ec, 0x27a6958),
                      (0x27a69d0, 0x268db64)):
        call(a, target)
    if command:
        # Direct typed dispatch and caller relationships replace the old embedded
        # firmware assertions; complete reviewed native bodies are guarded above.
        for v in (0x6dafc00,0x6dd3d3c,0x6dd47a0,0x6dd9cb0):
            if struct.unpack('<4I',elf.at(v+0x14,16)) != (0x3847f50,0x384840c,0x38484e4,0x38481fc):
                raise SystemExit('3.10 focus virtual dispatch changed')
        for a, expected in ((0x6dd74b4+0xd4,(0x30df6ac,0x30df364)),(0x6dd7808+0xc4,(0x149e904,)),(0x6d19b94+0x20,(0x1ad5284,0x1ad2568)),(0x6d04ee8+0x10,(0x155e700,))):
            if struct.unpack('<'+'I'*len(expected),elf.at(a,4*len(expected))) != expected:
                raise SystemExit('3.10 typed input/duplicate dispatch changed')
        if elf.at(0x155e700,12).hex() != '403090e5840dd3e51eff2fe1':
            raise SystemExit('3.10 physical modifier getter changed')
        if elf.at(0x35a3a38,24).hex() != '003095e50c20d4e5d810d4e10500a0e1203093e533ff2fe1':
            raise SystemExit('3.10 panel callback argument shape changed')
    if mirror_sites is not None:
        for witness in extension['calls'] + command_extension['calls']:
            call(int(witness['address'], 16), int(witness['target'], 16),
                 witness.get('tail', False))
    # The allowed concrete Program types must still have a public, nonvirtual,
    # zero-offset chain to Program. No other derived fields are admitted here.
    def word(a):
        return struct.unpack('<I', elf.at(a, 4))[0]
    for value in spec['programs']:
        t = word(int(value, 16) - 4)
        for _ in range(10):
            name = elf.at(word(t + 4), 128).split(b'\0', 1)[0]
            if name == b'7Program':
                break
            base = word(t + 8)
            if base <= 3:
                count = word(t + 12)
                if not 0 < count < 16:
                    raise SystemExit('invalid traced Program RTTI base count')
                bases = [word(t + 16 + 8*i) for i in range(count) if word(t + 20 + 8*i) == 2]
                if len(bases) != 1:
                    raise SystemExit('Program zero-offset inheritance changed')
                base = bases[0]
            t = base
        else:
            raise SystemExit('no traced Program zero-offset base')
    target = Path(output).parent
    text = ['/* Generated only after exact 3.10 ELF and coordinate checks. */']
    text += [f'#define MODEL_{name} {hex(value)}u' for name, value in spec['constants'].items()]
    text += ['#define MODEL_PROGRAM_VPTRS {' + ','.join(value + 'u' for value in spec['programs']) + '}']
    text += [f'#define MODEL_{name.upper()}_VPTR {value}u' for value, name in spec['programs'].items()]
    (target / 'model-310-layout.h').write_text('\n'.join(text) + '\n')
    (target / 'model-310-identity.h').write_text(
        f'#define MPC_EXECUTABLE_SIZE {pin["size"]}\n'
        f'#define MPC_EXECUTABLE_SHA256 "{pin["sha256"]}"\n'
        '#define MPC_EXECUTABLE_BUILD_ID_BYTES {' + ','.join(hex(x) for x in bytes.fromhex(spec['build_id'])) + '}\n')
    print(f'PASS exact 3.10 {"CE command" if command else "stock offline mirror" if mirror_sites is not None else "stock offline model"} coordinates: {len(sites)} hooks')
    print('PASS primary owner RTTI, Program zero-offset base chains, constructor calls and no direct ARM branch into patch interiors')
    return sites, [(int(f['start'], 16), f['length']) for f in spec['functions']], {
        int(s['address'], 16): int(s['old'], 16) for s in spec['sites']}, data_words
