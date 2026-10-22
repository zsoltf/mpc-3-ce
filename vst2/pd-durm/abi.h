#ifndef MPCLEARN_INSTRUMENT_ABI_H
#define MPCLEARN_INSTRUMENT_ABI_H
/* Minimal independent VST2 layout. All pointer-sized callback arguments are
 * intptr_t, including audioMaster's value argument on ARM32. */
#include <stdint.h>
#include <stddef.h>
typedef struct Effect Effect;
typedef intptr_t (*Host)(Effect *, int32_t, int32_t, intptr_t, void *, float);
typedef intptr_t (*Dispatch)(Effect *, int32_t, int32_t, intptr_t, void *, float);
typedef void (*Process)(Effect *, const float *const *, float **, int32_t);
struct Effect {
    int32_t magic;
    Dispatch dispatch;
    Process process;
    void (*set)(Effect *, uint32_t, float);
    float (*get)(Effect *, uint32_t);
    int32_t programs, params, inputs, outputs, flags;
    void *reserved[2];
    int32_t delay, reserved_int[2];
    float reserved_float;
    void *object, *user;
    int32_t uid, version;
    Process replacing;
    void (*double_replacing)(Effect *, const double *const *, double **, int32_t);
    uint8_t future[56];
};
typedef Effect *(*Factory)(Host);
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(Effect) == 144, "AEffect size");
_Static_assert(offsetof(Effect, dispatch) == 4, "dispatcher layout");
_Static_assert(offsetof(Effect, set) == 12, "parameter layout");
_Static_assert(offsetof(Effect, object) == 64, "object layout");
_Static_assert(offsetof(Effect, uid) == 72, "identity layout");
_Static_assert(offsetof(Effect, replacing) == 80, "replacing layout");
#endif
#endif
