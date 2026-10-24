#ifndef MPC_CLIP_REQUEST_H
#define MPC_CLIP_REQUEST_H
#include "command-state.h"
static inline int clip_request_bind(CommandRequest *r,const ClipCopy *s,unsigned view,unsigned intent,unsigned column,unsigned row,uint32_t now){
 if(view>=CLIP_VIEWS||column>=8||row>=8||intent<CLIP_LAUNCH||intent>CLIP_LAST||s->status!=CLIP_SOURCE_READY||s->tick>now||now-s->tick>=CLIP_FRESH_MS||now>UINT32_MAX-2000)return 0;
 if(clip_copy_intent(intent))return 0;
 const ClipProjection *v=s->views+view;
 if(v->status!=CLIP_SOURCE_READY||v->tick>now||now-v->tick>=CLIP_FRESH_MS||v->first_row+row>=s->rows||v->first_column+column>=s->columns)return 0;
 const ClipCell *cell=v->cells+row*8+column;unsigned scene=clip_scene_intent(intent);
 if(!scene&&!v->track_owner[column])return 0;
 *r=(CommandRequest){.epoch=s->epoch,.bits=intent,.created=now,.expires=now+2000,
  .before_bits=scene?v->scene_state[row]:cell->state,.reserved=CLIP_ACTION,
  .track_owner=scene?0:v->track_owner[column],.project_owner=s->project_owner,
  .pad_index=scene?0:v->first_column+column,.effect_index=v->first_row+row,
  .effect_generation=scene?0:cell->identity,.qlink_root=s->lanes};
 return command_clip_format(r);
}
/* Only coordinates/identities survive a hold. MPC reads contents at execution. */
typedef struct {uint32_t kind,epoch,project,lanes,column,row,track,clip,columns;} ClipEditSource;
static inline int clip_edit_source(ClipEditSource *out,const ClipCopy *s,unsigned view,unsigned column,unsigned row,unsigned whole,uint32_t now){
 if(view>=CLIP_VIEWS||column>=8||row>=8||s->status!=CLIP_SOURCE_READY||s->tick>now||now-s->tick>=CLIP_FRESH_MS)return 0;
 const ClipProjection *v=s->views+view;unsigned native_row=v->first_row+row;
 if(v->status!=CLIP_SOURCE_READY||v->tick>now||now-v->tick>=CLIP_FRESH_MS||!native_row||native_row>=s->rows||!s->columns||s->columns>128)return 0;
 const ClipCell *cell=v->cells+row*8+column;
 if(!whole&&(!cell->identity||!v->track_owner[column]||v->first_column+column>=s->columns))return 0;
 *out=(ClipEditSource){whole?2u:1u,s->epoch,s->project_owner,s->lanes,whole?0:v->first_column+column,native_row,whole?0:v->track_owner[column],whole?0:cell->identity,s->columns};return 1;
}
static inline int clip_copy_request(CommandRequest *r,const ClipCopy *s,unsigned view,const ClipEditSource *source,unsigned column,unsigned row,uint32_t now){
 if(view>=CLIP_VIEWS||column>=8||row>=8||!source->kind||source->epoch!=s->epoch||source->project!=s->project_owner||source->lanes!=s->lanes||source->columns!=s->columns||now>UINT32_MAX-2000)return 0;
 const ClipProjection *v=s->views+view;unsigned whole=source->kind==2,native_row=v->first_row+row,native_column=v->first_column+column;
 if(s->status!=CLIP_SOURCE_READY||v->status!=CLIP_SOURCE_READY||s->tick>now||now-s->tick>=CLIP_FRESH_MS||v->tick>now||now-v->tick>=CLIP_FRESH_MS||!native_row||native_row>=s->rows)return 0;
 const ClipCell *cell=v->cells+row*8+column;
 if(!whole&&(native_column>=s->columns||!v->track_owner[column]||cell->identity||(native_column==source->column&&native_row==source->row)))return 0;
 *r=(CommandRequest){.epoch=s->epoch,.bits=whole?CLIP_COPY_ROW:CLIP_COPY,.created=now,.expires=now+2000,.before_bits=whole?v->scene_state[row]:cell->state,.reserved=CLIP_ACTION,
  .track_owner=whole?0:v->track_owner[column],.project_owner=s->project_owner,.pad_index=whole?0:native_column,.effect_index=native_row,.qlink_root=s->lanes,
  .program_owner=source->track,.pad_owner=source->clip,.effect_slot=source->column,.effect_key=source->row,.effect_position=source->columns};
 return command_clip_format(r);
}
#endif
