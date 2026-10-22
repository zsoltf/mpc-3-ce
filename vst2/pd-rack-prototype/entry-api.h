#ifndef MPCLEARN_PATCH_EDITOR_ENTRY_H
#define MPCLEARN_PATCH_EDITOR_ENTRY_H
#include "../include/mpclearn-ui.h"
#include "editor-view.h"
/* Presentation composition only. The track adapter supplies its exact live
 * AEffect and admitted loaded module; binding never creates an engine. */
struct MpcEditorEntryApi {
    void* (*create)(MpcUiIntentCallback,void*);
    const MpcUiAppCallbacks* callbacks;
    int (*visible)(void*,int);
    int (*close)(void*);
    void (*destroy)(void*);
    int (*title)(void*,const char*);
    int (*bind)(void*,const void*,const char*);
    int (*parameters)(void*,MpcEditorParameters*);
    int (*parameter)(void*,uint64_t,uint32_t,uint32_t,float);
    void (*present)(void*);
    /* Existing owning UI drain; optional visible-only presentation work. */
    void (*idle)(void*);
};
const MpcEditorEntryApi* mpclearn_pd_editor_entry();
const MpcEditorEntryApi* mpclearn_cardinal_editor_entry();
#endif
