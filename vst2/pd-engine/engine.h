#ifndef PD_DURM_ENGINE_H
#define PD_DURM_ENGINE_H
#include <stdint.h>
#include "z_libpd.h"
#define PARAMS 19
#define HELD 28
#define STATE_BYTES 236
extern const char *const control_names[PARAMS];
extern const char *const receivers[PARAMS];
extern const float defaults[PARAMS];
typedef struct {
    float controls[PARAMS], held[HELD];
} State;
typedef struct {
    t_pdinstance *pd;
    void *patch, *bindings[HELD];
    State state;
    unsigned errors;
    int rate, loading;
} Engine;
void engine_global_init(void);
int engine_open(Engine *, const char *directory, int rate, const State *);
void engine_close(Engine *);
void engine_control(Engine *, unsigned, float);
void engine_trigger(Engine *, unsigned note);
void engine_panic(Engine *);
void engine_tick(Engine *, float stereo[128]);
void state_encode(const State *, unsigned char out[STATE_BYTES]);
int state_decode(State *, const unsigned char *, unsigned length);
#endif
