/* Each surface keeps an owning reference to its chosen native Clip. No notes,
 * playback or selection are owned here: re-resolution through MPC's rooted
 * Track/Clip lookup admits every read and edit against that same Clip. */
typedef struct {ClipNativeShared clip;uint32_t epoch,project_owner,track_owner,program_owner,sequence,row,bind_generation,pad,column,sequence_slot,sequence_used;} StepBinding;
static StepBinding step_bindings[STEP_VIEWS];
static uint32_t step_bound_attempt[STEP_VIEWS],step_bound_attempt_epoch[STEP_VIEWS];
static _Atomic uint32_t step_bound_owner[STEP_VIEWS],step_bound_generation[STEP_VIEWS];
static void step_bound_release(unsigned i){
 step_bound_clock_clear(i);
 atomic_store_explicit(step_bound_owner+i,0,memory_order_release);
 clip_native_release(&step_bindings[i].clip);memset(step_bindings+i,0,sizeof(step_bindings[i]));memset(step_projections+i,0,sizeof(step_projections[i]));
}
static void step_bound_changed(const Context *c){
 if(!mirror_state||!observer_running||atomic_load(&command_state->step_interest.until)<=atomic_load(&mirror_state->heartbeat))return;
 uint32_t owner=c->r[4];
 for(unsigned i=0;i<STEP_VIEWS;i++)if(owner&&owner==atomic_load_explicit(step_bound_owner+i,memory_order_acquire))atomic_fetch_add_explicit(step_bound_generation+i,1,memory_order_release);
}
static unsigned step_bound_track_column(const StepBinding *b,uint32_t lanes,unsigned columns,uint32_t *track,unsigned *column){
 if(columns>128)return C_CAPACITY;
 if(b->column<columns){uint32_t p=clip_native_track(lanes,b->column);if(p&&channel_owner_id(p)==b->track_owner&&typed(p,MODEL_TRACK_VPTR)){*track=p;*column=b->column;return C_OK;}}
 for(unsigned i=0;i<columns;i++){uint32_t p=clip_native_track(lanes,i);if(p&&channel_owner_id(p)==b->track_owner){if(!typed(p,MODEL_TRACK_VPTR))return C_NATIVE_MEMBERSHIP;*track=p;*column=i;return C_OK;}}
 return C_IDENTITY;
}
static unsigned step_bound_resolve(unsigned i,StepCopy *s){
 StepBinding *b=step_bindings+i;memset(s,0,sizeof(*s));s->status=STEP_UNAVAILABLE;s->tick=atomic_load(&mirror_state->heartbeat);s->epoch=epoch;
 uint32_t lanes=0,track=0;unsigned columns=0,rows=0,column=0,why=clip_native_context(&lanes,&columns,&rows);if(why)return why;
 if(!b->clip.object||b->epoch!=epoch||b->project_owner!=channel_owner_id(project))return C_IDENTITY;
 why=step_bound_track_column(b,lanes,columns,&track,&column);if(why)return why;b->column=column;
 if(b->row>=rows)return C_IDENTITY;
 if(!b->row){
  /* First find the opaque retained identity in the live rooted Sequence
   * array. Only that membership permits reading its slot/used fields. */
  uint32_t slots=word(project+MODEL_PROJECT_SEQUENCES),total=word(project+MODEL_PROJECT_SEQUENCE_COUNT);unsigned found=0;
  if(!total||total>SEQ_DUP_SLOT_LIMIT||!step_span(slots,4*total))return C_NATIVE_MEMBERSHIP;
  unsigned j=b->sequence_slot;if(j<total&&word(slots+4*j)==b->sequence){
   if(!step_span(b->sequence,0x25c)||word(b->sequence+0x250)!=j)return C_IDENTITY;
   /* A newly selected blank Sequence already owns its Clip. MPC marks it
    * used through the native event notification after the first note. Once
    * observed used, clearing it still invalidates this retained binding. */
   unsigned used=*(const unsigned char*)(uintptr_t)(b->sequence+0x258)!=0;
   if(b->sequence_used&&!used)return C_IDENTITY;
   b->sequence_used|=used;found=1;
  }
  if(!found)return C_IDENTITY;
 }
 ClipNativeShared live=b->row?clip_native_lookup(lanes,column,b->row):clip_native_lookup_sequence(lanes,track,b->sequence);unsigned same=live.object==b->clip.object;clip_native_release(&live);if(!same)return C_IDENTITY;
 uint32_t clip=b->clip.object,control=word(root+MODEL_ROOT_STEP_CONTROL),program=word(track+0x648);
 Track *t=NULL;for(unsigned j=0;j<count;j++)if(tracks[j].track==track){t=tracks+j;break;}
 if(!t||t->transition||t->program_owner!=b->program_owner||channel_owner_id(program)!=b->program_owner||!typed(program,MODEL_DRUMPROGRAM_VPTR)||word(program+0x1140)!=0)return C_NOT_DRUM;
 if(!typed(clip,MODEL_CLIP_VPTR)||!typed(clip+MODEL_CLIP_EVENT_OWNER,MODEL_EVENT_OWNER_VPTR)||!typed(control,MODEL_STEP_CONTROL_VPTR)||!step_span(control,0xaa38))return C_NATIVE_MEMBERSHIP;
 uint32_t quantiser=word(control+0xa940);if(!step_span(quantiser,0x74))return C_NATIVE_MEMBERSHIP;
 uint32_t bits=word(quantiser+0x70);float division;memcpy(&division,&bits,4);if(!(division>=1&&division<=3840)||division!=(unsigned)division||3840u%(unsigned)division)return C_SOURCE_CHANGED;
 s->root=root;s->control=control;s->context=word(root+MODEL_ROOT_STEP_CONTEXT);s->sequence=b->sequence;s->program=program;s->project_owner=b->project_owner;s->track_owner=b->track_owner;s->program_owner=b->program_owner;s->serial=t->serial;s->binding=t->binding;s->incarnation=t->incarnation;s->pad=b->pad;s->note=word(program+0x1170+4*b->pad);s->division_bits=bits;s->event_owner=clip+MODEL_CLIP_EVENT_OWNER;
 for(unsigned j=0;j<4;j++)s->time[j]=word(control+0xd0+4*j);
 if(b->row){int64_t end=((int64_t (*)(uint32_t))(uintptr_t)(observer_image_bias+MODEL_CLIP_EXTENT))(clip);s->extent[0]=(uint32_t)end;s->extent[1]=(uint32_t)((uint64_t)end>>32);}
 else {s->extent[0]=word(b->sequence+0x1a8);s->extent[1]=word(b->sequence+0x1ac);}
 if(word(track+0x508)==1&&*(const unsigned char*)(uintptr_t)(track+0x518)){s->extent[0]=word(track+0x510);s->extent[1]=word(track+0x514);}
 uint64_t extent=(uint64_t)s->extent[0]|((uint64_t)s->extent[1]<<32);if(!extent||extent>INT64_MAX)return C_SOURCE_CHANGED;
 s->status=STEP_READY;return C_OK;
}
static unsigned step_bound_acquire(unsigned i,const StepCopy *selected,uint32_t generation){
 step_bound_release(i);if(selected->status!=STEP_READY)return C_NOT_READY;
 uint32_t lanes=0,track=word(project+0xa0);unsigned columns=0,rows=0,why=clip_native_context(&lanes,&columns,&rows);if(why)return why;
 unsigned row=*(const unsigned char*)(uintptr_t)(project+0x1bc)?0:word(project+0x1b8),column=columns;
 if(word(project+0x1b4)!=track||row>=rows||columns>128)return C_NATIVE_MEMBERSHIP;
 for(unsigned j=0;j<columns;j++)if(clip_native_track(lanes,j)==track){column=j;break;}
 if(column==columns)return C_NATIVE_MEMBERSHIP;
 ClipNativeShared clip=clip_native_lookup(lanes,column,row);
 if(!clip.object||clip.object+MODEL_CLIP_EVENT_OWNER!=selected->event_owner){clip_native_release(&clip);return C_NATIVE_MEMBERSHIP;}
 StepBinding *b=step_bindings+i;*b=(StepBinding){.clip=clip,.epoch=epoch,.project_owner=selected->project_owner,.track_owner=selected->track_owner,.program_owner=selected->program_owner,.sequence=selected->sequence,.row=row,.bind_generation=generation,.pad=selected->pad,.column=column,.sequence_slot=row?0:word(selected->sequence+0x250)};
 atomic_store(step_bound_generation+i,1);atomic_store_explicit(step_bound_owner+i,selected->event_owner,memory_order_release);return C_OK;
}
static StepTarget step_bound_target(unsigned i,const StepCopy *s){
 StepBinding *b=step_bindings+i;
 StepTarget t={.epoch=s->epoch,.project_owner=s->project_owner,.track_owner=s->track_owner,.program_owner=s->program_owner,.serial=s->serial,.binding=s->binding,.incarnation=s->incarnation,.control=s->control,.sequence=s->sequence,.program=s->program,.event_owner=s->event_owner,.clip=b->clip.object,.clip_row=b->row,.bind_generation=b->bind_generation,.division_bits=s->division_bits,.extent={s->extent[0],s->extent[1]},.native_clip_state=clip_native_state(word(root+MODEL_ROOT_LANES),b->column,b->row)};return t;
}
