#!/bin/sh
# Explicit opt-in package first; all output belongs to the new scratch directory.
set -eu
cd "$(dirname "$0")"
[ "$#" -eq 1 ] || { echo 'drum-midi-probe-check.sh /absolute/new-scratch-directory' >&2; exit 2; }
case "$1" in /*) ;; *) exit 2;; esac
[ "$(sed -n 's/^#define MODEL_MIDI_PROBE //p' package/command/model-build-profile.h)" = 1 ] || { echo 'prepare the explicit MIDI probe command profile first' >&2; exit 2; }
mkdir "$1"
out=$1
docker run --rm --network none -v "$PWD:/src:ro" -v "$out:/out" mpclearn-controls-build sh -ec '
flags="-include /src/package/command/model-build-profile.h -I/src/package/command -std=c11 -O2 -Wall -Wextra -Werror -marm -mfpu=neon"
arm-linux-gnueabihf-gcc $flags -DVOLUME_MIRROR -DMIRROR_COMMAND -DCOMMAND_COMPONENT -no-pie -Wl,-Ttext-segment=0x10000000 /src/drum-midi-probe-component.c /src/component.S /src/gate.S /src/transport-queue.c -o /out/drum-midi-probe-component -pthread -ldl -lm
arm-linux-gnueabihf-gcc $flags /src/command-client.c -o /out/command-client
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /out/drum-midi-probe-component /out/probe.state
/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3 --library-path /usr/arm-linux-gnueabihf/lib /out/command-client midi-probe-export /out/probe.state > /out/probe.json
'
python3 -B drum-midi-probe-decode.py "$out/probe.json" > "$out/decoded.json"
python3 -B - "$out/decoded.json" "$out/probe.json" <<'PY'
import copy,importlib.util,json,sys
j=json.load(open(sys.argv[1]))
assert j['capture_complete'] and j['correlation_complete'] and not j['error'] and not j['physical_delivery_observed']
assert [x['bytes'] for x in j['outputs']]==[[145,72,100],[129,72,0],[145,72,100],[145,72,100],[129,72,0],[129,72,0]]
assert j['duration_expiry'][0]['candidate_source_roots']==[1]
assert j['outputs'][2]['drum_mappings_in_root'][0]['map_sequence']>j['outputs'][2]['sink_sequence']
assert j['pending_drain'][0]['association']=='unique'
assert j['outputs'][4]['drain_associations']==j['pending_drain']
assert not j['outputs'][4]['track_cleanup_contexts']
assert j['outputs'][5]['track_cleanup_contexts']==j['track_cleanup']
assert j['track_cleanup'][0]['association']=='source_wide_not_individual_emission'
assert j['track_cleanup'][0]['end_sequence']>j['outputs'][5]['sink_sequence']
assert not j['outputs'][5]['drum_mappings_in_root']
print('PASS actual export decoded: pending/expiry/drain association, source-wide cleanup, bytes, and output before downstream Drum mapping; physical MIDI unobserved')

spec=importlib.util.spec_from_file_location('midi_decode','drum-midi-probe-decode.py')
decoder=importlib.util.module_from_spec(spec);spec.loader.exec_module(decoder)
base=json.load(open(sys.argv[2]))
on=next(r for r in base['records'] if r['site']=='on_bytes')
sink=next(r for r in base['records'] if r['site']=='on_sink')
def trial(*rows):
    capture=copy.deepcopy(base);capture['records']=[copy.deepcopy(row) for row in rows]
    for n,row in enumerate(capture['records'],1):row['sequence']=n
    return capture
def uncertain(capture,expected,unpaired,field=None):
    result=decoder.decode(capture);event=result['outputs'][0]
    assert result['capture_complete'] and not result['correlation_complete']
    assert event['association']==expected and event['bytes'] is None and event['constructor_sequence'] is None
    assert result['constructed_without_sink']==unpaired
    if field:assert field in event['mismatch_fields']
    return result
positive=decoder.decode(trial(on,sink))
assert positive['correlation_complete'] and positive['outputs'][0]['bytes']==[145,72,100]
duplicate=trial(on,on,sink);duplicate['records'][0]['payload'][1]=60
result=uncertain(duplicate,'ambiguous',[1,2])
assert result['outputs'][0]['candidate_constructor_sequences']==[1,2]
for field in ('route_key','epoch','root'):
    mismatch=trial(on,sink)
    if field=='route_key':mismatch['records'][0]['payload'][5]^=1
    else:mismatch['records'][0][field]+=1
    uncertain(mismatch,'mismatch',[1],field)
gap=trial(on,sink);gap['records'][1]['sequence']=3
assert uncertain(gap,'mismatch',[1],'sequence')['sequence_gaps']==[[2,2]]
uncertain(trial(sink,on),'unmatched',[2])
for sequences in ((2,1),(1,1)):
    invalid=trial(on,sink)
    for row,n in zip(invalid['records'],sequences):row['sequence']=n
    try:decoder.decode(invalid)
    except ValueError:pass
    else:raise AssertionError('out-of-order or duplicate sequence accepted')
pending=next(r for r in base['records'] if r['site']=='pending')
due=next(r for r in base['records'] if r['site']=='due')
assert pending['payload'][4]!=due['payload'][4] and pending['payload'][14]!=due['payload'][14]
assert decoder.pending_key(pending)==decoder.pending_key(due)
for field in (1,3,5,11,13,14,15,17,18,19,20,21,22):
    mismatch=trial(pending,due);mismatch['records'][1]['payload'][field]^=1
    assert decoder.decode(mismatch)['duration_expiry'][0]['association']=='unmatched'
ambiguous=decoder.decode(trial(pending,pending,due))
assert not ambiguous['correlation_complete'] and ambiguous['duration_expiry'][0]['association']=='ambiguous'
assert ambiguous['duration_expiry'][0]['candidate_pending_sequences']==[1,2]
drain=copy.deepcopy(due);drain['site']='drain'
consumed=decoder.decode(trial(pending,due,drain))
assert consumed['duration_expiry'][0]['association']=='unique'
assert consumed['pending_drain'][0]['association']=='unmatched' and not consumed['correlation_complete']
foreign=trial(pending,drain);foreign['records'][1]['epoch']+=1
assert decoder.decode(foreign)['pending_drain'][0]['association']=='unmatched'
cleanup=next(r for r in base['records'] if r['site']=='cleanup')
cleanup_end=next(r for r in base['records'] if r['site']=='cleanup_end')
foreign=trial(cleanup,cleanup_end);foreign['records'][1]['epoch']+=1
assert not decoder.decode(foreign)['correlation_complete']
print('PASS decoder negatives: duplicate constructors retained, route/epoch/root/sequence mismatches withhold bytes, pending duplicates stay ambiguous, release identities consumed once, cleanup epoch mismatch stays incomplete')

PY
printf 'Retained fixture output: %s\n' "$out"
