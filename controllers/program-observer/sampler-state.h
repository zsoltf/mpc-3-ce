#ifndef MPC_SAMPLER_STATE_H
#define MPC_SAMPLER_STATE_H
/* A copied view of the native recorder, never a controller-owned recorder.
 * The qualified UI service publishes after native state/lifecycle events. */
typedef struct {
 _Atomic uint32_t revision,epoch,project_owner,editor_owner,recorder,mode,tick;
 _Atomic uint32_t available,state,ui_active,audio_active,recording,backend_enabled;
} SamplerState;
typedef struct {
 uint32_t revision,epoch,project_owner,editor_owner,recorder,mode,tick;
 uint32_t available,state,ui_active,audio_active,recording,backend_enabled;
} SamplerCopy;
static inline int sampler_copy_read(const SamplerState *s,SamplerCopy *out){
 for(unsigned attempt=0;attempt<2;attempt++){
  unsigned revision=atomic_load_explicit(&s->revision,memory_order_acquire);
  if(!revision||(revision&1u))continue;
#define SAMPLER_COPY_WORD(name) out->name=atomic_load_explicit(&s->name,memory_order_relaxed)
  SAMPLER_COPY_WORD(epoch);SAMPLER_COPY_WORD(project_owner);SAMPLER_COPY_WORD(editor_owner);
  SAMPLER_COPY_WORD(recorder);SAMPLER_COPY_WORD(mode);SAMPLER_COPY_WORD(tick);
  SAMPLER_COPY_WORD(available);SAMPLER_COPY_WORD(state);SAMPLER_COPY_WORD(ui_active);
  SAMPLER_COPY_WORD(audio_active);SAMPLER_COPY_WORD(recording);SAMPLER_COPY_WORD(backend_enabled);
#undef SAMPLER_COPY_WORD
  atomic_thread_fence(memory_order_acquire);
  if(revision!=atomic_load_explicit(&s->revision,memory_order_relaxed))continue;
  out->revision=revision;
  return out->available==1u&&out->state<=5u&&out->ui_active<=1u&&out->audio_active<=1u&&out->recording<=1u&&out->backend_enabled<=1u;
 }
 return 0;
}
#endif
