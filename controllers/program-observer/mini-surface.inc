/* Mini presentation and gestures. MirrorBank retains the playable track bank
 * in every assignment; all parameter values/commands remain native-owned. */
static const unsigned mini_top_notes[8]={89,90,40,41,42,43,44,45};
static const unsigned mini_bottom_notes[8]={87,88,91,92,86,93,94,95};
enum {MINI_SELECT,MINI_MUTE,MINI_SOLO,MINI_RECORD};
static unsigned mini_button_field(unsigned mode){return mode==MINI_SELECT?CF_SELECTION:mode==MINI_MUTE?CF_MUTE:mode==MINI_SOLO?CF_SOLO:CF_ARM;}
static void surface_jog(MirrorInput*,const CopiedMirror*,unsigned,unsigned,int,uint32_t,unsigned);
static unsigned mini_mode(const MirrorBank *bank){return bank->assignment==BA_EFFECT?5:bank->assignment==BA_MINI_SEND?4:bank->assignment==BA_QLINK?2+bank->qlink_page:bank->assignment==BA_PAN?1:0;}
static void mini_select_mode(Surface *s,MirrorInput *in,MirrorBank *bank,unsigned mode,uint32_t now){
 bank->assignment=mode==5?BA_EFFECT:mode==4?BA_MINI_SEND:mode>=2?BA_QLINK:mode==1?BA_PAN:BA_TRACK;
 bank->qlink_page=mode==3;bank->qlink_screen_order=1;bank->effects_slot=EFFECT_LIST;bank->effects_page=bank->effects_generation=0;
 bank->flip=0;bank->fader_field=CF_VOLUME;bank_view(bank,BV_TRACK,now);
 bank_drop(bank,now);input_discard(in);memset(s->display_until,0,sizeof(s->display_until));
 memset(s->wire_valid,0,sizeof(s->wire_valid));s->mode_valid=0;
 for(unsigned i=0;i<8;i++)s->display_field[i]=mode==1?CF_PAN:CF_VOLUME;
}
static void mini_consume_modifiers(Surface *s){s->mini_layer_consumed|=(s->general_down[84]?1u:0)|(s->general_down[85]?2u:0);}
static void mini_stop_navigation(Surface *s,MirrorInput *in,const CopiedMirror *snapshot,uint32_t now){
 for(unsigned i=0;i<2;i++)if(s->general_down[91+i]){mini_consume_modifiers(s);s->mini_navigation_locked|=1u<<i;surface_jog(in,snapshot,9,91+i,0,now,0);}
}
static void mini_unready_button(Surface *s,MirrorInput *in,const CopiedMirror *snapshot,unsigned note,int down,uint32_t now){
 if(note>=128)return;
 s->general_down[note]=down;s->mode_valid=0;
 if(note==84||note==85)s->mini_layer_consumed|=1u<<(note-84);
 if(note==91||note==92){s->mini_navigation_locked|=1u<<(note-91);surface_jog(in,snapshot,9,note,0,now,0);}
}
static int mini_button(Surface *s,MirrorInput *in,MirrorBank *bank,const CopiedMirror *snapshot,unsigned note,int down,uint32_t now){
 if(SURFACE_VALUE(s,controller).profile!=&mcu_profile_xtouch_mini)return 0;
 if(note==84||note==85){
  unsigned bit=1u<<(note-84);
  if(down&&!s->general_down[note]){
   s->mini_layer_consumed&=~bit;s->general_down[note]=1;mini_stop_navigation(s,in,snapshot,now);
   if(s->general_down[84]&&s->general_down[85])s->mini_layer_consumed|=3;
  }else if(!down&&s->general_down[note]){
   if(!(s->mini_layer_consumed&bit)){
    if(note==84)mini_select_mode(s,in,bank,(mini_mode(bank)+1)%6,now);
    else s->mini_button_mode=(s->mini_button_mode+1)%4;
   }
   s->mini_layer_consumed&=~bit;s->general_down[note]=0;
  }
  s->mode_valid=0;return 1;
 }
 unsigned top=8;for(unsigned i=0;i<8;i++)if(note==mini_top_notes[i])top=i;
 if(top<8){
  if(down&&!s->general_down[note]){
   mini_consume_modifiers(s);
   if(s->general_down[84]){
    if(top<6)mini_select_mode(s,in,bank,top,now);
    else input_global_add(in,snapshot,top==6?GLOBAL_SAVE:GLOBAL_TRACK_NEW,0,now);
   }else if(s->general_down[85]){if(top<4)s->mini_button_mode=top;}
   else{unsigned field=mini_button_field(s->mini_button_mode);input_control_bound(in,bank,snapshot,top,field,0,1,now,bank->strips+top,3);}
  }
  s->general_down[note]=down;s->mode_valid=0;return 1;
 }
 if(note==91||note==92){
  unsigned bit=1u<<(note-91);
  if(down&&!s->general_down[note]){
   s->mini_navigation_locked&=~bit;
   if(s->general_down[84]||s->general_down[85]){
    s->mini_navigation_locked|=bit;mini_consume_modifiers(s);
    if(s->general_down[84]&&s->general_down[85]){}
    else if(s->general_down[85]){unsigned before=bank->offset;bank_navigate(bank,note==92?1:-1,now);if(before!=bank->offset)input_bank_discard(in);}
    else if(bank->assignment==BA_QLINK)input_mode_add(in,snapshot,note==92?1:-1,now);
    else if(bank->assignment==BA_EFFECT&&bank->effects_slot<EFFECT_SLOTS&&snapshot->effects_available&&snapshot->effects.serial==bank->selected.serial){
     const EffectsCopy *e=&snapshot->effects;unsigned pages=(e->slots[bank->effects_slot].presentation_count+7)/8;
     if(note==92&&bank->effects_page+1<pages)bank->effects_page++;else if(note==91&&bank->effects_page)bank->effects_page--;
     memset(in->effects,0,sizeof(in->effects));memset(s->wire_valid,0,sizeof(s->wire_valid));
    }
   }else surface_jog(in,snapshot,9,note,1,now,0);
  }else if(!down){surface_jog(in,snapshot,9,note,0,now,0);s->mini_navigation_locked&=~bit;}
  s->general_down[note]=down;return 1;
 }
 if(note==87||note==88||note==86||note==93||note==94||note==95){
  if(down&&!s->general_down[note]){
   mini_consume_modifiers(s);
   if(!(s->general_down[84]&&s->general_down[85])){
    if(note==93||note==94)input_stop_button(in,snapshot,note,1,now);
    else input_global_add(in,snapshot,note==87?GLOBAL_UNDO:note==88?GLOBAL_REDO:note==86?GLOBAL_LOOP_TOGGLE:GLOBAL_RECORD_TOGGLE,0,now);
   }
  }else if(!down&&(note==93||note==94))input_stop_button(in,snapshot,note,0,now);
  s->general_down[note]=down;return 1;
 }
 return 0;
}
static void mini_send_encoder(MirrorInput *in,const MirrorBank *bank,const CopiedMirror *snapshot,unsigned strip,int delta,int push,uint32_t now){
 if(push)return;
 if(strip<4)input_control_bound(in,bank,snapshot,strip,CF_SEND1+strip,delta,0,now,&bank->selected,1);
 else{const CopiedTrack *ret=bank_return(snapshot,strip-4);if(ret){MotorIdentity identity=bank_identity(snapshot,ret);input_control_bound(in,bank,snapshot,strip,CF_VOLUME,delta,0,now,&identity,2);}}
}
static unsigned mini_field_ring(const CopiedMirror *snapshot,const CopiedTrack *track,unsigned field){
 if(!track)return 0;
 MIRROR_FIELD(volume);const CopiedField *value=copied_field(snapshot,track,field,&volume);
 if(!value||!value->available||(field==CF_MIDI_VOLUME?!midi_volume(track->vptr):!mixable(track->vptr)))return 0;
 float x;memcpy(&x,&value->bits,4);return isfinite(x)&&x>=0&&x<=1?(unsigned)(x*10+0.5f)+1+(field==CF_PAN&&x==0.5f?64:0):0;
}
static int mini_output(Surface *s,const MirrorBank *bank,const CopiedMirror *snapshot,uint32_t now){
 unsigned ready=bank->ready&&snapshot->ready&&snapshot->alive&&!snapshot->error;
 for(unsigned i=0;i<8;i++){
  unsigned value=0;
  if(ready){
   if(s->general_down[84])value=i==mini_mode(bank)?127:0;
   else if(s->general_down[85])value=i==s->mini_button_mode?127:0;
   else{const CopiedTrack *track=input_track(snapshot,bank->strips+i);if(track){unsigned field=mini_button_field(s->mini_button_mode);if(s->mini_button_mode==MINI_SELECT)value=snapshot->selection.available&&snapshot->selected_serial==track->serial?127:0;else if((field==CF_ARM||mixable(track->vptr))&&track->fields[field].available)value=track->fields[field].bits?127:0;}}
  }
  unsigned note=mini_top_notes[i];if(!s->mode_valid||s->mode_led[note]!=value){snd_seq_event_t e;snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,note,value);if(!channel_send(s,&e))return 0;s->mode_led[note]=value;}
 }
 unsigned playing=ready&&snapshot->playing.available,record=ready&&snapshot->record_mode.available&&(snapshot->record_mode.bits==1||snapshot->record_mode.bits==3);
 unsigned values[8]={0,0,ready&&SURFACE_VALUE(s,input).jog_rewind?127:0,ready&&SURFACE_VALUE(s,input).jog_forward?127:0,ready&&snapshot->loop.available&&snapshot->loop.bits?127:0,playing&&!snapshot->playing.bits?127:0,playing&&snapshot->playing.bits?127:0,record?127:0};
 if(surface_sampler_view(s))for(unsigned i=0;i<3;i++)values[5+i]=ready?sampler_transport_led(snapshot,93+i):0;
 for(unsigned i=0;i<8;i++){unsigned note=mini_bottom_notes[i];if(!s->mode_valid||s->mode_led[note]!=values[i]){snd_seq_event_t e;snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,note,values[i]);if(!channel_send(s,&e))return 0;s->mode_led[note]=values[i];}}
 s->mode_valid=1;
 unsigned strip=s->next_wire++%8,ring=0;
 if(ready){
  if(bank->assignment==BA_QLINK){const QLinkSlot *q=input_qlink_slot(snapshot,bank_qlink_index(bank,snapshot,strip,now),now);if(q){float x;memcpy(&x,&q->bits,4);if(isfinite(x)&&x>=0&&x<=1)ring=(unsigned)(x*10+0.5f)+1;}}
  else if(bank->assignment==BA_EFFECT){const EffectsCopy *e=&snapshot->effects;const EffectParameter *p=e->parameters+strip;if(snapshot->effects_available&&e->status==EF_READY&&e->serial==bank->selected.serial&&e->slot==bank->effects_slot&&e->page==bank->effects_page&&e->slot<EFFECT_SLOTS&&p->status==EF_READY&&p->tick<=now&&now-p->tick<EFFECT_FRESH_MS){float x;memcpy(&x,&p->bits,4);if(isfinite(x)&&x>=0&&x<=1)ring=(unsigned)(x*10+0.5f)+1;}}
  else if(bank->assignment==BA_MINI_SEND)ring=mini_field_ring(snapshot,strip<4?input_track(snapshot,&bank->selected):bank_return(snapshot,strip-4),strip<4?CF_SEND1+strip:CF_VOLUME);
  else ring=mini_field_ring(snapshot,input_track(snapshot,bank->strips+strip),surface_encoder_field(s,bank,strip));
 }
 if(!s->wire_valid[strip]||s->wire[strip].ring!=ring){snd_seq_event_t e;snd_seq_ev_clear(&e);snd_seq_ev_set_controller(&e,0,0x30+strip,ring);if(!channel_send(s,&e))return 0;s->wire[strip].ring=ring;s->wire_valid[strip]=1;}
 return 1;
}
