/* Per-surface codecs/projection only. Called by the one collection scheduler. */
static int native_wire(Surface *surface,const NovationWire *wire,int teardown){
 NovationState *n=&surface->novation;if(!teardown&&!novation_fits(n,wire))return 2;
 snd_seq_event_t event;snd_seq_ev_clear(&event);unsigned status=wire->bytes[0];
 if(status==0xf0)snd_seq_ev_set_sysex(&event,wire->length,(void*)wire->bytes);
 else if((status&0xf0)==0xb0)snd_seq_ev_set_controller(&event,status&15,wire->bytes[1],wire->bytes[2]);
 else if((status&0xf0)==0x90)snd_seq_ev_set_noteon(&event,status&15,wire->bytes[1],wire->bytes[2]);
 else return 0;
 /* Clean stop must send the documented disable even after SIGTERM. This
  * bypasses only channel_send's stopping gate, never the configured endpoint. */
 int ok;
 if(teardown){snd_seq_addr_t out=surface_output_address(surface);snd_seq_ev_set_source(&event,surface->source);snd_seq_ev_set_dest(&event,out.client,out.port);snd_seq_ev_set_direct(&event);ok=snd_seq_event_output_direct(surface->seq,&event)>=0;surfaces.encoded++;surfaces.sent+=ok;if(surface->context){surface->context->encoded++;surface->context->sent+=ok;}}
 else ok=channel_send(surface,&event);
 if(ok){n->cycle_messages++;n->cycle_bytes+=wire->length;}return ok;
}
static void native_teardown(Surface *surface){
 NovationState *n=&surface->novation;if(!n->teardown_pending)return;
 novation_budget(n);int ok;
 if(n->protocol==MCU_PROTOCOL_LCXL3){NovationWire wire=novation_teardown(n);ok=native_wire(surface,&wire,1)==1;}
 else {NovationWire mode=novation_command(n->protocol,0x0e,(int)n->pre_mode),layout=novation_layout_value(n->protocol,n->pre_layout,n->pre_page);
  ok=native_wire(surface,&mode,1)==1&&native_wire(surface,&layout,1)==1;
 }
 if(ok)n->teardown_pending=0;
 else BRIDGE_LOG("TEARDOWN_UNCONFIRMED profile=%s generation=%u; mode/layout restoration could not be delivered\n",SURFACE_VALUE(surface,controller).profile->name,n->generation);
 /* Physical unplug can make delivery impossible. LCXL3 has no DAW query:
  * its documented clean exit remains Standalone, never exact restoration. */
}
/* Documented feature readback settles initialization. One correction follows
 * an explicit Off report; missing replies never trigger recurring MIDI work. */
static int native_lcxl_admission(Surface *surface,uint32_t now){
 NovationState *n=&surface->novation;static const unsigned cc[]={69,72,73};
 if(!n->lc_round){n->lc_round=1;n->lc_step=n->lc_received=n->lc_on=n->lc_queried=0;}
 for(;;){
  unsigned count=n->lc_round==1?3:6;
  while(n->lc_step<count){
   unsigned correcting=n->lc_round==2&&n->lc_step<3,index=correcting?n->lc_step:n->lc_step-(n->lc_round==2?3:0);
   if(correcting&&!(n->lc_correction&(1u<<index))){n->lc_step++;continue;}
   NovationWire wire=novation_cc(correcting?6:7,cc[index],127);int sent=native_wire(surface,&wire,0);
   if(sent!=1)return sent==2?0:-1;
   if(!correcting)n->lc_queried|=1u<<index;
   n->lc_step++;if(n->lc_step==count)n->query_tick=now;
  }
  if(n->lc_received==7){
   if(n->lc_on==7)return 1;
   if(n->lc_round==1){n->lc_correction=7u&~n->lc_on;n->lc_round=2;n->lc_step=n->lc_received=n->lc_on=n->lc_queried=0;continue;}
  }
  if(!n->admission_reported&&now>=n->query_tick&&now-n->query_tick>=250){
   BRIDGE_LOG("NATIVE_INACTIVE profile=%s generation=%u; relative encoder mode is unverified received=%u on=%u\n",SURFACE_VALUE(surface,controller).profile->name,n->generation,n->lc_received,n->lc_on);n->admission_reported=1;
  }
  return 0;
 }
}
static int native_initialise(Surface *surface,uint32_t now){
 NovationState *n=&surface->novation;
 while(n->stage<16){
  NovationWire wire={0};
  if(n->protocol==MCU_PROTOCOL_LCXL3){
   static const unsigned controls[]={69,72,73,71,70,30};
   if(n->stage==0)wire=novation_command(n->protocol,2,127);
   else if(n->stage<=6)wire=novation_cc(6,controls[n->stage-1],n->stage==6?1:127);
   else if(n->stage<=14){wire=novation_sysex(n->protocol,4);novation_append(&wire,n->stage-7+5);novation_append(&wire,0x64);novation_append(&wire,0xf7);}
   else if(n->lc_display<24){
    wire=novation_sysex(n->protocol,4);novation_append(&wire,13+n->lc_display);novation_append(&wire,0x61);novation_append(&wire,0xf7);
    int sent=native_wire(surface,&wire,0);if(sent!=1)return sent==2?1:0;n->lc_display++;continue;
   }
   else {int ready=native_lcxl_admission(surface,now);if(ready<0)return 0;if(!ready)return 1;n->stage=16;n->observed_mode=1;break;}
  }else{
   if(n->stage==0){wire=novation_command(n->protocol,n->protocol==MCU_PROTOCOL_LPX?0x0e:0,-1);n->query_tick=now;}
   else if(n->stage==1&&n->protocol==MCU_PROTOCOL_LPX)wire=novation_command(n->protocol,0,-1);
   else if(n->stage<=2){
    if(!novation_restorable(n)){
     if(!n->admission_reported&&now>=n->query_tick&&now-n->query_tick>=250){BRIDGE_LOG("NATIVE_INACTIVE profile=%s generation=%u; prior mode/layout is unknown or cannot be restored\n",SURFACE_VALUE(surface,controller).profile->name,n->generation);n->admission_reported=1;}
     return 1;
    }
    n->stage=3;continue;
   }
   else if(n->stage==3)wire=novation_command(n->protocol,0x0e,1); /* Preserve the separate DAW flag. */
   else {n->stage=16;n->observed_mode=1;break;}
  }
  int sent=native_wire(surface,&wire,0);if(sent!=1)return sent==2?1:0;
  if(n->protocol==MCU_PROTOCOL_LCXL3||n->stage>=3)n->teardown_pending=1;
  n->stage++;
 }return 1;
}
static unsigned native_field(const NovationState *n,unsigned index){
 if(n->protocol==MCU_PROTOCOL_LCXL3)return index<8?CF_VOLUME:index<16?CF_SEND1:index<24?CF_SEND2:CF_PAN;
 unsigned layer=n->protocol==MCU_PROTOCOL_LPP3?index/8+1:n->layer;
 return layer==NOV_PAN?CF_PAN:layer==NOV_SEND?CF_SEND1:CF_VOLUME;
}
static int native_position(const CopiedMirror *snapshot,const MirrorBank *bank,unsigned strip,unsigned field,uint32_t *bits,uint32_t *incarnation){
 const CopiedTrack *track=bank_strip_track(snapshot,bank,strip);if(!bank->ready||!track)return -1;
 if(track->pad_owner&&!pad_controller(field))return -1;
 if(field==CF_VOLUME&&midi_volume(track->vptr))field=CF_MIDI_VOLUME;
 MIRROR_FIELD(volume);const CopiedField *f=copied_field(snapshot,track,command_source_field(field),&volume);
 if(!f||!f->available||!(field==CF_MIDI_VOLUME?midi_volume(track->vptr):mixable(track->vptr)))return -1;
 float value;memcpy(&value,&f->bits,4);if(!isfinite(value)||value<0||value>1)return -1;
 if(bits)*bits=f->bits;
 if(incarnation)*incarnation=f->incarnation;
 return (int)(value*127.0f+0.5f);
}
static int native_pickup(Surface *surface,const CopiedMirror *snapshot,const MirrorBank *bank,unsigned index,unsigned field,int value){
 HuiPickup *p=surface->native_pickup+index;unsigned strip=index%8;uint32_t bits,incarnation;
 int target=native_position(snapshot,bank,strip,field,&bits,&incarnation);if(target<0){memset(p,0,sizeof(*p));return 0;}
 const MotorIdentity *id=bank->strips+strip;
 if(surface->novation.touch[index]&&memcmp(&surface->native_touch_identity[index],id,sizeof(*id)))return 0;
 if(!p->source_valid||memcmp(&p->identity,id,sizeof(*id))||p->field!=field||p->field_incarnation!=incarnation){memset(p,0,sizeof(*p));p->identity=*id;p->field=field;p->field_incarnation=incarnation;p->source_bits=bits;p->source_valid=1;p->target=target;}
 if(bits!=p->source_bits){InputFlight *flight=input_existing(&SURFACE_VALUE(surface,input),id,field);InputGesture *g=&SURFACE_VALUE(surface,input).desires[strip][field];unsigned own=(flight&&flight->origin==&SURFACE_VALUE(surface,input)&&flight->bits==bits)||(g->pending&&g->bits==bits);p->source_bits=bits;p->target=target;if(!own)p->acquired=0;}
 int prior=p->physical;unsigned valid=p->physical_valid;p->physical=value;p->physical_valid=1;
 if(value==target||(valid&&((prior<=target&&value>=target)||(prior>=target&&value<=target))))p->acquired=1;
 return p->acquired;
}
static void native_layer(Surface *surface,MirrorBank *bank,unsigned layer){
 NovationState *n=&surface->novation;if(n->layer==layer)return;input_clip_feedback_clear(&SURFACE_VALUE(surface,input));n->layer=layer;n->layout_dirty=0;n->copy_source=(ClipEditSource){0};n->copy_used=1;memset(n->led_valid,0,sizeof(n->led_valid));
 memset(n->position_valid,0,sizeof(n->position_valid));memset(n->text_valid,0,sizeof(n->text_valid));memset(surface->native_pickup,0,sizeof(surface->native_pickup));
 memset(n->fader_color_valid,0,sizeof(n->fader_color_valid));
 bank->assignment=layer==NOV_DEVICE?BA_QLINK:BA_TRACK;bank->flip=0;bank->fader_field=CF_VOLUME;
}
static void native_mixer_view(Surface *surface,MirrorBank *bank,uint32_t now){
 unsigned pads=bank->view==BV_DRUM_PADS;if(!pads)surface->novation.track_offset=bank->offset;
 bank_view(bank,pads?BV_TRACK:BV_DRUM_PADS,now);if(pads)bank->offset=surface->novation.track_offset;
 bank->assignment=BA_TRACK;bank->flip=0;bank->fader_field=CF_VOLUME;
 bank_drop(bank,now);input_discard(&SURFACE_VALUE(surface,input));
 /* Keep touch identities: a held physical gesture must not acquire the new view. */
 memset(surface->native_pickup,0,sizeof(surface->native_pickup));
 NovationState *n=&surface->novation;
 memset(n->led_valid,0,sizeof(n->led_valid));memset(n->position_valid,0,sizeof(n->position_valid));memset(n->text_valid,0,sizeof(n->text_valid));
}
/* CE colours identify bound drum slots; they do not mirror MPC custom colours.
 * Off and the moving playhead retain their separate values 0 and 45. */
static unsigned native_drum_color(unsigned pad){
 static const unsigned char palette[]={5,9,13,17,21,25,29,33,37,41,49,53,57,61,3,60};
 return palette[pad%16];
}
/* XL3 DAW Mixer, Shift + a lower-row button: the track in that column of the
 * Track mixer bank (kept while the Pad mixer is shown). */
static const CopiedTrack *native_shift_track(const NovationState *n,const MirrorBank *bank,const CopiedMirror *snapshot,unsigned strip){
 unsigned index=(bank->view==BV_DRUM_PADS?n->track_offset:bank->offset)+strip,at=0;
 for(unsigned i=0;i<snapshot->count;i++)if(bank_role(BV_TRACK,snapshot->tracks[i].vptr)){if(at==index)return snapshot->tracks+i;at++;}
 return NULL;
}
/* Shift + Select selects that MPC track with the existing select command; a
 * Drum track opens (or stays in) the Pad mixer, which follows the selected
 * Drum program, and any other track returns to the Track mixer. The mixer
 * changes once MPC reports the new selection (or after one second), so the
 * select request is not discarded with the old strips and the Pad mixer
 * opens on the new track's pads. */
static void native_shift_settle(Surface *surface,MirrorBank *bank,const CopiedMirror *snapshot,uint32_t now){
 NovationState *n=&surface->novation;if(!n->shift_view)return;
 unsigned selected=snapshot->selection.available&&snapshot->selection.bits==n->shift_track;
 if(!selected&&now>=n->shift_tick&&now-n->shift_tick<1000)return;
 if((n->shift_view==1)!=(bank->view==BV_DRUM_PADS))native_mixer_view(surface,bank,now);
 n->shift_view=0;
}
static void native_shift_select(Surface *surface,MirrorBank *bank,const CopiedMirror *snapshot,unsigned strip,uint32_t now){
 NovationState *n=&surface->novation;const CopiedTrack *t=native_shift_track(n,bank,snapshot,strip);if(!t)return;
 MotorIdentity id={snapshot->epoch,t->serial,t->binding,t->incarnation,t->track,t->program,t->track_owner,t->program_owner,0,0,0};
 if(!snapshot->selection.available||snapshot->selection.bits!=t->track)input_control_bound(&SURFACE_VALUE(surface,input),bank,snapshot,strip,CF_SELECTION,0,1,now,&id,2);
 n->shift_view=t->vptr==MODEL_DRUMPROGRAM_VPTR?1:2;n->shift_track=t->track;n->shift_tick=now;
 native_shift_settle(surface,bank,snapshot,now);
}
static void native_control_page(Surface *surface,MirrorBank *bank,int direction){
 NovationState *n=&surface->novation;unsigned next=(unsigned)((int)n->control_page+direction+6)%6;n->control_page=next;
 bank->assignment=next<2?BA_QLINK:BA_EFFECT;bank->qlink_page=next<2?next:0;bank->effects_slot=next>=2?next-2:EFFECT_LIST;bank->effects_page=0;
 memset(n->position_valid,0,sizeof(n->position_valid));memset(n->text_valid,0,sizeof(n->text_valid));
 input_discard(&SURFACE_VALUE(surface,input));
}
static unsigned native_step_slot(const Surface *surface){
 uintptr_t p=(uintptr_t)surface->context,base=(uintptr_t)surfaces.contexts;
 return p>=base&&p<base+sizeof(surfaces.contexts)?(unsigned)((p-base)/sizeof(SurfaceContext)):0;
}
static const StepProjection *native_step_source(const Surface *surface,const CopiedMirror *snapshot,uint32_t now){
 const NovationState *n=&surface->novation;const StepCopy *s=&snapshot->steps;
 if(!snapshot->steps_available||s->epoch!=snapshot->epoch)return NULL;
 const StepProjection *v=s->views+native_step_slot(surface);
 if(v->status!=STEP_READY||(v->target.event_owner&&v->target.bind_generation!=n->step_bind_generation)||(!v->target.event_owner&&v->generation!=s->event_generation)||v->tick>now||now-v->tick>=STEP_FRESH_MS||v->first_step!=n->step_first||(n->step_pad!=UINT32_MAX&&v->pad!=n->step_pad))return NULL;
 return v;
}
static const ClipProjection *native_clip_source(const Surface *surface,const CopiedMirror *snapshot,uint32_t now){
 const NovationState *n=&surface->novation;if(!snapshot->clips_available||snapshot->clips.epoch!=snapshot->epoch)return NULL;
 const ClipProjection *v=snapshot->clips.views+native_step_slot(surface);
 return v->status==CLIP_SOURCE_READY&&v->tick<=now&&now-v->tick<CLIP_FRESH_MS&&v->first_column==n->clip_column&&v->first_row==n->clip_row?v:NULL;
}
static void native_copy_validate(NovationState *n,const CopiedMirror *snapshot){
 ClipEditSource *source=&n->copy_source;if(!source->kind)return;
 if(n->layer!=NOV_CLIPS||!snapshot->ready||source->epoch!=snapshot->epoch||source->project!=snapshot->project_owner){*source=(ClipEditSource){0};n->copy_used=1;return;}
 /* A publication miss cannot invalidate the retained native source identity. */
 if(!snapshot->clips_available)return;
 if(source->project!=snapshot->clips.project_owner||source->lanes!=snapshot->clips.lanes||source->columns!=snapshot->clips.columns||source->row>=snapshot->clips.rows){*source=(ClipEditSource){0};n->copy_used=1;return;}
 /* If the source is still on this page, detect removal/replacement promptly.
  * Off-page sources are resolved again by the native command consumer. */
 const ClipProjection *v=NULL;
 for(unsigned i=0;i<CLIP_VIEWS;i++){const ClipProjection *p=snapshot->clips.views+i;if(p->status!=CLIP_SOURCE_READY||source->row<p->first_row||source->row-p->first_row>=8)continue;
  if(source->kind==2)return;
  if(source->column>=p->first_column&&source->column-p->first_column<8){v=p;break;}}
 if(v&&(v->track_owner[source->column-v->first_column]!=source->track||v->cells[(source->row-v->first_row)*8+source->column-v->first_column].identity!=source->clip)){*source=(ClipEditSource){0};n->copy_used=1;}
}
static unsigned native_direct_edit(const NovationState *n){return n->protocol==MCU_PROTOCOL_LPP3?n->held[90]&&n->held[50]:n->held[95]&&n->held[97];}
static unsigned native_delete_held(const NovationState *n){return n->protocol==MCU_PROTOCOL_LPP3?n->held[60]:n->held[97]&&n->held[98];}
/* Direct edits consume their modifier chord without changing the clip view. */
static int native_direct_button(Surface *surface,const NovationEvent *e,unsigned prior){
 NovationState *n=&surface->novation;unsigned pro=n->protocol==MCU_PROTOCOL_LPP3,index=e->index;
 if(n->layer!=NOV_CLIPS||native_delete_held(n)||!native_direct_edit(n))return 0;
 n->copy_source=(ClipEditSource){0};n->copy_used=n->pick_used=1;
 return e->value&&!prior&&(index==(pro?90u:95u));
}
static int native_clip_edit_intent(Surface *surface,const CopiedMirror *snapshot,const NovationClipEvent *e,uint32_t now){
 NovationState *n=&surface->novation;unsigned pro=n->protocol==MCU_PROTOCOL_LPP3;
 unsigned deleting=pro?n->held[60]:(n->held[97]&&n->held[98]);
 unsigned copying=pro?n->held[50]:n->held[97];
 unsigned direct=native_direct_edit(n),quantize=pro&&n->held[40];
 if(!deleting&&!copying&&!direct&&!quantize)return 0;
 n->copy_used=1;if(novation_pick_held(n->protocol,n->held))n->pick_used=1;
 if(e->kind==NOV_CLIP_COLUMN||e->kind==NOV_CLIP_ROW)return 0; /* always page, including Custom+Capture */
 unsigned whole=e->kind==NOV_SCENE_TRIGGER||e->kind==NOV_SCENE_STOP;
 unsigned pad=e->kind==NOV_CLIP_TRIGGER||e->kind==NOV_CLIP_STOP||e->kind==NOV_CLIP_RECORD||e->kind==NOV_CLIP_PICK;
 if(!whole&&!pad)return 1;
 if(deleting){n->copy_source=(ClipEditSource){0};if((pad||whole)&&native_clip_source(surface,snapshot,now))input_clip_add(&SURFACE_VALUE(surface,input),snapshot,native_step_slot(surface),whole?CLIP_DELETE_ROW:CLIP_DELETE,e->column,e->row,now);return 1;}
 if(quantize){n->copy_source=(ClipEditSource){0};if(pad&&native_clip_source(surface,snapshot,now))input_clip_add(&SURFACE_VALUE(surface,input),snapshot,native_step_slot(surface),CLIP_QUANTIZE,e->column,e->row,now);return 1;}
 if(direct){n->copy_source=(ClipEditSource){0};if(native_clip_source(surface,snapshot,now))input_clip_add(&SURFACE_VALUE(surface,input),snapshot,native_step_slot(surface),whole?CLIP_INSERT_CAPTURE:CLIP_DOUBLE_EVENTS,e->column,e->row,now);return 1;}
 const ClipProjection *v=native_clip_source(surface,snapshot,now);
 if(!v){
  /* A missed destination consumes the press, not a still-held source. */
  if(n->copy_source.kind!=(whole?2u:1u))n->copy_source=(ClipEditSource){0};
  return 1;
 }
 if(n->copy_source.kind!=(whole?2u:1u)){
  n->copy_source=(ClipEditSource){0};clip_edit_source(&n->copy_source,&snapshot->clips,native_step_slot(surface),e->column,e->row,whole,now);return 1;
 }
 input_clip_copy_add(&SURFACE_VALUE(surface,input),snapshot,native_step_slot(surface),&n->copy_source,e->column,e->row,now);return 1;
}
static int native_clip_intent(Surface *surface,MirrorBank *bank,const CopiedMirror *snapshot,const NovationEvent *event,uint32_t now){
 NovationState *n=&surface->novation;unsigned session=n->protocol==MCU_PROTOCOL_LPP3?93:95;
 /* Pro's Stop Clip is a stop modifier too, like Capture MIDI/Projects. */
 unsigned stop=n->held[98]||(n->protocol==MCU_PROTOCOL_LPP3&&n->held[8]);
 NovationClipEvent e;unsigned modifiers=(stop?1u:0u)|(n->held[session]?2u:0u)|(novation_pick_held(n->protocol,n->held)?4u:0u);
 if(!novation_clip_decode(n->protocol,n->layer,event,modifiers,&e))return 0;
 if(n->layer==NOV_CLIPS&&native_clip_edit_intent(surface,snapshot,&e,now))return 1;
 if(modifiers&4u)n->pick_used=1; /* any pad during the hold: release opens nothing */
 if((modifiers&1u)&&n->row_hold==8)n->row_used=1; /* a Stop Clip chord latches nothing */
 if(e.kind==NOV_CLIP_VIEW){native_layer(surface,bank,e.view);input_discard(&SURFACE_VALUE(surface,input));return 1;}
 if(e.kind==NOV_CLIP_COLUMN||e.kind==NOV_CLIP_ROW){
  input_clip_feedback_clear(&SURFACE_VALUE(surface,input));
  int64_t next=(int64_t)(e.kind==NOV_CLIP_COLUMN?n->clip_column:n->clip_row)+e.direction*8;
  unsigned limit=e.kind==NOV_CLIP_COLUMN?snapshot->clips.columns:snapshot->clips.rows;
  if(snapshot->clips_available&&next>=0&&next<limit){if(e.kind==NOV_CLIP_COLUMN)n->clip_column=(unsigned)next;else n->clip_row=(unsigned)next;}
  /* Paging cannot cancel a paste already captured at destination press. */
  MirrorInput *pending_input=&SURFACE_VALUE(surface,input);unsigned editing=0;
  for(unsigned i=0;i<pending_input->step_count;i++){const CommandRequest *r=&pending_input->step_events[(pending_input->step_head+i)%INPUT_JOG_EVENTS].request;if(command_clip(r->reserved)&&clip_edit_intent(r->bits)){editing=1;break;}}
  if(!n->copy_held&&!editing)input_discard(pending_input);
  return 1;
 }
 if(native_clip_source(surface,snapshot,now)){
  if(e.kind==NOV_CLIP_PICK){
   /* Select natively without launching; this Launchpad opens its step view
    * once that request has been processed, so the bind sees the selection. */
   input_clip_add(&SURFACE_VALUE(surface,input),snapshot,native_step_slot(surface),CLIP_SELECT,e.column,e.row,now);
   n->pick_used=1;n->pick_pending=1;n->pick_tick=now;return 1;
  }
  unsigned state=e.kind==NOV_CLIP_TRIGGER&&e.row<8&&e.column<8?native_clip_source(surface,snapshot,now)->cells[e.row*8+e.column].state:CLIP_NO_SLOT;
  unsigned intent=e.kind==NOV_CLIP_TRIGGER?CLIP_LAUNCH:e.kind==NOV_CLIP_STOP?CLIP_STOP:e.kind==NOV_CLIP_RECORD?CLIP_RECORD_TOGGLE:e.kind==NOV_SCENE_TRIGGER?CLIP_SCENE_LAUNCH:CLIP_SCENE_STOP;
  input_clip_add(&SURFACE_VALUE(surface,input),snapshot,native_step_slot(surface),intent,e.column,e.row,now);
  /* Launching a clip also selects it natively (after the trigger, so launch
   * timing is unchanged): the MPC selection and the next step-view entry on
   * either Launchpad show it. An empty slot only stops its track. */
  if(e.kind==NOV_CLIP_TRIGGER&&state>=CLIP_READY&&state<=CLIP_WILL_STOP_OVERDUBBING)input_clip_add(&SURFACE_VALUE(surface,input),snapshot,native_step_slot(surface),CLIP_SELECT,e.column,e.row,now);
 }
 return 1;
}
/* The column's MPC track colour as a palette entry, or 0. The nearest entry is
 * recomputed only when that colour changes. */
static unsigned native_column_colour(NovationState *n,const CopiedMirror *snapshot,const ClipProjection *v,unsigned column){
 if(!v->track_owner[column])return 0;
 for(unsigned i=0;i<snapshot->count;i++){const CopiedTrack *t=snapshot->tracks+i;
  if(t->track_owner!=v->track_owner[column])continue;
  uint32_t rgb=t->available&&t->fields[CF_COLOR].available?novation_track_rgb(t->fields[CF_COLOR].bits):0;if(!rgb)return 0;
  if(n->column_rgb[column]!=rgb){n->column_rgb[column]=rgb;n->column_palette[column]=(unsigned char)novation_palette_nearest(rgb);}
  return n->column_palette[column];
 }
 return 0;
}
/* Clip cells, including the Arrangement row, follow the MPC clip launcher:
 * empty off, content in its track colour, playing (the active lane) white.
 * Queued launch flashes white against the track colour, playing pulses white,
 * queued stop flashes white against dark. Record pulses red and overdub amber;
 * their queued changes flash, so no record state is steady. Scene buttons
 * (cells=0) keep the earlier colours, except that empty scenes are off too. colour is a palette entry, 0 for none. */
static void native_clip_color(unsigned state,unsigned colour,unsigned cells,unsigned char *value,unsigned char *style){
 unsigned content=colour?colour:45;
 *style=0;
 switch(state){
 case CLIP_NO_SLOT:*value=0;break;
 case CLIP_EMPTY:*value=0;break;
 case CLIP_READY:*value=cells?content:45;break;
 case CLIP_WILL_PLAY:if(cells){*value=3;*style=NOV_FLASH_FROM|content;}else{*value=21;*style=1;}break;
 case CLIP_PLAYING:if(cells){*value=3;*style=2;}else *value=21;break;
 case CLIP_WILL_STOP_PLAYING:if(cells){*value=3;*style=NOV_FLASH_FROM;}else{*value=9;*style=1;}break;
 case CLIP_WILL_RECORD:case CLIP_WILL_STOP_RECORDING:*value=5;*style=1;break;
 case CLIP_RECORDING:*value=5;*style=2;break;
 case CLIP_WILL_OVERDUB:case CLIP_WILL_STOP_OVERDUBBING:*value=9;*style=1;break;
 case CLIP_OVERDUBBING:*value=9;*style=2;break;
 default:*value=0;break;
 }
}
/* Entering a step view from outside binds the MPC's current selection. */
static void native_enter_step(Surface *surface,MirrorBank *bank,unsigned view){
 NovationState *n=&surface->novation;n->step_mode=view;
 n->step_bind_generation++;if(!n->step_bind_generation)n->step_bind_generation=1;n->step_pad=UINT32_MAX;n->step_first=0;
 native_layer(surface,bank,view);input_discard(&SURFACE_VALUE(surface,input));
}
/* In the clip grid the step-view button (X Note, Pro Sequencer) acts on
 * release: alone it opens the view as before; with a pad it is the pick hold. */
static int native_pick_button(Surface *surface,MirrorBank *bank,const NovationEvent *event,unsigned prior){
 NovationState *n=&surface->novation;
 if(n->layer!=NOV_CLIPS||!novation_pick_button(n->protocol,event->index))return 0;
 if(event->value){if(!prior)n->pick_used=0;return 1;}
 if(!prior)return 1;
 if(!n->pick_used&&!n->pick_pending)native_enter_step(surface,bank,novation_step_entry(n));
 return 1;
}
/* After a pick, open the step view once the select request has left this
 * surface's queue and flight (or after 2 s), never before. */
static void native_pick_settle(Surface *surface,MirrorBank *bank,uint32_t now){
 NovationState *n=&surface->novation;MirrorInput *in=&SURFACE_VALUE(surface,input);if(!n->pick_pending)return;
 unsigned busy=in->step_count!=0;for(unsigned i=0;i<COMMAND_SLOTS&&!busy;i++)busy=in->flights[i].flight&&command_clip(in->flights[i].field);
 if(busy&&now>=n->pick_tick&&now-n->pick_tick<2000)return;
 n->pick_pending=0;
 if(n->layer==NOV_CLIPS)native_enter_step(surface,bank,novation_step_entry(n));
}
static int native_step_intent(Surface *surface,MirrorBank *bank,const CopiedMirror *snapshot,const NovationEvent *event,uint32_t now){
 NovationState *n=&surface->novation;NovationStepEvent e;if(!novation_step_decode(n->protocol,n->layer,event,&e))return 0;
 if(e.kind==NOV_STEP_VIEW){
  if(e.view==NOV_STEP_ENTER){native_enter_step(surface,bank,novation_step_entry(n));return 1;}
  /* Switching views keeps the binding, sound and time page. */
  if(novation_step_view(e.view))n->step_mode=e.view;
  native_layer(surface,bank,e.view);input_discard(&SURFACE_VALUE(surface,input));return 1;
 }
 const StepProjection *v=native_step_source(surface,snapshot,now);
 if(e.kind==NOV_STEP_SOUND){unsigned pad=n->step_pad==UINT32_MAX?(v?v->pad:0):n->step_pad;int next=(int)pad+e.direction*(n->layer==NOV_STEP16?4:1);if(next>=0&&next<128)n->step_pad=(unsigned)next;}
 else if(e.kind==NOV_STEP_PAGE){
  unsigned size=n->layer==NOV_STEP16?16:64;int64_t next=(int64_t)n->step_first+e.direction*(int)size;
  if(v&&snapshot->steps.status==STEP_READY&&next>=0&&next<=UINT32_MAX-63){
   float denominator;memcpy(&denominator,v->target.event_owner?&v->target.division_bits:&snapshot->steps.division_bits,4);
   if(denominator>=1&&denominator<=3840&&denominator==(unsigned)denominator&&3840u%(unsigned)denominator==0){
    const uint32_t *bounds=v->target.event_owner?v->target.extent:snapshot->steps.extent;uint64_t extent=(uint64_t)bounds[0]|((uint64_t)bounds[1]<<32);
    if((uint64_t)next*(3840u/(unsigned)denominator)<extent)n->step_first=(unsigned)next;
   }
  }
 }
 else if(e.kind==NOV_STEP_CELL){if(v)input_step_add(&SURFACE_VALUE(surface,input),snapshot,native_step_slot(surface),e.sound,e.step,now);return 1;}
 input_discard(&SURFACE_VALUE(surface,input));return 1;
}
/* Custom (X 97, Pro 96) opens the mixer page used last, Volume at first. */
static void native_mixer_page(Surface *surface,MirrorBank *bank,unsigned layer){
 NovationState *n=&surface->novation;n->mixer_page=layer+1;native_layer(surface,bank,layer);input_discard(&SURFACE_VALUE(surface,input));
}
static void native_enter_mixer(Surface *surface,MirrorBank *bank){
 NovationState *n=&surface->novation;unsigned layer=n->mixer_page&&novation_mixer_page(n->mixer_page-1)?n->mixer_page-1:NOV_VOLUME;
 if(n->protocol==MCU_PROTOCOL_LPP3&&layer==NOV_SESSION)layer=NOV_VOLUME;
 if(!novation_mixer_page(n->layer))native_mixer_page(surface,bank,layer);
}
/* Pro Track Select row: the 8 visible clip columns in the clip grid, the mixer
 * bank on mixer pages; nothing in step views. */
static const CopiedTrack *native_row_track(Surface *surface,const MirrorBank *bank,const CopiedMirror *snapshot,unsigned strip,uint32_t now){
 NovationState *n=&surface->novation;if(strip>=8)return NULL;
 if(n->layer==NOV_CLIPS){const ClipProjection *v=native_clip_source(surface,snapshot,now);if(!v||!v->track_owner[strip])return NULL;
  for(unsigned i=0;i<snapshot->count;i++){if(snapshot->tracks[i].track_owner==v->track_owner[strip])return snapshot->tracks+i;}
  return NULL;}
 return novation_mixer_page(n->layer)?bank_strip_track(snapshot,bank,strip):NULL;
}
/* The mode the row acts in: a held mode button wins over the latched one. */
static unsigned native_row_mode(const NovationState *n){
 unsigned mode=n->row_hold?novation_row_mode(n->row_hold):n->row_mode;
 return mode==NOV_ROW_STOP&&n->layer!=NOV_CLIPS?NOV_ROW_SELECT+5u:mode; /* Stop acts in the clip grid only */
}
static void native_row_press(Surface *surface,MirrorBank *bank,const CopiedMirror *snapshot,unsigned strip,uint32_t now){
 NovationState *n=&surface->novation;MirrorInput *in=&SURFACE_VALUE(surface,input);if(n->row_hold)n->row_used=1;
 const CopiedTrack *t=native_row_track(surface,bank,snapshot,strip,now);if(!t)return;
 unsigned mode=native_row_mode(n);
 if(mode==NOV_ROW_STOP){input_clip_add(in,snapshot,native_step_slot(surface),CLIP_STOP,strip,0,now);return;}
 static const unsigned fields[]={CF_SELECTION,CF_ARM,CF_MUTE,CF_SOLO};if(mode>NOV_ROW_SOLO)return;
 MotorIdentity id={snapshot->epoch,t->serial,t->binding,t->incarnation,t->track,t->program,t->track_owner,t->program_owner,0,0,0};
 if(mode==NOV_ROW_SELECT&&snapshot->selection.available&&snapshot->selection.bits==t->track)return;
 input_control_bound(in,bank,snapshot,strip,fields[mode],0,1,now,&id,2);
}
/* Record Arm/Mute/Solo (and Stop Clip in the clip grid): a quick tap latches
 * that mode, or clears it if it was latched; held, it is momentary. */
static void native_row_button(NovationState *n,unsigned index,int down,unsigned prior,uint32_t now){
 if(down){if(!prior){n->row_hold=index;n->row_used=0;n->row_tick=now;}return;}
 if(!prior||n->row_hold!=index)return;
 if(!n->row_used&&now>=n->row_tick&&now-n->row_tick<400){unsigned mode=novation_row_mode(index);n->row_mode=n->row_mode==mode?NOV_ROW_SELECT:mode;}
 n->row_hold=0;
}
/* Sampler chords reuse primitive native commands. The XL3's one Record key
 * chooses an intent from copied native state; execution revalidates that state. */
static unsigned native_sampler_view(const NovationState *n){
#ifdef MODEL_FIRMWARE_310
 if(n->protocol==MCU_PROTOCOL_LCXL3)return n->shift;
 if(n->protocol==MCU_PROTOCOL_LPP3)return n->held[90];
 return n->protocol==MCU_PROTOCOL_LPX&&novation_mixer_page(n->layer)&&n->held[98];
#else
 (void)n;return 0;
#endif
}
static int native_sampler_button(NovationState *n,MirrorInput *in,const CopiedMirror *snapshot,const NovationEvent *event,unsigned prior,uint32_t now,unsigned dispatch){
 if(event->kind!=NOV_BUTTON)return 0;
 unsigned index=event->index;
 if(n->sampler_held[index]){if(!event->value){n->sampler_held[index]=0;n->held[index]=0;}return 1;}
 if(!event->value||prior||!native_sampler_view(n))return 0;
 unsigned op=0;
 if(n->protocol==MCU_PROTOCOL_LCXL3){if(index==116)op=SAMPLER_START;else if(index==118){
   /* Latch even when unavailable, so readiness during this hold cannot fire. */
   if(dispatch&&snapshot->sampler_available){unsigned state=snapshot->sampler.state;op=state==0?SAMPLER_ARM:state==1||state==2?SAMPLER_START:state==3||state==5?SAMPLER_STOP:0;}
  }else return 0;
 }else if(n->protocol==MCU_PROTOCOL_LPP3){op=index==10?SAMPLER_ARM:index==20?SAMPLER_START:index==8?SAMPLER_STOP:0;if(!op)return 0;
 }else{op=index==19?SAMPLER_ARM:index==39?SAMPLER_START:index==29?SAMPLER_STOP:0;if(!op)return 0;}
 n->sampler_held[index]=1;n->held[index]=0; /* Do not leak Pro Stop Clip into pad modifiers. */
 if(dispatch&&op)input_global_add(in,snapshot,op,0,now);
 return 1;
}
static void native_sampler_lights(const NovationState *n,const CopiedMirror *snapshot,unsigned char *values,unsigned char *styles,unsigned char *used){
 if(!native_sampler_view(n))return;
 unsigned xl=n->protocol==MCU_PROTOCOL_LCXL3,pro=n->protocol==MCU_PROTOCOL_LPP3;
 unsigned play=xl?116:pro?20:39,record=xl?118:pro?10:19,stop=pro?8:29;
 used[play]=used[record]=1;values[play]=values[record]=styles[play]=styles[record]=0;
 if(!xl){used[stop]=1;values[stop]=styles[stop]=0;}
 if(!snapshot->ready||!snapshot->sampler_available)return;
 unsigned state=snapshot->sampler.state,recording=state==3||state==5;
 if(state==0){if(xl)values[record]=3;else values[stop]=5;}
 if(state==1||state==2||recording)values[record]=5;
 if(recording){values[play]=21;styles[record]=2;}
}
static void native_intent(Surface *surface,MirrorBank *bank,const CopiedMirror *snapshot,const NovationEvent *event,uint32_t now){
 NovationState *n=&surface->novation;MirrorInput *in=&SURFACE_VALUE(surface,input);unsigned index=event->index,strip=index%8;
 if(event->kind==NOV_MODE){
  if(n->protocol==MCU_PROTOCOL_LCXL3){n->observed_mode=(unsigned)event->value;bank->assignment=event->value==2?(n->control_page<2?BA_QLINK:BA_EFFECT):BA_TRACK;memset(n->position_valid,0,sizeof(n->position_valid));memset(n->text_valid,0,sizeof(n->text_valid));input_discard(in);}return;
 }
 if(event->kind==NOV_TOUCH){n->touch[index]=(unsigned char)event->value;if(event->value)surface->native_touch_identity[index]=bank->strips[index%8];return;}
 if(n->protocol==MCU_PROTOCOL_LCXL3&&n->observed_mode!=1&&n->observed_mode!=2)return;
 if(event->kind==NOV_BUTTON){
  unsigned prior=n->held[index];n->held[index]=(unsigned char)event->value;
  if(native_sampler_button(n,in,snapshot,event,prior,now,1))return;
  if(n->protocol==MCU_PROTOCOL_LCXL3&&index==63){n->shift=(unsigned)event->value;return;}
  if(n->protocol!=MCU_PROTOCOL_LCXL3){
   unsigned pro=n->protocol==MCU_PROTOCOL_LPP3;
   native_copy_validate(n,snapshot);
   if(n->layer==NOV_CLIPS&&pro&&n->held[60]&&(index==1||index==2)){n->copy_source=(ClipEditSource){0};n->copy_used=1;n->row_used=1;return;}
   if(n->layer==NOV_CLIPS&&event->value&&!prior&&((pro&&n->held[90]&&(index==1||index==2))||(!pro&&n->held[98]&&!n->held[97]&&(index==93||index==94)))){
    n->copy_source=(ClipEditSource){0};n->copy_used=1;n->row_used=1;
    input_global_add(in,snapshot,(pro?index==1:index==93)?GLOBAL_UNDO:GLOBAL_REDO,0,now);return;
   }
   /* Consume the release of a history chord, even if Shift was released first. */
   if(pro&&(index==1||index==2)&&!event->value&&n->row_hold!=index)return;
   if(n->layer==NOV_CLIPS&&((pro&&index==60)||(!pro&&index==98&&n->held[97]))){n->copy_source=(ClipEditSource){0};n->copy_used=1;return;}
   if(pro&&index==40){if(n->layer==NOV_CLIPS){n->copy_source=(ClipEditSource){0};n->copy_used=n->pick_used=1;}return;}
   if(native_direct_button(surface,event,prior))return;
   /* X Custom defers its normal mixer tap only while Session owns the hold. */
   if((pro&&index==50)||(!pro&&index==97&&(n->layer==NOV_CLIPS||n->copy_held))){
    if(event->value&&!prior){n->copy_held=1;n->copy_used=native_direct_edit(n)||native_delete_held(n);n->copy_source=(ClipEditSource){0};}
    if(!event->value&&prior){unsigned mixer=!pro&&n->copy_held&&!n->copy_used&&n->layer==NOV_CLIPS;n->copy_held=0;n->copy_source=(ClipEditSource){0};if(mixer)native_enter_mixer(surface,bank);}
    return;
   }
   if(index==98)return; /* Capture MIDI/Projects is the momentary stop modifier. */
   /* Pro Stop Clip never stops MPC. In the clip grid it is the stop modifier
    * (held with a pad or scene); in a step view it stops the bound clip's
    * track; elsewhere it does nothing. */
   if(n->protocol==MCU_PROTOCOL_LPP3&&index==8){
    if(novation_step_view(n->layer)){if(event->value&&!prior&&native_step_source(surface,snapshot,now))input_clip_stop_add(in,snapshot,native_step_slot(surface),now);}
    else if(n->layer==NOV_CLIPS||n->row_hold==8)native_row_button(n,8,event->value,prior,now);
    return;
   }
   /* Pro Record Arm, Mute and Solo set the Track Select row's mode. */
   if(n->protocol==MCU_PROTOCOL_LPP3&&index>=1&&index<=3){if(!novation_step_view(n->layer)||n->row_hold==index)native_row_button(n,index,event->value,prior,now);return;}
   if(n->protocol==MCU_PROTOCOL_LPP3&&index>=101&&index<=108){if(event->value&&!prior&&!novation_step_view(n->layer))native_row_press(surface,bank,snapshot,index-101,now);return;}
   /* Custom opens the mixer pages. */
   if(index==(n->protocol==MCU_PROTOCOL_LPP3?96u:97u)){if(event->value&&!prior)native_enter_mixer(surface,bank);return;}
   if(native_pick_button(surface,bank,event,prior))return;
   if(event->value&&!prior&&native_clip_intent(surface,bank,snapshot,event,now))return;
   if(n->layer==NOV_CLIPS&&n->protocol==MCU_PROTOCOL_LPX&&!event->value)return;
  }
  unsigned play=n->protocol==MCU_PROTOCOL_LCXL3?116:n->protocol==MCU_PROTOCOL_LPP3?20:39,stop=n->protocol==MCU_PROTOCOL_LCXL3?0:n->protocol==MCU_PROTOCOL_LPP3?8:29;
  if(index==play&&(n->protocol==MCU_PROTOCOL_LCXL3||n->protocol==MCU_PROTOCOL_LPP3)){
   if(!event->value){in->play_down=0;return;}
   if(prior||!snapshot->playing.available)return;
   input_jog_resync(in,snapshot);
   unsigned playing=snapshot->playing.bits||in->stop_play_wait;
   if(in->coordinator)for(unsigned i=0;i<in->coordinator->count;i++){MirrorInput *peer=in->coordinator->origins[i];if(peer->jog_epoch==snapshot->epoch)playing|=peer->stop_play_wait;}
   if(!playing){input_stop_button(in,snapshot,94,1,now);return;}
   /* Pause in place; never invoke Stop's separate return-to-start gesture. */
   in->play_down=1;in->stop_play_wait=in->stop_home=0;input_jog_release(in,3);
   if(in->coordinator)for(unsigned i=0;i<in->coordinator->count;i++){MirrorInput *peer=in->coordinator->origins[i];peer->stop_play_wait=peer->stop_home=0;input_jog_release(peer,3);}
   input_global_add(in,snapshot,GLOBAL_STOP,0,now);return;
  }
  if(index==play||index==stop){input_stop_button(in,snapshot,index==play?94:93,event->value,now);return;}
  if(!event->value||prior)return;
  if(index==(n->protocol==MCU_PROTOCOL_LCXL3?118u:n->protocol==MCU_PROTOCOL_LPP3?10u:19u)){input_global_add(in,snapshot,GLOBAL_RECORD_TOGGLE,0,now);return;}
  if(n->protocol==MCU_PROTOCOL_LCXL3){
   if(index==65&&n->shift&&n->observed_mode==1){native_mixer_view(surface,bank,now);return;}
   if(index>=45&&index<=52&&n->shift&&n->observed_mode==1){native_shift_select(surface,bank,snapshot,index-45,now);return;}
   if(index==65){if(bank->view!=BV_DRUM_PADS)n->solo_arm=!n->solo_arm;return;}if(index==66){if(bank->view!=BV_DRUM_PADS)n->mute_select=!n->mute_select;return;}
   if(index==102||index==103){bank_navigate(bank,index==102?1:-1,now);return;}
   if(index==106||index==107){if(n->observed_mode==2)native_control_page(surface,bank,index==107?1:-1);return;}
   if(index>=37&&index<=52){unsigned field=index<45?(n->solo_arm&&bank->view!=BV_DRUM_PADS?CF_ARM:CF_SOLO):(n->mute_select&&bank->view!=BV_DRUM_PADS?CF_SELECTION:CF_MUTE);input_control(in,bank,snapshot,(index-37)%8,field,0,1,now);}return;
  }
  if(native_step_intent(surface,bank,snapshot,event,now))return;
  if(novation_step_view(n->layer))return;
  if(n->protocol==MCU_PROTOCOL_LPP3){
   if(index==91||index==92){bank_navigate(bank,index==92?1:-1,now);return;}
   if(index>=4&&index<=7){native_mixer_page(surface,bank,index==4?NOV_VOLUME:index==5?NOV_PAN:index==6?NOV_SEND:NOV_DEVICE);return;}
   if((index==80||index==70)&&n->layer==NOV_DEVICE){bank->qlink_page=index==70;input_discard(in);memset(n->led_valid,0,sizeof(n->led_valid));return;}
  }else if(index==93||index==94){bank_navigate(bank,index==94?1:-1,now);return;}
  /* X right column on mixer pages: Volume, Pan, Sends, Device, mixer grid. */
  if(n->protocol==MCU_PROTOCOL_LPX&&(index==89||index==79||index==69||index==59||index==49)){native_mixer_page(surface,bank,index==89?NOV_VOLUME:index==79?NOV_PAN:index==69?NOV_SEND:index==59?NOV_DEVICE:NOV_SESSION);return;}
  if((index==91||index==92)&&n->layer==NOV_DEVICE){bank->qlink_page=index==92;input_discard(in);memset(n->led_valid,0,sizeof(n->led_valid));return;}
  if(index>=11&&index<=88&&index%10>=1&&index%10<=8){unsigned row=index/10;strip=index%10-1;
   if(n->layer==NOV_PAN){input_absolute_value(in,bank,snapshot,strip,CF_PAN,novation_pan_value(row),now);return;}
   if(n->layer!=NOV_SESSION){unsigned value=(row-1)*127/7;if(n->layer==NOV_DEVICE)input_qlink_edit(in,bank,snapshot,strip,0,(int)(value*16383/127),now);
    else {input_absolute_bound(in,bank,snapshot,strip,n->layer==NOV_PAN?CF_PAN:n->layer==NOV_SEND?CF_SEND1:CF_VOLUME,value,now);}return;}
   static const unsigned fields[]={CF_SELECTION,CF_MUTE,CF_SOLO,CF_ARM};
   if(row<=4)input_control(in,bank,snapshot,strip,fields[row-1],0,1,now);
   else input_absolute_bound(in,bank,snapshot,strip,CF_VOLUME,(row-5)*127/3,now);
  }return;
 }
 if(event->kind!=NOV_ABSOLUTE&&event->kind!=NOV_RELATIVE)return;
 if(n->protocol==MCU_PROTOCOL_LCXL3&&n->touch[index]&&memcmp(&surface->native_touch_identity[index],&bank->strips[strip],sizeof(MotorIdentity)))return;
 unsigned device=n->protocol==MCU_PROTOCOL_LCXL3?n->observed_mode==2&&index>=8:n->protocol==MCU_PROTOCOL_LPP3?index/8==3:n->layer==NOV_DEVICE;
 if(device){
  /* The mirror exposes one eight-control selected-context page at a time.
   * Control rows two/three stay dark; they do not fabricate extra pages. */
  if(n->protocol==MCU_PROTOCOL_LCXL3&&index>=16)return;
  if(bank->assignment==BA_QLINK)input_qlink_edit(in,bank,snapshot,strip,event->kind==NOV_RELATIVE?event->value:0,event->kind==NOV_ABSOLUTE?event->value*16383/127:-1,now);
  else if(bank->assignment==BA_EFFECT&&event->kind==NOV_RELATIVE)input_effect_edit(in,bank,snapshot,strip,event->value,0,now);
  return;
 }
 unsigned field=native_field(n,index);
 if(event->kind==NOV_RELATIVE)input_control_bound(in,bank,snapshot,strip,field,event->value,0,now,&bank->strips[strip],3);
 else if(n->protocol!=MCU_PROTOCOL_LCXL3||native_pickup(surface,snapshot,bank,index,field,event->value))input_absolute_bound(in,bank,snapshot,strip,field,(unsigned)event->value,now);
}
static int native_consume(Surface *surface,MirrorBank *bank,const MirrorState *state,ControllerRuntime *runtime,const snd_seq_event_t *event,int discard){
 if(!address_equal(event->source,surface->full))return 1;
 unsigned char midi[3];const unsigned char *bytes=midi;unsigned length=3;
 if(event->type==SND_SEQ_EVENT_SYSEX){bytes=event->data.ext.ptr;length=event->data.ext.len;}
 else if(event->type==SND_SEQ_EVENT_CONTROLLER){if(event->data.control.channel>15||event->data.control.param>127||event->data.control.value<0||event->data.control.value>127)return 1;midi[0]=0xb0|event->data.control.channel;midi[1]=(unsigned char)event->data.control.param;midi[2]=(unsigned char)event->data.control.value;}
 else if(event->type==SND_SEQ_EVENT_NOTEON||event->type==SND_SEQ_EVENT_NOTEOFF){if(event->data.note.channel>15||event->data.note.note>127||event->data.note.velocity>127)return 1;midi[0]=(event->type==SND_SEQ_EVENT_NOTEON?0x90:0x80)|event->data.note.channel;midi[1]=event->data.note.note;midi[2]=event->data.note.velocity;}
 else return 1;
 NovationEvent decoded;if(!novation_decode(&surface->novation,bytes,length,&decoded)||discard)return 1;novation_observe(&surface->novation,&decoded);
 if(decoded.kind==NOV_PROGRAMMER||decoded.kind==NOV_LAYOUT)return 1;
 if(decoded.kind==NOV_FEATURE){if(!novation_admitted(&surface->novation))input_discard(&SURFACE_VALUE(surface,input));return 1;}
 /* Mode reports describe controller state even while MPC has no ready
  * project. They never need a musical snapshot or publish an MPC command. */
 if(decoded.kind==NOV_MODE){native_intent(surface,bank,NULL,&decoded,0);return 1;}
 if(!novation_admitted(&surface->novation))return 1;
 if(!runtime)return 1; /* The shipped path always owns the shared cycle. */
 int copied=runtime_snapshot(runtime,state,bank->view==BV_DRUM_PADS);if(copied<0)return 0;
 if(!copied||!runtime->snapshot->ready||atomic_load(&SURFACE_VALUE(surface,input).commands->new_project_intent)){
  unsigned sampler_consumed=native_sampler_button(&surface->novation,&SURFACE_VALUE(surface,input),runtime->snapshot,&decoded,decoded.kind==NOV_BUTTON?surface->novation.held[decoded.index]:0,runtime->now,0);
  surface->novation.copy_source=(ClipEditSource){0};surface->novation.copy_used=1;
  input_discard(&SURFACE_VALUE(surface,input));input_jog_discard(&SURFACE_VALUE(surface,input));memset(surface->native_pickup,0,sizeof(surface->native_pickup));memset(surface->native_touch_identity,0,sizeof(surface->native_touch_identity));if(decoded.kind==NOV_BUTTON&&!sampler_consumed){surface->novation.held[decoded.index]=(unsigned char)decoded.value;if(surface->novation.protocol==MCU_PROTOCOL_LCXL3&&decoded.index==63)surface->novation.shift=(unsigned)decoded.value;}if(decoded.kind==NOV_TOUCH)surface->novation.touch[decoded.index]=(unsigned char)decoded.value;return 1;
 }
 bank_apply(bank,runtime->snapshot,runtime->now);input_sync(&SURFACE_VALUE(surface,input),bank);
 native_intent(surface,bank,runtime->snapshot,&decoded,runtime->now);return 1;
}
/* Launchpad side buttons. Every Programmer LED starts off and only a button
 * with a function in the current view lights: transport, Session, the
 * step-view button (X Note, Pro Sequencer) and Custom (mixer pages)
 * everywhere; Capture MIDI/Projects (stop hold) in the clip grid; Up/Down
 * where they scroll rows, change Q-Link page or drum sound; page buttons on
 * mixer pages (X right column, Pro bottom 4-7), the current page green; Pro
 * row-mode buttons outside step views. The clip grid then paints its scene
 * buttons over the right column. */
static void native_buttons(const NovationState *n,const CopiedMirror *snapshot,unsigned char *values,unsigned char *styles,unsigned char *used){
 unsigned pro=n->protocol==MCU_PROTOCOL_LPP3,layer=n->layer,step=novation_step_view(layer),clips=layer==NOV_CLIPS,mixer=novation_mixer_page(layer);
 for(unsigned i=0;i<128;i++)if(novation_led_index(n->protocol,i)){used[i]=1;values[i]=0;styles[i]=0;}
 unsigned play=pro?20:39,record=pro?10:19,stop=29;
 values[play]=snapshot->ready&&snapshot->playing.available&&snapshot->playing.bits?21:1;
 values[record]=snapshot->ready&&snapshot->record_mode.available&&(snapshot->record_mode.bits==1||snapshot->record_mode.bits==3)?5:1;styles[record]=values[record]==5?2:0;
 if(!pro)values[stop]=snapshot->playing.available&&!snapshot->playing.bits?5:1;
 values[pro?93:95]=clips?21:3;
 values[pro?96:97]=mixer?21:3;
 if(pro){values[97]=step?21:3;if(step)values[79]=layer==NOV_STEP16?21:3;}
 else values[96]=layer==NOV_STEP64?21:layer==NOV_STEP16?13:3;
 if(clips){values[98]=n->held[98]?5:3;if(pro){values[40]=n->held[40]?13:3;values[50]=n->held[50]?13:3;values[60]=n->held[60]?5:3;values[90]=3;}}
 values[pro?91:93]=values[pro?92:94]=3;
 if(clips||step||layer==NOV_DEVICE)values[pro?80:91]=values[pro?70:92]=3;
 if(!pro&&mixer){static const unsigned char side[]={89,79,69,59,49},pages[]={NOV_VOLUME,NOV_PAN,NOV_SEND,NOV_DEVICE,NOV_SESSION};for(unsigned i=0;i<sizeof(side);i++)values[side[i]]=layer==pages[i]?21:3;}
 if(pro&&!step){
  static const unsigned char pages[]={NOV_VOLUME,NOV_PAN,NOV_SEND,NOV_DEVICE};for(unsigned i=0;i<4;i++)values[4+i]=layer==pages[i]?21:3;
  unsigned mode=native_row_mode(n);values[1]=mode==NOV_ROW_ARM?5:1;values[2]=mode==NOV_ROW_MUTE?9:1;values[3]=mode==NOV_ROW_SOLO?45:1;
  if(clips)values[8]=mode==NOV_ROW_STOP||n->held[8]?5:3;
 }
 if(pro&&step)values[8]=3;
}
/* Pro Track Select row LEDs for the current mode. */
static void native_row_lights(Surface *surface,const MirrorBank *bank,const CopiedMirror *snapshot,uint32_t now,unsigned char *values){
 NovationState *n=&surface->novation;if(n->protocol!=MCU_PROTOCOL_LPP3||novation_step_view(n->layer))return;
 unsigned mode=native_row_mode(n);const ClipProjection *clips=n->layer==NOV_CLIPS?native_clip_source(surface,snapshot,now):NULL;
 for(unsigned strip=0;strip<8;strip++){
  const CopiedTrack *t=native_row_track(surface,bank,snapshot,strip,now);unsigned char *v=values+101+strip;if(!t)continue;
  ChannelWire state=channel_wire(snapshot,t,CF_VOLUME);
  switch(mode){
  case NOV_ROW_ARM:*v=state.led[0]?5:7;break;
  case NOV_ROW_MUTE:*v=state.led[2]?9:11;break;
  case NOV_ROW_SOLO:*v=state.led[1]?45:47;break;
  case NOV_ROW_STOP:{unsigned active=0;if(clips)for(unsigned r=0;r<8;r++){unsigned c=clips->cells[r*8+strip].state;active|=c>=CLIP_WILL_PLAY&&c<=CLIP_WILL_STOP_OVERDUBBING;}*v=active?21:0;break;}
  case NOV_ROW_SELECT:{
   if(snapshot->selection.available&&snapshot->selection.bits==t->track){*v=3;break;}
   uint32_t rgb=t->available&&t->fields[CF_COLOR].available?novation_track_rgb(t->fields[CF_COLOR].bits):0;
   *v=rgb?(unsigned char)novation_palette_nearest(NOV_RGB|(((rgb>>16)&127)/3)<<16|(((rgb>>8)&127)/3)<<8|((rgb&127)/3)):1;break;}
  default:break;
  }
 }
}
static int native_output(Surface *surface,MirrorBank *bank,const CopiedMirror *snapshot,uint32_t now){
 NovationState *n=&surface->novation;n->bank=bank->offset;novation_budget(n);if(!native_initialise(surface,now))return 0;if(!novation_admitted(n))return 1;
 if(n->protocol==MCU_PROTOCOL_LCXL3&&n->observed_mode!=1&&n->observed_mode!=2)return 1;
 native_copy_validate(n,snapshot);
 native_pick_settle(surface,bank,now);native_shift_settle(surface,bank,snapshot,now);

 unsigned char values[128]={0},styles[128]={0},used[128]={0};
 /* Side buttons first: every Launchpad LED starts off, then each layer paints
  * its own cells (the mixer grid below, the step or clip grid after). */
 if(n->protocol!=MCU_PROTOCOL_LCXL3)native_buttons(n,snapshot,values,styles,used);
 for(unsigned strip=0;strip<8;strip++){
  const CopiedTrack *track=bank_strip_track(snapshot,bank,strip);ChannelWire state=channel_wire(snapshot,track,CF_VOLUME);
  if(n->protocol==MCU_PROTOCOL_LCXL3){unsigned a=37+strip,b=45+strip;used[a]=used[b]=1;unsigned arm=n->solo_arm&&bank->view!=BV_DRUM_PADS,select=n->mute_select&&bank->view!=BV_DRUM_PADS;values[a]=state.led[arm?0:1]?arm?5:13:1;values[b]=state.led[select?3:2]?select?21:9:1;if(!track)values[a]=values[b]=0;
   /* Holding Shift in DAW Mixer, the lower row shows the Track mixer bank:
    * the selected track, whose pads the Pad mixer shows, is green. */
   if(n->shift&&n->observed_mode==1){const CopiedTrack *t=native_shift_track(n,bank,snapshot,strip);values[b]=!t?0:snapshot->selection.available&&snapshot->selection.bits==t->track?21:1;}}
  else {unsigned fields[]={3,2,1,0};for(unsigned row=1;row<=8;row++){unsigned pad=row*10+strip+1;used[pad]=1;
   if(n->layer!=NOV_SESSION){int pos=-1;if(n->layer==NOV_DEVICE){const QLinkSlot *slot=input_qlink_slot(snapshot,bank->qlink_page*8+strip,now);if(slot){float value;memcpy(&value,&slot->bits,4);if(isfinite(value)&&value>=0&&value<=1)pos=(int)(value*127+0.5f);}}
    else if(n->layer==NOV_PAN){
     /* Bipolar: lit outward from the centre; the centre pair white at 0.5. */
     uint32_t bits;float pan;if(native_position(snapshot,bank,strip,CF_PAN,&bits,NULL)<0){values[pad]=0;continue;}memcpy(&pan,&bits,4);
     values[pad]=novation_pan_lit(row,pan)?(pan>=0.5f-1e-4f&&pan<=0.5f+1e-4f?3:45):0;continue;
    }
    else pos=native_position(snapshot,bank,strip,n->layer==NOV_SEND?CF_SEND1:CF_VOLUME,NULL,NULL);
    values[pad]=pos>=0&&pos>=(int)((row-1)*127/7)?21:0;
   }else if(row<=4){values[pad]=track?(state.led[fields[row-1]]?(row==4?5:row==2?9:21):1):0;if(row==4&&values[pad]==5)styles[pad]=2;}
   else{int pos=native_position(snapshot,bank,strip,CF_VOLUME,NULL,NULL);values[pad]=pos>=0&&pos>=(int)((row-5)*127/3)?21:0;}
  }}
 }
 native_row_lights(surface,bank,snapshot,now,values);
 if(n->protocol==MCU_PROTOCOL_LCXL3){unsigned play=116,record=118;used[play]=used[record]=1;values[play]=snapshot->ready&&snapshot->playing.available&&snapshot->playing.bits?21:1;values[record]=snapshot->ready&&snapshot->record_mode.available&&(snapshot->record_mode.bits==1||snapshot->record_mode.bits==3)?5:1;
  used[65]=used[66]=1;values[65]=bank->view==BV_DRUM_PADS?45:n->solo_arm?5:13;values[66]=n->mute_select&&bank->view!=BV_DRUM_PADS?21:9;}
 if(novation_step_view(n->layer)){
  const StepProjection *v=native_step_source(surface,snapshot,now);
  for(unsigned cell=0;cell<64;cell++){unsigned pad=(8-cell/8)*10+cell%8+1,sound=n->layer==NOV_STEP16?cell/16:0,step=n->layer==NOV_STEP16?cell%16:cell;
   unsigned valid=v&&v->notes[sound]<=127&&(v->valid[step/32]&(1u<<(step%32)));
   used[pad]=1;styles[pad]=0;values[pad]=valid&&(v->on[sound][step/32]&(1u<<(step%32)))?native_drum_color(v->pad+sound):0;
   if(valid&&snapshot->playing.available&&snapshot->playing.bits&&(v->target.event_owner?v->target.clock_available:snapshot->steps.clock_available)){
    float denominator;memcpy(&denominator,v->target.event_owner?&v->target.division_bits:&snapshot->steps.division_bits,4);unsigned division=3840u/(unsigned)denominator;
    const uint32_t *clock=v->target.event_owner?v->target.clock_value:snapshot->steps.clock_value;uint64_t tick=(uint64_t)clock[0]|((uint64_t)clock[1]<<32),absolute=tick/division;
    if(absolute==(uint64_t)v->first_step+step)values[pad]=45;
   }
  }
 }
 if(n->layer==NOV_CLIPS){
  const ClipProjection *v=native_clip_source(surface,snapshot,now);
  unsigned colours[8]={0};if(v)for(unsigned column=0;column<8;column++)colours[column]=native_column_colour(n,snapshot,v,column);
  for(unsigned i=0;i<64;i++){unsigned pad=(8-i/8)*10+i%8+1;used[pad]=1;native_clip_color(v?v->cells[i].state:CLIP_NO_SLOT,colours[i%8],1,values+pad,styles+pad);}
  for(unsigned row=0;row<8;row++){unsigned button=89-row*10;used[button]=1;native_clip_color(v?v->scene_state[row]:CLIP_NO_SLOT,0,0,values+button,styles+button);}
  const ClipEditSource *source=&n->copy_source;
  if(v&&source->kind&&source->row>=v->first_row&&source->row-v->first_row<8){unsigned row=source->row-v->first_row;
   if(source->kind==2){unsigned button=89-row*10;values[button]=13;styles[button]=1;}
   else if(source->column>=v->first_column&&source->column-v->first_column<8){unsigned pad=(8-row)*10+source->column-v->first_column+1;values[pad]=13;styles[pad]=1;}
  }
 }
 /* A settled accepted edit briefly acknowledges its initiating target. This
  * overlays only LEDs, never the authoritative clip/playback projection. */
 MirrorInput *feedback=&SURFACE_VALUE(surface,input);
 if(feedback->clip_feedback_request){const CommandRequest *r=&feedback->clip_feedback;
  if(n->layer!=NOV_CLIPS||!snapshot->ready||!snapshot->clips_available||r->epoch!=snapshot->epoch||r->project_owner!=snapshot->clips.project_owner||r->qlink_root!=snapshot->clips.lanes||(feedback->clip_feedback_started&&(now<feedback->clip_feedback_tick||now-feedback->clip_feedback_tick>=500)))feedback->clip_feedback_request=0;
  else if(native_clip_source(surface,snapshot,now)&&r->effect_index>=n->clip_row&&r->effect_index-n->clip_row<8){
   unsigned row=r->effect_index-n->clip_row,index=0;
   if(clip_scene_intent(r->bits))index=89-row*10;
   else if(r->pad_index>=n->clip_column&&r->pad_index-n->clip_column<8)index=(8-row)*10+r->pad_index-n->clip_column+1;
   if(index){if(!feedback->clip_feedback_started){feedback->clip_feedback_started=1;feedback->clip_feedback_tick=now;}unsigned value=now-feedback->clip_feedback_tick<80?0:3;used[index]=1;values[index]=value;styles[index]=0;if(!n->led_valid[index]||n->led_value[index]!=value||n->led_style[index])n->led_cursor=index;}
  }
 }
 native_sampler_lights(n,snapshot,values,styles,used);
 /* Reserve up to eight messages for changing control positions/text. */
 for(unsigned step=0;step<128&&n->cycle_messages<8;step++){unsigned i=n->led_cursor;n->led_cursor=(i+1)%128;if(!used[i]||(n->led_valid[i]&&n->led_value[i]==values[i]&&n->led_style[i]==styles[i]))continue;NovationWire w=novation_led(n->protocol,i,styles[i],values[i]);int sent=native_wire(surface,&w,0);if(sent!=1)return sent==2?1:0;n->led_valid[i]=1;n->led_value[i]=values[i];n->led_style[i]=styles[i];}
 if(n->protocol!=MCU_PROTOCOL_LCXL3)return 1; /* Programmer grid is its own fader projection. */
 /* One changed name precedes values: even after eight button LEDs its
  * maximum26 bytes leave78 bytes and seven messages for value progress. */
 if(n->protocol==MCU_PROTOCOL_LCXL3)for(unsigned step=0;step<32;step++){
  unsigned i=n->text_cursor;n->text_cursor=(i+1)%32;const CopiedTrack *track=bank_strip_track(snapshot,bank,i%8);unsigned char text[17]={0};const char *label="Unavailable";unsigned len=11;char short_label[17];
  if(i<8&&track&&track->pad_owner){snprintf(short_label,sizeof(short_label),"Pad %c%02u",'A'+track->pad_index/16,track->pad_index%16+1);label=short_label;len=(unsigned)strlen(label);}
  else if(i<8&&track&&track->fields[CF_NAME].available){label=track->fields[CF_NAME].text;len=track->fields[CF_NAME].length;}
  else if(i>=8&&n->observed_mode==1&&track){if(track->pad_owner)snprintf(short_label,sizeof(short_label),i<24?"-":"Pad Pan %c%02u",'A'+track->pad_index/16,track->pad_index%16+1);else snprintf(short_label,sizeof(short_label),"Track %s %u",i<16?"Send1":i<24?"Send2":"Pan",bank->offset+i%8+1);label=short_label;len=(unsigned)strlen(label);}
  else if(i>=8&&i<16&&n->observed_mode==2){
   if(bank->assignment==BA_QLINK&&input_qlink_slot(snapshot,bank->qlink_page*8+i%8,now)){snprintf(short_label,sizeof(short_label),"Q-Link %u",bank->qlink_page*8+i%8+1);label=short_label;len=(unsigned)strlen(label);}
   else if(bank->assignment==BA_EFFECT&&snapshot->effects_available&&snapshot->effects.serial==bank->selected.serial&&snapshot->effects.slot==bank->effects_slot&&snapshot->effects.page==bank->effects_page&&snapshot->effects.parameters[i%8].status==EF_READY){label=snapshot->effects.parameters[i%8].name;len=(unsigned)strnlen(label,EFFECT_TEXT);}
  }
  if(len>16)len=16;
  for(unsigned j=0;j<len;j++){unsigned char c=(unsigned char)label[j];text[j]=c>=32&&c<=126?c:'?';}
  if(n->text_valid[i]&&!memcmp(text,n->text[i],17))continue;
  NovationWire w=novation_sysex(n->protocol,6);novation_append(&w,i+5);novation_append(&w,0);for(unsigned j=0;text[j]&&j<16;j++)novation_append(&w,text[j]);novation_append(&w,0xf7);
  int sent=native_wire(surface,&w,0);if(sent!=1)return sent==2?1:0;n->text_valid[i]=1;memcpy(n->text[i],text,17);break;
 }
 unsigned count=32;
 for(unsigned step=0;step<count&&n->cycle_messages<14;step++){
  unsigned i=n->position_cursor;n->position_cursor=(i+1)%count;unsigned strip=i%8;int value=-1;
  unsigned device=n->observed_mode==2&&i>=8;
  if(device){if(i<16&&bank->assignment==BA_QLINK){const QLinkSlot *slot=input_qlink_slot(snapshot,bank->qlink_page*8+strip,now);if(slot){float f;memcpy(&f,&slot->bits,4);if(isfinite(f)&&f>=0&&f<=1)value=(int)(f*127+0.5f);}}else if(i<16&&bank->assignment==BA_EFFECT&&snapshot->effects_available&&snapshot->effects.status==EF_READY&&snapshot->effects.slot==bank->effects_slot&&snapshot->effects.page==bank->effects_page&&snapshot->effects.serial==bank->selected.serial){const EffectParameter *p=snapshot->effects.parameters+strip;if(p->status==EF_READY&&p->tick<=now&&now-p->tick<EFFECT_FRESH_MS){float f;memcpy(&f,&p->bits,4);if(isfinite(f)&&f>=0&&f<=1)value=(int)(f*127+0.5f);}}}
  else value=native_position(snapshot,bank,strip,native_field(n,i),NULL,NULL);
  /* Unavailable controls are dark, not silently populated with a fake value. */
  if(n->protocol==MCU_PROTOCOL_LCXL3&&i>=8){
   /* Track columns take the MPC track colour; pads have none and device
    * pages are not per track, so both keep the palette default. */
   unsigned cc=i+5;uint32_t col=value<0?0:device?45:21;const CopiedTrack *track=value>=0&&!device?bank_strip_track(snapshot,bank,strip):NULL;
   if(track&&!track->pad_owner&&track->fields[CF_COLOR].available){uint32_t rgb=novation_track_rgb(track->fields[CF_COLOR].bits);if(rgb)col=rgb;}
   if(!n->led_valid[cc]||n->knob_led[i-8]!=col){NovationWire w=novation_lcxl3_led(cc,col);int sent=native_wire(surface,&w,0);if(sent!=1)return sent==2?1:0;n->led_valid[cc]=1;n->knob_led[i-8]=col;}
  }
  if(value<0&&i<8){n->position_valid[i]=0;continue;}
  /* Relative encoder touch brackets motion, not an actuator: keep its live
   * source feedback, but never retarget a held gesture through a bank change. */
  if(value>=0&&n->touch[i]&&(i<8||memcmp(&surface->native_touch_identity[i],&bank->strips[strip],sizeof(MotorIdentity))))continue;
  unsigned cached=value<0?255u:(unsigned)value;
  if(n->position_valid[i]&&n->position[i]==cached)continue;
  unsigned cc=i+5;
  NovationWire w;
  if(i<8)w=novation_cc(15,cc,(unsigned)value);
  else {
   /* Use explicit host text values for relative encoders.
    * Field 1 is the documented text-value route; '-' clears a lost source. */
   char text[4];if(value<0)strcpy(text,"-");else snprintf(text,sizeof(text),"%u",(unsigned char)value);
   w=novation_sysex(n->protocol,6);novation_append(&w,cc);novation_append(&w,1);
   for(unsigned j=0;text[j];j++)novation_append(&w,(unsigned char)text[j]);
   novation_append(&w,0xf7);
  }
  int sent=native_wire(surface,&w,0);if(sent!=1)return sent==2?1:0;n->position_valid[i]=1;n->position[i]=(unsigned char)cached;
 }
 return 1;
}
