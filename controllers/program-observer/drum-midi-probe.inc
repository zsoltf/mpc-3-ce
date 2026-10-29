/* Included after existing mirror/lane owners. No native invocation, output,
 * polling or allocation. Native addresses are read only at their live seams. */
#ifdef MODEL_MIDI_PROBE
typedef struct {uint32_t id,root,parent,site,sp,epoch,track,owner,program,program_owner,note,channel,mode,source;} DrumMidiFrame;
static __thread DrumMidiFrame drum_midi_frames[DMP_FRAMES] __attribute__((tls_model("initial-exec")));
static __thread DrumMidiFrame drum_midi_last_root __attribute__((tls_model("initial-exec")));
static __thread unsigned drum_midi_depth __attribute__((tls_model("initial-exec")));
static void drum_midi_fail(unsigned why){
 atomic_store_explicit(&command_state->drum_midi.error,why,memory_order_release);
 atomic_store_explicit(&command_state->drum_midi.until,0,memory_order_release);
}
static int drum_midi_active(void){
 unsigned until=atomic_load_explicit(&command_state->drum_midi.until,memory_order_acquire);
 if(until&&atomic_load_explicit(&mirror_state->heartbeat,memory_order_acquire)>=until){atomic_store(&command_state->drum_midi.until,0);return 0;}
 return until&&atomic_load_explicit(&mirror_state->heartbeat,memory_order_acquire)<until&&
  atomic_load_explicit(&observer_running,memory_order_seq_cst)&&!atomic_load(&command_state->drum_midi.error);
}
static uint32_t drum_midi_emit(Context *c,unsigned site,const DrumMidiFrame *f,const uint32_t payload[23],int opening){
 DrumMidiProbe *d=&command_state->drum_midi;uint32_t at=atomic_load(&d->count);unsigned got=0;
 for(unsigned i=0;i<4;i++){
  if(at>=DMP_RECORDS){drum_midi_fail(DMP_FULL);return 0;}
  if(processor_cas(&d->count,&at,at+1)){got=1;break;}
 }
 if(!got){drum_midi_fail(DMP_WRITER);return 0;}
 /* command_initialize precedes load-bias publication at observer startup.
  * Stamp the admitted runtime bias with the first record, not that early zero. */
 if(!at)atomic_store_explicit(&d->bias,observer_image_bias,memory_order_release);
 uint32_t id=at+1,w[DMP_WORDS]={site,atomic_load(&mirror_state->heartbeat),token(),atomic_load(&mirror_state->epoch),source_sp(c),
  f?(f->root?f->root:opening?id:0):0,f?(opening?id:f->id):0,f?f->parent:0};
 for(unsigned i=0;i<23;i++)w[8+i]=payload[i];
 DrumMidiRecord *r=d->records+at;
 for(unsigned i=0;i<DMP_WORDS;i++)atomic_store_explicit(r->words+i,w[i],memory_order_relaxed);
 atomic_store_explicit(&r->sequence,id,memory_order_release);return id;
}
static int drum_midi_track(DrumMidiFrame *f,uint32_t track){
 unsigned owner=channel_owner_id(track);
 if(!owner||!typed(track,MODEL_TRACK_VPTR)||track>UINT32_MAX-0x648u)return 0;
 uint32_t program=word(track+0x648u);unsigned po=channel_owner_id(program);
 if(!po||!pointer(program))return 0;
 f->track=track;f->owner=owner;f->program=program;f->program_owner=po;return 1;
}
static int drum_midi_frame_live(const DrumMidiFrame *f){
 if(f->epoch!=atomic_load(&mirror_state->epoch)){drum_midi_fail(DMP_EPOCH);return 0;}
 if(f->track&&(channel_owner_id(f->track)!=f->owner||channel_owner_id(f->program)!=f->program_owner||word(f->track+0x648u)!=f->program)){
  drum_midi_fail(DMP_IDENTITY);return 0;
 }
 return 1;
}
static void drum_midi_track_words(const DrumMidiFrame *f,uint32_t p[23]){
 p[0]=f->track;p[1]=f->owner;p[2]=f->program;p[3]=f->program_owner;p[21]=word(f->track+0x3ccu);p[22]=word(f->program);
}
static int drum_midi_pad(const DrumMidiFrame *f,unsigned note,uint32_t p[23]){
 p[8]=UINT32_MAX;
 if(!typed(f->program,MODEL_DRUMPROGRAM_VPTR))return 1;
 if(note>=128||f->program>UINT32_MAX-0x1ff4u)return 0;
 unsigned index=word(f->program+0x1370u+note*4u);p[8]=index;
 /* Native unmapped/special values are not Instrument identities. */
 if(index>=PAD_SLOTS)return 1;
 PadParent *parent=pad_parent(f->program,0);if(!parent)return 0;
 unsigned rev=atomic_load_explicit(&parent->revision,memory_order_acquire);
 unsigned member=atomic_load(&parent->slots[index].owner),generation=atomic_load(&parent->slots[index].generation);
 if((rev&1)||!member||member>PAD_OWNERS||!generation)return 0;
 PadOwner *o=mirror_state->pads.owners+member-1;
 uint32_t instrument=atomic_load(&o->address),a=word(f->program+0x1fecu),b=word(f->program+0x1ff0u),cap=word(f->program+0x1ff4u);
 if(!atomic_load(&o->live)||!atomic_load(&o->complete)||atomic_load(&o->incarnation)!=member||atomic_load(&o->parent)!=f->program||atomic_load(&o->parent_owner)!=f->program_owner||
  !pointer(a)||b<a||cap<b||(b-a)%4||(cap-a)%4||(cap-a)/4>PAD_SLOTS||index>=(b-a)/4||word(a+index*4)!=instrument||!typed(instrument,MODEL_INSTRUMENT_VPTR)||
  word(instrument+MODEL_INSTRUMENT_PARENT)!=f->program||word(instrument+MODEL_INSTRUMENT_SLOT)!=index)return 0;
 p[9]=instrument;p[10]=member;p[11]=generation;
 p[12]=*(const volatile unsigned char*)(uintptr_t)(instrument+0x170u);
 p[13]=*(const volatile unsigned char*)(uintptr_t)(instrument+0x3cu);
 p[14]=word(instrument+0x13a0u);p[15]=word(instrument+0x1374u);
 atomic_thread_fence(memory_order_acquire);
 return rev==atomic_load(&parent->revision)&&member==atomic_load(&parent->slots[index].owner)&&atomic_load(&o->live);
}
static void drum_midi_point(Context *c,unsigned site,unsigned lane){
 DrumMidiFrame *top=drum_midi_depth?drum_midi_frames+drum_midi_depth-1:NULL;
 uint32_t p[23]={0},sp=source_sp(c),epoch=atomic_load(&mirror_state->epoch);
 unsigned opening=site==DMP_ROOT||site==DMP_MAP||site==DMP_SEND||site==DMP_DUE||site==DMP_OFF_ROOT||site==DMP_DRAIN||site==DMP_CLEANUP;
 unsigned closing=site==DMP_ROOT_END||site==DMP_MAP_END||site==DMP_SEND_END||site==DMP_DUE_END||site==DMP_OFF_END||site==DMP_DRAIN_END||site==DMP_CLEANUP_END;
 if(top&&!drum_midi_frame_live(top))return;
 if(opening){
  if((site==DMP_MAP||site==DMP_SEND)&&!top){drum_midi_fail(DMP_UNASSOCIATED);return;}
  if(drum_midi_depth==DMP_FRAMES){drum_midi_fail(DMP_NESTING);return;}
  DrumMidiFrame f={.root=top?top->root:0,.parent=top?top->id:0,.site=site,.sp=sp,.epoch=epoch};
  if(site==DMP_ROOT||site==DMP_MAP||site==DMP_SEND||site==DMP_OFF_ROOT||site==DMP_CLEANUP){
   uint32_t track=site==DMP_CLEANUP?c->r[0]:site==DMP_ROOT||site==DMP_OFF_ROOT?c->r[4]:c->r[5];
   if(!drum_midi_track(&f,track)){drum_midi_fail(DMP_IDENTITY);return;}
   drum_midi_track_words(&f,p);
  }
  if(site==DMP_ROOT||site==DMP_MAP||site==DMP_SEND){
   uint32_t e=c->r[2];if(!pointer(e)||e>UINT32_MAX-0x38u){drum_midi_fail(DMP_IDENTITY);return;}
   p[4]=word(e+8);p[5]=*(const volatile unsigned char*)(uintptr_t)(e+0x30);p[6]=word(e+0x10);p[7]=e;
   if(site==DMP_ROOT){p[8]=word(sp+0x138);p[9]=word(sp+0x13c);p[10]=c->r[5];p[11]=c->r[6];p[12]=word(sp+0x134);
    f.note=p[5];f.channel=(p[4]>>12)&15;f.mode=p[8];f.source=p[9];memset(&drum_midi_last_root,0,sizeof(drum_midi_last_root));}
   if(site==DMP_MAP){
    /* Cleanup can traverse the mapper with controller events (sustain reset).
     * Only native type3 has a note to resolve to an Instrument. */
    p[8]=UINT32_MAX;
    if((p[4]&255u)==3u&&!drum_midi_pad(&f,p[5],p)){drum_midi_fail(DMP_IDENTITY);return;}
   }
   if(site==DMP_SEND){p[8]=c->r[0];p[9]=word(c->r[0]+0x18);p[10]=c->r[1];p[11]=c->r[3];}
  }else if(site==DMP_DUE||site==DMP_DRAIN){
   uint32_t e=site==DMP_DUE?c->r[0]:c->r[3];
   if(!pointer(e)||e>UINT32_MAX-0x58u){drum_midi_fail(DMP_IDENTITY);return;}
   p[0]=c->r[site==DMP_DUE?8:10];for(unsigned i=0;i<22;i++)p[i+1]=word(e+4*i);
  }else if(site==DMP_CLEANUP){
   /* Native source-wide cleanup can emit offs without the Sequencer's
    * targeted-off helper. Retain its Track identity, not a guessed pad. */
   p[4]=c->r[10];p[5]=c->r[11];
  }else if(site==DMP_OFF_ROOT){
   p[4]=c->r[2];p[5]=c->r[3];p[6]=c->r[1];p[7]=c->r[6];p[8]=word(sp+0x40);p[9]=word(sp+0x48);
  }
  f.id=drum_midi_emit(c,site,&f,p,1);if(!f.id)return;if(!f.root)f.root=f.id;
  drum_midi_frames[drum_midi_depth++]=f;
 }else if(closing){
  unsigned expected=site==DMP_ROOT_END?DMP_ROOT:site==DMP_MAP_END?DMP_MAP:site==DMP_SEND_END?DMP_SEND:site==DMP_DUE_END?DMP_DUE:site==DMP_DRAIN_END?DMP_DRAIN:site==DMP_CLEANUP_END?DMP_CLEANUP:DMP_OFF_ROOT;
  if(!top||top->site!=expected||top->sp!=sp){drum_midi_fail(DMP_NESTING);return;}
  if(site==DMP_MAP_END){uint32_t e=sp+0xb8;p[0]=*(const volatile unsigned char*)(uintptr_t)(sp+0xf0);if(p[0]){p[1]=word(e+8);p[2]=*(const volatile unsigned char*)(uintptr_t)(e+0x30);p[3]=word(e+0x10);}}
  drum_midi_emit(c,site,top,p,0);
  if(site==DMP_ROOT_END)drum_midi_last_root=*top;
  drum_midi_depth--;
 }else if(site==DMP_PENDING){
  if(!drum_midi_last_root.id||drum_midi_last_root.sp!=sp||drum_midi_last_root.track!=c->r[4]||!drum_midi_frame_live(&drum_midi_last_root)){
   drum_midi_fail(DMP_UNASSOCIATED);return;
  }
  uint32_t e=c->r[1];if(!pointer(e)||e>UINT32_MAX-0x58u){drum_midi_fail(DMP_IDENTITY);return;}
  p[0]=c->r[5];for(unsigned i=0;i<22;i++)p[i+1]=word(e+4*i);
  if((p[3]&255)!=3||p[21]!=drum_midi_last_root.track||(p[17]&255)!=drum_midi_last_root.note||p[18]!=drum_midi_last_root.source||p[19]!=drum_midi_last_root.mode||p[20]!=drum_midi_last_root.channel){drum_midi_fail(DMP_UNASSOCIATED);return;}
  drum_midi_emit(c,site,&drum_midi_last_root,p,0);memset(&drum_midi_last_root,0,sizeof(drum_midi_last_root));
 }else if(site==DMP_ON_BYTES||site==DMP_OFF_BYTES){
  p[0]=c->r[1];p[1]=c->r[2];p[2]=c->r[3];p[3]=c->r[4];
  p[4]=c->r[site==DMP_ON_BYTES?6:5];p[5]=c->r[site==DMP_ON_BYTES?7:6];
  drum_midi_emit(c,site,top,p,0);
  if(!top)drum_midi_fail(DMP_UNASSOCIATED);
 }else if(site==DMP_ON_SINK||site==DMP_OFF_SINK){
  p[0]=c->r[0];p[1]=c->r[1];p[2]=c->r[2];p[3]=c->r[9];
  if(!pointer(c->r[0])||c->r[0]!=c->r[8]){drum_midi_fail(DMP_IDENTITY);return;}
  /* Native dispatch owns this live sink. Vptr/target may belong to another
   * module: copy opaque identities, never follow them or call the target. */
  p[4]=word(c->r[0]);p[5]=c->r[8];
  drum_midi_emit(c,site,top,p,0);
  if(!top)drum_midi_fail(DMP_UNASSOCIATED);
 }
 atomic_store_explicit(command_state->drum_midi.open+lane,drum_midi_depth,memory_order_release);
}
/* Shared Property observation is called before source-key early returns. It
 * extends that existing hook; the instruction is never patched a second time. */
static void drum_midi_mute(Context *c,unsigned lane){
 uint32_t property=c->r[5],p[23]={0};PadOwner *o=NULL;unsigned offset=0;
 const unsigned offsets[2]={0x14,0x148};
 for(unsigned i=0;i<2;i++)if(property>=offsets[i]){
  PadOwner *candidate=pad_owner(property-offsets[i]);
  if(candidate&&atomic_load_explicit(&candidate->live,memory_order_acquire)&&atomic_load(&candidate->complete)){
   if(o){drum_midi_fail(DMP_IDENTITY);return;}o=candidate;offset=offsets[i];
  }
 }
 if(!o)return;
 p[0]=property;p[1]=c->r[7]&255;p[2]=atomic_load(&o->address);p[3]=atomic_load(&o->incarnation);
 p[4]=atomic_load(&o->parent);p[5]=atomic_load(&o->parent_owner);p[6]=offset;
 if(channel_owner_id(p[4])!=p[5]||!typed(p[2],MODEL_INSTRUMENT_VPTR)){drum_midi_fail(DMP_IDENTITY);return;}
 (void)lane;drum_midi_emit(c,DMP_MUTE,NULL,p,0);
}
static int drum_midi_dispatch(Context *c,unsigned site){
 unsigned own=(site>=DMP_ROOT&&site<=DMP_OFF_END)||(site>=DMP_DRAIN&&site<=DMP_CLEANUP_END);
 if(!own&&site!=M_MUTE)return 0;
 if(!drum_midi_active())return own;
 if(!own){uint32_t p=c->r[5];PadOwner *a=p>=0x14?pad_owner(p-0x14):NULL,*b=p>=0x148?pad_owner(p-0x148):NULL;
  if((!a||!atomic_load(&a->live))&&(!b||!atomic_load(&b->live)))return 0;
 }
 /* Passive capture never enrolls a thread or turns its loss into music failure. */
 unsigned lane=local_lane?local_lane-1:COMMAND_LANES;
 if(lane>=COMMAND_LANES){drum_midi_fail(DMP_UNENROLLED);return own;}
 if(atomic_load_explicit(lane_in_hook+lane,memory_order_seq_cst)){drum_midi_fail(DMP_REENTRY);return own;}
 atomic_store(command_state->drum_midi.active+lane,1);
 if(!drum_midi_active()){atomic_store(command_state->drum_midi.active+lane,0);return own;}
 unsigned entered_lane=trace_enter(site);if(entered_lane>=COMMAND_LANES){atomic_store(command_state->drum_midi.active+lane,0);drum_midi_fail(DMP_WRITER);return own;}
 if(drum_midi_active()){if(own)drum_midi_point(c,site,lane);else drum_midi_mute(c,lane);}
 trace_leave(lane);atomic_store(command_state->drum_midi.active+lane,0);return own;
}
#endif
