/* Clip actions reach the native UI facades exactly once. A receipt means the
 * native action was delivered/enqueued, never that quantized playback or
 * recording has started. Only Clip Matrix native state supplies feedback. */
static unsigned clip_request_resolve(const CommandRequest *r,uint32_t *out_track){
 uint32_t lanes=0;unsigned columns=0,rows=0,why=clip_native_context(&lanes,&columns,&rows);
 if(why)return why;
 if(r->epoch!=epoch||r->project_owner!=channel_owner_id(project)||r->qlink_root!=lanes||r->effect_index>=rows)return C_IDENTITY;
 if(r->bits>=CLIP_SCENE_LAUNCH)return columns<=128?C_OK:C_CAPACITY;
 if(r->pad_index>=columns)return C_IDENTITY;
 uint32_t track=clip_native_track(lanes,r->pad_index);
 if(!typed(track,0x6935554)||channel_owner_id(track)!=r->track_owner)return C_IDENTITY;
 ClipNativeShared clip=clip_native_lookup(lanes,r->pad_index,r->effect_index);
 unsigned match=clip.object==r->effect_generation;clip_native_release(&clip);if(!match)return C_SOURCE_CHANGED;
 unsigned state=clip_native_state(lanes,r->pad_index,r->effect_index);
 if(state>CLIP_WILL_STOP_OVERDUBBING)return C_SOURCE_FAILURE;
 /* Record is a state-dependent native button action. Do not reinterpret a
  * stale press as another toggle or retry it after native queue acceptance. */
 if(r->bits==CLIP_RECORD_TOGGLE){
  if(state!=r->before_bits)return C_SOURCE_CHANGED;
  unsigned finish=state==CLIP_RECORDING||state==CLIP_OVERDUBBING;
  if(!finish&&state!=CLIP_EMPTY&&state!=CLIP_READY&&state!=CLIP_PLAYING)return C_BUSY;
  if(!finish&&!*(const unsigned char*)(uintptr_t)(track+0x494))return C_NOT_READY;
 }
 *out_track=track;return C_OK;
}
static unsigned clip_action_native(const CommandRequest *r,uint32_t track){
 uint32_t lanes=r->qlink_root;unsigned column=r->pad_index,row=r->effect_index;
 switch(r->bits){
 case CLIP_LAUNCH:clip_native_trigger(lanes,column,row);return C_OK;
 case CLIP_STOP:clip_native_stop_track(lanes,column);return C_OK;
 case CLIP_RECORD_TOGGLE:
  /* Native record finishes state7 according to RecordTo, but is a no-op in
   * state10. Relaunch exits overdub through MPC's quantized native path. The
   * source state was revalidated above; this is never an enqueue retry. */
  if(r->before_bits==CLIP_OVERDUBBING){clip_native_trigger(lanes,column,row);return C_OK;}
  return clip_native_record(lanes,track,row)?C_OK:C_CAPACITY;
 case CLIP_SCENE_LAUNCH:clip_native_trigger_row(lanes,row);return C_OK;
 case CLIP_SCENE_STOP:{
  unsigned columns=word(lanes+0x34);if(columns>128)return C_CAPACITY;
  /* Native single-player-per-track stop also cancels a queued transition on
   * that participating track; it is not a row-isolated cancellation API. */
  for(unsigned i=0;i<columns;i++){unsigned state=clip_native_state(lanes,i,row);if(state>=CLIP_WILL_PLAY&&state<=CLIP_WILL_STOP_OVERDUBBING)clip_native_stop_track(lanes,i);}
  return C_OK;
 }
 default:return C_FORMAT;
 }
}
static void clip_command_service(Context *c,unsigned at,const CommandRequest *r){
 CommandSlot *slot=command_state->slots+at;unsigned why=C_OK,now=atomic_load(&mirror_state->heartbeat);uint32_t track=0;
 if(!command_clip_format(r))why=C_FORMAT;
 else if(r->pid!=command_state->pid||r->start_lo!=command_state->start_lo||r->start_hi!=command_state->start_hi||r->origin_sec!=command_state->origin_sec||r->origin_nsec!=command_state->origin_nsec)why=C_IDENTITY;
 else if(r->expires<r->created||r->expires-r->created>2000||r->expires<now)why=C_EXPIRED;
 else if(r->created>now)return;
 else why=clip_request_resolve(r,&track);
 accepted[at].request=*r;native_started[at]=0;atomic_store(&slot->rejected,0);
 for(unsigned i=0;i<COMMAND_LANES;i++){CommandLane *l=slot->lanes+i;atomic_store(&l->sequence,0);atomic_thread_fence(memory_order_release);atomic_store(&l->published,0);atomic_store_explicit(&l->sequence,r->seq,memory_order_release);}
 if(why)goto refused;
 if(!atomic_load(&observer_running)||atomic_load(&command_violation)||atomic_load(&command_state->trace_error)){why=C_SOURCE_FAILURE;goto refused;}
 accepted[at].program=r->qlink_root;accepted[at].property=r->effect_generation;accepted[at].controller=r->effect_index;accepted[at].native_bits=r->bits;accepted[at].synchronous=0;
 atomic_store_explicit(active_request+at,r->seq,memory_order_seq_cst);
 event(c,CE_CLIP_BEGIN,r->seq,0,0,r->qlink_root,0,r->effect_index,r->before_bits,r->effect_generation,r->epoch);
 native_started[at]=r->seq;why=clip_action_native(r,track);
 event(c,CE_CLIP_RETURN,r->seq,0,0,r->qlink_root,0,r->effect_index,r->bits,r->effect_generation,r->epoch);
 event(c,CE_CLIP_ACCEPT,r->seq,0,0,r->qlink_root,0,r->effect_index,why?0:1,r->effect_generation,why);
 atomic_store(active_request+at,0);atomic_store(&slot->done,r->seq);atomic_store(&slot->dispatched,r->seq);atomic_store(&slot->returned,r->seq);goto processed;
refused:atomic_store(&slot->rejected,why);
processed:atomic_store_explicit(&slot->processed,r->seq,memory_order_release);atomic_store(&command_state->consumed,atomic_load(&command_state->consumed)+1);
}
