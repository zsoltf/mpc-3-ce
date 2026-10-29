/* Opt-in diagnostic tail of the existing CommandState. One capture per process.
 * Rows never recycle; a released sequence makes the full fixed row readable. */
#ifndef MPC_DRUM_MIDI_PROBE_STATE_H
#define MPC_DRUM_MIDI_PROBE_STATE_H
#define DMP_RECORDS 1024u
#define DMP_WORDS 31u
#define DMP_FRAMES 16u
enum {DMP_ROOT=309,DMP_ROOT_END,DMP_MAP,DMP_MAP_END,DMP_SEND,DMP_SEND_END,
 DMP_ON_BYTES,DMP_ON_SINK,DMP_OFF_BYTES,DMP_OFF_SINK,DMP_PENDING,DMP_DUE,
 DMP_DUE_END,DMP_OFF_ROOT,DMP_OFF_END,DMP_MUTE,
 DMP_DRAIN,DMP_DRAIN_END,DMP_CLEANUP,DMP_CLEANUP_END};
enum {DMP_OK,DMP_FULL,DMP_WRITER,DMP_UNENROLLED,DMP_REENTRY,DMP_NESTING,
 DMP_IDENTITY,DMP_EPOCH,DMP_UNASSOCIATED};
typedef struct {_Atomic uint32_t sequence,words[DMP_WORDS];} DrumMidiRecord;
typedef struct {
 _Atomic uint32_t started,until,count,error,bias;
 _Atomic uint32_t open[COMMAND_LANES],active[COMMAND_LANES];
 DrumMidiRecord records[DMP_RECORDS];
} DrumMidiProbe;
_Static_assert(sizeof(DrumMidiRecord)==128,"MIDI probe fixed row bound");
#endif
