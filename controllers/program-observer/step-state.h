/* Native drum-step projection, copied on the existing qualified UI owner. */
#ifndef MPC_STEP_STATE_H
#define MPC_STEP_STATE_H
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#define STEP_CELLS 64u
#define STEP_VIEWS 16u
#define STEP_FRESH_MS 1000u
enum {STEP_OFF,STEP_READY,STEP_UNAVAILABLE};
typedef struct {
 uint32_t epoch,project_owner,track_owner,program_owner,serial,binding,incarnation;
 uint32_t control,sequence,program,event_owner,clip,clip_row,bind_generation,division_bits,extent[2];
 uint32_t native_clip_state,clock_available,clock_tick,clock_value[2];
} StepTarget;
typedef struct {
 StepTarget target;
 uint32_t status,tick,pad,first_step,notes[4],generation;
 uint32_t valid[2],on[4][2];
} StepProjection;
typedef struct {
 uint32_t status,tick,epoch,project_owner,track_owner,program_owner,serial,binding,incarnation;
 uint32_t root,control,context,sequence,program,pad,note,division_bits,time[4];
 uint32_t valid[2],on[2];
 uint32_t event_owner,extent[2],event_generation;
 uint32_t clock_available,clock_tick,clock_value[2],native_playhead;
 StepProjection views[STEP_VIEWS];
} StepCopy;
#define STEP_WORDS (sizeof(StepCopy)/4u)
typedef struct {_Atomic uint32_t revision,words[STEP_WORDS];} StepState;
typedef struct {_Atomic uint32_t until,epoch,revision,enabled,pad[STEP_VIEWS],first_step[STEP_VIEWS],bind_generation[STEP_VIEWS];} StepInterest;
typedef struct {uint32_t until,epoch,enabled,pad[STEP_VIEWS],first_step[STEP_VIEWS],bind_generation[STEP_VIEWS];} StepDemand;
static inline int step_demand_read(const StepInterest *s,StepDemand *out){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(rev&1)return 0;
 out->until=atomic_load_explicit(&s->until,memory_order_acquire);out->epoch=atomic_load(&s->epoch);out->enabled=atomic_load(&s->enabled);
 for(unsigned i=0;i<STEP_VIEWS;i++){out->pad[i]=atomic_load(s->pad+i);out->first_step[i]=atomic_load(s->first_step+i);out->bind_generation[i]=atomic_load(s->bind_generation+i);}
 atomic_thread_fence(memory_order_acquire);return rev==atomic_load_explicit(&s->revision,memory_order_relaxed);
}
static inline int step_copy_read(const StepState *s,StepCopy *out){
 unsigned rev=atomic_load_explicit(&s->revision,memory_order_acquire);if(!rev||(rev&1))return 0;
 uint32_t words[STEP_WORDS];for(unsigned i=0;i<STEP_WORDS;i++)words[i]=atomic_load_explicit(s->words+i,memory_order_relaxed);
 atomic_thread_fence(memory_order_acquire);if(rev!=atomic_load_explicit(&s->revision,memory_order_relaxed))return 0;
 memcpy(out,words,sizeof(*out));return 1;
}
static inline int step_cell_valid(const StepCopy *s,unsigned index){return s->status==STEP_READY&&index<STEP_CELLS&&(s->valid[index/32]&(1u<<(index%32)));}
static inline unsigned step_cell_on(const StepCopy *s,unsigned index){return (s->on[index/32]>>(index%32))&1u;}
#endif
