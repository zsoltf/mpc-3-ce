/* Absolute drum-step edits on the existing command/UI owner. Native MPC owns
 * note construction, removal, CommandManager history and cache refresh. */
#ifdef COMMAND_COMPONENT
static unsigned (*component_step_edit)(const CommandRequest*,const StepCopy*);
#endif
static StepCopy step_current;
static uint32_t step_next_tick;
static unsigned step_pending;
static _Atomic uint32_t step_hook_control,step_hook_event_owner,step_hook_epoch,step_event_generation;

static StepProjection step_projections[STEP_VIEWS];
static unsigned step_projection_cursor;
static int step_span(uint32_t p,unsigned n){return pointer(p)&&p<=UINT32_MAX-n;}
#include "step-native.inc"
#include "step-bound-clock.inc"
#include "step-bound.inc"
/* Only register values are inspected here. Native source pointers remain
 * callback-local in the UI drain; this notification never invokes native work. */
static void step_source_changed(const Context *c){
 if(!mirror_state||!observer_running||atomic_load(&command_state->step_interest.until)<=atomic_load(&mirror_state->heartbeat))return;
 uint32_t control=atomic_load_explicit(&step_hook_control,memory_order_acquire);
 if(control&&c->r[0]==control+12&&c->r[1]==atomic_load(&step_hook_event_owner)&&atomic_load(&step_hook_epoch)==atomic_load(&mirror_state->epoch))atomic_fetch_add_explicit(&step_event_generation,1,memory_order_release);
}
/* The existing native audio-owned Step timer has already obtained this local
 * ClipPlayer tick. We only copy its fixed scalar result; no getter, list walk,
 * allocation or new timer runs in this hook. */
typedef struct {_Atomic uint32_t revision,control,epoch,player,key0,key1,key_present,event_owner;} StepClockAdmission;
typedef struct {_Atomic uint32_t revision,admission,tick,lo,hi,found;} StepClockSample;
static StepClockAdmission step_clock_admission;static StepClockSample step_clock_sample;
static void step_clock_capture(Context *c){
 if(!mirror_state||!observer_running)return;
 unsigned now=atomic_load(&mirror_state->heartbeat),rev=atomic_load_explicit(&step_clock_admission.revision,memory_order_acquire);
 if(!rev||(rev&1)||atomic_load(&command_state->step_interest.until)<=now)return;
 uint32_t control=atomic_load(&step_clock_admission.control),player=atomic_load(&step_clock_admission.player),sp=source_sp(c);
 if(!control||atomic_load(&step_clock_admission.epoch)!=atomic_load(&mirror_state->epoch)||!step_span(c->r[4],8)||word(c->r[4]+4)!=control||!step_span(sp,0x40))return;
 if(word(sp+0x34)!=atomic_load(&step_clock_admission.key0)||word(sp+0x38)!=atomic_load(&step_clock_admission.key1)||(*(const unsigned char*)(uintptr_t)(sp+0x3c)!=0)!=atomic_load(&step_clock_admission.key_present))return;
 if(rev!=atomic_load_explicit(&step_clock_admission.revision,memory_order_acquire))return;
 unsigned write=atomic_load(&step_clock_sample.revision);if((write&1)||write>UINT32_MAX-2||!atomic_compare_exchange_strong(&step_clock_sample.revision,&write,write+1))return;
 atomic_store(&step_clock_sample.admission,rev);atomic_store(&step_clock_sample.tick,now);atomic_store(&step_clock_sample.lo,c->r[0]);atomic_store(&step_clock_sample.hi,c->r[1]);atomic_store(&step_clock_sample.found,c->r[3]!=player+MODEL_STEP_CLOCK_MISSING);
 atomic_store_explicit(&step_clock_sample.revision,write+2,memory_order_release);
}
static void step_clock_read(StepCopy *s){
 uint32_t sequencer=word(s->control+0xaa2c),player=step_span(sequencer,0x3f8)?word(sequencer+0x3f4):0;
 if(!pointer(player)||word(s->control+0xaa1c)!=project)return;
 uint32_t k0=word(project+0x1b4),k1=word(project+0x1b8),present=*(const unsigned char*)(uintptr_t)(project+0x1bc)!=0;
 StepClockAdmission *a=&step_clock_admission;unsigned rev=atomic_load(&a->revision);
 if(atomic_load(&a->control)!=s->control||atomic_load(&a->epoch)!=s->epoch||atomic_load(&a->player)!=player||atomic_load(&a->key0)!=k0||atomic_load(&a->key1)!=k1||atomic_load(&a->key_present)!=present||atomic_load(&a->event_owner)!=s->event_owner){
  if(rev>UINT32_MAX-2)return;
  atomic_store_explicit(&a->revision,rev+1,memory_order_release);atomic_store(&a->control,s->control);atomic_store(&a->epoch,s->epoch);atomic_store(&a->player,player);atomic_store(&a->key0,k0);atomic_store(&a->key1,k1);atomic_store(&a->key_present,present);atomic_store(&a->event_owner,s->event_owner);atomic_store_explicit(&a->revision,rev+2,memory_order_release);return;
 }
 StepClockSample *v=&step_clock_sample;unsigned q=atomic_load_explicit(&v->revision,memory_order_acquire);if(!q||(q&1))return;
 unsigned source=atomic_load(&v->admission),tick=atomic_load(&v->tick),lo=atomic_load(&v->lo),hi=atomic_load(&v->hi),found=atomic_load(&v->found);
 if(q!=atomic_load_explicit(&v->revision,memory_order_acquire)||source!=rev||!found||tick>s->tick||s->tick-tick>=STEP_FRESH_MS||(hi&0x80000000u))return;
 s->clock_available=1;s->clock_tick=tick;s->clock_value[0]=lo;s->clock_value[1]=hi;s->native_playhead=word(s->control+0xaa18);
}
static unsigned step_native_read(StepCopy *s){
 memset(s,0,sizeof(*s));s->status=STEP_UNAVAILABLE;s->epoch=epoch;s->tick=atomic_load(&mirror_state->heartbeat);
 if(!active()||!ready||loading||reset_pending||depth||!typed(root,MODEL_ROOT_VPTR)||word(root+MODEL_ROOT_PROJECT)!=project||!typed(project,MODEL_PROJECT_VPTR)||atomic_load(&mirror_state->meters.closing)||atomic_load(&command_state->stop_requested)||atomic_load(&command_state->new_project_intent))return C_NOT_READY;
 uint32_t control=word(root+MODEL_ROOT_STEP_CONTROL),context=word(root+MODEL_ROOT_STEP_CONTEXT),track=word(project+0xa0);
 if(!typed(control,MODEL_STEP_CONTROL_VPTR)||!step_span(control,0xaa38))return C_NATIVE_MEMBERSHIP;
 Track *t=NULL;for(unsigned i=0;i<count;i++)if(tracks[i].track==track){t=tracks+i;break;}
 if(!t||t->transition||!t->track_owner||!t->program_owner||!typed(track,MODEL_TRACK_VPTR)||channel_owner_id(track)!=t->track_owner||channel_owner_id(t->program)!=t->program_owner||word(track+0x648)!=t->program||!typed(t->program,MODEL_DRUMPROGRAM_VPTR)||word(t->program+0x1140)!=0)return C_NOT_DRUM;
 uint32_t clip=word(project+0x1e8);
 if(!typed(clip,MODEL_CLIP_VPTR)||word(control+0xa938)!=project+0x1c0||word(control+0x68)!=clip+MODEL_CLIP_EVENT_OWNER)return C_NATIVE_MEMBERSHIP;
 uint32_t pad=word(control+0x90),quantiser=word(control+0xa940),sequence=word(project+0xe4);
 if(pad>=128||!step_span(quantiser,0x74)||!step_span(sequence,0x239)||!step_span(word(control+0x68),0x48))return C_NATIVE_MEMBERSHIP;
 uint32_t bits=word(quantiser+0x70);float division;memcpy(&division,&bits,4);
 if(!(division>=1&&division<=3840)||division!=(unsigned)division||3840u%(unsigned)division)return C_SOURCE_CHANGED;
 s->root=root;s->control=control;s->context=context;s->sequence=sequence;s->program=t->program;s->project_owner=channel_owner_id(project);s->track_owner=t->track_owner;s->program_owner=t->program_owner;s->serial=t->serial;s->binding=t->binding;s->incarnation=t->incarnation;s->pad=pad;s->note=word(t->program+0x1170+4*pad);s->division_bits=bits;
 if(!s->project_owner||s->note>127)return C_NATIVE_MEMBERSHIP;
 for(unsigned i=0;i<4;i++)s->time[i]=word(control+0xd0+4*i);
 s->event_owner=word(control+0x68);s->extent[0]=word(sequence+0x1a8);s->extent[1]=word(sequence+0x1ac);
 if(word(track+0x508)==1&&*(const unsigned char*)(uintptr_t)(track+0x518)){s->extent[0]=word(track+0x510);s->extent[1]=word(track+0x514);}
 uint64_t extent=(uint64_t)s->extent[0]|((uint64_t)s->extent[1]<<32);if(!extent||extent>INT64_MAX)return C_SOURCE_CHANGED;
 s->status=STEP_READY;return C_OK;
}
/* Projection interest may change after dispatch. The existing binding must
 * survive until that exact request's native completion is classified. */
static int step_view_pending(unsigned view){
 if(!step_pending)return 0;
 for(unsigned at=0;at<COMMAND_SLOTS;at++)if((step_pending&(1u<<at))&&accepted[at].request.effect_slot==view+1)return 1;
 return 0;
}
static void step_project_views(StepCopy *s,const StepDemand *d){
 unsigned refreshed=0,start=step_projection_cursor;
 for(unsigned n=0;n<STEP_VIEWS;n++){
  unsigned i=(start+n)%STEP_VIEWS;StepProjection *v=step_projections+i;StepBinding *binding=step_bindings+i;
  if(step_view_pending(i)){
   if(!(d->enabled&(1u<<i))||binding->bind_generation!=d->bind_generation[i]){memset(v,0,sizeof(*v));step_bound_clock_clear(i);}
   continue;
  }
  if(!(d->enabled&(1u<<i))){if(binding->clip.object)step_bound_release(i);step_bound_attempt_epoch[i]=0;continue;}
  if(step_bound_attempt_epoch[i]!=epoch||step_bound_attempt[i]!=d->bind_generation[i]){
   /* No target was chosen yet. Wait for a selected Drum source without
    * consuming this entry; successful or invalidated bindings never retry. */
   if(s->status!=STEP_READY){if(binding->clip.object)step_bound_release(i);memset(v,0,sizeof(*v));v->status=STEP_UNAVAILABLE;continue;}
   step_bound_attempt_epoch[i]=epoch;step_bound_attempt[i]=d->bind_generation[i];
   if(step_bound_acquire(i,s,d->bind_generation[i])){memset(v,0,sizeof(*v));v->status=STEP_UNAVAILABLE;continue;}
  }
  if(!binding->clip.object){v->status=STEP_UNAVAILABLE;continue;}
  StepCopy bound;if(step_bound_resolve(i,&bound)){step_bound_release(i);v->status=STEP_UNAVAILABLE;continue;}
  unsigned pad=d->pad[i]==UINT32_MAX?binding->pad:d->pad[i],first=d->first_step[i];
  if(pad>=128||first>UINT32_MAX-63){memset(v,0,sizeof(*v));v->status=STEP_UNAVAILABLE;continue;}
  uint32_t generation=atomic_load_explicit(step_bound_generation+i,memory_order_acquire),notes[4];
  for(unsigned j=0;j<4;j++)notes[j]=pad+j<128?word(bound.program+0x1170+4*(pad+j)):UINT32_MAX;
  StepTarget target=step_bound_target(i,&bound);
  unsigned changed=v->status!=STEP_READY||v->pad!=pad||v->first_step!=first||v->generation!=generation||memcmp(v->notes,notes,sizeof(notes))||memcmp(&v->target,&target,offsetof(StepTarget,clock_available));
  if(!changed){v->tick=s->tick;continue;}
  if(refreshed)continue;
  refreshed=1;step_projection_cursor=(i+1)%STEP_VIEWS;
  StepProjection next={.target=target,.status=STEP_UNAVAILABLE,.tick=s->tick,.pad=pad,.first_step=first,.generation=generation};memcpy(next.notes,notes,sizeof(notes));
  float denominator;memcpy(&denominator,&bound.division_bits,4);unsigned division=3840u/(unsigned)denominator,tolerance=(unsigned)((float)division*0.2f);
  uint64_t extent=(uint64_t)bound.extent[0]|((uint64_t)bound.extent[1]<<32);StepCopy after;
  if(step_native_scan(bound.event_owner,notes,(int64_t)first*division,division,tolerance,64,next.on)&&generation==atomic_load_explicit(step_bound_generation+i,memory_order_acquire)&&!step_bound_resolve(i,&after)&&after.event_owner==bound.event_owner&&after.program_owner==bound.program_owner){
   next.status=STEP_READY;for(unsigned cell=0;cell<64;cell++)if((uint64_t)(first+cell)*division<extent)next.valid[cell/32]|=1u<<(cell%32);
  }
  *v=next;
 }
 for(unsigned i=0;i<STEP_VIEWS;i++){StepProjection *v=step_projections+i;v->target.clock_available=0;
  if(v->status==STEP_READY){uint32_t track=0;for(unsigned j=0;j<count;j++)if(tracks[j].track_owner==v->target.track_owner){track=tracks[j].track;break;}if(track)step_bound_clock_read(i,track,&v->target,s->tick);
   unsigned state=v->target.native_clip_state;if(state!=CLIP_PLAYING&&state!=CLIP_WILL_STOP_PLAYING&&state!=CLIP_RECORDING&&state!=CLIP_WILL_STOP_RECORDING&&state!=CLIP_WILL_OVERDUB&&state!=CLIP_OVERDUBBING&&state!=CLIP_WILL_STOP_OVERDUBBING)v->target.clock_available=0;}
 }
 memcpy(s->views,step_projections,sizeof(s->views));
 for(unsigned i=0;i<STEP_VIEWS;i++)if(s->views[i].status==STEP_READY)s->status=STEP_READY;
}
static void step_publish(const StepCopy *value){
 StepState *s=&mirror_state->steps;unsigned rev=atomic_load(&s->revision);if(rev>UINT32_MAX-2)return;
 uint32_t words[STEP_WORDS];memcpy(words,value,sizeof(words));atomic_store(&s->revision,rev+1);atomic_thread_fence(memory_order_release);
 for(unsigned i=0;i<STEP_WORDS;i++)atomic_store_explicit(s->words+i,words[i],memory_order_relaxed);
 atomic_store_explicit(&s->revision,rev+2,memory_order_release);
}
/* Requests pin the native event owner, absolute step, mapped note and effective
 * sequence/track extent. Native UI page and globally selected pad are not edit
 * targets and cannot redirect an admitted independent Launchpad view. */
static unsigned step_request_match(const CommandRequest *r,const StepCopy *s){
 if(r->effect_slot&&(r->effect_slot>STEP_VIEWS||step_bindings[r->effect_slot-1].bind_generation!=r->effect_position))return C_IDENTITY;
 if(r->epoch!=s->epoch||r->project_owner!=s->project_owner||r->track_owner!=s->track_owner||r->program_owner!=s->program_owner||r->serial!=s->serial||r->binding!=s->binding||r->incarnation!=s->incarnation||r->qlink_root!=s->control||r->qlink_wrapper!=s->event_owner||r->qlink_provider!=s->sequence||r->qlink_generation!=s->division_bits||r->pad_index>=128||r->effect_ap!=word(s->program+0x1170+4*r->pad_index)||r->effect_generation!=s->event_owner||r->effect_index!=s->extent[0]||r->effect_key!=s->extent[1])return C_IDENTITY;
 float denominator;memcpy(&denominator,&s->division_bits,4);unsigned division=3840u/(unsigned)denominator;
 uint64_t tick=(uint64_t)r->qlink_index*division,extent=(uint64_t)s->extent[0]|((uint64_t)s->extent[1]<<32);
 return tick<extent?C_OK:C_SOURCE_CHANGED;
}
static unsigned step_absolute_value(const CommandRequest *r,const StepCopy *s,unsigned *value){
 float denominator;memcpy(&denominator,&s->division_bits,4);unsigned division=3840u/(unsigned)denominator,tolerance=(unsigned)((float)division*0.2f);
 uint32_t notes[4]={r->effect_ap,UINT32_MAX,UINT32_MAX,UINT32_MAX},on[4][2];
 if(!step_native_scan(s->event_owner,notes,(int64_t)r->qlink_index*division,division,tolerance,1,on))return C_SOURCE_FAILURE;
 *value=on[0][0]&1u;return C_OK;
}
static unsigned step_absolute_edit(const CommandRequest *r,const StepCopy *s){
#ifdef COMMAND_COMPONENT
 if(component_step_edit)return component_step_edit(r,s);
#endif
 float denominator;memcpy(&denominator,&s->division_bits,4);unsigned division=3840u/(unsigned)denominator,tolerance=(unsigned)((float)division*0.2f);
 int64_t tick=(int64_t)r->qlink_index*division;
 if(!r->bits)return step_native_remove_owner(s->control,s->event_owner,r->effect_ap,r->qlink_index?tick-tolerance:tick,tick+division-tolerance)?C_OK:C_SOURCE_FAILURE;
 uint32_t level=word(s->control+0xaa34),utils=word(s->control+0xaa30),quantiser=word(s->control+0xaa20);
 if(!step_span(level,0xa0)||!step_span(utils,0x30)||!pointer(quantiser))return C_NATIVE_MEMBERSHIP;
 uint32_t bits=word(level+0x4c)==1?word(level+0x9c):0x3f800000;float velocity,override;memcpy(&velocity,&bits,4);bits=word(utils+0x2c);memcpy(&override,&bits,4);if(override>0)velocity=override;
 if(!(velocity>0&&velocity<=1))return C_SOURCE_CHANGED;
 tick=step_native_quantise(s->control,tick);uint64_t extent=(uint64_t)s->extent[0]|((uint64_t)s->extent[1]<<32);
 if(tick<0||(uint64_t)tick>=extent)return C_SOURCE_CHANGED;
 uint32_t target=0;for(unsigned i=0;i<count;i++)if(tracks[i].track_owner==s->track_owner){target=tracks[i].track;break;}if(!target)return C_IDENTITY;
 step_native_add_owner(s->control,target,s->event_owner,r->effect_ap,tick,division-1,velocity);return C_OK;
}
static void step_service(Context *c){
 unsigned now=atomic_load(&mirror_state->heartbeat);StepDemand wanted={0};unsigned demand=step_demand_read(&command_state->step_interest,&wanted)&&wanted.until>now&&wanted.epoch==epoch;
 if(demand&&!wanted.enabled){wanted.enabled=1;wanted.pad[0]=UINT32_MAX;}
 if(!demand&&!step_pending){for(unsigned i=0;i<STEP_VIEWS;i++){if(step_bindings[i].clip.object)step_bound_release(i);step_bound_attempt_epoch[i]=0;}if(step_current.status!=STEP_OFF){memset(&step_current,0,sizeof(step_current));step_publish(&step_current);}return;}
 if(!step_pending&&now<step_next_tick)return;
 step_next_tick=now>UINT32_MAX-20?UINT32_MAX:now+20;
 unsigned why=step_native_read(&step_current);if(demand){step_project_views(&step_current,&wanted);if(!why)step_clock_read(&step_current);}step_publish(&step_current);
 for(unsigned at=0;at<COMMAND_SLOTS;at++)if(step_pending&(1u<<at)){
  const CommandRequest *r=&accepted[at].request;StepCopy bound;unsigned result=r->effect_slot?step_bound_resolve(r->effect_slot-1,&bound):why;const StepCopy *source=r->effect_slot?&bound:&step_current;if(!result)result=step_request_match(r,source);
  unsigned actual=UINT32_MAX;if(!result)result=step_absolute_value(r,source,&actual);
  if(!result&&actual!=r->bits){if(now<=r->expires)continue;result=C_EXPIRED;}
  event(c,CE_STEP_STATE,r->seq,0,0,r->qlink_root,0,r->qlink_index,result?UINT32_MAX:actual,r->qlink_wrapper,result);
  atomic_store_explicit(active_request+at,0,memory_order_seq_cst);atomic_store_explicit(&command_state->slots[at].done,r->seq,memory_order_release);step_pending&=~(1u<<at);
 }
}
/* Pro Stop Clip in a step view: MPC's per-track stop for the bound clip's
 * column, resolved by the admission above. Playback continues. */
static unsigned step_stop_bound(const CommandRequest *r){
 uint32_t lanes=0;unsigned columns=0,rows=0,why=clip_native_context(&lanes,&columns,&rows);if(why)return why;
 const StepBinding *b=step_bindings+r->effect_slot-1;if(b->column>=columns)return C_IDENTITY;
 clip_native_stop_track(lanes,b->column);return C_OK;
}
static void step_command_service(Context *c,unsigned at,const CommandRequest *r){
 CommandSlot *slot=command_state->slots+at;StepCopy current;unsigned why=C_OK,now=atomic_load(&mirror_state->heartbeat);
 if(!command_step_format(r))why=C_FORMAT;
 else if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)why=C_IDENTITY;
 else if(r->expires<r->created||r->expires-r->created>2000||r->expires<now)why=C_EXPIRED;
 else if(r->created>now)return;
 else if(step_pending)return;
 else {why=r->effect_slot?step_bound_resolve(r->effect_slot-1,&current):step_native_read(&current);if(!why)why=step_request_match(r,&current);unsigned actual=UINT32_MAX;if(!why&&r->reserved==STEP_STOP){if(!r->effect_slot)why=C_FORMAT;}else{if(!why)why=step_absolute_value(r,&current,&actual);if(!why&&actual!=r->before_bits)why=C_SOURCE_CHANGED;}}
 accepted[at].request=*r;native_started[at]=0;atomic_store(&slot->rejected,0);
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *l=slot->lanes+i;atomic_store(&l->sequence,0);atomic_thread_fence(memory_order_release);atomic_store(&l->published,0);atomic_store_explicit(&l->sequence,r->seq,memory_order_release);}
 if(why)goto refused;
 if(!atomic_load(&observer_running)||atomic_load(&command_violation)||atomic_load(&command_state->trace_error)){why=C_SOURCE_FAILURE;goto refused;}
 accepted[at].program=current.program;accepted[at].property=current.event_owner;accepted[at].controller=r->qlink_index;accepted[at].native_bits=r->bits;accepted[at].synchronous=0;
 atomic_store_explicit(active_request+at,r->seq,memory_order_seq_cst);event(c,CE_STEP_BEGIN,r->seq,0,0,current.control,0,r->qlink_index,r->before_bits,current.event_owner,epoch);
 native_started[at]=r->seq;
 why=r->reserved==STEP_STOP?step_stop_bound(r):step_absolute_edit(r,&current);
 event(c,CE_STEP_RETURN,r->seq,0,0,current.control,0,r->qlink_index,r->bits,current.event_owner,epoch);
 /* A stop is one native call with no note state to wait for. */
 if(!why&&r->reserved==STEP_STOP){event(c,CE_STEP_STATE,r->seq,0,0,current.control,0,r->qlink_index,r->bits,current.event_owner,0);atomic_store(&slot->done,r->seq);atomic_store(active_request+at,0);}
 else if(why){event(c,CE_STEP_STATE,r->seq,0,0,current.control,0,r->qlink_index,UINT32_MAX,current.event_owner,why);atomic_store(&slot->done,r->seq);atomic_store(active_request+at,0);}
 else step_pending|=1u<<at;
 atomic_store(&slot->dispatched,r->seq);atomic_store(&slot->returned,r->seq);goto processed;
refused:atomic_store(&slot->rejected,why);
processed:atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);
}
