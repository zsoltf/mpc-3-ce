#ifndef MPCLEARN_CARDINAL_FULL_TOUCH_API_H
#define MPCLEARN_CARDINAL_FULL_TOUCH_API_H
#include <stdint.h>
#include "editor-view.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MPC_CARDINAL_TOUCH_ABI 6u
typedef struct {
    uint64_t token;
    char label[256];
    uint32_t modules, cables;
} MpcCardinalInstance;
#define MPC_CARDINAL_HOST_PARAMETERS 24u
typedef struct {
    uint64_t binding;
    uint32_t count;
    MpcEditorParameter parameters[MPC_CARDINAL_HOST_PARAMETERS];
} MpcCardinalHostParameters;
enum MpcCardinalHostPage { MPC_CARDINAL_HOST_OPEN=1, MPC_CARDINAL_HOST_READ,
    MPC_CARDINAL_HOST_CLOSE };
typedef struct {
    uint32_t abi, size;
    /* A returned token identifies a live plug-in, never an owning pointer. */
    uint32_t (*instances)(MpcCardinalInstance*, uint32_t capacity);
    int (*open)(uint64_t token, uint32_t width, uint32_t height);
    int (*pointer)(uint64_t token, uint32_t event, float x, float y);
    int (*scroll)(uint64_t token, float x, float y, float dx, float dy);
    int (*text)(uint64_t token, uint32_t codepoint);
    int (*action)(uint64_t token, uint32_t action, const char* path);
    int (*pixels)(uint64_t token, unsigned char*, uint32_t bytes, uint32_t* revision);
    void (*close)(uint64_t token);
    void (*catalog)(uint32_t* plugins, uint32_t* models);
    uint64_t (*bind)(const void* effect);
    int (*view)(uint64_t,uint32_t,float,float,float);
    uint32_t (*items)(MpcEditorItem*,uint32_t);
    int (*create)(uint64_t,uint32_t,float,float);
    int (*viewport)(uint64_t,MpcEditorViewport*);
    /* Current Rack control label/display value, or patch/selection caption. */
    int (*status)(uint64_t,char*,uint32_t);
    int (*parameters)(uint64_t,MpcEditorParameters*);
    int (*parameter)(uint64_t,uint64_t,uint32_t,uint32_t,float);
    /* Called at visible paint; refreshes scene state before the next render. */
    int (*advance)(uint64_t);
    int (*host_parameters)(uint64_t,uint32_t,MpcCardinalHostParameters*);
    int (*host_parameter)(uint64_t,uint64_t,uint32_t,uint32_t,float);
} MpcCardinalTouchApi;
enum MpcCardinalAction {
    MPC_CARDINAL_BROWSER=1, MPC_CARDINAL_SAVE, MPC_CARDINAL_LOAD,
    MPC_CARDINAL_UNDO, MPC_CARDINAL_REDO, MPC_CARDINAL_DELETE,
    MPC_CARDINAL_ZOOM_IN, MPC_CARDINAL_ZOOM_OUT, MPC_CARDINAL_REMOVE_CABLE,
    MPC_CARDINAL_MODULE_OPTIONS
};
/* Exported by the loaded plug-in; asking for this API never instantiates it. */
const MpcCardinalTouchApi* mpclearn_cardinal_touch_api(uint32_t abi);
#ifdef __cplusplus
}
#endif
#endif
