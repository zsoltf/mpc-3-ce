/* Real MCU codec, surface ingress, command publication and outbound feedback.
 * ALSA endpoints and MPC's copied state are fixtures, not physical hardware. */
static void mini_feed(Surface *surface,MirrorBank *bank,MirrorState *state,ControllerRuntime *runtime,unsigned status,unsigned key,unsigned value){
 snd_midi_event_t *codec;need(!snd_midi_event_new(16,&codec),"Mini production MIDI codec");
 unsigned char bytes[]={(unsigned char)status,(unsigned char)key,(unsigned char)value};snd_seq_event_t event;snd_seq_ev_clear(&event);
 need(snd_midi_event_encode(codec,bytes,3,&event)==3,"Mini wire decoded by ALSA");event.source=surface->full;
 shared_fixture_time(state,100);need(surface_consume(surface,bank,state,0,runtime,&event),"Mini wire traverses production ingress");snd_midi_event_free(codec);
}
/* Independent expected wire set from the Mini's physical MC layout; do not
 * derive this oracle from the production capability helper. */
static void mini_led_capture_check(void){
 static const unsigned notes[]={89,90,40,41,42,43,44,45,87,88,91,92,86,93,94,95};
 need(native_capture_count<512,"Mini output capture did not truncate");
 for(unsigned i=0;i<native_capture_count;i++){
  const unsigned char *bytes=native_capture[i].bytes;
  if(native_capture[i].length!=3||(bytes[0]&0xe0)!=0x80)continue;
  unsigned found=0;for(unsigned j=0;j<sizeof(notes)/sizeof(notes[0]);j++)found|=bytes[1]==notes[j];
  need((bytes[0]&15)==0&&found,"Mini emits only documented ordinary button LEDs on MIDI channel 1");
 }
}
static void mini_feedback(Surface *surface,MirrorBank *bank,CopiedMirror *snapshot){
 unsigned prior_capture=native_capture_enabled;native_capture_enabled=1;native_capture_count=0;
 bank_apply(bank,snapshot,100);for(unsigned i=0;i<8;i++)need(channel_output(surface,bank,snapshot,100),"Mini authoritative output sweep");
 mini_led_capture_check();native_capture_enabled=prior_capture;
}
static void mini_led_clear_checks(Surface *surface,Surface *full,MirrorBank *full_bank,CopiedMirror *snapshot){
 native_capture_enabled=1;native_capture_count=0;surface->chooser_cleared=0;
 need(chooser_output(surface),"Mini chooser clear traverses production output");mini_led_capture_check();
 unsigned notes=0,rings=0;for(unsigned i=0;i<native_capture_count;i++){
  const unsigned char *bytes=native_capture[i].bytes;
  if(native_capture[i].length==3&&bytes[0]==0x90){notes++;need(!bytes[2],"Mini chooser clears physical button LEDs");}
  if(native_capture[i].length==3&&bytes[0]==0xb0&&bytes[1]>=0x30&&bytes[1]<0x38){rings++;need(!bytes[2],"Mini chooser clears every ring");}
 }
 need(notes==16&&rings==8,"Mini clear emits exactly sixteen physical button notes and eight rings");
 native_capture_count=0;for(unsigned i=0;i<8;i++)surface->wire[i].led[3]=127;
 need(parent_led_clear(surface)&&!native_capture_count,"Mini parent clear emits no absent strip-select LEDs");
 /* Exercise NoteOff and alternate channels at the same boundary; ordinary
  * production writers currently use channel-1 NoteOn for on and off. */
 unsigned before=output_count;uint64_t encoded=surface->context->encoded,sent=surface->context->sent;
 snd_seq_event_t event;snd_seq_ev_clear(&event);snd_seq_ev_set_noteoff(&event,0,24,0);need(channel_send(surface,&event),"unsupported NoteOff is consumed");
 snd_seq_ev_set_noteon(&event,1,40,127);need(channel_send(surface,&event),"alternate-channel Mini lamp is consumed");
 need(output_count==before&&surface->context->encoded==encoded&&surface->context->sent==sent,"suppressed LEDs never reach ALSA or sent counters");
 full->source=3;full->output=(snd_seq_addr_t){33,0};native_capture_count=0;bank_apply(full_bank,snapshot,100);
 for(unsigned i=0;i<8;i++)need(channel_output(full,full_bank,snapshot,100),"full X-Touch output remains live");
 unsigned strip_notes=0;for(unsigned i=0;i<native_capture_count;i++)if(native_capture[i].length==3&&native_capture[i].bytes[0]==0x90&&native_capture[i].bytes[1]<32)strip_notes++;
 need(strip_notes==32,"full X-Touch still emits all arm solo mute and select LEDs");
 native_capture_count=0;full->chooser_cleared=0;need(chooser_output(full),"full X-Touch chooser clear remains live");
 strip_notes=0;unsigned automation=0;for(unsigned i=0;i<native_capture_count;i++)if(native_capture[i].length==3&&native_capture[i].bytes[0]==0x90){unsigned note=native_capture[i].bytes[1];strip_notes+=note<32;automation+=note==74||note==75||note==79;}
 need(strip_notes==32&&automation==3,"full X-Touch clear retains strip and automation LEDs");native_capture_enabled=0;
 puts("PASS Mini LED output boundary: only sixteen documented ordinary buttons, no Layer/strip/unsupported global notes, chooser and parent clear covered, rings preserved, suppressed notes excluded from sent counts; full X-Touch strip/global output unchanged (ALSA hardware substituted)");
}
/* Reset the substituted mailbox between independent target observations. */
static void mini_clean_commands(MirrorInput *in){
 CommandState *c=in->commands;InputCoordinator *coordinator=in->coordinator;MirrorBank *bank=in->origin_bank;
 memset(c,0,sizeof(*c));c->seconds=900;c->owner_token=123;atomic_store(&c->alive,1);memset(coordinator_flights,0,sizeof(coordinator_flights));
 need(input_begin(in,c,coordinator_flights),"fresh Mini target mailbox");in->coordinator=coordinator;in->origin_bank=bank;
}
static CommandRequest mini_published(MirrorInput *in,MirrorBank *bank,const CopiedMirror *snapshot,unsigned field){
 input_submit(in,bank,snapshot,100);unsigned seq=atomic_load(&in->commands->published),at=command_find(in->commands,seq);CommandRequest r;
 need(!in->error&&seq==1&&at<COMMAND_SLOTS&&command_request_read(in->commands->slots+at,seq,&r)&&r.reserved==field,"Mini production command has exact native operation");return r;
}
static void mini_production_checks(void){
 CommandState *commands=calloc(1,sizeof(*commands));MirrorState *state=calloc(1,sizeof(*state));need(commands&&state,"Mini fixtures");
 MirrorBank unused;MIRROR_COPY(snapshot);reset_fixture(commands,&input,&unused,&snapshot,20);collection_fixture_source(state);
 snapshot.selection.bits=snapshot.tracks[0].track;snapshot.selected_serial=1;snapshot.tracks[1].fields[CF_MUTE].bits=1;snapshot.tracks[2].fields[CF_SOLO].bits=1;snapshot.tracks[3].fields[CF_ARM].bits=1;
 snapshot.tracks[11].vptr=MODEL_MIDIPROGRAM_VPTR;snapshot.tracks[11].fields[CF_ARM].bits=1;
 for(unsigned i=0;i<4;i++){CopiedTrack *ret=snapshot.tracks+16+i;ret->vptr=MODEL_SENDPROGRAM_VPTR;ret->bits=0x3f400000;snapshot.send_programs[i]=ret->program;snapshot.send_owners[i]=ret->program_owner;snapshot.tracks[0].fields[CF_SEND1+i].bits=0x3e800000;}
 snapshot.qlinks_available=1;snapshot.qlinks=(QLinkCopy){.epoch=1,.status=QL_SAMPLED,.root=1,.wrapper=2,.provider=3,.generation=4,.mode_valid=1,.mode_controller=9,.mode_generation=10,.mode_tick=100,.mode_revision=2,.mode_id=2,.mode_count=4};
 for(unsigned i=0;i<16;i++)snapshot.qlinks.slots[i]=(QLinkSlot){.sampled=1,.normalized_valid=1,.tick=100,.bits=i<8?0x3e800000:0x3f400000,.revision=2};
 snapshot.master=(CopiedField){.property=0x800a8,.owner_incarnation=100,.incarnation=101,.bits=0x3f000000,.revision=2,.available=1,.seed=1};snapshot.editor_owner=100;
 snapshot.record_mode=(CopiedField){.available=1,.owner_incarnation=101,.incarnation=102};snapshot.loop=(CopiedField){.available=1,.owner_incarnation=103,.incarnation=104};
 snapshot.effects_available=1;snapshot.effects=(EffectsCopy){.epoch=1,.serial=1,.track_owner=snapshot.tracks[0].track_owner,.program_owner=snapshot.tracks[0].program_owner,.status=EF_READY,.generation=2,.slot=EFFECT_LIST};
 snapshot.effects.slots[0]=(EffectSlot){.status=EF_READY,.count=16,.presentation_count=16,.key=11,.ap=12};snapshot.effects.slots[1].status=EF_EMPTY;
 McuControllerCollection saved={.count=2};mcu_controller_default(saved.records);mcu_controller_default(saved.records+1);saved.records[0].input.profile=saved.records[0].output.profile=&mcu_profile_xtouch_mini;
 collection_init(&surfaces,&saved);Surface *surface=surfaces.surfaces;MirrorBank *bank=surfaces.banks;MirrorInput *in=&surfaces.contexts[0].input;
 surface->seq=(snd_seq_t*)1;surface->source=3;surface->full=(snd_seq_addr_t){32,0};surface->output=surface->full;
 ControllerRuntime runtime={.snapshot=&snapshot,.snapshot_attempted=1,.snapshot_result=1,.now=100};
 const unsigned top[8]={89,90,40,41,42,43,44,45};
#define MINI_FEED(status,key,value) mini_feed(surface,bank,state,&runtime,status,key,value)
#define MINI_TAP(note) do{MINI_FEED(0x90,note,127);MINI_FEED(0x80,note,0);}while(0)
#define MINI_MENU(layer,index) do{MINI_FEED(0x90,layer,127);MINI_TAP(top[index]);MINI_FEED(0x80,layer,0);}while(0)
 mini_feedback(surface,bank,&snapshot);need(mini_mode(bank)==0&&surface->mini_button_mode==0&&observed_led[89]==127,"Mini defaults Volume and selected-track feedback");
 for(unsigned mode=0;mode<6;mode++){
  MINI_MENU(84,mode);need(mini_mode(bank)==mode&&!bank->flip,"A menu selects each knob mode without release cycling");
  MINI_FEED(0x90,84,127);mini_feedback(surface,bank,&snapshot);for(unsigned i=0;i<8;i++)need(observed_led[top[i]]==(i==mode?127u:0u),"A menu exclusively owns top-row lamps");
  MINI_TAP(top[mode]);MINI_FEED(0x80,84,0);
  for(unsigned buttons=0;buttons<4;buttons++){
   MINI_MENU(85,buttons);MINI_FEED(0x90,85,127);mini_feedback(surface,bank,&snapshot);for(unsigned i=0;i<8;i++)need(observed_led[top[i]]==(i==buttons?127u:0u),"B menu exclusively owns top-row lamps");
   MINI_TAP(top[buttons]);MINI_FEED(0x80,85,0);mini_feedback(surface,bank,&snapshot);for(unsigned i=0;i<8;i++)need(observed_led[top[i]]==(i==buttons?127u:0u),"normal feedback restores selected muted solo or armed track");
   mini_clean_commands(in);MINI_TAP(top[3]);CommandRequest r=mini_published(in,bank,&snapshot,buttons==0?CF_SELECTION:buttons==1?CF_MUTE:buttons==2?CF_SOLO:CF_ARM);need(r.serial==4&&r.program_owner==snapshot.tracks[3].program_owner,"all knob modes retain track-button identity, including Sends/Inserts");if(buttons==3)need(r.track_owner==snapshot.tracks[3].track_owner&&r.field_incarnation==snapshot.tracks[3].fields[CF_ARM].incarnation&&!r.bits,"Record uses native per-track arm identity and toggles armed to off");
  }
  mini_clean_commands(in);MINI_MENU(85,0);
  if(mode<4){
   MINI_FEED(0xb0,0x17,1);unsigned field=mode==0?CF_VOLUME:mode==1?CF_PAN:QLINK_VALUE;CommandRequest r=mini_published(in,bank,&snapshot,field);
   need(mode<2?r.serial==8:r.qlink_index==(mode==2?7u:15u),"all mixer/Q-Link modes publish exact knob target");
   mini_feedback(surface,bank,&snapshot);unsigned expected=mode==0?6:mode==1?70:mode==2?4:9;need(observed_ring[7]==(int)expected,"rings use authoritative values while command pending");
   mini_clean_commands(in);MINI_TAP(0x23);if(mode<2){r=mini_published(in,bank,&snapshot,CF_SELECTION);need(r.serial==4&&!in->desires[3][field].pending,"Volume/Pan push selects track without resetting level");}else need(!input_pending(in),"Q-Link push has no reset or command");
   uint32_t *bits=mode>=2?&snapshot.qlinks.slots[(mode-2)*8].bits:mode==1?&snapshot.tracks[0].fields[CF_PAN].bits:&snapshot.tracks[0].bits;unsigned old=*bits;*bits=0x3f800000;mini_feedback(surface,bank,&snapshot);need(observed_ring[0]==11,"external MPC value reaches active ring");*bits=old;
  }
  if(mode==4){
   mini_feedback(surface,bank,&snapshot);for(unsigned i=0;i<8;i++)need(observed_ring[i]==(i<4?4:9),"Sends rings separate selected sends and return volumes");
   for(unsigned i=0;i<8;i++){mini_clean_commands(in);MINI_FEED(0xb0,0x10+i,1);CommandRequest r=mini_published(in,bank,&snapshot,i<4?CF_SEND1+i:CF_VOLUME);need(r.serial==(i<4?1:i+13),"Sends knob command targets selected track or correct return");mini_clean_commands(in);MINI_TAP(0x20+i);need(!input_pending(in),"Send/return push never resets a value");}
   unsigned dest=snapshot.send_programs[0];snapshot.send_programs[0]=0;mini_feedback(surface,bank,&snapshot);need(!observed_ring[4],"missing return clears ring");snapshot.send_programs[0]=dest;
  }
  mini_clean_commands(in);MINI_FEED(0x90,85,127);MINI_FEED(0x90,92,127);need(bank->offset==8,"B Forward pages tracks in every knob mode");MINI_FEED(0x80,85,0);MINI_FEED(0x90,92,127);need(!in->jog_forward&&!in->jog_count,"modifier-first release and repeated nav down do not become transport");MINI_FEED(0x80,92,0);
  MINI_MENU(85,3);need(surface->mini_button_mode==3,"B top4 selects Record without release cycle");mini_feedback(surface,bank,&snapshot);need(observed_led[top[3]]==127,"Record lights arm-capable MIDI track in paged bank");mini_clean_commands(in);MINI_TAP(top[3]);CommandRequest arm=mini_published(in,bank,&snapshot,CF_ARM);need(arm.serial==12&&arm.track_owner==snapshot.tracks[11].track_owner&&arm.field_incarnation==snapshot.tracks[11].fields[CF_ARM].incarnation&&!arm.bits,"every knob mode arms MIDI track in current bank through native CF_ARM owner");
  mini_feedback(surface,bank,&snapshot);need(observed_led[top[3]]==127,"pending arm command does not optimistically change armed LED");snapshot.tracks[11].fields[CF_ARM].bits=0;mini_feedback(surface,bank,&snapshot);need(!observed_led[top[3]],"native disarm source change clears Record LED");mini_clean_commands(in);MINI_TAP(top[3]);arm=mini_published(in,bank,&snapshot,CF_ARM);need(arm.bits==1,"unarmed MIDI track toggles on");snapshot.tracks[11].fields[CF_ARM].bits=1;mini_feedback(surface,bank,&snapshot);need(observed_led[top[3]]==127,"native arm source change lights Record LED");
  snapshot.tracks[11].fields[CF_ARM].available=0;mini_feedback(surface,bank,&snapshot);need(!observed_led[top[3]],"unavailable native arm field stays dark even with old armed bit");mini_clean_commands(in);MINI_TAP(top[3]);need(!input_pending(in),"unavailable native arm target has no action");snapshot.tracks[11].fields[CF_ARM].available=1;
  MINI_MENU(85,0);
  MINI_FEED(0x90,85,127);MINI_TAP(91);MINI_FEED(0x80,85,0);need(bank->offset==0&&surface->mini_button_mode==0,"B Rewind restores track bank without button-mode cycle");
 }
 mini_clean_commands(in);MINI_MENU(85,0);for(unsigned i=4;i<8;i++){MINI_MENU(85,i);need(surface->mini_button_mode==0&&!input_pending(in),"unused B menu buttons remain dark inert and consume release");}
 MINI_MENU(85,3);snapshot.tracks[15].vptr=MODEL_SENDPROGRAM_VPTR;MINI_FEED(0x90,85,127);MINI_TAP(92);MINI_FEED(0x80,85,0);mini_feedback(surface,bank,&snapshot);need(!observed_led[top[7]],"absent Record track-bank slot is dark");MINI_TAP(top[7]);need(!input_pending(in),"absent Record track-bank slot has no action");snapshot.tracks[15].vptr=MODEL_DRUMPROGRAM_VPTR;MINI_FEED(0x90,85,127);MINI_TAP(91);MINI_FEED(0x80,85,0);
 mini_clean_commands(in);MINI_MENU(85,0);MINI_MENU(84,0);
 for(unsigned i=0;i<6;i++){MINI_FEED(0x90,84,127);MINI_FEED(0x90,84,127);need(mini_mode(bank)==i,"A repeats do not cycle before release");MINI_FEED(0x80,84,0);MINI_FEED(0x90,84,0);need(mini_mode(bank)==(i+1)%6,"A bare release advances six modes exactly once");}
 for(unsigned i=0;i<4;i++){MINI_FEED(0x90,85,127);MINI_FEED(0x90,85,127);need(surface->mini_button_mode==i,"B repeats do not cycle before release");MINI_FEED(0x80,85,0);MINI_FEED(0x90,85,0);need(surface->mini_button_mode==(i+1)%4,"B bare release cycles four modes and wraps exactly once");}
 MINI_FEED(0x90,85,127);MINI_FEED(0x90,top[3],127);MINI_FEED(0x90,top[3],127);MINI_FEED(0x80,85,0);MINI_FEED(0x80,top[3],0);need(surface->mini_button_mode==3,"Record menu repeat and modifier-first release do not cycle or arm track");mini_clean_commands(in);MINI_TAP(95);(void)mini_published(in,bank,&snapshot,GLOBAL_RECORD_TOGGLE);need(surface->mini_button_mode==3,"bottom Record remains transport in track Record mode");mini_clean_commands(in);MINI_MENU(85,0);
 MINI_FEED(0x90,84,127);MINI_FEED(0x90,85,127);mini_feedback(surface,bank,&snapshot);need(observed_led[top[0]]==127,"both-held menu has deterministic A precedence");MINI_TAP(92);MINI_TAP(94);MINI_FEED(0x80,84,0);MINI_FEED(0x80,85,0);need(mini_mode(bank)==0&&surface->mini_button_mode==0&&!in->jog_forward&&!in->global_count,"both-held releases consume without accidental transport or cycle");
 for(unsigned mode=2;mode<4;mode++){
  MINI_MENU(84,mode);mini_clean_commands(in);MINI_FEED(0x90,84,127);MINI_TAP(92);MINI_FEED(0x80,84,0);CommandRequest r=mini_published(in,bank,&snapshot,QLINK_MODE);need(r.bits==1&&r.global_owner==9&&mini_mode(bank)==mode,"A Forward uses native Q-Link mode owner without bank change");
  snapshot.qlinks.slots[(mode-2)*8].sampled=0;mini_feedback(surface,bank,&snapshot);need(!observed_ring[0],"unavailable Q-Link clears ring");snapshot.qlinks.slots[(mode-2)*8].sampled=1;
 }
 mini_clean_commands(in);snapshot.qlinks_available=0;MINI_MENU(84,2);MINI_FEED(0xb0,0x10,1);mini_feedback(surface,bank,&snapshot);need(!input_pending(in)&&!observed_ring[0],"Q-Link mode enters safely before source acquisition");snapshot.qlinks_available=1;mini_feedback(surface,bank,&snapshot);need(observed_ring[0]==4,"Q-Link source acquisition updates ring without mode re-entry");MINI_FEED(0xb0,0x10,1);(void)mini_published(in,bank,&snapshot,QLINK_VALUE);
 // Independent native helper results for format3 (eight controls), directions
 // 0..3. In particular, Column page1 crosses both native contiguous halves.
 static const unsigned screen_order[4][16]={
  {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
  {12,13,14,15,8,9,10,11,4,5,6,7,0,1,2,3},
  {12,8,4,0,13,9,5,1,14,10,6,2,15,11,7,3},
  {0,4,8,12,1,5,9,13,2,6,10,14,3,7,11,15}};
 snapshot.qlinks.mode_id=0;
 for(unsigned i=0;i<16;i++){float value=i/16.f;memcpy(&snapshot.qlinks.slots[i].bits,&value,4);}
 for(unsigned direction=0;direction<4;direction++)for(unsigned page=0;page<2;page++){
  snapshot.qlinks.screen_direction=direction+1;MINI_MENU(84,2+page);mini_feedback(surface,bank,&snapshot);
  for(unsigned strip=0;strip<8;strip++){
   unsigned index=screen_order[direction][page*8+strip];mini_clean_commands(in);MINI_FEED(0xb0,0x10+strip,1);
   CommandRequest request=mini_published(in,bank,&snapshot,QLINK_VALUE);
   need(request.qlink_index==index&&request.before_bits==snapshot.qlinks.slots[index].bits,"Mini Screen turn retains mapped native index and value");
   need(observed_ring[strip]==(int)(index/16.f*10+.5f)+1,"Mini Screen ring uses same native index as turn");
  }
  input_qlink_interest(in,bank,&snapshot,100,1);QLinkDemand demand;
  need(qlink_demand_read(&commands->qlink_interest,&demand)&&demand.enabled&&demand.page==(direction?2:page),"Mini mapped Screen requests bounded all-slot sampling, linear direction retains contiguous demand");
 }
 mini_clean_commands(in);snapshot.qlinks.screen_direction=3;MINI_MENU(84,2);MINI_FEED(0xb0,0x10,1);need(in->qlinks[12].pending,"Column turn is pending in native slot13");
 snapshot.qlinks.screen_direction=4;input_submit(in,bank,&snapshot,100);
 need(!in->qlinks[12].pending&&!atomic_load(&commands->published),"direction change revokes unsent intent even with identical page membership");
 MINI_FEED(0xb0,0x10,1);CommandRequest mapped=mini_published(in,bank,&snapshot,QLINK_VALUE);need(mapped.qlink_index==0,"next turn follows changed native direction");
 snapshot.qlinks.screen_direction=3;input_submit(in,bank,&snapshot,100);need(input_pending(in)==1,"already-published native command survives mapping change for settlement");
 mini_clean_commands(in);MINI_FEED(0xb0,0x10,1);MINI_MENU(84,3);input_submit(in,bank,&snapshot,100);need(!atomic_load(&commands->published),"Mini bank switch discards old unsent mapping");
 for(unsigned fallback=0;fallback<4;fallback++){
  snapshot.qlinks.mode_id=fallback==0?2:0;snapshot.qlinks.screen_direction=fallback==1?0:fallback==2?5:3;snapshot.qlinks.mode_tick=fallback==3?101:100;
  mini_clean_commands(in);MINI_MENU(84,2);MINI_FEED(0xb0,0x10,1);mapped=mini_published(in,bank,&snapshot,QLINK_VALUE);need(mapped.qlink_index==0,"non-Screen, unknown, unsupported or unfresh direction preserves original order");
  input_qlink_interest(in,bank,&snapshot,100,1);need(atomic_load(&commands->qlink_interest.page)==0,"fallback retains ordinary sampling interest");
 }
 snapshot.qlinks.mode_tick=100;snapshot.qlinks.screen_direction=3;mini_clean_commands(in);
 MirrorBank ordinary=*bank;ordinary.qlink_screen_order=0;input_qlink_edit(in,&ordinary,&snapshot,0,1,-1,100);mapped=mini_published(in,&ordinary,&snapshot,QLINK_VALUE);need(mapped.qlink_index==0,"other controller keeps original order under native Column direction");
 input_qlink_interest(in,bank,&snapshot,100,0);need(!atomic_load(&commands->qlink_interest.enabled),"disconnected Mini withdraws mapped demand");
 // Full MCU uses the same projection for encoders, LCD/rings and Flip's
 // fader identities, motor targets and native-index receipt barrier.
 Surface *full=surfaces.surfaces+1;MirrorBank *full_bank=surfaces.banks+1;MirrorInput *full_input=&surfaces.contexts[1].input;
 full->seq=(snd_seq_t*)1;full->source=4;full->full=(snd_seq_addr_t){33,0};full->output=full->full;
 snapshot.qlinks.mode_id=0;snapshot.qlinks.screen_direction=3;
 for(unsigned i=0;i<16;i++)snprintf(snapshot.qlinks.slots[i].name,QLINK_TEXT,"P%02u",i);
 surface_assignment(full,full_input,full_bank,&snapshot,44,100);
 for(unsigned page=0;page<2;page++){
  surface_navigation(full_input,full_bank,&snapshot,page,100);bank_apply(full_bank,&snapshot,100);
  full->context->mode_notice_controller=9;full->context->mode_notice_generation=10;full->context->mode_notice_epoch=1;full->context->mode_notice_id=0;full->context->mode_notice_until=0;
  memset(full->wire_valid,0,sizeof(full->wire_valid));for(unsigned strip=0;strip<8;strip++)need(channel_output(full,full_bank,&snapshot,100),"full MCU Screen wire output");
  for(unsigned strip=0;strip<8;strip++){
   unsigned index=screen_order[2][8*page+strip];char label[8];snprintf(label,sizeof(label),"P%02u    ",index);
   need(!memcmp(observed_lcd+7*strip,label,7)&&observed_ring[strip]==(int)(index/16.f*10+.5f)+1,"full MCU LCD and ring use mapped native slot");
   mini_clean_commands(full_input);surface_encoder(full,full_input,full_bank,&snapshot,strip,1,0,100);mapped=mini_published(full_input,full_bank,&snapshot,QLINK_VALUE);need(mapped.qlink_index==index,"full MCU encoder keeps mapped native command index");
  }
  mini_clean_commands(full_input);surface_assignment(full,full_input,full_bank,&snapshot,50,100);bank_apply(full_bank,&snapshot,100);
  for(unsigned strip=0;strip<8;strip++){
   unsigned index=screen_order[2][8*page+strip];need(full_bank->faders[strip].effect[1]==index&&bank_target(full_bank,strip,100)==(int)(index/16.f*16383+.5f),"Flip identity and motor target use mapped native value");
   mini_clean_commands(full_input);input_touch(full_input,full_bank,&snapshot,strip,1,100);need(surface_pitch(full,full_input,full_bank,&snapshot,strip,16000-page*100-strip,100),"full MCU Flip fader input");
   mapped=mini_published(full_input,full_bank,&snapshot,QLINK_VALUE);need(mapped.qlink_index==index,"Flip publishes mapped native slot");input_touch(full_input,full_bank,&snapshot,strip,0,100);
  }
  mini_clean_commands(full_input);unsigned outputNow=101+page*20;snapshot.heartbeat=outputNow;bank_apply(full_bank,&snapshot,outputNow);motor_follow_reset(&full->context->following);full->context->following.enabled=1;
  for(unsigned strip=0;strip<8;strip++){unsigned index=screen_order[2][8*page+strip];need(strip_motor_output(full,full_bank,strip,outputNow)==1&&observed_pitch[strip]==(int)(index/16.f*16383+.5f),"actual MCU motor pitch follows mapped native slot");}
  snapshot.heartbeat=100;surface_assignment(full,full_input,full_bank,&snapshot,50,100);
 }
 mini_clean_commands(full_input);surface_assignment(full,full_input,full_bank,&snapshot,40,100);bank_apply(full_bank,&snapshot,100);full->seq=NULL;memset(full->wire_valid,0,sizeof(full->wire_valid));
 puts("PASS full MCU Screen projection: both Column banks align encoder commands, LCD/rings, Flip fader requests, native motor identities and emitted pitch positions (native state and ALSA endpoints substituted)");
 mini_clean_commands(in);snapshot.qlinks.mode_id=2;snapshot.qlinks.screen_direction=0;
 for(unsigned i=0;i<16;i++)snapshot.qlinks.slots[i].bits=i<8?0x3e800000:0x3f400000;
 puts("PASS Mini Screen order: all four native directions and both banks compose real MCU input, native command indices, ring feedback and bounded interest; direction/page changes revoke unsent intents, preserve published receipts; unknown/non-Screen/other-controller order unchanged (native state and ALSA endpoints substituted)");
 mini_clean_commands(in);snapshot.effects_available=0;MINI_MENU(84,5);MINI_TAP(32);mini_feedback(surface,bank,&snapshot);need(bank->effects_slot==EFFECT_LIST&&!input_pending(in)&&!observed_ring[0],"Inserts waits safely for native source acquisition");snapshot.effects_available=1;MINI_TAP(32);need(bank->effects_slot==0,"Insert push opens occupied native slot after late acquisition");snapshot.effects.slot=0;
 for(unsigned i=0;i<8;i++)snapshot.effects.parameters[i]=(EffectParameter){.status=EF_READY,.index=i,.position=i,.tick=100,.bits=0x3e800000,.revision=2,.steps=2};
 MINI_FEED(0xb0,0x17,1);CommandRequest r=mini_published(in,bank,&snapshot,EFFECT_PARAMETER);need(r.effect_slot==0&&r.effect_index==7&&r.serial==1,"Insert turn publishes native parameter identity");mini_feedback(surface,bank,&snapshot);need(observed_ring[7]==4,"Insert ring reads authoritative parameter");
 mini_clean_commands(in);MINI_TAP(32);r=mini_published(in,bank,&snapshot,EFFECT_PARAMETER);need(r.bits==0x3f800000,"Insert push toggles native two-state parameter");
 mini_clean_commands(in);snapshot.effects.parameters[0].steps=3;MINI_TAP(32);need(!input_pending(in),"Insert push does not reset continuous/multistate parameter");
 MINI_FEED(0x90,84,127);MINI_FEED(0x90,92,127);MINI_FEED(0x80,84,0);MINI_FEED(0x90,92,127);MINI_FEED(0x80,92,0);need(bank->effects_page==1&&!in->jog_forward,"A Forward pages insert parameters without transport leak");
 MINI_FEED(0x90,84,127);MINI_TAP(91);MINI_FEED(0x80,84,0);need(!bank->effects_page,"A Rewind returns insert parameter page");
 MINI_MENU(84,5);snapshot.effects.slot=EFFECT_LIST;need(bank->effects_slot==EFFECT_LIST,"selecting Inserts again returns slot list");mini_clean_commands(in);MINI_TAP(33);r=mini_published(in,bank,&snapshot,EFFECT_CHOOSER);need(r.effect_slot==1&&!r.bits,"empty insert opens native chooser");
 const unsigned action_notes[]={87,88,86,95},operations[]={GLOBAL_UNDO,GLOBAL_REDO,GLOBAL_LOOP_TOGGLE,GLOBAL_RECORD_TOGGLE};
 for(unsigned i=0;i<4;i++){mini_clean_commands(in);MINI_TAP(action_notes[i]);(void)mini_published(in,bank,&snapshot,operations[i]);}
 for(unsigned i=6;i<8;i++){mini_clean_commands(in);MINI_MENU(84,i);(void)mini_published(in,bank,&snapshot,i==6?GLOBAL_SAVE:GLOBAL_TRACK_NEW);need(mini_mode(bank)==5,"Save/Add Track chord preserves knob mode");}
 mini_clean_commands(in);MINI_TAP(94);(void)mini_published(in,bank,&snapshot,GLOBAL_PLAY);
 mini_clean_commands(in);snapshot.playing.bits=1;MINI_TAP(93);(void)mini_published(in,bank,&snapshot,GLOBAL_STOP);snapshot.playing.bits=0;mini_clean_commands(in);MINI_TAP(93);need(in->stop_home,"stopped Stop retains native return-to-start path");
 mini_clean_commands(in);MINI_FEED(0x90,91,127);need(in->jog_rewind,"ordinary Rewind retains repeat state");MINI_FEED(0x80,91,0);need(!in->jog_rewind,"Rewind release stops repeat");unsigned before_mode=mini_mode(bank);MINI_FEED(0x90,92,127);MINI_FEED(0x90,84,127);need(!in->jog_forward,"holding modifier stops an already-held transport gesture");MINI_FEED(0x80,84,0);MINI_FEED(0x80,92,0);need(mini_mode(bank)==before_mode,"navigation-first chord also consumes the Layer release");
 mini_clean_commands(in);MINI_FEED(0xe8,0,127);r=mini_published(in,bank,&snapshot,CF_MASTER);need(r.bits==0x3f800000,"Mini master retains calibrated full range");
 MINI_MENU(84,0);MINI_MENU(85,1);snapshot.tracks[1].fields[CF_MUTE].available=0;mini_feedback(surface,bank,&snapshot);need(!observed_led[top[1]],"unavailable track field has no light");mini_clean_commands(in);MINI_TAP(top[1]);need(!input_pending(in),"unavailable track field has no action");snapshot.tracks[1].fields[CF_MUTE].available=1;
 snapshot.ready=0;MINI_FEED(0x90,84,127);mini_feedback(surface,bank,&snapshot);need(!observed_ring[0]&&!observed_led[top[1]],"unready snapshot clears Mini feedback");snapshot.ready=1;MINI_FEED(0x80,84,0);need(mini_mode(bank)==0,"unready modifier press cannot cause delayed cycle");
 need(surfaces.banks[1].assignment==BA_TRACK&&!surfaces.banks[1].flip&&surface_encoder_field(surfaces.surfaces+1,surfaces.banks+1,0)==CF_PAN&&!surfaces.contexts[1].input.global_count,"Mini state does not leak into full X-Touch");
 surface_close(surface,bank);mini_feedback(surface,bank,&snapshot);need(mini_mode(bank)==0&&surface->mini_button_mode==0&&!surface->general_down[84]&&observed_ring[0]==6&&observed_led[top[0]]==127,"reconnect restores Volume and Select with fresh feedback");
 mini_led_clear_checks(surface,surfaces.surfaces+1,surfaces.banks+1,&snapshot);
#undef MINI_FEED
#undef MINI_TAP
#undef MINI_MENU
 free(state);free(commands);puts("PASS complete Mini production layout: six knob modes, independent Select/Mute/Solo/Record across all assignments and banks, arm-capable MIDI feedback, exact send/return/Q-Link/insert/native action targets, safe pushes, both menus and release order, navigation chords, authoritative LEDs/rings, reconnect and other-profile isolation (ALSA hardware and native source/receipts substituted)");
}
