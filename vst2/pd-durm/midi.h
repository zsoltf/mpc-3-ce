#ifndef MPCLEARN_INSTRUMENT_MIDI_H
#define MPCLEARN_INSTRUMENT_MIDI_H
#include <stddef.h>
#include <stdint.h>
/* Independent VST2 MIDI layout, matching pinned DPF/JUCE; see README.md. */
typedef struct {
    int32_t type, byte_size, delta, flags, note_length, note_offset;
    uint8_t data[4];
    int8_t detune;
    uint8_t off_velocity, reserved[2];
} MidiEvent;
typedef struct { int32_t count; void *reserved; const MidiEvent *events[]; } MidiEvents;
_Static_assert(sizeof(MidiEvent)==32,"VstMidiEvent size");
_Static_assert(offsetof(MidiEvent,delta)==8,"event sample offset");
_Static_assert(offsetof(MidiEvent,data)==24,"MIDI data offset");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(MidiEvents,events)==8,"ARM32 event list layout");
#endif
#endif
