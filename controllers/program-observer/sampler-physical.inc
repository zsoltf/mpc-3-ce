/* One native physical producer -> existing qualified UI/audio service. The
 * handoff stores intent/identity only; AudioRecorder remains the state owner. */
enum {SP_INPUT=370};
#ifdef MODEL_SAMPLER_PHYSICAL
#ifdef COMMAND_COMPONENT
static void (*component_sampler_feedback)(unsigned);
#endif
#define SP_ERASE 79u
#define SP_UNUSED 180u
static _Atomic uint32_t physical_controls,physical_ingress,physical_edge;
static uint32_t physical_hardware,physical_connection,physical_edge_controls;
static struct {uint32_t epoch,project_owner,editor_owner,operation;} physical_intent;
static _Atomic uint32_t physical_pending,physical_flight,physical_editor,physical_result,physical_feedback;
static TransportJob physical_job;
static unsigned physical_sequence,physical_partial,physical_follow;
static void sampler_physical_initialize(void){
 atomic_store(&physical_controls,0);atomic_store(&physical_ingress,0);atomic_store(&physical_edge,0);
 atomic_store(&physical_pending,0);atomic_store(&physical_flight,0);atomic_store(&physical_editor,0);
 atomic_store(&physical_result,0);atomic_store(&physical_feedback,0);physical_sequence=physical_partial=physical_follow=0;
 physical_hardware=physical_connection=physical_edge_controls=0;memset(&physical_job,0,sizeof(physical_job));
}
static int sampler_physical_busy(void){return atomic_load_explicit(&physical_flight,memory_order_acquire)!=0;}
static int sampler_physical_quiescent(void){return !atomic_load_explicit(&physical_ingress,memory_order_seq_cst)&&atomic_load_explicit(&physical_flight,memory_order_acquire)!=2u&&atomic_load_explicit(&physical_flight,memory_order_acquire)!=5u&&atomic_load(&physical_edge)!=2u;}
static void sampler_physical_retired(uint32_t editor){
 if(atomic_load_explicit(&physical_editor,memory_order_acquire)!=editor)return;
 uint32_t expected=2;
 if(atomic_compare_exchange_strong_explicit(&physical_flight,&expected,5,memory_order_acq_rel,memory_order_acquire)){atomic_store(&physical_result,0);atomic_store_explicit(&physical_flight,3,memory_order_release);}
}
static void sampler_physical_input(Context *c){
 if(c->r[1]!=SP_ERASE)return;
 uint32_t expected=0;
 if(!atomic_compare_exchange_strong_explicit(&physical_ingress,&expected,1,memory_order_seq_cst,memory_order_seq_cst))return;
 uint32_t h=c->r[0],connection=c->r[12],controls=atomic_load_explicit(&physical_controls,memory_order_acquire);
 if(!typed(h,MODEL_SAMPLER_HARDWARE_VPTR)||!pointer(connection)||connection>UINT32_MAX-0x2cu)goto done;
 uint32_t current=word(connection+0x28u);
 if(h!=physical_hardware||connection!=physical_connection||current!=physical_edge_controls){
  physical_hardware=h;physical_connection=connection;physical_edge_controls=current;atomic_store(&physical_edge,0);
 }
 unsigned down=c->r[2]==0x90u&&c->r[3]!=0;
 if(c->r[2]!=0x90u&&c->r[2]!=0x80u)goto done;
 unsigned edge=atomic_load(&physical_edge);
 /* An owned release stays owned across project reset, unavailable state and
  * Shift-first release. Closure waits for it before unloading this hook. */
 if(edge==2u){c->r[1]=SP_UNUSED;if(!down)atomic_store(&physical_edge,0);goto done;}
 if(!atomic_load_explicit(&observer_running,memory_order_seq_cst)||!controls||controls!=current||controls>UINT32_MAX-0x3a24u)goto done;
 if(!down){atomic_store(&physical_edge,0);goto done;}
 if(edge==1u)goto done;
 atomic_store(&physical_edge,1);
 if(word(controls+0x28u)!=13u||!*(const volatile unsigned char*)(uintptr_t)(controls+0xd84u)||*(const volatile unsigned char*)(uintptr_t)(controls+0x3a20u))goto done;
 atomic_store(&physical_edge,2);c->r[1]=SP_UNUSED;
 SamplerCopy state;
 if(!sampler_copy_read(&mirror_state->general.sampler,&state)||state.state==4u){atomic_store(&physical_feedback,3u);goto done;}
 expected=0;
 if(!atomic_compare_exchange_strong_explicit(&physical_pending,&expected,1,memory_order_acq_rel,memory_order_acquire)){atomic_store(&physical_feedback,3u);goto done;}
 physical_intent.epoch=state.epoch;physical_intent.project_owner=state.project_owner;physical_intent.editor_owner=state.editor_owner;
 physical_intent.operation=state.state==3u||state.state==5u?SAMPLER_STOP:SAMPLER_START;
 atomic_store_explicit(&physical_pending,2,memory_order_release);
done:atomic_store_explicit(&physical_ingress,0,memory_order_seq_cst);
}
static void sampler_physical_audio(void *opaque){
 TransportJob job=*(TransportJob*)opaque;unsigned result=0;
 if(atomic_load_explicit(&physical_flight,memory_order_acquire)!=2u||!transport_active(&job))goto done;
 unsigned lane=trace_enter(0x134);if(lane==COMMAND_LANES)goto done;trace_leave(lane);
 result=sampler_execute_job(&job,1u);
 if(result&&job.operation!=SAMPLER_STOP){
  uint32_t rec=0,mode=0;
  result=transport_active(&job)&&!sampler_probe_current(job.root,&rec,&mode)&&rec==job.responder&&mode==job.sampler_mode&&(word(rec+0x264u)==3u||word(rec+0x264u)==5u);
 }
 atomic_store_explicit(&sampler_source_dirty,1,memory_order_release);
done:
 if(!result)sampler_lifecycle_release();
 atomic_store(&physical_result,result?job.operation:0u);
 atomic_store_explicit(&physical_flight,3,memory_order_release); /* last job access */
}
static unsigned sampler_physical_commands_busy(void){
 for(unsigned at=0;at<COMMAND_SLOTS;at++){
  CommandSlot *s=command_state->slots+at;unsigned seq=atomic_load_explicit(&s->published,memory_order_acquire);CommandRequest r;
  if(!seq||atomic_load(&s->reclaimed)==seq)continue;
  if(!command_request_read(s,seq,&r))return 1;
  if(command_audio_global(r.reserved)&&(atomic_load(&s->processed)!=seq||atomic_load(active_request+at)||atomic_load(sampler_cleanup_pending+at)))return 1;
 }
 return 0;
}
static void sampler_physical_notify(unsigned message){
#ifdef COMMAND_COMPONENT
 if(component_sampler_feedback)component_sampler_feedback(message);
#elif defined(NATIVE_PREFERENCES)
 uint32_t tmi=word(root+MODEL_ROOT_TMI);
 if(!typed(tmi,MODEL_TMI_VPTR)||tmi>UINT32_MAX-0x3724u||word(tmi+0x124u)!=root||word(tmi+0x128u)!=word(root+MODEL_ROOT_PAGES))return;
 uint32_t overlay=word(tmi+0x3720u);
 if(!typed(overlay,MODEL_SAMPLER_BANNER_VPTR)||overlay>UINT32_MAX-0xa4u)return;
 native_sampler_banner(overlay,observer_image_bias,command_owner,message);
#else
 (void)message;
#endif
}
static void sampler_physical_service(Context *c){
 unsigned usable=active()&&!atomic_load(&enrollment_fault)&&!atomic_load(&command_violation)&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error)&&ready&&!loading&&!depth&&!reset_pending&&root&&project&&
  !atomic_load(&command_state->stop_requested)&&!atomic_load(&command_state->new_project_intent)&&
  c->r[0]==command_queues+4u&&!word(c->r[0]+0xf0u);
 uint32_t controls=0;
 /* Root+0x1fc owns a non-polymorphic input table. Its first member is the
  * raw Controls written by Acvx; +0x18 is a separate processed Controls view.
  * Native constructor0x104a980 stores raw Controls at0x104a9d4. */
 if(usable){uint32_t inputs=word(root+0x1fcu);if(pointer(inputs)&&inputs<=UINT32_MAX-4u){controls=word(inputs);if(!pointer(controls)||controls>UINT32_MAX-0x3a24u)controls=0;}}
 atomic_store_explicit(&physical_controls,controls,memory_order_release);
 unsigned flight=atomic_load_explicit(&physical_flight,memory_order_acquire);
 if(flight==3u){
  unsigned result=atomic_load(&physical_result);
  if(!result){sampler_lifecycle_release();physical_partial=atomic_load(&sampler_owned_backend)||atomic_load(&sampler_owned_listener);atomic_store(&physical_feedback,3u);}
  else if(result==SAMPLER_STOP)physical_follow=2u;
  else {physical_follow=1u;atomic_store(&physical_feedback,1u);}
  atomic_store_explicit(&physical_flight,4,memory_order_release);flight=4;
 }
 if(flight==4u){
  /* A partial UI enable stays pinned until its normal native cleanup. */
  if(!physical_partial||(!atomic_load(&sampler_release_requested)&&!atomic_load(&sampler_disabling))){physical_partial=0;atomic_store_explicit(&physical_flight,0,memory_order_release);flight=0;}
 }
 if(physical_follow&&usable){
  uint32_t rec=0,mode=0;
  if(physical_job.epoch!=epoch||!transport_active(&physical_job))physical_follow=0;
  else if(!sampler_probe_current(root,&rec,&mode)&&rec==physical_job.responder&&!word(rec+0x264u)&&!atomic_load(&sampler_pending)&&!atomic_load(&sampler_wait_completion)&&!atomic_load(&sampler_disabling)){
   atomic_store(&physical_feedback,2u);physical_follow=0;
  }
 }
 unsigned feedback=atomic_exchange(&physical_feedback,0);if(feedback&&usable)sampler_physical_notify(feedback);
 if(flight||atomic_load_explicit(&physical_pending,memory_order_acquire)!=2u||sampler_physical_commands_busy())return;
 uint32_t expected=2;if(!atomic_compare_exchange_strong_explicit(&physical_pending,&expected,3,memory_order_acq_rel,memory_order_acquire))return;
 unsigned why=0;CommandRequest r={.epoch=physical_intent.epoch,.project_owner=physical_intent.project_owner,.global_owner=physical_intent.editor_owner,.reserved=physical_intent.operation};
 if(!usable||r.epoch!=epoch){why=C_NOT_READY;goto refused;}
 uint32_t rec=0,mode=0;why=sampler_probe_current(root,&rec,&mode);if(why)goto refused;
 unsigned state=word(rec+0x264u);
 if(r.reserved==SAMPLER_START){if(!state)r.reserved=SAMPLER_ARM;else if(state!=1u&&state!=2u){why=C_NOT_READY;goto refused;}}
 uint32_t target=0,function=0,holder=0,editor=0;why=transport_current(&r,&target,&function,&holder,&editor);if(why)goto refused;
 if(physical_sequence==UINT32_MAX){why=C_SOURCE_FAILURE;goto refused;}
 r.seq=++physical_sequence;
 physical_job=(TransportJob){r.seq,0,r.epoch,root,project,r.project_owner,editor,r.global_owner,target,function,holder,r.reserved,0,0};
 physical_partial=0;atomic_store(&physical_result,0);atomic_store(&physical_flight,1);
 why=sampler_prepare_job(&r,&physical_job,&physical_partial);
 if(why){sampler_lifecycle_release();atomic_store(&physical_flight,4);goto refused;}
 /* Successful admission transfers partial enable ownership to native lifecycle. */
 physical_partial=0;atomic_store_explicit(&physical_editor,editor,memory_order_release);atomic_store_explicit(&physical_flight,2,memory_order_release);
 if(!transport_queue_submit(observer_image_bias,holder,&physical_job,sampler_physical_audio)){
  sampler_lifecycle_release();physical_partial=1;atomic_store(&physical_flight,4);why=C_NOT_READY;goto refused;
 }
 atomic_store_explicit(&physical_pending,0,memory_order_release);return;
refused:
 (void)why;atomic_store_explicit(&physical_pending,0,memory_order_release);if(usable)sampler_physical_notify(3u);
}
#else
static void sampler_physical_initialize(void){}
static int sampler_physical_busy(void){return 0;}
static int sampler_physical_quiescent(void){return 1;}
static void sampler_physical_retired(uint32_t editor){(void)editor;}
static void sampler_physical_input(Context *c){(void)c;}
static void sampler_physical_service(Context *c){(void)c;}
#endif
