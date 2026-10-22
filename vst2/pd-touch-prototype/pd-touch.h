#ifndef MPCLEARN_PD_TOUCH_H
#define MPCLEARN_PD_TOUCH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define PD_TOUCH_NODES 256
#define PD_TOUCH_EDGES 1024
#define PD_TOUCH_SAMPLES 65536
typedef struct PdTouch PdTouch;
enum PdControlKind {PD_CONTROL_NONE,PD_CONTROL_MESSAGE,PD_CONTROL_BANG,PD_CONTROL_NUMBER,PD_CONTROL_SYMBOL,PD_CONTROL_LIST,PD_CONTROL_TOGGLE,PD_CONTROL_SLIDER,PD_CONTROL_RADIO};
typedef struct { uint64_t identity; int x,y,inputs,outputs,width,kind,editable,text_editable,child,unresolved,control,pixel_width,pixel_height,horizontal,steps,logarithmic,gop,hide_text,owner_parent,owner_index,clip_x,clip_y,clip_w,clip_h,label_positioned,label_x,label_y,label_size; int array,array_count,array_offset,array_edit,array_save; float array_x1,array_x2,array_y1,array_y2; float value,minimum,maximum,position; uint32_t signal_inputs,signal_outputs; char text[512],class_name[64],label[128]; } PdTouchNode;
typedef struct { int source,outlet,target,inlet; } PdTouchEdge;
typedef struct { uint64_t canvas_identity; uint32_t revision; int count,visible,edges,readonly,depth,unresolved,samples; char name[128],path[512],warning[256]; PdTouchNode node[PD_TOUCH_NODES]; PdTouchEdge edge[PD_TOUCH_EDGES]; float sample[PD_TOUCH_SAMPLES]; } PdTouchSnapshot;
PdTouch* pd_touch_open(void);
PdTouch* pd_touch_open_document(const char* source,int saved_copy);
/* Hosted owner: the MPC callback alone advances 64-frame Pd ticks. */
PdTouch* pd_touch_open_hosted(uint64_t owner,int inputs,int sample_rate);
int pd_touch_audio(PdTouch*,int sample_rate,int active);
int pd_touch_tick(PdTouch*,const float* input,float* stereo);
int pd_touch_midi(PdTouch*,unsigned status,unsigned first,unsigned second);
int pd_touch_serialize(PdTouch*,unsigned char** bytes,uint32_t* length,char origin[512]);
int pd_touch_restore(PdTouch*,const char* origin,const void* bytes,uint32_t length);
void pd_touch_close(PdTouch**);
int pd_touch_snapshot(PdTouch*,PdTouchSnapshot*);
/* Refresh only an existing selected control; the copied route and identity
 * must still resolve in Pd. Avoid copying arrays/graphs on Q-Link refresh. */
int pd_touch_control_read(PdTouch*,const PdTouchSnapshot*,int,PdTouchNode*);
int pd_touch_add(PdTouch*,int kind,int x,int y);
int pd_touch_add_text(PdTouch*,const char*,int x,int y);
enum PdCreateKind {PD_CREATE_OBJECT,PD_CREATE_MESSAGE,PD_CREATE_COMMENT};
int pd_touch_create_text(PdTouch*,int kind,const char*,int x,int y);
int pd_touch_edit_text(PdTouch*,int node,const char* text,int* resulting_node);
int pd_touch_move(PdTouch*,int node,int x,int y);
int pd_touch_delete(PdTouch*,int node);
int pd_touch_connect(PdTouch*,int source,int outlet,int target,int inlet);
int pd_touch_unwire(PdTouch*,int node);
int pd_touch_save(PdTouch*);
int pd_touch_reload(PdTouch*);
int pd_touch_recall_copy(PdTouch*);
int pd_touch_load(PdTouch*,const char* path);
int pd_touch_enter(PdTouch*,int node);
int pd_touch_parent(PdTouch*);
int pd_touch_activate(PdTouch*,int node);
int pd_touch_number(PdTouch*,int node,float value);
/* Validate the copied route/type and current live bounds, then send immediately
 * under the same lock. Refuse stale identities; never retarget by bare index.
 * The caller still refreshes its complete view after the attempted input. */
int pd_touch_number_from_snapshot(PdTouch*,const PdTouchSnapshot*,int node,float value);
int pd_touch_control_position(PdTouch*,int node,float normalized);
int pd_touch_atom_text(PdTouch*,int node,const char* value);
int pd_touch_array(PdTouch*,int node,uint32_t revision,int first,int count,const float* values);
const char* pd_touch_error(PdTouch*);
#ifdef __cplusplus
}
#endif
#endif
