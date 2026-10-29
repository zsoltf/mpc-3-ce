/* Actual native codec -> shared ingress -> existing primitive command queue.
 * Hardware endpoints and native state are substituted, never device proof. */
static void sampler_novation_checks(const MirrorState *state,ControllerRuntime *runtime){
#ifdef MODEL_FIRMWARE_310
 CopiedMirror *s=runtime->snapshot;SamplerCopy old_sampler=s->sampler;unsigned old_available=s->sampler_available,old_owner=s->editor_owner,old_ready=s->ready;
 MIRROR_FIELD(old_record);old_record=s->record_mode;s->record_mode=(CopiedField){.available=1,.owner_incarnation=778,.incarnation=779};
 s->editor_owner=777;s->ready=1;s->sampler_available=1;
 NovationState saved[3];for(unsigned i=0;i<3;i++)saved[i]=surfaces.surfaces[i].novation;
 const unsigned modifiers[]={63,98,90},records[]={118,19,10},plays[]={116,39,20},stops[]={0,29,8};
 for(unsigned i=0;i<3;i++){
  Surface *surface=surfaces.surfaces+i;NovationState *n=&surface->novation;MirrorInput *in=&surfaces.contexts[i].input;
  memset(n->held,0,sizeof(n->held));memset(n->sampler_held,0,sizeof(n->sampler_held));n->shift=0;n->layer=NOV_VOLUME;n->observed_mode=1;
  for(unsigned phase=0;phase<3;phase++){
   input_jog_discard(in);s->sampler.state=phase==0?0:phase==1?1:3;
   unsigned key=i?phase==0?records[i]:phase==1?plays[i]:stops[i]:records[i];
   fake_input_reset();native_push(i,i?0xb0:0xb6,modifiers[i],127);native_push(i,0xb0,key,127);native_push(i,0xb0,key,127);
   native_push(i,i?0xb0:0xb6,modifiers[i],0);native_push(i,0xb0,key,127);native_push(i,0xb0,key,0);
   need(collection_drain(&surfaces,state,runtime)&&in->global_count==1&&in->global_events[in->global_head].operation==(phase==0?SAMPLER_ARM:phase==1?SAMPLER_START:SAMPLER_STOP),"native sampler codec routes exactly one Arm/Start/Stop despite repeats and modifier-first release");
   need(!in->play_down&&!in->stop_down&&!in->stop_home&&!n->sampler_held[key]&&!n->held[key],"sampler chord releases without sequencer or clip-stop hold");
  }
  /* A pre-existing ordinary press keeps its original release owner. */
  input_jog_discard(in);fake_input_reset();native_push(i,0xb0,records[i],127);native_push(i,i?0xb0:0xb6,modifiers[i],127);native_push(i,0xb0,records[i],127);native_push(i,0xb0,records[i],0);native_push(i,i?0xb0:0xb6,modifiers[i],0);
  need(collection_drain(&surfaces,state,runtime)&&in->global_count==1&&in->global_events[in->global_head].operation==GLOBAL_RECORD_TOGGLE,"ordinary Record remains sequencer Record when modifier is added after press");
  input_jog_discard(in);fake_input_reset();native_push(i,i?0xb0:0xb6,modifiers[i],127);need(collection_drain(&surfaces,state,runtime),"sampler modifier down");
  s->ready=0;fake_input_reset();native_push(i,0xb0,records[i],127);need(collection_drain(&surfaces,state,runtime),"unready sampler press consumed");
  s->ready=1;fake_input_reset();native_push(i,0xb0,records[i],127);native_push(i,i?0xb0:0xb6,modifiers[i],0);native_push(i,0xb0,records[i],0);
  need(collection_drain(&surfaces,state,runtime)&&!in->global_count&&!n->sampler_held[records[i]],"readiness during an owned hold cannot create a latent sampler or sequencer action");
  /* Exact output packets encode copied native state, with no prediction. */
  fake_input_reset();native_push(i,i?0xb0:0xb6,modifiers[i],127);need(collection_drain(&surfaces,state,runtime),"sampler LED modifier");
  for(unsigned native=0;native<=6;native++){
   s->sampler.state=native%6;s->sampler_available=native<6;native_capture_count=0;memset(n->led_valid,0,sizeof(n->led_valid));
   for(unsigned pass=0;pass<24;pass++)need(native_output(surface,surfaces.banks+i,s,runtime->now),"bounded native sampler LED output");
   unsigned running=native==3||native==5,armed=native==1||native==2;
   unsigned red=running||armed?5:!i&&native==0?3:0;
   NATIVE_HAS(150+i,running?0xb2:0xb0,records[i],red);NATIVE_HAS(150+i,0xb0,plays[i],running?21:0);
   if(i)NATIVE_HAS(150+i,0xb0,stops[i],native==0?5:0);
  }
  s->sampler_available=1;
  if(!i){
   for(unsigned unavailable=0;unavailable<2;unavailable++){
    input_jog_discard(in);s->sampler.state=4;s->sampler_available=unavailable;
    fake_input_reset();native_push(i,0xb0,118,127);native_push(i,0xb0,118,0);need(collection_drain(&surfaces,state,runtime)&&!in->global_count,"XL3 refuses unavailable or finalizing state without guessing a phase");
   }
   s->sampler_available=1;
  }
  fake_input_reset();native_push(i,i?0xb0:0xb6,modifiers[i],0);need(collection_drain(&surfaces,state,runtime),"sampler modifier released");input_jog_discard(in);
 }
 /* Capture MIDI retains Session/Step ownership on X. */
 for(unsigned layer=NOV_CLIPS;layer<=NOV_STEP16;layer++)if(layer==NOV_CLIPS||novation_step_view(layer)){
  NovationState *n=&surfaces.surfaces[1].novation;n->layer=layer;MirrorInput *in=&surfaces.contexts[1].input;input_jog_discard(in);
  fake_input_reset();native_push(1,0xb0,98,127);native_push(1,0xb0,19,127);native_push(1,0xb0,19,0);native_push(1,0xb0,98,0);need(collection_drain(&surfaces,state,runtime)&&!n->sampler_held[19],"X Capture sampler chord is absent from Session and Step");
  for(unsigned e=0;e<in->global_count;e++)need(!command_sampler(in->global_events[(in->global_head+e)%INPUT_JOG_EVENTS].operation),"Session/Step Capture does not enqueue sampler primitives");
 }
 for(unsigned i=0;i<3;i++){surfaces.surfaces[i].novation=saved[i];input_jog_discard(&surfaces.contexts[i].input);input_discard(&surfaces.contexts[i].input);}
 s->sampler=old_sampler;s->sampler_available=old_available;s->editor_owner=old_owner;s->ready=old_ready;s->record_mode=old_record;
 puts("PASS Pro Shift transport, X mixer-only Capture transport, XL3 Shift Record native phases: actual codecs, repeat/release ownership, ordinary Record, unavailable/finalizing refusal and exact native LED packets (native state and ALSA hardware substituted)");
#else
 (void)state;(void)runtime;
#endif
}
