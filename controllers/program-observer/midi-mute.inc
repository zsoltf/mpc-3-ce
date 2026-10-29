/* Exact 3.10 native causal source and transport accounting. Included after
 * pad/channel owners and the existing qualified UI/audio queue implementation. */
#ifdef MODEL_MIDI_MUTE
#include "drum-midi-probe-state.h"
#include "midi-mute-accounting.h"
enum {MM_ON_RETURN=360,MM_OFF_RETURN,MM_ROUTES_REPLACE,MM_ROUTES_CLEAR,MM_INSTRUMENT_MAP};
typedef struct {
 uint32_t site,sp,id,epoch,track,owner,program,program_owner,note,channel,source,mode;
 uint32_t map_track,map_note,pads,member[MM_PADS],instrument[MM_PADS],generation[MM_PADS];
 uint32_t status,output_note,velocity,enqueuer,offset,route,slot,release[MM_BUCKET_LIMIT],releases,cleanup_sink;
 MidiMuteKey key;
} MidiMuteFrame;
static __thread MidiMuteFrame midi_mute_frames[16],midi_mute_last __attribute__((tls_model("initial-exec")));
static __thread unsigned midi_mute_depth,midi_mute_skip,midi_mute_internal,midi_mute_internal_returned __attribute__((tls_model("initial-exec")));
static __thread uint32_t midi_mute_stamp_check,midi_mute_stamp,midi_mute_offset,midi_mute_stamp_failed __attribute__((tls_model("initial-exec")));
static void midi_mute_no_sink(uint32_t,uint32_t,uint32_t);
static void midi_mute_internal_point(Context *c,unsigned site){
 if(site==MM_OFF_RETURN)midi_mute_internal_returned=1;
 if(site==DMP_OFF_SINK&&midi_mute_stamp_check){
  uint32_t message=c->r[2];
  if(!pointer(message)||(midi_mute_stamp_check==1?word(message+0x18)!=midi_mute_stamp:word(message+0x18)<midi_mute_stamp)||word(message+0x1c)!=midi_mute_offset){
   midi_mute_stamp_failed=1;c->r[9]=(uint32_t)(uintptr_t)midi_mute_no_sink;
  }
 }
}
/* Publication is diagnostic context, not route validity: native uint32 keys
 * retain their native meaning across unrelated port discovery. Each Off uses
 * the current MidiDevices lookup, never a cached output-port pointer. */
static uint32_t midi_mute_serial,midi_mute_route_generation=1,midi_mute_sink_identity;
static _Atomic uint32_t midi_mute_audio_token,midi_mute_flight,midi_mute_dirty_any;
static TransportJob midi_mute_job;
static int midi_mute_native_emit(uint32_t engine,uint32_t holder,const MidiMuteEntry *entry){
 if(!typed(engine,MODEL_MIDI_ENGINE_VPTR))return 0;
 uint32_t audio=word(engine+0x30cu),clock=word(engine+0x110u),sink=word(engine+0x198u);
 if(!typed(audio,MODEL_MIDI_AUDIO_VPTR)||!pointer(clock)||!typed(sink,MODEL_MIDI_SINK_VPTR))return 0;
 uint32_t time=word(audio+0x3b0u),queue=word(holder),route=entry->route;unsigned channel=entry->status&15,note=entry->note;
 if(!typed(time,MODEL_MIDI_TIME_VPTR)||!pointer(queue)||word(sink+0x24u)!=queue||word(engine+0xfcu)!=queue||channel>15||note>127)return 0;
 /* All three dependencies outlive a claimed Editor callback. This is a
  * callback-local native value, never a retained Program or Instrument. */
 _Alignas(8) uint32_t enqueuer[4];
 typedef void (*Ctor)(void*,uint32_t,uint32_t,uint32_t);
 typedef void (*Off)(void*,uint32_t,uint32_t,uint32_t,uint32_t);
 typedef void (*Dtor)(void*);
 midi_mute_internal++;midi_mute_stamp=entry->timestamp;midi_mute_stamp_check=2;
 ((Ctor)(uintptr_t)(observer_image_bias+MODEL_MIDI_ENQUEUER_CTOR))(enqueuer,clock,time+0x20u,sink);
 /* Live input can precede this queue callback in the SAME audio block. Let
  * native timing construct offset0 first, withholding only its sink if it
  * would sort before the emitted On. One retry uses that On's native offset;
  * both attempts are checked against its actual native packet timestamp. */
 for(unsigned attempt=0;attempt<2;attempt++){
  midi_mute_internal_returned=midi_mute_stamp_failed=0;midi_mute_offset=attempt?entry->sample_offset:0;
  ((Off)(uintptr_t)(observer_image_bias+MODEL_MIDI_NOTE_OFF))(enqueuer,midi_mute_offset,route,channel,note);
  if(midi_mute_internal_returned&&!midi_mute_stamp_failed)break;
  if(!midi_mute_stamp_failed)break;
 }
 ((Dtor)(uintptr_t)(observer_image_bias+MODEL_MIDI_ENQUEUER_DTOR))(enqueuer);
 midi_mute_stamp_check=0;midi_mute_internal--;return midi_mute_internal_returned&&!midi_mute_stamp_failed;
}
/* The current native On invocation owns this enqueuer through its sink call.
 * Use only its admitted identity off mapper and its current sample offset;
 * never resurrect an enqueuer retained from an earlier emission. */
static int midi_mute_current_off(const MidiMuteFrame *f,const MidiMuteEntry *e,uint32_t message){
 uint32_t enqueuer=f->enqueuer;
 if((!typed(enqueuer,MODEL_MIDI_CURRENT_ENQUEUER_VPTR)&&!typed(enqueuer,MODEL_MIDI_DEFAULT_ENQUEUER_VPTR))||
    word(word(enqueuer)+0xcu)!=observer_image_bias+MODEL_MIDI_OFF_MAPPER||word(enqueuer+0xcu)!=e->sink||
    !pointer(message)||word(message+0x1cu)!=f->offset||word(message+0x18)<e->timestamp)return 0;
 typedef void (*Off)(void*,uint32_t,uint32_t,uint32_t,uint32_t);
 midi_mute_internal++;midi_mute_internal_returned=0;midi_mute_stamp_failed=0;midi_mute_stamp=word(message+0x18);midi_mute_offset=f->offset;midi_mute_stamp_check=1;
 ((Off)(uintptr_t)(observer_image_bias+MODEL_MIDI_NOTE_OFF))((void*)(uintptr_t)enqueuer,f->offset,e->route,e->status&15,e->note);
 midi_mute_stamp_check=0;midi_mute_internal--;return midi_mute_internal_returned&&!midi_mute_stamp_failed;
}
static void midi_mute_before_on(const MidiMuteFrame *f,const MidiMuteEntry *incoming,uint32_t message){
 for(unsigned i=0;i<incoming->pads;i++){
  unsigned member=incoming->member[i];
  if(!(atomic_load_explicit(midi_mute_dirty+(member-1)/32,memory_order_acquire)&(1u<<((member-1)%32))))continue;
  uint32_t node=midi_mute_ledger.pad_heads[member-1];unsigned visited=0;
  while(node&&visited++<MIDI_MUTE_CAPACITY){unsigned id=(node-1)/MM_PADS+1,index=(node-1)%MM_PADS;MidiMuteEntry *e=midi_mute_entry(id);uint32_t next=e->pad_next[index];
   if(e->state==MM_ACTIVE&&e->epoch==incoming->epoch&&e->generation[index]!=incoming->generation[i]&&
      e->sink==incoming->sink&&e->route==incoming->route&&(e->status&15)==(incoming->status&15)&&e->note==incoming->note){
    if(midi_mute_current_off(f,e,message))midi_mute_retire(id);
    else {midi_mute_fault(MM_NATIVE);midi_mute_deactivate(id,MM_ABANDONED);atomic_fetch_add(&midi_mute_projection()->ordering_lost,1);}
   }node=next;
  }
 }
}
static void midi_mute_initialize(void){
 midi_mute_account_initialize();memset(midi_mute_native_value,0,sizeof(midi_mute_native_value));memset(midi_mute_dirty,0,sizeof(midi_mute_dirty));
 atomic_store(&midi_mute_audio_token,0);atomic_store(&midi_mute_flight,0);atomic_store(&midi_mute_dirty_any,0);
 midi_mute_depth=midi_mute_skip=midi_mute_serial=midi_mute_sink_identity=0;midi_mute_route_generation=1;
}
static int midi_mute_is_internal(void){return midi_mute_internal!=0;}
static int midi_mute_quiescent(void){return !atomic_load_explicit(&midi_mute_flight,memory_order_acquire);}
static void midi_mute_editor_retired(uint32_t editor){
 if(atomic_load(&midi_mute_flight)&&midi_mute_job.editor==editor){atomic_store(&midi_mute_projection()->pending,0);atomic_store_explicit(&midi_mute_flight,0,memory_order_release);}
}
static void midi_mute_audio(void *opaque){
 TransportJob job=*(TransportJob*)opaque;
 if(!transport_active(&job))goto done;
 if(token()!=atomic_load(&midi_mute_audio_token)){midi_mute_fault(MM_LANE);goto done;}
 atomic_exchange_explicit(&midi_mute_dirty_any,0,memory_order_acq_rel);
 for(unsigned w=0;w<PAD_OWNERS/32;w++){
  uint32_t bits=atomic_exchange_explicit(midi_mute_dirty+w,0,memory_order_acq_rel);
  while(bits){unsigned b=(unsigned)__builtin_ctz(bits),member=w*32+b+1;bits&=bits-1;
   uint32_t generation=(atomic_load_explicit(midi_mute_native_value+member-1,memory_order_acquire)>>2),node=midi_mute_ledger.pad_heads[member-1];unsigned visited=0;
   while(node&&visited++<MIDI_MUTE_CAPACITY){unsigned id=(node-1)/MM_PADS+1,index=(node-1)%MM_PADS;MidiMuteEntry *e=midi_mute_entry(id);uint32_t next=e->pad_next[index];
    if(e->state==MM_ACTIVE&&e->epoch==job.epoch&&e->generation[index]!=generation){
     if(!transport_active(&job))goto done;
     if(e->sink!=word(job.root+0x198u)){midi_mute_fault(MM_ROUTE_CHANGED);midi_mute_deactivate(id,MM_ABANDONED);atomic_fetch_add(&midi_mute_projection()->route_lost,1);node=next;continue;}
     if(!midi_mute_native_emit(job.root,job.holder,e)){midi_mute_fault(MM_NATIVE);atomic_fetch_or(midi_mute_dirty+w,bits|(1u<<b));atomic_store(&midi_mute_dirty_any,1);goto done;}
     midi_mute_retire(id);
    }node=next;
   }
  }
 }
done:atomic_store(&midi_mute_projection()->pending,0);atomic_store_explicit(&midi_mute_flight,0,memory_order_release);
}
static void midi_mute_service(void){
 if(!atomic_load_explicit(&midi_mute_dirty_any,memory_order_acquire)||atomic_load(&midi_mute_flight)||!atomic_load(&midi_mute_audio_token)||!atomic_load(&observer_running)||!root||!project)return;
 unsigned dirty=0;for(unsigned i=0;i<PAD_OWNERS/32;i++)if(atomic_load_explicit(midi_mute_dirty+i,memory_order_acquire)){dirty=1;break;}if(!dirty)return;
 if(!ready||loading||depth||reset_pending||root<0xd0||!command_queues||atomic_load(&mirror_state->meters.closing)||atomic_load(&command_state->stop_requested)||atomic_load(&command_state->new_project_intent))return;
 GeneralState *s=&mirror_state->general;uint32_t editor=atomic_load(&s->editor),owner=atomic_load(&s->editor_owner);
 if(!owner||channel_owner_id(editor)!=owner||atomic_load(&s->editor_root)!=root||editor>UINT32_MAX-0x380||!typed(editor,MODEL_EDITOR_VPTR)||!typed(editor+0x230,MODEL_EDITOR_SECONDARY_VPTR)||word(editor+0x37c)!=root||word(root+MODEL_ROOT_PROJECT)!=project||word(root+MODEL_ROOT_QUEUES)!=command_queues||word(root-0xd0+MODEL_APP_EDITOR)!=editor||word(editor+0x2c8)!=command_queues)return;
 midi_mute_job=(TransportJob){.epoch=epoch,.root=root,.project=project,.project_owner=channel_owner_id(project),.editor=editor,.editor_owner=owner,.holder=editor+0x2c8u};
 if(!midi_mute_job.project_owner)return;
 atomic_store(&midi_mute_projection()->pending,1);atomic_store_explicit(&midi_mute_flight,1,memory_order_release);
 if(!transport_queue_submit(observer_image_bias,midi_mute_job.holder,&midi_mute_job,midi_mute_audio)){atomic_store(&midi_mute_flight,0);atomic_store(&midi_mute_projection()->pending,0);}
}
static void midi_mute_marker(Context *c){
 uint32_t property=c->r[5];if(property<0x148u||(c->r[7]&255)>1)return;
 PadOwner *o=pad_owner(property-0x148u);if(!o||!atomic_load(&o->live)||!atomic_load(&o->complete))return;
 unsigned member=atomic_load(&o->incarnation);if(!member||member>PAD_OWNERS)return;
 unsigned value=c->r[7]&255,previous=atomic_load_explicit(midi_mute_native_value+member-1,memory_order_acquire),generation=(previous>>2)+value;
 if(generation>0x3fffffffu)midi_mute_fault(MM_WRAP);
 /* The native Property mutex serializes commits. Bootstrap only CASes zero;
  * publishing both value and generation prevents a mute/unmute race from
  * attaching a new hit to the preceding transition. */
 atomic_store_explicit(midi_mute_native_value+member-1,(generation<<2)|1u|(value<<1),memory_order_release);
 if(!value)return;
 atomic_fetch_or_explicit(midi_mute_dirty+(member-1)/32,1u<<((member-1)%32),memory_order_release);
 atomic_store_explicit(&midi_mute_dirty_any,1,memory_order_release);
}
/* Private native qualification: preserve low-byte error category and encode
 * the FIRST failed identity branch above it. No extra shared state or polling. */
static int midi_mute_identity(unsigned detail){midi_mute_fault(MM_IDENTITY|(detail<<8));return 0;}
static int midi_mute_add_pad(MidiMuteFrame *f,uint32_t track,unsigned note,uint32_t borrowed_instrument){
 if(!typed(track,MODEL_TRACK_VPTR))return midi_mute_identity(10);
 if(!channel_owner_id(track))return midi_mute_identity(11);
 uint32_t program=word(track+0x648u);if(!typed(program,MODEL_DRUMPROGRAM_VPTR))return 1;
 unsigned po=channel_owner_id(program);if(!po)return midi_mute_identity(12);
 if(note>=128)return midi_mute_identity(13);
 unsigned index=word(program+0x1370u+note*4);if(index>=PAD_SLOTS)return 1;
 PadParent *parent=pad_parent(program,0);if(!parent)return midi_mute_identity(14);unsigned revision=atomic_load(&parent->revision),member=atomic_load(&parent->slots[index].owner);
 if(revision&1)return midi_mute_identity(15);
 if(!member||member>PAD_OWNERS)return midi_mute_identity(16);
 PadOwner *o=mirror_state->pads.owners+member-1;uint32_t instrument=atomic_load(&o->address),a=word(program+0x1fecu),b=word(program+0x1ff0u),cap=word(program+0x1ff4u);
 if(instrument!=borrowed_instrument)return midi_mute_identity(17);
 if(!pointer(a)||b<a||cap<b||(b-a)%4||(cap-a)%4||(cap-a)/4>PAD_SLOTS||index>=(b-a)/4)return midi_mute_identity(18);
 if(word(a+index*4)!=instrument)return midi_mute_identity(19);
 if(!atomic_load(&parent->slots[index].generation))return midi_mute_identity(20);
 if(atomic_load(&o->incarnation)!=member)return midi_mute_identity(21);
 if(!atomic_load(&o->live))return midi_mute_identity(22);
 if(!atomic_load(&o->complete))return midi_mute_identity(23);
 if(atomic_load(&o->parent)!=program)return midi_mute_identity(24);
 if(atomic_load(&o->parent_owner)!=po)return midi_mute_identity(25);
 if(!typed(instrument,MODEL_INSTRUMENT_VPTR))return midi_mute_identity(26);
 if(word(instrument+MODEL_INSTRUMENT_PARENT)!=program)return midi_mute_identity(27);
 if(word(instrument+MODEL_INSTRUMENT_SLOT)!=index)return midi_mute_identity(28);
 if(revision!=atomic_load(&parent->revision))return midi_mute_identity(29);
 /* Only this inner native mapper still borrows Instrument. A setter commit
  * wins over bootstrap; later sinks consume this copy without dereferencing it. */
 unsigned value=*(volatile unsigned char*)(uintptr_t)(instrument+0x170u),unknown=0;if(value>1)return midi_mute_identity(30);
 atomic_compare_exchange_strong(midi_mute_native_value+member-1,&unknown,1u|(value<<1));
 for(unsigned i=0;i<f->pads;i++)if(f->member[i]==member)return 1;
 if(f->pads==MM_PADS)return midi_mute_identity(31);
 unsigned n=f->pads++;f->member[n]=member;f->instrument[n]=instrument;return 1;
}
static void midi_mute_no_sink(uint32_t sink,uint32_t route,uint32_t message){(void)sink;(void)route;(void)message;}
/* Native PadPerform uses live mode2 only with its dedicated source token.
 * Keep all other mode2 sources outside the qualified positive path. */
static int midi_mute_live_source(uint32_t mode,uint32_t source){return mode==1||(mode==2&&source==0x7ffffffau);}
static void midi_mute_sink(Context *c,MidiMuteFrame *f,int on){
 if(!f||!f->status)return;
 if(on){
  if(!f->pads||(!midi_mute_live_source(f->mode,f->source)&&f->mode!=3)||(f->status&0xf0)!=0x90||!f->velocity)return;
  if(!typed(c->r[0],MODEL_MIDI_SINK_VPTR)||c->r[9]!=observer_image_bias+MODEL_MIDI_SINK_ENQUEUE)return;
  int muted=0;MidiMuteEntry e={.state=MM_CONSTRUCTING,.root=f->id,.epoch=f->epoch,.track=f->track,.owner=f->owner,.program=f->program,.program_owner=f->program_owner,.sink=c->r[0],.route=c->r[1],.status=f->status,.note=f->output_note,.velocity=f->velocity,.timestamp=word(c->r[2]+0x18),.sample_offset=word(c->r[2]+0x1c),.route_generation=midi_mute_route_generation,.pads=f->pads,.key=f->key};
  for(unsigned i=0;i<f->pads;i++){
   unsigned member=f->member[i];PadOwner *o=mirror_state->pads.owners+member-1;
   unsigned value=atomic_load_explicit(midi_mute_native_value+member-1,memory_order_acquire);e.member[i]=member;e.generation[i]=value>>2;
   if(!(value&1)){midi_mute_identity(1);return;}
   if(!atomic_load(&o->live)){midi_mute_identity(2);return;}
   if(atomic_load(&o->address)!=f->instrument[i]){midi_mute_identity(3);return;}
   muted|=(value&2)!=0;
  }
  if(!midi_mute_sink_identity)midi_mute_sink_identity=c->r[0];
  if(c->r[1]!=f->route||c->r[0]!=c->r[8]){midi_mute_fault(MM_ASSOCIATION);return;}
  midi_mute_before_on(f,&e,c->r[2]);
  if(muted){e.state=MM_BLOCKED;c->r[9]=(uint32_t)(uintptr_t)midi_mute_no_sink;atomic_fetch_add(&midi_mute_projection()->blocked,1);}
  f->slot=midi_mute_reserve(&e);
  if(!f->slot&&!muted)midi_mute_contaminate(&e.key);
 }else{
  uint32_t ids[MM_BUCKET_LIMIT];unsigned n=0,active=0;f->releases=0;
  if(f->site==DMP_CLEANUP){
   uint32_t h=((f->track>>2)^f->owner*2654435761u)&(MM_BUCKETS-1),id=midi_mute_ledger.sources[h];unsigned walked=0,matched=0;
   while(id&&walked++<MIDI_MUTE_CAPACITY){MidiMuteEntry *e=midi_mute_entry(id);
    if(e->track==f->track&&e->owner==f->owner&&e->sink==c->r[0]&&e->route==c->r[1]&&(e->status&15)==(f->status&15)&&e->note==f->output_note){matched=1;if(e->state==MM_ACTIVE||e->state==MM_ABANDONED||e->unmanaged)active=1;}
    id=e->source_next;
   }
   if(matched){f->cleanup_sink=c->r[0];if(!active)c->r[9]=(uint32_t)(uintptr_t)midi_mute_no_sink;}return;
  }else n=midi_mute_find(&f->key,ids);
  for(unsigned i=0;i<n;i++){MidiMuteEntry *e=midi_mute_entry(ids[i]);
   if(e->sink!=c->r[0]||e->route!=c->r[1]||(e->status&15)!=(f->status&15)||e->note!=f->output_note)continue;
   /* A refused CE choke did not end the emitted note. Its original native
    * release remains authoritative, including after a route publication. */
   if(e->state==MM_ACTIVE||e->state==MM_ABANDONED||e->unmanaged)active=1;
   f->release[f->releases++]=ids[i];
  }
  if(n&&!f->releases)midi_mute_fault(MM_ASSOCIATION);
  if(f->releases&&!active)c->r[9]=(uint32_t)(uintptr_t)midi_mute_no_sink;
 }
}
static int midi_mute_point(Context *c,unsigned site){
 unsigned own=(site>=DMP_ROOT&&site<=DMP_OFF_END)||(site>=DMP_DRAIN&&site<=DMP_CLEANUP_END)||site==MM_ON_RETURN||site==MM_OFF_RETURN||site==MM_ROUTES_REPLACE||site==MM_ROUTES_CLEAR||site==MM_INSTRUMENT_MAP;
 if(midi_mute_internal){midi_mute_internal_point(c,site);return own;}
 if(site==M_MUTE){midi_mute_marker(c);return 0;}if(!own)return 0;
 /* Only the exact command profile admits native output mutation. */
#if !defined(MODEL_MIDI_MUTE_ENABLED) || !MODEL_MIDI_MUTE_ENABLED
 return own;
#else
 uint32_t tid=token(),audio=atomic_load(&midi_mute_audio_token);
 if(!audio&&site==DMP_ROOT&&local_lane&&c->r[5]==atomic_load(&mirror_state->sequencer)){
  atomic_compare_exchange_strong(&midi_mute_audio_token,&audio,tid);audio=atomic_load(&midi_mute_audio_token);
 }
 if(tid!=audio)return own;
 if(site==MM_ROUTES_REPLACE||site==MM_ROUTES_CLEAR){if(c->r[site==MM_ROUTES_REPLACE?3:9]!=midi_mute_sink_identity)return own;midi_mute_route_generation++;return own;}
 uint32_t sp=source_sp(c);MidiMuteFrame *top=midi_mute_depth?midi_mute_frames+midi_mute_depth-1:NULL;
 unsigned begin=site==DMP_ROOT||site==DMP_MAP||site==DMP_SEND||site==DMP_DUE||site==DMP_DRAIN||site==DMP_OFF_ROOT||site==DMP_CLEANUP;
 unsigned end=site==DMP_ROOT_END||site==DMP_MAP_END||site==DMP_SEND_END||site==DMP_DUE_END||site==DMP_DRAIN_END||site==DMP_OFF_END||site==DMP_CLEANUP_END;
 if(top&&top->epoch!=atomic_load(&mirror_state->epoch)){midi_mute_identity(4);midi_mute_skip=midi_mute_depth;midi_mute_depth=0;top=NULL;}
 if(midi_mute_skip){if(begin)midi_mute_skip++;else if(end)midi_mute_skip--;return own;}

 if(site==DMP_ROOT||site==DMP_MAP||site==DMP_SEND||site==DMP_DUE||site==DMP_DRAIN||site==DMP_OFF_ROOT||site==DMP_CLEANUP){
  /* Native general-event/Stop forwarding can map and Send To outside a
   * captured note root. Preserve that subtree without inventing epoch0. */
  if((site==DMP_MAP||site==DMP_SEND)&&!top){midi_mute_skip=1;return own;}
  if(midi_mute_depth==16){midi_mute_fault(MM_NESTING);midi_mute_skip=1;return own;}
  MidiMuteFrame f=top?*top:(MidiMuteFrame){0};f.site=site;f.sp=sp;f.status=f.slot=f.releases=f.cleanup_sink=0;
  if(site==DMP_ROOT){
   memset(&midi_mute_last,0,sizeof(midi_mute_last));
   uint32_t event=c->r[2];if(!atomic_load(&mirror_state->ready)||!pointer(event)||!typed(c->r[4],MODEL_TRACK_VPTR)||!channel_owner_id(c->r[4])){midi_mute_skip=1;return own;}
   memset(&f,0,sizeof(f));f.site=site;f.sp=sp;f.epoch=atomic_load(&mirror_state->epoch);
   if(atomic_load(&midi_mute_projection()->epoch)!=f.epoch){midi_mute_account_initialize();atomic_store(&midi_mute_projection()->active,0);atomic_store(&midi_mute_projection()->retained,0);atomic_store(&midi_mute_projection()->error,0);atomic_store(&midi_mute_projection()->epoch,f.epoch);}
   f.id=++midi_mute_serial;if(!f.id)midi_mute_fault(MM_WRAP);
   f.track=c->r[4];f.owner=channel_owner_id(f.track);f.program=word(f.track+0x648u);f.program_owner=channel_owner_id(f.program);
   f.note=*(unsigned char*)(uintptr_t)(event+0x30);f.channel=(word(event+8)>>12)&15;f.mode=word(sp+0x138);f.source=word(sp+0x13c);
   if(midi_mute_live_source(f.mode,f.source))midi_mute_live_key(&f.key,f.epoch,f.owner,f.track,f.note,f.channel,f.source);
   else {f.key=(MidiMuteKey){.epoch=f.epoch,.owner=f.owner};f.key.w[0]=f.id;}
   memset(&midi_mute_last,0,sizeof(midi_mute_last));
  }else if(site==DMP_MAP){uint32_t event=c->r[2];f.map_track=c->r[5];f.map_note=(word(event+8)&255)==3?*(unsigned char*)(uintptr_t)(event+0x30):UINT32_MAX;
  }else if(site==DMP_DUE||site==DMP_DRAIN){
   uint32_t event=c->r[site==DMP_DUE?0:3],p[22];for(unsigned i=0;i<22;i++)p[i]=word(event+i*4);
   f.epoch=atomic_load(&mirror_state->epoch);midi_mute_pending_key(&f.key,f.epoch,channel_owner_id(p[20]),c->r[site==DMP_DUE?8:10],p);
  }else if(site==DMP_CLEANUP){
   f.track=c->r[0];f.owner=channel_owner_id(f.track);f.epoch=atomic_load(&mirror_state->epoch);
  }else if(site==DMP_OFF_ROOT&&!top){
   f.epoch=atomic_load(&mirror_state->epoch);midi_mute_live_key(&f.key,f.epoch,channel_owner_id(c->r[4]),c->r[4],c->r[2],c->r[3],word(sp+0x40));
  }
  midi_mute_frames[midi_mute_depth++]=f;
 }else if(site==DMP_ROOT_END||site==DMP_MAP_END||site==DMP_SEND_END||site==DMP_DUE_END||site==DMP_DRAIN_END||site==DMP_OFF_END||site==DMP_CLEANUP_END){
  unsigned expected=site==DMP_ROOT_END?DMP_ROOT:site==DMP_MAP_END?DMP_MAP:site==DMP_SEND_END?DMP_SEND:site==DMP_DUE_END?DMP_DUE:site==DMP_DRAIN_END?DMP_DRAIN:site==DMP_CLEANUP_END?DMP_CLEANUP:DMP_OFF_ROOT;
  if(!top||top->site!=expected||top->sp!=sp){midi_mute_fault(MM_NESTING);return own;}
  if(site==DMP_ROOT_END)midi_mute_last=*top;
  if(site==DMP_MAP_END&&midi_mute_depth>1){MidiMuteFrame *parent=top-1;parent->pads=top->pads;memcpy(parent->member,top->member,sizeof(top->member));memcpy(parent->instrument,top->instrument,sizeof(top->instrument));}
  midi_mute_depth--;
 }else if(site==DMP_PENDING){
  if(!midi_mute_last.id)return own;
  if(midi_mute_last.epoch!=atomic_load(&mirror_state->epoch)||midi_mute_last.sp!=sp||midi_mute_last.track!=c->r[4]){midi_mute_fault(MM_ASSOCIATION);return own;}
  uint32_t p[22];for(unsigned i=0;i<22;i++)p[i]=word(c->r[1]+i*4);MidiMuteKey key;
  if((p[2]&255)!=3||p[20]!=midi_mute_last.track||(p[16]&255)!=midi_mute_last.note||p[17]!=midi_mute_last.source||p[18]!=midi_mute_last.mode||p[19]!=midi_mute_last.channel){midi_mute_fault(MM_ASSOCIATION);return own;}
  midi_mute_pending_key(&key,midi_mute_last.epoch,midi_mute_last.owner,c->r[5],p);midi_mute_bind_pending(midi_mute_last.id,&midi_mute_last.key,&key);
 }else if(site==MM_INSTRUMENT_MAP){
  if(top&&top->site==DMP_MAP&&top->map_note<128&&!midi_mute_add_pad(top,top->map_track,top->map_note,c->r[7]))midi_mute_fault(MM_IDENTITY);
 }else if(site==DMP_ON_BYTES||site==DMP_OFF_BYTES){
  if(top){top->status=c->r[1]&255;top->output_note=c->r[2]&255;top->velocity=c->r[3]&255;top->enqueuer=c->r[4];top->offset=c->r[site==DMP_ON_BYTES?6:5];top->route=c->r[site==DMP_ON_BYTES?7:6];}
 }else if(site==DMP_ON_SINK||site==DMP_OFF_SINK)midi_mute_sink(c,top,site==DMP_ON_SINK);
 else if(site==MM_ON_RETURN&&top&&top->slot){MidiMuteEntry *e=midi_mute_entry(top->slot);if(e->state==MM_CONSTRUCTING)midi_mute_activate(top->slot);top->slot=0;}
 else if(site==MM_OFF_RETURN&&top){
  if(top->cleanup_sink){
   uint32_t h=((top->track>>2)^top->owner*2654435761u)&(MM_BUCKETS-1),id=midi_mute_ledger.sources[h];unsigned walked=0;
   while(id&&walked++<MIDI_MUTE_CAPACITY){MidiMuteEntry *e=midi_mute_entry(id);uint32_t next=e->source_next;
    if(e->track==top->track&&e->owner==top->owner&&e->sink==top->cleanup_sink&&e->route==top->route&&(e->status&15)==(top->status&15)&&e->note==top->output_note){
     if(e->key.kind==3){if(e->state==MM_ACTIVE)midi_mute_deactivate(id,MM_RETIRED);else if(e->state==MM_ABANDONED)e->state=MM_RETIRED;}else midi_mute_release(id);
    }id=next;
   }top->cleanup_sink=0;
  }
  for(unsigned i=0;i<top->releases;i++)midi_mute_release(top->release[i]);
  top->releases=0;
 }
 return own;
#endif
}
/* Reuse the existing enrolled-lane close handshake, without enrolling a
 * thread or copying unrelated command association tables on each note. */
static int midi_mute_dispatch(Context *c,unsigned site){
 unsigned own=(site>=DMP_ROOT&&site<=DMP_OFF_END)||(site>=DMP_DRAIN&&site<=DMP_CLEANUP_END)||(site>=MM_ON_RETURN&&site<=MM_INSTRUMENT_MAP);
 if(!own&&site!=M_MUTE)return 0;
 if(midi_mute_internal){midi_mute_internal_point(c,site);return own;}
 if(!local_lane||!atomic_load_explicit(&observer_running,memory_order_seq_cst))return own;
 unsigned lane=local_lane-1,held=atomic_load_explicit(lane_in_hook+lane,memory_order_seq_cst);
 if(held){if(site==M_MUTE)midi_mute_marker(c);else midi_mute_fault(MM_NESTING);return own;}
 atomic_store_explicit(lane_in_hook+lane,1,memory_order_seq_cst);
 if(!atomic_load_explicit(&observer_running,memory_order_seq_cst)){atomic_store_explicit(lane_in_hook+lane,0,memory_order_seq_cst);return own;}
 int result=midi_mute_point(c,site);
 atomic_store_explicit(lane_in_hook+lane,0,memory_order_seq_cst);return result;
}
#endif
