/* Native list-A player advance owns these pointers. The hook copies only the
 * already advanced primary row's local tick, for active bound drum targets. */
typedef struct {_Atomic uint32_t revision,epoch,track,clip,row;} StepBoundClockAdmission;
typedef struct {_Atomic uint32_t revision,admission,tick,lo,hi,playing;} StepBoundClockSample;
static StepBoundClockAdmission step_bound_clock_admission[STEP_VIEWS];
static StepBoundClockSample step_bound_clock_sample[STEP_VIEWS];
static _Atomic uint32_t step_bound_clock_mask;
static void step_bound_clock_clear(unsigned i){atomic_fetch_and_explicit(&step_bound_clock_mask,~(1u<<i),memory_order_release);}
static void step_bound_clock_capture(Context *c){
 unsigned mask=atomic_load_explicit(&step_bound_clock_mask,memory_order_acquire);if(!mask||!mirror_state||!observer_running)return;
 unsigned now=atomic_load(&mirror_state->heartbeat);if(atomic_load(&command_state->step_interest.until)<=now)return;
 uint32_t player=c->r[0];if(!step_span(player,MODEL_PLAYER_READ_SPAN)||c->r[4]+8!=player)return;
 uint32_t track=word(player+0x80);
 while(mask){unsigned i=(unsigned)__builtin_ctz(mask);mask&=mask-1;StepBoundClockAdmission *a=step_bound_clock_admission+i;
  unsigned rev=atomic_load_explicit(&a->revision,memory_order_acquire);if(!rev||(rev&1)||atomic_load(&a->epoch)!=atomic_load(&mirror_state->epoch)||atomic_load(&a->track)!=track)continue;
  StepBoundClockSample *out=step_bound_clock_sample+i;unsigned prior=atomic_load(&out->tick);if(atomic_load(&out->admission)==rev&&now>=prior&&now-prior<20)continue;
  uint32_t row=atomic_load(&a->row),clip=atomic_load(&a->clip),begin=word(player+MODEL_PLAYER_ROWS),end=word(player+MODEL_PLAYER_ROWS_END),playing=0,lo=0,hi=0;
  if(row==word(player+0x50)&&begin<=end&&step_span(begin,0xc0)&&row<(end-begin)/0xc0){
   uint32_t data=word(begin+row*0xc0),cache=data>=0x80?data-0x80:0,timeline=word(player+(row?MODEL_PLAYER_TIMELINE:MODEL_PLAYER_TIMELINE_ZERO));
   if(step_span(cache,0x204)&&typed(cache,MODEL_CLIP_CACHE_VPTR)&&word(cache+0x44)==clip+MODEL_CLIP_AUDIO_DATA&&word(cache+0x1ec)==clip+MODEL_CLIP_AUDIO_DATA&&word(cache+0x1fc)==track&&word(cache+0x200)==row&&step_span(timeline,0x68)){
    playing=*(const unsigned char*)(uintptr_t)(timeline+0x44)!=0;lo=word(timeline+0x60);hi=word(timeline+0x64);
   }
  }
  if(rev!=atomic_load_explicit(&a->revision,memory_order_acquire))continue;
  unsigned write=atomic_load(&out->revision);if((write&1)||write>UINT32_MAX-2||!atomic_compare_exchange_strong(&out->revision,&write,write+1))continue;
  atomic_store(&out->admission,rev);atomic_store(&out->tick,now);atomic_store(&out->lo,lo);atomic_store(&out->hi,hi);atomic_store(&out->playing,playing);atomic_store_explicit(&out->revision,write+2,memory_order_release);
 }
}
static void step_bound_clock_read(unsigned i,uint32_t track,StepTarget *target,unsigned now){
 StepBoundClockAdmission *a=step_bound_clock_admission+i;unsigned rev=atomic_load(&a->revision);
 if(!rev||atomic_load(&a->epoch)!=target->epoch||atomic_load(&a->track)!=track||atomic_load(&a->clip)!=target->clip||atomic_load(&a->row)!=target->clip_row){
  if(rev>UINT32_MAX-2)return;
  atomic_store_explicit(&a->revision,rev+1,memory_order_release);atomic_store(&a->epoch,target->epoch);atomic_store(&a->track,track);atomic_store(&a->clip,target->clip);atomic_store(&a->row,target->clip_row);atomic_store_explicit(&a->revision,rev+2,memory_order_release);atomic_fetch_or_explicit(&step_bound_clock_mask,1u<<i,memory_order_release);return;
 }
 atomic_fetch_or_explicit(&step_bound_clock_mask,1u<<i,memory_order_release);
 StepBoundClockSample *v=step_bound_clock_sample+i;unsigned q=atomic_load_explicit(&v->revision,memory_order_acquire);if(!q||(q&1))return;
 unsigned source=atomic_load(&v->admission),tick=atomic_load(&v->tick),playing=atomic_load(&v->playing),lo=atomic_load(&v->lo),hi=atomic_load(&v->hi);
 if(q!=atomic_load_explicit(&v->revision,memory_order_acquire)||source!=rev||!playing||tick>now||now-tick>=STEP_FRESH_MS||(hi&0x80000000u))return;
 target->clock_available=1;target->clock_tick=tick;target->clock_value[0]=lo;target->clock_value[1]=hi;
}
