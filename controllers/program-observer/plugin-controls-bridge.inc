/* A non-device origin of the existing coordinator. Only this bridge publishes
 * commands; the UI cells are bounded, replaceable intent, never receipts. */
static MirrorInput plugin_input;
static MirrorBank plugin_bank;
static PluginUiReply plugin_reply;
static uint32_t plugin_session,plugin_seen[8];
static int plugin_ui_live(uint32_t now){
 PluginUiDemand d;
 return input.commands&&atomic_load(&input.commands->plugin_ui.until)>now&&
  plugin_ui_read(&input.commands->plugin_ui.demand,&d,sizeof(d))&&d.active;
}
static void plugin_bridge_init(void){
 memset(&plugin_input,0,sizeof(plugin_input));bank_init(&plugin_bank);
 memset(&plugin_reply,0,sizeof(plugin_reply));memset(plugin_seen,0,sizeof(plugin_seen));plugin_session=0;
 plugin_input.commands=input.commands;plugin_input.flights=coordinator_flights;
 plugin_input.coordinator=&command_coordinator;plugin_input.origin_bank=&plugin_bank;
 command_coordinator.origins[command_coordinator.count++]=&plugin_input;
}
static int plugin_only_flights(void){
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(coordinator_flights[i].flight&&coordinator_flights[i].origin!=&plugin_input)return 0;
 return 1;
}
/* No full mixer/pad copy while a controller-absent app is open. Reuse the
 * runtime snapshot's storage with just the selected identity and Effects. */
static int plugin_copy(const MirrorState *state,CopiedMirror *s,uint32_t now){
 memset(s,0,offsetof(CopiedMirror,tracks));
 memset(&s->master_meter,0,offsetof(CopiedMirror,pads)-offsetof(CopiedMirror,master_meter));
 uint32_t rev=atomic_load_explicit(&state->revision,memory_order_acquire);
 if(rev&1)return 0;
 s->epoch=atomic_load(&state->epoch);s->ready=atomic_load(&state->ready);
 s->project_owner=atomic_load(&state->channel.project_owner);
 copy_channel(&state->channel,s->project_owner,CF_SELECTION,&s->selection);
 unsigned count=atomic_load(&state->count);if(count>MIRROR_TRACKS)return 0;
 if(s->selection.available)for(unsigned i=0;i<count;i++){
  const MirrorBinding *b=state->tracks+i;if(atomic_load(&b->track)!=s->selection.bits)continue;
  CopiedTrack *t=s->tracks;memset(t,0,sizeof(*t));
  t->serial=atomic_load(&b->serial);t->track=atomic_load(&b->track);t->program=atomic_load(&b->program);
  t->kind=atomic_load(&b->kind);t->incarnation=atomic_load(&b->incarnation);t->binding=atomic_load(&b->binding);
  t->vptr=atomic_load(&b->vptr);t->track_owner=atomic_load(&b->track_owner);t->program_owner=atomic_load(&b->program_owner);
  s->count=1;s->selected_serial=t->serial;break;
 }
 atomic_thread_fence(memory_order_acquire);
 if(rev!=atomic_load_explicit(&state->revision,memory_order_relaxed))return 0;
 s->revision=rev;s->heartbeat=atomic_load(&state->heartbeat);s->alive=atomic_load(&state->alive);s->error=atomic_load(&state->error);
 s->effects_available=effects_copy_read(&state->effects,&s->effects)&&s->ready&&s->effects.epoch==s->epoch&&s->effects.serial==s->selected_serial&&s->effects.tick<=now&&now-s->effects.tick<EFFECT_FRESH_MS;
 return 1;
}
static MirrorBank *plugin_bridge_interest(const CopiedMirror *s,uint32_t now,MirrorBank *controller){
 PluginUiState *state=&input.commands->plugin_ui;PluginUiDemand d={0};
 if(!atomic_load_explicit(&state->demand.revision,memory_order_acquire))return controller;
 unsigned live=atomic_load(&state->until)>now&&plugin_ui_read(&state->demand,&d,sizeof(d))&&d.active&&d.epoch==s->epoch&&d.serial==s->selected_serial;
 MirrorBank *wanted=controller;
 if(!wanted&&live&&d.slot<EFFECT_SLOTS&&d.page==0){
  plugin_bank.assignment=BA_EFFECT;plugin_bank.effects_slot=d.slot;plugin_bank.effects_page=d.page;
  plugin_bank.ready=s->ready&&s->selection.available&&s->count;
  for(unsigned i=0;i<s->count;i++)if(s->tracks[i].serial==s->selected_serial)plugin_bank.selected=bank_identity(s,s->tracks+i);
  wanted=&plugin_bank;
 }
 uint32_t slot=wanted?wanted->effects_slot:EFFECT_LIST,page=wanted?wanted->effects_page:0;
 if(plugin_session!=d.session||plugin_reply.controller!=(unsigned)(controller!=NULL)||plugin_reply.epoch!=s->epoch||plugin_reply.serial!=s->selected_serial||plugin_reply.slot!=slot||plugin_reply.page!=page||!live){
  memset(plugin_input.effects,0,sizeof(plugin_input.effects));
  if(plugin_reply.arbitration<UINT32_MAX)plugin_reply.arbitration++;else live=0;
 }
 plugin_session=d.session;plugin_reply.controller=controller!=NULL;plugin_reply.epoch=s->epoch;
 plugin_reply.serial=s->selected_serial;plugin_reply.slot=slot;plugin_reply.page=page;
 if(controller)plugin_bank=*controller; /* presentation/identity only; no device */
 plugin_reply.submitted=plugin_input.submitted;plugin_reply.settled=plugin_input.settled;
 plugin_reply.refused=plugin_input.refused;plugin_reply.error=plugin_input.error;
 PluginUiReply old;
 if(!plugin_ui_read(&state->reply,&old,sizeof(old))||memcmp(&old,&plugin_reply,sizeof(old)))plugin_ui_write(&state->reply,&plugin_reply,sizeof(plugin_reply));
 atomic_store(&state->reply_until,now<=UINT32_MAX-1000?now+1000:0);
 if(!live)return wanted;
 for(unsigned row=0;row<6;row++){
  PluginUiCell *cell=state->intents+row;unsigned revision=atomic_load_explicit(&cell->revision,memory_order_acquire);
  if(!revision||revision==plugin_seen[row]||(revision&1))continue;
  PluginUiIntent v;if(!plugin_ui_read(cell,&v,sizeof(v)))continue;plugin_seen[row]=revision;
  const EffectsCopy *e=&s->effects;const EffectParameter *p=e->parameters+row;
  if(!wanted||v.session!=d.session||v.arbitration!=plugin_reply.arbitration||v.epoch!=s->epoch||v.serial!=s->selected_serial||v.generation!=e->generation||v.slot>=EFFECT_SLOTS||v.slot!=slot||v.page!=page||v.key!=e->slots[v.slot].key||v.ap!=e->slots[v.slot].ap||v.index!=p->index||p->position!=page*8+row||!s->effects_available||p->status!=EF_READY||now>UINT32_MAX-120000)continue;
  float value;memcpy(&value,&v.bits,4);if(!(value>=0&&value<=1))continue;
  /* A controller's already waiting turn wins over a simultaneous app update. */
  unsigned peer_pending=0;
  for(unsigned o=0;o<command_coordinator.count;o++)if(command_coordinator.origins[o]!=&plugin_input)for(unsigned j=0;j<8;j++){
   const InputEffect *peer=command_coordinator.origins[o]->effects+j;
   if(peer->pending&&peer->request.serial==v.serial&&peer->request.effect_slot==v.slot&&peer->request.effect_index==v.index)peer_pending=1;
  }
  if(peer_pending)continue;
  InputEffect *g=plugin_input.effects+row;g->identity=plugin_bank.selected;g->relative=0;g->pending=1;
  g->request=(CommandRequest){.epoch=v.epoch,.serial=v.serial,.binding=g->identity.binding,.incarnation=g->identity.incarnation,.bits=v.bits,.expires=now+120000,.reserved=EFFECT_PARAMETER,.track_owner=e->track_owner,.program_owner=e->program_owner,.field_incarnation=e->generation,.project_owner=s->project_owner,.effect_generation=e->generation,.effect_slot=v.slot,.effect_index=v.index,.effect_key=v.key,.effect_ap=v.ap,.effect_position=p->position};
 }
 return wanted;
}
