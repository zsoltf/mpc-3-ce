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
 MirrorInput *pro=&surfaces.contexts[2].input;NovationState *n=&surfaces.surfaces[2].novation;
 fake_input_reset();native_push(2,0x90,81,127);native_push(2,0x90,81,0);need(collection_drain(&surfaces,state,runtime)&&pro->step_count==1,"old Pro bound target has a real queued pad edit");
 unsigned generation=n->step_bind_generation;fake_input_reset();native_push(2,0xb0,97,127);native_push(2,0xb0,97,127);native_push(2,0xb0,97,0);
 need(collection_drain(&surfaces,state,runtime)&&n->layer==NOV_STEP64&&n->step_bind_generation==generation+1&&n->step_pad==UINT32_MAX&&!n->step_first&&!pro->step_count,"one Sequencer press rebinds even in64, resets sound/time and cancels old unsent edit; held repeat has no second generation");
 fake_input_reset();native_push(2,0x90,81,127);native_push(2,0x90,81,0);native_push(1,0x90,81,127);native_push(1,0x90,81,0);need(collection_drain(&surfaces,state,runtime)&&!pro->step_count&&surfaces.contexts[1].input.step_count==1,"old Pro snapshot refused while independent X bound edit remains usable");
 StepProjection *v=s->steps.views+2;v->target.bind_generation=n->step_bind_generation;v->target.track_owner=9;v->target.program_owner=9;v->target.event_owner=29;v->pad=3;v->first_step=0;
 fake_input_reset();native_push(2,0x90,81,127);native_push(2,0x90,81,0);need(collection_drain(&surfaces,state,runtime)&&pro->step_count==1&&pro->step_events[pro->step_head].request.track_owner==9,"fresh generation can edit newly selected native target");
 n->step_pad=3;n->step_first=16;generation=n->step_bind_generation;fake_input_reset();native_push(2,0xb0,79,127);native_push(2,0xb0,79,0);need(collection_drain(&surfaces,state,runtime)&&n->layer==NOV_STEP16&&n->step_bind_generation==generation&&n->step_pad==3&&n->step_first==16,"Steps changes view only and retains target/sound/time");
 fake_input_reset();native_push(2,0xb0,97,127);native_push(2,0xb0,97,0);need(collection_drain(&surfaces,state,runtime)&&n->layer==NOV_STEP64&&n->step_bind_generation==generation+1&&n->step_pad==UINT32_MAX&&!n->step_first,"Sequencer from16 also resets and rebinds64");
 fake_input_reset();native_push(1,0xb0,96,127);native_push(1,0xb0,96,0);need(collection_drain(&surfaces,state,runtime)&&surfaces.surfaces[1].novation.step_bind_generation==21&&surfaces.surfaces[1].novation.step_pad==6&&surfaces.surfaces[1].novation.step_first==32,"X Note retains existing semantics and independent binding");
 for(unsigned i=1;i<=2;i++){input_discard(&surfaces.contexts[i].input);surfaces.surfaces[i].novation=old[i-1];}s->steps=saved;s->steps_available=available;
 puts("PASS actual Pro Sequencer repeated-entry rebind, held edge, old-target refusal, fresh target edit, peer independence, Steps preservation and unchanged X Note semantics; native snapshots/ALSA substituted");
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
 MirrorInput *x=&surfaces.contexts[1].input,*pro=&surfaces.contexts[2].input;need(x->step_count==1&&pro->step_count==1,"held clip pad has one action");
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
  need(in->step_count==1&&!in->global_count&&!in->edge_count,"grid has exactly one clip action and no generic history/action leakage");
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
 NATIVE_HAS(151,0x91,81,5);NATIVE_HAS(151,0x92,82,5);NATIVE_HAS(151,0x90,83,21);NATIVE_HAS(151,0xb1,89,21);
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
 fake_input_reset();for(unsigned i=1;i<=2;i++){native_push(i,0xb0,98,127);native_push(i,0xb0,i==1?95:93,127);native_push(i,0xb0,i==1?95:93,0);native_push(i,0xb0,98,0);}need(collection_drain(&surfaces,state,&runtime)&&surfaces.surfaces[1].novation.layer==NOV_SESSION&&surfaces.surfaces[2].novation.layer==NOV_SESSION,"documented modifier+Session exposes mixer on both models");
 for(unsigned pass=0;pass<20;pass++)for(unsigned i=1;i<=2;i++)need(native_output(surfaces.surfaces+i,surfaces.banks+i,&snapshot,runtime.now),"mixer repaint after clip default");
 NATIVE_HAS(151,0xb0,59,3);NATIVE_HAS(152,0xb0,59,3);
 lcxl_readback_checks(surfaces.surfaces,state,&runtime);native_restore_checks(surfaces.surfaces+1);native_capture_count=0;
 for(unsigned layer=NOV_VOLUME;layer<=NOV_DEVICE;layer++){
  native_layer(surfaces.surfaces+1,surfaces.banks+1,layer);
  for(unsigned i=0;i<16;i++)need(native_output(surfaces.surfaces+1,surfaces.banks+1,&snapshot,runtime.now),"Programmer layer bounded grid repaint");
  for(unsigned i=0;i<native_capture_count;i++)need(native_capture[i].bytes[0]!=0xf0,"layer switch never sends a hardware DAW fader/layout command");
  native_capture_count=0;native_layer(surfaces.surfaces+1,surfaces.banks+1,NOV_SESSION);for(unsigned i=0;i<16;i++)need(native_output(surfaces.surfaces+1,surfaces.banks+1,&snapshot,runtime.now),"Session layer bounded repaint");
  unsigned pads=0;for(unsigned i=0;i<native_capture_count;i++){const unsigned char *b=native_capture[i].bytes;if(native_capture[i].length==3&&(b[0]&0xf0)==0x90&&b[1]>=11&&b[1]<=88&&b[1]%10>=1&&b[1]%10<=8)pads++;}need(pads==64,"all Session cells repaint within bounded visits");native_capture_count=0;
 }
 fake_input_reset();native_push(1,0xb0,89,127);need(collection_drain(&surfaces,state,&runtime)&&surfaces.surfaces[1].novation.layer==NOV_VOLUME&&surfaces.surfaces[2].novation.layer==NOV_SESSION,"Programmer layer is per instance");
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
 for(unsigned instance=1;instance<=2;instance++){
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
 puts("PASS documented64-pad Session map on both Programmer profiles through selected ALSA ingress; native state/device mode/musical Track routing substituted");
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
 lcxl_pad_mixer_checks(state,&runtime);
 lcxl_name_fairness_checks(lc,bank,&snapshot);
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
 launchpad_step_checks(state,&runtime);launchpad_pro_rebind_checks(state,&runtime);launchpad_clip_checks(state,&runtime);novation_play_toggle_checks(state,&runtime);
 atomic_store(&state->ready,0);shared_fixture_time(state,1950);runtime_cycle_begin(&runtime);in->play_down=1;fake_input_reset();native_push(0,0xb6,30,1);native_push(0,0xbf,5,100);need(collection_drain(&surfaces,state,&runtime)&&bank->assignment==BA_TRACK&&lc->novation.observed_mode==1&&!in->play_down,"unready MPC still follows native mode reports and clears old transport holds");
 unsigned generation=lc->novation.generation;lc->novation.held[37]=lc->novation.touch[0]=1;lc->native_pickup[0].acquired=1;unsigned retained=surfaces.banks[1].offset;
 stopping=1;collection_disconnect(&surfaces,0);stopping=0;NATIVE_HAS(150,0xf0,0,0x20,0x29,2,0x15,2,0,0xf7);need(!lc->novation.held[37]&&!lc->novation.touch[0]&&!lc->native_pickup[0].acquired&&surfaces.banks[1].offset==retained,"disconnect resets only local physical state and sends disable even at clean stop");
 need(collection_topology(&surfaces,state)&&lc->novation.generation==generation+1&&!lc->novation.led_valid[37],"reconnect has fresh generation and dirty output");
 for(unsigned i=0;i<5;i++)collection_disconnect(&surfaces,i);
 NATIVE_HAS(151,0xf0,0,0x20,0x29,2,0x0c,0x0e,0,0xf7);NATIVE_HAS(152,0xf0,0,0x20,0x29,2,0x0e,0x0e,0,0xf7);
 native_capture_enabled=0;need(!unlink(path)&&!rmdir(directory),"native persistence fixture cleanup");free(commands);free(state);
 puts("PASS production native Novation restorable-state admission (unknown stays inactive), init/query/teardown, bounded cached Programmer/DAW feedback, manual persistence, separate musical ports, generation reset, canonical controls and five-profile shared cycle; MPC source/native execution and ALSA hardware substituted");
}
