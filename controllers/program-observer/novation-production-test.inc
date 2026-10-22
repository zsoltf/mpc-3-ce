/* Exact compiled bridge functions; ALSA inventory/output and MPC publication
 * are substituted. This is software qualification, not physical admission. */
static unsigned native_captured(unsigned client,const unsigned char *bytes,unsigned length){unsigned found=0;for(unsigned i=0;i<native_capture_count;i++)if(native_capture[i].destination.client==client&&native_capture[i].length==length&&!memcmp(native_capture[i].bytes,bytes,length))found++;return found;}
#define NATIVE_HAS(client,...) do{const unsigned char expected[]={__VA_ARGS__};if(!native_captured(client,expected,sizeof(expected))){fprintf(stderr,"missing native output line=%u client=%u bytes=",__LINE__,(unsigned)(client));for(unsigned j=0;j<sizeof(expected);j++)fprintf(stderr,"%02x",expected[j]);fputc('\n',stderr);}need(native_captured(client,expected,sizeof(expected)),"production native output exact bytes");}while(0)
static void native_push(unsigned instance,unsigned status,unsigned index,unsigned value){snd_seq_event_t event;snd_seq_ev_clear(&event);if((status&0xf0)==0xb0)snd_seq_ev_set_controller(&event,status&15,index,value);else snd_seq_ev_set_noteon(&event,status&15,index,value);event.source=surfaces.surfaces[instance].full;fake_input_push(event);}
static void launchpad_dark_grid_frame(unsigned surface,const CopiedMirror *snapshot,unsigned now){
 native_capture_count=0;
 for(unsigned pass=0;pass<20;pass++){
  need(native_output(surfaces.surfaces+surface,surfaces.banks+surface,snapshot,now),"drum rendering reaches captured MIDI output");
  need(surfaces.surfaces[surface].novation.cycle_messages<=16&&surfaces.surfaces[surface].novation.cycle_bytes<=128,"dark drum feedback preserves output budget");
 }
}
static void launchpad_dark_steps_checks(CopiedMirror *snapshot,unsigned now){
 for(unsigned i=1;i<=2;i++)for(unsigned mode=NOV_STEP64;mode<=NOV_STEP16;mode++){
  NovationState *n=&surfaces.surfaces[i].novation;unsigned client=i==1?151:152;
  n->layer=mode;n->step_pad=2;n->step_first=0;memset(n->led_valid,0,sizeof(n->led_valid));
  snapshot->steps_available=1;snapshot->steps=(StepCopy){.status=STEP_READY,.epoch=snapshot->epoch,.tick=now};
  StepProjection *v=snapshot->steps.views+i;*v=(StepProjection){.status=STEP_READY,.tick=now,.pad=2,.notes={36,37,38,39},.valid={UINT32_MAX,UINT32_MAX},.on={{2,0},{2,0},{2,0},{2,0}}};
  v->target=(StepTarget){.event_owner=20,.bind_generation=n->step_bind_generation,.division_bits=0x41800000,.clock_available=1};
  snapshot->playing.available=1;snapshot->playing.bits=0;launchpad_dark_grid_frame(i,snapshot,now);
  for(unsigned cell=0;cell<64;cell++){unsigned pad=(8-cell/8)*10+cell%8+1;const unsigned colors[]={13,17,21,25};unsigned sound=mode==NOV_STEP16?cell/16:0,step=mode==NOV_STEP16?cell%16:cell;NATIVE_HAS(client,0x90,pad,step==1?colors[sound]:0);}
  /* An inactive but valid cell must carry the moving playhead. */
  snapshot->playing.bits=1;launchpad_dark_grid_frame(i,snapshot,now);NATIVE_HAS(client,0x90,81,45);
  if(mode==NOV_STEP16){NATIVE_HAS(client,0x90,61,45);NATIVE_HAS(client,0x90,41,45);NATIVE_HAS(client,0x90,21,45);}
  v->target.clock_value[0]=240;launchpad_dark_grid_frame(i,snapshot,now);NATIVE_HAS(client,0x90,81,0);NATIVE_HAS(client,0x90,82,45);
  snapshot->playing.bits=0;launchpad_dark_grid_frame(i,snapshot,now);NATIVE_HAS(client,0x90,82,13);
  v->target.clock_value[0]=0;v->valid[0]&=~1u;snapshot->playing.bits=1;memset(n->led_valid,0,sizeof(n->led_valid));
  launchpad_dark_grid_frame(i,snapshot,now);NATIVE_HAS(client,0x90,81,0);need(!native_captured(client,(unsigned char[]){0x90,81,45},3),"invalid cell cannot acquire a playhead");
  snapshot->steps_available=0;launchpad_dark_grid_frame(i,snapshot,now);NATIVE_HAS(client,0x90,82,0);
  snapshot->steps_available=1;memset(n->led_valid,0,sizeof(n->led_valid));launchpad_dark_grid_frame(i,snapshot,now+STEP_FRESH_MS);
  for(unsigned cell=0;cell<64;cell++){unsigned pad=(8-cell/8)*10+cell%8+1;NATIVE_HAS(client,0x90,pad,0);}
 }
 puts("PASS Pro and X64/16 emitted drum LEDs: inactive dark, active lit, inactive-cell moving playhead, stopped active restoration, invalid/unavailable/stale dark; ALSA sink and native snapshots substituted");
}
static void launchpad_step_checks(const MirrorState *state,ControllerRuntime *runtime){
 CopiedMirror *snapshot=runtime->snapshot;StepCopy saved=snapshot->steps;unsigned available=snapshot->steps_available;MIRROR_FIELD(playing);playing=snapshot->playing;
 NovationState old[2]={surfaces.surfaces[1].novation,surfaces.surfaces[2].novation};unsigned now=runtime->now;
 snapshot->steps_available=1;snapshot->steps=(StepCopy){.status=STEP_READY,.epoch=snapshot->epoch,.tick=now,.project_owner=1,.track_owner=1,.program_owner=1,.serial=1,.binding=1,.incarnation=1,.control=10,.event_owner=20,.sequence=30,.division_bits=0x41800000,.extent={15360,0},.event_generation=7};
 for(unsigned i=1;i<=2;i++){StepProjection *v=snapshot->steps.views+i;*v=(StepProjection){.status=STEP_READY,.tick=now,.pad=i==1?2:6,.first_step=0,.generation=7,.valid={UINT32_MAX,UINT32_MAX}};for(unsigned j=0;j<4;j++)v->notes[j]=36+v->pad+j;surfaces.surfaces[i].novation.step_epoch=snapshot->epoch;surfaces.surfaces[i].novation.step_program=1;surfaces.surfaces[i].novation.step_first=v->first_step;surfaces.surfaces[i].novation.step_pad=v->pad;memset(surfaces.surfaces[i].novation.held,0,128);input_discard(&surfaces.contexts[i].input);}
 fake_input_reset();native_push(1,0xb0,96,127);native_push(2,0xb0,97,127);native_push(2,0xb0,79,127);need(collection_drain(&surfaces,state,runtime)&&surfaces.surfaces[1].novation.layer==NOV_STEP64&&surfaces.surfaces[2].novation.layer==NOV_STEP16,"model-specific physical mode buttons retain independent step views");
 fake_input_reset();native_push(2,0xb0,92,127);need(collection_drain(&surfaces,state,runtime)&&surfaces.surfaces[2].novation.step_first==16,"four-sound page moves without rebinding the chosen target");snapshot->steps.views[2].first_step=16;
 collection_step_interest(&surfaces,snapshot,now);StepDemand demand;need(step_demand_read(&input.commands->step_interest,&demand)&&demand.enabled==6&&demand.pad[1]==2&&demand.pad[2]==6&&demand.first_step[2]==16,"one shared demand retains both surface windows");
 fake_input_reset();native_push(1,0x90,41,127);native_push(1,0x90,41,127);native_push(2,0x90,18,127);need(collection_drain(&surfaces,state,runtime),"documented notes reach production step dispatch");
 MirrorInput *x=&surfaces.contexts[1].input,*pro=&surfaces.contexts[2].input;need(x->step_count==1&&pro->step_count==1,"held presses enqueue one edge per surface");
 const CommandRequest *a=&x->step_events[x->step_head].request,*b=&pro->step_events[pro->step_head].request;
 need(a->pad_index==2&&a->effect_ap==38&&a->qlink_index==32&&b->pad_index==9&&b->effect_ap==45&&b->qlink_index==31,"single64/four16 exact row-column maps preserve independently pinned drums and time");
 native_capture_count=0;for(unsigned pass=0;pass<20;pass++)for(unsigned i=1;i<=2;i++){need(native_output(surfaces.surfaces+i,surfaces.banks+i,snapshot,now),"step grid source projection");need(surfaces.surfaces[i].novation.cycle_messages<=16&&surfaces.surfaces[i].novation.cycle_bytes<=128,"step LEDs retain existing output limits");}
 NATIVE_HAS(151,0x90,41,0);NATIVE_HAS(152,0x90,18,0);need(!native_captured(151,(unsigned char[]){0x90,41,13},3),"unsettled input cannot invent a lit native step");
 snapshot->steps.views[1].on[0][1]=1;snapshot->steps.views[2].on[3][0]=1u<<15;native_capture_count=0;
 for(unsigned pass=0;pass<16;pass++)for(unsigned i=1;i<=2;i++)need(native_output(surfaces.surfaces+i,surfaces.banks+i,snapshot,now),"changed native step LEDs");
 NATIVE_HAS(151,0x90,41,13);NATIVE_HAS(152,0x90,18,41);
 snapshot->playing.available=1;snapshot->playing.bits=1;snapshot->steps.clock_available=1;snapshot->steps.clock_value[0]=32*240;native_capture_count=0;
 for(unsigned pass=0;pass<16;pass++)for(unsigned i=1;i<=2;i++)need(native_output(surfaces.surfaces+i,surfaces.banks+i,snapshot,now),"native local-clock overlay");
 NATIVE_HAS(151,0x90,41,45);need(!native_captured(152,(unsigned char[]){0x90,81,45},3),"playhead never wraps into a foreign16-step page");
 snapshot->playing.bits=0;native_capture_count=0;for(unsigned pass=0;pass<16;pass++)need(native_output(surfaces.surfaces+1,surfaces.banks+1,snapshot,now),"stopped playhead clear");NATIVE_HAS(151,0x90,41,13);
 snapshot->steps.views[1].status=STEP_UNAVAILABLE;native_capture_count=0;for(unsigned pass=0;pass<16;pass++)need(native_output(surfaces.surfaces+1,surfaces.banks+1,snapshot,now),"unavailable source clears grid");NATIVE_HAS(151,0x90,41,0);
 snapshot->steps.status=STEP_UNAVAILABLE;snapshot->steps.division_bits=0;
 fake_input_reset();native_push(1,0xb0,94,127);need(collection_drain(&surfaces,state,runtime)&&surfaces.surfaces[1].novation.step_first==0,"unavailable division refuses page navigation without arithmetic");
 snapshot->steps.status=STEP_READY;snapshot->steps.views[1].status=STEP_READY;
 fake_input_reset();native_push(1,0xb0,94,0);native_push(1,0xb0,94,127);need(collection_drain(&surfaces,state,runtime)&&surfaces.surfaces[1].novation.step_first==0,"invalid ready division refuses page navigation");
 snapshot->steps.division_bits=0x41800000;
 fake_input_reset();native_push(1,0xb0,94,0);native_push(1,0xb0,94,127);need(collection_drain(&surfaces,state,runtime)&&surfaces.surfaces[1].novation.step_first==0,"next page beyond native extent is refused");
 fake_input_reset();native_push(1,0xb0,95,127);native_push(2,0xb0,93,127);need(collection_drain(&surfaces,state,runtime)&&surfaces.surfaces[1].novation.layer==NOV_CLIPS&&surfaces.surfaces[2].novation.layer==NOV_CLIPS&&!x->step_count&&!pro->step_count,"physical Session returns both surfaces to clips and cancels unsubmitted steps");
 launchpad_dark_steps_checks(snapshot,now);
 for(unsigned i=1;i<=2;i++)surfaces.surfaces[i].novation=old[i-1];
 snapshot->steps=saved;snapshot->steps_available=available;snapshot->playing=playing;
 puts("PASS both Launchpads: actual selected ALSA decoder -> independent64/four16 intents, shared demand, native-only lit steps, bounded changed LEDs, local playhead and Session return; ALSA/native state substituted");
}
static void launchpad_pro_rebind_checks(const MirrorState *state,ControllerRuntime *runtime){
 CopiedMirror *s=runtime->snapshot;StepCopy saved=s->steps;unsigned available=s->steps_available;NovationState old[2]={surfaces.surfaces[1].novation,surfaces.surfaces[2].novation};unsigned now=runtime->now;
 s->steps_available=1;s->steps=(StepCopy){.status=STEP_READY,.epoch=s->epoch,.tick=now,.event_generation=7};
 for(unsigned i=1;i<=2;i++){
  NovationState *n=&surfaces.surfaces[i].novation;n->layer=NOV_STEP64;n->step_bind_generation=20+i;n->step_pad=6;n->step_first=32;memset(n->held,0,sizeof(n->held));input_discard(&surfaces.contexts[i].input);
  StepProjection *v=s->steps.views+i;*v=(StepProjection){.status=STEP_READY,.tick=now,.pad=6,.first_step=32,.notes={42,43,44,45},.valid={UINT32_MAX,UINT32_MAX}};
  v->target=(StepTarget){.epoch=s->epoch,.project_owner=1,.track_owner=i,.program_owner=i,.serial=i,.binding=1,.incarnation=1,.control=10,.event_owner=20+i,.sequence=30,.division_bits=0x41800000,.extent={15360,0},.bind_generation=n->step_bind_generation};
 }
 MirrorInput *pro=&surfaces.contexts[2].input;NovationState *n=&surfaces.surfaces[2].novation,*x=&surfaces.surfaces[1].novation;
 fake_input_reset();native_push(2,0x90,81,127);native_push(2,0x90,81,0);need(collection_drain(&surfaces,state,runtime)&&pro->step_count==1,"old Pro bound target has a real queued pad edit");
 /* Pressed again inside a step view, the step-view button switches single and
  * four-sound views and keeps the binding, sound and time page. */
 unsigned generation=n->step_bind_generation;fake_input_reset();native_push(2,0xb0,97,127);native_push(2,0xb0,97,127);native_push(2,0xb0,97,0);
 need(collection_drain(&surfaces,state,runtime)&&n->layer==NOV_STEP16&&n->step_bind_generation==generation&&n->step_pad==6&&n->step_first==32&&!pro->step_count&&n->step_mode==NOV_STEP16,"Sequencer in the 64-step view switches to 16 steps once (held repeat ignored), keeps the target and drops the unsent edit");
 fake_input_reset();native_push(2,0xb0,97,127);native_push(2,0xb0,97,0);need(collection_drain(&surfaces,state,runtime)&&n->layer==NOV_STEP64&&n->step_bind_generation==generation&&n->step_pad==6,"Sequencer again returns to 64 steps");
 fake_input_reset();native_push(2,0xb0,79,127);native_push(2,0xb0,79,0);need(collection_drain(&surfaces,state,runtime)&&n->layer==NOV_STEP16&&n->step_bind_generation==generation&&n->step_pad==6&&n->step_first==32,"Steps still switches views and keeps target/sound/time");
 fake_input_reset();native_push(1,0xb0,96,127);native_push(1,0xb0,96,0);need(collection_drain(&surfaces,state,runtime)&&x->layer==NOV_STEP16&&x->step_bind_generation==21&&x->step_pad==6&&x->step_first==32,"X Note in the 64-step view switches to 16 steps and keeps its binding");
 fake_input_reset();native_push(1,0xb0,96,127);native_push(1,0xb0,96,0);need(collection_drain(&surfaces,state,runtime)&&x->layer==NOV_STEP64&&x->step_bind_generation==21,"X Note in the 16-step view switches back to 64 steps");fake_input_reset();native_push(1,0xb0,97,127);native_push(1,0xb0,97,0);need(collection_drain(&surfaces,state,runtime)&&novation_mixer_page(x->layer)&&x->step_bind_generation==21,"X Custom leaves the step view for the mixer pages, keeping the binding");native_layer(surfaces.surfaces+1,surfaces.banks+1,NOV_STEP64);
 /* Entering from the clip grid binds the MPC selection again, in the view
  * this surface used last. */
 fake_input_reset();native_push(2,0xb0,93,127);native_push(2,0xb0,93,0);native_push(2,0xb0,97,127);native_push(2,0xb0,97,0);
 need(collection_drain(&surfaces,state,runtime)&&n->layer==NOV_STEP16&&n->step_bind_generation==generation+1&&n->step_pad==UINT32_MAX&&!n->step_first,"Session then Sequencer rebinds in the last-used 16-step view");
 fake_input_reset();native_push(2,0x90,81,127);native_push(2,0x90,81,0);native_push(1,0x90,81,127);native_push(1,0x90,81,0);need(collection_drain(&surfaces,state,runtime)&&!pro->step_count&&surfaces.contexts[1].input.step_count==1,"old Pro snapshot refused while independent X bound edit remains usable");
 StepProjection *v=s->steps.views+2;v->target.bind_generation=n->step_bind_generation;v->target.track_owner=9;v->target.program_owner=9;v->target.event_owner=29;v->pad=3;v->first_step=0;n->step_pad=3;
 fake_input_reset();native_push(2,0x90,81,127);native_push(2,0x90,81,0);need(collection_drain(&surfaces,state,runtime)&&pro->step_count==1&&pro->step_events[pro->step_head].request.track_owner==9,"fresh generation can edit newly selected native target");
 fake_input_reset();native_push(1,0xb0,95,127);native_push(1,0xb0,95,0);native_push(1,0xb0,96,127);native_push(1,0xb0,96,0);need(collection_drain(&surfaces,state,runtime)&&x->layer==NOV_STEP64&&x->step_bind_generation==22,"X enters its last-used 64-step view from the clip grid with Note or Custom");
 NovationState fresh;novation_reset(&fresh,MCU_PROTOCOL_LPP3,1);need(novation_step_entry(&fresh)==NOV_STEP64,"a new surface enters the single-sound view first");
 for(unsigned i=1;i<=2;i++){input_discard(&surfaces.contexts[i].input);surfaces.surfaces[i].novation=old[i-1];}s->steps=saved;s->steps_available=available;
 puts("PASS step-view button: inside a step view it switches 64/16 on X (Note or Custom) and Pro (Sequencer; Steps too) keeping target/sound/time; entry from the clip grid rebinds in the last-used view; old-target refusal and peer independence; native snapshots/ALSA substituted");
}
static void launchpad_clip_checks(const MirrorState *state,ControllerRuntime *runtime){
 CopiedMirror *snapshot=runtime->snapshot;ClipCopy saved=snapshot->clips;unsigned available=snapshot->clips_available,now=runtime->now;
 NovationState old[2]={surfaces.surfaces[1].novation,surfaces.surfaces[2].novation};snapshot->clips_available=1;snapshot->clips=(ClipCopy){.status=CLIP_SOURCE_READY,.tick=now,.epoch=snapshot->epoch,.project_owner=1,.lanes=100,.columns=16,.rows=17};
 for(unsigned i=1;i<=2;i++){NovationState *n=&surfaces.surfaces[i].novation;memset(n->held,0,sizeof(n->held));n->clip_epoch=snapshot->epoch;n->clip_column=(i-1)*8;n->clip_row=(i-1)*8;
  ClipProjection *v=snapshot->clips.views+i;v->status=CLIP_SOURCE_READY;v->tick=now;v->first_column=n->clip_column;v->first_row=n->clip_row;for(unsigned j=0;j<8;j++)v->track_owner[j]=j+1+(i-1)*8;for(unsigned j=0;j<64;j++)v->cells[j]=(ClipCell){CLIP_READY,1000+i*100+j};
  input_discard(&surfaces.contexts[i].input);
 }
 fake_input_reset();native_push(1,0xb0,95,127);native_push(1,0xb0,95,0);native_push(2,0xb0,93,127);native_push(2,0xb0,93,0);need(collection_drain(&surfaces,state,runtime),"both physical Session buttons enter clip windows");
 for(unsigned i=1;i<=2;i++){
  NovationState fresh;novation_reset(&fresh,i==1?MCU_PROTOCOL_LPX:MCU_PROTOCOL_LPP3,1);need(fresh.clip_row==0,"cold native clip page starts with Arrangement");
 }
 collection_clip_interest(&surfaces,snapshot,now);ClipDemand demand;need(clip_demand_read(&input.commands->clip_interest,&demand)&&demand.enabled==6&&demand.column[2]==8&&demand.row[2]==8,"both native clip windows share one expiring interest owner");
 fake_input_reset();native_push(1,0xb0,92,127);native_push(1,0xb0,92,0);need(collection_drain(&surfaces,state,runtime)&&surfaces.surfaces[1].novation.clip_row==8,"second clip page begins scene8 after Arrangement plus scenes1-7");
 fake_input_reset();native_push(1,0xb0,91,127);native_push(1,0xb0,91,0);native_push(1,0xb0,91,127);native_push(1,0xb0,91,0);need(collection_drain(&surfaces,state,runtime)&&!surfaces.surfaces[1].novation.clip_row,"page back reaches Arrangement and cannot underflow");
 fake_input_reset();native_push(1,0x90,81,127);native_push(1,0x90,81,127);native_push(2,0x90,18,127);need(collection_drain(&surfaces,state,runtime),"actual Programmer notes reach clip intent path");
 MirrorInput *x=&surfaces.contexts[1].input,*pro=&surfaces.contexts[2].input;need(x->step_count==2&&pro->step_count==2&&x->step_events[(x->step_head+1)%INPUT_JOG_EVENTS].request.bits==CLIP_SELECT&&pro->step_events[(pro->step_head+1)%INPUT_JOG_EVENTS].request.bits==CLIP_SELECT,"held clip pad has one launch, then the native select of that clip");
 const CommandRequest *a=&x->step_events[x->step_head].request,*b=&pro->step_events[pro->step_head].request;need(a->reserved==CLIP_ACTION&&a->bits==CLIP_LAUNCH&&a->pad_index==0&&a->effect_index==0&&b->pad_index==15&&b->effect_index==15,"per-surface clip pages bind actual native columns/scenes");
 native_capture_count=0;for(unsigned pass=0;pass<20;pass++)for(unsigned i=1;i<=2;i++){need(native_output(surfaces.surfaces+i,surfaces.banks+i,snapshot,now),"native-only clip state projection");need(surfaces.surfaces[i].novation.cycle_messages<=16&&surfaces.surfaces[i].novation.cycle_bytes<=128,"clip feedback retains output budgets");}
 NATIVE_HAS(151,0x90,81,45);need(!native_captured(151,(unsigned char[]){0x90,81,21},3),"launch receipt/input cannot predict playing LED");
 input_discard(x);input_discard(pro);fake_input_reset();native_push(1,0xb0,98,127);native_push(1,0x90,71,127);native_push(1,0xb0,89,127);native_push(1,0xb0,98,0);native_push(2,0xb0,93,127);native_push(2,0x90,81,127);need(collection_drain(&surfaces,state,runtime),"stop and explicit record chords reach shared ingress");
 need(x->step_count==2&&x->step_events[x->step_head].request.bits==CLIP_STOP&&x->step_events[(x->step_head+1)%INPUT_JOG_EVENTS].request.bits==CLIP_SCENE_STOP&&pro->step_count==1&&pro->step_events[pro->step_head].request.bits==CLIP_RECORD_TOGGLE,"physical modifier scene stop and Session+slot recording remain distinct");
 /* Every grid note, including legacy Learn notes81/82, is consumed by the
  * native surface. The separate MPC Control input route is not this ingress. */
 for(unsigned surface=1;surface<=2;surface++)for(unsigned action=0;action<3;action++)for(unsigned cell=0;cell<64;cell++){
  MirrorInput *in=&surfaces.contexts[surface].input;NovationState *n=&surfaces.surfaces[surface].novation;
  input_discard(in);input_jog_discard(in);memset(n->held,0,sizeof(n->held));
  unsigned session=surface==1?95:93,pad=(8-cell/8)*10+cell%8+1;
  fake_input_reset();if(action)native_push(surface,0xb0,action==1?98:session,127);
  native_push(surface,0x90,pad,127);native_push(surface,0x90,pad,0);
  need(collection_drain(&surfaces,state,runtime),"all64 clip cells traverse selected ALSA source and production collection");
  if(action==2&&n->clip_row+cell/8==0){need(!in->step_count&&!in->global_count,"Arrangement refuses the clip-record modifier");continue;}
  need(in->step_count==(action==0&&snapshot->clips.views[surface].cells[cell].state>=CLIP_READY?2u:1u)&&!in->global_count&&!in->edge_count,"grid has exactly one clip action (a launch of a clip also selects it) and no generic history/action leakage");
  const CommandRequest *r=&in->step_events[in->step_head].request;
  need(r->reserved==CLIP_ACTION&&r->bits==(action==0?CLIP_LAUNCH:action==1?CLIP_STOP:CLIP_RECORD_TOGGLE)&&r->pad_index==n->clip_column+cell%8&&r->effect_index==n->clip_row+cell/8,"every clip cell retains actual independent column/scene and modifier action");
 }
 for(unsigned surface=1;surface<=2;surface++)for(unsigned stop=0;stop<2;stop++)for(unsigned row=0;row<8;row++){
  MirrorInput *in=&surfaces.contexts[surface].input;NovationState *n=&surfaces.surfaces[surface].novation;input_discard(in);memset(n->held,0,sizeof(n->held));
  fake_input_reset();if(stop)native_push(surface,0xb0,98,127);native_push(surface,0xb0,89-row*10,127);
  need(collection_drain(&surfaces,state,runtime)&&in->step_count==1&&!in->global_count,"all scene controls stay in clip owner");
  const CommandRequest *r=&in->step_events[in->step_head].request;need(r->bits==(stop?CLIP_SCENE_STOP:CLIP_SCENE_LAUNCH)&&r->effect_index==n->clip_row+row,"all scene controls preserve independent scene offset");
 }
 snapshot->clips.views[1].cells[0].state=CLIP_WILL_RECORD;snapshot->clips.views[1].cells[1].state=CLIP_RECORDING;snapshot->clips.views[1].cells[2].state=CLIP_PLAYING;snapshot->clips.views[1].scene_state[0]=CLIP_WILL_PLAY;native_capture_count=0;
 for(unsigned pass=0;pass<20;pass++)need(native_output(surfaces.surfaces+1,surfaces.banks+1,snapshot,now),"actual pending/record/playing colors");
 NATIVE_HAS(151,0x91,81,5);NATIVE_HAS(151,0x92,82,5);NATIVE_HAS(151,0x92,83,3);NATIVE_HAS(151,0xb1,89,21);
 native_capture_count=0;need(native_output(surfaces.surfaces+1,surfaces.banks+1,snapshot,now)&&!native_capture_count,"unchanged clip state sends no MIDI");
 for(unsigned i=1;i<=2;i++){input_discard(&surfaces.contexts[i].input);surfaces.surfaces[i].novation=old[i-1];}snapshot->clips=saved;snapshot->clips_available=available;
 puts("PASS both Launchpads clip ingress, independent windows, launch/track-stop/scene-stop/record chords and native-only bounded RGB feedback; ALSA/native clip state substituted");
}
static void lcxl_pad_mixer_checks(const MirrorState *state,ControllerRuntime *runtime){
 CopiedMirror *s=runtime->snapshot,*saved=calloc(1,sizeof(*saved));need(saved!=NULL,"pad mixer source save");*saved=*s;
 Surface *surface=surfaces.surfaces;MirrorBank *bank=surfaces.banks,old_bank=*bank;NovationState old=surface->novation;MirrorInput *in=&surfaces.contexts[0].input;
 snapshot_fixture(s,16);s->selection.bits=s->tracks[0].track;s->selected_serial=s->tracks[0].serial;s->pad_count=128;
 for(unsigned i=0;i<128;i++){CopiedTrack *p=s->pads+i;*p=s->tracks[0];p->pad_owner=100+i;p->pad_index=i;p->pad_generation=1;memset(p->fields,0,sizeof(p->fields));for(unsigned f=0;f<PF_COUNT;f++){unsigned field=f==PF_VOLUME?CF_VOLUME:f==PF_PAN?CF_PAN:f==PF_MUTE?CF_MUTE:f==PF_SOLO?CF_SOLO:CF_SOLO_AUDIO;p->fields[field]=(CopiedField){.available=1,.incarnation=p->pad_owner*PF_COUNT+f,.bits=f<PF_MUTE?0x3f000000:0,.revision=2,.seed=1};}}
 runtime->snapshot_pads=0;surface->novation.observed_mode=1;memset(surface->novation.held,0,sizeof(surface->novation.held));memset(surface->novation.touch,0,sizeof(surface->novation.touch));bank->view=BV_TRACK;bank->offset=0;bank_apply(bank,s,runtime->now);input_discard(in);
 /* A fader held on a track cannot cross the mixer-view boundary. */
 fake_input_reset();native_push(0,0xbe,5,127);native_push(0,0xb6,63,127);native_push(0,0xb0,65,127);native_push(0,0xb0,65,0);native_push(0,0xb6,63,0);
 need(collection_drain(&surfaces,state,runtime)&&bank->view==BV_DRUM_PADS&&!surface->novation.shift&&!surface->novation.held[65],"pad entry defers missing pad copy but still releases chord modifiers");runtime->snapshot_pads=1;bank_apply(bank,s,runtime->now);
 fake_input_reset();native_push(0,0xbf,5,64);native_push(0,0xbf,93,65);native_push(0,0xbf,77,65);native_push(0,0xbf,85,65);native_push(0,0xb0,37,127);native_push(0,0xb0,45,127);
 need(collection_drain(&surfaces,state,runtime)&&!in->desires[0][CF_VOLUME].pending&&in->desires[0][CF_PAN].pending&&in->desires[0][CF_PAN].identity.pad_owner==100,"pad Pan edits its Instrument; held track fader stays invalid");
 need(!in->desires[0][CF_SEND1].pending&&!in->desires[0][CF_SEND2].pending&&!in->desires[0][CF_SELECTION].pending&&in->edge_count==2&&in->edges[in->edge_head].field==CF_SOLO&&in->edges[(in->edge_head+1)%INPUT_EDGES].field==CF_MUTE,"pad rows use only supported solo/mute and sends issue no command");
 input_discard(in);fake_input_reset();native_push(0,0xbe,5,0);native_push(0,0xbf,5,0);need(collection_drain(&surfaces,state,runtime)&&!in->desires[0][CF_VOLUME].pending,"pad fader pickup does not jump to unmatched physical value");
 fake_input_reset();native_push(0,0xbf,5,64);need(collection_drain(&surfaces,state,runtime)&&in->desires[0][CF_VOLUME].pending&&in->desires[0][CF_VOLUME].identity.pad_owner==100,"released fader acquires pad volume only when crossing source");input_discard(in);
 memset(surface->novation.led_valid,0,sizeof(surface->novation.led_valid));memset(surface->novation.text_valid,0,sizeof(surface->novation.text_valid));memset(surface->novation.position_valid,0,sizeof(surface->novation.position_valid));native_capture_count=0;
 for(unsigned pass=0;pass<40;pass++){need(native_output(surface,bank,s,runtime->now),"pad mixer reaches actual MIDI output");need(surface->novation.cycle_messages<=16&&surface->novation.cycle_bytes<=128,"pad projection retains output caps");}
 NATIVE_HAS(150,0xf0,0,0x20,0x29,2,0x15,6,5,0,'P','a','d',' ','A','0','1',0xf7);
 NATIVE_HAS(150,0xb0,65,45);NATIVE_HAS(150,0xb0,13,0);NATIVE_HAS(150,0xf0,0,0x20,0x29,2,0x15,6,13,1,'-',0xf7);
 for(unsigned page=0;page<15;page++){fake_input_reset();native_push(0,0xb0,102,127);native_push(0,0xb0,102,0);need(collection_drain(&surfaces,state,runtime),"pad bank button ingress");}bank_apply(bank,s,runtime->now);need(bank->strips[7].pad_index==127,"pad banks reach H16");
 fake_input_reset();native_push(0,0xb6,30,2);need(collection_drain(&surfaces,state,runtime)&&bank->assignment==BA_QLINK&&bank->view==BV_DRUM_PADS,"Control page retains existing Q-Link owner and mixer target");native_control_page(surface,bank,2);need(bank->assignment==BA_EFFECT&&bank->effects_slot==0,"pad mixer preserves Control effects pages");
 fake_input_reset();native_push(0,0xb6,30,1);native_push(0,0xb6,63,127);native_push(0,0xb0,65,127);native_push(0,0xb0,65,0);native_push(0,0xb6,63,0);need(collection_drain(&surfaces,state,runtime)&&bank->view==BV_TRACK&&bank->assignment==BA_TRACK,"same chord returns Track mixer without changing control-page selection");
 input_discard(in);*s=*saved;free(saved);*bank=old_bank;surface->novation=old;memset(surface->native_pickup,0,sizeof(surface->native_pickup));runtime->snapshot_pads=0;
 puts("PASS XL3 physical Shift/Solo-Arm Track/Pad toggle, exact pad volume/pan/solo/mute, unsupported sends dark, held/pickup refusal, all128 banks, emitted Pad labels and retained Control pages (ALSA/native publication substituted)");
}
/* XL3 DAW Mixer: Shift + a lower-row button selects that track; a Drum track
 * opens its Pad mixer (after MPC reports the selection), another track the
 * Track mixer. Holding Shift shows the selected track in the lower row. */
static void lcxl_shift_select_checks(const MirrorState *state,ControllerRuntime *runtime){
 CopiedMirror *s=runtime->snapshot,*saved=calloc(1,sizeof(*saved));need(saved!=NULL,"shift select source save");*saved=*s;
 Surface *surface=surfaces.surfaces;MirrorBank *bank=surfaces.banks,old_bank=*bank;NovationState old=surface->novation,*n=&surface->novation;MirrorInput *in=&surfaces.contexts[0].input;
 snapshot_fixture(s,16);s->tracks[2].vptr=0x693147c;s->selection.bits=s->tracks[0].track;s->selected_serial=s->tracks[0].serial;s->pad_count=128;
 for(unsigned i=0;i<128;i++){CopiedTrack *pad=s->pads+i;*pad=s->tracks[0];pad->pad_owner=100+i;pad->pad_index=i;pad->pad_generation=1;}
 runtime->snapshot_pads=1;n->observed_mode=1;n->mute_select=0;n->shift_view=0;memset(n->held,0,sizeof(n->held));memset(n->touch,0,sizeof(n->touch));bank->view=BV_TRACK;bank->offset=0;bank_apply(bank,s,runtime->now);input_discard(in);
 fake_input_reset();native_push(0,0xb6,63,127);need(collection_drain(&surfaces,state,runtime)&&n->shift,"Shift held");
 memset(n->led_valid,0,sizeof(n->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surface,bank,s,runtime->now),"shift row output");
 need(n->led_value[45]==21&&n->led_value[46]==1&&n->led_value[47]==1,"with Shift held the lower row shows the selected track");
 fake_input_reset();native_push(0,0xb0,46,127);native_push(0,0xb0,46,0);
 need(collection_drain(&surfaces,state,runtime)&&in->desires[1][CF_SELECTION].pending&&in->desires[1][CF_SELECTION].identity.track==s->tracks[1].track&&bank->view==BV_TRACK&&n->shift_view==1,"Shift + Select on Drum track 2 selects it natively and waits for MPC");
 need(native_output(surface,bank,s,runtime->now)&&bank->view==BV_TRACK,"the Pad mixer waits for the selection");
 input_discard(in);s->selection.bits=s->tracks[1].track;s->selected_serial=s->tracks[1].serial;bank_apply(bank,s,runtime->now);
 need(native_output(surface,bank,s,runtime->now)&&bank->view==BV_DRUM_PADS&&!n->shift_view,"once MPC selects it, the Pad mixer opens on its pads");
 bank_apply(bank,s,runtime->now);memset(n->led_valid,0,sizeof(n->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surface,bank,s,runtime->now),"shift row in Pad mixer");
 need(n->led_value[46]==21&&n->led_value[45]==1,"in the Pad mixer, Shift shows whose pads these are");
 fake_input_reset();native_push(0,0xb0,45,127);native_push(0,0xb0,45,0);
 need(collection_drain(&surfaces,state,runtime)&&in->desires[0][CF_SELECTION].pending&&in->desires[0][CF_SELECTION].identity.track==s->tracks[0].track,"in the Pad mixer, Shift + Select still addresses the Track mixer bank");
 input_discard(in);s->selection.bits=s->tracks[0].track;s->selected_serial=s->tracks[0].serial;bank_apply(bank,s,runtime->now);need(native_output(surface,bank,s,runtime->now)&&bank->view==BV_DRUM_PADS,"another Drum track keeps the Pad mixer, now on its pads");
 fake_input_reset();native_push(0,0xb0,47,127);native_push(0,0xb0,47,0);
 need(collection_drain(&surfaces,state,runtime)&&in->desires[2][CF_SELECTION].pending&&in->desires[2][CF_SELECTION].identity.track==s->tracks[2].track,"Shift + Select on a Keygroup track selects it");
 input_discard(in);s->selection.bits=s->tracks[2].track;s->selected_serial=s->tracks[2].serial;bank_apply(bank,s,runtime->now);
 need(native_output(surface,bank,s,runtime->now)&&bank->view==BV_TRACK&&!bank->offset,"a non-Drum track returns to the Track mixer at the same bank");
 fake_input_reset();native_push(0,0xb6,63,0);native_push(0,0xb0,46,127);native_push(0,0xb0,46,0);need(collection_drain(&surfaces,state,runtime)&&!n->shift&&!n->shift_view&&in->edge_count==1&&in->edges[in->edge_head].field==CF_MUTE,"without Shift the lower row is Mute as before");
 input_discard(in);*s=*saved;free(saved);*bank=old_bank;surface->novation=old;memset(surface->native_pickup,0,sizeof(surface->native_pickup));runtime->snapshot_pads=0;
 puts("PASS XL3 Shift+Select: native track select, Drum track opens its Pad mixer once selected, another Drum track switches pads, non-Drum returns to Track mixer, Shift row shows the selected track, plain row unchanged (ALSA/native publication substituted)");
}
static void native_admission_checks(Surface *surface){
 NovationState saved=surface->novation;
 for(unsigned protocol=MCU_PROTOCOL_LPX;protocol<=MCU_PROTOCOL_LPP3;protocol++){
  novation_reset(&surface->novation,protocol,76);native_capture_count=0;
  need(native_initialise(surface,1000)&&surface->novation.stage<3,"only queries precede mode admission");
  unsigned queries=native_capture_count;need(queries==(protocol==MCU_PROTOCOL_LPX?2u:1u),"only documented one-shot queries");
  for(unsigned now=1250;now<2000;now+=10){novation_budget(&surface->novation);need(native_initialise(surface,now)&&surface->novation.stage<3,"unanswered query stays inactive beyond timeout");}
  need(native_capture_count==queries&&!surface->novation.teardown_pending,"unknown state emits no mode changes, feedback or repeated queries");
  snd_seq_event_t event;snd_seq_ev_clear(&event);event.source=surface->full;snd_seq_ev_set_controller(&event,4,0,127);
  need(native_consume(surface,NULL,NULL,NULL,&event,0)&&!surface->novation.held[0],"inactive input is ignored before snapshot/command path");
  stopping=1;native_teardown(surface);stopping=0;need(native_capture_count==queries,"inactive clean stop does not disable an unknown original mode");
  unsigned char layout[]={0xf0,0,0x20,0x29,2,(unsigned char)novation_device(protocol),0,126,0,0,0xf7};
  unsigned length=protocol==MCU_PROTOCOL_LPX?9:11;if(length==9)layout[8]=0xf7;
  snd_seq_ev_set_sysex(&event,length,layout);need(native_consume(surface,NULL,NULL,NULL,&event,0),"unsupported pre-layout parsed without activation");
  if(protocol==MCU_PROTOCOL_LPX){unsigned char mode[]={0xf0,0,0x20,0x29,2,0x0c,0x0e,0,0xf7};snd_seq_ev_set_sysex(&event,sizeof(mode),mode);need(native_consume(surface,NULL,NULL,NULL,&event,0),"known Live mode alone cannot admit unsupported layout");}
  novation_budget(&surface->novation);need(native_initialise(surface,2100)&&surface->novation.stage<3&&native_capture_count==queries,"non-restorable reply stays inactive");

  /* A later valid report can finish admission on the existing subscription. */
  layout[7]=protocol==MCU_PROTOCOL_LPX?1:0;snd_seq_ev_set_sysex(&event,length,layout);need(native_consume(surface,NULL,NULL,NULL,&event,0),"delayed restorable layout accepted");
  novation_budget(&surface->novation);need(native_initialise(surface,2200)&&surface->novation.stage==16&&surface->novation.teardown_pending,"late valid report activates once and owns restoration");
  need(surface->novation.pre_layout==(protocol==MCU_PROTOCOL_LPX?1u:0u),"latest pre-mutation layout is retained");
  layout[7]=1;snd_seq_ev_set_sysex(&event,length,layout);need(native_consume(surface,NULL,NULL,NULL,&event,0)&&surface->novation.pre_layout==(protocol==MCU_PROTOCOL_LPX?1u:0u),"post-admission report cannot overwrite restoration state");
  native_capture_count=0;native_teardown(surface);need(native_capture_count==2&&!surface->novation.teardown_pending,"late admission clean stop restores observed mode/layout");
 }
 surface->novation=saved;
}
static void native_restore_checks(Surface *surface){
 native_admission_checks(surface);NovationState saved=surface->novation;
 const unsigned xlayouts[]={0,1,4,5,6,7,13,127};
 for(unsigned stop=0;stop<2;stop++)for(unsigned at=0;at<sizeof(xlayouts)/sizeof(xlayouts[0]);at++){
  unsigned layout=xlayouts[at],mode=layout==127;novation_reset(&surface->novation,MCU_PROTOCOL_LPX,77);surface->novation.pre_valid=3;surface->novation.pre_mode=mode;surface->novation.pre_layout=layout;
  need(novation_restorable(&surface->novation),"all documented LPX pre-layouts restorable");novation_budget(&surface->novation);need(native_initialise(surface,1300)&&surface->novation.stage==16,"LPX enters Programmer after captured mode/layout");native_capture_count=0;stopping=(sig_atomic_t)stop;native_teardown(surface);stopping=0;
  need(!surface->novation.teardown_pending&&native_capture_count==2,"clean/SIGTERM restoration bounded and complete");
  need(native_capture[0].bytes[6]==0x0e&&native_capture[0].bytes[7]==mode&&native_capture[1].bytes[6]==0&&native_capture[1].bytes[7]==layout,"LPX restores captured Live/Programmer mode then exact layout without DAW toggle");
 }
 for(unsigned layout=0;layout<=19;layout++){
  unsigned pages=layout==1||(layout>=6&&layout<=16)?4:layout==3||layout==19?8:layout==18?5:1;
  for(unsigned page=0;page<pages;page++){
   novation_reset(&surface->novation,MCU_PROTOCOL_LPP3,78);NovationEvent report={.kind=NOV_LAYOUT,.index=page,.value=(int)layout};novation_observe(&surface->novation,&report);need(novation_restorable(&surface->novation),"documented Pro layout/page is restorable");surface->novation.teardown_pending=1;native_capture_count=0;stopping=1;native_teardown(surface);stopping=0;
   need(native_capture_count==2&&!surface->novation.teardown_pending,"Pro exact layout/page restoration bounded");need(native_capture[0].bytes[6]==0x0e&&native_capture[0].bytes[7]==(layout==17)&&native_capture[1].bytes[6]==0&&native_capture[1].bytes[7]==layout&&native_capture[1].bytes[8]==page,"Pro returns prior mode and full page, including Sequencer and prior Programmer");
  }
 }
 novation_reset(&surface->novation,MCU_PROTOCOL_LPX,79);surface->novation.pre_valid=3;surface->novation.pre_mode=0;surface->novation.pre_layout=13;surface->novation.teardown_pending=1;
 FILE *log=tmpfile();need(log!=NULL,"failed teardown log capture");fflush(stdout);int out=dup(STDOUT_FILENO);need(out>=0&&dup2(fileno(log),STDOUT_FILENO)>=0,"failed teardown stdout redirect");unsigned verbose=bridge_verbose;bridge_verbose=1;native_output_fail_after=2;stopping=1;native_teardown(surface);stopping=0;fflush(stdout);need(dup2(out,STDOUT_FILENO)>=0,"restore test stdout");close(out);bridge_verbose=verbose;rewind(log);char text[256]={0};need(fread(text,1,sizeof(text)-1,log)>0&&strstr(text,"TEARDOWN_UNCONFIRMED")&&surface->novation.teardown_pending,"failed restoration remains pending and reports unconfirmed");fclose(log);surface->novation=saved;
}
static void lcxl_feature_reply(Surface *surface,unsigned channel,unsigned cc,unsigned value){
 snd_seq_event_t event;snd_seq_ev_clear(&event);snd_seq_ev_set_controller(&event,channel,cc,value);event.source=surface->full;
 need(native_consume(surface,NULL,NULL,NULL,&event,0),"LCXL feature reply production dispatch without MPC snapshot");
}
static void lcxl_readback_checks(Surface *surface,const MirrorState *state,ControllerRuntime *runtime){
 NovationState saved=surface->novation;native_capture_count=0;
 novation_reset(&surface->novation,MCU_PROTOCOL_LCXL3,79);surface->novation.stage=15;
 native_output_fail_after=1;need(!native_initialise(surface,900)&&!surface->novation.lc_display&&!native_capture_count,"failed encoder layout send never advances initialization");
 novation_budget(&surface->novation);need(native_initialise(surface,910)&&surface->novation.lc_display==12&&surface->novation.cycle_bytes==120&&!surface->novation.lc_round,"encoder layouts stop at existing byte cap before admission");
 novation_budget(&surface->novation);need(native_initialise(surface,920)&&surface->novation.lc_display==24&&surface->novation.cycle_bytes==126&&!novation_admitted(&surface->novation),"remaining encoder layouts and queries resume within same budget");
 for(unsigned i=0;i<24;i++){unsigned char config[]={0xf0,0,0x20,0x29,2,0x15,4,(unsigned char)(i+13),0x61,0xf7};need(native_captured(150,config,sizeof(config))==1,"retried layout delivered exactly once per encoder");}
 native_capture_count=0;
 novation_reset(&surface->novation,MCU_PROTOCOL_LCXL3,80);surface->novation.stage=15;surface->novation.lc_display=24;surface->novation.teardown_pending=1;
 lcxl_feature_reply(surface,6,69,127);need(!surface->novation.lc_received,"unsolicited pre-query reply cannot admit new generation");
 novation_budget(&surface->novation);need(native_initialise(surface,1000)&&!novation_admitted(&surface->novation)&&native_capture_count==3,"three read-only queries precede LCXL admission");
 NATIVE_HAS(150,0xb7,69,127);NATIVE_HAS(150,0xb7,72,127);NATIVE_HAS(150,0xb7,73,127);
 for(unsigned now=1250;now<2000;now+=10){novation_budget(&surface->novation);need(native_initialise(surface,now)&&native_capture_count==3&&!novation_admitted(&surface->novation),"missing replies stay inactive without retry or polling output");}
 need(surface->novation.admission_reported,"missing readback crosses existing250ms status threshold");
 lcxl_feature_reply(surface,7,69,127);lcxl_feature_reply(surface,6,69,1);lcxl_feature_reply(surface,6,70,127);
 need(!surface->novation.lc_received,"query channel, malformed OnOff and unrelated feature cannot admit");
 lcxl_feature_reply(surface,6,73,127);lcxl_feature_reply(surface,6,72,127);
 novation_budget(&surface->novation);need(native_initialise(surface,2000)&&native_capture_count==3&&!novation_admitted(&surface->novation),"partial out-of-order readback does not configure or admit");
 lcxl_feature_reply(surface,6,69,0);novation_budget(&surface->novation);need(native_initialise(surface,2010)&&native_capture_count==7&&surface->novation.lc_round==2&&!surface->novation.lc_received&&!surface->novation.lc_on,"observed row1Off gets one correction and all fresh readbacks");
 NATIVE_HAS(150,0xb6,69,127);need(!native_captured(150,(unsigned char[]){0xb6,72,127},3)&&!native_captured(150,(unsigned char[]){0xb6,73,127},3),"alreadyOn rows are not rewritten");
 lcxl_feature_reply(surface,6,72,127);lcxl_feature_reply(surface,6,69,127);lcxl_feature_reply(surface,6,69,127);
 novation_budget(&surface->novation);need(native_initialise(surface,2020)&&!novation_admitted(&surface->novation),"duplicates and old round evidence do not complete fresh readback");
 lcxl_feature_reply(surface,6,73,127);novation_budget(&surface->novation);need(native_initialise(surface,2030)&&novation_admitted(&surface->novation)&&native_capture_count==7,"late complete allOn readback admits without more output");
 surface->context->input.desires[0][CF_PAN].pending=1;lcxl_feature_reply(surface,6,69,0);
 need(!novation_admitted(&surface->novation)&&!surface->context->input.desires[0][CF_PAN].pending,"activeOff revokes admission and unsent commands");
 novation_budget(&surface->novation);need(native_initialise(surface,3000)&&native_capture_count==7,"activeOff never restarts initialization or recurring retries");
 fake_input_reset();native_push(0,0xbf,85,65);need(collection_drain(&surfaces,state,runtime)&&!surface->context->input.desires[0][CF_SEND2].pending,"activeOff blocks actual relative input through shared dispatcher");
 need(native_output(surface,surfaces.banks,runtime->snapshot,3000)&&native_capture_count==7,"activeOff blocks cached projection without mode reentry");
 /* Worst case: all three rows need their one bounded correction. */
 novation_reset(&surface->novation,MCU_PROTOCOL_LCXL3,81);surface->novation.stage=15;surface->novation.lc_display=24;surface->novation.teardown_pending=1;native_capture_count=0;
 novation_budget(&surface->novation);surface->novation.cycle_messages=15;need(native_initialise(surface,4000)&&native_capture_count==1&&surface->novation.cycle_messages==16,"readback respects remaining per-cycle message budget");
 novation_budget(&surface->novation);need(native_initialise(surface,4010)&&native_capture_count==3,"split query resumes without duplication");
 lcxl_feature_reply(surface,6,69,0);lcxl_feature_reply(surface,6,72,0);lcxl_feature_reply(surface,6,73,0);
 novation_budget(&surface->novation);surface->novation.cycle_bytes=125;need(native_initialise(surface,4020)&&native_capture_count==4&&surface->novation.cycle_bytes==128,"correction respects remaining byte budget");
 novation_budget(&surface->novation);need(native_initialise(surface,4030)&&native_capture_count==9,"allOff needs at most nine additional CC messages");
 lcxl_feature_reply(surface,6,73,0);lcxl_feature_reply(surface,6,69,0);lcxl_feature_reply(surface,6,72,0);
 for(unsigned now=4300;now<5000;now+=10){novation_budget(&surface->novation);need(native_initialise(surface,now)&&native_capture_count==9&&!novation_admitted(&surface->novation),"persistentOff remains inactive without further correction");}
 need(surface->novation.admission_reported,"persistentOff has visible inactive status");
 native_capture_count=0;native_teardown(surface);need(native_capture_count==1&&!surface->novation.teardown_pending,"unadmitted DAW entry retains exact clean-exit responsibility");NATIVE_HAS(150,0xf0,0,0x20,0x29,2,0x15,2,0,0xf7);
 novation_reset(&surface->novation,MCU_PROTOCOL_LCXL3,82);need(!surface->novation.lc_round&&!surface->novation.lc_received&&!surface->novation.lc_on&&!surface->novation.lc_queried&&!novation_admitted(&surface->novation),"reconnect drops all readback and correction evidence");
 surface->novation=saved;native_capture_count=0;
 puts("PASS LCXL3 documented feature readback, explicit Off one-correction, delayed/out-of-order replies, no-reply/persistent-Off parking, active-Off revocation, output caps and clean exit (ALSA/MPC substituted)");
}
/* Continually changing values must not starve bank, effect or Q-Link names. */
static void lcxl_name_fairness_checks(Surface *surface,const MirrorBank *original_bank,const CopiedMirror *source){
 NovationState saved=surface->novation;CopiedMirror *snapshot=calloc(1,sizeof(*snapshot));need(snapshot!=NULL,"name fairness source allocation");
 for(unsigned mode=0;mode<3;mode++)for(unsigned width=0;width<2;width++){
  *snapshot=*source;surface->novation=saved;NovationState *n=&surface->novation;memset(n->touch,0,sizeof(n->touch));memset(n->text_valid,0,sizeof(n->text_valid));n->text_cursor=0;
  MirrorBank bank=*original_bank;bank.offset=8;bank_apply(&bank,snapshot,1700);n->observed_mode=mode?2:1;bank.assignment=mode==1?BA_EFFECT:mode==2?BA_QLINK:BA_TRACK;bank.effects_slot=bank.effects_page=bank.qlink_page=0;
  for(unsigned strip=0;strip<8;strip++){CopiedField *name=&snapshot->tracks[8+strip].fields[CF_NAME];name->available=1;name->length=16;snprintf(name->text,sizeof(name->text),"Bank track %05u",strip);}
  snapshot->effects_available=1;snapshot->effects=(EffectsCopy){.status=EF_READY,.serial=bank.selected.serial,.slot=0,.page=0};
  snapshot->qlinks_available=1;snapshot->qlinks=(QLinkCopy){.epoch=snapshot->epoch,.status=QL_SAMPLED,.root=1,.wrapper=2,.provider=3,.generation=4};
  for(unsigned strip=0;strip<8;strip++){
   snapshot->effects.parameters[strip]=(EffectParameter){.status=EF_READY,.tick=1700};snprintf(snapshot->effects.parameters[strip].name,EFFECT_TEXT,"New effect %05u",strip);
   snapshot->qlinks.slots[strip]=(QLinkSlot){.sampled=1,.normalized_valid=1,.tick=1700,.revision=2};
  }
  /* Settle the initial bank/mode repaint, then dirty the names while values
   * continue changing; this isolates starvation from initial fader catch-up. */
  for(unsigned pass=0;pass<40;pass++)need(native_output(surface,&bank,snapshot,1700),"name fairness bank/mode setup");
  memset(n->text_valid,0,sizeof(n->text_valid));n->text_cursor=0;
  uint32_t names=0,value_targets=0;unsigned long_names=0;
  for(unsigned pass=0;pass<32;pass++){
   float value=(float)(width?100+pass%28:40+pass)/127.0f;uint32_t bits;memcpy(&bits,&value,4);
   for(unsigned strip=0;strip<8;strip++){
    CopiedTrack *track=&snapshot->tracks[8+strip];unsigned fields[]={CF_SEND1,CF_SEND2,CF_PAN};
    for(unsigned f=0;f<3;f++){track->fields[fields[f]].bits=bits;track->fields[fields[f]].available=1;}
    track->fields[CF_SOLO].bits=track->fields[CF_MUTE].bits=pass&1;
    snapshot->effects.parameters[strip].bits=bits;snapshot->qlinks.slots[strip].bits=bits;
   }
   native_capture_count=0;need(native_output(surface,&bank,snapshot,1700),"continuous encoder/name projection");
   need(n->cycle_messages<=16&&n->cycle_bytes<=128,"name/value fairness preserves shared message and byte caps");
   unsigned values=0,name_messages=0;
   for(unsigned i=0;i<native_capture_count;i++){
    const unsigned char *b=native_capture[i].bytes;
    if(native_capture[i].destination.client!=150||native_capture[i].length<10||b[0]!=0xf0||b[6]!=6||b[7]<5||b[7]>36)continue;
    if(b[8]==0){names|=1u<<(b[7]-5);name_messages++;if(native_capture[i].length==26)long_names++;}
    else if(b[8]==1&&b[7]>=13&&b[9]!='-'){values++;value_targets|=1u<<(b[7]-13);}
   }
   if(!values)fprintf(stderr,"name fairness stalled mode=%u width=%u cycle=%u messages=%u bytes=%u\n",mode,width,pass,n->cycle_messages,n->cycle_bytes);
   need(values>0,"continuous values progress while names are dirty");
   need(name_messages<=1,"name share emits at most one changed name per cycle");
  }
  need(names==UINT32_MAX&&long_names>=8,"all32 dirty names including16-character names delivered within32 busy cycles");
  need(value_targets==(mode?0xffu:0xffffffu),"every applicable encoder value continues progressing");
 }
 surface->novation=saved;native_capture_count=0;free(snapshot);
 puts("PASS bounded LCXL3 name delivery during continuous24-value Mixer and8-value effect/Q-Link updates, two/three-digit values,16-character names and button LED churn (ALSA/MPC substituted)");
}
/* XL3 knob LEDs follow each column's MPC track colour (RGB SysEx, programmer's
 * reference v1.0); pads, device pages and uncoloured tracks keep the palette. */
static void lcxl_track_colour_checks(Surface *surface,const MirrorBank *original_bank,const CopiedMirror *source){
 need(novation_track_rgb(0xffff0000u)==(NOV_RGB|0x7f0000u)&&novation_track_rgb(0xff00ff00u)==(NOV_RGB|0x7f00u)&&novation_track_rgb(0xff0000ffu)==(NOV_RGB|0x7fu),"primary MPC colours map to full 7-bit components");
 need(novation_track_rgb(0xffdd4488u)==(NOV_RGB|0x6e2244u)&&novation_track_rgb(0x00808080u)==(NOV_RGB|0x404040u),"8-bit channels round down to the nearest 7-bit value; alpha is ignored");
 need(!novation_track_rgb(0)&&!novation_track_rgb(0xff010101u),"black or sub-step colour is unusable and keeps the default");
 NovationWire w=novation_lcxl3_led(13,NOV_RGB|0x6e2244u);
 need(w.length==13&&!memcmp(w.bytes,(unsigned char[]){0xf0,0,0x20,0x29,2,0x15,1,0x53,13,0x6e,0x22,0x44,0xf7},13),"RGB colour uses the documented 01 53 SysEx with the encoder's absolute index");
 w=novation_lcxl3_led(13,21);need(w.length==3&&w.bytes[0]==0xb0&&w.bytes[1]==13&&w.bytes[2]==21,"palette colour keeps the channel-1 CC form");
 NovationState saved=surface->novation;CopiedMirror *snapshot=calloc(1,sizeof(*snapshot));need(snapshot!=NULL,"colour source allocation");*snapshot=*source;
 MirrorBank bank=*original_bank;bank.offset=0;bank.view=BV_TRACK;bank.assignment=BA_TRACK;bank_apply(&bank,snapshot,1700);
 NovationState *n=&surface->novation;n->observed_mode=1;memset(n->touch,0,sizeof(n->touch));memset(n->led_valid,0,sizeof(n->led_valid));memset(n->position_valid,0,sizeof(n->position_valid));
 const uint32_t colours[8]={0xffff0000u,0xff00ff00u,0xff0000ffu,0xffdd4488u,0xffffff00u,0,0xff00ffffu,0xffffffffu};
 for(unsigned strip=0;strip<8;strip++){CopiedField *c=&snapshot->tracks[strip].fields[CF_COLOR];c->available=strip!=6;c->bits=colours[strip];}
 native_capture_count=0;for(unsigned pass=0;pass<40;pass++){need(native_output(surface,&bank,snapshot,1700),"colour mixer projection");need(n->cycle_messages<=16&&n->cycle_bytes<=128,"colour SysEx keeps the shared output caps");}
 for(unsigned strip=0;strip<8;strip++)for(unsigned row=0;row<3;row++){
  unsigned cc=13+row*8+strip;uint32_t rgb=strip==6?0:novation_track_rgb(colours[strip]);
  if(rgb){unsigned char m[]={0xf0,0,0x20,0x29,2,0x15,1,0x53,(unsigned char)cc,(unsigned char)((rgb>>16)&127),(unsigned char)((rgb>>8)&127),(unsigned char)(rgb&127),0xf7};need(native_captured(150,m,sizeof(m))==1,"each track column's three knob LEDs take that track's colour once");}
  else need(native_captured(150,(unsigned char[]){0xb0,(unsigned char)cc,21},3)==1,"black or unavailable track colour keeps the default palette green");
 }
 for(unsigned b=37;b<=52;b++)for(unsigned i=0;i<native_capture_count;i++){const unsigned char *x=native_capture[i].bytes;need(!(x[0]==0xf0&&native_capture[i].length==13&&x[8]==b),"Solo/Mute/Arm/Select buttons never take track colours");}
 native_capture_count=0;for(unsigned pass=0;pass<8;pass++)need(native_output(surface,&bank,snapshot,1700),"unchanged colour projection");
 for(unsigned i=0;i<native_capture_count;i++)need(!(native_capture[i].bytes[0]==0xf0&&native_capture[i].bytes[6]==1),"unchanged colours send nothing");
 snapshot->tracks[2].fields[CF_COLOR].bits=0xff00ff00u;snapshot->tracks[2].fields[CF_COLOR].revision++;native_capture_count=0;
 for(unsigned pass=0;pass<8;pass++)need(native_output(surface,&bank,snapshot,1700),"changed colour projection");
 unsigned changed=0;for(unsigned i=0;i<native_capture_count;i++){const unsigned char *x=native_capture[i].bytes;if(x[0]==0xf0&&native_capture[i].length==13&&x[6]==1)changed+=(x[8]-13)%8==2&&x[9]==0&&x[10]==0x7f&&x[11]==0;}
 need(changed==3,"a changed MPC track colour resends only that column's three knob LEDs");
 for(unsigned i=0;i<native_capture_count;i++){const unsigned char *x=native_capture[i].bytes;need(!(x[0]==0xf0&&native_capture[i].length==13&&x[6]==1&&(x[8]-13)%8!=2),"other columns stay cached");}
 /* DAW Control device page: not per track, keeps palette 45 on the top row. */
 n->observed_mode=2;bank.assignment=BA_QLINK;bank.qlink_page=0;memset(n->led_valid,0,sizeof(n->led_valid));memset(n->position_valid,0,sizeof(n->position_valid));
 snapshot->qlinks_available=1;snapshot->qlinks=(QLinkCopy){.epoch=snapshot->epoch,.status=QL_SAMPLED,.root=1,.wrapper=2,.provider=3,.generation=4};
 for(unsigned i=0;i<16;i++)snapshot->qlinks.slots[i]=(QLinkSlot){.sampled=1,.normalized_valid=1,.tick=1700,.bits=0x3f000000,.revision=2};
 native_capture_count=0;for(unsigned pass=0;pass<40;pass++)need(native_output(surface,&bank,snapshot,1700),"device page projection");
 for(unsigned strip=0;strip<8;strip++){need(native_captured(150,(unsigned char[]){0xb0,(unsigned char)(13+strip),45},3)==1,"device page keeps its palette colour");}
 for(unsigned i=0;i<native_capture_count;i++)need(!(native_capture[i].bytes[0]==0xf0&&native_capture[i].bytes[6]==1),"device page sends no track colours");
 surface->novation=saved;native_capture_count=0;free(snapshot);
 puts("PASS XL3 knob LEDs: MPC track colour to nearest 7-bit RGB through documented SysEx, default palette for black/unavailable, buttons and device pages unchanged, change-only resend within caps (ALSA/MPC substituted)");
}
/* Launchpad clip cells take their column's MPC track colour (nearest palette
 * entry); steady/flashing/pulsing keep ready/queued/playing meaning. */
static void launchpad_clip_colour_checks(CopiedMirror *snapshot,unsigned now){
 need(novation_palette_nearest(novation_track_rgb(0x0022ffu))==45&&novation_palette_nearest(novation_track_rgb(0x00ff80u))==29&&
      novation_palette_nearest(novation_track_rgb(0xff00bbu))==95&&novation_palette_nearest(novation_track_rgb(0xff8800u))==9,"Controllers project colours map to the nearest documented palette entries");
 need(novation_palette_nearest(novation_track_rgb(0xff0000u))==5&&novation_palette_nearest(novation_track_rgb(0x00ff00u))==21&&novation_palette_nearest(novation_track_rgb(0xffffffu))==3,"primary colours and white map to the palette's red, green and white");
 need(novation_palette_nearest(novation_track_rgb(0x020202u))!=0,"a near-black colour never maps to the dark entry");
 const unsigned states[]={CLIP_NO_SLOT,CLIP_EMPTY,CLIP_READY,CLIP_WILL_PLAY,CLIP_PLAYING,CLIP_WILL_STOP_PLAYING,CLIP_WILL_RECORD,CLIP_RECORDING,CLIP_WILL_STOP_RECORDING,CLIP_WILL_OVERDUB,CLIP_OVERDUBBING,CLIP_WILL_STOP_OVERDUBBING};
 const unsigned char coloured[][2]={{0,0},{0,0},{95,0},{3,NOV_FLASH_FROM|95},{3,2},{3,NOV_FLASH_FROM},{5,1},{5,2},{5,1},{9,1},{9,2},{9,1}};
 const unsigned char uncoloured[][2]={{0,0},{0,0},{45,0},{3,NOV_FLASH_FROM|45},{3,2},{3,NOV_FLASH_FROM},{5,1},{5,2},{5,1},{9,1},{9,2},{9,1}};
 const unsigned char legacy[][2]={{0,0},{0,0},{45,0},{21,1},{21,0},{9,1},{5,1},{5,2},{5,1},{9,1},{9,2},{9,1}};
 for(unsigned i=0;i<sizeof(states)/sizeof(states[0]);i++){unsigned char value,style;
  native_clip_color(states[i],95,1,&value,&style);need(value==coloured[i][0]&&style==coloured[i][1],"clip cells: empty off, ready track colour, queued white flash from track colour, playing white pulse, queued stop white flash, record red, overdub amber");
  native_clip_color(states[i],0,1,&value,&style);need(value==uncoloured[i][0]&&style==uncoloured[i][1],"no track colour: ready keeps the previous blue");
  native_clip_color(states[i],95,0,&value,&style);need(value==legacy[i][0]&&style==legacy[i][1],"scene buttons keep the previous colours; empty scenes are off");
  if(states[i]>=CLIP_WILL_RECORD)for(unsigned cells=0;cells<2;cells++)for(unsigned colour=0;colour<128;colour+=5){native_clip_color(states[i],colour,cells,&value,&style);need(style!=0,"every record and overdub state is animated, so steady red only ever means a ready clip");}
 }
 NovationWire w=novation_led(MCU_PROTOCOL_LPX,45,NOV_FLASH_FROM|95,3);need(w.length==12&&!memcmp(w.bytes,(unsigned char[]){0xf0,0,0x20,0x29,2,0x0c,3,1,45,3,95,0xf7},12),"Launchpad X explicit flash uses LED lighting SysEx type 1, colour B then A");
 w=novation_led(MCU_PROTOCOL_LPP3,45,NOV_FLASH_FROM,3);need(w.length==12&&w.bytes[5]==0x0e&&w.bytes[6]==3&&w.bytes[7]==1&&w.bytes[9]==3&&w.bytes[10]==0,"Launchpad Pro Mk3 uses its own header; queued stop flashes against dark");
 ClipCopy saved=snapshot->clips;unsigned available=snapshot->clips_available;CopiedTrack tracks[8];memcpy(tracks,snapshot->tracks,sizeof(tracks));
 NovationState old[2]={surfaces.surfaces[1].novation,surfaces.surfaces[2].novation};
 snapshot->clips_available=1;snapshot->clips=(ClipCopy){.status=CLIP_SOURCE_READY,.tick=now,.epoch=snapshot->epoch,.project_owner=1,.lanes=100,.columns=16,.rows=17};
 const uint32_t colours[8]={0x00ff80u,0xff00bbu,0xff8800u,0x0022ffu,0,0xff0000u,0xff0000u,0xff0000u};
 for(unsigned t=0;t<8;t++){snapshot->tracks[t].fields[CF_COLOR].available=t!=4;snapshot->tracks[t].fields[CF_COLOR].bits=colours[t];}
 const unsigned column_state[8]={CLIP_READY,CLIP_WILL_PLAY,CLIP_PLAYING,CLIP_WILL_STOP_PLAYING,CLIP_READY,CLIP_RECORDING,CLIP_EMPTY,CLIP_WILL_RECORD};
 for(unsigned s=1;s<=2;s++){NovationState *n=&surfaces.surfaces[s].novation;n->layer=NOV_CLIPS;n->clip_epoch=snapshot->epoch;n->clip_column=0;n->clip_row=s==1?1:0;memset(n->led_valid,0,sizeof(n->led_valid));
  ClipProjection *v=snapshot->clips.views+native_step_slot(surfaces.surfaces+s);v->status=CLIP_SOURCE_READY;v->tick=now;v->first_column=0;v->first_row=n->clip_row;
  for(unsigned j=0;j<8;j++){v->track_owner[j]=snapshot->tracks[j].track_owner;v->scene_state[j]=CLIP_READY;}
  for(unsigned j=0;j<64;j++)v->cells[j]=(ClipCell){column_state[j%8],2000+s*100+j};
 }
 native_capture_count=0;
 for(unsigned pass=0;pass<30;pass++)for(unsigned s=1;s<=2;s++){need(native_output(surfaces.surfaces+s,surfaces.banks+s,snapshot,now),"clip colour projection");need(surfaces.surfaces[s].novation.cycle_messages<=16&&surfaces.surfaces[s].novation.cycle_bytes<=128,"clip colours keep output caps");}
 for(unsigned s=1;s<=2;s++){unsigned client=150+s,device=s==1?0x0c:0x0e;
  for(unsigned row=0;row<8;row++){unsigned base=(8-row)*10+1,arrangement=s==2&&row==0;
   (void)arrangement;NATIVE_HAS(client,0x90,base+0,29);
   NATIVE_HAS(client,0xf0,0,0x20,0x29,2,device,3,1,base+1,3,95,0xf7);
   NATIVE_HAS(client,0x92,base+2,3);
   NATIVE_HAS(client,0xf0,0,0x20,0x29,2,device,3,1,base+3,3,0,0xf7);
   NATIVE_HAS(client,0x90,base+4,45);NATIVE_HAS(client,0x92,base+5,5);NATIVE_HAS(client,0x90,base+6,0);NATIVE_HAS(client,0x91,base+7,5);
  }
  for(unsigned row=0;row<8;row++)NATIVE_HAS(client,0xb0,89-row*10,45);
 }
 native_capture_count=0;for(unsigned pass=0;pass<4;pass++)for(unsigned s=1;s<=2;s++)need(native_output(surfaces.surfaces+s,surfaces.banks+s,snapshot,now),"unchanged clip colours");
 for(unsigned i=0;i<native_capture_count;i++)need(native_capture[i].destination.client!=151&&native_capture[i].destination.client!=152,"unchanged clip colours send nothing");
 snapshot->tracks[0].fields[CF_COLOR].bits=0xff0000u;native_capture_count=0;
 for(unsigned pass=0;pass<4;pass++)need(native_output(surfaces.surfaces+1,surfaces.banks+1,snapshot,now),"changed column colour");
 need(native_capture_count==8,"a changed track colour resends only that column's eight cells");
 for(unsigned row=0;row<8;row++)NATIVE_HAS(151,0x90,(8-row)*10+1,5);
 snapshot->clips=saved;snapshot->clips_available=available;memcpy(snapshot->tracks,tracks,sizeof(tracks));
 surfaces.surfaces[1].novation=old[0];surfaces.surfaces[2].novation=old[1];native_capture_count=0;
 puts("PASS Launchpad X/Pro clip cells: nearest palette track colour, MPC launcher language: empty off, steady track-colour content, white flash/pulse queued/playing, animated red/amber record/overdub, Arrangement row in the same language, scenes/no-colour fallback, change-only resend within caps (ALSA/MPC substituted)");
}
/* Pick: a held step-view button plus a clip pad selects natively (no launch)
 * and opens this Launchpad's step view after the request has left its queue;
 * alone the button opens the view on release; the peer never re-binds. */
/* Every Launchpad button without a function in the current view is dark,
 * including a clip-grid scene light left on when a view is changed. */
static void launchpad_side_render(unsigned i,const CopiedMirror *snapshot,unsigned now){
 NovationState *n=&surfaces.surfaces[i].novation;memset(n->led_valid,0,sizeof(n->led_valid));native_capture_count=0;
 for(unsigned pass=0;pass<40;pass++)need(native_output(surfaces.surfaces+i,surfaces.banks+i,snapshot,now),"side-button rendering");
}
static int launchpad_side_lit(unsigned i,unsigned index){const NovationState *n=&surfaces.surfaces[i].novation;return n->led_valid[index]&&n->led_value[index]!=0;}
static void launchpad_side_checks(CopiedMirror *snapshot,unsigned now){
 ClipCopy clips=snapshot->clips;unsigned clips_available=snapshot->clips_available,steps_available=snapshot->steps_available;
 snapshot->clips_available=0;snapshot->steps_available=0;
 NovationState old[2]={surfaces.surfaces[1].novation,surfaces.surfaces[2].novation};
 static const unsigned char x_clips[]={91,92,93,94,95,96,97,98},x_step[]={19,29,39,91,92,93,94,95,96,97},x_mixer[]={19,29,39,49,59,69,79,89,93,94,95,96,97},x_device[]={19,29,39,49,59,69,79,89,91,92,93,94,95,96,97};
 static const unsigned char p_clips[]={1,2,3,4,5,6,7,8,10,20,70,80,91,92,93,96,97,98},p_step[]={8,10,20,70,79,80,91,92,93,96,97},p_mixer[]={1,2,3,4,5,6,7,10,20,91,92,93,96,97},p_device[]={1,2,3,4,5,6,7,10,20,70,80,91,92,93,96,97};
 struct {unsigned layer;const unsigned char *x,*p;unsigned xn,pn;const char *name;} cases[]={
  {NOV_CLIPS,x_clips,p_clips,sizeof(x_clips),sizeof(p_clips),"clip grid"},{NOV_STEP64,x_step,p_step,sizeof(x_step),sizeof(p_step),"64-step view"},{NOV_STEP16,x_step,p_step,sizeof(x_step),sizeof(p_step),"16-step view"},
  {NOV_SESSION,x_mixer,p_mixer,sizeof(x_mixer),sizeof(p_mixer),"mixer grid"},{NOV_VOLUME,x_mixer,p_mixer,sizeof(x_mixer),sizeof(p_mixer),"Volume"},{NOV_DEVICE,x_device,p_device,sizeof(x_device),sizeof(p_device),"Device"}};
 for(unsigned i=1;i<=2;i++)for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);c++){
  NovationState *n=&surfaces.surfaces[i].novation;unsigned protocol=n->protocol;memset(n->held,0,sizeof(n->held));native_layer(surfaces.surfaces+i,surfaces.banks+i,cases[c].layer);launchpad_side_render(i,snapshot,now);
  const unsigned char *want=i==1?cases[c].x:cases[c].p;unsigned count=i==1?cases[c].xn:cases[c].pn;
  for(unsigned index=0;index<128;index++){
   if(!novation_led_index(protocol,index)||(index>=11&&index<=88&&index%10>=1&&index%10<=8)||(index>=101&&index<=108))continue; /* grid and Track Select row: own tests */
   unsigned expected=0;for(unsigned k=0;k<count;k++)expected|=want[k]==index;
   if(launchpad_side_lit(i,index)!=(int)expected)fprintf(stderr,"side button surface=%u view=%s index=%u lit=%d expected=%u\n",i,cases[c].name,index,launchpad_side_lit(i,index),expected);
   need(launchpad_side_lit(i,index)==(int)expected,"only buttons with a function in this view are lit");
  }
 }
 /* A lit Pro scene button (39) is turned off when the step view opens. */
 NovationState *pro=&surfaces.surfaces[2].novation;native_layer(surfaces.surfaces+2,surfaces.banks+2,NOV_CLIPS);pro->clip_epoch=snapshot->epoch;pro->clip_column=0;pro->clip_row=0;
 snapshot->clips_available=1;snapshot->clips=(ClipCopy){.status=CLIP_SOURCE_READY,.tick=now,.epoch=snapshot->epoch,.project_owner=1,.lanes=100,.columns=8,.rows=8};
 ClipProjection *v=snapshot->clips.views+native_step_slot(surfaces.surfaces+2);*v=(ClipProjection){.status=CLIP_SOURCE_READY,.tick=now};v->scene_state[5]=CLIP_PLAYING;
 launchpad_side_render(2,snapshot,now);need(launchpad_side_lit(2,39),"a playing scene lights its scene button in the clip grid");
 native_layer(surfaces.surfaces+2,surfaces.banks+2,NOV_STEP64);launchpad_side_render(2,snapshot,now);need(pro->led_valid[39]&&!pro->led_value[39],"leaving the clip grid turns that scene button off");
 for(unsigned i=1;i<=2;i++)surfaces.surfaces[i].novation=old[i-1];
 snapshot->clips=clips;snapshot->clips_available=clips_available;snapshot->steps_available=steps_available;native_capture_count=0;
 puts("PASS Launchpad X/Pro side buttons: in clip grid, both step views, mixer, Volume and Device only buttons with a function there are lit; a clip-grid scene light is cleared on leaving (ALSA sink and native state substituted)");
}
/* Pro Stop Clip never stops MPC: in the clip grid it is the stop modifier
 * (held with a pad or scene), in a step view it stops the bound clip's track,
 * alone in the grid or a mixer layer it does nothing. Its light follows. */
static void launchpad_pro_stop_clip_checks(const MirrorState *state,ControllerRuntime *runtime){
 CopiedMirror *snapshot=runtime->snapshot;ClipCopy saved=snapshot->clips;StepCopy saved_steps=snapshot->steps;unsigned available=snapshot->clips_available,steps_available=snapshot->steps_available,now=runtime->now;MIRROR_FIELD(playing);playing=snapshot->playing;
 NovationState old[2]={surfaces.surfaces[1].novation,surfaces.surfaces[2].novation};snapshot->clips_available=1;snapshot->clips=(ClipCopy){.status=CLIP_SOURCE_READY,.tick=now,.epoch=snapshot->epoch,.project_owner=1,.lanes=100,.columns=16,.rows=17};
 snapshot->playing.available=1;snapshot->playing.bits=1;unsigned editor=snapshot->editor_owner;snapshot->editor_owner=777;
 for(unsigned i=1;i<=2;i++){NovationState *n=&surfaces.surfaces[i].novation;memset(n->held,0,sizeof(n->held));n->clip_epoch=snapshot->epoch;n->clip_column=0;n->clip_row=1;native_layer(surfaces.surfaces+i,surfaces.banks+i,NOV_CLIPS);
  ClipProjection *v=snapshot->clips.views+native_step_slot(surfaces.surfaces+i);*v=(ClipProjection){.status=CLIP_SOURCE_READY,.tick=now,.first_column=0,.first_row=1};
  for(unsigned j=0;j<8;j++){v->track_owner[j]=j+1;}for(unsigned j=0;j<64;j++)v->cells[j]=(ClipCell){CLIP_PLAYING,5000+i*100+j};input_discard(&surfaces.contexts[i].input);}
 MirrorInput *x=&surfaces.contexts[1].input,*pro=&surfaces.contexts[2].input;NovationState *pn=&surfaces.surfaces[2].novation;
 unsigned xg=x->global_count,pg=pro->global_count;
 fake_input_reset();native_push(1,0xb0,98,127);native_push(1,0x90,72,127);native_push(1,0x90,72,0);native_push(1,0xb0,98,0);
 native_push(2,0xb0,8,127);native_push(2,0x90,72,127);native_push(2,0x90,72,0);native_push(2,0xb0,8,0);
 need(collection_drain(&surfaces,state,runtime),"stop chords reach ingress");
 need(x->step_count==1&&x->step_events[x->step_head].request.bits==CLIP_STOP&&x->step_events[x->step_head].request.pad_index==1,"X Capture MIDI + pad stops that track");
 need(pro->step_count==1&&pro->step_events[pro->step_head].request.bits==CLIP_STOP&&pro->step_events[pro->step_head].request.pad_index==1&&pro->global_count==pg,"Pro Stop Clip + pad stops only that track: no MPC Stop");
 input_discard(pro);fake_input_reset();native_push(2,0xb0,98,127);native_push(2,0x90,73,127);native_push(2,0x90,73,0);native_push(2,0xb0,98,0);
 need(collection_drain(&surfaces,state,runtime)&&pro->step_count==1&&pro->step_events[pro->step_head].request.bits==CLIP_STOP&&pro->step_events[pro->step_head].request.pad_index==2&&pro->global_count==pg,"Pro Projects + pad still stops that track");
 input_discard(pro);fake_input_reset();native_push(2,0xb0,8,127);native_push(2,0xb0,79,127);native_push(2,0xb0,79,0);native_push(2,0xb0,8,0);
 need(collection_drain(&surfaces,state,runtime)&&pro->step_count==1&&pro->step_events[pro->step_head].request.bits==CLIP_SCENE_STOP&&pro->global_count==pg,"Pro Stop Clip + scene button stops that scene");
 input_discard(pro);fake_input_reset();native_push(2,0xb0,8,127);native_push(2,0xb0,8,0);need(collection_drain(&surfaces,state,runtime)&&!pro->step_count&&pro->global_count==pg&&pn->row_mode==NOV_ROW_STOP,"a quick Stop Clip tap in the clip grid sends nothing and latches the Stop row");
 memset(pn->led_valid,0,sizeof(pn->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surfaces.surfaces+2,surfaces.banks+2,snapshot,now),"Stop Clip light in the grid");need(pn->led_valid[8]&&pn->led_value[8]==5,"latched Stop Clip is red in the clip grid");fake_input_reset();native_push(2,0xb0,8,127);native_push(2,0xb0,8,0);need(collection_drain(&surfaces,state,runtime)&&pn->row_mode==NOV_ROW_SELECT,"a second tap clears it");memset(pn->led_valid,0,sizeof(pn->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surfaces.surfaces+2,surfaces.banks+2,snapshot,now),"Stop Clip light idle");need(pn->led_valid[8]&&pn->led_value[8]==3,"idle Stop Clip is lit in the clip grid");
 native_layer(surfaces.surfaces+2,surfaces.banks+2,NOV_VOLUME);fake_input_reset();native_push(2,0xb0,8,127);native_push(2,0xb0,8,0);need(collection_drain(&surfaces,state,runtime)&&!pro->step_count&&pro->global_count==pg,"Stop Clip alone in a mixer layer does nothing");
 memset(pn->led_valid,0,sizeof(pn->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surfaces.surfaces+2,surfaces.banks+2,snapshot,now),"Stop Clip light in Volume");need(pn->led_valid[8]&&!pn->led_value[8],"Stop Clip is dark where it does nothing");
 /* In a step view it stops the track of the bound clip, without MPC Stop. */
 native_layer(surfaces.surfaces+2,surfaces.banks+2,NOV_STEP64);pn->step_epoch=snapshot->epoch;pn->step_pad=2;pn->step_first=0;
 snapshot->steps_available=1;snapshot->steps=(StepCopy){.status=STEP_READY,.epoch=snapshot->epoch,.tick=now};
 StepProjection *sv=snapshot->steps.views+native_step_slot(surfaces.surfaces+2);*sv=(StepProjection){.status=STEP_READY,.tick=now,.pad=2,.notes={38,39,40,41},.valid={UINT32_MAX,UINT32_MAX}};
 sv->target=(StepTarget){.epoch=snapshot->epoch,.project_owner=1,.track_owner=7,.program_owner=8,.serial=9,.binding=1,.incarnation=1,.control=10,.sequence=30,.program=40,.event_owner=20,.clip=50,.bind_generation=pn->step_bind_generation,.division_bits=0x41800000,.extent={15360,0}};
 fake_input_reset();native_push(2,0xb0,8,127);native_push(2,0xb0,8,0);need(collection_drain(&surfaces,state,runtime),"Stop Clip in a step view reaches ingress");
 const CommandRequest *r=&pro->step_events[pro->step_head].request;
 need(pro->step_count==1&&r->reserved==STEP_STOP&&r->effect_slot==native_step_slot(surfaces.surfaces+2)+1&&r->track_owner==7&&r->effect_generation==20&&r->effect_position==pn->step_bind_generation&&command_step_format(r)&&pro->global_count==pg,"Stop Clip in a step view stops the bound clip's track, not MPC");
 memset(pn->led_valid,0,sizeof(pn->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surfaces.surfaces+2,surfaces.banks+2,snapshot,now),"Stop Clip light in a step view");need(pn->led_valid[8]&&pn->led_value[8]==3,"Stop Clip is lit in a step view");
 need(x->global_count==xg,"X has no MPC Stop from its chord");
 for(unsigned i=1;i<=2;i++){input_discard(&surfaces.contexts[i].input);surfaces.surfaces[i].novation=old[i-1];}snapshot->clips=saved;snapshot->clips_available=available;snapshot->steps=saved_steps;snapshot->steps_available=steps_available;snapshot->playing=playing;snapshot->editor_owner=editor;native_capture_count=0;
 puts("PASS Launchpad Pro Stop Clip: never MPC Stop; held with a pad or scene in the clip grid it stops that track or scene like Projects and X Capture MIDI; in a step view it stops the bound clip's track; alone in the grid or a mixer layer nothing; lit only where it works (ALSA/native state substituted)");
}
/* Approved layout (2026-10-21): Pro Track Select row with Record Arm/Mute/Solo/
 * Stop Clip row modes (tap latches, hold is momentary), X Custom reopening the
 * last mixer page, Session never blue, bipolar pan pads and display. */
static void launchpad_layout_checks(const MirrorState *state,ControllerRuntime *runtime){
 CopiedMirror *s=runtime->snapshot,*saved=calloc(1,sizeof(*saved));need(saved!=NULL,"layout source save");*saved=*s;
 MirrorBank old_banks[2]={surfaces.banks[1],surfaces.banks[2]};NovationState old[2]={surfaces.surfaces[1].novation,surfaces.surfaces[2].novation};unsigned now=runtime->now;
 snapshot_fixture(s,16);s->selection.bits=s->tracks[0].track;s->editor_owner=777;
 for(unsigned i=1;i<=2;i++){NovationState *n=&surfaces.surfaces[i].novation;memset(n->held,0,sizeof(n->held));n->row_mode=NOV_ROW_SELECT;n->row_hold=0;n->mixer_page=0;surfaces.banks[i].view=BV_TRACK;surfaces.banks[i].offset=0;bank_apply(surfaces.banks+i,s,now);input_discard(&surfaces.contexts[i].input);}
 NovationState *x=&surfaces.surfaces[1].novation,*pro=&surfaces.surfaces[2].novation;MirrorInput *xin=&surfaces.contexts[1].input,*pin=&surfaces.contexts[2].input;
 /* Pro: Volume page, Track Select row selects. */
 native_layer(surfaces.surfaces+2,surfaces.banks+2,NOV_CLIPS);fake_input_reset();native_push(2,0xb0,4,127);native_push(2,0xb0,4,0);need(collection_drain(&surfaces,state,runtime)&&pro->layer==NOV_VOLUME,"Pro Volume page");
 fake_input_reset();native_push(2,0xb0,102,127);native_push(2,0xb0,102,0);need(collection_drain(&surfaces,state,runtime)&&pin->desires[1][CF_SELECTION].pending&&pin->desires[1][CF_SELECTION].identity.serial==2,"Track Select row selects track 2 with no mode");input_discard(pin);
 /* Tap Record Arm: latched; row arms. */
 fake_input_reset();native_push(2,0xb0,1,127);native_push(2,0xb0,1,0);need(collection_drain(&surfaces,state,runtime)&&pro->row_mode==NOV_ROW_ARM,"a quick Record Arm tap latches the Arm row");
 fake_input_reset();native_push(2,0xb0,101,127);native_push(2,0xb0,101,0);need(collection_drain(&surfaces,state,runtime)&&pin->edge_count==1&&pin->edges[pin->edge_head].field==CF_ARM&&pin->edges[pin->edge_head].desire.identity.serial==1,"the Arm row toggles arm on track 1");input_discard(pin);
 /* Hold Mute: momentary; release keeps Arm latched. */
 fake_input_reset();native_push(2,0xb0,2,127);native_push(2,0xb0,103,127);native_push(2,0xb0,103,0);need(collection_drain(&surfaces,state,runtime)&&pin->edge_count==1&&pin->edges[pin->edge_head].field==CF_MUTE&&pin->edges[pin->edge_head].desire.identity.serial==3,"held Mute makes the row mute track 3");input_discard(pin);
 fake_input_reset();native_push(2,0xb0,2,0);need(collection_drain(&surfaces,state,runtime)&&pro->row_mode==NOV_ROW_ARM&&!pro->row_hold,"releasing a used Mute hold keeps the latched Arm row");
 /* Tap Solo: latch moves to Solo; tap Solo again clears. */
 fake_input_reset();native_push(2,0xb0,3,127);native_push(2,0xb0,3,0);need(collection_drain(&surfaces,state,runtime)&&pro->row_mode==NOV_ROW_SOLO,"tapping Solo latches the Solo row");
 memset(pro->led_valid,0,sizeof(pro->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surfaces.surfaces+2,surfaces.banks+2,s,now),"Pro row output");
 need(pro->led_value[3]==45&&pro->led_value[1]==1&&pro->led_value[2]==1&&pro->led_value[101]==47&&pro->led_value[108]==47&&pro->led_value[4]==21&&pro->led_value[5]==3&&!pro->led_value[49]&&!pro->led_value[8],"Solo row: unsoloed tracks dim blue; Solo button blue; Volume page green; right column and Stop Clip dark on mixer pages");
 fake_input_reset();native_push(2,0xb0,3,127);native_push(2,0xb0,3,0);need(collection_drain(&surfaces,state,runtime)&&pro->row_mode==NOV_ROW_SELECT,"tapping the latched mode clears it");
 memset(pro->led_valid,0,sizeof(pro->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surfaces.surfaces+2,surfaces.banks+2,s,now),"Pro select row output");
 need(pro->led_value[101]==3&&pro->led_value[102]&&pro->led_value[102]!=3&&pro->led_value[93]==3&&pro->led_value[96]==21,"Select row: selected track white, others lit; Session dim (never blue); Custom green");
 /* Step views: row and row modes dark and inert. */
 native_layer(surfaces.surfaces+2,surfaces.banks+2,NOV_STEP64);fake_input_reset();native_push(2,0xb0,1,127);native_push(2,0xb0,1,0);native_push(2,0xb0,101,127);native_push(2,0xb0,101,0);need(collection_drain(&surfaces,state,runtime)&&pro->row_mode==NOV_ROW_SELECT&&!pin->edge_count&&!pin->desires[0][CF_SELECTION].pending,"in a step view the row and row modes do nothing");
 memset(pro->led_valid,0,sizeof(pro->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surfaces.surfaces+2,surfaces.banks+2,s,now),"Pro step row output");need(!pro->led_value[101]&&!pro->led_value[1],"row dark in step views");
 /* Clip grid: latched Stop row stops that track's clip. */
 ClipCopy clips=s->clips;unsigned clips_available=s->clips_available;s->clips_available=1;s->clips=(ClipCopy){.status=CLIP_SOURCE_READY,.tick=now,.epoch=s->epoch,.project_owner=1,.lanes=100,.columns=16,.rows=17};
 native_layer(surfaces.surfaces+2,surfaces.banks+2,NOV_CLIPS);pro->clip_epoch=s->epoch;pro->clip_column=0;pro->clip_row=0;
 ClipProjection *v=s->clips.views+native_step_slot(surfaces.surfaces+2);*v=(ClipProjection){.status=CLIP_SOURCE_READY,.tick=now};for(unsigned j=0;j<8;j++)v->track_owner[j]=s->tracks[j].track_owner;for(unsigned j=0;j<64;j++)v->cells[j]=(ClipCell){j%8==2?CLIP_PLAYING:CLIP_READY,7000+j};
 fake_input_reset();native_push(2,0xb0,8,127);native_push(2,0xb0,8,0);native_push(2,0xb0,103,127);native_push(2,0xb0,103,0);need(collection_drain(&surfaces,state,runtime)&&pro->row_mode==NOV_ROW_STOP&&pin->step_count==1&&pin->step_events[pin->step_head].request.bits==CLIP_STOP&&pin->step_events[pin->step_head].request.pad_index==2,"latched Stop row stops track 3's clip");
 memset(pro->led_valid,0,sizeof(pro->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surfaces.surfaces+2,surfaces.banks+2,s,now),"Pro stop row output");need(pro->led_value[103]==21&&!pro->led_value[101]&&pro->led_value[8]==5,"Stop row: playing track green, others off; Stop Clip red");
 input_discard(pin);pro->row_mode=NOV_ROW_SELECT;s->clips=clips;s->clips_available=clips_available;
 /* X: Custom reopens the last mixer page; pan pads give an exact centre and light outward. */
 native_layer(surfaces.surfaces+1,surfaces.banks+1,NOV_CLIPS);fake_input_reset();native_push(1,0xb0,97,127);native_push(1,0xb0,97,0);native_push(1,0xb0,79,127);native_push(1,0xb0,79,0);need(collection_drain(&surfaces,state,runtime)&&x->layer==NOV_PAN,"X Custom then Pan");
 fake_input_reset();native_push(1,0xb0,95,127);native_push(1,0xb0,95,0);native_push(1,0xb0,97,127);native_push(1,0xb0,97,0);need(collection_drain(&surfaces,state,runtime)&&x->layer==NOV_PAN,"X Custom reopens the last mixer page (Pan)");
 fake_input_reset();native_push(1,0x90,41,127);native_push(1,0x90,41,0);native_push(1,0x90,18,127);native_push(1,0x90,18,0);need(collection_drain(&surfaces,state,runtime),"pan pads");
 {float a,b;memcpy(&a,&xin->desires[0][CF_PAN].bits,4);memcpy(&b,&xin->desires[7][CF_PAN].bits,4);need(xin->desires[0][CF_PAN].pending&&a==0.5f&&xin->desires[7][CF_PAN].pending&&b==0.0f,"the middle pad is the exact centre (0.5); the bottom pad hard left");}
 input_discard(xin);memset(x->led_valid,0,sizeof(x->led_valid));for(unsigned pass=0;pass<40;pass++)need(native_output(surfaces.surfaces+1,surfaces.banks+1,s,now),"X pan output");
 for(unsigned strip=0;strip<8;strip++)for(unsigned row=1;row<=8;row++){unsigned pad=row*10+strip+1;need(x->led_value[pad]==(row==4||row==5?3u:0u),"centred pan: the middle pair white, the rest dark");}
 need(x->led_value[79]==21&&x->led_value[89]==3&&x->led_value[97]==21&&x->led_value[95]==3,"X Pan page button green, others dim, Custom green, Session dim");
 for(unsigned i=1;i<=2;i++){input_discard(&surfaces.contexts[i].input);surfaces.surfaces[i].novation=old[i-1];surfaces.banks[i]=old_banks[i-1];}*s=*saved;free(saved);native_capture_count=0;
 puts("PASS approved Launchpad layout: Pro Track Select row selects, Arm/Mute/Solo/Stop row modes (tap latch, hold momentary) with LEDs, inert in step views; X Custom reopens the last mixer page; Session never blue; bipolar pan pads with an exact centre (ALSA/native state substituted)");
}
static void launchpad_pick_checks(const MirrorState *state,ControllerRuntime *runtime){
 CopiedMirror *snapshot=runtime->snapshot;ClipCopy saved=snapshot->clips;unsigned available=snapshot->clips_available,now=runtime->now;
 NovationState old[2]={surfaces.surfaces[1].novation,surfaces.surfaces[2].novation};snapshot->clips_available=1;snapshot->clips=(ClipCopy){.status=CLIP_SOURCE_READY,.tick=now,.epoch=snapshot->epoch,.project_owner=1,.lanes=100,.columns=16,.rows=17};
 for(unsigned i=1;i<=2;i++){NovationState *n=&surfaces.surfaces[i].novation;memset(n->held,0,sizeof(n->held));n->clip_epoch=snapshot->epoch;n->clip_column=0;n->clip_row=1;n->layer=NOV_CLIPS;n->pick_used=n->pick_pending=0;n->step_mode=i==1?NOV_STEP16:0;
  ClipProjection *v=snapshot->clips.views+native_step_slot(surfaces.surfaces+i);*v=(ClipProjection){.status=CLIP_SOURCE_READY,.tick=now,.first_column=0,.first_row=1};
  for(unsigned j=0;j<8;j++)v->track_owner[j]=j+1;
  for(unsigned j=0;j<64;j++)v->cells[j]=(ClipCell){CLIP_READY,3000+i*100+j};
  input_discard(&surfaces.contexts[i].input);
 }
 for(unsigned s=1;s<=2;s++){
  NovationState *n=&surfaces.surfaces[s].novation,*peer=&surfaces.surfaces[3-s].novation;MirrorInput *in=&surfaces.contexts[s].input;unsigned button=s==1?96:97,peer_bind=peer->step_bind_generation,bind=n->step_bind_generation;
  fake_input_reset();native_push(s,0xb0,button,127);need(collection_drain(&surfaces,state,runtime)&&n->layer==NOV_CLIPS&&n->step_bind_generation==bind,"a held step-view button does not leave the clip grid on press");
  fake_input_reset();native_push(s,0x90,72,127);native_push(s,0x90,72,0);need(collection_drain(&surfaces,state,runtime)&&in->step_count==1,"pick queues exactly one request");
  const CommandRequest *r=&in->step_events[in->step_head].request;need(r->bits==CLIP_SELECT&&r->reserved==CLIP_ACTION&&r->pad_index==1&&r->effect_index==2&&r->track_owner==2,"pick is a native select of the pressed clip, not a launch");
  need(n->pick_pending&&n->pick_used,"pick waits for its request");
  need(native_output(surfaces.surfaces+s,surfaces.banks+s,snapshot,now)&&n->layer==NOV_CLIPS,"the step view does not open while the select is still queued");
  in->step_count=0; /* the bridge submitted it and the observer processed it */
  need(native_output(surfaces.surfaces+s,surfaces.banks+s,snapshot,now)&&n->layer==(s==1?NOV_STEP16:NOV_STEP64)&&n->step_bind_generation==bind+1&&!n->pick_pending,"after the select, this Launchpad opens its last-used step view and binds afresh (X last used 16 steps, a new Pro 64)");
  need(peer->step_bind_generation==peer_bind&&peer->layer==NOV_CLIPS,"the other Launchpad does not re-bind on its own");
  fake_input_reset();native_push(s,0xb0,button,0);need(collection_drain(&surfaces,state,runtime)&&n->layer==(s==1?NOV_STEP16:NOV_STEP64)&&n->step_bind_generation==bind+1,"releasing after a pick changes nothing more");
  native_layer(surfaces.surfaces+s,surfaces.banks+s,NOV_CLIPS);n->clip_row=1;
  /* Alone: opens on release, as before but no longer on press. */
  unsigned buttons[2]={96,97};for(unsigned b=0;b<1u;b++){unsigned id=s==1?buttons[b]:97,expect=novation_step_entry(n);bind=n->step_bind_generation;
   fake_input_reset();native_push(s,0xb0,id,127);need(collection_drain(&surfaces,state,runtime)&&n->layer==NOV_CLIPS,"step-view button alone waits for release");
   fake_input_reset();native_push(s,0xb0,id,0);need(collection_drain(&surfaces,state,runtime)&&n->layer==expect&&n->step_bind_generation==bind+1&&!in->step_count,"step-view button alone opens its view on release and binds the MPC selection");
   native_layer(surfaces.surfaces+s,surfaces.banks+s,NOV_CLIPS);n->clip_row=1;}
  /* Plain press still launches; stop and record holds win over pick. */
  fake_input_reset();native_push(s,0x90,72,127);native_push(s,0x90,72,0);need(collection_drain(&surfaces,state,runtime)&&in->step_count==2&&in->step_events[in->step_head].request.bits==CLIP_LAUNCH&&in->step_events[(in->step_head+1)%INPUT_JOG_EVENTS].request.bits==CLIP_SELECT&&in->step_events[(in->step_head+1)%INPUT_JOG_EVENTS].request.pad_index==in->step_events[in->step_head].request.pad_index,"a plain clip press launches, then selects that clip");input_discard(in);
  fake_input_reset();native_push(s,0xb0,98,127);native_push(s,0xb0,button,127);native_push(s,0x90,72,127);native_push(s,0x90,72,0);native_push(s,0xb0,button,0);native_push(s,0xb0,98,0);
  need(collection_drain(&surfaces,state,runtime)&&in->step_count==1&&in->step_events[in->step_head].request.bits==CLIP_STOP&&n->layer==NOV_CLIPS,"stop hold wins over pick and release does not open the view");input_discard(in);
 }
 /* Outside the clip grid the step-view buttons keep acting on press. */
 NovationState *x=&surfaces.surfaces[1].novation;native_layer(surfaces.surfaces+1,surfaces.banks+1,NOV_VOLUME);fake_input_reset();native_push(1,0xb0,96,127);
 need(collection_drain(&surfaces,state,runtime)&&x->layer==NOV_STEP16,"from the mixer, Note still opens the last-used step view on press");
 for(unsigned i=1;i<=2;i++){input_discard(&surfaces.contexts[i].input);surfaces.surfaces[i].novation=old[i-1];}snapshot->clips=saved;snapshot->clips_available=available;
 puts("PASS Launchpad pick: hold Note (X) or Sequencer (Pro) + clip selects natively without launch, opens this step view after the select, peer unchanged; alone opens on release; plain press launches; stop wins (ALSA/native clip state substituted)");
}
static void novation_play_toggle_checks(MirrorState *state,ControllerRuntime *runtime){
 CopiedMirror *s=runtime->snapshot;MIRROR_FIELD(playing);playing=s->playing;unsigned owner=s->editor_owner;
 s->editor_owner=777;s->playing.available=1;
 const unsigned layers[]={NOV_SESSION,NOV_CLIPS,NOV_STEP64,NOV_STEP16};
 for(unsigned instance=0;instance<=2;instance+=2){
  Surface *surface=surfaces.surfaces+instance;NovationState saved=surface->novation;
  MirrorInput *in=&surfaces.contexts[instance].input;unsigned cc=instance?20:116;
  for(unsigned layer=0;layer<4;layer++){
   for(unsigned i=0;i<surfaces.count;i++){input_discard(&surfaces.contexts[i].input);input_jog_discard(&surfaces.contexts[i].input);}
   memset(surface->novation.held,0,sizeof(surface->novation.held));surface->novation.layer=layers[layer];surface->novation.observed_mode=instance?1:layer%2+1;s->playing.bits=0;
   fake_input_reset();native_push(instance,0xb0,cc,0);native_push(instance,0xb0,cc,127);native_push(instance,0xb0,cc,127);native_push(instance,0xb0,cc,0);
   need(collection_drain(&surfaces,state,runtime)&&in->global_count==1&&in->global_events[in->global_head].operation==GLOBAL_PLAY,"XL3/Pro stopped Play starts once; held and release packets add no commands");
   fake_input_reset();native_push(instance,0xb0,cc,127);native_push(instance,0xb0,cc,0);
   need(collection_drain(&surfaces,state,runtime)&&in->global_count==2&&in->global_events[(in->global_head+1)%INPUT_JOG_EVENTS].operation==GLOBAL_STOP&&!in->stop_home&&!in->jog_count,"second Play pauses pending playback without returning home");
   s->playing.bits=1;fake_input_reset();native_push(instance,0xb0,cc,127);native_push(instance,0xb0,cc,127);native_push(instance,0xb0,cc,0);
   need(collection_drain(&surfaces,state,runtime)&&in->global_count==3&&in->global_events[(in->global_head+2)%INPUT_JOG_EVENTS].operation==GLOBAL_STOP&&!in->stop_home,"externally started playback pauses once from authoritative Playing");
   s->playing.bits=0;fake_input_reset();native_push(instance,0xb0,cc,127);native_push(instance,0xb0,cc,0);
   need(collection_drain(&surfaces,state,runtime)&&in->global_count==4&&in->global_events[(in->global_head+3)%INPUT_JOG_EVENTS].operation==GLOBAL_PLAY,"paused Play resumes via existing native Play");
   s->playing.bits=1;input_stop_home(in,s,runtime->now);s->playing.bits=0;
   fake_input_reset();native_push(instance,0xb0,cc,127);native_push(instance,0xb0,cc,0);
   need(collection_drain(&surfaces,state,runtime)&&in->global_count==5&&in->global_events[(in->global_head+4)%INPUT_JOG_EVENTS].operation==GLOBAL_PLAY,"external Stop after observed playback does not retain predicted Playing");
   s->playing.available=0;fake_input_reset();native_push(instance,0xb0,cc,127);native_push(instance,0xb0,cc,0);
   need(collection_drain(&surfaces,state,runtime)&&in->global_count==5,"unavailable Playing cannot guess a toggle");s->playing.available=1;
   input_jog_discard(in);in->jog_epoch=s->epoch-1;in->stop_play_wait=1;s->playing.bits=0;
   fake_input_reset();native_push(instance,0xb0,cc,127);native_push(instance,0xb0,cc,0);
   need(collection_drain(&surfaces,state,runtime)&&in->global_count==1&&in->global_events[in->global_head].operation==GLOBAL_PLAY,"old-project pending Play cannot turn new stopped Play into Pause");
  }
  surface->novation=saved;
 }
 for(unsigned i=0;i<surfaces.count;i++){input_discard(&surfaces.contexts[i].input);input_jog_discard(&surfaces.contexts[i].input);}
 s->playing=playing;s->editor_owner=owner;
 puts("PASS XL3/Pro documented Play CCs: start/pause/resume, held/release suppression, pending Play, external transport, unavailable readback and all layers; MPC/ALSA substituted");
}
static void novation_production_checks(void){
 CommandState *commands=calloc(1,sizeof(*commands));MirrorState *state=calloc(1,sizeof(*state));need(commands&&state,"native production fixture allocation");
 MirrorBank unused;MIRROR_COPY(snapshot);reset_fixture(commands,&input,&unused,&snapshot,16);collection_fixture_source(state);shared_fixture_time(state,1000);
 const McuProfile *profiles[]={&mcu_profile_lcxl3,&mcu_profile_lpx,&mcu_profile_lpp3,&mcu_profile_generic,&mcu_profile_hui};
 const char *names[]={"Native LC","Native X","Native Pro","MCU","HUI"};
 McuControllerCollection saved={.count=5};FakeClient clients[5];memset(clients,0,sizeof(clients));
 for(unsigned i=0;i<5;i++){
  saved.records[i]=(McuControllerPreference){.enabled=1,.hui_policy=i==4?MCU_HUI_PICKUP:0,.input={profiles[i],names[i],i==1||i==2?"From MIDI":"From DAW",MCU_ENDPOINT_USER},.output={profiles[i],names[i],i==1||i==2?"To MIDI":"To DAW",MCU_ENDPOINT_USER}};
  clients[i]=(FakeClient){.name=names[i],.type=SND_SEQ_USER_CLIENT,.client=150+i,.count=3,.ports={{i==1||i==2?"From MIDI":"From DAW",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,1},{i==1||i==2?"To MIDI":"To DAW",SND_SEQ_PORT_CAP_WRITE,2},{i==1||i==2?"DAW MIDI":"Musical MIDI",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE,3}}};
 }
 char directory[]="/tmp/mpclearn-novation-XXXXXX",path[256];need(mkdtemp(directory)!=NULL,"native persistence directory");snprintf(path,sizeof(path),"%s/controller-preferences",directory);
 need(mcu_controller_preferences_write_collection(path,&saved),"native fixed IDs persist through unchanged MCUCFG2 format");McuControllerCollection loaded;need(mcu_controller_preferences_read_collection(path,&loaded)&&loaded.count==5,"native collection readback");for(unsigned i=0;i<5;i++)need(loaded.records[i].input.profile==profiles[i]&&loaded.records[i].output.profile==profiles[i],"exact profile ID readback");
 collection_init(&surfaces,&loaded);surfaces.seq=(snd_seq_t*)1;surfaces.sink=1;surfaces.source=2;fake_enumeration(clients,5);fake_input_reset();native_capture_enabled=1;native_capture_count=0;
 unsigned subscriptions=fake_subscriptions;need(collection_topology(&surfaces,state)&&surfaces.connected==5&&fake_subscriptions==subscriptions+5&&surfaces.topology_scans==1,"five manual DAW endpoints attach through one topology owner");
 ControllerRuntime runtime={.snapshot=&snapshot};runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime),"five-profile init uses one snapshot and settlement");
 NATIVE_HAS(150,0xf0,0,0x20,0x29,2,0x15,2,127,0xf7);NATIVE_HAS(150,0xb6,69,127);NATIVE_HAS(150,0xb6,72,127);NATIVE_HAS(150,0xb6,73,127);NATIVE_HAS(150,0xb6,70,127);NATIVE_HAS(150,0xb6,71,127);
 NATIVE_HAS(151,0xf0,0,0x20,0x29,2,0x0c,0x0e,0xf7);NATIVE_HAS(151,0xf0,0,0x20,0x29,2,0x0c,0,0xf7);NATIVE_HAS(152,0xf0,0,0x20,0x29,2,0x0e,0,0xf7);
 need(surfaces.surfaces[1].novation.stage==2&&surfaces.surfaces[2].novation.stage==1,"Launchpad init waits boundedly for only documented queries");
 unsigned char daw[]={0xf0,0,0x20,0x29,2,0x0c,0x0e,0,0xf7},layout[]={0xf0,0,0x20,0x29,2,0x0c,0,1,0xf7};
 snd_seq_event_t reply;snd_seq_ev_clear(&reply);snd_seq_ev_set_sysex(&reply,sizeof(daw),daw);reply.source=surfaces.surfaces[1].full;fake_input_reset();fake_input_push(reply);snd_seq_ev_set_sysex(&reply,sizeof(layout),layout);fake_input_push(reply);
 unsigned char pro_layout[]={0xf0,0,0x20,0x29,2,0x0e,0,0,0,0,0xf7};snd_seq_ev_set_sysex(&reply,sizeof(pro_layout),pro_layout);reply.source=surfaces.surfaces[2].full;fake_input_push(reply);
 need(collection_drain(&surfaces,state,&runtime)&&surfaces.surfaces[1].novation.pre_valid==3&&surfaces.surfaces[1].novation.pre_mode==0&&surfaces.surfaces[1].novation.pre_layout==1,"query replies recorded before DAW entry and without another snapshot");
 for(unsigned pass=0;pass<40;pass++){if(surfaces.surfaces[0].novation.lc_queried==7&&!surfaces.surfaces[0].novation.lc_received){fake_input_reset();native_push(0,0xb6,69,127);native_push(0,0xb6,72,127);native_push(0,0xb6,73,127);need(collection_drain(&surfaces,state,&runtime),"LCXL readbacks traverse shared ALSA ingress");}shared_fixture_time(state,1300+pass*10);runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime),"bounded native projection cycle");for(unsigned i=0;i<3;i++)need(surfaces.surfaces[i].novation.cycle_messages<=16&&surfaces.surfaces[i].novation.cycle_bytes<=128,"per-profile output cap including init");}
 for(unsigned i=0;i<native_capture_count;i++)if(native_capture[i].destination.client==150){const unsigned char *b=native_capture[i].bytes;need(!(b[0]==0xbf&&b[1]>=13&&b[1]<=36),"encoder display values never use ignored absolute CC feedback");}
 for(unsigned i=0;i<32;i++){
  unsigned char config[]={0xf0,0,0x20,0x29,2,0x15,4,(unsigned char)(i+5),(unsigned char)(i<8?0x64:0x61),0xf7};
  need(native_captured(150,config,sizeof(config))==1,"every fader numeric and encoder text layout configured exactly once");
 }
 need(runtime.snapshot_copies==41&&runtime.settlement_scans==41&&surfaces.interest_visits==41,"Novation plus MCU/HUI add no snapshot/settlement/interest scans");
 NATIVE_HAS(151,0xf0,0,0x20,0x29,2,0x0c,0x0e,1,0xf7);NATIVE_HAS(152,0xf0,0,0x20,0x29,2,0x0e,0x0e,1,0xf7);
 for(unsigned i=0;i<native_capture_count;i++)if(native_capture[i].destination.client<=152){const unsigned char *b=native_capture[i].bytes;need(native_capture[i].destination.port==2,"no output to From or musical port");if(b[0]==0xf0)need(b[6]!=0x10&&b[6]!=1&&b[6]!=9,"Programmer init never changes DAW flag or fader engine");else need((b[0]&15)!=8,"no Drum channel");}
 for(unsigned i=0;i<3;i++){unsigned before=observed_destinations[150+i];need(native_output(surfaces.surfaces+i,surfaces.banks+i,&snapshot,runtime.now),"stable native output");need(observed_destinations[150+i]==before,"unchanged native projection encodes/sends nothing");}
 NATIVE_HAS(151,0xb0,59,0);NATIVE_HAS(152,0xb0,59,0);
 native_capture_count=0; /* Start a new bounded capture for the mode repaint. */
 fake_input_reset();for(unsigned i=1;i<=2;i++){native_push(i,0xb0,98,127);native_push(i,0xb0,i==1?95:93,127);native_push(i,0xb0,i==1?95:93,0);native_push(i,0xb0,98,0);}need(collection_drain(&surfaces,state,&runtime)&&surfaces.surfaces[1].novation.layer==NOV_CLIPS&&surfaces.surfaces[2].novation.layer==NOV_CLIPS,"Capture MIDI/Projects + Session no longer opens the mixer");
 fake_input_reset();native_push(1,0xb0,97,127);native_push(1,0xb0,97,0);native_push(2,0xb0,96,127);native_push(2,0xb0,96,0);need(collection_drain(&surfaces,state,&runtime)&&surfaces.surfaces[1].novation.layer==NOV_VOLUME&&surfaces.surfaces[2].novation.layer==NOV_VOLUME,"Custom opens the mixer pages (Volume first) on both models");
 for(unsigned pass=0;pass<20;pass++)for(unsigned i=1;i<=2;i++)need(native_output(surfaces.surfaces+i,surfaces.banks+i,&snapshot,runtime.now),"mixer repaint after clip default");
 NATIVE_HAS(151,0xb0,89,21);NATIVE_HAS(151,0xb0,59,3);NATIVE_HAS(151,0xb0,97,21);NATIVE_HAS(152,0xb0,4,21);NATIVE_HAS(152,0xb0,7,3);NATIVE_HAS(152,0xb0,96,21);NATIVE_HAS(152,0xb0,59,0);
 lcxl_readback_checks(surfaces.surfaces,state,&runtime);native_restore_checks(surfaces.surfaces+1);native_capture_count=0;
 for(unsigned layer=NOV_VOLUME;layer<=NOV_DEVICE;layer++){
  native_layer(surfaces.surfaces+1,surfaces.banks+1,layer);
  for(unsigned i=0;i<16;i++)need(native_output(surfaces.surfaces+1,surfaces.banks+1,&snapshot,runtime.now),"Programmer layer bounded grid repaint");
  for(unsigned i=0;i<native_capture_count;i++)need(native_capture[i].bytes[0]!=0xf0,"layer switch never sends a hardware DAW fader/layout command");
  native_capture_count=0;native_layer(surfaces.surfaces+1,surfaces.banks+1,NOV_SESSION);for(unsigned i=0;i<16;i++)need(native_output(surfaces.surfaces+1,surfaces.banks+1,&snapshot,runtime.now),"Session layer bounded repaint");
  unsigned pads=0,lit=0;for(unsigned i=0;i<native_capture_count;i++){const unsigned char *b=native_capture[i].bytes;if(native_capture[i].length==3&&(b[0]&0xf0)==0x90&&b[1]>=11&&b[1]<=88&&b[1]%10>=1&&b[1]%10<=8){pads++;lit+=b[2]!=0;}}need(pads==64,"all Session cells repaint within bounded visits");need(lit>=8,"the mixer grid shows its tracks: side buttons never blank the layer (D14)");native_capture_count=0;
 }
 fake_input_reset();native_push(1,0xb0,79,127);native_push(1,0xb0,79,0);need(collection_drain(&surfaces,state,&runtime)&&surfaces.surfaces[1].novation.layer==NOV_PAN&&surfaces.surfaces[2].novation.layer==NOV_VOLUME,"Programmer layer is per instance");fake_input_reset();native_push(1,0xb0,89,127);native_push(1,0xb0,89,0);need(collection_drain(&surfaces,state,&runtime)&&surfaces.surfaces[1].novation.layer==NOV_VOLUME,"X right column returns to Volume");
 /* Different banks/strips keep five simultaneous inputs independently bound. */
 surfaces.banks[1].offset=8;bank_apply(surfaces.banks+1,&snapshot,runtime.now);fake_input_reset();native_push(0,0xbf,5,64);native_push(0,0xbf,5,80);native_push(1,0x90,82,127);native_push(2,0x90,83,127);
 snd_seq_event_t mcu;snd_seq_ev_clear(&mcu);snd_seq_ev_set_pitchbend(&mcu,3,4096);mcu.source=surfaces.surfaces[3].full;fake_input_push(mcu);native_push(4,0xb0,4,64);native_push(4,0xb0,36,0);native_push(4,0xb0,4,80);native_push(4,0xb0,36,0);
 shared_fixture_time(state,1800);runtime_cycle_begin(&runtime);need(collection_drain(&surfaces,state,&runtime)&&collection_cycle(&surfaces,state,&runtime),"five-profile production input and shared command admission");
 need(atomic_load(&commands->published)==5&&runtime.snapshot_copies==42&&runtime.settlement_scans==42,"five commands use one shared copy/scan");unsigned targets=0;for(unsigned i=0;i<COMMAND_SLOTS;i++)if(coordinator_flights[i].flight){targets|=1u<<(coordinator_flights[i].target.serial-1);need(coordinator_flights[i].origin_bank!=NULL,"native command retains originating bank");}need(targets==0x21d,"LCXL3/LPX/LPP3/MCU/HUI retain tracks1/10/3/4/5");
 unsigned before=surfaces.routed;fake_input_reset();native_push(1,0x98,11,127);fake_input_events[0].source.port=3;need(collection_drain(&surfaces,state,&runtime)&&surfaces.routed==before,"unselected DAW interface is never routed into Programmer profile");
 for(unsigned request=1;request<=5;request++){unsigned at=command_find(commands,request);InputFlight *flight=coordinator_flights+at;complete(commands,request,1801);atomic_store(&state->cells[flight->target.serial-1].bits,flight->bits);atomic_store(&state->cells[flight->target.serial-1].revision,4);}
 shared_fixture_time(state,1820);runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime),"one scan settles all five profile origins");for(unsigned i=0;i<5;i++)need(surfaces.contexts[i].input.settled==1,"each profile receives its own source settlement");
 for(unsigned request=1;request<=5;request++)reclaim_fixture(commands,request);
 shared_fixture_time(state,1840);runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime)&&input_drained(&input),"all native/generic flights reclaimed by sole coordinator");
 /* Programmer uses its own eight-level grid, never hardware fader CCs. */
 snapshot_fixture(&snapshot,16);snapshot.qlinks_available=1;snapshot.qlinks=(QLinkCopy){.epoch=snapshot.epoch,.status=QL_SAMPLED,.root=1,.wrapper=2,.provider=3,.generation=4};for(unsigned i=0;i<16;i++)snapshot.qlinks.slots[i]=(QLinkSlot){.sampled=1,.normalized_valid=1,.tick=1700,.bits=0x3f000000,.revision=2};
 runtime.snapshot_attempted=1;runtime.snapshot_result=1;runtime.now=1700;
 fake_input_reset();native_push(1,0xb0,79,127);native_push(1,0x90,88,127);need(collection_drain(&surfaces,state,&runtime)&&surfaces.contexts[1].input.desires[7][CF_PAN].pending,"Programmer Pan grid reaches canonical field");
 fake_input_reset();native_push(1,0xb0,69,127);native_push(1,0x90,17,127);need(collection_drain(&surfaces,state,&runtime)&&surfaces.contexts[1].input.desires[6][CF_SEND1].pending,"Programmer Send grid reaches canonical field");
 fake_input_reset();native_push(1,0xb0,59,127);native_push(1,0xb0,92,127);native_push(1,0x90,88,0);native_push(1,0x90,88,127);need(collection_drain(&surfaces,state,&runtime)&&surfaces.contexts[1].input.qlinks[15].pending&&surfaces.contexts[1].input.qlinks[15].request.qlink_index==15,"Programmer Q-Link page2 grid reaches actual Q-Link16");input_discard(&surfaces.contexts[1].input);
 /* Documented Programmer grid addresses, through both actual profile decoders.
  * Native fields and ALSA are fixtures; this does not prove device mode/routing. */
 for(unsigned instance=1;instance<=1;instance++){ /* The Pro has no CE mixer grid: its Track Select row replaces it. */
  MirrorInput *grid=&surfaces.contexts[instance].input;surfaces.banks[instance].offset=0;
  fake_input_reset();native_push(instance,0xb0,49,127);native_push(instance,0xb0,49,0);need(collection_drain(&surfaces,state,&runtime),"both Programmer profiles enter Session from documented CC");
  const unsigned fields[]={CF_SELECTION,CF_MUTE,CF_SOLO,CF_ARM};const float levels[]={0.0f,42.0f/127.0f,84.0f/127.0f,1.0f};
  for(unsigned row=0;row<8;row++)for(unsigned strip=0;strip<8;strip++){
   input_discard(grid);fake_input_reset();native_push(instance,0x90,11+row*10+strip,0);native_push(instance,0x90,11+row*10+strip,127);native_push(instance,0x90,11+row*10+strip,0);need(collection_drain(&surfaces,state,&runtime),"documented Programmer grid press/release");
   if(row>0&&row<4)need(grid->edge_count==1&&grid->edges[0].field==fields[row]&&grid->edges[0].desire.identity.serial==strip+1,"both Programmer mixer grids preserve every row/column target");
   else{InputGesture *g=&grid->desires[strip][row?CF_VOLUME:CF_SELECTION];uint32_t bits=1;if(row)memcpy(&bits,levels+row-4,4);need(g->pending&&g->identity.serial==strip+1&&g->bits==bits,"both Programmer selection/volume grids preserve every row/column/value");}
  }
  input_discard(grid);
 }
 {NovationState *pro=&surfaces.surfaces[2].novation;unsigned layer=pro->layer;fake_input_reset();native_push(2,0xb0,49,127);native_push(2,0xb0,49,0);need(collection_drain(&surfaces,state,&runtime)&&pro->layer==layer&&pro->layer!=NOV_SESSION,"the Pro right column never opens a mixer grid");}
 puts("PASS documented64-pad mixer grid map on X through selected ALSA ingress; the Pro has none; native state/device mode/musical Track routing substituted");
 /* Pure page/mixer projection uses the canonical fields from the same snapshot. */
 Surface *lc=surfaces.surfaces;MirrorBank *bank=surfaces.banks;MirrorInput *in=&surfaces.contexts[0].input;input_discard(in);snapshot_fixture(&snapshot,16);snapshot.selected_serial=1;snapshot.selection.bits=snapshot.tracks[0].track;bank_apply(bank,&snapshot,1700);
 runtime.snapshot_attempted=1;runtime.snapshot_result=1;runtime.now=1700;
 for(unsigned cc=77;cc<=100;cc++){fake_input_reset();native_push(0,0xb6,cc,65);need(collection_drain(&surfaces,state,&runtime),"host feature ingress");}
 need(collection_drain(&surfaces,state,&runtime),"LCXL3 host feature traffic drains");
 for(unsigned strip=0;strip<8;strip++)need(!in->desires[strip][CF_SEND1].pending&&!in->desires[strip][CF_SEND2].pending&&!in->desires[strip][CF_PAN].pending,"B6 relative-looking feature traffic cannot become commands");
 for(unsigned cc=77;cc<=100;cc++){fake_input_reset();native_push(0,0xbf,cc,65);need(collection_drain(&surfaces,state,&runtime),"relative row ingress");}
 need(collection_drain(&surfaces,state,&runtime),"all LCXL3 BF relative rows traverse shared ALSA ingress");
 for(unsigned strip=0;strip<8;strip++)need(in->desires[strip][CF_SEND1].pending&&in->desires[strip][CF_SEND2].pending&&in->desires[strip][CF_PAN].pending,"BF77..100 bind all eight Send1 Send2 Pan fields");
 /* Native capture uses BE55/BE5D for touch alongside BF55/BF5D movement.
  * Every relative row must retain the same physical identity as its absolute CC. */
 for(unsigned row=0;row<3;row++){
  unsigned cc=77+row*8,index=8+row*8,field=row==0?CF_SEND1:row==1?CF_SEND2:CF_PAN;
  bank->offset=0;bank_apply(bank,&snapshot,1700);input_discard(in);
  fake_input_reset();native_push(0,0xbe,cc,127);native_push(0,0xbf,cc,65);
  need(collection_drain(&surfaces,state,&runtime)&&lc->novation.touch[index]&&in->desires[0][field].pending,"raw relative touch and movement bind canonical row/field");
  need(!memcmp(&lc->native_touch_identity[index],&bank->strips[0],sizeof(MotorIdentity)),"relative touch captures current strip identity");
  bank->offset=8;bank_apply(bank,&snapshot,1700);input_discard(in);
  fake_input_reset();native_push(0,0xbf,cc,65);
  need(collection_drain(&surfaces,state,&runtime)&&!in->desires[0][field].pending,"held relative encoder cannot retarget through bank remap");
  native_capture_count=0;lc->novation.position_valid[index]=0;
  for(unsigned pass=0;pass<8;pass++)need(native_output(lc,bank,&snapshot,1700),"held relative feedback projection");
  unsigned char position[14]={0xf0,0,0x20,0x29,2,0x15,6,(unsigned char)(index+5),1};
  int digits=snprintf((char*)position+9,4,"%u",(unsigned char)native_position(&snapshot,bank,0,field,NULL,NULL));unsigned position_length=10+(unsigned)digits;position[position_length-1]=0xf7;
  need(!native_captured(150,position,position_length)&&!lc->novation.position_valid[index],"held relative touch suppresses position feedback");
  fake_input_reset();native_push(0,0xbe,cc,0);native_push(0,0xbf,cc,65);
  need(collection_drain(&surfaces,state,&runtime)&&!lc->novation.touch[index]&&in->desires[0][field].pending,"raw release permits movement on current bank");
  input_discard(in);native_capture_count=0;
  for(unsigned pass=0;pass<8;pass++)need(native_output(lc,bank,&snapshot,1700),"released relative feedback projection");
  need(native_captured(150,position,position_length)&&lc->novation.position_valid[index],"release resumes current-bank position feedback");
 }
 bank->offset=0;bank_apply(bank,&snapshot,1700);input_discard(in);
 /* Native BE5D touch brackets movement, so same-track relative feedback
  * must continue during the gesture rather than waiting for its release. */
 input_discard(in);bank->offset=0;bank_apply(bank,&snapshot,1700);
 for(unsigned pass=0;pass<40;pass++)need(native_output(lc,bank,&snapshot,1700),"gesture feedback cache prime");
 uint32_t saved_pan=snapshot.tracks[0].fields[CF_PAN].bits,saved_revision=snapshot.tracks[0].fields[CF_PAN].revision;
 fake_input_reset();native_push(0,0xbe,93,127);native_push(0,0xbf,93,65);
 need(collection_drain(&surfaces,state,&runtime)&&lc->novation.touch[24]&&in->desires[0][CF_PAN].pending,"raw Pan gesture reaches held identity and canonical intent");
 float changed_pan=0.63f;memcpy(&snapshot.tracks[0].fields[CF_PAN].bits,&changed_pan,4);snapshot.tracks[0].fields[CF_PAN].revision+=2;
 native_capture_count=0;for(unsigned pass=0;pass<8;pass++)need(native_output(lc,bank,&snapshot,1700),"authoritative Pan changes during held gesture");
 need(native_captured(150,(unsigned char[]){0xf0,0,0x20,0x29,2,0x15,6,29,1,'8','0',0xf7},12)==1,"same-identity held relative encoder receives changed authoritative position once");
 changed_pan=81.0f/127.0f;memcpy(&snapshot.tracks[0].fields[CF_PAN].bits,&changed_pan,4);lc->novation.position_cursor=24;
 native_output_fail_after=1;need(!native_output(lc,bank,&snapshot,1700)&&lc->novation.position[24]==80,"failed text value send retains last delivered cache");
 native_capture_count=0;for(unsigned pass=0;pass<8;pass++){need(native_output(lc,bank,&snapshot,1700),"held changed text value retries");need(lc->novation.cycle_messages<=16&&lc->novation.cycle_bytes<=128,"text value output preserves scheduler caps");}
 need(native_captured(150,(unsigned char[]){0xf0,0,0x20,0x29,2,0x15,6,29,1,'8','1',0xf7},12)==1,"retry sends only newest authoritative text value once");
 native_capture_count=0;need(native_output(lc,bank,&snapshot,1700)&&!native_capture_count,"unchanged held text values send nothing");
 bank->offset=8;bank_apply(bank,&snapshot,1700);input_discard(in);native_capture_count=0;
 fake_input_reset();native_push(0,0xbf,93,65);need(collection_drain(&surfaces,state,&runtime)&&!in->desires[0][CF_PAN].pending,"held Pan input cannot follow bank remap");
 for(unsigned pass=0;pass<8;pass++)need(native_output(lc,bank,&snapshot,1700),"held Pan on another bank");
 need(!native_captured(150,(unsigned char[]){0xf0,0,0x20,0x29,2,0x15,6,29,1,'6','4',0xf7},12),"held relative feedback cannot follow bank remap");
 fake_input_reset();native_push(0,0xbe,93,0);need(collection_drain(&surfaces,state,&runtime)&&!lc->novation.touch[24],"raw Pan release");
 native_capture_count=0;for(unsigned pass=0;pass<8;pass++)need(native_output(lc,bank,&snapshot,1700),"released Pan settles current bank");
 need(native_captured(150,(unsigned char[]){0xf0,0,0x20,0x29,2,0x15,6,29,1,'6','4',0xf7},12)==1,"released relative encoder receives current-bank position");
 bank->offset=0;bank_apply(bank,&snapshot,1700);input_discard(in);
 uint32_t saved_volume=snapshot.tracks[0].bits;fake_input_reset();native_push(0,0xbe,5,127);need(collection_drain(&surfaces,state,&runtime)&&lc->novation.touch[0],"raw absolute fader touch");
 snapshot.tracks[0].bits=0x3f400000;native_capture_count=0;
 for(unsigned pass=0;pass<8;pass++)need(native_output(lc,bank,&snapshot,1700),"held absolute fader source changes");
 need(!native_captured(150,(unsigned char[]){0xbf,5,95},3),"held absolute fader still suppresses source position");
 fake_input_reset();native_push(0,0xbe,5,0);need(collection_drain(&surfaces,state,&runtime)&&!lc->novation.touch[0],"raw absolute fader release");
 native_capture_count=0;for(unsigned pass=0;pass<8;pass++)need(native_output(lc,bank,&snapshot,1700),"released absolute fader settles");
 need(native_captured(150,(unsigned char[]){0xbf,5,95},3)==1,"released absolute fader receives authoritative position");
 snapshot.tracks[0].bits=saved_volume;snapshot.tracks[0].fields[CF_PAN].bits=saved_pan;snapshot.tracks[0].fields[CF_PAN].revision=saved_revision;input_discard(in);
 puts("PASS raw relative gesture receives live same-track source feedback, held bank identity blocks retargeting, absolute fader remains suppressed until release (ALSA/MPC substituted)");
 lcxl_pad_mixer_checks(state,&runtime);lcxl_shift_select_checks(state,&runtime);
 lcxl_name_fairness_checks(lc,bank,&snapshot);
 lcxl_track_colour_checks(lc,bank,&snapshot);
 {
  /* Launchpad/XL3 input moves focus; the X-Touch meter demand must stay. */
  MeterInterest *m=&input.commands->meter_interest;const McuProfile *profile=surfaces.contexts[3].controller.profile;
  surfaces.contexts[3].controller.profile=&mcu_profile_xtouch;bank_apply(surfaces.banks+3,&snapshot,1700);
  for(unsigned focus=0;focus<5;focus++){
   atomic_store(&m->enabled,0);atomic_store(&m->until,0);surfaces.focus=focus;collection_interests(&surfaces,&snapshot,1700);
   need(atomic_load(&m->enabled)==1&&atomic_load(&m->count)>0&&atomic_load(m->serial)==surfaces.banks[3].strips[0].serial,"meter demand follows the connected metered surface whichever surface sent input last");
  }
  surfaces.contexts[3].controller.profile=profile;atomic_store(&m->enabled,0);atomic_store(&m->until,0);surfaces.focus=1;collection_interests(&surfaces,&snapshot,1700);
  need(!atomic_load(&m->enabled),"no metered surface connected requests no meters");
  puts("PASS meter demand stays on the metered surface across Launchpad, XL3 and HUI input focus (ALSA/MPC substituted)");
 }
 NovationEvent event;
 /* Two actual B6 1E 02 reports retained from the 718340b Control capture.
  * Subsequent relative movement is documented synthetic input, not captured. */
 fake_input_reset();native_push(0,0xb6,30,2);native_push(0,0xb6,30,2);need(collection_drain(&surfaces,state,&runtime)&&bank->assignment==BA_QLINK,"recorded Control reports request existing shared Q-Link interest once");snapshot.qlinks_available=1;snapshot.qlinks=(QLinkCopy){.epoch=snapshot.epoch,.status=QL_SAMPLED,.root=1,.wrapper=2,.provider=3,.generation=4};
 for(unsigned i=0;i<16;i++)snapshot.qlinks.slots[i]=(QLinkSlot){.sampled=1,.normalized_valid=1,.tick=1700,.bits=0x3f000000,.revision=2};
 fake_input_reset();for(unsigned cc=77;cc<85;cc++)native_push(0,0xbf,cc,65);need(collection_drain(&surfaces,state,&runtime),"BF relative Q-Link ingress");
 for(unsigned i=0;i<8;i++)need(in->qlinks[i].pending&&in->qlinks[i].request.qlink_index==i,"BF77..84 reach canonical Q-Link requests");
 /* The retained Control capture stops before movement. Use the documented
  * relative/touch pair here and substitute a fresh authoritative Q-Link update. */
 fake_input_reset();native_push(0,0xbe,77,127);native_push(0,0xbf,77,65);need(collection_drain(&surfaces,state,&runtime),"Control relative touch traverses shared ingress");
 snapshot.qlinks.slots[0].bits=0x3f400000;snapshot.qlinks.slots[0].revision++;
 native_capture_count=0;for(unsigned pass=0;pass<8;pass++)need(native_output(lc,bank,&snapshot,1700),"Control authoritative Q-Link text projection while held");
 NATIVE_HAS(150,0xf0,0,0x20,0x29,2,0x15,6,13,1,'9','5',0xf7);
 fake_input_reset();native_push(0,0xbe,77,0);need(collection_drain(&surfaces,state,&runtime),"Control relative release traverses shared ingress");
 puts("PASS recorded duplicate Control reports plus documented held relative Q-Link movement produce authoritative field1 text; ALSA/native update/render substituted");
 native_control_page(lc,bank,1);need(bank->qlink_page==1,"bounded second Q-Link page");native_control_page(lc,bank,1);need(bank->assignment==BA_EFFECT&&bank->effects_slot==0,"Control exposes existing selected effect page");
 native_capture_count=0;snapshot.effects_available=1;snapshot.effects=(EffectsCopy){.status=EF_READY,.serial=bank->selected.serial,.slot=0,.generation=2};snapshot.effects.slots[0]=(EffectSlot){.status=EF_READY,.count=1};snapshot.effects.parameters[0]=(EffectParameter){.status=EF_READY,.tick=1700,.bits=0x3f400000,.position=7};
 fake_input_reset();native_push(0,0xbf,77,63);need(collection_drain(&surfaces,state,&runtime),"BF relative effect ingress");need(in->effects[0].pending,"Control relative encoder reaches existing effect intent");for(unsigned i=0;i<40;i++)need(native_output(lc,bank,&snapshot,1700),"Control effect projection");NATIVE_HAS(150,0xf0,0,0x20,0x29,2,0x15,6,13,1,'9','5',0xf7);
 snapshot.effects_available=0;native_capture_count=0;lc->novation.position_cursor=8;native_output_fail_after=2;
 need(!native_output(lc,bank,&snapshot,1700)&&lc->novation.position_valid[8]&&lc->novation.position[8]==95,"failed unavailable text clear retains delivered value for retry");
 for(unsigned i=0;i<40;i++)need(native_output(lc,bank,&snapshot,1700),"unavailable effect output");
 need(lc->novation.position_valid[8]&&lc->novation.position[8]==255,"unavailable source clears cached display value");need(native_captured(150,(unsigned char[]){0xf0,0,0x20,0x29,2,0x15,6,13,1,'-',0xf7},11)==1,"unavailable clear retries exactly once");native_capture_count=0;need(native_output(lc,bank,&snapshot,1700)&&!native_capture_count,"unavailable values clear once and then park");snapshot.effects_available=1;native_capture_count=0;for(unsigned i=0;i<8;i++)need(native_output(lc,bank,&snapshot,1700),"reacquired unchanged source output");NATIVE_HAS(150,0xf0,0,0x20,0x29,2,0x15,6,13,1,'9','5',0xf7);
 event=(NovationEvent){.kind=NOV_TOUCH,.index=0,.value=1};native_intent(lc,bank,&snapshot,&event,1700);bank->offset=8;bank_apply(bank,&snapshot,1701);input_discard(in);event=(NovationEvent){.kind=NOV_ABSOLUTE,.index=0,.value=64};native_intent(lc,bank,&snapshot,&event,1701);need(!in->desires[0][CF_VOLUME].pending,"held LCXL3 fader cannot retarget through bank remap");event=(NovationEvent){.kind=NOV_TOUCH,.index=0,.value=0};native_intent(lc,bank,&snapshot,&event,1702);
 launchpad_step_checks(state,&runtime);launchpad_pro_rebind_checks(state,&runtime);launchpad_clip_checks(state,&runtime);launchpad_clip_colour_checks(runtime.snapshot,runtime.now);launchpad_pick_checks(state,&runtime);launchpad_side_checks(runtime.snapshot,runtime.now);launchpad_pro_stop_clip_checks(state,&runtime);launchpad_layout_checks(state,&runtime);novation_play_toggle_checks(state,&runtime);
 atomic_store(&state->ready,0);shared_fixture_time(state,1950);runtime_cycle_begin(&runtime);in->play_down=1;fake_input_reset();native_push(0,0xb6,30,1);native_push(0,0xbf,5,100);need(collection_drain(&surfaces,state,&runtime)&&bank->assignment==BA_TRACK&&lc->novation.observed_mode==1&&!in->play_down,"unready MPC still follows native mode reports and clears old transport holds");
 unsigned generation=lc->novation.generation;lc->novation.held[37]=lc->novation.touch[0]=1;lc->native_pickup[0].acquired=1;unsigned retained=surfaces.banks[1].offset;
 stopping=1;collection_disconnect(&surfaces,0);stopping=0;NATIVE_HAS(150,0xf0,0,0x20,0x29,2,0x15,2,0,0xf7);need(!lc->novation.held[37]&&!lc->novation.touch[0]&&!lc->native_pickup[0].acquired&&surfaces.banks[1].offset==retained,"disconnect resets only local physical state and sends disable even at clean stop");
 need(collection_topology(&surfaces,state)&&lc->novation.generation==generation+1&&!lc->novation.led_valid[37],"reconnect has fresh generation and dirty output");
 for(unsigned i=0;i<5;i++)collection_disconnect(&surfaces,i);
 NATIVE_HAS(151,0xf0,0,0x20,0x29,2,0x0c,0x0e,0,0xf7);NATIVE_HAS(152,0xf0,0,0x20,0x29,2,0x0e,0x0e,0,0xf7);
 native_capture_enabled=0;need(!unlink(path)&&!rmdir(directory),"native persistence fixture cleanup");free(commands);free(state);
 puts("PASS production native Novation restorable-state admission (unknown stays inactive), init/query/teardown, bounded cached Programmer/DAW feedback, manual persistence, separate musical ports, generation reset, canonical controls and five-profile shared cycle; MPC source/native execution and ALSA hardware substituted");
}
