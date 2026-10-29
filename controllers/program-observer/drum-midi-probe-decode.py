#!/usr/bin/env python3
"""Decode finite native receipts; never equate enqueue with physical delivery.

Pending association compares copied semantic fields, not a moved heap address.
Ambiguous duplicates stay ambiguous. This is offline reporting, not a note ledger.
"""
import argparse
import json
from collections import defaultdict


def pending_key(row):
    p = row['payload']
    # Native type3 move (3.10 RVA 0x1ad0440/0x1ad05a0) copies Event+0..8,
    # +0x10..0x33 and only byte+0x34. Event+0x0c and +0x35..0x37 are padding;
    # real heap moves leave those bytes different from the insertion temporary.
    # Keep every copied note-event byte, expiry and routing identity. Other
    # event layouts are not inferred from the note move.
    event = (*p[1:4], *p[5:14], p[14] & 255) if (p[3] & 255) == 3 else tuple(p[1:15])
    return (row['epoch'], p[0], *event, *p[15:17], p[17] & 255, *p[18:22], p[22] & 255)


def decode(capture):
    if capture.get('format') != 'MIDI_NATIVE_PROBE1' or capture.get('command_version') != 3201:
        raise ValueError('requires MIDI_NATIVE_PROBE1 / diagnostic CMD3201')
    rows = capture['records']
    if len(rows) > 1024:
        raise ValueError('record bound exceeded')
    seen = set()
    pending = defaultdict(list)
    constructors = defaultdict(list)
    previous_sequence = 0
    sequence_gaps = []
    output = []
    pads = defaultdict(list)
    expiry = []
    drains = []
    cleanups = []
    for row in rows:
        seq, p, site = row['sequence'], row['payload'], row['site']
        if not 1 <= seq <= 1024 or seq <= previous_sequence or len(p) != 23:
            raise ValueError('invalid fixed receipt')
        if seq != previous_sequence + 1:
            sequence_gaps.append([previous_sequence + 1, seq - 1])
        previous_sequence = seq
        seen.add(seq)
        if site == 'map' and p[9]:
            pads[(row['epoch'], row['root'])].append({'map_sequence': seq, 'track': p[0],
                                     'program': p[2], 'pad': p[8], 'instrument': p[9],
                                     'incarnation': p[10], 'membership_generation': p[11]})
        elif site == 'pending':
            pending[pending_key(row)].append(row)
        elif site in ('due', 'drain'):
            candidates = [r for r in pending[pending_key(row)] if r['sequence'] < seq]
            releases = expiry if site == 'due' else drains
            releases.append({site + '_sequence': seq, 'epoch': row['epoch'], 'release_root': row['root'],
                           'candidate_source_roots': [r['root'] for r in candidates],
                           'candidate_pending_sequences': [r['sequence'] for r in candidates],
                           'association': 'unique' if len(candidates) == 1 else 'ambiguous' if candidates else 'unmatched'})
            if len(candidates) == 1:
                pending[pending_key(row)].remove(candidates[0])
        elif site == 'cleanup':
            cleanups.append({'cleanup_sequence': seq, 'end_sequence': None,
                             'epoch': row['epoch'], 'root': row['root'],
                             'frame': row['frame'], 'token': row['token'],
                             'track': p[0], 'track_incarnation': p[1],
                             'program': p[2], 'program_incarnation': p[3],
                             'association': 'source_wide_not_individual_emission'})
        elif site == 'cleanup_end':
            for cleanup in reversed(cleanups):
                if cleanup['end_sequence'] is None and all(cleanup[f] == row[f] for f in ('epoch', 'root', 'frame', 'token')):
                    cleanup['end_sequence'] = seq
                    break
        elif site in ('on_bytes', 'off_bytes'):
            constructors[(row['token'], row['frame'], site[:2])].append(row)
        elif site in ('on_sink', 'off_sink'):
            key = (row['token'], row['frame'], site[:2])
            candidates = constructors[key]
            mismatches = []
            construction = candidates[0] if len(candidates) == 1 else None
            if construction:
                for field in ('epoch', 'root'):
                    if not row[field] or construction[field] != row[field]:
                        mismatches.append(field)
                if construction['payload'][5] != p[1]:
                    mismatches.append('route_key')
                if not row['frame']:
                    mismatches.append('frame')
                if construction['sequence'] >= seq or any(n not in seen for n in range(construction['sequence'], seq)):
                    mismatches.append('sequence')
                if mismatches:
                    construction = None
            association = ('unique' if construction else 'ambiguous' if len(candidates) > 1
                           else 'mismatch' if candidates else 'unmatched')
            output.append({'sink_sequence': seq, 'epoch': row['epoch'], 'root': row['root'], 'frame': row['frame'],
                           'association': association, 'mismatch_fields': mismatches,
                           'candidate_constructor_sequences': [r['sequence'] for r in candidates],
                           'constructor_sequence': construction['sequence'] if construction else None,
                           'bytes': [v & 255 for v in construction['payload'][:3]] if construction else None,
                           'sink': p[0], 'route_key': p[1], 'sink_vptr': p[4], 'target': p[3],
                           'physical_delivery_observed': False})
            # Only an unambiguous compatible sink retires its constructor.
            # Ambiguous/mismatched evidence remains visible at the end.
            if construction:
                constructors[key] = []
    for event in output:
        event['drum_mappings_in_root'] = pads[(event['epoch'], event['root'])]
        event['duration_associations'] = [e for e in expiry if e['epoch'] == event['epoch'] and e['release_root'] == event['root']]
        event['drain_associations'] = [e for e in drains if e['epoch'] == event['epoch'] and e['release_root'] == event['root']]
        event['track_cleanup_contexts'] = [e for e in cleanups if e['epoch'] == event['epoch'] and e['root'] == event['root']
                                         and e['end_sequence'] is not None and e['cleanup_sequence'] < event['sink_sequence'] < e['end_sequence']]
    unpaired = sorted(r['sequence'] for group in constructors.values() for r in group)
    correlation_complete = (bool(capture.get('capture_complete')) and not capture['error'] and not sequence_gaps
                            and not unpaired and all(e['association'] == 'unique' for e in output + expiry + drains)
                            and all(e['end_sequence'] is not None for e in cleanups))
    return {'capture_complete': capture.get('capture_complete', False),
            'correlation_complete': correlation_complete, 'sequence_gaps': sequence_gaps,
            'error': capture['error'], 'physical_delivery_observed': False,
            'association_limit': 'A shared root links observations; it does not make a downstream Drum pad the owner of an earlier MIDI output. Track cleanup is source-wide context, not an individual emission pairing.',
            'outputs': output, 'duration_expiry': expiry, 'pending_drain': drains,
            'track_cleanup': cleanups,
            'constructed_without_sink': unpaired}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    args = parser.parse_args()
    with open(args.capture, encoding='ascii') as stream:
        capture = json.load(stream)
    print(json.dumps(decode(capture), indent=2))


if __name__ == '__main__':
    main()
