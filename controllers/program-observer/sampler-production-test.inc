/* Included in general_checks with rooted native-object substitutes. The real
 * command service, retained queue, source hooks and consumer remain in use. */
{
 unsigned char sampler_engine_object[0x100] __attribute__((aligned(8)))={0};
 unsigned char sampler_deferred_object[0x18] __attribute__((aligned(8)))={0};
 uint32_t sampler_modal_row[5]={0};
 uint32_t engine_mode=ptr(sampler_engine_object),action=ptr(sampler_deferred_object);
 put(engine_mode,MODEL_SAMPLER_ENGINE_MODE_VPTR);put(sampler_mode+0x1f8,engine_mode);
 put(sampler_rec+0x264,0);put(sampler_rec+0x488,0);put(sampler_rec+0x490,0);put(sampler_rec+0x498,0);
 sampler_objects[0x574]=sampler_objects[0x290]=sampler_objects[0x61f]=sampler_objects[0x620]=0;
 sampler_fixture_recorder=sampler_rec;sampler_fixture_mode=sampler_mode;sampler_fixture_force_idle=sampler_backend_fault=0;
 component_sampler_listener=sampler_listener_fixture;component_sampler_call=sampler_call_fixture;component_sampler_backend_enable=sampler_backend_enable_fixture;
 Context native=context();native.r[4]=root;call(&native,SL_BIRTH);
 require(atomic_load(&sampler_recorder)==sampler_rec&&!sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode),"native construction establishes exact recorder lifecycle before first command");
#define SAMPLER_COMPLETE() do {Context completed=context();put(sampler_rec+0x264,0);sampler_objects[0x290]=0;completed.r[4]=sampler_rec;call(&completed,SL_SETTLED);} while(0)
#define SAMPLER_SETTLE(at_,id_,op_) do { \
 sampler_cleanup_fixture();command_retire();atomic_fetch_add(&fixture->heartbeat,1); \
 MIRROR_COPY(settled);require(copy_mirror(fixture,&settled),"production sampler consumer snapshot"); \
 MirrorInput reader={.commands=command_state};MirrorBank bank;bank_init(&bank);bank_apply(&bank,&settled,settled.heartbeat); \
 reader.flights[at_]=(InputFlight){.flight=id_,.field=op_,.expires=120000,.global_owner=channel_owner_id(e),.global_epoch=epoch}; \
 input_drain(&reader,&bank,&settled,settled.heartbeat);require(!reader.error&&reader.settled==1,"production sampler consumer settles call receipt without claiming sample completion"); \
 command_retire();require(command_idle(command_state),"sampler command slots retire independently of resource lease"); \
 } while(0)
#define SAMPLER_RUN(op_) do { \
 CommandRequest request={.epoch=epoch,.expires=120000,.reserved=op_,.project_owner=channel_owner_id(project),.global_owner=channel_owner_id(e)}; \
 unsigned at_=submit_fixture(request),id_=atomic_load(&command_state->published); \
 require(!atomic_load(&command_state->slots[at_].rejected)&&transport_queue_component_pending(),"production sampler command reaches retained audio queue"); \
 pthread_t audio_;require(!pthread_create(&audio_,NULL,transport_queue_runner,NULL)&&!pthread_join(audio_,NULL),"sampler method runs on existing audio consumer"); \
 SAMPLER_SETTLE(at_,id_,op_); \
 } while(0)
 sampler_objects[0x61f]=sampler_objects[0x620]=1;
 require(sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode)==COMMAND_WAIT_HEARTBEAT,"active native page waits for its own queued backend enable");
 sampler_objects[0x61f]=sampler_objects[0x620]=0;
 SAMPLER_RUN(SAMPLER_ARM);
 require(word(sampler_rec+0x264)==1&&sampler_objects[0x574]&&atomic_load(&sampler_owned_backend)&&atomic_load(&sampler_wait_completion),"Arm acquires backend once and retains completion ownership");
 SAMPLER_RUN(SAMPLER_START);
 MIRROR_COPY(projection);require(copy_mirror(fixture,&projection)&&projection.sampler_available&&projection.sampler.state==3&&projection.sampler.backend_enabled,"native source projection reaches actual mirror consumer");
 SAMPLER_RUN(SAMPLER_STOP);
 require(word(sampler_rec+0x264)==4&&atomic_load(&sampler_pending)==1&&sampler_objects[0x574],"Stop return cannot disable during native finalization");
 put(sampler_rec+0x264,0);sampler_objects[0x290]=0;sampler_cleanup_fixture();
 require(sampler_objects[0x574]&&sampler_fixture_listener_present,"idle before native UI completion cannot release listener/backend");
 SAMPLER_COMPLETE();sampler_cleanup_fixture();
 require(!sampler_objects[0x574]&&atomic_load(&sampler_pending)==1&&atomic_load(&sampler_disabling)&&!sampler_fixture_listener_present,"settled finish disables once and waits native disable's second empty completion");
 require(sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode)==COMMAND_WAIT_HEARTBEAT,"re-arm waits second native callback");
 SAMPLER_COMPLETE();sampler_cleanup_fixture();
 require(!atomic_load(&sampler_disabling)&&!atomic_load(&sampler_release_requested)&&!sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode),"settled cleanup makes next Arm available");
 /* Native delayed-action replacement and modal close are read through their
  * actual owner. Empty vector alone cannot admit reentrant decision work. */
 put(sampler_mode+0x234,engine_mode);put(action+4,engine_mode);put(action+8,sampler_mode);put(action+0x10,MODEL_SAMPLER_ACTION_MANAGER);put(action+0x14,MODEL_SAMPLER_ACTION_INVOKER);put(action,0x12345000);put(sampler_mode+0x854,action);
 require(sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode)==COMMAND_WAIT_HEARTBEAT,"native pending delayed action blocks Arm");
 put(action+4,sampler_mode+0x234);require(sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode)==C_NATIVE_MEMBERSHIP,"deferred prerequisite is the stored native owner, not its pointer field address");put(action+4,engine_mode);
 put(action,0);native=context();native.r[0]=sampler_mode;call(&native,SL_DEFERRED_ENTER);
 require(sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode)==COMMAND_WAIT_HEARTBEAT,"delivered action remains guarded while executing");
 native.r[5]=sampler_mode;call(&native,SL_DEFERRED_DONE);put(sampler_mode+0x854,0);
 sampler_modal_row[0]=0x3082;put(engine_mode+0x98,ptr(sampler_modal_row));put(engine_mode+0x9c,ptr(sampler_modal_row)+20);
 require(sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode)==COMMAND_WAIT_HEARTBEAT,"native Keep/Discard modal blocks Arm");
 native=context();native.r[0]=engine_mode;call(&native,SL_MODAL_ENTER);put(engine_mode+0x9c,ptr(sampler_modal_row));
 require(sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode)==COMMAND_WAIT_HEARTBEAT,"modal removal before decision callback cannot admit nested Arm");
 native.r[5]=engine_mode;call(&native,SL_MODAL_DONE);require(!sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode),"decision return permits subsequent Arm");
 /* The old request cannot retire in the callback-return gap. */
 CommandRequest request={.epoch=epoch,.expires=120000,.reserved=SAMPLER_ARM,.project_owner=channel_owner_id(project),.global_owner=channel_owner_id(e)};
 unsigned at=submit_fixture(request),id=atomic_load(&command_state->published);
 atomic_store(&pause_at,21);atomic_store(&pause_reached,0);atomic_store(&pause_resume,0);
 require(!pthread_create(&sampler_fixture_audio,NULL,transport_queue_runner,NULL),"production sampler retirement gap consumer");
 while(!atomic_load_explicit(&pause_reached,memory_order_acquire))sched_yield();
 command_retire();require(atomic_load(&command_state->slots[at].sealed)!=id&&command_free(command_state)!=at,"production callback gap stays pinned against retirement and reuse");
 sampler_finish_after_cleanup();SAMPLER_SETTLE(at,id,SAMPLER_ARM);
 SAMPLER_RUN(SAMPLER_STOP);require(atomic_load(&sampler_pending)==1&&sampler_objects[0x574],"armed cancellation waits native UI completion too");
 SAMPLER_COMPLETE();sampler_cleanup_fixture();SAMPLER_COMPLETE();sampler_cleanup_fixture();
 /* Native page entry adopts resources; CE never disables an active page. */
 SAMPLER_RUN(SAMPLER_ARM);sampler_objects[0x61f]=1;native=context();native.r[0]=sampler_mode;call(&native,SL_ENTER);sampler_objects[0x620]=1;sampler_cleanup_fixture();
 require(!atomic_load(&sampler_owned_backend)&&!atomic_load(&sampler_owned_listener)&&sampler_objects[0x574],"native page adopts backend and completion listener");
 sampler_objects[0x61f]=0;native.r[0]=sampler_mode;call(&native,SL_EXIT);sampler_objects[0x620]=0;
 require(sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode)==COMMAND_WAIT_HEARTBEAT,"audio inactive does not bypass pending native exit-disable");
 native.r[4]=sampler_rec;call(&native,SL_PUBLISH);put(sampler_rec+0x264,0);sampler_objects[0x290]=0;sampler_objects[0x574]=0;put(sampler_rec+0x490,0);sampler_fixture_listener_present=0;
 uint32_t capture[2]={0,sampler_mode};native.r[4]=ptr(capture);call(&native,SL_DISABLE_DONE);
 require(sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode)==COMMAND_WAIT_HEARTBEAT,"disable return still waits older native completion delivery");
 SAMPLER_COMPLETE();sampler_cleanup_fixture();require(!sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode),"native exit and completion both settled");
 /* Epoch change retires only the CE resource lease through native disable.
  * Old callback accounting survives the project epoch change. */
 SAMPLER_RUN(SAMPLER_ARM);unsigned old_epoch=epoch;epoch++;atomic_store(&fixture->epoch,epoch);sampler_cleanup_fixture();
 require(!sampler_objects[0x574]&&atomic_load(&sampler_pending)==1&&sampler_fixture_listener_present,"project change stops/cancels through native disable and retains completion listener");
 SAMPLER_COMPLETE();sampler_cleanup_fixture();require(!atomic_load(&sampler_release_requested)&&!sampler_fixture_listener_present,"project-change completion releases old resource lease");epoch=old_epoch;atomic_store(&fixture->epoch,epoch);
 /* A CE recording may borrow an existing inactive backend. Project change
  * still terminates its recording, although normal finish preserves the borrow. */
 sampler_objects[0x574]=1;put(sampler_rec+0x490,0x12345000);SAMPLER_RUN(SAMPLER_ARM);
 require(!atomic_load(&sampler_owned_backend)&&atomic_load(&sampler_owned_listener),"offscreen Arm distinguishes a borrowed native backend");
 epoch++;atomic_store(&fixture->epoch,epoch);sampler_cleanup_fixture();
 require(!sampler_objects[0x574]&&atomic_load(&sampler_pending)==1&&sampler_fixture_listener_present,"project change also cancels CE recording on a borrowed backend");
 SAMPLER_COMPLETE();sampler_cleanup_fixture();require(!atomic_load(&sampler_disabling)&&!sampler_fixture_listener_present,"borrowed-backend project cancellation releases once after delivery");epoch=old_epoch;atomic_store(&fixture->epoch,epoch);
 /* UI admission owns native modal/deferred reads. A later native lifecycle
  * change cancels the queued Arm; audio must not dereference those UI fields. */
 unsigned stale_calls=sampler_fixture_calls;
 at=submit_fixture(request);id=atomic_load(&command_state->published);
 native=context();native.r[0]=sampler_mode;call(&native,SL_DEFERRED_ENTER);native.r[5]=sampler_mode;call(&native,SL_DEFERRED_DONE);
 put(sampler_mode+0x854,0xfffffffcu);
 pthread_t stale_audio;require(!pthread_create(&stale_audio,NULL,transport_queue_runner,NULL)&&!pthread_join(stale_audio,NULL),"stale lifecycle audio callback does not read native UI object");
 put(sampler_mode+0x854,0);
 require(sampler_fixture_calls==stale_calls&&!word(sampler_rec+0x264)&&atomic_load(&sampler_release_requested),"intervening native lifecycle prevents stale Arm and releases unused resource lease");
 SAMPLER_SETTLE(at,id,SAMPLER_ARM);require(atomic_load(&sampler_pending)==1&&!sampler_objects[0x574],"unused backend disables on UI and awaits its empty completion");SAMPLER_COMPLETE();sampler_cleanup_fixture();
 /* Failed enable is a partial resource operation, never a successful Arm. */
 sampler_backend_fault=1;unsigned before_calls=sampler_fixture_calls;at=submit_fixture(request);id=atomic_load(&command_state->published);
 require(atomic_load(&command_state->slots[at].rejected)==C_NOT_READY&&!transport_queue_component_pending()&&sampler_fixture_calls==before_calls,"failed backend enable never queues Arm");
 sampler_cleanup_fixture();command_retire();require(command_idle(command_state)&&!atomic_load(&sampler_owned_backend)&&!sampler_fixture_listener_present,"failed enable releases native resources before rejected request retires");sampler_backend_fault=0;
#include "sampler-physical-test.inc"
 /* Rejected native completion publication is a source failure, not a
  * successful finish merely because the pending count returns to zero. */
 native=context();native.r[4]=sampler_rec;call(&native,SL_PUBLISH);native.r[0]=0;call(&native,SL_PUBLISH_RESULT);
 require(!atomic_load(&sampler_pending)&&sampler_probe_admit(SAMPLER_ARM,sampler_rec,sampler_mode)==C_SOURCE_FAILURE,"rejected native completion never reopens sampler admission");
 component_sampler_listener=NULL;component_sampler_call=NULL;component_sampler_backend_enable=NULL;
 sampler_native_death(root);
 puts("PASS production sampler lifecycle: native construction, offscreen Arm/Start/Stop and copied state, cancel/finish UI delivery, second disable completion, native page adoption/exit, delayed action/modal decision, epoch cancellation, failed enable cleanup and callback retirement gap (native objects/method bodies/queue substituted; device sample and audible quality unobserved)");
#undef SAMPLER_COMPLETE
#undef SAMPLER_SETTLE
#undef SAMPLER_RUN
}
