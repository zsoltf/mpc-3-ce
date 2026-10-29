#ifndef MPC_MIDI_MUTE_STATE_H
#define MPC_MIDI_MUTE_STATE_H
#define MIDI_MUTE_CAPACITY 8192u
/* Output accounting only. Native Instrument effective mute owns mute state. */
typedef struct {
 _Atomic uint32_t error,epoch,active,retained,high_water,emitted,blocked,terminated,released;
 _Atomic uint32_t unaccounted,max_lookup,pending,slot_bytes,storage_bytes,route_lost,ordering_lost;
} MidiMuteState;
enum {MM_OK,MM_CAPACITY,MM_IDENTITY,MM_NESTING,MM_ASSOCIATION,MM_LANE,MM_WRAP,MM_NATIVE,MM_ROUTE_CHANGED};
#endif
