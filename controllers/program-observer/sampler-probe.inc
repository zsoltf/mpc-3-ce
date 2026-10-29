static _Atomic uint32_t sampler_source_dirty;
static void sampler_native_changed(void){atomic_store_explicit(&sampler_source_dirty,1,memory_order_release);}
#if defined(MODEL_SAMPLER_FEATURE) && defined(MODEL_FIRMWARE_310)
static unsigned sampler_lifecycle_admit(unsigned,uint32_t,uint32_t,unsigned);
#endif
/* Exact 3.10 native owner path. Production adds observed resource lifecycle;
 * explicit diagnostic profiles retain their separately bounded trial behavior.
 * The persistent engine owns both recorder and SamplerMode. */
static unsigned sampler_probe_current(uint32_t engine,uint32_t *recorder,uint32_t *mode){
#if defined(MODEL_SAMPLER_PROBE) && defined(MODEL_FIRMWARE_310)
 if(!typed(engine,MODEL_ROOT_VPTR)||engine>UINT32_MAX-0x60cu)return C_NATIVE_MEMBERSHIP;
 uint32_t audio=word(engine+MODEL_ROOT_AUDIO);
 if(!typed(audio,MODEL_AUDIO_VPTR)||audio>UINT32_MAX-0x3dcu)return C_NATIVE_MEMBERSHIP;
 uint32_t rec=word(audio+0x3d8u),m=word(engine+0x608u);
 if(!typed(rec,MODEL_SAMPLING_RECORDER_VPTR)||rec>UINT32_MAX-0x578u||
    !typed(rec+0x20u,MODEL_SAMPLING_RECORDER_SECONDARY_VPTR)||
    !typed(rec+0x28u,MODEL_SAMPLING_RECORDER_THIRD_VPTR)||
    !typed(m,MODEL_SAMPLER_MODE_VPTR)||m>UINT32_MAX-0x858u||
    !typed(m+0x1ecu,MODEL_SAMPLER_LISTENER_VPTR)||word(m+0x1f4u)!=rec)return C_NATIVE_MEMBERSHIP;
 if(*(const volatile unsigned char*)(uintptr_t)(rec+0x574u)>1u||word(rec+0x488u)>1u||word(rec+0x264u)>5u||*(const volatile unsigned char*)(uintptr_t)(rec+0x290u)>1u||
    *(const volatile unsigned char*)(uintptr_t)(m+0x1fu)>1u||*(const volatile unsigned char*)(uintptr_t)(m+0x20u)>1u)return C_SOURCE_FAILURE;
 *recorder=rec;*mode=m;return 0;
#else
 (void)engine;(void)recorder;(void)mode;return C_NOT_READY;
#endif
}
static void sampler_source_publish(void){
 if(!atomic_exchange_explicit(&sampler_source_dirty,0,memory_order_acq_rel))return;
 SamplerState *out=&mirror_state->general.sampler;
 unsigned revision=atomic_load_explicit(&out->revision,memory_order_relaxed);
 if(revision>UINT32_MAX-2u)return;
 uint32_t recorder=0,mode=0;
 unsigned available=ready&&!loading&&!depth&&!reset_pending&&root&&project&&
  !sampler_probe_current(root,&recorder,&mode);
 atomic_store_explicit(&out->revision,revision+1u,memory_order_relaxed);
 atomic_thread_fence(memory_order_release);
 atomic_store(&out->epoch,epoch);atomic_store(&out->project_owner,channel_owner_id(project));
 atomic_store(&out->editor_owner,atomic_load(&mirror_state->general.editor_owner));
 atomic_store(&out->tick,atomic_load(&mirror_state->heartbeat));
 atomic_store(&out->available,available);atomic_store(&out->recorder,recorder);atomic_store(&out->mode,mode);
 atomic_store(&out->state,available?word(recorder+0x264u):0u);
 atomic_store(&out->ui_active,available?*(const volatile unsigned char*)(uintptr_t)(mode+0x1fu):0u);
 atomic_store(&out->audio_active,available?*(const volatile unsigned char*)(uintptr_t)(mode+0x20u):0u);
 atomic_store(&out->recording,available?*(const volatile unsigned char*)(uintptr_t)(recorder+0x290u):0u);
 atomic_store(&out->backend_enabled,available?*(const volatile unsigned char*)(uintptr_t)(recorder+0x574u):0u);
 atomic_store_explicit(&out->revision,revision+2u,memory_order_release);
}
static void sampler_probe_snapshot(const TransportJob *job,uint32_t mode){
 /* STATE.capture=SamplerMode, counter=latest sample pointer (not dereferenced),
  * program=AudioRecorder, bits[7:0]=native state, bit8=active Sampler page,
  * bit9=native recording bool, bit10=native audio-mode active.
  * arg_kind bit0=backend selector, bit8=backend enabled;
  * property=selected backend object (opaque).
  * Receipt means observation, not finalization. */
 uint32_t recorder=job->responder,backend=word(recorder+0x488u);
 unsigned bits=word(recorder+0x264u)|
  ((unsigned)*(const volatile unsigned char*)(uintptr_t)(mode+0x1fu)<<8)|
  ((unsigned)*(const volatile unsigned char*)(uintptr_t)(recorder+0x290u)<<9)|
  ((unsigned)*(const volatile unsigned char*)(uintptr_t)(mode+0x20u)<<10);
 event(0,CE_GLOBAL_STATE,job->seq,mode,word(recorder+0x420u),recorder,backend|((unsigned)*(const volatile unsigned char*)(uintptr_t)(recorder+0x574u)<<8),job->operation,bits,word(recorder+0x490u+backend*8u),job->epoch);
}

/* Native UI phase owns listener changes. Calls are enabled only after exact
 * preparation of both the recorder ABI and the native mode/UI dispatch path. */
#if defined(COMMAND_COMPONENT) && defined(MODEL_SAMPLER_CALLS) && defined(MODEL_FIRMWARE_310)
static void (*component_sampler_listener)(uint32_t,uint32_t,unsigned);
static unsigned (*component_sampler_call)(unsigned,uint32_t);
#endif
/* Backend changes run on UI. Production releases its resource lease through
 * the native lifecycle; explicit flag3 retains the original enable-only trial. */
#if defined(MODEL_SAMPLER_BACKEND_TRIAL) && defined(MODEL_FIRMWARE_310)
#ifdef COMMAND_COMPONENT
static void (*component_sampler_backend_enable)(uint32_t,unsigned);
#endif
static void sampler_probe_backend_set(uint32_t recorder,unsigned enabled){
#ifdef COMMAND_COMPONENT
 component_sampler_backend_enable(recorder,enabled);
#else
 ((void (*)(void*,unsigned))(uintptr_t)(observer_image_bias+MODEL_SAMPLER_BACKEND_ENABLE))((void*)(uintptr_t)recorder,enabled);
#endif
}
static void sampler_probe_backend_enable(uint32_t recorder){sampler_probe_backend_set(recorder,1u);}
static unsigned sampler_probe_backend_ready(uint32_t recorder){
 unsigned selector=word(recorder+0x488u);
 return *(const volatile unsigned char*)(uintptr_t)(recorder+0x574u)==1u&&selector<=1u&&pointer(word(recorder+0x490u+selector*8u));
}
#endif
static unsigned sampler_probe_admit_context(unsigned op,uint32_t recorder,uint32_t mode,unsigned ui){
#if defined(MODEL_SAMPLER_CALLS) && defined(MODEL_FIRMWARE_310)
#if defined(MODEL_SAMPLER_FEATURE)
 if(*(const volatile unsigned char*)(uintptr_t)(mode+0x1fu)!=*(const volatile unsigned char*)(uintptr_t)(mode+0x20u))return C_NOT_READY;
 unsigned lifecycle=sampler_lifecycle_admit(op,recorder,mode,ui);if(lifecycle)return lifecycle;
#else
 (void)ui;
 if(*(const volatile unsigned char*)(uintptr_t)(mode+0x1fu)||*(const volatile unsigned char*)(uintptr_t)(mode+0x20u))return C_NOT_READY;
#endif
 unsigned state=word(recorder+0x264u);
 if(op==SAMPLER_ARM){
#ifdef MODEL_SAMPLER_FEATURE
  /* Slot7 queues native page enable later. Both activity flags alone do not
   * prove that UI callback ran; only an offscreen Arm may acquire a CE lease. */
  if(*(const volatile unsigned char*)(uintptr_t)(mode+0x1fu)&&!sampler_probe_backend_ready(recorder))return COMMAND_WAIT_HEARTBEAT;
#endif
  return state==0?0:C_NOT_READY;
 }
 if(op==SAMPLER_START)return state==1||state==2?0:C_NOT_READY;
 if(op==SAMPLER_STOP)return state==1||state==2||state==3||state==5?0:C_NOT_READY;
 return C_FORMAT;
#else
 (void)op;(void)recorder;(void)mode;(void)ui;return C_NOT_READY;
#endif
}
static unsigned sampler_probe_admit(unsigned op,uint32_t recorder,uint32_t mode){return sampler_probe_admit_context(op,recorder,mode,1u);}
static void sampler_probe_listener(uint32_t,uint32_t,unsigned);
#include "sampler-lifecycle.inc"
static void sampler_probe_listener(uint32_t recorder,uint32_t mode,unsigned add){
#if defined(MODEL_SAMPLER_CALLS) && defined(MODEL_FIRMWARE_310)
#ifdef COMMAND_COMPONENT
 component_sampler_listener(recorder+4u,mode+0x1ecu,add);
#else
 if(add)((void (*)(void*,void*,unsigned))(uintptr_t)(observer_image_bias+MODEL_SAMPLER_LISTENER_ADD))((void*)(uintptr_t)(recorder+4u),(void*)(uintptr_t)(mode+0x1ecu),1u);
 else ((void (*)(void*,void*))(uintptr_t)(observer_image_bias+MODEL_SAMPLER_LISTENER_REMOVE))((void*)(uintptr_t)(recorder+4u),(void*)(uintptr_t)(mode+0x1ecu));
#endif
#else
 (void)recorder;(void)mode;(void)add;
#endif
}
static unsigned sampler_probe_call(unsigned op,uint32_t recorder){
#if defined(MODEL_SAMPLER_CALLS) && defined(MODEL_FIRMWARE_310)
#ifdef COMMAND_COMPONENT
 unsigned result=component_sampler_call(op,recorder);
 return op==SAMPLER_START?1u|((result!=0)<<1):1u;
#else
 void *r=(void*)(uintptr_t)recorder;
 if(op==SAMPLER_ARM)((void (*)(void*))(uintptr_t)(observer_image_bias+MODEL_SAMPLER_ARM))(r);
 else if(op==SAMPLER_START){unsigned result=((unsigned (*)(void*,unsigned,unsigned))(uintptr_t)(observer_image_bias+MODEL_SAMPLER_START))(r,0u,0u);return 1u|((result!=0)<<1);}
 else if(op==SAMPLER_STOP)((void (*)(void*,unsigned))(uintptr_t)(observer_image_bias+MODEL_SAMPLER_STOP))(r,0u);
 return 1u;
#endif
#else
 (void)op;(void)recorder;return 0;
#endif
}
static int sampler_probe_receipted(unsigned at){
 /* processed acquire in command_retire sees the admission pin. Read native
  * queue lifetime first; cleanup pending stays set across its transition to0. */
 return !atomic_load_explicit(transport_live+at,memory_order_seq_cst)&&!atomic_load_explicit(sampler_cleanup_pending+at,memory_order_acquire);
}
static void sampler_probe_cleanup(void){
 if(!atomic_exchange_explicit(&sampler_cleanup_any,0,memory_order_acq_rel))return;
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  unsigned seq=atomic_load_explicit(sampler_cleanup_ready+at,memory_order_acquire);if(!seq)continue;
#ifndef MODEL_SAMPLER_FEATURE
  const TransportJob *job=transport_jobs+at;
  /* Never follow an old owner after epoch/editor retirement. Its native
   * listener contains no CE pointer and stays with the native mode lifetime.
   * Close may prevent this UI pass altogether; no detached cleanup is claimed. */
  if(atomic_load_explicit(sampler_cleanup_remove+at,memory_order_acquire)&&job->seq==seq&&root==job->root&&transport_active(job)){
   uint32_t recorder=0,mode=0;
   if(!sampler_probe_current(job->root,&recorder,&mode)&&recorder==job->responder&&mode==job->sampler_mode&&
      !word(recorder+0x264u)&&!*(const volatile unsigned char*)(uintptr_t)(mode+0x1fu)&&!*(const volatile unsigned char*)(uintptr_t)(mode+0x20u))
    sampler_probe_listener(recorder,mode,0);
   /* Recording completion may reset state before delivering its listener.
    * Only an observed cancel permits removal;
    * successful arm/start/recording-stop and failed enqueue retain native ownership. */
  }
#else
  if(atomic_load(&command_state->slots[at].dispatched)!=seq&&native_started[at]==seq){
   if(atomic_load(&sampler_release_requested)||atomic_load(&sampler_disabling)){
    atomic_store_explicit(&sampler_cleanup_any,1,memory_order_release);continue;
   }
   native_started[at]=0; /* Partial enable was released before refusal retires. */
  }
#endif
  atomic_store_explicit(sampler_cleanup_ready+at,0,memory_order_release);
  atomic_store_explicit(sampler_cleanup_pending+at,0,memory_order_release); /* Last old-job access: retirement/reuse may now proceed. */
 }
}
