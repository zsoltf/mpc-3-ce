/* Selection-only navigation for the focused native Grid. Shared by the outer
 * UI command drain and the panel wheel callback on that same qualified owner.
 * No retained items, timer or scan outside an explicit gesture/request.
 * See docs/note-selection.md for native evidence. */
#define NOTE_GRID_VTABLE MODEL_NOTE_GRID_VPTR
#define NOTE_ITEM_VTABLE MODEL_NOTE_ITEM_VPTR
#define NOTE_HOLDER_VTABLE MODEL_NOTE_HOLDER_VPTR
#define NOTE_BATCH_RVA MODEL_INPUT_GRID_BATCH
#define NOTE_CLEAR_RVA MODEL_INPUT_GRID_CLEAR
#define NOTE_SELECT_RVA MODEL_INPUT_GRID_SELECT
#define NOTE_REVEAL_RVA MODEL_INPUT_GRID_REVEAL
#define NOTE_CHILD_LIMIT 4096u
typedef struct {uint32_t item;int64_t time;unsigned pitch;} NotePosition;
#ifdef COMMAND_COMPONENT
static void (*component_note_call)(unsigned kind,uint32_t grid,uint32_t item);
#endif
static int note_extent(uint32_t p,unsigned size){return pointer(p)&&p<=UINT32_MAX-size;}
static int note_typed(uint32_t p,uint32_t v,unsigned size){return note_extent(p,size)&&word(p)==observer_image_bias+v;}
static unsigned note_byte(uint32_t p){return (word(p&~3u)>>((p&3u)*8))&255u;}
static int note_compare(NotePosition a,NotePosition b){
 if(a.time!=b.time)return a.time<b.time?-1:1;
 if(a.pitch!=b.pitch)return a.pitch<b.pitch?-1:1;
 /* Exact duplicates use current live addresses, never persisted identities.
  * UI child order cannot break ties: selecting a note changes that order. */
 return a.item==b.item?0:a.item<b.item?-1:1;
}
static uint32_t note_grid(void){
 uint32_t receiver=word(observer_image_bias+FOCUS_SLOT_RVA);if(!pointer(receiver))return 0;
 unsigned legal=0;for(unsigned i=0;i<4;i++)legal|=word(receiver)==observer_image_bias+wheel_focus_vtables[i];
 if(!legal)return 0;
 uint32_t grid=word(receiver+WHEEL_FOCUSED_OFFSET);
 if(!note_typed(grid,NOTE_GRID_VTABLE,0x4a8)||!(word(grid+0x68)&2u))return 0;
 /* A hidden ancestor or an overlay must not redirect this operation into a
  * retained page. Walk the native Component parent chain, bounded. */
 uint32_t p=grid;unsigned i;
 for(i=0;i<32&&p;i++){if(!note_extent(p,0x6c)||!(word(p+0x68)&2u))return 0;p=word(p+0xc);}
 return p?0:grid;
}
static int note_position(uint32_t grid,uint32_t item,NotePosition *out){
 if(!note_typed(item,NOTE_ITEM_VTABLE,0x114)||word(item+MODEL_NOTE_OWNER)!=grid||word(item+MODEL_NOTE_SECOND_OWNER)!=grid||note_byte(item+MODEL_NOTE_ACTIVE))return 0;
 if((word(item+MODEL_NOTE_ORIGINAL_TYPE)&255u)!=3u||(word(item+MODEL_NOTE_CURRENT_TYPE)&255u)!=3u)return 0;
 out->item=item;out->time=(int64_t)(((uint64_t)word(item+MODEL_NOTE_CURRENT_TIME_HIGH)<<32)|word(item+MODEL_NOTE_CURRENT_TIME));out->pitch=note_byte(item+MODEL_NOTE_PITCH);return 1;
}
static int note_quiet(uint32_t item){
 if(note_byte(item+MODEL_NOTE_QUIET)||word(item+MODEL_NOTE_PENDING))return 0;
 /* The live velocity-lane companion has a selection-state/repaint callback.
  * Unknown companions are not admitted to the clear/select composition. */
 uint32_t companion=word(item+MODEL_NOTE_COMPANION);
 if(companion){
  if(!note_typed(companion,MODEL_NOTE_COMPANION_VPTR,0xec)||word(word(companion)+0xc0)!=observer_image_bias+MODEL_NOTE_COMPANION_SELECT||(word(companion+0x68)&1u))return 0;
  uint32_t owner=word(companion+MODEL_NOTE_OWNER),parent=word(companion+0xc);
  if(!note_typed(owner,MODEL_NOTE_COMPANION_OWNER_VPTR,0x58)||word(word(owner)+0xc4)!=observer_image_bias+MODEL_NOTE_COMPANION_REPAINT||
     !note_typed(parent,MODEL_NOTE_COMPANION_OWNER_VPTR,0x58)||word(parent+0x54)||word(word(parent)+0x4c)!=observer_image_bias+MODEL_COMPONENT_DEFAULT_CALLBACK)return 0;
 }
 if(word(item+MODEL_NOTE_ORIGINAL_TIME)!=word(item+MODEL_NOTE_CURRENT_TIME)||word(item+MODEL_NOTE_ORIGINAL_TIME_HIGH)!=word(item+MODEL_NOTE_CURRENT_TIME_HIGH)||
    ((word(item+MODEL_NOTE_ORIGINAL_TYPE)^word(item+MODEL_NOTE_CURRENT_TYPE))&~0x800u))return 0;
 /* Compare copied event payload, excluding native record alignment bytes. */
 for(unsigned i=0;i<37;i++)if(note_byte(item+MODEL_NOTE_ORIGINAL_PAYLOAD+i)!=note_byte(item+MODEL_NOTE_CURRENT_PAYLOAD+i))return 0;
 return 1;
}
static int note_children(uint32_t grid,uint32_t *array,unsigned *count){
 *array=word(grid+0x28);*count=word(grid+0x30);
 return *count<=NOTE_CHILD_LIMIT&&(!*count||note_extent(*array,*count*4u));
}
static int note_member(uint32_t grid,NotePosition target){
 uint32_t a;unsigned n;if(!note_children(grid,&a,&n))return 0;
 for(unsigned i=0;i<n;i++)if(word(a+i*4)==target.item){NotePosition now;return note_position(grid,target.item,&now)&&!note_compare(now,target)&&note_quiet(target.item);}
 return 0;
}
static void note_native(unsigned kind,uint32_t grid,uint32_t item){
#ifdef COMMAND_COMPONENT
 if(component_note_call){component_note_call(kind,grid,item);return;}
#endif
 if(kind==0||kind==3)((void(*)(uint32_t,unsigned))(uintptr_t)(observer_image_bias+NOTE_BATCH_RVA))(grid,kind==0);
 else if(kind==1)((void(*)(uint32_t))(uintptr_t)(observer_image_bias+NOTE_CLEAR_RVA))(grid);
 else if(kind==2)((void(*)(uint32_t,uint32_t,unsigned))(uintptr_t)(observer_image_bias+NOTE_SELECT_RVA))(grid,item,1);
 else if(kind==4)((void(*)(uint32_t,int,int))(uintptr_t)(observer_image_bias+NOTE_REVEAL_RVA))(grid,(int)word(item+0x10),(int)word(item+0x14));
}
static int note_select_steps(int steps,unsigned *failure){
 *failure=0;
 if(token()!=command_owner||!steps||steps < -128||steps > 127)return 0;
 uint32_t grid=note_grid();if(!grid)return 0;
 uint32_t holder=word(grid+0x20c),array;unsigned count;
 if(!note_typed(holder,NOTE_HOLDER_VTABLE,0x28)||word(holder+0x24)||word(grid+0x32c)||!note_children(grid,&array,&count))return 0;
 unsigned selected_count=word(grid+0x174);uint32_t selected=word(grid+0x16c);
 if(selected_count>count||(selected_count&&!note_extent(selected,selected_count*4)))return 0;
 NotePosition cursor={0},target={0};int direction=steps>0?1:-1;
 for(unsigned i=0;i<selected_count;i++){
  NotePosition p;if(!note_position(grid,word(selected+i*4),&p)||!note_quiet(p.item)||!note_member(grid,p))return 0;
  if(!cursor.item||note_compare(p,cursor)*direction>0)cursor=p;
 }
 /* One linear pass per represented step, bounded by the native signed byte
  * (128 passes); the X-Touch wrapper still admits at most four. One final
  * selection batch, no allocation, sort or repeated wheel acceleration. */
 int moved=0;
 for(int step=0;step<(steps>0?steps:-steps);step++){
  NotePosition next={0};
  for(unsigned i=0;i<count;i++){
   NotePosition p;if(!note_position(grid,word(array+i*4),&p))continue;
   if(!note_quiet(p.item))return 0;
   if(cursor.item&&note_compare(p,cursor)*direction<=0)continue;
   if(!next.item||note_compare(p,next)*direction<0)next=p;
  }
  if(!next.item)break;
  cursor=next;target=next;moved+=direction;
 }
 if(!target.item)return 0; /* empty grid or clamped endpoint */
 if(note_grid()!=grid||word(grid+0x20c)!=holder||!note_member(grid,target))return 0;
 uint32_t start[2]={word(target.item+MODEL_NOTE_CURRENT_TIME),word(target.item+MODEL_NOTE_CURRENT_TIME_HIGH)};
 unsigned char payload[37];for(unsigned i=0;i<37;i++)payload[i]=(unsigned char)note_byte(target.item+MODEL_NOTE_CURRENT_PAYLOAD+i);
 note_native(0,grid,0);
 note_native(1,grid,0);
 /* This is inside the batch, before deferred callbacks run. Qualify again
  * after clear; do not silently retarget if synchronous notifications changed
  * the collection. End the original batch only while its binding is intact. */
 if(!note_typed(grid,NOTE_GRID_VTABLE,0x4a8)||word(grid+0x20c)!=holder){*failure=C_IDENTITY;return 0;}
 unsigned applied=note_grid()==grid&&note_member(grid,target);
 if(applied)note_native(2,grid,target.item);
 if(!note_typed(grid,NOTE_GRID_VTABLE,0x4a8)||word(grid+0x20c)!=holder){*failure=C_IDENTITY;return 0;}
 note_native(3,grid,0);
 /* Batch end can reconstruct every item. Read selection anew; never use the
  * pre-callback pointer for feedback or scrolling. */
 if(!applied||note_grid()!=grid||word(grid+0x20c)!=holder||word(grid+0x174)!=1){*failure=C_IDENTITY;return 0;}
 selected=word(grid+0x16c);if(!note_extent(selected,4))return 0;
 NotePosition result;if(!note_position(grid,word(selected),&result))return 0;
 if(word(result.item+MODEL_NOTE_CURRENT_TIME)!=start[0]||word(result.item+MODEL_NOTE_CURRENT_TIME_HIGH)!=start[1]||!(word(result.item+MODEL_NOTE_ORIGINAL_TYPE)&0x800u))return 0;
 for(unsigned i=0;i<37;i++)if(note_byte(result.item+MODEL_NOTE_CURRENT_PAYLOAD+i)!=payload[i])return 0;
 uint32_t viewport=word(grid+0x184);
 if(note_typed(viewport,MODEL_NOTE_VIEWPORT_VPTR,0x104))note_native(4,grid,result.item);
 return moved;
}

/* Preserve the external command lane's existing four-step contract. */
static int note_dispatch(int steps,unsigned *failure){
 if(steps>WHEEL_MAX_STEPS)steps=WHEEL_MAX_STEPS;
 if(steps<-WHEEL_MAX_STEPS)steps=-WHEEL_MAX_STEPS;
 return note_select_steps(steps,failure);
}
#ifndef NATIVE_WHEEL_PROBE
/* The callback must still execute its BLX and reference cleanup. A naked ARM
 * return preserves every argument, flags and native stack word. Only saved r3
 * is changed, after the original pair has resolved the native callee. */
__attribute__((naked,noinline)) static void note_wheel_consumed(void){__asm__ volatile("bx lr");}
static void note_native_wheel(Context *c){
 int steps=(int32_t)c->r[1];
 /* Relevance before any pointer read or fault: unrelated native input has its
  * own owner. No input is banked or retried; each callback carries its count. */
 if(c->r[2]>1||steps < -128||steps > 127||c->r[3]!=observer_image_bias+WHEEL_ROTATE_RVA||
    token()!=command_owner||!atomic_load_explicit(&observer_running,memory_order_seq_cst)||
    atomic_load_explicit(&command_in_hook,memory_order_seq_cst))return;
 /* Reuse the command owner's SC entry/close handshake, so observer shutdown
  * cannot finish while the synchronous selection composition holds objects. */
 atomic_store_explicit(&command_in_hook,1,memory_order_seq_cst);
 if(!atomic_load_explicit(&observer_running,memory_order_seq_cst))goto done;
 if(!active()||!ready||loading||reset_pending||depth||atomic_load(&command_state->error)||
    atomic_load(&command_state->trace_error)||atomic_load(&command_violation))goto done;
 #ifdef NATIVE_PREFERENCES
 /* The page-two editor owns focused list selection while it is visible.
  * Preserve the signed count shape of the physical wheel and leave every
  * other native focus path to its original owner. */
 if(native_preferences_wheel(steps)){
  c->r[3]=(uint32_t)(uintptr_t)note_wheel_consumed;
  goto done;
 }
 #endif
 /* The existing Grid shortcut is coarse/Shift-only; the app's selected list
  * above also accepts the unmodified panel wheel. */
 if(c->r[2]!=1)goto done;
 if(word(observer_image_bias+FOCUS_SLOT_RVA)!=c->r[0]||!note_grid())goto done;
 /* Recognition is separate from movement. Empty grids, endpoints and active
  * note gestures must not fall through to the native note-editing handler. */
 c->r[3]=(uint32_t)(uintptr_t)note_wheel_consumed;
 unsigned why=0;(void)note_select_steps(steps,&why);
 if(why)command_fail(why); /* Actual post-mutation identity failure, not no-op. */
done:
 atomic_store_explicit(&command_in_hook,0,memory_order_seq_cst);
}
#endif
