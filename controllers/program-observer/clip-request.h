#ifndef MPC_CLIP_REQUEST_H
#define MPC_CLIP_REQUEST_H
#include "command-state.h"
static inline int clip_request_bind(CommandRequest *r,const ClipCopy *s,unsigned view,unsigned intent,unsigned column,unsigned row,uint32_t now){
 if(view>=CLIP_VIEWS||column>=8||row>=8||intent<CLIP_LAUNCH||intent>CLIP_SELECT||s->status!=CLIP_SOURCE_READY||s->tick>now||now-s->tick>=CLIP_FRESH_MS||now>UINT32_MAX-2000)return 0;
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
#endif
