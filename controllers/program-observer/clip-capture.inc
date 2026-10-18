/* Clip Matrix state uses the existing UI service and expiring collection
 * interest. Only one requested eight-by-eight window is read per visit. */
#include "clip-native.inc"
static ClipCopy clip_current;
static uint32_t clip_next_tick;
static unsigned clip_view_cursor;
static void clip_publish(const ClipCopy *value){
 ClipState *s=&mirror_state->clips;unsigned rev=atomic_load(&s->revision);if(rev>UINT32_MAX-2)return;
 uint32_t words[CLIP_WORDS];memcpy(words,value,sizeof(words));atomic_store_explicit(&s->revision,rev+1,memory_order_release);
 for(unsigned i=0;i<CLIP_WORDS;i++)atomic_store_explicit(s->words+i,words[i],memory_order_relaxed);
 atomic_store_explicit(&s->revision,rev+2,memory_order_release);
}
static void clip_service(void){
 unsigned now=atomic_load(&mirror_state->heartbeat);ClipDemand demand={0};
 if(!clip_demand_read(&command_state->clip_interest,&demand)||demand.until<=now||demand.epoch!=epoch||!demand.enabled){
  if(clip_current.status!=CLIP_SOURCE_OFF){memset(&clip_current,0,sizeof(clip_current));clip_publish(&clip_current);}return;
 }
 if(now<clip_next_tick)return;
 clip_next_tick=now>UINT32_MAX-20?UINT32_MAX:now+20;
 uint32_t lanes=0;unsigned columns=0,rows=0,why=clip_native_context(&lanes,&columns,&rows);
 if(why){memset(&clip_current,0,sizeof(clip_current));clip_current.status=CLIP_SOURCE_UNAVAILABLE;clip_current.epoch=epoch;clip_current.tick=now;clip_publish(&clip_current);return;}
 uint32_t owner=channel_owner_id(project);
 if(clip_current.epoch!=epoch||clip_current.lanes!=lanes||clip_current.project_owner!=owner||clip_current.columns!=columns||clip_current.rows!=rows)memset(&clip_current,0,sizeof(clip_current));
 clip_current.status=CLIP_SOURCE_READY;clip_current.epoch=epoch;clip_current.tick=now;clip_current.project_owner=owner;clip_current.lanes=lanes;clip_current.columns=columns;clip_current.rows=rows;
 unsigned start=clip_view_cursor,refreshed=0;
 for(unsigned n=0;n<CLIP_VIEWS;n++){
  unsigned i=(start+n)%CLIP_VIEWS;ClipProjection *old=clip_current.views+i;
  if(!(demand.enabled&(1u<<i))){memset(old,0,sizeof(*old));continue;}
  if(refreshed)continue;
  refreshed=1;clip_view_cursor=(i+1)%CLIP_VIEWS;
  ClipProjection next={.status=CLIP_SOURCE_READY,.tick=now,.first_column=demand.column[i],.first_row=demand.row[i]};
  /* Native row0 is Arrangement; subsequent rows retain native scene numbers. */
  if(next.first_row>UINT32_MAX-7||next.first_column>UINT32_MAX-7){next.status=CLIP_SOURCE_UNAVAILABLE;*old=next;continue;}
  for(unsigned column=0;column<CLIP_SIDE;column++){
   unsigned native_column=next.first_column+column;if(native_column>=columns)continue;
   uint32_t track=clip_native_track(lanes,native_column);if(!typed(track,0x6935554))continue;
   next.track_owner[column]=channel_owner_id(track);
   for(unsigned row=0;row<CLIP_SIDE;row++){
    unsigned native_row=next.first_row+row;if(native_row>=rows)continue;
    ClipCell *cell=next.cells+row*CLIP_SIDE+column;unsigned state=clip_native_state(lanes,native_column,native_row);
    if(state>CLIP_WILL_STOP_OVERDUBBING){next.status=CLIP_SOURCE_UNAVAILABLE;continue;}
    ClipNativeShared clip=clip_native_lookup(lanes,native_column,native_row);
    if(clip.object&&!typed(clip.object,0x692f010)){clip_native_release(&clip);next.status=CLIP_SOURCE_UNAVAILABLE;continue;}
    cell->state=state;cell->identity=clip.object;clip_native_release(&clip);
   }
  }
  for(unsigned row=0;row<CLIP_SIDE;row++)if(next.first_row+row<rows)next.scene_state[row]=clip_native_scene_state(lanes,next.first_row+row);
  next.generation=old->generation;
  if(next.status!=old->status||next.first_column!=old->first_column||next.first_row!=old->first_row||memcmp(next.track_owner,old->track_owner,sizeof(next.track_owner))||memcmp(next.scene_state,old->scene_state,sizeof(next.scene_state))||memcmp(next.cells,old->cells,sizeof(next.cells)))next.generation=next.generation==UINT32_MAX?1:next.generation+1;
  if(word(root+0x754)!=lanes||word(root+0x24c)!=project||word(lanes+0x184)!=project)next.status=CLIP_SOURCE_UNAVAILABLE;
  *old=next;
 }
 clip_publish(&clip_current);
}
