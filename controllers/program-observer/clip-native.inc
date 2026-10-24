/* Native Clip Matrix UI facades. They enqueue MPC's own quantized audio work;
 * the bridge never drives ClipPlayers or owns a clip playback state machine. */
typedef struct {uint32_t object,control;} ClipNativeShared;
#ifdef COMMAND_COMPONENT
static uint32_t (*component_clip_track)(uint32_t,unsigned);
static ClipNativeShared (*component_clip_lookup)(uint32_t,unsigned,unsigned);
static unsigned (*component_clip_state)(uint32_t,unsigned,unsigned);
static unsigned (*component_clip_scene)(uint32_t,unsigned);
static unsigned (*component_clip_emit)(unsigned,uint32_t,unsigned,unsigned);
static unsigned (*component_clip_edit)(uint32_t,uint32_t,unsigned,unsigned,unsigned,unsigned);
static void (*component_clip_delete_row)(uint32_t,uint32_t,unsigned);
static unsigned (*component_clip_insert_row)(uint32_t,uint32_t,unsigned,unsigned);
static void (*component_clip_quantize_midi)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
static void (*component_clip_quantize_audio)(uint32_t,uint32_t,uint32_t,const ClipNativeShared*,uint32_t,uint32_t);
static void (*component_clip_loop_shift)(uint32_t,uint32_t,unsigned,uint64_t,uint32_t,uint32_t,unsigned);
static unsigned (*component_clip_length)(uint32_t,uint32_t,uint32_t,unsigned,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
static unsigned (*component_clip_recording)(uint32_t,uint32_t,unsigned);
static unsigned (*component_clip_audio)(uint32_t,uint32_t);
#endif
#define CLIP_NATIVE(rva,type) ((type)(uintptr_t)(observer_image_bias+(rva)))
static unsigned clip_native_context(uint32_t *lanes,unsigned *columns,unsigned *rows){
 if(!active()||!ready||loading||reset_pending||depth||!typed(root,MODEL_ROOT_VPTR)||word(root+MODEL_ROOT_PROJECT)!=project||!typed(project,MODEL_PROJECT_VPTR)||atomic_load(&mirror_state->meters.closing)||atomic_load(&command_state->stop_requested)||atomic_load(&command_state->new_project_intent))return C_NOT_READY;
 uint32_t p=word(root+MODEL_ROOT_LANES);
 if(!typed(p,MODEL_LANES_VPTR)||word(p+0x178)!=root||word(p+0x184)!=project)return C_NATIVE_MEMBERSHIP;
 *lanes=p;*columns=word(p+0x34);*rows=word(p+0x48);return C_OK;
}
static uint32_t clip_native_track(uint32_t lanes,unsigned column){
#ifdef COMMAND_COMPONENT
 if(component_clip_track)return component_clip_track(lanes,column);
#endif
 return CLIP_NATIVE(MODEL_CLIP_TRACK,uint32_t (*)(uint32_t,unsigned))(lanes,column);
}
static unsigned clip_native_state(uint32_t lanes,unsigned column,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_state)return component_clip_state(lanes,column,row);
#endif
 return CLIP_NATIVE(MODEL_CLIP_STATE,unsigned (*)(uint32_t,unsigned,unsigned))(lanes,column,row);
}
static unsigned clip_native_scene_state(uint32_t lanes,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_scene)return component_clip_scene(lanes,row);
#endif
 return CLIP_NATIVE(MODEL_CLIP_SCENE_STATE,unsigned (*)(uint32_t,unsigned))(lanes,row);
}
static ClipNativeShared clip_native_lookup(uint32_t lanes,unsigned column,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_lookup)return component_clip_lookup(lanes,column,row);
#endif
 ClipNativeShared result={0};
 CLIP_NATIVE(MODEL_CLIP_LOOKUP,void (*)(ClipNativeShared*,uint32_t,unsigned,unsigned))(&result,lanes,column,row);return result;
}
static ClipNativeShared clip_native_lookup_sequence(uint32_t lanes,uint32_t track,uint32_t sequence){
#ifdef COMMAND_COMPONENT
 if(component_clip_lookup)return component_clip_lookup(lanes,0,0);
#endif
 uint32_t coordinate[3]={track,sequence,1};ClipNativeShared result={0};
 CLIP_NATIVE(MODEL_CLIP_MATRIX_LOOKUP,void (*)(ClipNativeShared*,uint32_t,const uint32_t*))(&result,word(lanes+0x1c0),coordinate);return result;
}
static void clip_native_release(ClipNativeShared *value){
 if(value->control)CLIP_NATIVE(MODEL_SHARED_RELEASE,void (*)(uint32_t))(value->control);
 *value=(ClipNativeShared){0};
}
#ifdef MODEL_FIRMWARE_310
static unsigned clip_native_track_recording(uint32_t lanes,unsigned column){
#ifdef COMMAND_COMPONENT
 if(component_clip_recording)return component_clip_recording(observer_image_bias+MODEL_CLIP_TRACK_HAS_STATE,lanes,column);
#endif
 for(unsigned state=CLIP_WILL_RECORD;state<=CLIP_WILL_STOP_OVERDUBBING;state++)
  if(CLIP_NATIVE(MODEL_CLIP_TRACK_HAS_STATE,unsigned (*)(uint32_t,unsigned,unsigned))(lanes+0x1c4,column,state))return 1;
 return 0;
}
static unsigned clip_native_audio(uint32_t track){
#ifdef COMMAND_COMPONENT
 if(component_clip_audio)return component_clip_audio(observer_image_bias+MODEL_CLIP_TRACK_IS_AUDIO,track);
#endif
 return CLIP_NATIVE(MODEL_CLIP_TRACK_IS_AUDIO,unsigned (*)(uint32_t))(track);
}
static unsigned clip_native_edit(const CommandRequest *r){
 uint32_t lanes=r->qlink_root;unsigned result;
 /* Native Edit Row Delete and Snapshot use void facades, unlike CopyRow.
  * Never interpret its return register as an eligibility or completion code. */
 if(r->bits==CLIP_DELETE_ROW||r->bits==CLIP_SNAPSHOT||r->bits==CLIP_CLEAR_SNAPSHOT){
  unsigned address=r->bits==CLIP_DELETE_ROW?MODEL_CLIP_DELETE_ROW:r->bits==CLIP_SNAPSHOT?MODEL_CLIP_SNAPSHOT:MODEL_CLIP_CLEAR_SNAPSHOT;
  uint32_t receiver=lanes+(r->bits==CLIP_DELETE_ROW?0:0xd0);
#ifdef COMMAND_COMPONENT
  if(component_clip_delete_row){component_clip_delete_row(observer_image_bias+address,receiver,r->effect_index);return C_OK;}
#endif
  CLIP_NATIVE(address,void (*)(uint32_t,unsigned))(receiver,r->effect_index);return C_OK;
 }
 if(r->bits==CLIP_INSERT_ROW||r->bits==CLIP_INSERT_CAPTURE){
#ifdef COMMAND_COMPONENT
  if(component_clip_insert_row)result=component_clip_insert_row(observer_image_bias+(r->bits==CLIP_INSERT_ROW?MODEL_CLIP_INSERT_ROW:MODEL_CLIP_INSERT_CAPTURE),lanes,r->effect_index,r->bits==CLIP_INSERT_ROW?1:0);
  else
#endif
  if(r->bits==CLIP_INSERT_ROW)result=CLIP_NATIVE(MODEL_CLIP_INSERT_ROW,unsigned (*)(uint32_t,unsigned,unsigned))(lanes,r->effect_index,1);
  else result=CLIP_NATIVE(MODEL_CLIP_INSERT_CAPTURE,unsigned (*)(uint32_t,unsigned))(lanes,r->effect_index);
  return result==0?C_OK:result==3?C_BUSY:C_NOT_READY;
 }
 if(r->bits==CLIP_DOUBLE_EVENTS||r->bits==CLIP_LOOP_HALF||r->bits==CLIP_LOOP_DOUBLE){
  uint32_t sequence=word(project+0xe4),track=clip_native_track(lanes,r->pad_index),factory=word(lanes+0x17c),pool=word(lanes+0x1c0);
  uint32_t manager=r->bits==CLIP_DOUBLE_EVENTS?word(lanes+0x180):0,monitor=r->bits==CLIP_DOUBLE_EVENTS?word(lanes+0x1b0):0,tracks=r->bits==CLIP_LOOP_HALF?word(project+0xb1c):0;
  if(!typed(sequence,MODEL_SEQUENCE_VPTR))return C_NOT_READY;
#ifdef COMMAND_COMPONENT
  if(component_clip_length)result=component_clip_length(observer_image_bias+(r->bits==CLIP_DOUBLE_EVENTS?MODEL_CLIP_DOUBLE_EVENTS:r->bits==CLIP_LOOP_HALF?MODEL_CLIP_LOOP_HALF:MODEL_CLIP_LOOP_DOUBLE),sequence,track,r->effect_index,factory,manager,monitor,tracks,pool);
  else
#endif
  if(r->bits==CLIP_DOUBLE_EVENTS)result=CLIP_NATIVE(MODEL_CLIP_DOUBLE_EVENTS,unsigned (*)(uint32_t,uint32_t,unsigned,uint32_t,uint32_t,uint32_t,uint32_t))(sequence,track,r->effect_index,factory,manager,monitor,pool);
  else if(r->bits==CLIP_LOOP_HALF)result=CLIP_NATIVE(MODEL_CLIP_LOOP_HALF,unsigned (*)(uint32_t,uint32_t,unsigned,uint32_t,uint32_t,uint32_t))(sequence,track,r->effect_index,factory,tracks,pool);
  else result=CLIP_NATIVE(MODEL_CLIP_LOOP_DOUBLE,unsigned (*)(uint32_t,uint32_t,unsigned,uint32_t,uint32_t))(sequence,track,r->effect_index,factory,pool);
  return result?C_OK:C_NOT_READY;
 }
 if(r->bits==CLIP_LOOP_EARLIER||r->bits==CLIP_LOOP_LATER){
  uint32_t sequence=word(project+0xe4),track=clip_native_track(lanes,r->pad_index),factory=word(lanes+0x17c);
  if(!typed(sequence,MODEL_SEQUENCE_VPTR))return C_NOT_READY;
  ClipNativeShared clip=clip_native_lookup(lanes,r->pad_index,r->effect_index);
  if(clip.object!=r->effect_generation||!typed(clip.object,MODEL_CLIP_VPTR)){clip_native_release(&clip);return C_SOURCE_CHANGED;}
  uint64_t start=(uint64_t)word(clip.object+0x218)|((uint64_t)word(clip.object+0x21c)<<32),length=0;
  if(*(const unsigned char*)(uintptr_t)(clip.object+0x260))length=(uint64_t)word(clip.object+0x258)|((uint64_t)word(clip.object+0x25c)<<32);
  /* Match native Grid arrows: modular64 add/sub; left clamps a negative
   * result to zero. Missing optional length still delivers the native no-op. */
  uint64_t next=r->bits==CLIP_LOOP_EARLIER?start-length:start+length;
  if(r->bits==CLIP_LOOP_EARLIER&&(next>>63))next=0;
#ifdef COMMAND_COMPONENT
  if(component_clip_loop_shift)component_clip_loop_shift(observer_image_bias+MODEL_CLIP_LOOP_SHIFT,factory,2,next,sequence,track+0x468,r->effect_index);
  else
#endif
  CLIP_NATIVE(MODEL_CLIP_LOOP_SHIFT,void (*)(uint32_t,unsigned,uint64_t,uint32_t,uint32_t,unsigned))(factory,2,next,sequence,track+0x468,r->effect_index);
  clip_native_release(&clip);return C_OK;
 }
 if(r->bits==CLIP_QUANTIZE){
  uint32_t quantiser=word(root+0x23c),division=word(root+0x1a0),track=clip_native_track(lanes,r->pad_index),manager=word(lanes+0x180);
  /* Quantiser is nonpolymorphic. Retain the engine's own current settings and
   * its TimeDivision backlink; never create a parallel settings object. */
  if(!pointer(quantiser)||!pointer(division)||word(quantiser+0x204)!=division)return C_NOT_READY;
  ClipNativeShared clip=clip_native_lookup(lanes,r->pad_index,r->effect_index);
  if(clip.object!=r->effect_generation||!typed(clip.object,MODEL_CLIP_VPTR)){clip_native_release(&clip);return C_SOURCE_CHANGED;}
  if(clip_native_audio(track)){
   uint32_t audio=word(root+MODEL_ROOT_AUDIO),time=typed(audio,MODEL_AUDIO_VPTR)?word(audio+MODEL_AUDIO_TIME):0;
   if(!typed(time,MODEL_CLIP_QUANTIZE_TIME_VPTR)||time>UINT32_MAX-0xe8){clip_native_release(&clip);return C_NOT_READY;}
#ifdef COMMAND_COMPONENT
   if(component_clip_quantize_audio)component_clip_quantize_audio(observer_image_bias+MODEL_CLIP_QUANTIZE_AUDIO,quantiser,track,&clip,time+0xe8,manager);
   else
#endif
   CLIP_NATIVE(MODEL_CLIP_QUANTIZE_AUDIO,void (*)(uint32_t,uint32_t,const ClipNativeShared*,uint32_t,uint32_t))(quantiser,track,&clip,time+0xe8,manager);
  }else{
   uint32_t program=word(track+0x648);if(!pointer(program)){clip_native_release(&clip);return C_NOT_READY;}
#ifdef COMMAND_COMPONENT
   if(component_clip_quantize_midi)component_clip_quantize_midi(observer_image_bias+MODEL_CLIP_QUANTIZE_MIDI,program,quantiser,clip.object+0x3b8,manager);
   else
#endif
   CLIP_NATIVE(MODEL_CLIP_QUANTIZE_MIDI,void (*)(uint32_t,uint32_t,uint32_t,uint32_t))(program,quantiser,clip.object+0x3b8,manager);
  }
  clip_native_release(&clip);return C_OK;
 }
 /* Further explicit editing actions must never fall through to DeleteClip. */
 if(r->bits>=CLIP_DOUBLE_EVENTS)return C_FORMAT;
#ifdef COMMAND_COMPONENT
 if(component_clip_edit){uint32_t address=r->bits==CLIP_COPY?MODEL_CLIP_COPY:r->bits==CLIP_COPY_ROW?MODEL_CLIP_COPY_ROW:MODEL_CLIP_DELETE;
  return component_clip_edit(observer_image_bias+address,lanes,r->effect_slot,r->effect_key,r->pad_index,r->effect_index);}
#endif
 if(r->bits==CLIP_COPY)result=CLIP_NATIVE(MODEL_CLIP_COPY,unsigned (*)(uint32_t,unsigned,unsigned,unsigned,unsigned))(lanes,r->effect_slot,r->effect_key,r->pad_index,r->effect_index);
 else if(r->bits==CLIP_COPY_ROW)result=CLIP_NATIVE(MODEL_CLIP_COPY_ROW,unsigned (*)(uint32_t,unsigned,unsigned))(lanes,r->effect_key,r->effect_index);
 else result=CLIP_NATIVE(MODEL_CLIP_DELETE,unsigned (*)(uint32_t,unsigned,unsigned))(lanes,r->pad_index,r->effect_index);
 return result==0?C_OK:result==3?C_BUSY:C_NOT_READY;
}
#endif
static inline void clip_native_trigger(uint32_t lanes,unsigned column,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_emit){component_clip_emit(CLIP_LAUNCH,lanes,column,row);return;}
#endif
 CLIP_NATIVE(MODEL_CLIP_TRIGGER,void (*)(uint32_t,unsigned,unsigned))(lanes,column,row);
}
/* MpcClipLanes vtable slot 9 (MODEL_CLIP_SELECT), the native behind the script API's
 * SelectClip: bounds-checks the slot, then Project 0x254c460 sets the current
 * track and the selected-clip property (project+0x1b4/0x1b8/0x1bc). It makes no
 * trigger call. Admission guards pin both bodies. */
static inline void clip_native_select(uint32_t lanes,unsigned column,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_emit){component_clip_emit(CLIP_SELECT,lanes,column,row);return;}
#endif
 CLIP_NATIVE(MODEL_CLIP_SELECT,void (*)(uint32_t,unsigned,unsigned))(lanes,column,row);
}
/* MpcClipLanes vtable slot 10 (MODEL_CLIP_CREATE), the native behind the script API's
 * CreateClip ("Create a clip in an empty slot"). It refuses the Arrangement row,
 * out-of-range slots, a missing track or current sequence and an occupied slot,
 * then performs MPC's create-clip command at the NewClip default length
 * (clip lanes+0x198, +0x70). Admission guards pin the body. */
enum {CLIP_EMIT_CREATE=16};
static inline void clip_native_create(uint32_t lanes,unsigned column,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_emit){component_clip_emit(CLIP_EMIT_CREATE,lanes,column,row);return;}
#endif
 CLIP_NATIVE(MODEL_CLIP_CREATE,void (*)(uint32_t,unsigned,unsigned))(lanes,column,row);
}
static inline void clip_native_stop_track(uint32_t lanes,unsigned column){
#ifdef COMMAND_COMPONENT
 if(component_clip_emit){component_clip_emit(CLIP_STOP,lanes,column,0);return;}
#endif
 CLIP_NATIVE(MODEL_CLIP_STOP,void (*)(uint32_t,unsigned))(lanes,column);
}
static inline void clip_native_trigger_row(uint32_t lanes,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_emit){component_clip_emit(CLIP_SCENE_LAUNCH,lanes,0,row);return;}
#endif
 CLIP_NATIVE(MODEL_CLIP_TRIGGER_ROW,void (*)(uint32_t,unsigned))(lanes,row);
}
/* Native record callable ABI, using the existing qualified Lanes holder.
 * The native UI uses its panel holder instead. The invoker decrements after
 * execution; the Lanes destructor owns cancellation and its count adjustment. */
static int clip_native_record(uint32_t lanes,uint32_t track,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_emit)return component_clip_emit(CLIP_RECORD_TOGGLE,lanes,track,row);
#endif
 uint32_t tag=lanes+0x154,queue=word(tag),sequencer=word(lanes+0x1a8);
 if(!pointer(queue)||!pointer(sequencer))return 0;
 uint32_t task[38]={observer_image_bias+MODEL_CLIP_RECORD_INVOKER,observer_image_bias+MODEL_CLIP_RECORD_MANAGER,
  tag,sequencer,track,row,0,row};
 _Atomic uint32_t *pending=(_Atomic uint32_t*)(uintptr_t)(lanes+0x158);
 atomic_fetch_add_explicit(pending,1,memory_order_seq_cst);
 CLIP_NATIVE(MODEL_MUTEX_LOCK,void (*)(uint32_t))(queue+MODEL_QUEUE_MUTEX);
 unsigned accepted=CLIP_NATIVE(MODEL_QUEUE_PUSH,unsigned (*)(uint32_t,uint32_t*,uint32_t))(queue+MODEL_QUEUE_AUDIO,task,tag&~3u);
 if(task[1])((void (*)(unsigned,uint32_t*,void*))(uintptr_t)task[1])(1,task+2,NULL);
 CLIP_NATIVE(MODEL_MUTEX_UNLOCK,void (*)(uint32_t))(queue+MODEL_QUEUE_MUTEX);
 if(!accepted)atomic_fetch_sub_explicit(pending,1,memory_order_seq_cst);
 return accepted!=0;
}
#undef CLIP_NATIVE
