#ifdef MODEL_SAMPLER_PHYSICAL
{
 static unsigned char raw_buttons[0x3a28] __attribute__((aligned(8)));
 static unsigned char hardware[0x7c8] __attribute__((aligned(8))),connection[0x30] __attribute__((aligned(8)));
 static unsigned char input_owners[0x2c] __attribute__((aligned(8))),processed_buttons[0x3a28] __attribute__((aligned(8)));
 uint32_t raw=ptr(raw_buttons),hw=ptr(hardware),conn=ptr(connection),old_inputs=word(root+0x1fc);
 memset(raw_buttons,0,sizeof(raw_buttons));memset(hardware,0,sizeof(hardware));memset(connection,0,sizeof(connection));
 put(hw,MODEL_SAMPLER_HARDWARE_VPTR);put(hw+0x7c0,conn);put(conn+0x28,raw);put(raw+0x28,13);
 /* Native0x104a980 owns raw Controls at+0 and processed Controls at+0x18.
  * Neither this table nor raw Controls has an EngineMode vptr. */
 put(root+0x1fc,ptr(input_owners));put(ptr(input_owners),raw);put(ptr(input_owners)+0x18,ptr(processed_buttons));
 /* Parked bridge has no enabled interest or snapshot/command publisher. The
  * real native UI drain must seed/read/dispatch independently of that bridge. */
 unsigned interests[]={atomic_exchange(&command_state->effects_interest.enabled,0),atomic_exchange(&command_state->qlink_interest.enabled,0),atomic_exchange(&command_state->io_interest.enabled,0),atomic_exchange(&command_state->step_interest.enabled,0),atomic_exchange(&command_state->clip_interest.enabled,0),atomic_exchange(&command_state->meter_interest.enabled,0)};
 sampler_physical_initialize();component_sampler_feedback=sampler_physical_feedback_fixture;
 sampler_native_changed();sampler_cleanup_fixture();
 require(atomic_load(&physical_controls)==raw&&atomic_load(&fixture->general.sampler.available),"native outer UI drain seeds physical controls and sampler without controller demand or bridge activity");
#define PHYSICAL_EDGE(down_,shift_,consume_) do { \
 raw_buttons[0xd84]=(shift_);Context input=context();input.r[0]=hw;input.r[1]=SP_ERASE;input.r[2]=(down_)?0x90u:0x80u;input.r[3]=(down_)?127u:0u;input.r[12]=conn; \
 call(&input,SP_INPUT);require(input.r[1]==((consume_)?SP_UNUSED:SP_ERASE)&&input.r[0]==hw&&input.r[12]==conn,"actual physical ingress preserves registers and pairs ownership"); \
 if(input.r[1]==SP_ERASE)raw_buttons[0x3a20]=(down_); \
 } while(0)
#define PHYSICAL_AUDIO() do {pthread_t a_;require(!pthread_create(&a_,NULL,transport_queue_runner,NULL)&&!pthread_join(a_,NULL),"physical sampler uses existing retained audio consumer");sampler_cleanup_fixture();} while(0)
#define PHYSICAL_COMPLETE() do {SAMPLER_COMPLETE();sampler_cleanup_fixture();if(atomic_load(&sampler_pending)){SAMPLER_COMPLETE();sampler_cleanup_fixture();}} while(0)
 unsigned pub=atomic_load(&command_state->published),calls=sampler_fixture_calls;
 PHYSICAL_EDGE(1,0,0);PHYSICAL_EDGE(1,1,0);PHYSICAL_EDGE(0,1,0);
 require(!atomic_load(&physical_pending)&&sampler_fixture_calls==calls,"Erase-first then Shift remains native including held-pad erase property");
 const unsigned occupied_models[]={12,14,16,20,0};
 for(unsigned mi=0;mi<sizeof(occupied_models)/sizeof(*occupied_models);mi++){put(raw+0x28,occupied_models[mi]);PHYSICAL_EDGE(1,1,0);PHYSICAL_EDGE(0,1,0);}
 put(raw+0x28,13);
 Context other=context();other.r[0]=hw;other.r[12]=conn;other.r[1]=41;other.r[2]=0x90;other.r[3]=127;call(&other,SP_INPUT);require(other.r[1]==41,"Play and all transport have no interception");
 PHYSICAL_EDGE(1,1,1);PHYSICAL_EDGE(1,1,1);PHYSICAL_EDGE(0,0,1);
 require(!raw_buttons[0x3a20]&&atomic_load(&physical_pending)==2,"owned Shift-first release never reaches native Erase property");
 sampler_cleanup_fixture();require(transport_queue_component_pending()&&physical_job.operation==SAMPLER_ARM,"idle immediate Start enters shared UI preparation as Arm");
 PHYSICAL_AUDIO();require(sampler_fixture_calls==calls+2&&word(sampler_rec+0x264)==3&&sampler_physical_feedback_last==1,"high threshold Arm then immediate Start in same audio flight, observed Recording feedback");
 require(atomic_load(&command_state->published)==pub,"native physical producer never writes shared command mailbox");
 /* Erase-first release order and command serialization while physical Stop is queued. */
 PHYSICAL_EDGE(1,1,1);PHYSICAL_EDGE(0,1,1);sampler_cleanup_fixture();
 CommandRequest blocked={.epoch=epoch,.expires=120000,.reserved=SAMPLER_ARM,.project_owner=channel_owner_id(project),.global_owner=channel_owner_id(e)};
 unsigned blocked_at=submit_fixture(blocked),blocked_id=atomic_load(&command_state->published);
 require(atomic_load(&command_state->slots[blocked_at].processed)!=blocked_id,"shared command waits physical audio flight");
 PHYSICAL_AUDIO();require(word(sampler_rec+0x264)==4&&sampler_physical_feedback_last==1,"Stop return is not a finished-sample notification");
 PHYSICAL_COMPLETE();require(sampler_physical_feedback_last==2,"native completion produces Stopped feedback");
 /* The waiting command may now acquire the shared owner normally. */
 sampler_cleanup_fixture();require(transport_queue_component_pending(),"existing command continues after physical completion");
 PHYSICAL_AUDIO();SAMPLER_SETTLE(blocked_at,blocked_id,SAMPLER_ARM);
 PHYSICAL_EDGE(1,1,1);PHYSICAL_EDGE(0,1,1);sampler_cleanup_fixture();calls=sampler_fixture_calls;PHYSICAL_AUDIO();
 require(sampler_fixture_calls==calls+1&&word(sampler_rec+0x264)==3,"already armed physical Start does not re-arm");
 PHYSICAL_EDGE(1,1,1);PHYSICAL_EDGE(0,0,1);sampler_cleanup_fixture();PHYSICAL_AUDIO();PHYSICAL_COMPLETE();
 /* Arm may itself cross the native threshold. */
 sampler_fixture_force_idle=2;sampler_native_changed();sampler_cleanup_fixture();calls=sampler_fixture_calls;
 PHYSICAL_EDGE(1,1,1);PHYSICAL_EDGE(0,1,1);sampler_cleanup_fixture();PHYSICAL_AUDIO();
 require(sampler_fixture_calls==calls+1&&word(sampler_rec+0x264)==3,"auto threshold Arm already records, no duplicate Start");sampler_fixture_force_idle=0;
 PHYSICAL_EDGE(1,1,1);PHYSICAL_EDGE(0,0,1);sampler_cleanup_fixture();PHYSICAL_AUDIO();PHYSICAL_COMPLETE();
 /* Failed immediate Start cancels its own armed state through native Stop. */
 sampler_fixture_force_idle=3;sampler_native_changed();sampler_cleanup_fixture();calls=sampler_fixture_calls;
 PHYSICAL_EDGE(1,1,1);PHYSICAL_EDGE(0,1,1);sampler_cleanup_fixture();PHYSICAL_AUDIO();
 require(sampler_fixture_calls==calls+3&&!word(sampler_rec+0x264)&&sampler_physical_feedback_last==3,"failed immediate Start cancels Arm and reports refusal without recording success");sampler_fixture_force_idle=0;PHYSICAL_COMPLETE();
 /* Pending intent cancellation on project change does not leak its release. */
 sampler_native_changed();sampler_cleanup_fixture();PHYSICAL_EDGE(1,1,1);calls=sampler_fixture_calls;
 epoch++;atomic_store(&fixture->epoch,epoch);sampler_cleanup_fixture();PHYSICAL_EDGE(0,0,1);
 require(!atomic_load(&physical_pending)&&!transport_queue_component_pending()&&sampler_fixture_calls==calls,"epoch change cancels intent but retains release ownership");epoch--;atomic_store(&fixture->epoch,epoch);
 sampler_native_changed();sampler_cleanup_fixture();PHYSICAL_EDGE(1,1,1);PHYSICAL_EDGE(0,1,1);sampler_cleanup_fixture();
 require(transport_queue_component_cancel(),"native Editor cancels physical preclaim flight");sampler_physical_retired(e);sampler_cleanup_fixture();sampler_cleanup_fixture();
 if(atomic_load(&sampler_pending))PHYSICAL_COMPLETE();
 require(!sampler_physical_busy()&&!atomic_load(&physical_result),"Editor retirement settles physical flight without native call");
 if(atomic_load(&sampler_pending))PHYSICAL_COMPLETE();
 /* Unavailable/finalizing consumes only this chord, never an Erase action. */
 put(sampler_rec+0x264,4);sampler_native_changed();sampler_cleanup_fixture();PHYSICAL_EDGE(1,1,1);PHYSICAL_EDGE(0,0,1);sampler_cleanup_fixture();
 require(!atomic_load(&physical_pending)&&!transport_queue_component_pending()&&sampler_physical_feedback_last==3,"finalizing refuses without Erase leakage");put(sampler_rec+0x264,0);
 atomic_store(&command_state->effects_interest.enabled,interests[0]);atomic_store(&command_state->qlink_interest.enabled,interests[1]);atomic_store(&command_state->io_interest.enabled,interests[2]);atomic_store(&command_state->step_interest.enabled,interests[3]);atomic_store(&command_state->clip_interest.enabled,interests[4]);atomic_store(&command_state->meter_interest.enabled,interests[5]);
 put(root+0x1fc,old_inputs);component_sampler_feedback=NULL;sampler_physical_initialize();
 puts("PASS physical Shift+Erase: no-controller demand/parked bridge native UI service, Live II guard, both release orders, repeated down, Erase-first native ownership, threshold immediate start, shared mailbox unchanged, command serialization, failed-start cancellation, project epoch and Editor retirement; native input dispatch continuation, objects, methods, timer UI rendering substituted");
#undef PHYSICAL_EDGE
#undef PHYSICAL_AUDIO
#undef PHYSICAL_COMPLETE
}
#endif
