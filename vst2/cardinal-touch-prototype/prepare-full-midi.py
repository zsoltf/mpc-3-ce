#!/usr/bin/env python3
"""Repair reproduced MIDI/transport faults in their pinned Cardinal/DPF owners."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
root = parser.parse_args().source


def edit(name, replacements):
    path = root / name
    source = path.read_text()
    for old, new in replacements:
        if source.count(old) != 1:
            raise SystemExit(f'Pinned MIDI source mismatch: {path}: {old[:80]}')
        source = source.replace(old, new)
    path.write_text(source)


# VST PPQ counts quarter notes. A bar may be a fraction of one quarter note.
edit('dpf/distrho/src/DistrhoPluginVST2.cpp', [
    ('const int    ppqPerBar = vstTimeInfo->timeSigNumerator * 4 / vstTimeInfo->timeSigDenominator;',
     'const double ppqPerBar = vstTimeInfo->timeSigNumerator * 4.0 / vstTimeInfo->timeSigDenominator;'),
    ('static_cast<int32_t>(ppqPos) / ppqPerBar + 1;',
     'static_cast<int32_t>(std::floor(ppqPos / ppqPerBar)) + 1;'),
])

# Drain each block's events at their sample, before the host replaces the
# borrowed event array. Retain the existing 32-sample parameter/filter cadence.
edit('plugins/Cardinal/src/HostMIDI-Map.cpp', [
    ('if (bypassed || !divider.process())', 'if (bypassed)'),
    ('        ++midiEventFrame;\n\n        // Step channels',
     '        ++midiEventFrame;\n\n        if (!divider.process())\n            return;\n\n        // Step channels'),
])

# Tracker consumes this generic Rack queue; HostMIDI has its own queue.
edit('src/CardinalCommon.cpp', [
    ('if (frame > internal->midiEvents->frame)',
     'if (frame < internal->midiEvents->frame)'),
])

# Bar numbers are one-based at both the callback and interior crossing paths.
edit('plugins/Cardinal/src/HostTime.cpp', [
    ('if (timeInfo.beat == 1)',
     'if (timeInfo.beat == 1 && (timeInfo.bar - 1) % barDivision == 0)'),
    ('if (timeInfo.bar % barDivision == 1)',
     'if ((timeInfo.bar - 1) % barDivision == 0)'),
    ('((float)((timeInfo.bar - 1) % barDivision) + (timeInfo.beat - 1) + beatPhase)',
     '((float)((timeInfo.bar - 1) % barDivision) * pcontext->beatsPerBar + (timeInfo.beat - 1) + beatPhase)'),
])

# BBT ticks count denominator-note beats, while tempo counts quarter notes.
# Keep the existing ticksPerClock subdivision and its callback anchor intact.
edit('src/CardinalPlugin.cpp', [
    ('/ timePos.bbt.ticksPerBeat;',
     '/ timePos.bbt.ticksPerBeat\n                                            * 4.0 / timePos.bbt.beatType;'),
])
print('Repaired pinned VST PPQ, MIDI-map drain, Rack queue ordering and HostTime bar/phase/interpolation')
