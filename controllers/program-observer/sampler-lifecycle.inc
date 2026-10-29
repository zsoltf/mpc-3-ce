/* Resource ownership only. Native AudioRecorder owns every recording state.
 * Source hooks publish atomic lifecycle facts; only the qualified UI service
 * calls native enable/disable or changes the native completion listener. */
#if defined(MODEL_SAMPLER_FEATURE) && defined(MODEL_FIRMWARE_310)
enum {SL_ENTER=340,SL_EXIT,SL_ENABLE_DONE,SL_DISABLE_DONE,SL_PUBLISH,SL_PUBLISH_RESULT,SL_SETTLED,SL_STATE,SL_BIRTH,
 SL_DEFERRED_ENTER,SL_DEFERRED_DONE,SL_MODAL_ENTER,SL_MODAL_DONE};
static _Atomic uint32_t sampler_engine,sampler_recorder,sampler_mode_owner,sampler_engine_mode;
static _Atomic uint32_t sampler_pending,sampler_exit_pending,sampler_deferred,sampler_modal_depth,sampler_lifetime_fault,sampler_generation;
static _Atomic uint32_t sampler_owned_backend,sampler_owned_listener,sampler_release_requested;
static _Atomic uint32_t sampler_wait_completion,sampler_lease_epoch,sampler_lease_editor;
static _Atomic unsigned sampler_disabling;
static void sampler_count_add(_Atomic uint32_t *count){
 if(atomic_fetch_add_explicit(count,1,memory_order_acq_rel)==UINT32_MAX)atomic_store(&sampler_lifetime_fault,1);
}
static void sampler_count_drop(_Atomic uint32_t *count){
 if(!atomic_fetch_sub_explicit(count,1,memory_order_acq_rel))atomic_store(&sampler_lifetime_fault,1);
}
static void sampler_native_birth(uint32_t engine){
 uint32_t recorder=0,mode=0;
 if(sampler_probe_current(engine,&recorder,&mode))return;
 if(atomic_load(&sampler_recorder)){atomic_store(&sampler_lifetime_fault,1);return;}
 atomic_store(&sampler_pending,0);atomic_store(&sampler_exit_pending,0);atomic_store(&sampler_deferred,0);atomic_store(&sampler_modal_depth,0);
 atomic_store(&sampler_owned_backend,0);atomic_store(&sampler_owned_listener,0);atomic_store(&sampler_release_requested,0);sampler_disabling=0;
 atomic_store(&sampler_wait_completion,0);
 atomic_store(&sampler_generation,1);atomic_store(&sampler_lifetime_fault,0);atomic_store(&sampler_engine,engine);atomic_store(&sampler_mode_owner,mode);
 atomic_store(&sampler_engine_mode,word(mode+0x1f8u));
 atomic_store_explicit(&sampler_recorder,recorder,memory_order_release);
 atomic_store_explicit(&sampler_source_dirty,1,memory_order_release);
}
static void sampler_native_death(uint32_t engine){
 if(engine!=atomic_load(&sampler_engine))return;
 atomic_store_explicit(&sampler_recorder,0,memory_order_release);atomic_store(&sampler_engine,0);atomic_store(&sampler_mode_owner,0);atomic_store(&sampler_engine_mode,0);
 atomic_store(&sampler_owned_backend,0);atomic_store(&sampler_owned_listener,0);atomic_store(&sampler_release_requested,0);sampler_disabling=0;
 atomic_store_explicit(&sampler_source_dirty,1,memory_order_release);
}
static int sampler_native_point(Context *c,unsigned site){
 if(site<SL_ENTER||site>SL_MODAL_DONE)return 0;
 if(site==SL_BIRTH){sampler_native_birth(c->r[4]);return 1;}
 uint32_t recorder=atomic_load_explicit(&sampler_recorder,memory_order_acquire),mode=atomic_load(&sampler_mode_owner);
 if(!recorder)return 1;
 if(site==SL_ENTER||site==SL_EXIT){
  if(c->r[0]!=mode)return 1;
  if(site==SL_ENTER){atomic_store(&sampler_owned_backend,0);atomic_store(&sampler_owned_listener,0);atomic_store(&sampler_release_requested,0);atomic_store(&sampler_wait_completion,0);atomic_store(&sampler_disabling,0);}
  else sampler_count_add(&sampler_exit_pending);
 }else if(site==SL_ENABLE_DONE||site==SL_DISABLE_DONE){
  /* The native retained capture is still live at this return boundary. */
  if(!pointer(c->r[4])||c->r[4]>UINT32_MAX-8u||word(c->r[4]+4u)!=mode)return 1;
  if(site==SL_DISABLE_DONE)sampler_count_drop(&sampler_exit_pending);
 }else if(site==SL_STATE){if(c->r[5]!=recorder+0x23cu)return 1;
  if(c->r[7]&&(atomic_load(&sampler_owned_backend)||atomic_load(&sampler_owned_listener)))atomic_store(&sampler_wait_completion,1);
 }else if(site==SL_DEFERRED_ENTER){if(c->r[0]!=mode)return 1;sampler_count_add(&sampler_deferred);
 }else if(site==SL_DEFERRED_DONE){if(c->r[5]!=mode)return 1;sampler_count_drop(&sampler_deferred);
 }else if(site==SL_MODAL_ENTER||site==SL_MODAL_DONE){
  if(c->r[site==SL_MODAL_ENTER?0:5]!=atomic_load(&sampler_engine_mode))return 1;
  if(site==SL_MODAL_ENTER)sampler_count_add(&sampler_modal_depth);else sampler_count_drop(&sampler_modal_depth);
 }else{
  if(c->r[4]!=recorder)return 1;
  if(site==SL_PUBLISH)sampler_count_add(&sampler_pending);
  else if(site==SL_PUBLISH_RESULT){if(!c->r[0]){sampler_count_drop(&sampler_pending);atomic_store(&sampler_lifetime_fault,1);}}
  else if(site==SL_SETTLED){sampler_count_drop(&sampler_pending);if(!atomic_load(&sampler_pending))atomic_store(&sampler_wait_completion,0);if(atomic_load(&sampler_owned_backend)||atomic_load(&sampler_owned_listener))atomic_store(&sampler_release_requested,1);}
 }
 if(site!=SL_STATE&&atomic_fetch_add_explicit(&sampler_generation,1,memory_order_acq_rel)==UINT32_MAX)atomic_store(&sampler_lifetime_fault,1);
 atomic_store_explicit(&sampler_source_dirty,1,memory_order_release);return 1;
}
static unsigned sampler_lifecycle_admit(unsigned op,uint32_t recorder,uint32_t mode,unsigned ui){
 if(atomic_load(&sampler_lifetime_fault))return C_SOURCE_FAILURE;
 if((ui&&atomic_load(&sampler_engine)!=root)||atomic_load(&sampler_recorder)!=recorder||atomic_load(&sampler_mode_owner)!=mode)return C_NOT_READY;
 if(sampler_disabling||atomic_load(&sampler_pending)||atomic_load(&sampler_exit_pending))return COMMAND_WAIT_HEARTBEAT;
 if(op==SAMPLER_ARM){
  if(atomic_load(&sampler_release_requested)||atomic_load(&sampler_deferred)||atomic_load(&sampler_modal_depth))return COMMAND_WAIT_HEARTBEAT;
  /* Native vectors and deferred-object linkage belong to UI. Audio validates
   * the generation captured after this admission, never these containers. */
  if(!ui)return 0;
  uint32_t engine_mode=word(mode+0x1f8u);
  if(engine_mode!=atomic_load(&sampler_engine_mode)||!typed(engine_mode,MODEL_SAMPLER_ENGINE_MODE_VPTR)||engine_mode>UINT32_MAX-0xa0u)return C_NATIVE_MEMBERSHIP;
  uint32_t begin=word(engine_mode+0x98u),end=word(engine_mode+0x9cu);
  if(begin>end||(begin&3u)||(end&3u)||(end-begin)%0x14u||(end-begin)/0x14u>32u||(!begin&&end)||(begin&&!pointer(begin)))return C_NATIVE_MEMBERSHIP;
  for(uint32_t entry=begin;entry<end;entry+=0x14u){unsigned id=word(entry);if(id==0x3082u||id==0x3084u)return COMMAND_WAIT_HEARTBEAT;}
  /* The native delayed action can be replaced/canceled by page activity.
   * Read its current native registration instead of accumulating CE jobs. */
  uint32_t action=word(mode+0x854u);
  if(action){
   if(!pointer(action)||action>UINT32_MAX-0x18u||word(action+4u)!=word(mode+0x234u)||word(action+8u)!=mode||
      word(action+0x10u)!=observer_image_bias+MODEL_SAMPLER_ACTION_MANAGER||word(action+0x14u)!=observer_image_bias+MODEL_SAMPLER_ACTION_INVOKER)return C_NATIVE_MEMBERSHIP;
   if(word(action))return COMMAND_WAIT_HEARTBEAT;
  }
 }
 return 0;
}
static void sampler_lifecycle_service(void){
 unsigned owner_changed=(atomic_load(&sampler_owned_backend)||atomic_load(&sampler_owned_listener))&&(atomic_load(&sampler_lease_epoch)!=epoch||atomic_load(&sampler_lease_editor)!=atomic_load(&mirror_state->general.editor_owner));
 if(owner_changed)atomic_store(&sampler_release_requested,1);
 if(!atomic_load(&sampler_release_requested)&&!atomic_load(&sampler_disabling))return;
 if(atomic_load(&sampler_lifetime_fault)||atomic_load(&sampler_pending)||atomic_load(&sampler_exit_pending))return;
 uint32_t recorder=0,mode=0;
 if(root!=atomic_load(&sampler_engine)||sampler_probe_current(root,&recorder,&mode)||recorder!=atomic_load(&sampler_recorder)||mode!=atomic_load(&sampler_mode_owner))return;
 /* Native page adoption owns subsequent teardown. Never disable its backend. */
 if(*(const volatile unsigned char*)(uintptr_t)(mode+0x1fu)||*(const volatile unsigned char*)(uintptr_t)(mode+0x20u))return;
 if(owner_changed&&!atomic_load(&sampler_disabling)){
  /* An offscreen CE recording can borrow an already-enabled backend. Project
   * retirement must terminate that recording too, through native disable. */
  atomic_store(&sampler_owned_backend,0);atomic_store(&sampler_disabling,1);sampler_probe_backend_set(recorder,0u);sampler_native_changed();return;
 }
 if(atomic_load(&sampler_wait_completion))return;
 if(word(recorder+0x264u))return;
 if(atomic_exchange(&sampler_owned_listener,0))sampler_probe_listener(recorder,mode,0);
 if(atomic_exchange(&sampler_owned_backend,0)&&*(const volatile unsigned char*)(uintptr_t)(recorder+0x574u)){
  atomic_store(&sampler_disabling,1);
  sampler_probe_backend_set(recorder,0u);
  sampler_native_changed();
  /* Native disable can queue one additional empty completion. That callback
   * sees enabled0 and cannot recreate the backend; wait for its own receipt. */
  if(atomic_load(&sampler_pending))return;
 }
 if(atomic_load(&sampler_disabling)&&*(const volatile unsigned char*)(uintptr_t)(recorder+0x574u)){
  atomic_store(&sampler_lifetime_fault,1);return;
 }
 atomic_store(&sampler_disabling,0);atomic_store(&sampler_release_requested,0);
}
static void sampler_lifecycle_prepare(uint32_t recorder,uint32_t mode){
 if(*(const volatile unsigned char*)(uintptr_t)(mode+0x1fu))return;
 if(!atomic_load(&sampler_owned_backend)&&!atomic_load(&sampler_owned_listener)){
  atomic_store(&sampler_lease_epoch,epoch);atomic_store(&sampler_lease_editor,atomic_load(&mirror_state->general.editor_owner));
 }
 sampler_probe_listener(recorder,mode,1);atomic_store(&sampler_owned_listener,1);
}
static void sampler_lifecycle_enable(uint32_t recorder){
 atomic_store(&sampler_owned_backend,1);sampler_probe_backend_enable(recorder);sampler_native_changed();
}
static void sampler_lifecycle_release(void){atomic_store_explicit(&sampler_release_requested,1,memory_order_release);}
#else
static int sampler_native_point(Context *c,unsigned site){(void)c;(void)site;return 0;}
static void sampler_native_death(uint32_t engine){(void)engine;}
static void sampler_lifecycle_service(void){}
#endif
