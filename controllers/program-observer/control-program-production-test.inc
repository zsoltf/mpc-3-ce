/* Production routing/commands with fake ALSA and copied native state. */
static void program_feed(unsigned instance,unsigned status,unsigned cc,unsigned value,MirrorState *state,ControllerRuntime *runtime){
 fake_input_reset();native_push(instance,status,cc,value);need(collection_drain(&surfaces,state,runtime),"saved-program CC traverses shared ALSA collection");
}
static void program_sysex_feed(unsigned instance,const unsigned char *bytes,unsigned length,MirrorState *state,ControllerRuntime *runtime){
 snd_seq_event_t event;snd_seq_ev_clear(&event);snd_seq_ev_set_sysex(&event,length,(void*)bytes);event.source=surfaces.surfaces[instance].full;fake_input_reset();fake_input_push(event);need(collection_drain(&surfaces,state,runtime),"MMC bytes traverse shared ALSA ingress");
}
static void controller_mmc_checks(void){
 CommandState *commands=calloc(1,sizeof(*commands));MirrorState *state=calloc(1,sizeof(*state));need(commands&&state,"MMC fixtures");MirrorBank unused;MIRROR_COPY(snapshot);reset_fixture(commands,&input,&unused,&snapshot,8);collection_fixture_source(state);shared_fixture_time(state,1000);
 snapshot.editor_owner=100;snapshot.record_mode=(CopiedField){.available=1,.owner_incarnation=101,.incarnation=102};
 McuControllerCollection saved={.count=2};const McuProfile *profiles[]={&mcu_profile_mpk249,&mcu_profile_mpk_mini3};FakeClient clients[2]={0};
 for(unsigned i=0;i<2;i++){saved.records[i]=(McuControllerPreference){.enabled=1,.input={profiles[i],i?"mini":"MPK249","Chosen",MCU_ENDPOINT_USER},.output={profiles[i],i?"mini":"MPK249","Chosen",MCU_ENDPOINT_USER}};clients[i]=(FakeClient){.name=i?"mini":"MPK249",.type=SND_SEQ_USER_CLIENT,.client=190+i,.count=2,.ports={{"Chosen",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE,1},{"Other",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE,2}}};}
 collection_init(&surfaces,&saved);surfaces.seq=(snd_seq_t*)1;surfaces.sink=1;surfaces.source=2;fake_enumeration(clients,2);fake_input_reset();need(collection_topology(&surfaces,state)&&surfaces.connected==2,"MMC uses selected profile endpoint");
 ControllerRuntime runtime={.snapshot=&snapshot,.snapshot_attempted=1,.snapshot_result=1,.now=100};MirrorInput *in=&surfaces.contexts[0].input;
 unsigned char bytes[]={0xf0,0x7f,0x7f,6,1,0xf7};const unsigned codes[]={1,2,3,6,2,6},operations[]={GLOBAL_STOP,GLOBAL_PLAY,GLOBAL_PLAY,GLOBAL_RECORD_ENABLE,GLOBAL_PLAY,GLOBAL_RECORD_ENABLE};
 for(unsigned i=0;i<6;i++){bytes[2]=i&1?0:127;bytes[4]=codes[i];program_sysex_feed(0,bytes,sizeof(bytes),state,&runtime);}
 need(in->global_count==6&&!in->stop_home,"MMC frames are independent actions, stopped Stop never returns home");for(unsigned i=0;i<6;i++)need(in->global_events[i].operation==operations[i],"MMC distinct Play/Stop/desired-Record operations");
 program_sysex_feed(1,bytes,sizeof(bytes),state,&runtime);need(!surfaces.contexts[1].input.global_count,"MMC is excluded from other profiles");
 for(unsigned command=0;command<128;command++){if(command==1||command==2||command==3||command==6)continue;bytes[4]=command;program_sysex_feed(0,bytes,sizeof(bytes),state,&runtime);}
 bytes[4]=2;for(unsigned length=0;length<=7;length++)if(length!=6)program_sysex_feed(0,bytes,length,state,&runtime);
 const unsigned bad_positions[]={0,1,2,3,5},bad_values[]={0x7f,0x7e,128,7,0};for(unsigned i=0;i<5;i++){unsigned at=bad_positions[i],before=bytes[at];bytes[at]=bad_values[i];program_sysex_feed(0,bytes,6,state,&runtime);bytes[at]=before;}
 program_feed(0,0x90,60,127,state,&runtime);snd_seq_event_t clock;snd_seq_ev_clear(&clock);clock.type=SND_SEQ_EVENT_CLOCK;clock.source=surfaces.surfaces[0].full;fake_input_reset();fake_input_push(clock);need(collection_drain(&surfaces,state,&runtime),"MPK249 clock remains outside native commands");
 snd_seq_event_t other;snd_seq_ev_clear(&other);snd_seq_ev_set_sysex(&other,6,bytes);other.source=(snd_seq_addr_t){190,2};fake_input_reset();fake_input_push(other);need(collection_drain(&surfaces,state,&runtime),"unselected MMC endpoint ignored");
 need(in->global_count==6,"malformed unsupported response and other-port MMC never queues");
 /* Retained MPK249 Port A capture, 2026-10-14: type20, Start, Play,
  * Stop realtime, Stop MMC, Record Strobe twice, type20, Stop MMC.
  * Preserve ordering and bytes; timing and native execution are substituted. */
 in->global_count=in->global_head=0;
 const unsigned recorded_types[]={20,SND_SEQ_EVENT_START,SND_SEQ_EVENT_SYSEX,SND_SEQ_EVENT_STOP,SND_SEQ_EVENT_SYSEX,SND_SEQ_EVENT_SYSEX,SND_SEQ_EVENT_SYSEX,20,SND_SEQ_EVENT_SYSEX};
 const unsigned recorded_commands[]={0,0,2,0,1,6,6,0,1};
 for(unsigned i=0;i<9;i++){
  if(recorded_types[i]==SND_SEQ_EVENT_SYSEX){bytes[2]=127;bytes[4]=recorded_commands[i];program_sysex_feed(0,bytes,6,state,&runtime);}
  else{snd_seq_event_t captured;snd_seq_ev_clear(&captured);captured.type=recorded_types[i];captured.source=surfaces.surfaces[0].full;fake_input_reset();fake_input_push(captured);need(collection_drain(&surfaces,state,&runtime),"recorded MPK249 non-MMC event drains");}
 }
 const unsigned recorded_operations[]={GLOBAL_PLAY,GLOBAL_STOP,GLOBAL_RECORD_ENABLE,GLOBAL_RECORD_ENABLE,GLOBAL_STOP};
 need(in->global_count==5,"recorded MPK249 realtime plus MMC produces exactly five actions");
 for(unsigned i=0;i<5;i++)need(in->global_events[i].operation==recorded_operations[i],"recorded MPK249 transport preserves action ordering and repeated desired-on Record");
 puts("PASS retained MPK249 transport sequence: realtime Start/Stop and type20 ignored, five ordered MMC intents including repeated desired-on Record; timing/native execution substituted");
 in->global_count=in->global_head=0;bytes[4]=1;program_sysex_feed(0,bytes,6,state,&runtime);
 input_submit(in,surfaces.banks,&snapshot,100);CommandRequest request;need(command_request_read(commands->slots,1,&request)&&request.reserved==GLOBAL_STOP&&request.global_owner==snapshot.editor_owner,"MMC Stop publishes canonical owner request");
 snapshot.ready=0;program_sysex_feed(0,bytes,6,state,&runtime);need(!in->global_count,"unready source clears pending MMC actions");snapshot.ready=1;snapshot.editor_owner=0;program_sysex_feed(0,bytes,6,state,&runtime);need(!in->global_count,"missing current transport owner refuses MMC Play");
 for(unsigned i=0;i<2;i++){collection_disconnect(&surfaces,i);}free(state);free(commands);
 puts("PASS MPK249 MMC simple commands through selected ALSA endpoint and canonical queue: independent frames, Play/Deferred Play, Stop, desired-on Record, malformed/unsupported exclusion and source fencing; physical USB endpoint/native execution substituted");
}
static void controller_pickup_origin_checks(void){
 CommandState *commands=calloc(1,sizeof(*commands));MirrorState *state=calloc(1,sizeof(*state));need(commands&&state,"pickup origin fixtures");MirrorBank unused;MIRROR_COPY(snapshot);reset_fixture(commands,&input,&unused,&snapshot,8);collection_fixture_source(state);shared_fixture_time(state,1000);
 McuControllerCollection saved={.count=2};const McuProfile *profiles[]={&mcu_profile_mpk_mini3,&mcu_profile_lpd8_mk2};FakeClient clients[2]={0};
 for(unsigned i=0;i<2;i++){saved.records[i]=(McuControllerPreference){.enabled=1,.input={profiles[i],i?"LPD":"mini","Control",MCU_ENDPOINT_USER},.output={profiles[i],i?"LPD":"mini","Control",MCU_ENDPOINT_USER}};clients[i]=(FakeClient){.name=i?"LPD":"mini",.type=SND_SEQ_USER_CLIENT,.client=180+i,.count=1,.ports={{"Control",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE,1}}};}
 collection_init(&surfaces,&saved);surfaces.seq=(snd_seq_t*)1;surfaces.sink=1;surfaces.source=2;fake_enumeration(clients,2);fake_input_reset();need(collection_topology(&surfaces,state)&&surfaces.connected==2,"pickup pair shares real coordinator");
 ControllerRuntime runtime={.snapshot=&snapshot,.snapshot_attempted=1,.snapshot_result=1,.now=100};for(unsigned i=0;i<2;i++){bank_apply(surfaces.banks+i,&snapshot,100);input_sync(&surfaces.contexts[i].input,surfaces.banks+i);}
 MirrorInput *a=&surfaces.contexts[0].input,*b=&surfaces.contexts[1].input;
 program_feed(0,0xb0,70,64,state,&runtime);program_feed(1,0xbf,70,64,state,&runtime);program_feed(1,0xbf,70,100,state,&runtime);input_submit(b,surfaces.banks+1,&snapshot,100);
 InputFlight *flight=input_existing(a,surfaces.banks[0].strips,CF_VOLUME);need(flight&&flight->origin==b&&!a->desires[0][CF_VOLUME].pending,"B owns shared volume flight and coalesces prior A desire");
 snapshot.tracks[0].bits=flight->bits;snapshot.tracks[0].revision++;
 program_feed(1,0xbf,70,110,state,&runtime);need(surfaces.surfaces[1].native_pickup[0].acquired&&b->desires[0][CF_VOLUME].pending,"own volume settlement retains pickup");
 program_feed(0,0xb0,70,65,state,&runtime);need(!surfaces.surfaces[0].native_pickup[0].acquired&&!a->desires[0][CF_VOLUME].pending,"peer volume settlement requires A to repickup");
 snapshot.qlinks_available=1;snapshot.qlinks=(QLinkCopy){.epoch=snapshot.epoch,.status=QL_SAMPLED,.root=1,.wrapper=2,.provider=3,.generation=4};for(unsigned i=0;i<16;i++)snapshot.qlinks.slots[i]=(QLinkSlot){.sampled=1,.normalized_valid=1,.tick=100,.bits=0x3f000000,.revision=2};
 for(unsigned i=0;i<2;i++){program_feed(i,0xb9,46,127,state,&runtime);program_feed(i,0xb9,46,0,state,&runtime);}
 program_feed(0,0xb0,70,64,state,&runtime);program_feed(1,0xbf,70,64,state,&runtime);program_feed(1,0xbf,70,100,state,&runtime);input_submit(b,surfaces.banks+1,&snapshot,100);
 flight=NULL;for(unsigned i=0;i<COMMAND_SLOTS;i++)if(b->flights[i].flight&&command_qlink(b->flights[i].field)){flight=b->flights+i;break;}
 need(flight&&flight->origin==b&&!a->qlinks[0].pending,"B owns shared Q-Link flight and coalesces prior A desire");snapshot.qlinks.slots[0].bits=flight->bits;snapshot.qlinks.slots[0].revision++;
 program_feed(1,0xbf,70,110,state,&runtime);need(surfaces.surfaces[1].program.qlinks[0].acquired&&b->qlinks[0].pending,"own Q-Link settlement retains pickup");
 program_feed(0,0xb0,70,65,state,&runtime);need(!surfaces.surfaces[0].program.qlinks[0].acquired&&!a->qlinks[0].pending,"peer Q-Link settlement requires A to repickup");
 for(unsigned i=0;i<2;i++){collection_disconnect(&surfaces,i);}free(state);free(commands);
 puts("PASS two saved-program profiles share mixer/Q-Link targets: peer flight settlement revokes pickup while own-origin settlement preserves it; native source update substituted");
}
static void controller_admission_restart_checks(void){
 char directory[]="/tmp/mpclearn-admission-restart-XXXXXX",path[256],source[256];need(mkdtemp(directory)!=NULL,"restart page directory");
 snprintf(path,sizeof(path),"%s/native-preferences.state",directory);snprintf(source,sizeof(source),"%s/volume.state",directory);
 int fd=open(path,O_CREAT|O_EXCL|O_RDWR,0600);need(fd>=0&&!ftruncate(fd,NATIVE_PREFERENCES_BYTES),"restart page file");
 NativePreferencesState *page=mmap(NULL,NATIVE_PREFERENCES_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);need(page!=MAP_FAILED,"restart shared page");
 page->magic=NATIVE_PREFERENCES_MAGIC;page->version=NATIVE_PREFERENCES_VERSION;page->bytes=NATIVE_PREFERENCES_BYTES;page->pid=123;
 McuControllerCollection saved={.count=1};saved.records[0]=(McuControllerPreference){.enabled=1,.input={&mcu_profile_lpx,"LP","MIDI",MCU_ENDPOINT_USER},.output={&mcu_profile_lpx,"LP","MIDI",MCU_ENDPOINT_USER}};
 native_state_collection(page,&saved,NULL,0);
 for(unsigned boundary=1;boundary<=5;boundary++){
  need(!native_preferences_invalidate(path,0),"new bridge launch token");collection_init(&surfaces,&saved);
  need(collection_admission_open(&surfaces,source,123)&&surfaces.preferences,"bridge opens retained page");
  surfaces.surfaces[0].seq=(snd_seq_t*)1;novation_reset(&surfaces.surfaces[0].novation,MCU_PROTOCOL_LPX,1);
  pid_t child=fork();need(child>=0,"admission publisher child");
  if(!child){admission_interrupt_after=boundary;admission_write_count=0;collection_admission_publish(&surfaces);_exit(2);}
  int status=0;need(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL,"publisher interrupted at actual write boundary");
  need(page->admission.revision&1,"interrupted writer leaves unreadable revision");
  NativePreferencesController row=page->controllers[0];row.status=NATIVE_CONTROLLER_READY;native_controller_project(page,&row,1,page->collection_generation);need(row.status==NATIVE_CONTROLLER_NO_BRIDGE,"partial publication never activates UI");
  need(!native_preferences_invalidate(path,surfaces.preferences_lifetime),"reaped publisher lifetime invalidated");unsigned revision=page->admission.revision;
  collection_admission_publish(&surfaces);need(page->admission.revision==revision,"revoked writer cannot repair or overwrite successor");
  munmap(surfaces.preferences,NATIVE_PREFERENCES_BYTES);surfaces.preferences=NULL;
  need(!native_preferences_invalidate(path,0),"replacement launch token");collection_init(&surfaces,&saved);
  need(collection_admission_open(&surfaces,source,123),"replacement opens same page without recreation");
  row=page->controllers[0];native_controller_project(page,&row,1,page->collection_generation);
  need(!(page->admission.revision&1)&&row.status==NATIVE_CONTROLLER_DISCONNECTED,"replacement recovers coherent disconnected projection");
  revision=page->admission.revision;collection_admission_publish(&surfaces);need(page->admission.revision==revision,"recovered unchanged status stays parked");
  munmap(surfaces.preferences,NATIVE_PREFERENCES_BYTES);surfaces.preferences=NULL;
 }
 munmap(page,NATIVE_PREFERENCES_BYTES);need(!unlink(path)&&!rmdir(directory),"restart page cleanup");
 puts("PASS killed admission publisher at every payload boundary: same-page replacement recovers even publication, old lifetime stays fenced, unchanged projection remains parked");
}
static void controller_admission_checks(void){
 char page_path[]="/tmp/mpclearn-admission-XXXXXX";int fd=mkstemp(page_path);need(fd>=0&&!ftruncate(fd,NATIVE_PREFERENCES_BYTES),"admission page file");
 NativePreferencesState *mapped=mmap(NULL,NATIVE_PREFERENCES_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);close(fd);need(mapped!=MAP_FAILED,"admission page fixture map");
 mapped->magic=NATIVE_PREFERENCES_MAGIC;mapped->version=NATIVE_PREFERENCES_VERSION-1;mapped->bytes=NATIVE_PREFERENCES_BYTES;NativePreferencesState *opened=NULL;
 need(native_state_open(page_path,&opened)<0&&native_preferences_invalidate(page_path,0)==1,"old UI page rejects new bridge/session without mutation");mapped->version=NATIVE_PREFERENCES_VERSION;
 need(!native_state_open(page_path,&opened),"matching UI/runtime page version admitted");munmap(opened,NATIVE_PREFERENCES_BYTES);need(!native_preferences_invalidate(page_path,0)&&mapped->controller_lifetime==1,"actual session helper invalidates existing page lifetime");need(!native_preferences_invalidate(page_path,1)&&mapped->controller_lifetime==2,"exact exiting bridge invalidates its own launch");need(!native_preferences_invalidate(page_path,1)&&mapped->controller_lifetime==2,"stale exiting waiter cannot revoke a later launch");munmap(mapped,NATIVE_PREFERENCES_BYTES);need(!unlink(page_path),"admission page cleanup");
 NativePreferencesState page={0};page.controller_lifetime=7;
 McuControllerCollection saved={.count=1};saved.records[0]=(McuControllerPreference){.enabled=1,.input={&mcu_profile_lpx,"LP","DAW",MCU_ENDPOINT_USER},.output={&mcu_profile_lpx,"LP","DAW",MCU_ENDPOINT_USER}};
 collection_init(&surfaces,&saved);native_state_collection(&page,&saved,NULL,0);
 page.controllers[0].status=NATIVE_CONTROLLER_READY;NativePreferencesController row=page.controllers[0];
 native_controller_project(&page,&row,1,page.collection_generation);need(row.status==NATIVE_CONTROLLER_NO_BRIDGE,"missing admission never claims active");
 surfaces.preferences=&page;surfaces.preferences_lifetime=7;surfaces.preferences_generation=page.collection_generation;
 Surface *surface=surfaces.surfaces;surface->seq=(snd_seq_t*)1;surface->full=(snd_seq_addr_t){150,1};surface->output=(snd_seq_addr_t){150,2};surface->shared_connection=1;surfaces.connected=1;
 novation_reset(&surface->novation,MCU_PROTOCOL_LPX,1);collection_admission_publish(&surfaces);
 row=page.controllers[0];native_controller_project(&page,&row,1,page.collection_generation);need(row.status==NATIVE_CONTROLLER_WAIT_MODE,"connected LP waiting on prior mode is visible");
 native_capture_enabled=1;native_capture_count=0;need(native_initialise(surface,1000)&&native_initialise(surface,1300),"unknown LP admits only queries");collection_admission_publish(&surfaces);
 row=page.controllers[0];native_controller_project(&page,&row,1,page.collection_generation);need(row.status==NATIVE_CONTROLLER_UNKNOWN_MODE,"expired unknown mode reason reaches existing Preferences page");
 unsigned revision=page.admission.revision;collection_admission_publish(&surfaces);need(page.admission.revision==revision,"unchanged admission performs no page write");
 NovationEvent mode={.kind=NOV_PROGRAMMER,.value=0},layout={.kind=NOV_LAYOUT,.value=1};novation_observe(&surface->novation,&mode);novation_observe(&surface->novation,&layout);novation_budget(&surface->novation);need(native_initialise(surface,1400)&&surface->novation.stage==16,"delayed valid mode enters same LP instance");collection_admission_publish(&surfaces);
 row=page.controllers[0];native_controller_project(&page,&row,1,page.collection_generation);need(row.status==NATIVE_CONTROLLER_ACTIVE,"late valid reply publishes ACTIVE on original row");
 NovationEvent exit_mode={.kind=NOV_PROGRAMMER,.value=0};novation_observe(&surface->novation,&exit_mode);collection_admission_publish(&surfaces);row=page.controllers[0];native_controller_project(&page,&row,1,page.collection_generation);need(row.status==NATIVE_CONTROLLER_UNKNOWN_MODE,"observed external exit from Programmer cannot stay ACTIVE");
 unsigned sent=native_capture_count;MirrorBank blank_bank={0};MIRROR_COPY(blank_snapshot);memset(&blank_snapshot,0,sizeof(blank_snapshot));need(native_output(surface,&blank_bank,&blank_snapshot,1401)&&native_capture_count==sent,"observed mode exit suppresses grid output without reentering automatically");exit_mode.value=1;novation_observe(&surface->novation,&exit_mode);collection_admission_publish(&surfaces);
 /* Session sync retains its own fields and generation; overlay writes are disjoint. */
 uint32_t generation=page.collection_generation;native_state_collection(&page,&saved,NULL,0);need(page.collection_generation==generation,"endpoint refresh does not change config generation");
 row=page.controllers[0];native_controller_project(&page,&row,1,generation);need(row.status==NATIVE_CONTROLLER_ACTIVE,"session endpoint sync cannot erase live admission");
 page.admission.revision++;row=page.controllers[0];row.status=NATIVE_CONTROLLER_READY;native_controller_project(&page,&row,1,generation);need(row.status==NATIVE_CONTROLLER_NO_BRIDGE,"interrupted overlay write reads conservatively");page.admission.revision++;
 page.controller_lifetime++;row=page.controllers[0];row.status=NATIVE_CONTROLLER_READY;native_controller_project(&page,&row,1,generation);need(row.status==NATIVE_CONTROLLER_NO_BRIDGE,"session child exit/restart invalidates same-MPC stale ACTIVE");
 revision=page.admission.revision;collection_admission_publish(&surfaces);need(page.admission.revision==revision,"old bridge cannot republish after lifetime invalidation");
 surfaces.preferences_lifetime=page.controller_lifetime;collection_disconnect(&surfaces,0);row=page.controllers[0];native_controller_project(&page,&row,1,generation);need(row.status==NATIVE_CONTROLLER_DISCONNECTED,"disconnect clears admitted row");
 surface->seq=(snd_seq_t*)1;surfaces.connected=1;novation_reset(&surface->novation,MCU_PROTOCOL_LPX,2);collection_admission_publish(&surfaces);row=page.controllers[0];native_controller_project(&page,&row,1,generation);need(row.status==NATIVE_CONTROLLER_WAIT_MODE,"reconnect cannot reuse old admitted mode");
 page.collection_generation++;row=page.controllers[0];row.status=NATIVE_CONTROLLER_READY;native_controller_project(&page,&row,1,page.collection_generation);need(row.status==NATIVE_CONTROLLER_NO_BRIDGE,"old collection generation cannot activate replacement row");
 page.collection_revision++;revision=page.admission.revision;collection_admission_publish(&surfaces);need(page.admission.revision==revision,"in-progress session sync cannot be projected");page.collection_revision++;
 saved.records[0].input.profile=saved.records[0].output.profile=&mcu_profile_lcxl3;
 surfaces.contexts[0].controller.profile=surfaces.contexts[0].controller_output.profile=&mcu_profile_lcxl3;
 native_state_collection(&page,&saved,NULL,0);surfaces.preferences_generation=page.collection_generation;
 novation_reset(&surface->novation,MCU_PROTOCOL_LCXL3,3);surface->novation.stage=15;
 for(unsigned pass=0;pass<8&&surface->novation.lc_queried!=7;pass++){novation_budget(&surface->novation);need(native_initialise(surface,2000),"LCXL completes bounded display initialization before readback");}
 need(surface->novation.lc_queried==7,"LCXL has actually sent all three feature queries");collection_admission_publish(&surfaces);row=page.controllers[0];native_controller_project(&page,&row,1,page.collection_generation);need(row.status==NATIVE_CONTROLLER_WAIT_MODE,"LCXL endpoint does not claim active before feature readback");
 novation_budget(&surface->novation);need(native_initialise(surface,2250),"LCXL readback status threshold");collection_admission_publish(&surfaces);row=page.controllers[0];native_controller_project(&page,&row,1,page.collection_generation);need(row.status==NATIVE_CONTROLLER_UNKNOWN_MODE,"missing LCXL readback projects existing inactive reason");
 lcxl_feature_reply(surface,6,73,127);lcxl_feature_reply(surface,6,69,127);lcxl_feature_reply(surface,6,72,127);novation_budget(&surface->novation);need(native_initialise(surface,2260),"LCXL delayed readback settles");collection_admission_publish(&surfaces);row=page.controllers[0];native_controller_project(&page,&row,1,page.collection_generation);need(row.status==NATIVE_CONTROLLER_ACTIVE,"all three observedOn rows activate same Preferences row");
 lcxl_feature_reply(surface,6,69,0);collection_admission_publish(&surfaces);row=page.controllers[0];native_controller_project(&page,&row,1,page.collection_generation);need(row.status==NATIVE_CONTROLLER_UNKNOWN_MODE,"active rowOff revokes visible admission");
 surfaces.preferences=NULL;collection_disconnect(&surfaces,0);native_capture_enabled=0;
 puts("PASS existing Preferences page projection: unknown/delayed mode, coherent revisions, unchanged-write suppression, configuration/lifetime fencing and reconnect; page/session interleavings substituted");
}
/* CE setup tables in docs/controller-programs.md, not captured factory presets.
 * Sweep every column: a valid first strip alone cannot detect row/column aliases. */
static void controller_program_assignment_checks(MirrorState *state,ControllerRuntime *runtime){
 const unsigned fields[]={CF_VOLUME,CF_PAN,CF_SEND1,CF_SEND2};
 for(unsigned instance=0;instance<2;instance++){
  MirrorInput *in=&surfaces.contexts[instance].input;
  for(unsigned layer=0;layer<6;layer++){
   program_feed(instance,0xb9,42+layer,127,state,runtime);program_feed(instance,0xb9,42+layer,0,state,runtime);
   for(unsigned strip=0;strip<8;strip++){
    input_discard(in);program_feed(instance,instance?0xbf:0xb0,70+strip,0,state,runtime);program_feed(instance,instance?0xbf:0xb0,70+strip,127,state,runtime);
    if(layer<4){InputGesture *g=&in->desires[strip][fields[layer]];need(g->pending&&g->identity.serial==strip+1&&g->bits==0x3f800000,"all compact layer/column assignments retain exact track and value");}
    else{unsigned index=(layer-4)*8+strip;need(in->qlinks[index].pending&&in->qlinks[index].request.qlink_index==index&&in->qlinks[index].request.bits==0x3f800000,"all compact Q-Link columns address exact native slot");}
   }
  }
  program_feed(instance,0xb9,42,127,state,runtime);program_feed(instance,0xb9,42,0,state,runtime);input_discard(in);
 }
 MirrorInput *mpk=&surfaces.contexts[2].input;
 for(unsigned bank=0;bank<3;bank++)for(unsigned column=0;column<16;column++){
  input_discard(mpk);program_feed(2,0xb0+bank,16+column,0,state,runtime);program_feed(2,0xb0+bank,16+column,127,state,runtime);
  if(bank<2){unsigned field=fields[bank*2+(column>=8)];InputGesture *g=&mpk->desires[column%8][field];need(g->pending&&g->identity.serial==column%8+1&&g->bits==0x3f800000,"all MPK249 banks/rows retain exact mixer target");}
  else need(mpk->qlinks[column].pending&&mpk->qlinks[column].request.qlink_index==column,"all MPK249 bankC controls address sixteen distinct Q-Links");
 }
 const unsigned buttons[]={CF_MUTE,CF_SOLO,CF_ARM};
 for(unsigned bank=0;bank<3;bank++)for(unsigned strip=0;strip<8;strip++){
  input_discard(mpk);program_feed(2,0xb0+bank,40+strip,127,state,runtime);program_feed(2,0xb0+bank,40+strip,0,state,runtime);
  need(mpk->edge_count==1&&mpk->edges[0].field==buttons[bank]&&mpk->edges[0].desire.identity.serial==strip+1,"all MPK249 switches retain bank and column");
 }
 input_discard(mpk);
 /* Reconnect is already tested elsewhere; reset pickup/layers here so this
  * table sweep does not lend acquired state to the independent checks below. */
 for(unsigned i=0;i<3;i++){control_program_mode(surfaces.surfaces+i,surfaces.banks+i,CP_VOLUME,0);memset(surfaces.surfaces[i].native_pickup,0,sizeof(surfaces.surfaces[i].native_pickup));memset(surfaces.surfaces[i].program.qlinks,0,sizeof(surfaces.surfaces[i].program.qlinks));input_discard(&surfaces.contexts[i].input);}
 puts("PASS documented CE assignments through shared ingress:96 compact layer/knob targets,48 MPK249 absolute targets,24 switches; hardware preset/native callbacks substituted");
}
static void control_program_production_checks(void){
 need(!mcu_profile_named("sq64-control")&&!mcu_controller_profile(11),"retired SQ profile name and persisted ID are not admitted or reused");
 CommandState *commands=calloc(1,sizeof(*commands));MirrorState *state=calloc(1,sizeof(*state));need(commands&&state,"program fixtures");MirrorBank unused;MIRROR_COPY(snapshot);reset_fixture(commands,&input,&unused,&snapshot,16);collection_fixture_source(state);shared_fixture_time(state,1000);
 const McuProfile *profiles[]={&mcu_profile_mpk_mini3,&mcu_profile_lpd8_mk2,&mcu_profile_mpk249};const char *names[]={"mini","LPD8 Mk2","MPK249"};
 McuControllerCollection saved={.count=3};FakeClient clients[3]={0};
 for(unsigned i=0;i<3;i++){
  saved.records[i]=(McuControllerPreference){.enabled=1,.input={profiles[i],names[i],"Control",MCU_ENDPOINT_USER},.output={profiles[i],names[i],"Control",MCU_ENDPOINT_USER}};
  clients[i]=(FakeClient){.name=names[i],.type=SND_SEQ_USER_CLIENT,.client=160+i,.count=2,.ports={{"Control",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE,1},{"Musical",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE,2}}};
 }
 char directory[]="/tmp/mpclearn-programs-XXXXXX",path[256];need(mkdtemp(directory)!=NULL,"program persistence directory");snprintf(path,sizeof(path),"%s/controller-preferences",directory);need(mcu_controller_preferences_write_collection(path,&saved),"program profiles save");McuControllerCollection loaded;need(mcu_controller_preferences_read_collection(path,&loaded)&&loaded.count==3,"program profiles restore");for(unsigned i=0;i<3;i++)need(loaded.records[i].input.profile==profiles[i],"stable program profile ID");
 collection_init(&surfaces,&loaded);surfaces.seq=(snd_seq_t*)1;surfaces.sink=1;surfaces.source=2;fake_enumeration(clients,3);fake_input_reset();need(collection_topology(&surfaces,state)&&surfaces.connected==3,"manual program endpoints use one topology");
 ControllerRuntime runtime={.snapshot=&snapshot,.snapshot_attempted=1,.snapshot_result=1,.now=100};
 for(unsigned i=0;i<3;i++){bank_apply(surfaces.banks+i,&snapshot,100);input_sync(&surfaces.contexts[i].input,surfaces.banks+i);}
 snapshot.qlinks_available=1;snapshot.qlinks=(QLinkCopy){.epoch=snapshot.epoch,.status=QL_SAMPLED,.root=1,.wrapper=2,.provider=3,.generation=4};for(unsigned i=0;i<16;i++)snapshot.qlinks.slots[i]=(QLinkSlot){.sampled=1,.normalized_valid=1,.tick=100,.bits=0x3f000000,.revision=2};
 controller_program_assignment_checks(state,&runtime);
 unsigned outputs=output_count;for(unsigned i=0;i<3;i++)need(channel_output(surfaces.surfaces+i,surfaces.banks+i,&snapshot,100),"receive-only profile output visit");need(output_count==outputs,"program profiles never emit mode/preset/LED bytes");
 for(unsigned i=0;i<3;i++){program_feed(i,0x99,40,127,state,&runtime);program_feed(i,0xb8,70,127,state,&runtime);}need(!atomic_load(&commands->published),"musical notes and wrong channels never issue commands");
 program_feed(0,0xb0,70,0,state,&runtime);need(!surfaces.contexts[0].input.desires[0][CF_VOLUME].pending,"mini knob waits for pickup");program_feed(0,0xb0,70,64,state,&runtime);program_feed(0,0xb0,70,80,state,&runtime);need(surfaces.contexts[0].input.desires[0][CF_VOLUME].pending,"mini volume pickup uses keyboard-shared channel1");
 program_feed(1,0xbf,71,64,state,&runtime);program_feed(1,0xbf,71,90,state,&runtime);need(surfaces.contexts[1].input.desires[1][CF_VOLUME].pending,"LPD8 knob uses separate channel16");
 program_feed(2,0xb0,18,64,state,&runtime);program_feed(2,0xb0,18,100,state,&runtime);need(surfaces.contexts[2].input.desires[2][CF_VOLUME].pending,"MPK249 bankA volume");
 for(unsigned i=0;i<3;i++){input_submit(&surfaces.contexts[i].input,surfaces.banks+i,&snapshot,100);}
 need(atomic_load(&commands->published)==3,"three profiles share canonical command mailbox");
 for(unsigned n=1;n<=3;n++){CommandRequest request;unsigned slot=command_find(commands,n);need(command_request_read(commands->slots+slot,n,&request)&&request.reserved==CF_VOLUME&&request.serial==n,"independent profile volume binds expected track");}
 /* Reset unsent work for layer tests; retain published flights as real runtime does. */
 for(unsigned i=0;i<3;i++)input_discard(&surfaces.contexts[i].input);
 program_feed(0,0xb9,43,64,state,&runtime);need(surfaces.surfaces[0].program.layer==CP_PAN,"compact nonzero CC pad selects Pan");
 program_feed(0,0xb9,43,0,state,&runtime);program_feed(0,0xb0,70,64,state,&runtime);program_feed(0,0xb0,70,100,state,&runtime);need(surfaces.contexts[0].input.desires[0][CF_PAN].pending,"compact selected layer reaches canonical pan");
 program_feed(0,0xb9,41,127,state,&runtime);program_feed(0,0xb9,41,127,state,&runtime);need(surfaces.banks[0].offset==8,"duplicate CC pad down navigates only once");program_feed(0,0xb9,41,0,state,&runtime);program_feed(0,0xb9,40,1,state,&runtime);need(surfaces.banks[0].offset==0,"compact previous bank accepts nonzero press");program_feed(0,0xb9,40,0,state,&runtime);
 program_feed(2,0xb1,16,0,state,&runtime);program_feed(2,0xb1,16,80,state,&runtime);program_feed(2,0xb1,24,0,state,&runtime);program_feed(2,0xb1,24,90,state,&runtime);need(surfaces.contexts[2].input.desires[0][CF_SEND1].pending&&surfaces.contexts[2].input.desires[0][CF_SEND2].pending,"MPK249 bankB rows control send1/send2");
 for(unsigned bank=0;bank<3;bank++){program_feed(2,0xb0+bank,44,127,state,&runtime);program_feed(2,0xb0+bank,44,0,state,&runtime);}need(surfaces.contexts[2].input.edge_count==3,"MPK249 A/B/C switches map mute solo arm");
 snapshot.qlinks_available=1;snapshot.qlinks=(QLinkCopy){.epoch=snapshot.epoch,.status=QL_SAMPLED,.root=1,.wrapper=2,.provider=3,.generation=4};for(unsigned i=0;i<16;i++)snapshot.qlinks.slots[i]=(QLinkSlot){.sampled=1,.normalized_valid=1,.tick=100,.bits=0x3f000000,.revision=2};
 program_feed(2,0xb2,31,0,state,&runtime);need(!surfaces.contexts[2].input.qlinks[15].pending,"MPK249 bankC knob waits for Q-Link pickup");program_feed(2,0xb2,31,64,state,&runtime);program_feed(2,0xb2,31,90,state,&runtime);need(surfaces.banks[2].qlink_page==2&&surfaces.contexts[2].input.qlinks[15].pending&&surfaces.contexts[2].input.qlinks[15].request.qlink_index==15,"MPK249 bankC accesses actual Q-Link16 using existing all-slot demand");
 program_feed(0,0xb9,47,127,state,&runtime);program_feed(0,0xb9,47,0,state,&runtime);program_feed(0,0xb0,77,64,state,&runtime);program_feed(0,0xb0,77,110,state,&runtime);need(surfaces.banks[0].qlink_page==1&&surfaces.contexts[0].input.qlinks[15].pending,"compact pad selects Q-Link9-16 bank");need(!surfaces.contexts[2].input.qlinks[15].pending,"later compact value coalesces the same MPK249 Q-Link target");
 for(unsigned i=0;i<3;i++)input_submit(&surfaces.contexts[i].input,surfaces.banks+i,&snapshot,100);
 unsigned qlinks=0;for(unsigned n=1;n<=atomic_load(&commands->published);n++){CommandRequest r;unsigned at=command_find(commands,n);if(at<COMMAND_SLOTS&&command_request_read(commands->slots+at,n,&r)&&r.reserved==QLINK_VALUE){need(r.qlink_index==15&&r.bits==surfaces.contexts[0].input.qlinks[15].request.bits,"published actual Q-Link index and latest shared value");qlinks++;}}need(qlinks==1,"shared Q-Link16 edits publish one latest canonical request");
 unsigned old_output=output_count;collection_disconnect(&surfaces,0);need(!surfaces.surfaces[0].program.layer&&!surfaces.surfaces[0].program.qlinks[15].acquired&&output_count==old_output,"compact disconnect resets local pickup/layer without output");need(collection_topology(&surfaces,state)&&surfaces.connected==3,"program reconnect through shared topology");
 program_feed(0,0xb0,70,0,state,&runtime);need(!surfaces.contexts[0].input.desires[0][CF_VOLUME].pending,"reconnect reacquires pickup instead of stale jump");
 snapshot.ready=0;program_feed(1,0xbf,70,127,state,&runtime);need(!surfaces.contexts[1].input.desires[0][CF_VOLUME].pending,"unready source cancels program intent");
 for(unsigned i=0;i<3;i++){collection_disconnect(&surfaces,i);}
 need(!unlink(path)&&!rmdir(directory),"program fixture cleanup");
 controller_admission_checks();controller_admission_restart_checks();free(state);free(commands);controller_pickup_origin_checks();controller_mmc_checks();
 puts("PASS three CE programs through saved IDs, shared ALSA ingress/mailbox, compact layers, MPK249 banks, mixer/Q-Link pickup, musical-message exclusion and reconnect; hardware presets/native MPC callbacks substituted");
}
