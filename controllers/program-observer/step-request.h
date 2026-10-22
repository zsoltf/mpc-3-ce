#ifndef MPC_STEP_REQUEST_H
#define MPC_STEP_REQUEST_H
#include "command-state.h"
static inline int step_request_bind_view(CommandRequest *r,const StepCopy *s,unsigned view,unsigned sound,unsigned cell,uint32_t now){
 if(view>=STEP_VIEWS||sound>=4||cell>=STEP_CELLS||s->status!=STEP_READY||s->tick>now||now-s->tick>=STEP_FRESH_MS||now>UINT32_MAX-2000)return 0;
 const StepProjection *v=s->views+view;
 if(v->status!=STEP_READY||(!v->target.event_owner&&v->generation!=s->event_generation)||v->tick>now||now-v->tick>=STEP_FRESH_MS||v->pad+sound>=128||v->notes[sound]>127||!(v->valid[cell/32]&(1u<<(cell%32))))return 0;
 StepCopy bound;
 if(v->target.event_owner){const StepTarget *t=&v->target;bound=*s;bound.epoch=t->epoch;bound.project_owner=t->project_owner;bound.track_owner=t->track_owner;bound.program_owner=t->program_owner;bound.serial=t->serial;bound.binding=t->binding;bound.incarnation=t->incarnation;bound.control=t->control;bound.sequence=t->sequence;bound.program=t->program;bound.event_owner=t->event_owner;bound.division_bits=t->division_bits;memcpy(bound.extent,t->extent,sizeof(bound.extent));s=&bound;}
 unsigned before=(v->on[sound][cell/32]>>(cell%32))&1u;
 *r=(CommandRequest){.epoch=s->epoch,.serial=s->serial,.binding=s->binding,.incarnation=s->incarnation,.bits=!before,.created=now,.expires=now+2000,.before_bits=before,.reserved=STEP_TOGGLE,.track_owner=s->track_owner,.program_owner=s->program_owner,.project_owner=s->project_owner,.pad_index=v->pad+sound,.effect_generation=s->event_owner,.effect_index=s->extent[0],.effect_key=s->extent[1],.effect_ap=v->notes[sound],.qlink_root=s->control,.qlink_wrapper=s->event_owner,.qlink_provider=s->sequence,.qlink_generation=s->division_bits,.qlink_index=v->first_step+cell,.effect_slot=v->target.event_owner?view+1:0,.effect_position=v->target.bind_generation};return 1;
}
static inline int step_request_bind(CommandRequest *r,const StepCopy *s,unsigned index,uint32_t now){return step_request_bind_view(r,s,0,0,index,now);}
/* Stop the track of this view's bound clip: the same pinned identity as an edit
 * of its first sound and visible step, as a one-shot request. */
static inline int step_request_stop_view(CommandRequest *r,const StepCopy *s,unsigned view,uint32_t now){
 if(view>=STEP_VIEWS||s->status!=STEP_READY||s->tick>now||now-s->tick>=STEP_FRESH_MS||now>UINT32_MAX-2000)return 0;
 const StepProjection *v=s->views+view;
 if(v->status!=STEP_READY||!v->target.event_owner||v->tick>now||now-v->tick>=STEP_FRESH_MS||v->pad>=128||v->notes[0]>127)return 0;
 const StepTarget *t=&v->target;
 *r=(CommandRequest){.epoch=t->epoch,.serial=t->serial,.binding=t->binding,.incarnation=t->incarnation,.bits=1,.created=now,.expires=now+2000,.before_bits=0,.reserved=STEP_STOP,.track_owner=t->track_owner,.program_owner=t->program_owner,.project_owner=t->project_owner,.pad_index=v->pad,.effect_generation=t->event_owner,.effect_index=t->extent[0],.effect_key=t->extent[1],.effect_ap=v->notes[0],.qlink_root=t->control,.qlink_wrapper=t->event_owner,.qlink_provider=t->sequence,.qlink_generation=t->division_bits,.qlink_index=v->first_step,.effect_slot=view+1,.effect_position=t->bind_generation};
 return command_step_format(r);
}
#endif
