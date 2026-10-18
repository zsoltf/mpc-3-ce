/* Native Clip Matrix UI facades. They enqueue MPC's own quantized audio work;
 * the bridge never drives ClipPlayers or owns a clip playback state machine. */
typedef struct {uint32_t object,control;} ClipNativeShared;
#ifdef COMMAND_COMPONENT
static uint32_t (*component_clip_track)(uint32_t,unsigned);
static ClipNativeShared (*component_clip_lookup)(uint32_t,unsigned,unsigned);
static unsigned (*component_clip_state)(uint32_t,unsigned,unsigned);
static unsigned (*component_clip_scene)(uint32_t,unsigned);
static unsigned (*component_clip_emit)(unsigned,uint32_t,unsigned,unsigned);
#endif
#define CLIP_NATIVE(rva,type) ((type)(uintptr_t)(observer_image_bias+(rva)))
static unsigned clip_native_context(uint32_t *lanes,unsigned *columns,unsigned *rows){
 if(!active()||!ready||loading||reset_pending||depth||!typed(root,0x68d7bf8)||word(root+0x24c)!=project||!typed(project,0x6933c4c)||atomic_load(&mirror_state->meters.closing)||atomic_load(&command_state->stop_requested)||atomic_load(&command_state->new_project_intent))return C_NOT_READY;
 uint32_t p=word(root+0x754);
 if(!typed(p,0x6894708)||word(p+0x178)!=root||word(p+0x184)!=project)return C_NATIVE_MEMBERSHIP;
 *lanes=p;*columns=word(p+0x34);*rows=word(p+0x48);return C_OK;
}
static uint32_t clip_native_track(uint32_t lanes,unsigned column){
#ifdef COMMAND_COMPONENT
 if(component_clip_track)return component_clip_track(lanes,column);
#endif
 return CLIP_NATIVE(0xe658d0,uint32_t (*)(uint32_t,unsigned))(lanes,column);
}
static unsigned clip_native_state(uint32_t lanes,unsigned column,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_state)return component_clip_state(lanes,column,row);
#endif
 return CLIP_NATIVE(0xe67140,unsigned (*)(uint32_t,unsigned,unsigned))(lanes,column,row);
}
static unsigned clip_native_scene_state(uint32_t lanes,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_scene)return component_clip_scene(lanes,row);
#endif
 return CLIP_NATIVE(0xe669f8,unsigned (*)(uint32_t,unsigned))(lanes,row);
}
static ClipNativeShared clip_native_lookup(uint32_t lanes,unsigned column,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_lookup)return component_clip_lookup(lanes,column,row);
#endif
 ClipNativeShared result={0};
 CLIP_NATIVE(0xe66230,void (*)(ClipNativeShared*,uint32_t,unsigned,unsigned))(&result,lanes,column,row);return result;
}
static ClipNativeShared clip_native_lookup_sequence(uint32_t lanes,uint32_t track,uint32_t sequence){
#ifdef COMMAND_COMPONENT
 if(component_clip_lookup)return component_clip_lookup(lanes,0,0);
#endif
 uint32_t coordinate[3]={track,sequence,1};ClipNativeShared result={0};
 CLIP_NATIVE(0x253ea48,void (*)(ClipNativeShared*,uint32_t,const uint32_t*))(&result,word(lanes+0x1c0),coordinate);return result;
}
static void clip_native_release(ClipNativeShared *value){
 if(value->control)CLIP_NATIVE(0xc96a68,void (*)(uint32_t))(value->control);
 *value=(ClipNativeShared){0};
}
static inline void clip_native_trigger(uint32_t lanes,unsigned column,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_emit){component_clip_emit(CLIP_LAUNCH,lanes,column,row);return;}
#endif
 CLIP_NATIVE(0xe672bc,void (*)(uint32_t,unsigned,unsigned))(lanes,column,row);
}
static inline void clip_native_stop_track(uint32_t lanes,unsigned column){
#ifdef COMMAND_COMPONENT
 if(component_clip_emit){component_clip_emit(CLIP_STOP,lanes,column,0);return;}
#endif
 CLIP_NATIVE(0xe65f24,void (*)(uint32_t,unsigned))(lanes,column);
}
static inline void clip_native_trigger_row(uint32_t lanes,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_emit){component_clip_emit(CLIP_SCENE_LAUNCH,lanes,0,row);return;}
#endif
 CLIP_NATIVE(0xe660b8,void (*)(uint32_t,unsigned))(lanes,row);
}
/* Native inline task and native tag/count lifetime, exactly as the MPC UI
 * record action. The native audio invoker owns decrement after successful
 * enqueue; the MpcClipLanes destructor owns cancellation. */
static int clip_native_record(uint32_t lanes,uint32_t track,unsigned row){
#ifdef COMMAND_COMPONENT
 if(component_clip_emit)return component_clip_emit(CLIP_RECORD_TOGGLE,lanes,track,row);
#endif
 uint32_t tag=lanes+0x154,queue=word(tag),sequencer=word(lanes+0x1a8);
 if(!pointer(queue)||!pointer(sequencer))return 0;
 uint32_t task[38]={observer_image_bias+0x38a43dc,observer_image_bias+0x38a4224,
  tag,sequencer,track,row,0,row};
 _Atomic uint32_t *pending=(_Atomic uint32_t*)(uintptr_t)(lanes+0x158);
 atomic_fetch_add_explicit(pending,1,memory_order_seq_cst);
 CLIP_NATIVE(0x94cdb0,void (*)(uint32_t))(queue+0x124);
 unsigned accepted=CLIP_NATIVE(0x28cb18c,unsigned (*)(uint32_t,uint32_t*,uint32_t))(queue+0xfc,task,tag&~3u);
 if(task[1])((void (*)(unsigned,uint32_t*,void*))(uintptr_t)task[1])(1,task+2,NULL);
 CLIP_NATIVE(0x94cdd4,void (*)(uint32_t))(queue+0x124);
 if(!accepted)atomic_fetch_sub_explicit(pending,1,memory_order_seq_cst);
 return accepted!=0;
}
#undef CLIP_NATIVE
