#include "plugin-controls-binding.h"
#include <stdio.h>
static PluginUiDemand plugin_ui_demand;
static PluginUiReply plugin_ui_reply;
static uint32_t plugin_ui_binding,plugin_ui_identity[8],plugin_ui_held[8];
static int plugin_ui_eligible(void){
 return command_state&&mirror_state&&token()==command_owner&&active()&&ready&&!loading&&!reset_pending&&
  !atomic_load(&command_state->stop_requested)&&!atomic_load(&command_state->new_project_intent);
}
static void plugin_ui_cancel(void){
 memset(plugin_ui_held,0,sizeof(plugin_ui_held));
 if(plugin_ui_demand.session<UINT32_MAX)plugin_ui_demand.session++;
 else plugin_ui_demand.active=0;
}
void plugin_controls_active(int enabled){
 if(!command_state||token()!=command_owner)return;
 if(plugin_ui_demand.active!=(unsigned)(enabled!=0)){
  plugin_ui_cancel();plugin_ui_demand.active=enabled&&plugin_ui_demand.session<UINT32_MAX;
  plugin_ui_write(&command_state->plugin_ui.demand,&plugin_ui_demand,sizeof(plugin_ui_demand));
 }
 if(!enabled)atomic_store_explicit(&command_state->plugin_ui.until,0,memory_order_release);
}
void plugin_controls_navigate(uint32_t intent){
 if(!plugin_ui_eligible()||!plugin_ui_demand.active||plugin_ui_reply.controller)return;
 if(intent==MPCLEARN_PLUGIN_PREVIOUS_INSERT&&plugin_ui_demand.slot)plugin_ui_demand.slot--;
 else if(intent==MPCLEARN_PLUGIN_NEXT_INSERT&&plugin_ui_demand.slot+1<EFFECT_SLOTS)plugin_ui_demand.slot++;
 else return;
 plugin_ui_demand.page=0;plugin_ui_cancel();
 plugin_ui_write(&command_state->plugin_ui.demand,&plugin_ui_demand,sizeof(plugin_ui_demand));
}
int plugin_controls_snapshot(MpclearnPluginControlsSnapshot *v){
 if(!v)return 0;
 memset(v,0,sizeof(*v));
 snprintf(v->title,sizeof(v->title),"3 BAND EQ");
 snprintf(v->status,sizeof(v->status),"Waiting for selected track");
 if(!plugin_ui_eligible()||!plugin_ui_demand.active)return 1;
 uint32_t now=atomic_load(&mirror_state->heartbeat),selected=0;
 ChannelCell *selection=channel_cell(project+channel_offset(CF_SELECTION));
 if(selection&&atomic_load(&selection->live))for(unsigned i=0;i<count;i++)if(tracks[i].track==atomic_load(&selection->bits)){selected=tracks[i].serial;break;}
 if(plugin_ui_demand.epoch!=epoch||plugin_ui_demand.serial!=selected){
  plugin_ui_cancel();plugin_ui_demand.epoch=epoch;plugin_ui_demand.serial=selected;
 }
 PluginUiState *state=&command_state->plugin_ui;
 PluginUiDemand published;
 if(!plugin_ui_read(&state->demand,&published,sizeof(published))||memcmp(&published,&plugin_ui_demand,sizeof(published)))
  if(!plugin_ui_write(&state->demand,&plugin_ui_demand,sizeof(plugin_ui_demand)))return 1;
 atomic_store_explicit(&state->until,now<=UINT32_MAX-1000?now+1000:0,memory_order_release);
 PluginUiReply reply;
 if(!selected||!plugin_ui_read(&state->reply,&reply,sizeof(reply))||atomic_load(&state->reply_until)<=now||reply.epoch!=epoch||reply.serial!=selected){
  snprintf(v->status,sizeof(v->status),"Waiting for control service");return 1;
 }
 plugin_ui_reply=reply;v->navigation=!reply.controller;
 const EffectsCopy *e=&effects_current;
 uint32_t identity[8]={epoch,selected,e->generation,reply.arbitration,reply.slot,reply.page,0,0};
 if(reply.slot<EFFECT_SLOTS){identity[6]=e->slots[reply.slot].key;identity[7]=e->slots[reply.slot].ap;}
 if(memcmp(identity,plugin_ui_identity,sizeof(identity))){
  memcpy(plugin_ui_identity,identity,sizeof(identity));memset(plugin_ui_held,0,sizeof(plugin_ui_held));
  if(plugin_ui_binding<UINT32_MAX-1)plugin_ui_binding++;else return 1;
 }
 v->binding=plugin_ui_binding;
 snprintf(v->status,sizeof(v->status),reply.controller?"Following controller insert / page":"Insert %u - select the 3BandEQ insert",reply.slot+1);
 if(reply.error){snprintf(v->status,sizeof(v->status),"Control service unavailable");return 1;}
 if(atomic_load(&mirror_state->effects.invalid)||atomic_load(&mirror_state->effects.error)||e->status!=EF_READY||e->epoch!=epoch||e->serial!=selected||e->slot!=reply.slot||e->page!=reply.page||e->tick>now||now-e->tick>=EFFECT_FRESH_MS||reply.slot>=EFFECT_SLOTS)return 1;
 const EffectSlot *slot=e->slots+reply.slot;
 if(slot->status==EF_EMPTY){snprintf(v->status,sizeof(v->status),"Empty insert");return 1;}
 if(slot->count!=6||!slot->description.valid||slot->description.uid!=0x44334551u||strcmp(slot->description.format,"VST")||reply.page){
  snprintf(v->status,sizeof(v->status),"Select a 3BandEQ insert%s",reply.controller?" on the controller":"");return 1;
 }
 snprintf(v->title,sizeof(v->title),"3 BAND EQ - INSERT %u",reply.slot+1);
 snprintf(v->status,sizeof(v->status),"%s - confirmed values",reply.controller?"Following controller":"Touch a band or crossover");
 for(unsigned i=0;i<6;i++){
  const EffectParameter *p=e->parameters+i;
  if(p->status!=EF_READY||p->index!=i||p->position!=i||p->tick>now||now-p->tick>=EFFECT_FRESH_MS)continue;
  float value;memcpy(&value,&p->bits,4);if(!(value>=0&&value<=1))continue;
  v->rows[i].ready=1;v->rows[i].value=value;
  memcpy(v->rows[i].name,p->name,32);memcpy(v->rows[i].text,p->text,32);
 }
 return 1;
}
void plugin_controls_gesture(uint32_t binding,uint32_t row,uint32_t event,double value){
 if(row>=6)return;
 if(event==MPC_UI_SLIDER_DRAG_ENDED){plugin_ui_held[row]=0;return;}
 MpclearnPluginControlsSnapshot current;
 if(!plugin_controls_snapshot(&current)||!current.binding||binding!=current.binding||!current.rows[row].ready)return;
 if(event==MPC_UI_SLIDER_DRAG_STARTED){plugin_ui_held[row]=binding;return;}
 if(event!=MPC_UI_SLIDER_VALUE_CHANGED||plugin_ui_held[row]!=binding||!(value>=0&&value<=1))return;
 const EffectsCopy *e=&effects_current;float desired=(float)value;uint32_t bits;memcpy(&bits,&desired,4);
 PluginUiIntent intent={plugin_ui_demand.session,plugin_ui_reply.arbitration,epoch,e->serial,e->generation,e->slot,e->page,e->parameters[row].index,e->slots[e->slot].key,e->slots[e->slot].ap,bits};
 (void)plugin_ui_write(command_state->plugin_ui.intents+row,&intent,sizeof(intent));
}
