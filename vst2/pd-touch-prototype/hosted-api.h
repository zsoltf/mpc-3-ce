#ifndef MPCLEARN_PD_HOSTED_API_H
#define MPCLEARN_PD_HOSTED_API_H
#include <stdint.h>
#include "editor-view.h"
#define MPC_PD_HOSTED_ABI 6
typedef struct {
    uint64_t token,frames;
    uint32_t active,sample_rate;
    char label[128];
} MpcPdInstance;
typedef struct {
    uint32_t version,size;
    uint32_t (*instances)(MpcPdInstance*,uint32_t);
    int (*open)(uint64_t,uint32_t,uint32_t);
    int (*pointer)(uint64_t,uint32_t,float,float);
    int (*action)(uint64_t,uint32_t);
    int (*load)(uint64_t,const char*);
    /* ABI6: revision is the caller's cached buffer revision on entry. Zero
     * requests a complete first frame; reset it after open/resize/replacement.
     * Success leaves an unchanged buffer untouched, otherwise copies the full
     * frame and updates revision. ABI1..5 always copy, with output-only revision.
     * A caller must retain its buffer unchanged while retaining its revision. */
    int (*pixels)(uint64_t,unsigned char*,uint32_t,uint32_t*);
    int (*status)(uint64_t,char*,uint32_t);
    void (*close)(uint64_t);
    uint64_t (*bind)(const void* effect);
    int (*view)(uint64_t,uint32_t,float,float,float);
    uint32_t (*catalog)(MpcEditorItem*,uint32_t);
    int (*create)(uint64_t,uint32_t,float,float);
    int (*viewport)(uint64_t,MpcEditorViewport*);
    int (*text_begin)(uint64_t,char*,uint32_t,char*,uint32_t);
    int (*text_finish)(uint64_t,int,const char*);
    int (*parameters)(uint64_t,MpcEditorParameters*);
    int (*parameter)(uint64_t,uint64_t,uint32_t,uint32_t,float);
} MpcPdHostedApi;
#endif
