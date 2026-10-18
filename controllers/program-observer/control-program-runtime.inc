/* Profile-local interpretation only. The collection owns input, snapshot,
 * command publication/settlement and the existing bounded Q-Link demand. */
static void control_program_mode(Surface *surface,MirrorBank *bank,unsigned layer,unsigned all_qlinks){
 unsigned assignment=layer>=CP_QLINK1?BA_QLINK:BA_TRACK,page=all_qlinks?2:layer==CP_QLINK2;
 if(surface->program.layer==layer&&bank->assignment==assignment&&bank->qlink_page==page)return;
 surface->program.layer=layer;bank->assignment=assignment;bank->qlink_page=page;bank->flip=0;bank->fader_field=CF_VOLUME;
 memset(surface->native_pickup,0,sizeof(surface->native_pickup));memset(surface->program.qlinks,0,sizeof(surface->program.qlinks));input_discard(&SURFACE_VALUE(surface,input));
}
static int control_program_qlink_pickup(Surface *surface,const CopiedMirror *snapshot,unsigned index,unsigned value,uint32_t now){
 ProgramQLinkPickup *p=surface->program.qlinks+index;const QLinkSlot *slot=input_qlink_slot(snapshot,index,now);
 if(!slot){memset(p,0,sizeof(*p));return 0;}
 float f;memcpy(&f,&slot->bits,4);if(!isfinite(f)||f<0||f>1){memset(p,0,sizeof(*p));return 0;}
 const QLinkCopy *q=&snapshot->qlinks;uint32_t key[]={snapshot->epoch,snapshot->project_owner,q->root,q->wrapper,q->provider,q->generation,index};
 if(!p->valid||memcmp(p->key,key,sizeof(key))){memset(p,0,sizeof(*p));memcpy(p->key,key,sizeof(key));p->source_bits=slot->bits;p->valid=1;}
 if(p->source_bits!=slot->bits){
  MirrorInput *in=&SURFACE_VALUE(surface,input);InputQLink *g=in->qlinks+index;unsigned own=g->pending&&g->request.bits==slot->bits&&input_qlink_same(&g->request,snapshot,index);
  for(unsigned i=0;i<COMMAND_SLOTS&&!own;i++){const InputFlight *flight=in->flights+i;own=flight->flight&&flight->origin==in&&command_qlink(flight->field)&&flight->bits==slot->bits&&input_qlink_same(&flight->qlink_request,snapshot,index);}
  p->source_bits=slot->bits;if(!own)p->acquired=0;
 }
 int target=(int)(f*127.0f+0.5f),prior=p->physical;unsigned valid=p->physical_valid;p->physical=(int)value;p->physical_valid=1;
 if((int)value==target||(valid&&((prior<=target&&(int)value>=target)||(prior>=target&&(int)value<=target))))p->acquired=1;
 return p->acquired;
}
static void control_program_intent(Surface *surface,MirrorBank *bank,const CopiedMirror *snapshot,const ControlProgramEvent *event,uint32_t now){
 unsigned protocol=SURFACE_VALUE(surface,controller).profile->protocol,index=event->index,strip=index%8;
 ControlProgramState *p=&surface->program;MirrorInput *in=&SURFACE_VALUE(surface,input);
 if(event->kind==CP_TRANSPORT){
  unsigned operation=index==1?GLOBAL_STOP:index==6?GLOBAL_RECORD_ENABLE:GLOBAL_PLAY;
  input_global_add(in,snapshot,operation,0,now);return;
 }
 if(event->kind==CP_BUTTON){
  unsigned held_index=protocol==MCU_PROTOCOL_MPK249?event->bank*8+index:index,prior=p->held[held_index];p->held[held_index]=event->value!=0;if(!event->value||prior)return;
  if(protocol==MCU_PROTOCOL_MPK_MINI3||protocol==MCU_PROTOCOL_LPD8_MK2){
   if(index<2)bank_navigate(bank,index?1:-1,now);else control_program_mode(surface,bank,index-2,0);return;
  }
  if(protocol==MCU_PROTOCOL_MPK249){static const unsigned fields[]={CF_MUTE,CF_SOLO,CF_ARM};input_control(in,bank,snapshot,strip,fields[event->bank],0,1,now);return;}

 }
 unsigned field=CF_VOLUME,qlink=QLINK_SLOTS;
 if(protocol==MCU_PROTOCOL_MPK249){
  control_program_mode(surface,bank,event->bank==2?CP_QLINK1:CP_VOLUME,event->bank==2);
  if(event->bank==2)qlink=index;
  else field=event->bank==1?(index<8?CF_SEND1:CF_SEND2):(index<8?CF_VOLUME:CF_PAN);
 }else {static const unsigned fields[]={CF_VOLUME,CF_PAN,CF_SEND1,CF_SEND2};if(p->layer>=CP_QLINK1)qlink=(p->layer-CP_QLINK1)*8+index;else field=fields[p->layer];}
 if(qlink<QLINK_SLOTS){if(control_program_qlink_pickup(surface,snapshot,qlink,event->value,now))input_qlink_edit_index(in,bank,snapshot,qlink,0,(int)(event->value*16383/127),now);return;}
 const CopiedTrack *track=bank_strip_track(snapshot,bank,strip);if(field==CF_VOLUME&&track&&midi_volume(track->vptr))field=CF_MIDI_VOLUME;
 if(native_pickup(surface,snapshot,bank,index,field,(int)event->value))input_absolute_bound(in,bank,snapshot,strip,field,event->value,now);
}
static int control_program_consume(Surface *surface,MirrorBank *bank,const MirrorState *state,ControllerRuntime *runtime,const snd_seq_event_t *event,int discard){
 if(discard||!address_equal(event->source,surface->full))return 1;
 unsigned protocol=SURFACE_VALUE(surface,controller).profile->protocol;ControlProgramEvent decoded;
 if(event->type==SND_SEQ_EVENT_SYSEX){if(!control_program_mmc(protocol,event->data.ext.ptr,event->data.ext.len,&decoded))return 1;}
 else if(event->type==SND_SEQ_EVENT_CONTROLLER){
  if(event->data.control.channel>15||event->data.control.param>127||event->data.control.value<0||event->data.control.value>127)return 1;
  if(!control_program_decode(protocol,0xb0|event->data.control.channel,event->data.control.param,(unsigned)event->data.control.value,&decoded))return 1;
 }else return 1;
 if(!runtime)return 1;
 int copied=runtime_snapshot(runtime,state,0);if(copied<0)return 0;
 if(!copied||!runtime->snapshot->ready||atomic_load(&SURFACE_VALUE(surface,input).commands->new_project_intent)){
  input_discard(&SURFACE_VALUE(surface,input));input_jog_discard(&SURFACE_VALUE(surface,input));memset(surface->native_pickup,0,sizeof(surface->native_pickup));memset(surface->program.qlinks,0,sizeof(surface->program.qlinks));
  if(decoded.kind==CP_BUTTON){unsigned at=SURFACE_VALUE(surface,controller).profile->protocol==MCU_PROTOCOL_MPK249?decoded.bank*8+decoded.index:decoded.index;surface->program.held[at]=decoded.value!=0;}return 1;
 }
 bank_apply(bank,runtime->snapshot,runtime->now);input_sync(&SURFACE_VALUE(surface,input),bank);control_program_intent(surface,bank,runtime->snapshot,&decoded,runtime->now);return 1;
}
