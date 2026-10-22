#ifndef MPCLEARN_PD_CANVAS_H
#define MPCLEARN_PD_CANVAS_H
#include <stdint.h>
#include "editor-view.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PdCanvas PdCanvas;
typedef struct PdTouch PdTouch;
typedef struct { MpcEditorViewport level[17]; uint32_t valid; } PdCanvasViewState;
void pd_canvas_view_state(PdCanvas*,PdCanvasViewState*,int restore);
int pd_canvas_viewport(PdCanvas*,MpcEditorViewport*);
int pd_canvas_parameters(PdCanvas*,MpcEditorParameters*);
int pd_canvas_parameter(PdCanvas*,uint64_t,uint32_t,uint32_t,float);
int pd_canvas_view(PdCanvas*,uint32_t intent,float x,float y,float amount);
int pd_canvas_create(PdCanvas*,int kind,const char* text,float x,float y);
/* Transfer the existing canvas text intent to a host-owned keyboard. */
int pd_canvas_text_begin(PdCanvas*,char* title,uint32_t,char* initial,uint32_t);
int pd_canvas_text_finish(PdCanvas*,int accepted,const char* text);
/* Borrow a hosted owner. Closing this canvas never closes the Pd instance.
 * Preparation/close access only view resources. Bind under the hosted owner's
 * audio lock, then restore view state using only the copied snapshot. The caller
 * keeps the owner alive and holds its view lifetime lock across these stages. */
PdCanvas* pd_canvas_prepare_attachment(uint32_t width,uint32_t height);
int pd_canvas_bind_attachment(PdCanvas*,PdTouch* owner);
PdCanvas* pd_canvas_attach(uint32_t width,uint32_t height,PdTouch* owner);
void pd_canvas_sync(PdCanvas*);
PdCanvas* pd_canvas_open(uint32_t width,uint32_t height);
PdCanvas* pd_canvas_open_document(uint32_t width,uint32_t height,const char* source,int saved_copy);
void pd_canvas_close(PdCanvas**);
void pd_canvas_pointer(PdCanvas*,uint32_t event,float x,float y);
void pd_canvas_action(PdCanvas*,uint32_t intent);
int pd_canvas_load(PdCanvas*,const char* path);
const unsigned char* pd_canvas_pixels(PdCanvas*,uint32_t* revision);
const char* pd_canvas_status(PdCanvas*);
#ifdef __cplusplus
}
#endif
#endif
