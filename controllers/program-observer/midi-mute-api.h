#ifndef MPC_MIDI_MUTE_API_H
#define MPC_MIDI_MUTE_API_H
#ifdef MODEL_MIDI_MUTE
static void midi_mute_initialize(void);
static void midi_mute_service(void);
static int midi_mute_quiescent(void);
static int midi_mute_is_internal(void);
static void midi_mute_editor_retired(uint32_t editor);
#else
static void midi_mute_initialize(void){}
static void midi_mute_service(void){}
static int midi_mute_quiescent(void){return 1;}
static int midi_mute_is_internal(void){return 0;}
static void midi_mute_editor_retired(uint32_t editor){(void)editor;}
#endif
#endif
