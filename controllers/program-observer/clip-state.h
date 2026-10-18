/* Native Clip Matrix projections, published by the existing UI observer. */
#ifndef MPC_CLIP_STATE_H
#define MPC_CLIP_STATE_H
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#define CLIP_VIEWS 16u
#define CLIP_SIDE 8u
#define CLIP_CELLS 64u
#define CLIP_FRESH_MS 1000u
/* MpcClipLanes' ScriptableClipLanes state enum. These are native states,
 * not optimistic predictions from a Launchpad button press. */
enum {CLIP_NO_SLOT,CLIP_EMPTY,CLIP_READY,CLIP_WILL_PLAY,CLIP_PLAYING,
 CLIP_WILL_STOP_PLAYING,CLIP_WILL_RECORD,CLIP_RECORDING,CLIP_WILL_STOP_RECORDING,
 CLIP_WILL_OVERDUB,CLIP_OVERDUBBING,CLIP_WILL_STOP_OVERDUBBING};
enum {CLIP_LAUNCH=1,CLIP_STOP,CLIP_RECORD_TOGGLE,CLIP_SCENE_LAUNCH,CLIP_SCENE_STOP};
enum {CLIP_SOURCE_OFF,CLIP_SOURCE_READY,CLIP_SOURCE_UNAVAILABLE};
typedef struct {uint32_t state,identity;} ClipCell;
typedef struct {
 uint32_t status,tick,first_column,first_row,generation;
 uint32_t track_owner[CLIP_SIDE],scene_state[CLIP_SIDE];
 ClipCell cells[CLIP_CELLS];
} ClipProjection;
typedef struct {
 uint32_t status,tick,epoch,project_owner,lanes,columns,rows;
 ClipProjection views[CLIP_VIEWS];
} ClipCopy;
#define CLIP_WORDS (sizeof(ClipCopy)/4u)
typedef struct {_Atomic uint32_t revision,words[CLIP_WORDS];} ClipState;
typedef struct {_Atomic uint32_t until,epoch,revision,enabled,column[CLIP_VIEWS],row[CLIP_VIEWS];} ClipInterest;
typedef struct {uint32_t until,epoch,enabled,column[CLIP_VIEWS],row[CLIP_VIEWS];} ClipDemand;
static inline int clip_demand_read(const ClipInterest *s,ClipDemand *out){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(rev&1)return 0;
 out->until=atomic_load_explicit(&s->until,memory_order_acquire);out->epoch=atomic_load(&s->epoch);out->enabled=atomic_load(&s->enabled);
 for(unsigned i=0;i<CLIP_VIEWS;i++){out->column[i]=atomic_load(s->column+i);out->row[i]=atomic_load(s->row+i);}
 atomic_thread_fence(memory_order_acquire);return rev==atomic_load_explicit(&s->revision,memory_order_relaxed);
}
static inline int clip_copy_read(const ClipState *s,ClipCopy *out){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(!rev||(rev&1))return 0;
 uint32_t words[CLIP_WORDS];for(unsigned i=0;i<CLIP_WORDS;i++)words[i]=atomic_load_explicit(s->words+i,memory_order_relaxed);
 atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load_explicit(&s->revision,memory_order_relaxed))return 0;
 memcpy(out,words,sizeof(*out));return 1;
}
#endif
