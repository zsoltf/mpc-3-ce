/* Native temporary objects for absolute drum edits. These adapters are called
 * only by the existing UI command owner after its source/target admission.
 * MPC constructs the note and owns the resulting command and Undo history. */
typedef struct {uint32_t words[6];} StepNativeSet;
typedef struct {uint64_t words[7];} StepNativeEvent;
typedef struct {StepNativeSet events;int64_t first,last;} StepNativeQuery;
_Static_assert(sizeof(StepNativeSet)==24,"native step event set ABI");
_Static_assert(sizeof(StepNativeEvent)==56,"native note temporary ABI");
_Static_assert(sizeof(StepNativeQuery)==40,"native range query ABI");
#define STEP_NATIVE(rva,type) ((type)(uintptr_t)(observer_image_bias+(rva)))
static void step_native_set_init(StepNativeSet *set){
 memset(set,0,sizeof(*set));
 set->words[3]=set->words[4]=(uint32_t)(uintptr_t)(set->words+1);
}
static void step_native_set_release(StepNativeSet *set){
 STEP_NATIVE(MODEL_STEP_SET_RELEASE,void (*)(uint32_t))(set->words[2]);
}
static void step_native_note_predicate(uint32_t predicate[4],unsigned note){
 predicate[0]=note;predicate[1]=0;
 predicate[2]=observer_image_bias+MODEL_STEP_PREDICATE_MANAGER;
 predicate[3]=observer_image_bias+MODEL_STEP_PREDICATE_INVOKER;
}
static int64_t step_native_quantise(uint32_t control,int64_t tick){
 return STEP_NATIVE(MODEL_STEP_QUANTISE,int64_t (*)(uint32_t,int64_t))(word(control+0xaa20),tick);
}
/* Same borrowed-node iteration used by MPC's native range query, with no
 * allocation, retained nodes or mutation. The caller owns source admission
 * and rechecks it after this single UI callback. A dense range that exceeds
 * the fixed scan bound is unavailable, never a partially painted pattern. */
#ifdef COMMAND_COMPONENT
static int (*component_step_scan)(uint32_t,const uint32_t[4],int64_t,unsigned,unsigned,unsigned,uint32_t[4][2]);
#endif
static int step_native_scan(uint32_t owner,const uint32_t notes[4],int64_t first,unsigned division,unsigned tolerance,unsigned cells,uint32_t on[4][2]){
 #ifdef COMMAND_COMPONENT
 if(component_step_scan)return component_step_scan(owner,notes,first,division,tolerance,cells,on);
 #endif
 enum {STEP_SCAN_LIMIT=4096};
 if(!pointer(owner)||owner>UINT32_MAX-0x48||!cells||cells>64||!division||tolerance>=division||first<0||first>INT64_MAX-cells*(int64_t)division)return 0;
 memset(on,0,8*sizeof(uint32_t));
 int64_t begin=first-(int64_t)tolerance,end=first+cells*(int64_t)division-tolerance;
 uint32_t sentinel=owner+0x34,node=STEP_NATIVE(MODEL_STEP_LOWER_BOUND,uint32_t (*)(uint32_t,int64_t))(owner+0x30,begin);
 for(unsigned count=0;node!=sentinel;count++){
  if(count==STEP_SCAN_LIMIT||!pointer(node)||node>UINT32_MAX-72)return 0;
  int64_t tick=(int64_t)((uint64_t)word(node+0x10)|((uint64_t)word(node+0x14)<<32));
  if(tick>=end)return 1;
  if(tick<begin)return 0;
  if(*(const unsigned char*)(uintptr_t)(node+0x18)==3&&tick>=0){
   unsigned cell=(unsigned)((tick-first+tolerance)/division),note=*(const unsigned char*)(uintptr_t)(node+0x40);
   if(cell>=cells)return 0;
   for(unsigned sound=0;sound<4;sound++)if(notes[sound]==note)on[sound][cell/32]|=1u<<(cell%32);
  }
  node=STEP_NATIVE(MODEL_CONST_RB_INCREMENT,uint32_t (*)(uint32_t))(node);
 }
 return 1;
}
static inline void step_native_remove(uint32_t control,unsigned note,int64_t first,int64_t end){
 uint32_t lane[8]={0},predicate[4];StepNativeSet removed;
 step_native_note_predicate(predicate,note);
 STEP_NATIVE(MODEL_STEP_RANGE_REMOVE,void (*)(StepNativeSet*,uint32_t,int64_t,int64_t,const void*,const void*))(&removed,control,first,end,lane,predicate);
 step_native_set_release(&removed);
}
/* Explicit event-owner composition also serves a bound, nonselected Clip.
 * The native command copies both sets and retains its native SafePointer. */
static void step_native_submit_sets(uint32_t control,uint32_t owner,uint32_t track_context,unsigned mode,const StepNativeSet *removed,const StepNativeSet *added){
 StepNativeSet modified;uint32_t strings[2],result[2]={0};step_native_set_init(&modified);
 for(unsigned i=0;i<2;i++)STEP_NATIVE(MODEL_STRING_DEFAULT,void (*)(uint32_t*))(strings+i);
 STEP_NATIVE(MODEL_STEP_COMMAND_FACTORY,void (*)(uint32_t*,uint32_t,uint32_t,const StepNativeSet*,const StepNativeSet*,const StepNativeSet*,uint32_t*,uint32_t*,unsigned,uint32_t))
  (result,word(control+0xaa28),owner,removed,added,&modified,strings,strings+1,mode,track_context);
 if(result[1])STEP_NATIVE(MODEL_SHARED_RELEASE,void (*)(uint32_t))(result[1]);
 for(unsigned i=2;i;i--)STEP_NATIVE(MODEL_STRING_DESTROY,void (*)(uint32_t*))(strings+i-1);
 STEP_NATIVE(MODEL_STEP_MODIFIED_RELEASE,void (*)(uint32_t))(modified.words[2]);
}
static void step_native_add_owner(uint32_t control,uint32_t track,uint32_t owner,unsigned note,int64_t tick,int64_t length,float velocity){
 StepNativeEvent event;StepNativeSet removed,added;
 STEP_NATIVE(MODEL_STEP_NOTE_CTOR,void (*)(StepNativeEvent*,unsigned,int64_t,float,int64_t,unsigned,unsigned))(&event,note,tick,velocity,length,100,0);
 step_native_set_init(&removed);step_native_set_init(&added);
 STEP_NATIVE(MODEL_STEP_SET_INSERT,uint32_t (*)(StepNativeSet*,const StepNativeEvent*))(&added,&event);
 step_native_submit_sets(control,owner,track+0x14b4,1,&removed,&added);
 step_native_set_release(&added);step_native_set_release(&removed);
}
static inline void step_native_add(uint32_t control,uint32_t track,unsigned note,int64_t tick,int64_t length,float velocity){
 step_native_add_owner(control,track,word(control+0x68),note,tick,length,velocity);
}
/* Copy exact matching native note events into MPC's own temporary set. No
 * partially collected set can reach the command factory after a scan refusal. */
static inline int step_native_remove_owner(uint32_t control,uint32_t owner,unsigned note,int64_t first,int64_t end){
 if(!pointer(owner)||owner>UINT32_MAX-0x48||note>127||end<=first)return 0;
 StepNativeSet removed,added;step_native_set_init(&removed);step_native_set_init(&added);
 uint32_t sentinel=owner+0x34,node=STEP_NATIVE(MODEL_STEP_LOWER_BOUND,uint32_t (*)(uint32_t,int64_t))(owner+0x30,first);
 unsigned examined=0;int ok=1;
 while(node!=sentinel){
  if(examined++==4096||!pointer(node)||node>UINT32_MAX-72){ok=0;break;}
  int64_t tick=(int64_t)((uint64_t)word(node+0x10)|((uint64_t)word(node+0x14)<<32));
  if(tick>=end)break;
  if(tick<first){ok=0;break;}
  if(*(const unsigned char*)(uintptr_t)(node+0x18)==3&&*(const unsigned char*)(uintptr_t)(node+0x40)==note){
   StepNativeEvent event;memcpy(&event,(const void*)(uintptr_t)(node+0x10),sizeof(event));
   STEP_NATIVE(MODEL_STEP_SET_INSERT,uint32_t (*)(StepNativeSet*,const StepNativeEvent*))(&removed,&event);
  }
  node=STEP_NATIVE(MODEL_CONST_RB_INCREMENT,uint32_t (*)(uint32_t))(node);
 }
 if(ok&&removed.words[5])step_native_submit_sets(control,owner,0,0,&removed,&added);
 step_native_set_release(&added);step_native_set_release(&removed);return ok;
}
#undef STEP_NATIVE
