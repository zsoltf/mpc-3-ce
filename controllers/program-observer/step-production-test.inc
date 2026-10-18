#include "step-request.h"
static unsigned step_fixture_calls,step_fixture_index,step_fixture_bits,step_fixture_note,step_fixture_track_lookups;
static uint32_t step_fixture_pattern[128][2];
static uint32_t step_fixture_clip;
static unsigned step_fixture_clip_state(uint32_t lanes,unsigned column,unsigned row){(void)lanes;(void)column;(void)row;return CLIP_PLAYING;}
static uint32_t step_fixture_track_lookup(uint32_t lanes,unsigned column){(void)lanes;step_fixture_track_lookups++;return column?0:tracks[0].track;}
static ClipNativeShared step_fixture_clip_lookup(uint32_t lanes,unsigned column,unsigned row){(void)lanes;(void)row;return (ClipNativeShared){column?0:step_fixture_clip,0};}

static int step_fixture_scan(uint32_t owner,const uint32_t notes[4],int64_t first,unsigned division,unsigned tolerance,unsigned cells,uint32_t on[4][2]){
 require(owner==step_fixture_clip+0x388&&division==240&&tolerance==48,"absolute scan uses native source and division");
 memset(on,0,8*sizeof(uint32_t));unsigned offset=(unsigned)(first/division);
 for(unsigned sound=0;sound<4;sound++)for(unsigned cell=0;cell<cells;cell++)if(notes[sound]<128&&offset+cell<64&&(step_fixture_pattern[notes[sound]][(offset+cell)/32]&(1u<<((offset+cell)%32))))on[sound][cell/32]|=1u<<(cell%32);
 return 1;
}
static unsigned step_fixture_edit(const CommandRequest *r,const StepCopy *s){
 require(s->control==word(root+0x398)&&r->effect_ap==word(s->program+0x1170+4*r->pad_index),"edit retains explicit mapped drum identity");
 step_fixture_calls++;step_fixture_index=r->qlink_index;step_fixture_bits=r->bits;step_fixture_note=r->effect_ap;return 0;
}
static void step_fixture_projection(StepCopy *s){
 require(!step_native_read(s),"absolute source available");StepDemand d={.enabled=3};d.pad[0]=2;d.pad[1]=0;d.first_step[1]=16;
 step_project_views(s,&d);step_project_views(s,&d);
}
/* Native mutation/lookup are substituted, but dispatch, interest withdrawal,
 * completion events and the bridge's sealed-receipt consumer are production. */
static void step_lifecycle_checks(uint32_t ctrl,uint32_t clip){
 put(project+0xa0,tracks[0].track);put(project+0x1e8,clip);put(ctrl+0x68,clip+0x388);put(project+0x1b4,tracks[0].track);put(project+0x1bc,1);
 put(tracks[0].program+0x1170+4*2,2);command_retire();
 for(unsigned branch=0;branch<4;branch++){
  unsigned now=100+100*branch,generation=10+2*branch;
  for(unsigned i=0;i<STEP_VIEWS;i++){step_bound_release(i);step_bound_attempt_epoch[i]=0;}
  step_fixture_clip=clip;put(ctrl+0x90,2);memset(step_fixture_pattern,0,sizeof(step_fixture_pattern));
  atomic_store(&fixture->heartbeat,now);atomic_store(&command_state->step_interest.until,1000);atomic_store(&command_state->step_interest.enabled,3);atomic_store(&command_state->step_interest.bind_generation[0],generation);atomic_store(&command_state->step_interest.pad[0],2);
  StepDemand demand;require(step_demand_read(&command_state->step_interest,&demand),"lifecycle demand read");StepCopy source;require(!step_native_read(&source),"lifecycle native source");step_project_views(&source,&demand);step_project_views(&source,&demand);
  CommandRequest r;require(step_request_bind_view(&r,&source,0,0,33,now),"lifecycle exact step target");unsigned calls=step_fixture_calls,at=submit_fixture(r),seq=atomic_load(&command_state->slots[at].published);
  require(step_pending&&(step_fixture_calls==calls+1)&&step_fixture_note==2,"exactly one dispatched native edit before view transition");
  if(branch==0)atomic_store(&command_state->step_interest.enabled,2);
  if(branch==1){atomic_store(&command_state->step_interest.bind_generation[0],generation+1);atomic_store(&command_state->step_interest.pad[0],5);put(ctrl+0x90,5);}
  if(branch==2){atomic_store(&command_state->step_interest.enabled,0);atomic_store(&command_state->step_interest.until,0);}
  if(branch==3)step_fixture_clip=0; /* The rooted slot no longer owns the Clip. */
  Context drain=context();drain.r[0]=ptr(queue_object)+4;atomic_store(&fixture->heartbeat,now+1);call(&drain,COMMAND_DRAIN);
  if(branch!=3){
   require(step_pending&&step_bindings[0].clip.object==clip&&step_bindings[0].bind_generation==generation&&atomic_load(&command_state->slots[at].done)!=seq,"withdrawal/rebind/disconnect retains dispatched binding until native state arrives");
   if(branch!=2)require(step_current.views[1].status==STEP_READY,"pending withdrawn surface does not interrupt peer projection");
   step_fixture_pattern[2][1]=2;atomic_store(&fixture->heartbeat,now+2);call(&drain,COMMAND_DRAIN);
  }
  require(!step_pending&&atomic_load(&command_state->slots[at].done)==seq&&step_fixture_calls==calls+1,"view transition never retries native edit");command_retire();
  MirrorInput input={.commands=command_state};InputFlight *flight=input.flights+at;flight->flight=seq;flight->field=STEP_TOGGLE;flight->bits=r.bits;flight->expires=r.expires;flight->step_request=r;
  MirrorBank bank;bank_init(&bank);MIRROR_COPY(copy);copy.alive=copy.ready=1;copy.epoch=epoch;copy.heartbeat=now+3;input_events(&input,flight,at);input_settle(&input,&bank,&copy,now+3);
  if(branch==3){
   require(input.error==C_IDENTITY&&flight->step_events[2].bits==UINT32_MAX&&!flight->acknowledged&&atomic_load(&command_state->slots[at].settled)!=seq,"genuinely deleted target cannot certify completion from desired or stale bits");
   atomic_store(&fixture->heartbeat,now+30);call(&drain,COMMAND_DRAIN);require(step_current.views[0].status!=STEP_READY,"deleted target clears projection");
  }else{
   require(!input.error&&flight->acknowledged&&atomic_load(&command_state->slots[at].settled)==seq&&!atomic_load(&command_state->error)&&!atomic_load(&command_state->trace_error),"production receipt settles without bridge or shared owner error");command_retire();require(atomic_load(&command_state->slots[at].reclaimed)==seq,"withdrawn request storage reclaimed");
   atomic_store(&command_state->step_interest.until,1000);atomic_store(&command_state->step_interest.enabled,3);atomic_store(&command_state->step_interest.bind_generation[0],generation+1);atomic_store(&command_state->step_interest.pad[0],5);put(ctrl+0x90,5);
   atomic_store(&fixture->heartbeat,now+30);call(&drain,COMMAND_DRAIN);atomic_store(&fixture->heartbeat,now+51);call(&drain,COMMAND_DRAIN);
   require(step_bindings[0].bind_generation==generation+1&&step_bindings[0].pad==5&&step_current.views[0].status==STEP_READY&&step_current.views[1].status==STEP_READY&&step_fixture_calls==calls+1,"re-entry binds current target only after old completion; peer stays usable");
  }
 }
 step_fixture_clip=clip;
 puts("PASS pending step lifecycle: withdrawal, deferred rebind and disconnect settle/reclaim one edit with peer usable; genuine slot deletion refuses certification");
}
/* Selected-source readiness and binding retention are production code; only
 * native rooted lookup and note scanning use the existing fixture adapters. */
static void step_initial_source_checks(uint32_t ctrl,uint32_t clip){
 StepDemand d={.enabled=1};d.bind_generation[0]=101;d.pad[0]=2;
 StepCopy s;unsigned lookups=step_fixture_track_lookups;
 put(tracks[0].program+0x1140,1); /* Selected program is not a Drum program. */
 for(unsigned i=0;i<32;i++){
  require(step_native_read(&s)==C_NOT_DRUM,"initial Plugin selection has no Drum source");step_project_views(&s,&d);
  require(s.views[0].status==STEP_UNAVAILABLE&&!step_bindings[0].clip.object,"unavailable initial source does not invent a binding");
 }
 require(step_fixture_track_lookups==lookups,"repeated unavailable source cycles perform no native acquisition lookup");
 put(tracks[0].program+0x1140,0);
 require(!step_native_read(&s),"selected Drum becomes ready without mode reentry");step_project_views(&s,&d);
 require(s.views[0].status==STEP_READY&&s.views[0].target.clip==clip&&s.views[0].target.bind_generation==101,"initial Plugin to Drum recovers the same generation");
 d.enabled=3;d.bind_generation[1]=202;d.pad[1]=0;
 require(!step_native_read(&s),"peer source ready");step_project_views(&s,&d);step_project_views(&s,&d);
 require(s.views[1].status==STEP_READY&&s.views[1].target.clip==clip,"independent peer bound");
 /* A new entry must release the old target even before a source is ready. */
 d.bind_generation[0]=102;put(project+0xa0,0);
 require(step_native_read(&s)==C_NOT_DRUM,"new generation has unavailable selected source");step_project_views(&s,&d);
 require(!step_bindings[0].clip.object&&s.views[0].status==STEP_UNAVAILABLE&&s.views[1].status==STEP_READY&&s.views[1].target.bind_generation==202,"failed new entry releases old target without disturbing peer");
 put(project+0xa0,tracks[0].track);
 require(!step_native_read(&s),"source ready for deferred new entry");step_project_views(&s,&d);
 require(s.views[0].status==STEP_READY&&s.views[0].target.bind_generation==102&&s.views[1].status==STEP_READY&&s.views[1].target.bind_generation==202,"deferred acquisition recovers only its own view");
 put(project+0xa0,0);require(step_native_read(&s)==C_NOT_DRUM,"foreign selection unavailable");step_project_views(&s,&d);
 require(s.views[0].status==STEP_READY&&s.views[1].status==STEP_READY&&s.views[0].target.clip==clip&&s.views[1].target.clip==clip,"successful bindings remain independent of foreign selection");
 /* Rooted membership disappears; a subsequent valid foreign Clip must not
  * turn the already-consumed generation into an initial acquisition again. */
 step_fixture_clip=0;step_project_views(&s,&d);
 require(!step_bindings[0].clip.object&&!step_bindings[1].clip.object,"deleted rooted targets release successful bindings");
 static unsigned char other_clip[0x400];uint32_t other=ptr(other_clip);put(other,0x692f010);put(other+0x388,0x6896efc);
 step_fixture_clip=other;put(project+0xa0,tracks[0].track);put(project+0x1e8,other);put(ctrl+0x68,other+0x388);lookups=step_fixture_track_lookups;
 for(unsigned i=0;i<8;i++){require(!step_native_read(&s),"foreign Drum source is ready after invalidation");step_project_views(&s,&d);}
 require(!step_bindings[0].clip.object&&!step_bindings[1].clip.object&&s.views[0].status==STEP_UNAVAILABLE&&s.views[1].status==STEP_UNAVAILABLE&&step_fixture_track_lookups==lookups,"invalidated successful generations never acquire a foreign target");
 d.bind_generation[0]++;step_project_views(&s,&d);
 require(s.views[0].status==STEP_READY&&s.views[0].target.clip==other&&s.views[0].target.bind_generation==103&&s.views[1].status==STEP_UNAVAILABLE,"explicit new entry acquires current foreign Clip without reviving invalidated peer");
 CommandRequest stale={.effect_slot=1,.effect_position=102};require(step_request_match(&stale,&s)==C_IDENTITY,"previous binding generation cannot admit a newly submitted edit");
 step_fixture_clip=clip;put(project+0x1e8,clip);put(ctrl+0x68,clip+0x388);
 for(unsigned i=0;i<STEP_VIEWS;i++){step_bound_release(i);step_bound_attempt_epoch[i]=0;}
 puts("PASS initial unavailable Drum binding: Plugin to Drum same-generation recovery, no repeated unavailable acquisition, new-entry release, peer isolation, retained selection independence and no retarget after invalidation");
}
static void step_owner_checks(void){
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"step owner fixture allocation");
 static unsigned char ctrl[0xaa38],ctx[0x3cec],clip[0x400],quantiser[0x74],lanes[0x250];static uint32_t sequence_slots[1];
 initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 seed_fixture(0,0,0x3f000000,1);load_fixture();
 command_state->magic=COMMAND_MAGIC;command_state->version=COMMAND_VERSION;command_state->bytes=sizeof(*command_state);command_state->capacity=COMMAND_SLOTS;atomic_store(&command_state->step_interest.until,100);
 put(root+0x398,ptr(ctrl));put(root+0x39c,ptr(ctx));put(ptr(ctrl),0x690a8d0);put(ptr(ctrl)+0x68,ptr(clip)+0x388);put(ptr(ctrl)+0xa938,project+0x1c0);put(project+0x1e8,ptr(clip));put(ptr(clip),0x692f010);put(ptr(ctrl)+0xa940,ptr(quantiser));put(ptr(ctrl)+0xd8,3840);put(ptr(quantiser)+0x70,0x41800000);put(ptr(ctx)+0x1a8,15360);
 put(project+0xa0,tracks[0].track);put(project+0xe4,ptr(ctx));put(tracks[0].program+0x1140,0);for(unsigned i=0;i<128;i++)put(tracks[0].program+0x1170+4*i,i);
 put(root+0x754,ptr(lanes));put(ptr(lanes),0x6894708);put(ptr(lanes)+0x178,root);put(ptr(lanes)+0x184,project);put(ptr(lanes)+0x34,1);put(ptr(lanes)+0x48,9);
 sequence_slots[0]=ptr(ctx);put(project+0xa70,ptr(sequence_slots));put(project+0xa78,1);put(ptr(ctx)+0x250,0);put(ptr(ctx)+0x258,1);
 put(project+0x1b4,tracks[0].track);put(project+0x1b8,ptr(ctx));put(project+0x1bc,1);put(ptr(clip)+0x388,0x6896efc);step_fixture_clip=ptr(clip);component_clip_track=step_fixture_track_lookup;component_clip_lookup=step_fixture_clip_lookup;component_clip_state=step_fixture_clip_state;
 component_step_scan=step_fixture_scan;component_step_edit=step_fixture_edit;step_fixture_calls=0;memset(step_fixture_pattern,0,sizeof(step_fixture_pattern));
 atomic_store(&command_state->step_interest.epoch,epoch);atomic_store(&command_state->step_interest.enabled,3);atomic_store(command_state->step_interest.pad,2);atomic_store(command_state->step_interest.first_step+1,16);
 step_initial_source_checks(ptr(ctrl),ptr(clip));
 StepCopy s;step_fixture_projection(&s);require(s.views[0].valid[0]==UINT32_MAX&&s.views[0].valid[1]==UINT32_MAX&&s.views[1].valid[1]==65535,"independent projections span the full sequence, not the native visible bar");
 CommandRequest r;require(step_request_bind_view(&r,&s,0,0,32,0)&&r.pad_index==2&&r.qlink_index==32,"offscreen nonselected intent pins A03 step33");
 /* A foreign selected track/clip cannot move either already bound view. */
 put(project+0xa0,0);put(project+0x1e8,0);put(ptr(ctrl)+0x68,0);
 StepCopy foreign;require(step_native_read(&foreign)!=C_OK,"new global selection is unavailable as a drum source");StepDemand bound_demand={.enabled=3};bound_demand.pad[0]=2;bound_demand.first_step[1]=16;step_project_views(&foreign,&bound_demand);require(foreign.views[0].status==STEP_READY&&foreign.views[0].target.event_owner==ptr(clip)+0x388&&foreign.views[1].status==STEP_READY,"selected clip changes retain both independently owned drum projections");
 /* Native selection and visible page may change without redirecting this target. */
 put(ptr(ctrl)+0x90,1);put(ptr(ctrl)+0xd0,3840);put(ptr(ctrl)+0xd8,7680);
 unsigned at=submit_fixture(r);require(step_fixture_calls==1&&step_fixture_index==32&&step_fixture_note==2&&step_fixture_bits==1&&atomic_load(&command_state->slots[at].returned)&&!atomic_load(&command_state->slots[at].done),"absolute call return alone cannot settle a step");
 Context drain=context();drain.r[0]=ptr(queue_object)+4;call(&drain,COMMAND_DRAIN);require(!atomic_load(&command_state->slots[at].done),"unchanged native event source cannot settle");
 atomic_store(&command_state->step_interest.enabled,2);step_fixture_pattern[2][1]=1;atomic_store(&fixture->heartbeat,1);call(&drain,COMMAND_DRAIN);require(atomic_load(&command_state->slots[at].done)==1&&!step_pending,"native later changed-state receipt completes");command_retire();
 MirrorInput in={.commands=command_state};InputFlight *tx=in.flights+at;tx->flight=1;tx->field=STEP_TOGGLE;tx->bits=r.bits;tx->expires=r.expires;tx->step_request=r;MirrorBank bank;bank_init(&bank);MIRROR_COPY(copy);copy.alive=copy.ready=1;copy.epoch=epoch;copy.heartbeat=4000;
 input_events(&in,tx,at);require(!in.error,"read actual sealed step receipts");InputFlight saved=*tx;
 tx->step_events[2].tick=r.expires+1;input_settle(&in,&bank,&copy,4000);require(in.error==C_EXPIRED&&!atomic_load(&command_state->slots[at].settled),"native state observed after deadline cannot certify completion");
 *tx=saved;in.error=0;input_settle(&in,&bank,&copy,4000);require(!in.error&&tx->acknowledged&&atomic_load(&command_state->slots[at].settled)==1,"late consumer settles valid native evidence without replay");command_retire();require(atomic_load(&command_state->slots[at].reclaimed)==1&&step_fixture_calls==1&&step_current.views[1].status==STEP_READY,"withdrawn edit reclaimed once and peer remains available");atomic_store(&command_state->step_interest.enabled,3);
 atomic_store(&fixture->heartbeat,2);Context notice=context();notice.r[0]=ptr(ctrl)+12;notice.r[1]=ptr(clip)+0x388;notice.r[4]=ptr(clip)+0x388;step_bound_changed(&notice);step_project_views(&s,&bound_demand);step_project_views(&s,&bound_demand);
 require(s.views[0].on[0][1]==1&&s.views[1].on[2][0]==65536,"one native edit updates both independently positioned surfaces");
 require(step_request_bind_view(&r,&s,0,0,32,2)&&r.bits==0,"lit absolute step requests removal");step_fixture_pattern[2][1]=0;at=submit_fixture(r);require(step_fixture_calls==1&&atomic_load(&command_state->slots[at].rejected)==C_SOURCE_CHANGED,"external native edit before dispatch refuses stale toggle");command_retire();
 step_bound_changed(&notice);step_project_views(&s,&bound_demand);step_project_views(&s,&bound_demand);require(step_request_bind_view(&r,&s,0,0,32,2),"fresh empty step");put(tracks[0].program+0x1170+4*2,90);at=submit_fixture(r);require(step_fixture_calls==1&&atomic_load(&command_state->slots[at].rejected)==C_IDENTITY,"mapped drum replacement refuses stale target");
 put(project+0xa0,tracks[0].track);put(project+0x1e8,ptr(clip));put(ptr(ctrl)+0x68,ptr(clip)+0x388);
 static unsigned char sequencer[0x400],player[0x100];static uint32_t retained[3];
 put(ptr(ctrl)+0xaa1c,project);put(ptr(ctrl)+0xaa2c,ptr(sequencer));put(ptr(sequencer)+0x3f4,ptr(player));put(project+0x1b4,7);put(project+0x1b8,9);put(project+0x1bc,1);
 StepCopy clock_source;require(!step_native_read(&clock_source),"clock source member available");step_clock_read(&clock_source);atomic_store(&command_state->step_interest.until,100);
 struct {Context c;uint32_t stack[16];} clock_frame={0};clock_frame.c=context();retained[1]=ptr(ctrl);clock_frame.c.r[4]=ptr(retained);clock_frame.c.r[0]=480;clock_frame.c.r[1]=0;clock_frame.c.r[3]=ptr(player)+0x20;
 put(source_sp(&clock_frame.c)+0x34,7);put(source_sp(&clock_frame.c)+0x38,9);put(source_sp(&clock_frame.c)+0x3c,0xaabbcc01);call(&clock_frame.c,STEP_PLAYHEAD);
 step_clock_read(&clock_source);require(clock_source.clock_available&&clock_source.clock_value[0]==480&&!clock_source.clock_value[1],"actual scalar hook consumes native local tick and only engagement low byte");
 clock_frame.c.r[3]=ptr(player)+0x98;call(&clock_frame.c,STEP_PLAYHEAD);clock_source.clock_available=0;step_clock_read(&clock_source);require(!clock_source.clock_available,"native missing-player zero is unavailable, not step zero");
 clock_frame.c.r[3]=ptr(player)+0x20;call(&clock_frame.c,STEP_PLAYHEAD);clock_source.clock_available=0;clock_source.tick=1002;step_clock_read(&clock_source);require(!clock_source.clock_available,"stale local clock disappears");
 clock_source.tick=2;put(project+0x1b4,8);step_clock_read(&clock_source);clock_source.clock_available=0;step_clock_read(&clock_source);require(!clock_source.clock_available,"selection key invalidates old native clock");
 /* A separately bound Clip clock uses the existing native all-player
  * callback, including its native row/source identity, not global selection. */
 static unsigned char player_storage[0x108],row_cache[0x204],timeline[0x70],row_vector[0xc0];uint32_t bound_player=ptr(player_storage)+8;
 StepTarget clock_target=s.views[0].target;clock_target.clock_available=0;
 put(bound_player+0x80,tracks[0].track);put(bound_player+0xe0,ptr(row_vector));put(bound_player+0xe4,ptr(row_vector)+sizeof(row_vector));put(bound_player+0xfc,ptr(timeline));put(ptr(row_vector),ptr(row_cache)+0x80);put(ptr(row_cache),0x68ac724);put(ptr(row_cache)+0x44,ptr(clip)+0x320);put(ptr(row_cache)+0x1ec,ptr(clip)+0x320);put(ptr(row_cache)+0x1fc,tracks[0].track);put(ptr(timeline)+0x44,1);put(ptr(timeline)+0x60,960);
 step_bound_clock_read(0,tracks[0].track,&clock_target,2);Context advance=context();advance.r[0]=bound_player;advance.r[4]=bound_player-8;call(&advance,STEP_BOUND_PLAYHEAD);step_bound_clock_read(0,tracks[0].track,&clock_target,2);
 require(clock_target.clock_available&&clock_target.clock_value[0]==960,"bound primary row clock survives foreign global selection");
 atomic_store(&fixture->heartbeat,22);put(ptr(row_cache)+0x44,ptr(clip)+0x324);call(&advance,STEP_BOUND_PLAYHEAD);clock_target.clock_available=0;step_bound_clock_read(0,tracks[0].track,&clock_target,22);require(!clock_target.clock_available,"same row with another native Clip cache cannot paint old target playhead");
 step_lifecycle_checks(ptr(ctrl),ptr(clip));
 for(unsigned i=0;i<STEP_VIEWS;i++)step_bound_release(i);
 component_clip_track=NULL;component_clip_lookup=NULL;component_clip_state=NULL;
 component_step_scan=NULL;component_step_edit=NULL;free(fixture);free(command_state);fixture=NULL;command_state=NULL;mirror_state=NULL;step_pending=0;memset(&step_current,0,sizeof(step_current));
 puts("PASS absolute step owner: production queue/dispatch/receipt, independent full64 projections, offscreen/nonselected identity, source notification and stale-edit refusal; native tree/edit calls and timer input substituted");
}
