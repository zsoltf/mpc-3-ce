/* Both command requests and physical intents use these native owner phases.
 * This module owns no recorder state and publishes no command mailbox. */
static unsigned sampler_prepare_job(const CommandRequest *r,TransportJob *job,unsigned *started){
 uint32_t recorder=0,mode=0;unsigned why=sampler_probe_current(job->root,&recorder,&mode);
 if(!why)why=sampler_probe_admit(job->operation,recorder,mode);
 if(why||recorder!=job->responder)return why?why:C_SOURCE_CHANGED;
#ifdef MODEL_SAMPLER_FEATURE
 sampler_lifecycle_prepare(recorder,mode);
#else
 sampler_probe_listener(recorder,mode,1);
#endif
 job->sampler_mode=mode;
#if defined(MODEL_SAMPLER_BACKEND_TRIAL) && defined(MODEL_FIRMWARE_310)
 if(job->operation==SAMPLER_ARM){
  if(!*(const volatile unsigned char*)(uintptr_t)(recorder+0x574u)){
   *started=job->seq;
#ifdef MODEL_SAMPLER_FEATURE
   sampler_lifecycle_enable(recorder);
#else
   sampler_probe_backend_enable(recorder);
#endif
  }
  uint32_t next_recorder=0,next_mode=0,target=0,function=0,holder=0,editor=0;
  why=transport_current(r,&target,&function,&holder,&editor);
  if(!why)why=sampler_probe_current(job->root,&next_recorder,&next_mode);
  if(why||target!=job->responder||function!=job->function||holder!=job->holder||editor!=job->editor||next_recorder!=recorder||next_mode!=mode)return why?why:C_SOURCE_CHANGED;
  if(!sampler_probe_backend_ready(recorder))return C_NOT_READY;
 }
#else
 (void)r;(void)started;
#endif
#ifdef MODEL_SAMPLER_FEATURE
 job->sampler_generation=atomic_load_explicit(&sampler_generation,memory_order_acquire);
#endif
 return 0;
}
static unsigned sampler_execute_job(const TransportJob *job,unsigned immediate){
 uint32_t recorder=0,mode=0;
 if(!transport_active(job)||sampler_probe_current(job->root,&recorder,&mode)||recorder!=job->responder||mode!=job->sampler_mode)return 0;
 unsigned admission=sampler_probe_admit_context(job->operation,recorder,mode,0u);
#ifdef MODEL_SAMPLER_FEATURE
 if(job->sampler_generation!=atomic_load_explicit(&sampler_generation,memory_order_acquire))admission=C_SOURCE_CHANGED;
#endif
#if defined(MODEL_SAMPLER_BACKEND_TRIAL) && defined(MODEL_FIRMWARE_310)
 if(!admission&&job->operation==SAMPLER_ARM&&!sampler_probe_backend_ready(recorder))admission=C_NOT_READY;
#endif
 unsigned result=admission?0:sampler_probe_call(job->operation,recorder);
 if(immediate&&result&&job->operation==SAMPLER_ARM&&transport_active(job)){
  /* Auto threshold may already have started. Never start twice or convert
   * this captured Start intent into a Stop because the state changed. */
  unsigned state=word(recorder+0x264u);
  if((state==1u||state==2u)&&!sampler_probe_admit_context(SAMPLER_START,recorder,mode,0u)){
   sampler_probe_call(SAMPLER_START,recorder);state=word(recorder+0x264u);
  }
  result=state==3u||state==5u;
  /* An immediate Start that could only arm must not strand a CE recording.
   * Cancel through the same native audio primitive, retaining completion. */
  if(!result&&(state==1u||state==2u)&&!sampler_probe_admit_context(SAMPLER_STOP,recorder,mode,0u))sampler_probe_call(SAMPLER_STOP,recorder);
 }
#ifdef MODEL_SAMPLER_FEATURE
 if((result&&job->operation==SAMPLER_STOP)||(job->operation==SAMPLER_ARM&&(!result||!word(recorder+0x264u))))sampler_lifecycle_release();
#endif
 return result;
}
