/* Production input policy, actual ALSA MIDI codecs and shared-file client.
 * Synthetic snapshots cover lifecycle branches; integration uses real producer
 * files with a component process in place of MPC and no physical ALSA device. */
#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <sys/wait.h>
static int captured_output(snd_seq_t*,snd_seq_event_t*);
static int fake_query_next_client(snd_seq_t*,snd_seq_client_info_t*);
static int fake_query_next_port(snd_seq_t*,snd_seq_port_info_t*);
static void fake_client_set(snd_seq_client_info_t*,int);
static int fake_client_id(const snd_seq_client_info_t*);
static int fake_client_type(const snd_seq_client_info_t*);
static const char *fake_client_name(const snd_seq_client_info_t*);
static void fake_port_client(snd_seq_port_info_t*,int);
static void fake_port_set(snd_seq_port_info_t*,int);
static unsigned fake_port_capability(const snd_seq_port_info_t*);
static const snd_seq_addr_t *fake_port_addr(const snd_seq_port_info_t*);
static int fake_port_number(const snd_seq_port_info_t*);
static const char *fake_port_name(const snd_seq_port_info_t*);
static int fake_event_input(snd_seq_t*,snd_seq_event_t**);
static int fake_event_pending(snd_seq_t*,int);
static int fake_get_client_info(snd_seq_t*,snd_seq_client_info_t*);
static int fake_event_lost(const snd_seq_client_info_t*);
static int fake_seq_close(snd_seq_t*);
static int fake_seq_open(snd_seq_t**,const char*,int,int);
static int fake_seq_name(snd_seq_t*,const char*);
static int fake_connect_from(snd_seq_t*,int,int,int);
static int fake_disconnect_from(snd_seq_t*,int,int,int);
static int fake_seq_id(snd_seq_t*);
static int fake_create_port(snd_seq_t*,const char*,unsigned,unsigned);
#define snd_seq_create_simple_port fake_create_port
#define snd_seq_connect_from fake_connect_from
#define snd_seq_disconnect_from fake_disconnect_from
#define snd_seq_client_id fake_seq_id
#define snd_seq_open fake_seq_open
#define snd_seq_set_client_name fake_seq_name
#define snd_seq_event_output_direct captured_output
#define snd_seq_query_next_client fake_query_next_client
#define snd_seq_query_next_port fake_query_next_port
#define snd_seq_client_info_set_client fake_client_set
#define snd_seq_client_info_get_client fake_client_id
#define snd_seq_client_info_get_type fake_client_type
#define snd_seq_client_info_get_name fake_client_name
#define snd_seq_port_info_set_client fake_port_client
#define snd_seq_port_info_set_port fake_port_set
#define snd_seq_port_info_get_capability fake_port_capability
#define snd_seq_port_info_get_addr fake_port_addr
#define snd_seq_port_info_get_port fake_port_number
#define snd_seq_port_info_get_name fake_port_name
#define snd_seq_event_input fake_event_input
#define snd_seq_event_input_pending fake_event_pending
#define snd_seq_get_client_info fake_get_client_info
#define snd_seq_client_info_get_event_lost fake_event_lost
#define snd_seq_close fake_seq_close
static void admission_write_checkpoint(void);
#define CONTROLLER_ADMISSION_CHECKPOINT() admission_write_checkpoint()
#define MIRROR_INPUT
#define main input_bridge_main_not_called
#include "mirror-motor.c"
#undef main
#include "../mcu-simulator/mcu-codec.c"
#undef snd_seq_event_output_direct
#undef snd_seq_query_next_client
#undef snd_seq_query_next_port
#undef snd_seq_client_info_set_client
#undef snd_seq_client_info_get_client
#undef snd_seq_client_info_get_type
#undef snd_seq_client_info_get_name
#undef snd_seq_port_info_set_client
#undef snd_seq_port_info_set_port
#undef snd_seq_port_info_get_capability
#undef snd_seq_port_info_get_addr
#undef snd_seq_port_info_get_port
#undef snd_seq_port_info_get_name
#undef snd_seq_event_input
#undef snd_seq_event_input_pending
#undef snd_seq_get_client_info
#undef snd_seq_client_info_get_event_lost
#undef snd_seq_close
#undef snd_seq_open
#undef snd_seq_set_client_name
#undef snd_seq_connect_from
#undef snd_seq_disconnect_from
#undef snd_seq_client_id
#undef snd_seq_create_simple_port
static unsigned relay_ports;
static int fake_create_port(snd_seq_t *seq,const char *name,unsigned caps,unsigned type){(void)seq;(void)type;if(strcmp(name,STOP_RELAY_PORT)||caps!=(SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_NO_EXPORT))return -1;relay_ports++;return 4;}
typedef struct {const char *name;unsigned capabilities;unsigned port;} FakePort;
typedef struct {const char *name;int type;unsigned client,count;FakePort ports[32];} FakeClient;
static FakeClient fake_clients[8];static unsigned fake_client_count,fake_port_queries[8];static int fake_ci=-1,fake_pi=-1,fake_pc=-1;static snd_seq_addr_t fake_address;
static void fake_enumeration(const FakeClient *clients,unsigned count){if(count>8)abort();memset(fake_clients,0,sizeof(fake_clients));memcpy(fake_clients,clients,count*sizeof(*clients));fake_client_count=count;memset(fake_port_queries,0,sizeof(fake_port_queries));fake_ci=fake_pi=fake_pc=-1;}
static int fake_query_next_client(snd_seq_t *seq,snd_seq_client_info_t *info){(void)seq;(void)info;return ++fake_ci<(int)fake_client_count?0:-ENOENT;}
static int fake_query_next_port(snd_seq_t *seq,snd_seq_port_info_t *info){(void)seq;(void)info;if(fake_pc<0||fake_pc>=(int)fake_client_count)return -ENOENT;fake_port_queries[fake_pc]++;return ++fake_pi<(int)fake_clients[fake_pc].count?0:-ENOENT;}
static void fake_client_set(snd_seq_client_info_t *info,int client){(void)info;if(client<0)fake_ci=-1;}
static int fake_client_id(const snd_seq_client_info_t *info){(void)info;return fake_clients[fake_ci].client;}
static int fake_client_type(const snd_seq_client_info_t *info){(void)info;return fake_clients[fake_ci].type;}
static const char *fake_client_name(const snd_seq_client_info_t *info){(void)info;return fake_clients[fake_ci].name;}
static void fake_port_client(snd_seq_port_info_t *info,int client){(void)info;fake_pc=-1;for(unsigned i=0;i<fake_client_count;i++)if(fake_clients[i].client==(unsigned)client){fake_pc=(int)i;break;}}
static void fake_port_set(snd_seq_port_info_t *info,int port){(void)info;if(port<0)fake_pi=-1;}
static unsigned fake_port_capability(const snd_seq_port_info_t *info){(void)info;return fake_clients[fake_pc].ports[fake_pi].capabilities;}
static const snd_seq_addr_t *fake_port_addr(const snd_seq_port_info_t *info){(void)info;fake_address=(snd_seq_addr_t){.client=fake_clients[fake_pc].client,.port=fake_clients[fake_pc].ports[fake_pi].port};return &fake_address;}
static int fake_port_number(const snd_seq_port_info_t *info){(void)info;return (int)fake_clients[fake_pc].ports[fake_pi].port;}
static const char *fake_port_name(const snd_seq_port_info_t *info){(void)info;return fake_clients[fake_pc].ports[fake_pi].name;}
static snd_seq_event_t fake_input_events[16];static unsigned fake_input_count,fake_input_at,fake_close_count;
static void fake_input_reset(void){fake_input_count=fake_input_at=0;}
static void fake_input_push(snd_seq_event_t event){if(fake_input_count>=16)abort();fake_input_events[fake_input_count++]=event;}
static int fake_event_input(snd_seq_t *seq,snd_seq_event_t **event){(void)seq;if(fake_input_at>=fake_input_count)return -EAGAIN;*event=fake_input_events+fake_input_at++;return 1;}
static int fake_event_pending(snd_seq_t *seq,int fetch){(void)seq;(void)fetch;return (int)(fake_input_count-fake_input_at);}
static int fake_get_client_info(snd_seq_t *seq,snd_seq_client_info_t *info){(void)seq;(void)info;return 0;}
static int fake_event_lost(const snd_seq_client_info_t *info){(void)info;return 0;}
static int fake_seq_close(snd_seq_t *seq){(void)seq;fake_close_count++;return 0;}
static unsigned fake_inventory_open;
static int fake_seq_open(snd_seq_t **seq,const char *name,int streams,int mode){(void)name;(void)streams;(void)mode;if(!fake_inventory_open)return -ENOENT;*seq=(snd_seq_t*)1;return 0;}
static int fake_seq_name(snd_seq_t *seq,const char *name){(void)seq;(void)name;return 0;}
static unsigned fake_subscriptions,fake_unsubscriptions;
static int fake_connect_from(snd_seq_t *seq,int sink,int client,int port){(void)seq;(void)sink;(void)client;(void)port;fake_subscriptions++;return 0;}
static int fake_disconnect_from(snd_seq_t *seq,int sink,int client,int port){(void)seq;(void)sink;(void)client;(void)port;fake_unsubscriptions++;return 0;}
static int fake_seq_id(snd_seq_t *seq){(void)seq;return 200;}
static snd_seq_event_t last_output;static unsigned output_count;static CopiedMirror *stop_on_output;
static unsigned observed_led[128],observed_pitch_count[9];static int observed_pitch[9];static unsigned char observed_lcd[112];
static unsigned observed_hui_zone,observed_hui_zone_valid,observed_hui_led_count,observed_hui_heartbeat;
static unsigned char observed_hui_led[16][16];
static unsigned observed_hui_fader_msb_count[8],observed_hui_fader_lsb_count[8];static unsigned char observed_hui_fader_msb[8],observed_hui_fader_lsb[8];
static unsigned observed_color_count,observed_lcd_count,observed_ring_count[8];static int observed_ring[8];
static unsigned observed_destinations[256],observed_destination_pitch_count[256][8];
static int observed_destination_pitch[256][8];
static unsigned native_capture_count,native_capture_enabled,native_output_fail_after;
static struct {snd_seq_addr_t destination;unsigned length;unsigned char bytes[64];} native_capture[512];
/* Outbound census: channel-pressure meter levels per strip and the ten
 * playhead controllers, so a suppressed resend is observable as a count. */
static unsigned observed_press_count[8],observed_position_count;static int observed_press_level[8];
static McuFeedbackState hui_consumer;
static int captured_output(snd_seq_t *seq,snd_seq_event_t *event){(void)seq;last_output=*event;output_count++;
 if(native_output_fail_after&&!--native_output_fail_after)return -EIO;
 if(event->type==SND_SEQ_EVENT_CONTROLLER){unsigned char wire[]={0xb0|event->data.control.channel,(unsigned char)event->data.control.param,(unsigned char)event->data.control.value};(void)hui_feedback_decode(&hui_consumer,wire,sizeof(wire));}
 if(native_capture_enabled&&native_capture_count<512){unsigned at=native_capture_count++;native_capture[at].destination=event->dest;
  if(event->type==SND_SEQ_EVENT_SYSEX){unsigned length=event->data.ext.len;if(length>64)abort();native_capture[at].length=length;memcpy(native_capture[at].bytes,event->data.ext.ptr,length);}
  else{native_capture[at].length=3;unsigned char *b=native_capture[at].bytes;
   if(event->type==SND_SEQ_EVENT_CONTROLLER){b[0]=0xb0|event->data.control.channel;b[1]=(unsigned char)event->data.control.param;b[2]=(unsigned char)event->data.control.value;}
   else if(event->type==SND_SEQ_EVENT_NOTEON){b[0]=0x90|event->data.note.channel;b[1]=event->data.note.note;b[2]=event->data.note.velocity;}
   else native_capture[at].length=0;
  }
 }
 observed_destinations[event->dest.client]++;
 if(event->type==SND_SEQ_EVENT_PITCHBEND&&event->data.control.channel<8){observed_destination_pitch_count[event->dest.client][event->data.control.channel]++;observed_destination_pitch[event->dest.client][event->data.control.channel]=event->data.control.value+8192;}
 if(event->type==SND_SEQ_EVENT_NOTEON&&event->data.note.note==0&&event->data.note.velocity==0)observed_hui_heartbeat++;
 if(event->type==SND_SEQ_EVENT_CONTROLLER&&event->data.control.param==0x0c){observed_hui_zone=(unsigned)event->data.control.value;observed_hui_zone_valid=1;}
 if(event->type==SND_SEQ_EVENT_CONTROLLER&&event->data.control.param==0x2c&&observed_hui_zone_valid){unsigned raw=(unsigned)event->data.control.value;observed_hui_led[observed_hui_zone&15][raw&15]=(raw&0x40)!=0;observed_hui_led_count++;observed_hui_zone_valid=0;}
 if(event->type==SND_SEQ_EVENT_CONTROLLER&&event->data.control.param<8){unsigned strip=(unsigned)event->data.control.param;observed_hui_fader_msb_count[strip]++;observed_hui_fader_msb[strip]=(unsigned char)event->data.control.value;}
 if(event->type==SND_SEQ_EVENT_CONTROLLER&&event->data.control.param>=0x20&&event->data.control.param<0x28){unsigned strip=(unsigned)event->data.control.param-0x20;observed_hui_fader_lsb_count[strip]++;observed_hui_fader_lsb[strip]=(unsigned char)event->data.control.value;}
 if(event->type==SND_SEQ_EVENT_CHANPRESS){unsigned v=(unsigned)event->data.control.value;observed_press_count[(v>>4)&7]++;observed_press_level[(v>>4)&7]=(int)(v&0xf);}
 if(event->type==SND_SEQ_EVENT_CONTROLLER&&event->data.control.param>=0x40&&event->data.control.param<=0x49)observed_position_count++;
 if(event->type==SND_SEQ_EVENT_CONTROLLER&&event->data.control.param>=0x30&&event->data.control.param<0x38){unsigned strip=(unsigned)event->data.control.param-0x30;observed_ring_count[strip]++;observed_ring[strip]=event->data.control.value;}
 if(stop_on_output&&event->type==SND_SEQ_EVENT_NOTEON&&event->data.note.note==93)stop_on_output->playing.bits=0;
 if(event->type==SND_SEQ_EVENT_PITCHBEND&&event->data.control.channel<9){observed_pitch_count[event->data.control.channel]++;observed_pitch[event->data.control.channel]=event->data.control.value+8192;}
 if(event->type==SND_SEQ_EVENT_NOTEON&&event->data.note.note<128)observed_led[event->data.note.note]=event->data.note.velocity;
 if(event->type==SND_SEQ_EVENT_SYSEX&&event->data.ext.len==15){const unsigned char *p=event->data.ext.ptr;if(p[5]==0x12&&p[6]<=105){memcpy(observed_lcd+p[6],p+7,7);observed_lcd_count++;}if(p[5]==0x72)observed_color_count++;}
 return 0;}
static void need(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static void snapshot_fixture(CopiedMirror *s,unsigned n){
 memset(s,0,sizeof(*s));s->count=n;s->ready=s->alive=1;s->epoch=1;s->project_owner=1;s->selection=(CopiedField){.property=0x70078,.incarnation=1,.owner_incarnation=1,.revision=2,.seed=1,.available=1};s->heartbeat=100;s->revision=2;s->playing=(CopiedField){.available=1,.owner_incarnation=900,.incarnation=901,.revision=2};
 for(unsigned i=0;i<n;i++)s->tracks[i]=(CopiedTrack){.serial=i+1,.track=0x10000+i*0x1000,.program=0x90000+i*0x1000,.incarnation=i+1,.binding=i+1,.vptr=0x6930c00,.revision=2,.bits=0x3f000000,.seed=1,.available=1,.track_owner=2+i*2,.program_owner=3+i*2};
 for(unsigned i=0;i<n;i++)for(unsigned f=CF_PAN;f<CF_COUNT;f++)s->tracks[i].fields[f]=(CopiedField){.property=(channel_kind(f)==CO_TRACK?s->tracks[i].track:s->tracks[i].program)+channel_offset(f),.incarnation=10+i*CF_COUNT+f,.owner_incarnation=channel_kind(f)==CO_TRACK?s->tracks[i].track_owner:s->tracks[i].program_owner,.revision=2,.bits=f==CF_PAN?0x3f000000:0,.available=1,.seed=1};
}
static void reset_fixture(CommandState *commands,MirrorInput *in,MirrorBank *bank,CopiedMirror *s,unsigned n){
 following.enabled=1;motor_follow_reset(&following);memset(commands,0,sizeof(*commands));memset(&command_coordinator,0,sizeof(command_coordinator));commands->seconds=900;commands->owner_token=123;atomic_store(&commands->alive,1);need(input_begin(in,commands,coordinator_flights),"fresh synthetic input state");bank_init(bank);snapshot_fixture(s,n);bank_apply(bank,s,100);
}
static uint32_t shared_fixture_time(MirrorState *state,uint32_t requested){
 struct timespec clock;need(!clock_gettime(CLOCK_MONOTONIC,&clock),"fixture monotonic clock");uint64_t current=(uint64_t)clock.tv_sec*1000+(uint64_t)clock.tv_nsec/1000000;need(current>=requested,"fixture source origin range");uint64_t origin=current-requested;state->origin_sec=(uint32_t)(origin/1000);state->origin_nsec=(uint32_t)(origin%1000)*1000000;uint32_t now=source_now(state);need(now!=UINT32_MAX,"fixture source clock");atomic_store(&state->heartbeat,now);return now;
}
static void runtime_cycle_checks(void){
 CommandState *commands=calloc(1,sizeof(*commands));MirrorState *state=calloc(1,sizeof(*state));need(commands&&state,"runtime cycle fixtures");
 MirrorInput in;MirrorBank bank;MIRROR_COPY(snapshot);reset_fixture(commands,&in,&bank,&snapshot,0);
 state->magic=MIRROR_MAGIC;state->version=MIRROR_VERSION;state->bytes=sizeof(*state);state->capacity=MIRROR_CELLS;
 atomic_store(&state->alive,1);atomic_store(&state->revision,2);atomic_store(&state->ready,0);shared_fixture_time(state,1000);
 ControllerRuntime runtime={.snapshot=&snapshot};runtime_cycle_begin(&runtime);
 int first=runtime_snapshot(&runtime,state,0),second=runtime_snapshot(&runtime,state,0);
 need(first==second&&first>=0&&runtime.snapshot_requests==2&&runtime.snapshot_copies==1,"one runtime cycle performs one full snapshot copy even when two consumers request it");
 runtime_settle(&runtime,&in,&bank);runtime_settle(&runtime,&in,&bank);runtime_submit(&runtime,&in,&bank);
 need(!in.error&&runtime.settlement_requests==3&&runtime.settlement_scans==1&&runtime.submission_visits==1,"one runtime cycle scans shared command settlement once before its submission visit");
 runtime_cycle_begin(&runtime);shared_fixture_time(state,1010);need(runtime_snapshot(&runtime,state,0)>=0,"next runtime cycle takes a fresh snapshot");
 need(runtime_snapshot(&runtime,state,1)==0&&runtime.deferred_pad_views==1,"a pad view entered mid-cycle defers instead of taking or misusing a second full snapshot");runtime_submit(&runtime,&in,&bank);
 need(runtime.snapshot_copies==2&&runtime.settlement_scans==2&&runtime.submission_visits==2,"the next runtime cycle gets exactly one new snapshot and settlement scan");
 free(state);free(commands);
 puts("PASS shared runtime cycle instrumentation: duplicate snapshot and settlement requests collapse to one copy/scan per cycle; submission stays separate (synthetic source/mailbox)");
}
static void hui_unready_liveness_checks(void){
 char *hui[]={(char*)"mirror-input",(char*)"/volume",(char*)"/command",(char*)"--profile=hui",(char*)"--endpoint-client=HUI Device",(char*)"--endpoint-port=DAW",(char*)"--endpoint-type=kernel",(char*)"--hui-mode=pickup"};need(servo_arguments(8,hui)==3,"HUI unready-liveness profile");
 CommandState *commands=calloc(1,sizeof(*commands));MirrorState *state=calloc(1,sizeof(*state));need(commands&&state,"HUI unready-liveness fixtures");MirrorBank bank;MIRROR_COPY(snapshot);reset_fixture(commands,&input,&bank,&snapshot,0);
 state->magic=MIRROR_MAGIC;state->version=MIRROR_VERSION;state->bytes=sizeof(*state);state->capacity=MIRROR_CELLS;atomic_store(&state->alive,1);atomic_store(&state->revision,2);atomic_store(&state->ready,0);atomic_store(&state->epoch,0);atomic_store(&state->count,0);
 Surface surface={.seq=(snd_seq_t*)1,.full={32,0},.source=3,.hui_connected_tick=0};fake_close_count=0;
 const uint32_t replies[]={1000,3200,5400};
 for(unsigned i=0;i<3;i++){
  uint32_t now=shared_fixture_time(state,replies[i]);snd_seq_event_t event;snd_seq_ev_clear(&event);snd_seq_ev_set_noteon(&event,0,0,0x7f);event.source=surface.full;fake_input_reset();fake_input_push(event);
  need(surface_drain(&surface,&bank,state,0,NULL),"exact HUI reply drains while source is unready");now=source_now(state);need(surface.hui_reply_count==i+1&&hui_liveness_current(&surface,now),"unready exact-source HUI replies keep bounded liveness current");need(!input.submitted&&!atomic_load(&commands->published),"unready HUI drain publishes no musical control intent");
 }
 shared_fixture_time(state,5600);snd_seq_event_t msb,lsb;snd_seq_ev_clear(&msb);snd_seq_ev_set_controller(&msb,0,0,64);msb.source=surface.full;snd_seq_ev_clear(&lsb);snd_seq_ev_set_controller(&lsb,0,0x20,1);lsb.source=surface.full;fake_input_reset();fake_input_push(msb);fake_input_push(lsb);need(surface_drain(&surface,&bank,state,0,NULL),"unready HUI fader intent follows the ordinary drain");need(!input.submitted&&!atomic_load(&commands->published)&&surface.hui_pickup[0].physical_valid,"unready non-liveness intent updates only its physical baseline and publishes no command");
 uint32_t expired=shared_fixture_time(state,8000);fake_input_reset();need(surface_drain(&surface,&bank,state,0,NULL)&&!hui_liveness_current(&surface,expired),"stopped HUI replies expire after the reply timeout");surface_close(&surface,&bank);need(fake_close_count==1&&!surface.seq&&!surface.hui_reply_count&&!bank.ready&&!bank.chooser&&input_drained(&input),"one timeout close clears protocol state, disconnects the bank and retains a drained command owner");
 uint32_t parked=shared_fixture_time(state,8100);need(fresh_copy_view(state,&snapshot,&parked,0)==1,"disconnected path reacquires a fresh chooser snapshot");bank_apply(&bank,&snapshot,parked);input_drain(&input,&bank,&snapshot,parked);need(bank.chooser&&!bank.ready&&input_drained(&input),"disconnected path drains commands before parking the fresh chooser snapshot");
 atomic_store(&state->epoch,1);atomic_store(&state->ready,1);atomic_store(&state->revision,4);uint32_t reacquired=shared_fixture_time(state,8200);bank_disconnect(&bank);need(fresh_copy_view(state,&snapshot,&reacquired,0)==1,"reconnect path reacquires a fresh ready source");bank_apply(&bank,&snapshot,reacquired);need(bank.ready&&!bank.chooser,"fresh ready source applies only after reconnect reset");
 free(state);free(commands);char *defaults[]={(char*)"mirror-input",(char*)"/volume",(char*)"/command"};need(servo_arguments(3,defaults)==3,"restore default profile after HUI unready-liveness check");
 puts("PASS production surface drain: exact HUI heartbeat replies retain liveness through unready chooser time, no control bypasses readiness, reply loss closes once, then command drain/chooser park/fresh ready-source reacquire remain ordered (ALSA queue and source snapshots substituted)");
}
static void gesture(MirrorInput *in,MirrorBank *bank,CopiedMirror *s,unsigned strip,int position,uint32_t now){
 input_touch(in,bank,s,strip,1,now);input_pitch(in,bank,strip,8192,now);input_pitch(in,bank,strip,position,now);input_touch(in,bank,s,strip,0,now+1);
}
static void evidence(CommandState *s,unsigned request,unsigned kind,unsigned tick,unsigned revision){
 unsigned at=command_find(s,request);need(at<COMMAND_SLOTS,"fixture slot identity");CommandLane *l=s->slots[at].lanes+1;CommandRequest r;need(command_request_read(s->slots+at,request,&r),"sequence-qualified fixture request");
 if(atomic_load(&l->sequence)!=request){atomic_store(&l->published,0);atomic_store(&l->sequence,request);}
 unsigned n=atomic_load(&l->published);uint32_t program=0x90000+(r.serial-1)*0x1000;
 unsigned synchronous=r.reserved==CF_ARM||r.reserved==CF_SELECTION;
 CommandEvent e={.kind=kind,.request=request,.tick=tick,.token=123,.capture=synchronous?0:0x8000,.counter=synchronous?0:program+0x40,.program=program,.arg_kind=synchronous?0:0x101,.controller=command_controller(r.reserved),.bits=kind==CE_COMMIT?r.bits:command_native_bits(r.reserved,r.bits),.incarnation=r.reserved?r.field_incarnation:r.incarnation,.property=kind==CE_COMMIT?(r.reserved==CF_SELECTION?0x70078:(r.reserved==CF_ARM?0x10000+(r.serial-1)*0x1000:program)+channel_offset(command_source_field(r.reserved))):0,.revision=revision,.reserved=r.reserved};
 command_event_store(l->events+n,&e);atomic_store(&l->token,123);atomic_store_explicit(&l->published,n+1,memory_order_release);
}
static void complete(CommandState *c,unsigned request,unsigned tick){
 unsigned at=command_find(c,request);need(at<COMMAND_SLOTS,"completion slot identity");CommandSlot *slot=c->slots+at;CommandRequest r;need(command_request_read(slot,request,&r),"completed fixture request");
 if(r.reserved!=CF_ARM&&r.reserved!=CF_SELECTION)evidence(c,request,CE_QUEUE,tick,0);
 evidence(c,request,r.reserved==CF_ARM||r.reserved==CF_SELECTION?CE_OWNER_BEGIN:CE_BODY,tick,0);evidence(c,request,CE_COMMIT,tick,r.before_revision+2);evidence(c,request,r.reserved==CF_ARM||r.reserved==CF_SELECTION?CE_OWNER_END:CE_DONE,tick,0);
 atomic_store(&c->consumed,atomic_load(&c->consumed)+1);atomic_store(&slot->returned,request);atomic_store(&slot->processed,request);atomic_store_explicit(&slot->done,request,memory_order_release);atomic_store_explicit(&slot->sealed,request,memory_order_release);
}
static void reclaim_fixture(CommandState *c,unsigned seq){
 unsigned at=command_find(c,seq);need(at<COMMAND_SLOTS&&atomic_load(&c->slots[at].settled)==seq,"reclaim only settled fixture");atomic_store(&c->reclaimed,atomic_load(&c->reclaimed)+1);atomic_store_explicit(&c->slots[at].reclaimed,seq,memory_order_release);
}
static void collection_fixture_source(MirrorState *state){
 state->magic=MIRROR_MAGIC;state->version=MIRROR_VERSION;state->bytes=sizeof(*state);state->capacity=MIRROR_CELLS;
 atomic_store(&state->alive,1);atomic_store(&state->ready,1);atomic_store(&state->epoch,1);atomic_store(&state->revision,2);atomic_store(&state->count,16);atomic_store(&state->channel.project_owner,1);
 /* Native startup has a real selected Track even when B pages away from it.
  * An unavailable selection masks a disconnect reset of selection history. */
 ChannelOwner *project=state->channel.owners;atomic_store(&project->live,1);atomic_store(&project->kind,CO_PROJECT);atomic_store(&project->address,0x70000);atomic_store(&project->incarnation,1);
 ChannelCell *selection=state->channel.fields;atomic_store(&selection->property,0x70078);atomic_store(&selection->owner_incarnation,1);atomic_store(&selection->incarnation,1);atomic_store(&selection->field,CF_SELECTION);atomic_store(&selection->revision,2);atomic_store(&selection->live,1);atomic_store(&selection->bits,0x10000);atomic_store(&selection->seed,1);
 atomic_store(state->channel.field_directory+(((0x70078u>>4)*2654435761u)&(CHANNEL_HASH-1)),1);
 for(unsigned i=0;i<16;i++){
  MirrorBinding *t=state->tracks+i;unsigned program=0x90000+i*0x1000,track=0x10000+i*0x1000;
  atomic_store(&t->serial,i+1);atomic_store(&t->track,track);atomic_store(&t->program,program);atomic_store(&t->incarnation,i+1);atomic_store(&t->binding,i+1);atomic_store(&t->vptr,0x6930c00);atomic_store(&t->track_owner,2+i*2);atomic_store(&t->program_owner,3+i*2);
  MirrorCell *cell=state->cells+i;atomic_store(&cell->revision,2);atomic_store(&cell->live,1);atomic_store(&cell->incarnation,i+1);atomic_store(&cell->program,program);atomic_store(&cell->property,program+0x5b0);atomic_store(&cell->seed,1);atomic_store(&cell->bits,i<8?0x3f000000:0x3e800000);
  ChannelOwner *owner=state->channel.owners+2+i*2;atomic_store(&owner->live,1);atomic_store(&owner->incarnation,3+i*2);atomic_store(&owner->kind,CO_PROGRAM);atomic_store(&owner->address,program);
 }
}
static void collection_production_checks(void){
 CommandState *commands=calloc(1,sizeof(*commands));MirrorState *state=calloc(1,sizeof(*state));need(commands&&state,"collection command/source fixture");
 MirrorBank unused;MIRROR_COPY(snapshot);reset_fixture(commands,&input,&unused,&snapshot,16);collection_fixture_source(state);shared_fixture_time(state,1000);
 McuControllerCollection saved={.count=3};
 saved.records[0]=(McuControllerPreference){.enabled=1,.input={&mcu_profile_generic,"Surface A","From A",MCU_ENDPOINT_USER},.output={&mcu_profile_generic,"Feedback A","To A",MCU_ENDPOINT_USER}};
 saved.records[1]=(McuControllerPreference){.enabled=1,.input={&mcu_profile_generic,"Surface B","From B",MCU_ENDPOINT_USER},.output={&mcu_profile_generic,"Feedback B","To B",MCU_ENDPOINT_USER}};
 saved.records[2]=saved.records[0];saved.records[2].enabled=0;
 char directory[]="/tmp/mpclearn-two-surface-XXXXXX";need(mkdtemp(directory)!=NULL,"collection preference directory");char path[256],option[300];snprintf(path,sizeof(path),"%s/controller-preferences",directory);snprintf(option,sizeof(option),"--controller-preferences=%s",path);
 need(mcu_controller_preferences_write_collection(path,&saved),"persist exact three-record collection");char *args[]={(char*)"mirror-input",(char*)"/volume",(char*)"/command",option};need(servo_arguments(4,args)==3&&runtime_collection.count==3,"production arguments retain all persisted records, not just first enabled");
 collection_init(&surfaces,&runtime_collection);surfaces.seq=(snd_seq_t*)1;surfaces.sink=1;surfaces.source=2;fake_subscriptions=fake_unsubscriptions=fake_close_count=0;
 const char *prior_adapter=stop_adapter_exe;stop_adapter_exe="/absent-adapter";fake_enumeration(NULL,0);relay_ports=0;
 need(collection_stop_ingress(&surfaces)&&surfaces.stop_sink==4&&relay_ports==1,"production private Stop ingress starts before adapter/MPC route exists");
 Surface relay={.seq=surfaces.seq};need(!surface_stop_route(&relay,state),"actual Stop delivery still rejects the absent exact adapter/MPC subscription");stop_adapter_exe=prior_adapter;
 const unsigned read=SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,write=SND_SEQ_PORT_CAP_WRITE;
 FakeClient clients[]={{"Surface A",SND_SEQ_USER_CLIENT,129,1,{{"From A",read,0}}},{"Feedback A",SND_SEQ_USER_CLIENT,130,1,{{"To A",write,1}}},{"Surface B",SND_SEQ_USER_CLIENT,131,1,{{"From B",read,2}}},{"Feedback B",SND_SEQ_USER_CLIENT,132,1,{{"To B",write,3}}}};
 fake_enumeration(clients,4);need(collection_topology(&surfaces,state)&&surfaces.connected==2&&fake_subscriptions==2,"production collection attaches both enabled split-port surfaces and ignores disabled duplicate");
 need(surfaces.topology_scans==1&&surfaces.topology_rows==4,"one topology inventory for the entire collection");
 need(surfaces.contexts[0].input.flights==surfaces.contexts[1].input.flights&&surfaces.contexts[0].input.flights==input.flights,"all origins use sole coordinator flight storage");
 surfaces.focus=0;surfaces.banks[1].assignment=BA_EFFECT;need(collection_interest_view(&surfaces,BA_EFFECT)==surfaces.banks+1,"mixer interaction on A retains B's only effects demand");surfaces.banks[0].assignment=BA_EFFECT;need(collection_interest_view(&surfaces,BA_EFFECT)==surfaces.banks,"interaction chooses one competing expensive view");surfaces.banks[0].assignment=surfaces.banks[1].assignment=BA_TRACK;
 memset(observed_destinations,0,sizeof(observed_destinations));memset(observed_destination_pitch_count,0,sizeof(observed_destination_pitch_count));
 ControllerRuntime runtime={.snapshot=&snapshot};runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,collection_pads(&surfaces))==1,"shared fresh source before two-surface input");
 snd_seq_event_t event;snd_seq_ev_clear(&event);snd_seq_ev_set_noteon(&event,0,47,127);event.source=surfaces.surfaces[1].full;fake_input_reset();fake_input_push(event);
 snd_seq_ev_clear(&event);snd_seq_ev_set_pitchbend(&event,0,-4096);event.source=surfaces.surfaces[0].full;fake_input_push(event);snd_seq_ev_set_pitchbend(&event,0,4096);event.source=surfaces.surfaces[1].full;fake_input_push(event);
 need(collection_drain(&surfaces,state,&runtime)&&collection_cycle(&surfaces,state,&runtime),"production source routing and shared cycle submit both banks");
 need(surfaces.banks[0].offset==0&&surfaces.banks[1].offset==8,"surface B banks without moving A");
 need(surfaces.submit_cursor==1,"shared mailbox first admission rotates across configured origins");
 need(atomic_load(&commands->published)==2&&input_pending(&input)==2,"two surfaces publish independent simultaneous commands through one coordinator");
 unsigned targets=0;for(unsigned i=0;i<COMMAND_SLOTS;i++)if(coordinator_flights[i].flight){InputFlight *flight=coordinator_flights+i;targets|=1u<<(flight->target.serial-1);need(flight->origin==&surfaces.contexts[flight->target.serial==1?0:1].input,"flight retains exact originating input");need(flight->origin_bank==surfaces.banks+(flight->target.serial==1?0:1),"flight retains exact originating bank");}
 need(targets==0x101&&runtime.snapshot_copies==1&&runtime.settlement_scans==1&&surfaces.interest_visits==1,"independent tracks 1/9 share one actual snapshot, one settlement scan and one interest visit");
 need(observed_destinations[130]&&observed_destinations[132]&&!observed_destinations[129]&&!observed_destinations[131],"feedback uses each configured To role, never the From role");
 for(unsigned request=1;request<=2;request++){unsigned at=command_find(commands,request);InputFlight *flight=coordinator_flights+at;complete(commands,request,1001);atomic_store(&state->cells[flight->target.serial-1].bits,flight->bits);atomic_store(&state->cells[flight->target.serial-1].revision,4);}
 shared_fixture_time(state,1020);runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime),"one settlement scan handles both origin receipts");
 need(surfaces.contexts[0].input.settled==1&&surfaces.contexts[1].input.settled==1&&surfaces.banks[0].faders[0].wait&&surfaces.banks[1].faders[0].wait,"settlement applies barriers to both correct originating banks");
 reclaim_fixture(commands,1);reclaim_fixture(commands,2);shared_fixture_time(state,1100);runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime),"shared coordinator reclaims both source-settled flights");
 need(input_drained(&input)&&observed_destination_pitch[130][0]==4096&&observed_destination_pitch[132][0]==12288,"independent final fader values return only to their surfaces");
 MirrorInput *a=&surfaces.contexts[0].input,*b=&surfaces.contexts[1].input;
 surfaces.banks[1].offset=0;bank_apply(surfaces.banks+1,&snapshot,runtime.now);
 input_control(a,surfaces.banks,&snapshot,0,CF_VOLUME,1,0,runtime.now);
 float first;memcpy(&first,&a->desires[0][CF_VOLUME].bits,4);
 input_control(b,surfaces.banks+1,&snapshot,0,CF_VOLUME,1,0,runtime.now);
 float combined;memcpy(&combined,&b->desires[0][CF_VOLUME].bits,4);
 need(!a->desires[0][CF_VOLUME].pending&&b->desires[0][CF_VOLUME].pending&&combined>first,"cross-origin relative control accumulates and coalesces by target/field");
 snapshot.selection=(CopiedField){.available=1,.incarnation=1,.owner_incarnation=1,.revision=2,.property=0x70078};
 input_control(b,surfaces.banks+1,&snapshot,1,CF_SELECTION,0,1,runtime.now);
 input_control(a,surfaces.banks,&snapshot,0,CF_SELECTION,0,1,runtime.now);
 need(!b->desires[1][CF_SELECTION].pending&&a->desires[0][CF_SELECTION].pending,"latest Select wins globally even when origin visit order is reversed");
 snapshot.editor_owner=700;input_global_add(b,&snapshot,GLOBAL_UNDO,0,runtime.now);input_global_add(a,&snapshot,GLOBAL_REDO,0,runtime.now);
 need(!input_order_admit(a,a->global_events[a->global_head].order)&&input_order_admit(b,b->global_events[b->global_head].order),"bounded edge order is global, not surface-array order");
 input_stop_button(b,&snapshot,94,1,runtime.now);snapshot.playing.available=1;snapshot.playing.bits=0;input_stop_button(a,&snapshot,93,1,runtime.now);
 need(!a->stop_home&&a->global_events[(a->global_head+a->global_count-1)%INPUT_JOG_EVENTS].operation==GLOBAL_STOP,"cross-origin Stop after unsampled Play remains Stop, never homes");
 input_discard(a);input_jog_discard(a);input_discard(b);input_jog_discard(b);surfaces.banks[1].offset=8;
 input_wheel_add(b,&snapshot,1);input_global_add(a,&snapshot,GLOBAL_KEY_TAB,0,runtime.now);
 need(input_order_admit(b,b->wheel_order)&&!input_order_admit(a,a->global_events[a->global_head].order),"older peer data-wheel steps precede focus-changing edges");
 input_jog_discard(a);input_jog_discard(b);input_press_add(b,&snapshot);input_wheel_add(a,&snapshot,1);
 need(input_order_admit(b,b->press_order)&&!input_order_admit(a,a->wheel_order),"older peer focus press precedes later data-wheel steps");
 input_jog_discard(a);input_jog_discard(b);
 command_coordinator.jog_wait=1;command_coordinator.jog_wait_owner=900;command_coordinator.jog_wait_epoch=snapshot.epoch;command_coordinator.jog_wait_count=7;snapshot.playing.bits=1;
 need(!input_jog_admit(a,&snapshot)&&!input_jog_admit(b,&snapshot),"both origins share the unresolved native transport restart barrier");
 snapshot.playing.bits=0;need(input_jog_admit(b,&snapshot)&&!command_coordinator.jog_wait,"actual stopped source clears the shared transport barrier");
 unsigned b_sent=observed_destinations[132],a_connections=fake_subscriptions;
 fake_enumeration(clients,2);need(collection_topology(&surfaces,state)&&surfaces.connected==1&&surfaces.surfaces[0].seq&&!surfaces.surfaces[1].seq,"absent B disconnects only B");
 shared_fixture_time(state,1120);runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime),"A remains live while B is absent");need(observed_destinations[132]==b_sent&&fake_subscriptions==a_connections&&!fake_close_count,"absent B emits nothing and does not reopen shared ALSA or reconnect A");
 const unsigned absent_positions[]={6144,4096};
 for(unsigned step=0;step<2;step++){
  unsigned tick=1140+step*60,request=3+step;shared_fixture_time(state,tick);runtime_cycle_begin(&runtime);
  snd_seq_ev_clear(&event);snd_seq_ev_set_pitchbend(&event,0,(int)absent_positions[step]-8192);event.source=surfaces.surfaces[0].full;fake_input_reset();fake_input_push(event);
  need(collection_drain(&surfaces,state,&runtime)&&collection_cycle(&surfaces,state,&runtime),"A input continues through shared router while B is absent");
  unsigned at=command_find(commands,request);need(at<COMMAND_SLOTS&&coordinator_flights[at].target.serial==1&&coordinator_flights[at].origin==a,"absent-peer trial admits only A's original target");
  uint32_t bits=coordinator_flights[at].bits;complete(commands,request,tick+1);atomic_store(&state->cells[0].bits,bits);atomic_store(&state->cells[0].revision,6+step*2);
  shared_fixture_time(state,tick+20);runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime),"A command source-settles while B absent");reclaim_fixture(commands,request);
  shared_fixture_time(state,tick+40);runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime),"A fresh feedback resumes after absent-peer command settlement");
  need(input_drained(&input)&&observed_destination_pitch[130][0]==(int)absent_positions[step]&&observed_destinations[132]==b_sent,"A's two later values return only to A while disconnected B receives nothing");
 }
 clients[2].client=141;clients[3].client=142;fake_enumeration(clients,4);need(collection_topology(&surfaces,state)&&surfaces.connected==2&&surfaces.surfaces[1].full.client==141&&surfaces.surfaces[1].output.client==142,"B reconnect resolves changed independent addresses");
 need(surfaces.banks[0].offset==0&&surfaces.banks[1].offset==8&&!surfaces.contexts[1].input.gestures[0].held,"B reconnect retains independent bank but clears old gesture state");
 shared_fixture_time(state,1300);runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime),"reconnected B receives only fresh shared feedback");need(observed_destinations[142]&&observed_destinations[132]==b_sent,"reconnected feedback targets new output only");
 need(snapshot.selection.available&&snapshot.selected_serial==1&&surfaces.banks[0].offset==0&&surfaces.banks[1].offset==8&&surfaces.banks[1].faders[0].identity.serial==9,"post-reconnect fresh selected Track1 must not undo B's retained bank8");
 need(!observed_destination_pitch_count[142][0],"B reacquires a fresh post-remap heartbeat before fader output");
 shared_fixture_time(state,1320);runtime_cycle_begin(&runtime);need(runtime_snapshot(&runtime,state,0)==1&&collection_cycle(&surfaces,state,&runtime),"reconnected B reaches fresh motor feedback without another bank command");
 need(observed_destination_pitch[142][0]==12288&&observed_destination_pitch[130][0]==4096&&surfaces.banks[1].offset==8,"reconnected B receives retained Track9 value while A stays Track1");
 MirrorBank retained=surfaces.banks[1];bank_disconnect(&retained);need(!retained.ready&&retained.faders[0].touch==MT_UNKNOWN&&retained.faders[0].last_sent==-1&&!retained.faders[0].identity.serial,"retained selection history does not retain physical state or trusted output target");
 snapshot.selection.bits=snapshot.tracks[1].track;bank_apply(&retained,&snapshot,1320);need(retained.offset==0&&retained.selected.serial==2,"actual changed selection while disconnected still follows the newly selected Track");
 retained.offset=8;bank_disconnect(&retained);snapshot.epoch++;bank_apply(&retained,&snapshot,1320);need(retained.offset==0,"new project epoch still resets a disconnected bank");snapshot.epoch--;snapshot.selection.bits=snapshot.tracks[0].track;
 collection_disconnect(&surfaces,0);collection_disconnect(&surfaces,1);need(!surfaces.connected&&fake_unsubscriptions==3&&!fake_close_count&&input_drained(&input),"exact per-origin teardown retains one drained shared client");
 uint64_t copies=runtime.snapshot_copies,scans=runtime.settlement_scans,interests=surfaces.interest_visits,topology=surfaces.topology_scans;
 for(unsigned cycle=0;cycle<100;cycle++){runtime_cycle_begin(&runtime);need(!collection_needs_snapshot(&surfaces),"all-absent drained collection parks full source work");}
 need(runtime.snapshot_copies==copies&&runtime.settlement_scans==scans&&surfaces.interest_visits==interests&&surfaces.topology_scans==topology,"disabled/disconnected slots add no snapshot, settlement, interest or topology work per parked cycle");
 coordinator_flights[0].flight=999;need(collection_needs_snapshot(&surfaces),"absent collection keeps copying until sole coordinator drains");coordinator_flights[0].flight=0;
 McuControllerCollection capacity={.count=MCU_CONTROLLER_CAPACITY};for(unsigned i=0;i<capacity.count;i++){capacity.records[i]=saved.records[0];capacity.records[i].enabled=0;}
 collection_init(&surfaces,&capacity);need(surfaces.count==16&&command_coordinator.count==1&&!collection_needs_snapshot(&surfaces),"sixteen disabled records remain bounded and parked");
 need(!unlink(path)&&!rmdir(directory),"remove exact collection fixture preference");free(state);free(commands);memset(&surfaces,0,sizeof(surfaces));memset(&input,0,sizeof(input));memset(&command_coordinator,0,sizeof(command_coordinator));
 char *defaults[]={(char*)"mirror-input",(char*)"/volume",(char*)"/command"};need(servo_arguments(3,defaults)==3,"restore default after collection fixture");
 puts("PASS production persisted collection -> shared topology -> source router -> independent banks/commands -> origin settlement -> exact split feedback -> one-side absent/reconnect -> bounded teardown; actual source copier/coordinator/codec, substituted ALSA and native receipts");
}
static void policy_checks(void){
 char *plain[]={"mirror-input","/volume","/command"},*off[]={"mirror-input","/volume","/command","--servo=off"},*on[]={"mirror-input","/volume","/command","900","--servo=on"};
 need(servo_disabled&&servo_arguments(3,plain)==3&&servo_disabled,"X-Touch starts without raw echo by default");
 need(servo_arguments(4,off)==3&&servo_disabled,"explicit servo-off without duration accepted");
 need(servo_arguments(5,on)==4&&!servo_disabled,"explicit servo-on with duration selects reference comparison");
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"synthetic command memory");MirrorInput in;MirrorBank b;MIRROR_COPY(s);
 reset_fixture(c,&in,&b,&s,8);s.ready=0;s.epoch=0;s.count=0;s.heartbeat=110;bank_apply(&b,&s,110);bank_touch(&b,1,1,111);
 s.heartbeat=120;bank_apply(&b,&s,120);need(b.chooser&&!b.ready&&bank_due(&b,0,120)==0&&bank_due(&b,1,120)<0,"healthy epoch-zero chooser parks unheld and protects observed held without fake root");
 Surface chooser_surface={.source=3,.full={32,0}};memset(observed_lcd,'X',sizeof(observed_lcd));memset(observed_led,127,sizeof(observed_led));
 need(channel_output(&chooser_surface,&b,&s,s.heartbeat),"chooser output traverses actual channel sender");for(unsigned i=0;i<112;i++)need(observed_lcd[i]==' ',"chooser blanks both display rows");for(unsigned i=0;i<=103;i++)need(!observed_led[i],"chooser clears native/controller mode LEDs");
 unsigned chooser_messages=output_count;need(channel_output(&chooser_surface,&b,&s,s.heartbeat)&&output_count==chooser_messages,"chooser output coalesces unchanged clearing");
 chooser_surface.master_touch=1;s.heartbeat=125;bank_apply(&b,&s,125);need(channel_output(&chooser_surface,&b,&s,s.heartbeat)&&output_count==chooser_messages,"held master does not park at chooser");chooser_surface.master_touch=0;need(channel_output(&chooser_surface,&b,&s,s.heartbeat)&&output_count==chooser_messages+1&&last_output.type==SND_SEQ_EVENT_PITCHBEND&&last_output.data.control.channel==8&&last_output.data.control.value==-8192,"released master parks on fresh chooser heartbeat");
 in.global_count=1;in.global_events[0].operation=GLOBAL_SAVE;input_pump(&in,&b,&s,121);need(!in.error&&!in.global_count&&!atomic_load(&c->published),"unready chooser discards queued global input without mailbox publication");
 bank_touch(&b,1,0,122);s.heartbeat=130;bank_apply(&b,&s,130);need(bank_due(&b,1,130)==0,"held chooser fader parks only after release/freshness barrier");
 snapshot_fixture(&s,8);s.heartbeat=140;bank_apply(&b,&s,140);s.heartbeat=150;bank_apply(&b,&s,150);need(b.ready&&!b.chooser&&bank_due(&b,0,150)==8192,"same bank binds saved-project source after chooser");
 input_pitch(&in,&b,0,12000,151);atomic_store(&c->new_project_intent,1);input_pump(&in,&b,&s,152);need(!in.error&&!atomic_load(&c->published)&&!in.desires[0][CF_VOLUME].pending,"accepted intent discards unpublished fader desires");
 puts("PASS chooser production policy and output with synthetic snapshot/captured ALSA sink: blank displays, no native commands, held suppression and saved-root rebind");
 reset_fixture(c,&in,&b,&s,12);input_pitch(&in,&b,0,5336,99);input_discard(&in);s.tracks[0].bits=1056151962;s.tracks[0].revision=44;bank_apply(&b,&s,100);
 input_touch(&in,&b,&s,0,1,100);input_pitch(&in,&b,0,5336,100);input_touch(&in,&b,&s,0,0,101);input_pump(&in,&b,&s,102);
 need(!in.flights[0].flight&&!atomic_load(&c->published),"duplicate observed physical report does not revert newer MPC value");s.heartbeat=110;bank_apply(&b,&s,110);need(bank_due(&b,0,110)==7795,"stationary release catches up from physical5336 to actual MPC state");
 reset_fixture(c,&in,&b,&s,12);s.heartbeat=110;bank_apply(&b,&s,110);need(bank_due(&b,0,110)==8192,"automatic startup follows fresh MPC without touch initialization");
 Surface physical={.source=3,.full={32,0}};unsigned echo_before=output_count;
 need(surface_pitch(&physical,&in,&b,&s,0,0,110)&&surface_pitch(&physical,&in,&b,&s,0,12000,110)&&surface_pitch(&physical,&in,&b,&s,0,16383,111),"production handler accepts rapid touchless endpoint travel");
 need(output_count==echo_before+3&&last_output.data.control.value==8191&&in.desires[0][CF_VOLUME].bits==0x3f800000&&!atomic_load(&c->published)&&!atomic_load(&c->slots[0].settled),"every raw servo target is emitted while latest fullscale desire coalesces separately");
 input_pitch(&in,&b,0,16383,111);input_touch(&in,&b,&s,0,1,112);need(in.desires[0][CF_VOLUME].pending,"late sensed touch preserves first touchless position");input_touch(&in,&b,&s,0,0,113);input_pump(&in,&b,&s,113);need(in.flights[0].flight&&in.flights[0].bits==0x3f800000&&b.faders[0].touch==MT_UP,"first touchless fullscale report reaches command input immediately");
 /* workflow-4 records303/305/309: physical8864, servo8864, then redundant
  * same-value MOTOR after request134 settled. Native call/commit is substituted. */
 reset_fixture(c,&in,&b,&s,8);s.tracks[2].bits=1039008696;s.tracks[2].revision=270;s.heartbeat=300950;bank_apply(&b,&s,300950);b.faders[2].last_sent=1904;b.faders[2].last_tick=300431;
 need(surface_pitch(&physical,&in,&b,&s,2,8864,300952),"recorded touchless position reaches production servo handler");input_pump(&in,&b,&s,300953);need(in.flights[0].flight==1&&in.flights[0].bits==1057653290,"recorded8864 value reaches native request");
 complete(c,1,300957);s.tracks[2].bits=1057653290;s.tracks[2].revision=272;s.heartbeat=301012;bank_apply(&b,&s,301012);input_pump(&in,&b,&s,301012);s.heartbeat=301016;bank_apply(&b,&s,301016);
 need(in.settled==1&&bank_due(&b,2,301016)<0,"source settlement cannot resend recorded same8864 servo target");
 bank_touch(&b,2,1,301020);bank_touch(&b,2,0,301021);s.heartbeat=301030;bank_apply(&b,&s,301030);need(bank_due(&b,2,301030)<0,"touch-only cycle does not force same-value motor output");
 s.tracks[2].bits=0x3f200000;s.tracks[2].revision=274;s.heartbeat=301070;bank_apply(&b,&s,301070);need(bank_due(&b,2,301070)==10239,"different actual MPC source value still drives host motor feedback");
 reset_fixture(c,&in,&b,&s,9);bank_navigate(&b,1,100);bank_apply(&b,&s,101);s.heartbeat=110;bank_apply(&b,&s,110);need(b.faders[1].empty&&bank_due(&b,1,110)==0,"one-track last bank parks unassigned strips");
 input_pitch(&in,&b,1,12000,111);input_pump(&in,&b,&s,112);input_control(&in,&b,&s,1,CF_MUTE,0,1,112);input_pump(&in,&b,&s,113);need(!in.flights[0].flight&&!atomic_load(&c->published),"empty strip never creates MPC commands");
 ChannelWire empty=channel_wire(&s,NULL,CF_VOLUME);need(!memcmp(empty.name,"       ",7)&&!memcmp(empty.value,"       ",7)&&!empty.color&&!empty.ring&&!empty.led[0]&&!empty.led[1]&&!empty.led[2]&&!empty.led[3],"known empty presentation clears names/values/colors/ring/buttons");
 unsigned before_output=output_count;bank_drop(&b,114);need(channel_output(&physical,&b,&s,s.heartbeat)&&output_count==before_output,"unavailable bank does not masquerade as empty presentation");
 reset_fixture(c,&in,&b,&s,1);b.faders[0].last_sent=8192;input_touch(&in,&b,&s,0,1,100);servo_disabled=1;before_output=output_count;
 need(surface_pitch(&physical,&in,&b,&s,0,12000,101)&&output_count==before_output&&b.faders[0].last_sent==8192&&in.desires[0][CF_VOLUME].pending&&!atomic_load(&c->published)&&!atomic_load(&c->slots[0].settled),"X-Touch servo-off suppresses only raw output, never pretends target was sent or request settled");
 input_pump(&in,&b,&s,102);need(in.flights[0].flight==1&&bank_due(&b,0,102)<0,"servo-off retains command publication and held motor suppression");servo_disabled=0;
 puts("PASS recorded8864 source settlement emits no redundant MOTOR; actual changes still follow; fresh empty parking/presentation stays distinct from unavailable and creates no commands");
 reset_fixture(c,&in,&b,&s,12);input_touch(&in,&b,&s,0,1,100);input_pitch(&in,&b,0,12000,101);input_pump(&in,&b,&s,102);CommandRequest r;need(command_request_read(c->slots,1,&r),"held movement publishes request");uint32_t bits=r.bits;
 need(in.flights[0].flight==1&&b.faders[0].touch==MT_DOWN&&input_blocked(&in,0),"deliberate motion submits continuously before release");
 gesture(&in,&b,&s,1,4000,103);input_pump(&in,&b,&s,104);need(!in.desires[1][CF_VOLUME].pending&&atomic_load(&c->published)==2&&input_pending(&in)==2,"independent strip publishes without waiting for another body");
 atomic_store(&c->slots[0].returned,1);input_pump(&in,&b,&s,104);need(!in.settled,"return never settles source");
 complete(c,1,105);atomic_store(&c->slots[0].sealed,0);input_pump(&in,&b,&s,104);need(!in.error&&in.flights[0].flight==1,"next-millisecond event does not fail earlier clock sample");
 s.tracks[0].bits=bits;s.tracks[0].revision=4;s.heartbeat=106;bank_apply(&b,&s,106);input_pump(&in,&b,&s,106);need(!in.settled,"body done and copied commit are not sealed evidence");
 atomic_store(&c->slots[0].sealed,1);input_pump(&in,&b,&s,106);need(in.settled==1&&in.flights[0].acknowledged&&atomic_load(&c->slots[0].settled)==1,"per-slot client settlement waits consumer reuse permission");
 reclaim_fixture(c,1);input_pump(&in,&b,&s,110);need(!in.flights[0].flight&&in.flights[1].flight==2,"first completed slot releases independently of second native flight");
 input_pitch(&in,&b,0,13000,111);input_pump(&in,&b,&s,111);need(in.flights[0].flight==3&&atomic_load(&c->published)==3,"same strip reuses completed slot without global rate delay or waiting for other strip");
 input_touch(&in,&b,&s,0,0,151);s.heartbeat=160;bank_apply(&b,&s,160);
 reset_fixture(c,&in,&b,&s,12);input_touch(&in,&b,&s,0,1,100);input_pitch(&in,&b,0,8192,100);input_pitch(&in,&b,0,12000,101);bank_navigate(&b,1,102);input_discard(&in);bank_apply(&b,&s,103);input_sync(&in,&b);input_touch(&in,&b,&s,0,1,104);input_pitch(&in,&b,0,13000,104);input_touch(&in,&b,&s,0,0,105);input_pump(&in,&b,&s,106);need(!in.flights[0].flight,"held bank change and duplicate press cannot retarget");
 gesture(&in,&b,&s,0,12000,110);s.epoch=2;s.tracks[8].serial+=100;s.tracks[8].binding+=100;bank_apply(&b,&s,112);input_sync(&in,&b);input_pump(&in,&b,&s,113);need(!in.flights[0].flight,"reload cancels unsent motion with retained Program");
 gesture(&in,&b,&s,0,12000,120);input_discard(&in);bank_disconnect(&b);bank_apply(&b,&s,122);input_pump(&in,&b,&s,123);need(!in.flights[0].flight&&b.faders[0].touch==MT_UNKNOWN,"disconnect cancels motion and resets physical baseline");
 reset_fixture(c,&in,&b,&s,1);s.tracks[0].vptr=0x6932338;s.tracks[0].fields[CF_MIDI_VOLUME].available=0;bank_apply(&b,&s,100);gesture(&in,&b,&s,0,12000,100);input_pump(&in,&b,&s,102);need(!in.flights[0].flight,"MIDI input without captured Track level remains unavailable");
 for(unsigned fault=0;fault<5;fault++){
  reset_fixture(c,&in,&b,&s,1);gesture(&in,&b,&s,0,12000,100);input_pump(&in,&b,&s,102);
  if(fault==0){atomic_store(&c->slots[0].done,1);atomic_store(&c->slots[0].sealed,1);evidence(c,1,CE_QUEUE,102,0);evidence(c,1,CE_BODY,102,0);evidence(c,1,CE_DONE,102,0);}
  if(fault==1)atomic_store(&c->trace_error,C_TRACE_LOSS);
  if(fault==2){atomic_store(&c->slots[0].rejected,C_IDENTITY);atomic_store(&c->slots[0].processed,1);}
  if(fault==3){s.tracks[0].binding++;bank_apply(&b,&s,103);}
  input_pump(&in,&b,&s,fault==4||fault==2?120102:103);need(in.error&&input_blocked(&in,0)&&!in.settled,"missing commit/error/rejection/rebinding/expiry never certify settlement");
 }
 /* Real Sep12 failure shape: CANCEL29, GLOBAL_BEGIN, processed IDENTITY4,
  * no native dispatch and later producer reclamation. Native ownership and
  * hooks are substituted here; rejection must not become success or a retry. */
 reset_fixture(c,&in,&b,&s,2);s.editor_owner=39;in.jog_epoch=s.epoch;input_global_add(&in,&s,GLOBAL_KEY_CANCEL,0,100);input_pump(&in,&b,&s,101);
 need(in.flights[0].field==GLOBAL_KEY_CANCEL&&in.flights[0].flight==1,"captured Cancel operation translated to current schema reaches the real global input queue");
 CommandSlot *refused=c->slots;CommandLane *lane=refused->lanes;
 CommandEvent begin={.kind=CE_GLOBAL_BEGIN,.request=1,.tick=102,.token=123,.program=0x90000,.controller=GLOBAL_KEY_CANCEL,.reserved=GLOBAL_KEY_CANCEL,.revision=1};
 atomic_store(&lane->sequence,1);command_event_store(lane->events,&begin);atomic_store(&lane->published,1);
 atomic_store(&refused->rejected,C_IDENTITY);atomic_store(&refused->processed,1);atomic_store(&c->consumed,1);
 input_control(&in,&b,&s,1,CF_PAN,3,0,102);input_pump(&in,&b,&s,103);
 need(!in.error&&!in.refused&&!in.settled&&input_pending(&in)==2,"refusal waits producer closure while an independent pan request progresses");
 atomic_store(&c->reclaimed,1);atomic_store_explicit(&refused->reclaimed,1,memory_order_release);input_pump(&in,&b,&s,104);
 need(!in.error&&in.refused==1&&!in.settled&&!in.flights[0].flight&&atomic_load(&c->published)==2&&!atomic_load(&refused->settled),"reclaimed refusal clears only client flight, without settlement or retry");
 complete(c,2,105);s.tracks[1].fields[CF_PAN].bits=in.flights[1].bits;s.tracks[1].fields[CF_PAN].revision=4;s.heartbeat=106;input_pump(&in,&b,&s,106);
 need(!in.error&&in.settled==1&&in.refused==1,"independent channel settles after refused navigation");
 /* Even an inconsistent reclaimed flag cannot certify dispatched work. */
 reset_fixture(c,&in,&b,&s,1);gesture(&in,&b,&s,0,12000,100);input_pump(&in,&b,&s,101);
 atomic_store(&c->slots[0].rejected,C_IDENTITY);atomic_store(&c->slots[0].processed,1);atomic_store(&c->slots[0].dispatched,1);atomic_store(&c->slots[0].reclaimed,1);input_pump(&in,&b,&s,102);
 need(in.error==C_TRACE_AMBIGUOUS&&!in.refused&&!in.settled,"post-dispatch rejection remains fatal and cannot masquerade as harmless refusal");
 reset_fixture(c,&in,&b,&s,1);atomic_store(&c->published,16);atomic_store(&c->consumed,16);atomic_store(&c->reclaimed,16);gesture(&in,&b,&s,0,12000,100);input_pump(&in,&b,&s,102);need(in.flights[0].flight==17,"no sixteen-request consumption limit");
 reset_fixture(c,&in,&b,&s,1);atomic_store(&c->published,COMMAND_SEQUENCE_LAST);atomic_store(&c->consumed,COMMAND_SEQUENCE_LAST);atomic_store(&c->reclaimed,COMMAND_SEQUENCE_LAST);gesture(&in,&b,&s,0,12000,100);input_pump(&in,&b,&s,102);need(in.error==C_CAPACITY,"sequence never wraps or aliases an old request");
 unsigned channel_fields[]={CF_PAN,CF_MUTE,CF_SOLO,CF_ARM,CF_SELECTION};
 for(unsigned i=0;i<5;i++){
  unsigned f=channel_fields[i];reset_fixture(c,&in,&b,&s,1);input_control(&in,&b,&s,0,f,1,0,100);input_pump(&in,&b,&s,101);need(in.flights[0].flight==1,"channel control publishes same reusable mailbox");CommandRequest r;need(command_request_read(c->slots,1,&r)&&r.reserved==f&&r.track_owner==2&&r.program_owner==3,"complete Track/Program/field target identity");
  complete(c,1,102);CopiedField *actual=f==CF_SELECTION?&s.selection:s.tracks[0].fields+command_source_field(f);actual->bits=f==CF_SELECTION?s.tracks[0].track:r.bits;actual->revision=4;s.heartbeat=103;bank_apply(&b,&s,103);input_pump(&in,&b,&s,103);need(!in.error&&in.settled==1,"all channel command evidence settles through actual retained field");
 }
 reset_fixture(c,&in,&b,&s,1);s.tracks[0].available=0;bank_apply(&b,&s,100);input_control(&in,&b,&s,0,CF_MUTE,0,1,100);input_pump(&in,&b,&s,101);need(in.flights[0].flight==1&&!b.faders[0].available,"unavailable volume does not falsify mute capability");
 reset_fixture(c,&in,&b,&s,1);input_control(&in,&b,&s,0,CF_PAN,2,0,100);s.tracks[0].fields[CF_PAN].incarnation++;input_pump(&in,&b,&s,101);need(!in.flights[0].flight&&!in.desires[0][CF_PAN].pending,"pending encoder never retargets Property incarnation");
 /* Ordered toggles retain both operations, unlike continuous controls. */
 reset_fixture(c,&in,&b,&s,2);input_control(&in,&b,&s,0,CF_MUTE,0,1,100);input_control(&in,&b,&s,0,CF_MUTE,0,1,101);input_pump(&in,&b,&s,102);
 need(in.edge_count==1&&atomic_load(&c->published)==1&&in.flights[0].bits==1,"double Mute retains first on and pending off");complete(c,1,103);s.tracks[0].fields[CF_MUTE].bits=1;s.tracks[0].fields[CF_MUTE].revision=4;s.heartbeat=104;bank_apply(&b,&s,104);input_pump(&in,&b,&s,104);reclaim_fixture(c,1);input_pump(&in,&b,&s,105);
 need(atomic_load(&c->published)==2&&in.flights[0].bits==0&&!in.edge_count,"second Mute executes after actual first settlement");
 reset_fixture(c,&in,&b,&s,2);b.fader_field=CF_PAN;bank_drop(&b,100);bank_apply(&b,&s,101);input_touch(&in,&b,&s,0,1,102);input_pitch(&in,&b,0,12000,103);input_control(&in,&b,&s,0,CF_VOLUME,2,0,103);input_pump(&in,&b,&s,104);
 need(atomic_load(&c->published)==2&&in.gestures[0].held&&in.flights[0].field==CF_VOLUME&&in.flights[1].field==CF_PAN,"Flip keeps independent held pan fader and volume encoder desires");
 reset_fixture(c,&in,&b,&s,2);s.tracks[1].vptr=0x6931f70;bank_view(&b,BV_RETURN,100);bank_apply(&b,&s,101);need(b.playable_count==1&&b.faders[0].identity.serial==2&&b.faders[1].empty,"Return role bank preserves real master membership identity");
 input_control(&in,&b,&s,0,CF_SEND3,10,0,102);input_pump(&in,&b,&s,103);need(in.flights[0].field==CF_SEND3&&in.flights[0].target.serial==2,"send assignment uses channel field identity in role bank");
 unsigned char clock_text[10];s.position_available=1;s.bar=12;s.beat=2;s.clock=480;channel_position(clock_text,&s);need(!memcmp(clock_text,"  13 3  48",10),"native default playhead BBT adds one to bar/beat only");s.position_available=0;channel_position(clock_text,&s);need(!memcmp(clock_text,"----------",10),"expired position visibly unavailable");
 puts("PASS ordered discrete edges, independent Flip controls, real role/send identity and native BBT display formatting (synthetic snapshots)");
 /* Select presses are ordered globally, independent of strip scan order. */
 reset_fixture(c,&in,&b,&s,12);input_control(&in,&b,&s,1,CF_SELECTION,0,1,100);input_control(&in,&b,&s,0,CF_SELECTION,0,1,101);
 need(!in.desires[1][CF_SELECTION].pending&&in.desires[0][CF_SELECTION].pending,"latest Select replaces older other-strip intent before round-robin service");
 input_pump(&in,&b,&s,102);CommandRequest selection_request;need(command_request_read(c->slots,1,&selection_request)&&selection_request.serial==1,"reverse-order presses submit only latest exact selection target");
 complete(c,1,103);s.selection.bits=s.tracks[0].track;s.selection.revision=4;s.heartbeat=104;bank_apply(&b,&s,104);input_pump(&in,&b,&s,104);
 need(!in.error&&in.settled==1&&in.flights[0].acknowledged,"latest Select settles actual selected Track");reclaim_fixture(c,1);s.heartbeat=150;bank_apply(&b,&s,150);input_pump(&in,&b,&s,150);need(atomic_load(&c->published)==1&&!in.flights[0].flight,"older selection cannot execute after latest selection settlement");
 reset_fixture(c,&in,&b,&s,12);input_control(&in,&b,&s,1,CF_SELECTION,0,1,100);input_pump(&in,&b,&s,101);input_control(&in,&b,&s,0,CF_SELECTION,0,1,102);
 need(in.flights[0].flight==1&&in.flights[0].target.serial==2&&in.desires[0][CF_SELECTION].pending,"new Select preserves already dispatched target and pending latest identity");
 complete(c,1,103);s.selection.bits=s.tracks[1].track;s.selection.revision=4;s.heartbeat=104;bank_apply(&b,&s,104);input_pump(&in,&b,&s,104);need(in.settled==1&&in.flights[0].acknowledged,"older dispatched Select still requires its source settlement");
 reclaim_fixture(c,1);s.heartbeat=150;bank_apply(&b,&s,150);input_pump(&in,&b,&s,150);need(command_request_read(c->slots,2,&selection_request)&&selection_request.serial==1,"latest selection follows settled dispatched work without retargeting it");
 complete(c,2,151);s.selection.bits=s.tracks[0].track;s.selection.revision=6;s.heartbeat=152;bank_apply(&b,&s,152);input_pump(&in,&b,&s,152);need(!in.error&&in.settled==2&&in.flights[0].acknowledged,"latest selection wins after two properly settled native extents");
 for(unsigned cancel=0;cancel<4;cancel++){
  reset_fixture(c,&in,&b,&s,12);input_control(&in,&b,&s,1,CF_SELECTION,0,1,100);input_control(&in,&b,&s,0,CF_SELECTION,0,1,101);
  if(cancel==0){bank_navigate(&b,1,102);input_discard(&in);}
  else if(cancel==1)s.epoch++;
  else if(cancel==2)s.tracks[0].binding++;
  else{input_discard(&in);bank_disconnect(&b);}
  s.heartbeat=103;bank_apply(&b,&s,103);input_sync(&in,&b);input_pump(&in,&b,&s,103);
  need(!in.flights[0].flight&&!atomic_load(&c->published),"bank/reload/binding/disconnect cancels latest unsent selection");
  for(unsigned i=0;i<8;i++)need(!in.desires[i][CF_SELECTION].pending,"cancellation cannot restore an older selection desire");
 }
 puts("PASS global latest Select: reverse strip order, dispatched extent settlement, and bank/reload/binding/disconnect cancellation");
 ChannelWire w;reset_fixture(c,&in,&b,&s,1);memcpy(s.tracks[0].fields[CF_NAME].text,"Drum 008",8);s.tracks[0].fields[CF_NAME].length=8;w=channel_wire(&s,s.tracks,CF_VOLUME);
 need(!memcmp(w.name,"Dru008 ",7)&&w.color==7&&w.ring==70,"distinguishable fixture suffix, readable populated black and centered pan ring");
 unsigned char lcd[15];channel_lcd(lcd,7,1,w.value);need(!memcmp(lcd,(unsigned char[]){0xf0,0,0,0x66,0x14,0x12,105},7)&&lcd[14]==0xf7,"bounded MCU LCD offset and framing");
 unsigned char colors[8]={1,2,3,4,5,6,7,0};channel_colors(lcd,colors);need(lcd[5]==0x72&&!memcmp(lcd+6,colors,8)&&lcd[14]==0xf7,"X-Touch eight palette values framing");
 reset_fixture(c,&in,&b,&s,2);c->seconds=0;input_pitch(&in,&b,0,12000,900010);input_pump(&in,&b,&s,900010);need(in.flights[0].flight==1,"manual input accepts actual gesture beyond900 seconds");
 input_pitch(&in,&b,1,4000,900011);input_discard(&in);bank_disconnect(&b);
 input_drain(&in,&b,&s,900012);need(in.flights[0].flight==1&&atomic_load(&c->published)==1&&!command_request_stop(c),"disconnected drain preserves published request without submitting or stopping producer");
 complete(c,1,900013);s.heartbeat=900014;s.tracks[0].bits=in.flights[0].bits;s.tracks[0].revision=4;bank_apply(&b,&s,900014);input_drain(&in,&b,&s,900014);
 need(in.flights[0].acknowledged&&in.settled==1&&!input_drained(&in)&&!command_request_stop(c),"stop waits owner reclaim after certified source settlement");
 reclaim_fixture(c,1);input_drain(&in,&b,&s,900015);need(input_drained(&in)&&command_request_stop(c),"exclusive writer can hand producer stop only the reclaimed frontier");
 need(atomic_load(&c->published)==1&&atomic_load(&c->stop_sequence)==1,"discarded desire never submits during stop drain");
 puts("PASS manual beyond900s input, disconnected published-only drain, unsent cancellation, settled-versus-reclaimed stop handoff");
 free(c);puts("PASS automatic sync, touchless/first position and duplicate observed stationary input, continuous held motion, sealed settlement/reuse/rate, sequence17, wrap rejection, bank/reload/disconnect and error policy");
}

static void fixture_motor_pass(Surface*,MirrorBank*,CopiedMirror*,uint32_t);
static void profile_policy_checks(void){
 char *generic[]={"mirror-input","/volume","/command","--profile=generic","--endpoint-client=MCU Device","--endpoint-port=MCU Port"};
 need(servo_arguments(6,generic)==3&&controller.profile==&mcu_profile_generic&&!strcmp(controller.client,"MCU Device")&&!strcmp(controller.port,"MCU Port")&&controller.physical_only&&!servo_disabled,"explicit generic profile selects exact endpoint and standard closed-loop raw echo");
 need(mcu_endpoint_match(&controller,"MCU Device",1,"MCU Port",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE),"generic physical endpoint matches exact stable names and bidirectional capabilities");
 need(!mcu_endpoint_match(&controller,"MCU Device",0,"MCU Port",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE)&&!mcu_endpoint_match(&controller,"MCU Device",1,"Other",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE)&&!mcu_endpoint_match(&controller,"MCU Device",1,"MCU Port",MCU_ENDPOINT_READ|MCU_ENDPOINT_WRITE),"generic default rejects virtual, wrong-name and non-bidirectional candidates");
 unsigned caps=SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE;snd_seq_addr_t found,first;
 FakeClient unique[]={
  {.name="Unrelated",.type=SND_SEQ_KERNEL_CLIENT,.client=10,.count=1,.ports={{.name="MCU Port",.capabilities=caps,.port=1}}},
  {.name="MCU Device",.type=SND_SEQ_USER_CLIENT,.client=11,.count=1,.ports={{.name="MCU Port",.capabilities=caps,.port=2}}},
  {.name="MCU Device",.type=SND_SEQ_KERNEL_CLIENT,.client=20,.count=1,.ports={{.name="MCU Port",.capabilities=caps,.port=3}}}
 };
 fake_enumeration(unique,3);need(discover_endpoints((snd_seq_t*)1,&controller,MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE,&found,NULL,0,NULL)&&found.client==20&&found.port==3&&!fake_port_queries[0]&&!fake_port_queries[1]&&fake_port_queries[2],"production discovery filters unrelated clients and nonphysical same-name clients before enumerating exact endpoint ports");first=found;
 FakeClient duplicate[]={
  {.name="MCU Device",.type=SND_SEQ_KERNEL_CLIENT,.client=20,.count=1,.ports={{.name="MCU Port",.capabilities=caps,.port=3}}},
  {.name="MCU Device",.type=SND_SEQ_KERNEL_CLIENT,.client=21,.count=1,.ports={{.name="MCU Port",.capabilities=caps,.port=4}}}
 };
 fake_enumeration(duplicate,2);need(!discover_endpoints((snd_seq_t*)1,&controller,MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE,&found,NULL,0,NULL),"production discovery rejects duplicate exact endpoint identities rather than selecting by ALSA scan order");
 FakeClient absent[]={{.name="Unrelated",.type=SND_SEQ_KERNEL_CLIENT,.client=30,.count=1,.ports={{.name="MCU Port",.capabilities=caps,.port=5}}}};
 fake_enumeration(absent,1);need(!discover_endpoints((snd_seq_t*)1,&controller,MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE,&found,NULL,0,NULL)&&!fake_port_queries[0],"production discovery reports exact endpoint absent without enumerating unrelated ports");
 FakeClient reconnected[]={{.name="MCU Device",.type=SND_SEQ_KERNEL_CLIENT,.client=47,.count=1,.ports={{.name="MCU Port",.capabilities=caps,.port=9}}}};
 fake_enumeration(reconnected,1);need(discover_endpoints((snd_seq_t*)1,&controller,MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE,&found,NULL,0,NULL)&&found.client==47&&found.port==9&&!address_equal(first,found),"production discovery resolves the same stable endpoint identity after its numeric ALSA address changes");
 char *split[]={(char*)"mirror-input",(char*)"/volume",(char*)"/command",(char*)"--profile=generic",(char*)"--input-client=Split Device",(char*)"--input-port=From Controller",(char*)"--input-type=kernel",(char*)"--output-client=Split Device",(char*)"--output-port=To Controller",(char*)"--output-type=kernel"};
 need(servo_arguments(10,split)==3&&!strcmp(controller.port,"From Controller")&&!strcmp(controller_output.port,"To Controller"),"generic profile accepts independent From Controller and To Controller identities");
 FakeClient split_ports[]={{.name="Split Device",.type=SND_SEQ_KERNEL_CLIENT,.client=52,.count=2,.ports={{.name="From Controller",.capabilities=SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,.port=6},{.name="To Controller",.capabilities=SND_SEQ_PORT_CAP_WRITE,.port=7}}}};
 snd_seq_addr_t split_input,split_output;fake_enumeration(split_ports,1);
 need(discover_endpoints((snd_seq_t*)1,&controller,MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ,&split_input,&controller_output,MCU_ENDPOINT_WRITE,&split_output)&&split_input.client==52&&split_input.port==6&&split_output.client==52&&split_output.port==7&&fake_port_queries[0]==3,"one topology walk resolves input-only and output-only ports by their independent ALSA capabilities");
 Surface split_surface={.seq=(snd_seq_t*)1,.source=3,.full=split_input,.output=split_output};snd_seq_event_t split_event;unsigned split_kind,split_channel;int split_value;
 snd_seq_ev_clear(&split_event);snd_seq_ev_set_noteon(&split_event,0,0,127);split_event.source=split_output;
 need(!surface_event(&split_event,split_surface.full,&split_kind,&split_channel,&split_value),"To Controller traffic is not decoded as controller input");
 split_event.source=split_input;need(surface_event(&split_event,split_surface.full,&split_kind,&split_channel,&split_value)==1,"From Controller traffic reaches the generic input decoder");
 unsigned split_before=output_count;need(channel_send(&split_surface,&split_event)&&output_count==split_before+1&&address_equal(last_output.dest,split_output),"controller feedback targets only the configured To Controller address");
 split_ports[0].ports[1].port=11;fake_enumeration(split_ports,1);snd_seq_addr_t reconnected_input,reconnected_output;
 need(discover_endpoints((snd_seq_t*)1,&controller,MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ,&reconnected_input,&controller_output,MCU_ENDPOINT_WRITE,&reconnected_output)&&address_equal(reconnected_input,split_input)&&!address_equal(reconnected_output,split_output)&&reconnected_output.port==11,"input and output reconnect addresses resolve independently from one stable topology walk");
 need(servo_arguments(6,generic)==3,"restore same-port generic profile after split endpoint checks");
 NativeEndpointInventory hui_inventory;FakeClient hui_candidates[]={
  {.name="System",.type=SND_SEQ_KERNEL_CLIENT,.client=0,.count=1,.ports={{.name="Timer",.capabilities=caps,.port=0}}},
  {.name="MPC Live II",.type=SND_SEQ_KERNEL_CLIENT,.client=20,.count=1,.ports={{.name="MPC MIDI Port A",.capabilities=caps,.port=2}}},
  {.name="JUCE",.type=SND_SEQ_USER_CLIENT,.client=21,.count=1,.ports={{.name="Internal Musical",.capabilities=caps,.port=0}}},
  {.name="mpclearn-mcu-discovery",.type=SND_SEQ_USER_CLIENT,.client=22,.count=1,.ports={{.name="Owned",.capabilities=caps,.port=0}}},
  {.name="Kernel HUI",.type=SND_SEQ_KERNEL_CLIENT,.client=30,.count=2,.ports={{.name="DAW",.capabilities=caps,.port=1},{.name="mpclearn-owned",.capabilities=caps,.port=2}}},
  {.name="User HUI",.type=SND_SEQ_USER_CLIENT,.client=31,.count=2,.ports={{.name="DAW",.capabilities=caps,.port=3},{.name="Input Only",.capabilities=SND_SEQ_PORT_CAP_READ,.port=4}}}
 };
 fake_enumeration(hui_candidates,6);need(!enumerate_external((snd_seq_t*)1,&hui_inventory)&&hui_inventory.count==2&&!hui_inventory.overflow&&hui_inventory.rows[0].endpoint_type==NATIVE_ENDPOINT_KERNEL&&hui_inventory.rows[0].client_number==30&&hui_inventory.rows[0].port_number==1&&!strcmp((char*)hui_inventory.rows[0].client,"Kernel HUI")&&!strcmp((char*)hui_inventory.rows[0].port,"DAW")&&hui_inventory.rows[1].endpoint_type==NATIVE_ENDPOINT_USER&&hui_inventory.rows[1].client_number==31&&hui_inventory.rows[1].port_number==3&&!strcmp((char*)hui_inventory.rows[1].client,"User HUI"),"HUI inventory lists qualified kernel and user endpoints with stable names/type/address while excluding System, MPC, JUCE, owned and non-bidirectional ports");
 NativePreferencesEndpoint expected=hui_inventory.rows[1];need(inventory_exact_role(&hui_inventory,&expected,NATIVE_ENDPOINT_FROM_CONTROLLER)==1,"generation row resolves one exact typed input identity");
 expected.endpoint_type=NATIVE_ENDPOINT_KERNEL;need(!inventory_exact_role(&hui_inventory,&expected,NATIVE_ENDPOINT_FROM_CONTROLLER),"same stable names with a different endpoint type remain absent");
 NativeEndpointInventory duplicate_hui={.count=2};duplicate_hui.rows[0]=hui_inventory.rows[1];duplicate_hui.rows[1]=hui_inventory.rows[1];need(inventory_exact_role(&duplicate_hui,&hui_inventory.rows[1],NATIVE_ENDPOINT_TO_CONTROLLER)==2,"duplicate stable typed output identities remain ambiguous");
 fake_enumeration(split_ports,1);need(!enumerate_external((snd_seq_t*)1,&hui_inventory)&&hui_inventory.count==2&&hui_inventory.rows[0].capabilities==NATIVE_ENDPOINT_FROM_CONTROLLER&&hui_inventory.rows[1].capabilities==NATIVE_ENDPOINT_TO_CONTROLLER,"Preferences inventory retains independent READ/SUBS_READ-only and WRITE-only port roles");
 McuEndpoint any={.profile=&mcu_profile_generic,.client="Split Device",.port="From Controller",.physical_only=MCU_ENDPOINT_ANY};
 need(inventory_has_endpoint(&hui_inventory,&any,NATIVE_ENDPOINT_FROM_CONTROLLER)&&!inventory_has_endpoint(&hui_inventory,&any,NATIVE_ENDPOINT_TO_CONTROLLER),"legacy ANY endpoint availability matches its exact logical name and role");
 duplicate_hui.rows[0]=duplicate_hui.rows[1]=hui_inventory.rows[0];need(!inventory_has_endpoint(&duplicate_hui,&any,NATIVE_ENDPOINT_FROM_CONTROLLER),"duplicate logical identities never report available");
 FakeClient crowded[5];memset(crowded,0,sizeof(crowded));for(unsigned i=0;i<5;i++){crowded[i].name="External HUI";crowded[i].type=i&1?SND_SEQ_USER_CLIENT:SND_SEQ_KERNEL_CLIENT;crowded[i].client=40+i;crowded[i].count=3;for(unsigned p=0;p<3;p++)crowded[i].ports[p]=(FakePort){.name=p==0?"DAW A":p==1?"DAW B":"DAW C",.capabilities=caps,.port=p};}
 fake_enumeration(crowded,5);need(!enumerate_external((snd_seq_t*)1,&hui_inventory)&&hui_inventory.count==NATIVE_PREFERENCES_ENDPOINTS&&hui_inventory.overflow,"HUI inventory reports overflow instead of publishing an unbounded endpoint set");
 /* Exact LCXL3/MPK249 native inventory: internal MPC mirrors must not consume
    the twelve external endpoint slots or reject Apply before persistence. */
 FakeClient lcxl_native[]={
  {"System",SND_SEQ_KERNEL_CLIENT,0,2,{{"Timer",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE,0},{"Announce",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,1}}},
  {"MPC Live II",SND_SEQ_KERNEL_CLIENT,20,4,{{"MPC Public",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,0},{"MPC Private",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,1},{"MPC MIDI Port A",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,2},{"MPC MIDI Port B",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,3}}},
  {"MPK249",SND_SEQ_KERNEL_CLIENT,24,4,{{"Port A",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,0},{"Port B",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,1},{"MIDI",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,2},{"Remote",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,3}}},
  {"LCXL3 1",SND_SEQ_KERNEL_CLIENT,28,4,{{"LCXL3 1 MIDI In",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,0},{"LCXL3 1 DAW In",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,1},{"LCXL3 1 To DIN Out",SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,2},{"LCXL3 1 To DIN Out 2",SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,3}}},
  {"JUCE",SND_SEQ_USER_CLIENT,128,4,{{"Virtual MIDI Output 1",SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,0},{"Virtual MIDI Output 2",SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,1},{"Virtual MIDI Input 1",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,2},{"Virtual MIDI Input 2",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,3}}},
  {"MPC",SND_SEQ_USER_CLIENT,129,23,{{"MPC Live II MPC MIDI Port A",SND_SEQ_PORT_CAP_WRITE,0},{"MPC Live II MPC MIDI Port B",SND_SEQ_PORT_CAP_WRITE,1},{"MPK249 Port A",SND_SEQ_PORT_CAP_WRITE,2},{"MPK249 Port B",SND_SEQ_PORT_CAP_WRITE,3},{"MPK249 MIDI",SND_SEQ_PORT_CAP_WRITE,4},{"MPK249 Remote",SND_SEQ_PORT_CAP_WRITE,5},{"LCXL3 1 MIDI In",SND_SEQ_PORT_CAP_WRITE,6},{"LCXL3 1 DAW In",SND_SEQ_PORT_CAP_WRITE,7},{"JUCE Virtual MIDI Input 2",SND_SEQ_PORT_CAP_WRITE,8},{"MPC Live II MPC MIDI Port A",SND_SEQ_PORT_CAP_READ,9},{"MPC Live II MPC MIDI Port B",SND_SEQ_PORT_CAP_READ,10},{"MPK249 Port A",SND_SEQ_PORT_CAP_READ,11},{"MPK249 Port B",SND_SEQ_PORT_CAP_READ,12},{"MPK249 MIDI",SND_SEQ_PORT_CAP_READ,13},{"MPK249 Remote",SND_SEQ_PORT_CAP_READ,14},{"LCXL3 1 MIDI In",SND_SEQ_PORT_CAP_READ,15},{"LCXL3 1 DAW In",SND_SEQ_PORT_CAP_READ,16},{"LCXL3 1 To DIN Out",SND_SEQ_PORT_CAP_READ,17},{"LCXL3 1 To DIN Out 2",SND_SEQ_PORT_CAP_READ,18},{"JUCE Virtual MIDI Output 2",SND_SEQ_PORT_CAP_READ,19},{"JUCE Virtual MIDI Input 1",SND_SEQ_PORT_CAP_WRITE,20},{"JUCE Virtual MIDI Output 1",SND_SEQ_PORT_CAP_READ,21},{"mpclearn-controls MIDI",SND_SEQ_PORT_CAP_WRITE,22}}},
  {"mpclearn-controls",SND_SEQ_USER_CLIENT,130,2,{{"private ingress",SND_SEQ_PORT_CAP_WRITE,0},{"mpclearn-controls MIDI",SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,1}}},
  {"mpclearn-mirror-motors",SND_SEQ_USER_CLIENT,131,3,{{"private MCU touch and bank",SND_SEQ_PORT_CAP_WRITE,0},{"MCU eight motor output",SND_SEQ_PORT_CAP_READ,1},{"mpclearn Stop relay v1",SND_SEQ_PORT_CAP_WRITE,2}}}
 };
 fake_enumeration(lcxl_native,sizeof(lcxl_native)/sizeof(lcxl_native[0]));
 need(!enumerate_external((snd_seq_t*)1,&hui_inventory)&&hui_inventory.count==8&&!hui_inventory.overflow,"observed LCXL3 plus MPK249 inventory excludes MPC mirror ports instead of overflowing twelve external slots");
 char *virtual_endpoint[]={"mirror-input","/volume","/command","--profile=generic","--endpoint-client=Virtual MCU","--endpoint-port=Port 0","--endpoint-type=any"};
 need(servo_arguments(7,virtual_endpoint)==3&&!controller.physical_only&&mcu_endpoint_match(&controller,"Virtual MCU",0,"Port 0",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE),"explicit virtual emulation selection accepts only its exact named endpoint");
 char *missing[]={"mirror-input","/volume","/command","--profile=generic"};
 need(servo_arguments(4,missing)<0,"generic selection without exact client and port is rejected");
 char *wrong_xtouch[]={"mirror-input","/volume","/command","--profile=xtouch","--endpoint-client=Other","--endpoint-port=Port"};
 need(servo_arguments(6,wrong_xtouch)<0,"X-Touch endpoint identity cannot be overridden");

 char *restore[]={"mirror-input","/volume","/command"};need(servo_arguments(3,restore)==3&&controller.profile==&mcu_profile_xtouch&&controller.physical_only&&servo_disabled,"default selection restores exact physical X-Touch with raw echo off");
 char controller_directory[]="/tmp/mpclearn-controller.XXXXXX",controller_path[256];need(mkdtemp(controller_directory)!=NULL,"private controller preference fixture directory");snprintf(controller_path,sizeof(controller_path),"%s/controller-preferences",controller_directory);
 char controller_option_text[300];snprintf(controller_option_text,sizeof(controller_option_text),"--controller-preferences=%s",controller_path);char *saved_default[]={"mirror-input","/volume","/command",controller_option_text};
 need(servo_arguments(4,saved_default)==3&&controller.profile==&mcu_profile_xtouch&&!strcmp(controller.client,"X-Touch"),"missing saved selection retains unchanged X-Touch default through production option parser");
 char *set_generic[]={"mirror-input","--controller-set",controller_path,"--profile=generic","--endpoint-client=Controller With Spaces","--endpoint-port=Port Name","--endpoint-type=any"};
 need(input_bridge_main_not_called(7,set_generic)==0,"production controller-set stores generic exact endpoint");struct stat controller_stat;need(!stat(controller_path,&controller_stat)&&(controller_stat.st_mode&0777)==0600&&controller_stat.st_size==(off_t)sizeof(McuControllerCollectionFile),"saved controller collection has fixed bounded bytes and private mode");
 need(servo_arguments(4,saved_default)==3&&controller.profile==&mcu_profile_generic&&!strcmp(controller.client,"Controller With Spaces")&&!strcmp(controller.port,"Port Name")&&!controller.physical_only&&!servo_disabled,"production bridge loads saved generic identity including spaces and explicit virtual opt-in");
 char *set_split_generic[]={"mirror-input","--controller-set",controller_path,"--profile=generic","--input-client=Split Saved","--input-port=From Port","--input-type=kernel","--output-client=Split Saved","--output-port=To Port","--output-type=user"};
 need(input_bridge_main_not_called(10,set_split_generic)==0&&servo_arguments(4,saved_default)==3&&!strcmp(controller.port,"From Port")&&controller.physical_only==MCU_ENDPOINT_KERNEL&&!strcmp(controller_output.port,"To Port")&&controller_output.physical_only==MCU_ENDPOINT_USER,"production persistence retains independent input/output names and types");
 McuControllerCollection stored;need(mcu_controller_preferences_read_collection(controller_path,&stored)&&stored.count==1&&!stored.imported_legacy&&!strcmp(stored.records[0].input.port,"From Port")&&!strcmp(stored.records[0].output.port,"To Port"),"MCUCFG2 readback exposes one split endpoint record without legacy migration state");
 McuControllerCollection full={.count=MCU_CONTROLLER_CAPACITY};for(unsigned i=0;i<full.count;i++){mcu_controller_default(full.records+i);full.records[i].enabled=i%3!=0;}
 need(mcu_controller_preferences_write_collection(controller_path,&full)&&mcu_controller_preferences_read_collection(controller_path,&stored)&&stored.count==MCU_CONTROLLER_CAPACITY&&stored.records[0].enabled==0&&stored.records[1].enabled==1,"fixed collection atomically persists all sixteen bounded records including disabled entries");
 full.count=MCU_CONTROLLER_CAPACITY+1;need(!mcu_controller_preferences_write_collection(controller_path,&full)&&mcu_controller_preferences_read_collection(controller_path,&stored)&&stored.count==MCU_CONTROLLER_CAPACITY,"a seventeenth record is rejected without replacing the valid collection");
 need(input_bridge_main_not_called(10,set_split_generic)==0,"restore split singleton after collection capacity check");
 char *invalid_set[]={"mirror-input","--controller-set",controller_path,"--profile=generic"};need(input_bridge_main_not_called(4,invalid_set)==2,"invalid generic selection is rejected without replacing saved selection");need(servo_arguments(4,saved_default)==3&&controller.profile==&mcu_profile_generic,"rejected selection leaves prior saved controller intact");
 int corrupt_checksum=open(controller_path,O_RDWR);unsigned char changed;need(corrupt_checksum>=0&&pread(corrupt_checksum,&changed,1,32)==1,"read full-size preference checksum fixture");changed^=1;need(pwrite(corrupt_checksum,&changed,1,32)==1&&!close(corrupt_checksum)&&!mcu_controller_preferences_read_collection(controller_path,&stored),"full-size MCUCFG2 with changed content and stale checksum is rejected");
 need(input_bridge_main_not_called(10,set_split_generic)==0,"valid split selection atomically recovers checksum-corrupt MCUCFG2");
 int corrupt=open(controller_path,O_WRONLY|O_TRUNC);need(corrupt>=0&&write(corrupt,"broken",6)==6&&!close(corrupt),"malformed saved controller fixture");need(servo_arguments(4,saved_default)<0,"malformed existing selection is rejected rather than interpreted as a MIDI device");
 char *set_mini[]={"mirror-input","--controller-set",controller_path,"--profile=xtouch-mini"};need(input_bridge_main_not_called(4,set_mini)==0&&servo_arguments(4,saved_default)==3&&controller.profile==&mcu_profile_xtouch_mini,"valid Mini selection atomically recovers a malformed regular preference");
 char *set_xtouch[]={"mirror-input","--controller-set",controller_path,"--profile=xtouch"};need(input_bridge_main_not_called(4,set_xtouch)==0&&servo_arguments(4,saved_default)==3&&controller.profile==&mcu_profile_xtouch,"explicit saved X-Touch restores the default hardware profile");
 char native_path[256];snprintf(native_path,sizeof(native_path),"%s/native-preferences.state",controller_directory);int native_fd=open(native_path,O_RDWR|O_CREAT|O_EXCL,0600);need(native_fd>=0&&!ftruncate(native_fd,NATIVE_PREFERENCES_BYTES),"native request state fixture");NativePreferencesState *native=mmap(NULL,sizeof(*native),PROT_READ|PROT_WRITE,MAP_SHARED,native_fd,0);close(native_fd);need(native!=MAP_FAILED,"native request state mapping");memset(native,0,sizeof(*native));native->magic=NATIVE_PREFERENCES_MAGIC;native->version=NATIVE_PREFERENCES_VERSION;native->bytes=sizeof(*native);native->request_sequence=1;native->request_kind=NATIVE_REQUEST_APPLY;native->request_profile=NATIVE_CONTROLLER_XTOUCH_MINI;native->request_controller_index=0;native->request_enabled=1;native->request_input_endpoint_index=NATIVE_ENDPOINT_UNCHANGED;native->request_output_endpoint_index=NATIVE_ENDPOINT_UNCHANGED;
 need(native_preferences_apply(native_path,controller_path)==0&&native->claimed_sequence==1&&native->saved_profile==NATIVE_CONTROLLER_XTOUCH_MINI&&native->status==NATIVE_PREFERENCES_APPLYING&&!native->completed_sequence,"production owner helper claims one bounded Mini request and persists before restart");need(native_preferences_finish(native_path,1)==0&&native->completed_sequence==1&&native->active_profile==NATIVE_CONTROLLER_XTOUCH_MINI&&native->status==NATIVE_PREFERENCES_ACTIVE,"production owner helper records active restart completion");
 native->request_collection_generation=native->collection_generation;native->request_sequence=2;native->request_profile=99;need(native_preferences_apply(native_path,controller_path)==NATIVE_APPLY_INVALID&&native->claimed_sequence==2&&native->completed_sequence==2&&native->status==NATIVE_PREFERENCES_INVALID,"production owner helper completes an invalid request without replacing selection");need(servo_arguments(4,saved_default)==3&&controller.profile==&mcu_profile_xtouch_mini,"invalid native request retains the saved Mini selection");
 native->request_sequence=3;native->request_profile=NATIVE_CONTROLLER_HUI;native->request_input_inventory_generation=7;native->request_input_endpoint_index=0;native->request_input_endpoint_type=NATIVE_ENDPOINT_USER;native->request_output_inventory_generation=7;native->request_output_endpoint_index=0;native->request_output_endpoint_type=NATIVE_ENDPOINT_USER;native->request_hui_policy=NATIVE_HUI_MOTORIZED;need(native_preferences_apply(native_path,controller_path)==NATIVE_APPLY_STALE&&native->claimed_sequence==3&&native->completed_sequence==3&&native->status==NATIVE_PREFERENCES_STALE,"production owner rejects stale generation-tagged split HUI endpoints without opening discovery or replacing the active selection");need(servo_arguments(4,saved_default)==3&&controller.profile==&mcu_profile_xtouch_mini,"stale HUI request retains the saved Mini selection");
 fake_inventory_open=1;fake_enumeration(split_ports,1);need(!native_preferences_sync(native_path,controller_path,"saved"),"production owner refresh exposes split port inventory");
 native->request_sequence=4;native->request_collection_generation=native->collection_generation;native->request_controller_index=NATIVE_CONTROLLER_NEW;native->request_input_inventory_generation=native->request_output_inventory_generation=native->inventory_generation;native->request_input_endpoint_index=0;native->request_output_endpoint_index=1;native->request_input_endpoint_type=native->request_output_endpoint_type=NATIVE_ENDPOINT_KERNEL;
 need(!native_preferences_apply(native_path,controller_path)&&!native_preferences_finish(native_path,1)&&mcu_controller_preferences_read_collection(controller_path,&stored)&&stored.count==2&&stored.records[1].input.profile==&mcu_profile_hui&&!strcmp(stored.records[1].input.port,"From Controller")&&!strcmp(stored.records[1].output.port,"To Controller")&&stored.records[1].hui_policy==MCU_HUI_MOTORIZED,"production Add persists HUI with independent role-only ports without replacing Mini");
 native->request_sequence=5;native->request_collection_generation=native->collection_generation;native->request_kind=NATIVE_REQUEST_TOGGLE;native->request_controller_index=0;native->request_enabled=0;
 need(!native_preferences_apply(native_path,controller_path)&&!native_preferences_finish(native_path,1)&&native->active_controller_index==1&&native->active_profile==NATIVE_CONTROLLER_HUI&&native->active_hui_policy==NATIVE_HUI_MOTORIZED,"disabling record zero selects the first enabled record and its own HUI policy");
 need(!native_preferences_sync(native_path,controller_path,"saved")&&native->active_profile==NATIVE_CONTROLLER_HUI&&native->active_hui_policy==NATIVE_HUI_MOTORIZED,"owner sync retains first-enabled compatibility metadata");
 native->request_sequence=6;native->request_kind=NATIVE_REQUEST_REMOVE;native->request_collection_generation=native->collection_generation-1;
 need(native_preferences_apply(native_path,controller_path)==NATIVE_APPLY_STALE&&mcu_controller_preferences_read_collection(controller_path,&stored)&&stored.count==2,"a delayed collection mutation cannot retarget a changed row");
 native->request_sequence=7;native->request_kind=NATIVE_REQUEST_APPLY;native->request_collection_generation=native->collection_generation;native->request_controller_index=1;native->request_profile=NATIVE_CONTROLLER_XTOUCH;native->request_hui_policy=0;native->request_enabled=1;native->request_input_inventory_generation=native->request_output_inventory_generation=native->inventory_generation;
 need(!native_preferences_apply(native_path,controller_path)&&!native_preferences_finish(native_path,1)&&mcu_controller_preferences_read_collection(controller_path,&stored)&&stored.count==2&&stored.records[1].input.profile==&mcu_profile_xtouch&&!strcmp(stored.records[1].input.port,"From Controller")&&!strcmp(stored.records[1].output.port,"To Controller"),"production Edit supports manual split endpoint override for a known profile");
 native->request_sequence=8;native->request_kind=NATIVE_REQUEST_TOGGLE;native->request_enabled=0;native->request_collection_generation=native->collection_generation;
 char *any_enabled[]={"mirror-input","--controller-any-enabled",controller_path};
 need(!native_preferences_apply(native_path,controller_path)&&!native_preferences_finish(native_path,1)&&!native->active_profile&&input_bridge_main_not_called(3,any_enabled)==3,"all-disabled collection has no active compatibility profile and signals owner to stay stopped");
 for(unsigned n=0;n<2;n++){native->request_sequence=9+n;native->request_kind=NATIVE_REQUEST_REMOVE;native->request_controller_index=0;native->request_collection_generation=native->collection_generation;need(!native_preferences_apply(native_path,controller_path)&&!native_preferences_finish(native_path,1),"production Remove drains the collection without stale internal endpoint pointers");}
 need(mcu_controller_preferences_read_collection(controller_path,&stored)&&!stored.count&&input_bridge_main_not_called(3,any_enabled)==3,"empty collection survives checksum readback and stays stopped");fake_inventory_open=0;
 for(unsigned profile=NATIVE_CONTROLLER_GENERIC;profile<=NATIVE_CONTROLLER_PROFILE_COUNT;profile++){
  need(!input_bridge_main_not_called(4,set_xtouch)&&!native_preferences_sync(native_path,controller_path,"saved"),"disconnected known-profile edit fixture");
  McuControllerCollectionFile before_bytes,after_bytes;int capture=open(controller_path,O_RDONLY);need(capture>=0&&read(capture,&before_bytes,sizeof(before_bytes))==sizeof(before_bytes)&&!close(capture),"capture exact known-profile collection bytes");
  native->request_sequence++;native->request_kind=NATIVE_REQUEST_APPLY;native->request_controller_index=0;native->request_profile=profile;native->request_hui_policy=profile==NATIVE_CONTROLLER_HUI?NATIVE_HUI_PICKUP:0;native->request_enabled=1;native->request_collection_generation=native->collection_generation;native->request_input_endpoint_index=native->request_output_endpoint_index=NATIVE_ENDPOINT_UNCHANGED;
  need(native_preferences_apply(native_path,controller_path)==NATIVE_APPLY_INVALID&&native->completed_sequence==native->request_sequence,"owner rejects known-to-generic transition with both ports unchanged");
  capture=open(controller_path,O_RDONLY);need(capture>=0&&read(capture,&after_bytes,sizeof(after_bytes))==sizeof(after_bytes)&&!close(capture)&&!memcmp(&before_bytes,&after_bytes,sizeof(before_bytes)),"rejected protocol transition preserves exact collection bytes");
  fake_inventory_open=1;fake_enumeration(split_ports,1);need(!native_preferences_sync(native_path,controller_path,"saved"),"publish independent role-only endpoints for explicit transition");
  native->request_sequence++;native->request_input_inventory_generation=native->request_output_inventory_generation=native->inventory_generation;native->request_input_endpoint_type=native->request_output_endpoint_type=NATIVE_ENDPOINT_KERNEL;native->request_input_endpoint_index=0;
  need(native_preferences_apply(native_path,controller_path)==NATIVE_APPLY_INVALID,"owner requires explicit To as well as From on generic transition");
  native->request_sequence++;native->request_input_endpoint_index=NATIVE_ENDPOINT_UNCHANGED;native->request_output_endpoint_index=1;
  need(native_preferences_apply(native_path,controller_path)==NATIVE_APPLY_INVALID,"owner requires explicit From as well as To on generic transition");
  native->request_sequence++;native->request_input_endpoint_index=1;native->request_output_endpoint_index=0;
  need(native_preferences_apply(native_path,controller_path)==NATIVE_APPLY_INVALID,"owner rejects reversed endpoint roles on generic transition");
  native->request_sequence++;native->request_input_endpoint_index=0;native->request_output_endpoint_index=1;
  need(!native_preferences_apply(native_path,controller_path)&&!native_preferences_finish(native_path,1)&&mcu_controller_preferences_read_collection(controller_path,&stored)&&stored.records[0].input.profile==mcu_controller_profile(profile)&&!strcmp(stored.records[0].input.port,"From Controller")&&!strcmp(stored.records[0].output.port,"To Controller"),"owner accepts generic transition only after explicit independent role-valid selections");
  capture=open(controller_path,O_RDONLY);need(capture>=0&&read(capture,&before_bytes,sizeof(before_bytes))==sizeof(before_bytes)&&!close(capture),"capture explicitly selected generic collection");
  fake_inventory_open=0;native->request_sequence++;native->request_collection_generation=native->collection_generation;native->request_input_endpoint_index=native->request_output_endpoint_index=NATIVE_ENDPOINT_UNCHANGED;
  need(!native_preferences_apply(native_path,controller_path)&&!native_preferences_finish(native_path,1),"same-profile offline generic edit can retain its explicit endpoints");
  capture=open(controller_path,O_RDONLY);need(capture>=0&&read(capture,&after_bytes,sizeof(after_bytes))==sizeof(after_bytes)&&!close(capture)&&!memcmp(&before_bytes,&after_bytes,sizeof(before_bytes)),"same-profile offline edit preserves exact saved endpoint bytes");
  NativeEndpointInventory poisoned;memset(&poisoned,0xa5,sizeof(poisoned));need(!native_inventory_refresh(native,&poisoned)&&!poisoned.count&&!poisoned.overflow&&native->inventory_status==NATIVE_PREFERENCES_UNAVAILABLE,"ALSA open failure clears a poisoned inventory before any caller inspects overflow");
  unsigned active_before=native->active_controller_index;
  for(unsigned retry=0;retry<3;retry++){native->request_sequence++;native->request_kind=NATIVE_REQUEST_REFRESH;need(native_preferences_apply(native_path,controller_path)==NATIVE_APPLY_NONE&&native->status==NATIVE_PREFERENCES_UNAVAILABLE&&native->completed_sequence==native->request_sequence&&native->active_controller_index==active_before,"failed Refresh deterministically completes UNAVAILABLE with no restart request and retains current controller");}
  capture=open(controller_path,O_RDONLY);need(capture>=0&&read(capture,&after_bytes,sizeof(after_bytes))==sizeof(after_bytes)&&!close(capture)&&!memcmp(&before_bytes,&after_bytes,sizeof(before_bytes)),"failed Refresh leaves persisted collection bytes unchanged");
 }
 {
  need(input_bridge_main_not_called(4,set_mini)==0,"restore existing Mini before native LCXL3 Add regression");
  fake_inventory_open=1;fake_enumeration(lcxl_native,sizeof(lcxl_native)/sizeof(lcxl_native[0]));
  need(!native_preferences_sync(native_path,controller_path,"saved")&&native->inventory_status==NATIVE_PREFERENCES_ACTIVE&&native->inventory_count==8,"production inventory publishes eight external endpoints with MPC mirror ports present");
  unsigned daw=NATIVE_ENDPOINT_UNCHANGED;
  for(unsigned i=0;i<native->inventory_count;i++)if(native->endpoints[i].client_number==28&&native->endpoints[i].port_number==1)daw=i;
  need(daw!=NATIVE_ENDPOINT_UNCHANGED,"LCXL3 exact DAW endpoint remains selectable");
  native->request_sequence++;native->request_kind=NATIVE_REQUEST_APPLY;native->request_profile=NATIVE_CONTROLLER_LCXL3;
  native->request_controller_index=NATIVE_CONTROLLER_NEW;native->request_enabled=1;native->request_hui_policy=0;
  native->request_collection_generation=native->collection_generation;
  native->request_input_inventory_generation=native->request_output_inventory_generation=native->inventory_generation;
  native->request_input_endpoint_index=native->request_output_endpoint_index=daw;
  native->request_input_endpoint_type=native->request_output_endpoint_type=NATIVE_ENDPOINT_KERNEL;
  need(!native_preferences_apply(native_path,controller_path)&&!native_preferences_finish(native_path,1),"production LCXL3 Apply succeeds through inventory refresh, exact role resolution, persistence and session completion");
  need(mcu_controller_preferences_read_collection(controller_path,&stored)&&stored.count==2&&stored.records[0].input.profile==&mcu_profile_xtouch_mini&&stored.records[1].input.profile==&mcu_profile_lcxl3&&
       !strcmp(stored.records[1].input.client,"LCXL3 1")&&!strcmp(stored.records[1].input.port,"LCXL3 1 DAW In")&&
       !strcmp(stored.records[1].output.client,"LCXL3 1")&&!strcmp(stored.records[1].output.port,"LCXL3 1 DAW In")&&
       native->collection_count==2&&native->controllers[1].profile==NATIVE_CONTROLLER_LCXL3&&native->completed_sequence==native->request_sequence,
       "LCXL3 Add retains Mini and publishes saved LCXL3 row with exact bidirectional DAW names");
  fake_inventory_open=0;
 }
 puts("PASS native Preferences repairs: explicit role-valid generic transitions, same-profile offline retention, and initialized ALSA-open failure with deterministic UNAVAILABLE completion and unchanged persistence (ALSA substituted)");
 need(!munmap(native,sizeof(*native))&&!unlink(native_path)&&input_bridge_main_not_called(4,set_xtouch)==0,"native request fixture cleanup and saved default restoration");
 need(!unlink(controller_path)&&!rmdir(controller_directory),"controller preference fixture cleanup");
 need(servo_arguments(3,restore)==3&&controller.profile==&mcu_profile_xtouch&&mcu_has(&controller,MCU_CAP_COLOR),"profile output fixture restores default X-Touch capability policy");
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"profile fixture command memory");MirrorInput in;MirrorBank b;MIRROR_COPY(s);reset_fixture(c,&in,&b,&s,8);
 unsigned colors=observed_color_count;Surface xtouch={.source=3,.full={32,0}};for(unsigned i=0;i<8;i++)need(channel_output(&xtouch,&b,&s,100),"X-Touch profile output pass");need(observed_color_count>colors,"X-Touch retains vendor 0x72 color output");
 need(servo_arguments(6,generic)==3,"generic output selection");colors=observed_color_count;unsigned pressure=0;for(unsigned i=0;i<8;i++)pressure+=observed_press_count[i];Surface standard={.source=3,.full={32,0}};for(unsigned i=0;i<8;i++)need(channel_output(&standard,&b,&s,100),"generic profile output pass");unsigned after_pressure=0;for(unsigned i=0;i<8;i++)after_pressure+=observed_press_count[i];need(observed_color_count==colors&&after_pressure==pressure,"generic MCU emits neither X-Touch 0x72 color nor device-specific meter-enable/levels");
 unsigned echo=output_count;need(surface_pitch(&standard,&in,&b,&s,0,12000,101)&&output_count==echo+1&&last_output.type==SND_SEQ_EVENT_PITCHBEND,"generic profile applies standard MCU raw fader echo in the production pitch handler");

 char *mini[]={"mirror-input","/volume","/command","--profile=xtouch-mini"};need(servo_arguments(4,mini)==3&&controller.profile==&mcu_profile_xtouch_mini&&controller.physical_only&&servo_disabled&&!strcmp(controller.client,"X-TOUCH MINI")&&!strcmp(controller.port,"X-TOUCH MINI MIDI 1"),"explicit Mini profile selects its fixed physical endpoint with raw echo disabled");
 need(mcu_endpoint_match(&controller,"X-TOUCH MINI",1,"X-TOUCH MINI MIDI 1",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE)&&!mcu_endpoint_match(&controller,"X-TOUCH MINI",0,"X-TOUCH MINI MIDI 1",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE),"Mini profile accepts only its exact physical bidirectional endpoint");
 char *mini_override[]={"mirror-input","/volume","/command","--profile=xtouch-mini","--endpoint-client=Other","--endpoint-port=Port"};need(servo_arguments(6,mini_override)<0,"Mini endpoint identity cannot be overridden");
 char *mini_echo[]={"mirror-input","/volume","/command","--profile=xtouch-mini","--servo=on"};need(servo_arguments(5,mini_echo)==3&&servo_disabled,"Mini motor capability cannot be overridden by the diagnostic echo option");need(servo_arguments(4,mini)==3,"restore Mini policy after option refusal");
 reset_fixture(c,&in,&b,&s,8);
 snd_midi_event_t *codec;need(!snd_midi_event_new(16,&codec),"Mini captured-vector MIDI codec");snd_seq_addr_t mini_address={32,0};snd_seq_event_t mini_event;unsigned kind,channel;int value;
 const unsigned char encoder_up[]={0xb0,0x10,0x01},encoder_down[]={0xb0,0x10,0x41},push_down[]={0x90,0x20,0x7f},push_up[]={0x90,0x20,0},play_down[]={0x90,0x5e,0x7f},master_low[]={0xe8,0,0},master_high[]={0xe8,0,0x7f},master_outside[]={0xe8,0x7f,0x7f},strip_pitch[]={0xe0,0,0x40},master_touch[]={0x90,112,127},jog[]={0xb0,60,1};
#define MINI_EVENT(bytes) do{snd_seq_ev_clear(&mini_event);need(snd_midi_event_encode(codec,(bytes),3,&mini_event)==3,"Mini captured vector decode");mini_event.source=mini_address;}while(0)
 MINI_EVENT(encoder_up);need(surface_event(&mini_event,mini_address,&kind,&channel,&value)==1&&kind==4&&channel==0&&value==1,"captured Mini encoder clockwise vector uses production V-Pot decoding");
 MINI_EVENT(encoder_down);need(surface_event(&mini_event,mini_address,&kind,&channel,&value)==1&&kind==4&&channel==0&&value==-1,"captured Mini encoder counterclockwise vector uses production V-Pot decoding");
 MINI_EVENT(push_down);need(surface_event(&mini_event,mini_address,&kind,&channel,&value)==1&&kind==5&&channel==32&&value,"captured Mini V-Pot press uses existing MCU push semantics");
 MINI_EVENT(push_up);need(surface_event(&mini_event,mini_address,&kind,&channel,&value)==1&&kind==5&&channel==32&&!value,"captured Mini V-Pot release uses existing MCU push semantics");
 MINI_EVENT(play_down);need(surface_event(&mini_event,mini_address,&kind,&channel,&value)==1&&kind==12&&channel==94&&value,"captured Mini Play press uses existing MCU transport semantics");
 MINI_EVENT(master_low);need(surface_event(&mini_event,mini_address,&kind,&channel,&value)==1&&kind==10&&channel==8&&value==0,"captured Mini master minimum maps to the existing command-domain minimum");
 s.master=(CopiedField){.property=0x800a8,.owner_incarnation=100,.incarnation=101,.bits=0x3f000000,.revision=2,.available=1,.seed=1};input_master_pitch(&in,&s,value,102);input_pump(&in,&b,&s,102);CommandRequest mini_request;need(command_request_read(c->slots,1,&mini_request)&&mini_request.reserved==CF_MASTER&&mini_request.bits==0,"captured Mini master minimum publishes an exact zero master request");
 reset_fixture(c,&in,&b,&s,8);s.master=(CopiedField){.property=0x800a8,.owner_incarnation=100,.incarnation=101,.bits=0x3f000000,.revision=2,.available=1,.seed=1};MINI_EVENT(master_high);need(surface_event(&mini_event,mini_address,&kind,&channel,&value)==1&&kind==10&&value==16383,"captured Mini raw maximum 16256 calibrates to the existing command-domain maximum");input_master_pitch(&in,&s,value,103);input_pump(&in,&b,&s,103);need(command_request_read(c->slots,1,&mini_request)&&mini_request.reserved==CF_MASTER&&mini_request.bits==0x3f800000,"captured Mini master maximum publishes exact unity");
 MINI_EVENT(master_outside);need(surface_event(&mini_event,mini_address,&kind,&channel,&value)<0,"Mini rejects master input above its captured physical range instead of extrapolating");
 MINI_EVENT(strip_pitch);need(!surface_event(&mini_event,mini_address,&kind,&channel,&value),"Mini profile ignores absent strip faders");MINI_EVENT(master_touch);need(!surface_event(&mini_event,mini_address,&kind,&channel,&value),"Mini profile ignores absent touch sensing");MINI_EVENT(jog);need(!surface_event(&mini_event,mini_address,&kind,&channel,&value),"Mini profile ignores absent jog wheel");
#undef MINI_EVENT
 snd_midi_event_free(codec);
 unsigned mini_pitch=0,mini_lcd=observed_lcd_count,mini_time=observed_position_count,mini_meter=0,mini_colors=observed_color_count,mini_rings=0;for(unsigned i=0;i<9;i++)mini_pitch+=observed_pitch_count[i];for(unsigned i=0;i<8;i++){mini_meter+=observed_press_count[i];mini_rings+=observed_ring_count[i];}
 Surface mini_surface={.source=3,.full=mini_address};unsigned mini_echo_count=output_count;need(surface_servo(&mini_surface,&b,0,12000,109)&&output_count==mini_echo_count,"Mini suppresses raw pitch echo even when the diagnostic echo option was requested");reset_fixture(c,&in,&b,&s,0);s.ready=0;s.epoch=0;s.heartbeat=110;fixture_motor_pass(&mini_surface,&b,&s,110);need(mini_surface.chooser_cleared,"Mini chooser output completes without motor feedback");
 reset_fixture(c,&in,&b,&s,8);s.master=(CopiedField){.property=0x800a8,.owner_incarnation=100,.incarnation=101,.bits=0x3f800000,.revision=2,.available=1,.seed=1};for(unsigned i=0;i<8;i++)fixture_motor_pass(&mini_surface,&b,&s,120+i);
 unsigned mini_after_pitch=0,mini_after_meter=0,mini_after_rings=0;for(unsigned i=0;i<9;i++)mini_after_pitch+=observed_pitch_count[i];for(unsigned i=0;i<8;i++){mini_after_meter+=observed_press_count[i];mini_after_rings+=observed_ring_count[i];need(!following.motors[i].bound,"Mini skips strip motor eligibility and due state");}
 need(mini_after_pitch==mini_pitch&&!following.motors[8].bound,"Mini emits no chooser, source-following, master or strip motor pitch output");need(observed_lcd_count==mini_lcd&&observed_position_count==mini_time&&mini_after_meter==mini_meter&&observed_color_count==mini_colors,"Mini suppresses LCD, time, meters and vendor color feedback");need(mini_after_rings>mini_rings&&observed_led[93]==127,"Mini retains production V-Pot ring and global button feedback");

 char *hui[]={(char*)"mirror-input",(char*)"/volume",(char*)"/command",(char*)"--profile=hui",(char*)"--endpoint-client=HUI Device",(char*)"--endpoint-port=DAW",(char*)"--endpoint-type=kernel",(char*)"--hui-mode=pickup"};
 need(servo_arguments(8,hui)==3&&controller.profile==&mcu_profile_hui&&controller.profile->protocol==MCU_PROTOCOL_HUI&&controller.physical_only==MCU_ENDPOINT_KERNEL&&controller_hui_policy==MCU_HUI_PICKUP&&servo_disabled,"explicit HUI profile selects one exact kernel endpoint and Pickup policy without MCU motor echo");
 need(mcu_endpoint_match(&controller,"HUI Device",1,"DAW",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE)&&!mcu_endpoint_match(&controller,"HUI Device",0,"DAW",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE)&&!mcu_endpoint_match(&controller,"Other",1,"DAW",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE),"explicit HUI endpoint remains exact by stable names, type and bidirectional capabilities");
 char *hui_any[]={(char*)"mirror-input",(char*)"/volume",(char*)"/command",(char*)"--profile=hui",(char*)"--endpoint-client=HUI Device",(char*)"--endpoint-port=DAW",(char*)"--endpoint-type=any"};need(servo_arguments(7,hui_any)<0,"new HUI selections cannot omit the exact kernel/user endpoint type");
 char *hui_user[]={(char*)"mirror-input",(char*)"/volume",(char*)"/command",(char*)"--profile=hui",(char*)"--endpoint-client=HUI Device",(char*)"--endpoint-port=DAW",(char*)"--endpoint-type=user",(char*)"--hui-mode=pickup"};need(servo_arguments(8,hui_user)==3&&controller.physical_only==MCU_ENDPOINT_USER&&mcu_endpoint_match(&controller,"HUI Device",0,"DAW",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE)&&!mcu_endpoint_match(&controller,"HUI Device",1,"DAW",MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ|MCU_ENDPOINT_WRITE),"exact user HUI endpoint remains distinct from the same kernel identity");need(servo_arguments(8,hui)==3,"restore Pickup HUI policy");
 reset_fixture(c,&in,&b,&s,8);Surface hui_surface={.source=3,.full={32,0}};snd_seq_event_t hui_event;unsigned hui_kind,hui_channel;int hui_value;
#define HUI_CC(param,data,tick) do{snd_seq_ev_clear(&hui_event);hui_event.source=hui_surface.full;snd_seq_ev_set_controller(&hui_event,0,(param),(data));}while(0)
 HUI_CC(3,0x12,100);need(!surface_hui_event(&hui_surface,&hui_event,100,&hui_kind,&hui_channel,&hui_value),"production HUI parser retains per-strip fader MSB");
 HUI_CC(0x23,0x34,101);need(surface_hui_event(&hui_surface,&hui_event,101,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==3&&hui_channel==3&&hui_value==0x934,"production HUI parser emits normalized paired fader intent");
 HUI_CC(0x0f,1,101);need(!surface_hui_event(&hui_surface,&hui_event,101,&hui_kind,&hui_channel,&hui_value),"production HUI parser retains touch zone");HUI_CC(0x2f,0x40,102);need(surface_hui_event(&hui_surface,&hui_event,102,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==1&&hui_channel==1&&hui_value,"HUI touch enters the existing strip touch intent");
 HUI_CC(0x0f,1,102);need(!surface_hui_event(&hui_surface,&hui_event,102,&hui_kind,&hui_channel,&hui_value),"production HUI parser retains select zone");HUI_CC(0x2f,0x41,103);need(surface_hui_event(&hui_surface,&hui_event,103,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==5&&hui_channel==25&&hui_value,"HUI select enters the existing channel-button intent");
 HUI_CC(0x0f,2,102);need(!surface_hui_event(&hui_surface,&hui_event,102,&hui_kind,&hui_channel,&hui_value),"production HUI parser retains switch zone");HUI_CC(0x2f,0x42,103);need(surface_hui_event(&hui_surface,&hui_event,103,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==5&&hui_channel==18&&hui_value,"HUI mute enters existing channel-button intent without MCU wire translation");
 HUI_CC(0x0f,3,103);need(!surface_hui_event(&hui_surface,&hui_event,103,&hui_kind,&hui_channel,&hui_value),"production HUI parser retains solo zone");HUI_CC(0x2f,0x43,104);need(surface_hui_event(&hui_surface,&hui_event,104,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==5&&hui_channel==11&&hui_value,"HUI solo enters the existing channel-button intent");
 HUI_CC(0x0f,4,104);need(!surface_hui_event(&hui_surface,&hui_event,104,&hui_kind,&hui_channel,&hui_value),"production HUI parser retains arm zone");HUI_CC(0x2f,0x47,105);need(surface_hui_event(&hui_surface,&hui_event,105,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==5&&hui_channel==4&&hui_value,"HUI arm enters the existing channel-button intent");
 HUI_CC(0x0f,0x0e,104);need(!surface_hui_event(&hui_surface,&hui_event,104,&hui_kind,&hui_channel,&hui_value),"production HUI parser retains transport zone");HUI_CC(0x2f,0x44,105);need(surface_hui_event(&hui_surface,&hui_event,105,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==12&&hui_channel==94&&hui_value,"HUI Play enters the existing serialized Play path");
 HUI_CC(0x0f,0x0e,106);need(!surface_hui_event(&hui_surface,&hui_event,106,&hui_kind,&hui_channel,&hui_value),"production HUI parser retains Stop zone");HUI_CC(0x2f,0x43,107);need(surface_hui_event(&hui_surface,&hui_event,107,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==12&&hui_channel==93&&hui_value,"HUI Stop retains the one existing Stop classification path");
 HUI_CC(0x0f,0x0e,107);need(!surface_hui_event(&hui_surface,&hui_event,107,&hui_kind,&hui_channel,&hui_value),"production HUI parser retains Record zone");HUI_CC(0x2f,0x45,108);need(surface_hui_event(&hui_surface,&hui_event,108,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==11&&hui_channel==95&&hui_value,"HUI Record enters the existing record-toggle intent");
 HUI_CC(0x0f,0x0a,108);need(!surface_hui_event(&hui_surface,&hui_event,108,&hui_kind,&hui_channel,&hui_value),"production HUI parser retains navigation zone");HUI_CC(0x2f,0x43,109);need(surface_hui_event(&hui_surface,&hui_event,109,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==2&&hui_channel==1&&hui_value,"HUI bank-right enters the existing bank-navigation intent");
 HUI_CC(0x40,0x43,108);need(surface_hui_event(&hui_surface,&hui_event,108,&hui_kind,&hui_channel,&hui_value)==1&&hui_kind==4&&!hui_channel&&hui_value==3,"HUI clockwise pan is normalized directly with protocol direction");hui_event.source.client++;need(!surface_hui_event(&hui_surface,&hui_event,109,&hui_kind,&hui_channel,&hui_value),"foreign HUI source is rejected");
#undef HUI_CC
 hui_pickup_sync(&hui_surface,&in,&b,100);need(hui_surface.hui_pickup[0].source_valid&&!hui_surface.hui_pickup[0].acquired&&hui_surface.hui_pickup[0].target==8192,"fresh authoritative source arms exact-crossing pickup");
 need(!hui_pickup_position(&hui_surface,0,3000)&&!hui_pickup_position(&hui_surface,0,7000),"nonmotor fader motion below target remains input-only baseline");
 unsigned hui_before_output=output_count,hui_pitch_before=0;for(unsigned i=0;i<9;i++)hui_pitch_before+=observed_pitch_count[i];need(hui_pickup_position(&hui_surface,0,9000)&&surface_pitch(&hui_surface,&in,&b,&s,0,9000,110)&&output_count==hui_before_output,"travelled segment crossing acquires HUI input without motor or raw echo");input_pump(&in,&b,&s,111);need(in.flights[0].flight&&in.flights[0].field==CF_VOLUME,"acquired HUI fader publishes through the existing native command owner");
 s.tracks[0].bits=in.flights[0].bits;s.tracks[0].revision+=2;s.heartbeat=112;bank_apply(&b,&s,112);input_sync(&in,&b);hui_pickup_sync(&hui_surface,&in,&b,112);need(hui_surface.hui_pickup[0].acquired,"own pending request source value does not rearm pickup");
 s.tracks[0].bits=0x3e800000;s.tracks[0].revision+=2;s.heartbeat=113;bank_apply(&b,&s,113);input_sync(&in,&b);hui_pickup_sync(&hui_surface,&in,&b,113);need(!hui_surface.hui_pickup[0].acquired&&hui_surface.hui_pickup[0].target==4096,"external authoritative source change rearms pickup at the new target");
 memset(observed_hui_led,0,sizeof(observed_hui_led));memset(observed_hui_fader_msb_count,0,sizeof(observed_hui_fader_msb_count));memset(observed_hui_fader_lsb_count,0,sizeof(observed_hui_fader_lsb_count));observed_hui_led_count=observed_hui_heartbeat=0;memset(hui_surface.hui_led_valid,0,sizeof(hui_surface.hui_led_valid));hui_surface.hui_transport_valid=hui_surface.hui_heartbeat_valid=0;
 need(hui_channel_output(&hui_surface,&b,&s,113)&&observed_hui_heartbeat==1&&observed_hui_led_count==35&&!observed_hui_fader_msb_count[0]&&!observed_hui_fader_lsb_count[0],"Pickup HUI output emits heartbeat and bounded LEDs but no fader feedback");unsigned hui_output_after=output_count;need(hui_channel_output(&hui_surface,&b,&s,114)&&output_count==hui_output_after,"unchanged Pickup HUI feedback and sub-second heartbeat coalesce");need(hui_channel_output(&hui_surface,&b,&s,1113)&&observed_hui_heartbeat==2&&output_count==hui_output_after+1,"standard host heartbeat repeats once per second");
 Surface liveness={.hui_connected_tick=100};need(hui_liveness_current(&liveness,3099)&&!hui_liveness_current(&liveness,3100),"HUI startup grace is bounded before any heartbeat or reply");liveness.hui_heartbeat_valid=1;liveness.hui_reply_count=1;liveness.hui_reply_tick=1000;need(hui_liveness_current(&liveness,3499)&&!hui_liveness_current(&liveness,3500),"a HUI reply moves liveness to the bounded reply timeout");liveness.hui_reply_tick=3400;liveness.hui_reply_count++;need(hui_liveness_current(&liveness,3500),"a fresh HUI heartbeat reply reacquires the liveness window");
 unsigned hui_pitch_after=0;for(unsigned i=0;i<9;i++)hui_pitch_after+=observed_pitch_count[i];need(hui_pitch_after==hui_pitch_before&&!following.motors[0].bound,"HUI profile emits no motor/raw fader output and never enters strip motor due work");
 char *hui_motor[]={(char*)"mirror-input",(char*)"/volume",(char*)"/command",(char*)"--profile=hui",(char*)"--endpoint-client=HUI Device",(char*)"--endpoint-port=DAW",(char*)"--endpoint-type=kernel",(char*)"--hui-mode=motorized"};need(servo_arguments(8,hui_motor)==3&&controller_hui_policy==MCU_HUI_MOTORIZED,"generic Motorized HUI policy selects the same standard codec path");reset_fixture(c,&in,&b,&s,8);s.heartbeat=200;bank_apply(&b,&s,200);Surface motorized={.source=3,.full={32,0}};need(hui_pickup_position(&motorized,0,1000)&&!motorized.hui_pickup[0].physical_valid,"Motorized HUI input bypasses Pickup crossing state");need(hui_channel_output(&motorized,&b,&s,200),"Motorized HUI emits fresh source feedback");for(unsigned strip=0;strip<8;strip++)need(observed_hui_fader_msb_count[strip]==1&&observed_hui_fader_lsb_count[strip]==1&&observed_hui_fader_msb[strip]==64&&!observed_hui_fader_lsb[strip],"Motorized HUI emits one standard paired 14-bit fader value per fresh strip");s.tracks[0].bits=0x3e800000;s.tracks[0].revision+=2;s.heartbeat=210;bank_apply(&b,&s,210);need(hui_channel_output(&motorized,&b,&s,210)&&observed_hui_fader_msb_count[0]==2&&observed_hui_fader_lsb_count[0]==2&&observed_hui_fader_msb[0]==32&&!observed_hui_fader_lsb[0],"Motorized HUI follows a changed fresh authoritative MPC value with another paired fader value");need(servo_arguments(8,hui)==3,"restore Pickup HUI policy after Motorized output check");
 /* Independent simulator decoder consumes the real bridge's ALSA wire. */
 for(unsigned policy=MCU_HUI_PICKUP;policy<=MCU_HUI_MOTORIZED;policy++){
  controller_hui_policy=policy;reset_fixture(c,&in,&b,&s,16);Surface rings={.source=3,.full={32,0}};mcu_feedback_init(&hui_consumer);
  s.tracks[0].fields[CF_PAN].bits=0;s.tracks[2].fields[CF_PAN].bits=0x3f800000;s.tracks[3].fields[CF_PAN].available=0;s.tracks[4].vptr=0x6932338;bank_apply(&b,&s,100);
  need(hui_channel_output(&rings,&b,&s,100)&&hui_consumer.counts[MCU_FEEDBACK_RING]==8&&hui_consumer.ring[0]==1&&hui_consumer.ring[1]==6&&hui_consumer.ring[2]==11&&!hui_consumer.ring[3]&&!hui_consumer.ring[4],"HUI consumer receives left/centre/right and unavailable/nonmixable off in either policy");
  unsigned ring_count=hui_consumer.counts[MCU_FEEDBACK_RING];need(hui_channel_output(&rings,&b,&s,101)&&hui_consumer.counts[MCU_FEEDBACK_RING]==ring_count,"unchanged HUI rings do not resend");
  s.tracks[1].fields[CF_PAN].bits=0x3f400000;s.tracks[1].fields[CF_PAN].revision+=2;bank_apply(&b,&s,102);
  need(hui_channel_output(&rings,&b,&s,102)&&hui_consumer.counts[MCU_FEEDBACK_RING]==++ring_count&&hui_consumer.ring[1]==9,"one authoritative pan change emits one independently decoded HUI ring");
  s.tracks[1].fields[CF_PAN].revision+=2;bank_apply(&b,&s,103);need(hui_channel_output(&rings,&b,&s,103)&&hui_consumer.counts[MCU_FEEDBACK_RING]==ring_count,"same quantized position with a new revision is silent");
  s.tracks[1].fields[CF_PAN].incarnation++;bank_apply(&b,&s,104);need(hui_channel_output(&rings,&b,&s,104)&&hui_consumer.counts[MCU_FEEDBACK_RING]==++ring_count,"pan property rebind reasserts the unchanged ring");
  bank_navigate(&b,1,105);bank_apply(&b,&s,106);need(hui_channel_output(&rings,&b,&s,106)&&hui_consumer.counts[MCU_FEEDBACK_RING]==ring_count+8,"new bank reasserts all bound HUI rings including unchanged centres");ring_count+=8;for(unsigned strip=0;strip<8;strip++)need(hui_consumer.ring[strip]==6,"new bank consumer receives centre for every strip");
  b.ready=0;need(hui_channel_output(&rings,&b,&s,107)&&hui_consumer.counts[MCU_FEEDBACK_RING]==ring_count+8,"unavailable source clears all HUI rings");ring_count+=8;for(unsigned strip=0;strip<8;strip++)need(!hui_consumer.ring[strip],"unavailable ring reaches the consumer as off");
  need(hui_channel_output(&rings,&b,&s,108)&&hui_consumer.counts[MCU_FEEDBACK_RING]==ring_count,"continued unavailable source does not resend off");bank_apply(&b,&s,109);need(hui_channel_output(&rings,&b,&s,109)&&hui_consumer.counts[MCU_FEEDBACK_RING]==ring_count+8,"fresh source reacquisition restores all HUI rings");ring_count+=8;
  surface_close(&rings,&b);bank_apply(&b,&s,110);need(hui_channel_output(&rings,&b,&s,110)&&hui_consumer.counts[MCU_FEEDBACK_RING]==ring_count+8,"production disconnect and fresh rebind resend all ring feedback");
 }
 need(servo_arguments(8,hui)==3,"restore Pickup policy after ring consumer checks");

 Surface disconnected={.full={32,0}};MirrorBank disconnected_bank={0};snd_seq_ev_clear(&hui_event);hui_event.source=disconnected.full;snd_seq_ev_set_controller(&hui_event,0,0,1);need(!surface_hui_event(&disconnected,&hui_event,120,&hui_kind,&hui_channel,&hui_value)&&disconnected.hui_input.fader_valid[0],"disconnect fixture holds a partial HUI fader");snd_seq_ev_set_controller(&hui_event,0,0x0f,1);need(!surface_hui_event(&disconnected,&hui_event,121,&hui_kind,&hui_channel,&hui_value)&&disconnected.hui_input.zone_valid,"disconnect fixture holds a partial HUI switch zone");disconnected.hui_pickup[0].source_valid=disconnected.hui_pickup[0].physical_valid=disconnected.hui_pickup[0].acquired=1;surface_close(&disconnected,&disconnected_bank);need(!disconnected.hui_input.fader_valid[0]&&!disconnected.hui_input.zone_valid&&!disconnected.hui_pickup[0].source_valid&&!disconnected.hui_pickup[0].physical_valid&&!disconnected.hui_pickup[0].acquired,"production disconnect clears partial HUI wire and transient pickup state");
 char hui_directory[]="/tmp/mpclearn-hui.XXXXXX",hui_path[256],hui_option_text[300];need(mkdtemp(hui_directory)!=NULL,"private HUI preference fixture directory");snprintf(hui_path,sizeof(hui_path),"%s/controller-preferences",hui_directory);snprintf(hui_option_text,sizeof(hui_option_text),"--controller-preferences=%s",hui_path);
 char *set_hui[]={(char*)"mirror-input",(char*)"--controller-set",hui_path,(char*)"--profile=hui",(char*)"--endpoint-client=HUI Saved",(char*)"--endpoint-port=DAW Port",(char*)"--endpoint-type=kernel",(char*)"--hui-mode=motorized"},*saved_hui[]={(char*)"mirror-input",(char*)"/volume",(char*)"/command",hui_option_text};
 need(input_bridge_main_not_called(8,set_hui)==0&&servo_arguments(4,saved_hui)==3&&controller.profile==&mcu_profile_hui&&controller.physical_only==MCU_ENDPOINT_KERNEL&&controller_hui_policy==MCU_HUI_MOTORIZED&&!strcmp(controller.client,"HUI Saved")&&!strcmp(controller.port,"DAW Port"),"session preference owner explicitly persists exact HUI profile, type, endpoint and Motorized policy");
 McuControllerPreferenceFile legacy={.magic="MCUCFG1",.profile=4,.physical_only=MCU_ENDPOINT_KERNEL,.client_length=9,.port_length=8};memcpy(legacy.client,"HUI Saved",9);memcpy(legacy.port,"DAW Port",8);legacy.checksum=mcu_controller_checksum(&legacy);
 int legacy_fd=open(hui_path,O_WRONLY|O_TRUNC);need(legacy_fd>=0&&write(legacy_fd,&legacy,sizeof(legacy))==(ssize_t)sizeof(legacy)&&!close(legacy_fd),"write old MCUCFG1 HUI compatibility fixture");
 need(servo_arguments(4,saved_hui)==3&&controller_hui_policy==MCU_HUI_PICKUP&&!stat(hui_path,&controller_stat)&&controller_stat.st_size==(off_t)sizeof(McuControllerPreferenceFile),"old MCUCFG1 imports as Pickup without rewriting bytes during read");
 McuControllerCollection migrated;need(mcu_controller_preferences_read_collection(hui_path,&migrated)&&migrated.count==1&&migrated.imported_legacy&&!strcmp(migrated.records[0].input.port,"DAW Port")&&!strcmp(migrated.records[0].output.port,"DAW Port"),"legacy singleton migrates in memory to one same-port input/output record");
 char *set_hui_pickup[]={(char*)"mirror-input",(char*)"--controller-set",hui_path,(char*)"--profile=hui",(char*)"--input-client=HUI Saved",(char*)"--input-port=DAW In",(char*)"--input-type=user",(char*)"--output-client=HUI Saved",(char*)"--output-port=DAW Out",(char*)"--output-type=kernel",(char*)"--hui-mode=pickup"};need(input_bridge_main_not_called(11,set_hui_pickup)==0&&servo_arguments(4,saved_hui)==3&&controller.physical_only==MCU_ENDPOINT_USER&&!strcmp(controller.port,"DAW In")&&controller_output.physical_only==MCU_ENDPOINT_KERNEL&&!strcmp(controller_output.port,"DAW Out")&&controller_hui_policy==MCU_HUI_PICKUP&&!stat(hui_path,&controller_stat)&&controller_stat.st_size==(off_t)sizeof(McuControllerCollectionFile),"first Apply replaces imported MCUCFG1 with split MCUCFG2 and explicit Pickup policy");need(!unlink(hui_path)&&!rmdir(hui_directory),"HUI preference fixture cleanup");

 static const McuProfile strips_only={.name="strips-only",.capabilities=MCU_CAP_STRIP_FADER,.raw_fader_echo=1,.master_input_max=16383};controller=(McuEndpoint){.profile=&strips_only,.client="Fixture",.port="Port",.physical_only=1};
 snd_seq_addr_t address={32,0};snd_seq_event_t event;snd_seq_ev_clear(&event);event.source=address;event.type=SND_SEQ_EVENT_PITCHBEND;event.data.control.channel=8;event.data.control.value=0;
 need(!surface_event(&event,address,&kind,&channel,&value),"profile without master ignores master pitch input");
 event.data.control.channel=0;need(surface_event(&event,address,&kind,&channel,&value)==1&&kind==3,"eight logical strip pitch inputs remain available without optional controls");
 snd_seq_ev_clear(&event);event.source=address;snd_seq_ev_set_controller(&event,0,60,1);need(!surface_event(&event,address,&kind,&channel,&value),"profile without jog ignores jog input");
 snd_seq_ev_clear(&event);event.source=address;snd_seq_ev_set_noteon(&event,0,94,127);need(!surface_event(&event,address,&kind,&channel,&value),"profile without globals ignores transport input");
 snd_seq_ev_clear(&event);event.source=address;snd_seq_ev_set_noteon(&event,0,40,127);need(!surface_event(&event,address,&kind,&channel,&value),"profile without globals ignores assignment input");
 unsigned lcd=observed_lcd_count,position=observed_position_count,limited_pressure=0,master=observed_pitch_count[8];for(unsigned i=0;i<8;i++)limited_pressure+=observed_press_count[i];
 Surface limited={.source=3,.full=address};for(unsigned i=0;i<8;i++)need(channel_output(&limited,&b,&s,100),"strips-only capability output pass");unsigned limited_after_pressure=0;for(unsigned i=0;i<8;i++)limited_after_pressure+=observed_press_count[i];
 need(observed_lcd_count==lcd&&observed_position_count==position&&limited_after_pressure==limited_pressure&&observed_color_count==colors&&observed_pitch_count[8]==master,"absent LCD/time/meters/color/master capabilities emit none of those outputs");
 need(servo_arguments(3,restore)==3,"restore default profile after capability checks");free(c);
 puts("PASS MCU/HUI profiles, split endpoint roles/reconnect, fixed sixteen-record MCUCFG2 plus MCUCFG1 migration, and bounded typed inventory with synthetic enumeration/captured ALSA sink: exact identities, stale/absent/ambiguous refusal, standard HUI paired input, Pickup crossing, Motorized feedback, bounded heartbeat liveness, unchanged X-Touch/generic/Mini behavior, and optional capability gates");
}
/* The musical snapshot/native method remain substituted. These assertions
 * inspect production handlers' request identities and emitted MCU bytes. */
static void send_returns_fixture(CopiedMirror *s){
 for(unsigned i=0;i<4;i++){
  CopiedTrack *t=s->tracks+8+i;t->vptr=0x6931f70;
  snprintf(t->fields[CF_NAME].text,CHANNEL_TEXT,"Return %u",i+1);t->fields[CF_NAME].length=8;
  s->send_programs[i]=t->program;s->send_owners[i]=t->program_owner;
 }
}
static void display_pass(Surface *surface,MirrorBank *bank,CopiedMirror *s,unsigned now){
 for(unsigned i=0;i<8;i++)need(channel_output(surface,bank,s,now),"paired display production output");
}
static void assignment_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"assignment fixture");MirrorInput in;MirrorBank b;MIRROR_COPY(s);Surface surface={.source=3,.full={32,0}};CommandRequest r;
 reset_fixture(c,&in,&b,&s,12);s.selection.bits=s.tracks[2].track;s.selected_serial=3;memcpy(s.tracks[2].fields[CF_NAME].text,"Drum 003",8);s.tracks[2].fields[CF_NAME].length=8;bank_apply(&b,&s,101);
 surface_assignment(&surface,&in,&b,&s,40,102);bank_apply(&b,&s,103);
 for(unsigned i=0;i<8;i++)need(channel_output(&surface,&b,&s,s.heartbeat),"Track display output");
 need(observed_led[40]==127&&!observed_led[41]&&!observed_led[42],"Track assignment has exactly one assignment LED");
 need(!memcmp(observed_lcd+14,"Dru003 ",7)&&!memcmp(observed_lcd+56,"-6.04  ",7),"Track LCD shows strip name and volume, without a mixed selected-track page");
 surface_encoder(&surface,&in,&b,&s,0,8,0,104);surface_encoder(&surface,&in,&b,&s,4,8,0,104);surface_encoder(&surface,&in,&b,&s,6,8,0,104);input_pump(&in,&b,&s,105);
 need(atomic_load(&c->published)==3,"Track encoders address three independent channel pans");
 unsigned channel_mask=0;for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in.flights[i].flight){need(in.flights[i].field==CF_PAN,"Track never assigns a Send");channel_mask|=1u<<(in.flights[i].target.serial-1);}need(channel_mask==0x51,"Track encoders follow strips1,5,7 regardless of selection");
 for(unsigned i=0;i<8;i++)need(channel_output(&surface,&b,&s,s.heartbeat),"Track display after encoder input");
 need(!memcmp(observed_lcd+56,"-6.04  ",7)&&!memcmp(observed_lcd+84,"-6.04  ",7)&&!memcmp(observed_lcd+98,"-6.04  ",7)&&!memcmp(observed_lcd+14,"Dru003 ",7),"normal Track keeps channel names and volume after pan encoder moves");
 surface_assignment(&surface,&in,&b,&s,42,106);bank_apply(&b,&s,107);need(input_pending(&in)==3,"assignment change preserves published source settlement");
 for(unsigned i=0;i<8;i++){channel_output(&surface,&b,&s,s.heartbeat);}
 need(!observed_led[40]&&!observed_led[41]&&observed_led[42]==127&&observed_lcd[56]=='P',"Pan clears Track/Send LEDs and displays channel pan separately");
 reset_fixture(c,&in,&b,&s,12);s.selection.bits=s.tracks[2].track;bank_apply(&b,&s,101);surface_assignment(&surface,&in,&b,&s,41,102);bank_apply(&b,&s,103);
 need(b.flip&&bank_field(&b,1)==CF_SEND2,"Send enters existing flipped send-fader assignment");surface_pitch(&surface,&in,&b,&s,1,12000,104);input_pump(&in,&b,&s,105);need(command_request_read(c->slots,1,&r)&&r.serial==3&&r.reserved==CF_SEND2,"default Send fader2 changes selected Track Send2");
 surface_assignment(&surface,&in,&b,&s,50,106);bank_apply(&b,&s,107);surface_encoder(&surface,&in,&b,&s,0,10,0,108);input_pump(&in,&b,&s,109);need(!b.flip&&command_request_read(c->slots+1,2,&r)&&r.serial==3&&r.reserved==CF_SEND1,"Flip swaps Sends back to selected-track encoders");
 surface_assignment(&surface,&in,&b,&s,41,110);bank_apply(&b,&s,111);
 const uint32_t levels[]={0,0x3f800000,0x3f000000,0x3e800000};const char *percent[]={"S1  0% ","S2100% ","S3 50% ","S4 25% "};
 for(unsigned i=0;i<4;i++){CopiedTrack *destination=s.tracks+8+i;destination->vptr=0x6931f70;destination->bits=0x3f800000;snprintf(destination->fields[CF_NAME].text,CHANNEL_TEXT,"Return %u",i+1);destination->fields[CF_NAME].length=8;s.send_programs[i]=destination->program;s.send_owners[i]=destination->program_owner;s.tracks[2].fields[CF_SEND1+i].bits=levels[i];}
 bank_apply(&b,&s,111);for(unsigned i=0;i<8;i++){channel_output(&surface,&b,&s,s.heartbeat);}
 need(!observed_led[40]&&observed_led[41]==127&&!observed_led[42],"Send clears Track and Pan LEDs");
 for(unsigned i=0;i<4;i++){char label[8];snprintf(label,sizeof(label),"R%u+6.0 ",i+1);need(!memcmp(observed_lcd+7*i,label,7)&&!memcmp(observed_lcd+56+7*i,percent[i],7),"Send LCD shows actual return dB above selected sends percent");}
 in.jog_shift=1;surface_assignment(&surface,&in,&b,&s,66,112);in.jog_shift=0;bank_apply(&b,&s,113);need(b.view==BV_RETURN&&b.assignment==BA_TRACK&&!b.flip,"Aux clears Send overlay and selects ordinary return mixer");for(unsigned i=0;i<8;i++)need(channel_output(&surface,&b,&s,s.heartbeat),"Aux return mixer output");need(!memcmp(observed_lcd,"Retrn1 ",7)&&bank_field(&b,0)==CF_VOLUME&&b.faders[0].identity.serial==9,"Aux shows real Return name and return volume target");
 surface_assignment(&surface,&in,&b,&s,41,114);bank_apply(&b,&s,115);need(b.assignment==BA_SEND&&b.flip&&bank_field(&b,0)==CF_SEND1&&b.faders[0].identity.serial==3,"Send restores default selected-track send faders after Aux");
 for(unsigned mode=0;mode<2;mode++){
  reset_fixture(c,&in,&b,&s,12);if(!mode)send_returns_fixture(&s);s.selection.bits=s.tracks[2].track;bank_apply(&b,&s,101);surface_assignment(&surface,&in,&b,&s,mode?40:41,102);if(mode)surface_assignment(&surface,&in,&b,&s,50,103);bank_apply(&b,&s,104);
  surface_pitch(&surface,&in,&b,&s,0,12000,105);surface_encoder(&surface,&in,&b,&s,0,5,0,105);input_pump(&in,&b,&s,106);
  unsigned target_field=mode?CF_PAN:CF_SEND1,saw_volume=0,saw_parameter=0;
  for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in.flights[i].flight){InputFlight *f=in.flights+i;saw_volume+=f->field==CF_VOLUME&&f->target.serial==(mode?1u:9u);saw_parameter+=f->field==target_field&&f->target.serial==(mode?1u:3u);}
  need(saw_volume==1&&saw_parameter==1,"Flip binds exact return volume in Send and ordinary strip volume in Track");
  need(b.faders[0].identity.serial==(mode?1u:3u)&&bank_field(&b,0)==target_field,"Flip motor follows actual selected parameter identity");
  input_touch(&in,&b,&s,1,1,107);s.selection.bits=s.tracks[3].track;bank_apply(&b,&s,108);input_sync(&in,&b);input_pitch(&in,&b,1,13000,109);
  need(mode?in.desires[1][CF_PAN].pending:!in.desires[1][bank_field(&b,1)].pending,"selection cancels selected Send Flip but preserves channel Track Flip");
 }
 reset_fixture(c,&in,&b,&s,12);s.selection.bits=s.tracks[6].track;bank_apply(&b,&s,101);
 surface_navigation(&in,&b,&s,3,102);surface_navigation(&in,&b,&s,3,103);input_pump(&in,&b,&s,104);
 need(command_request_read(c->slots,1,&r)&&r.serial==9&&r.reserved==CF_SELECTION&&b.offset==0,"two Channel+ presses select Track9 through command, without optimistic bank movement");
 complete(c,1,105);s.selection.bits=s.tracks[8].track;s.selected_serial=9;s.selection.revision=4;s.heartbeat=106;bank_apply(&b,&s,106);input_pump(&in,&b,&s,106);
 need(in.settled==1&&b.offset==8&&b.faders[0].identity.serial==9,"actual Track9 selection switches to fixed bank2 fader1");
 reclaim_fixture(c,1);input_pump(&in,&b,&s,107);surface_navigation(&in,&b,&s,0,108);bank_apply(&b,&s,109);need(b.offset==0&&atomic_load(&c->published)==1,"Bank- changes visible strips but never selection command");
 bank_apply(&b,&s,110);need(b.offset==0,"unchanged selection does not undo explicit Bank navigation");
 surface_navigation(&in,&b,&s,2,111);input_pump(&in,&b,&s,112);need(command_request_read(c->slots,2,&r)&&r.serial==8,"Channel- navigates from actual selection, not bank offset");
 reset_fixture(c,&in,&b,&s,20);send_returns_fixture(&s);s.selection.bits=s.tracks[2].track;bank_apply(&b,&s,101);surface_assignment(&surface,&in,&b,&s,41,102);surface_assignment(&surface,&in,&b,&s,50,102);bank_apply(&b,&s,103);
 surface_encoder(&surface,&in,&b,&s,0,3,0,104);surface_navigation(&in,&b,&s,3,104);surface_pitch(&surface,&in,&b,&s,0,12000,104);
 surface_navigation(&in,&b,&s,1,105);bank_apply(&b,&s,106);input_pump(&in,&b,&s,106);
 unsigned kept_parameter=0,kept_selection=0;for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in.flights[i].flight){InputFlight *f=in.flights+i;kept_parameter+=f->field==CF_SEND1&&f->target.serial==3;kept_selection+=f->field==CF_SELECTION&&f->target.serial==4;}
 need(atomic_load(&c->published)==3&&kept_parameter==1&&kept_selection==1&&b.offset==0,"Send pairs ignore Bank paging and preserve exact return/send/Channel requests");
 reset_fixture(c,&in,&b,&s,20);s.selection.bits=s.tracks[2].track;bank_apply(&b,&s,101);surface_assignment(&surface,&in,&b,&s,41,102);bank_apply(&b,&s,104);
 input_touch(&in,&b,&s,0,1,105);surface_pitch(&surface,&in,&b,&s,0,12000,106);surface_navigation(&in,&b,&s,1,107);bank_apply(&b,&s,108);input_pump(&in,&b,&s,108);
 need(in.gestures[0].held&&in.flights[0].field==CF_SEND1&&in.flights[0].target.serial==3,"same-drain Bank move preserves held selected Send Flip fader and its request");
 surface_pitch(&surface,&in,&b,&s,0,13000,109);need(in.desires[0][CF_SEND1].pending&&in.desires[0][CF_SEND1].identity.serial==3,"held selected Flip fader continues after Bank without re-touch");
 reset_fixture(c,&in,&b,&s,20);s.selection.bits=s.tracks[2].track;bank_apply(&b,&s,101);
 surface_navigation(&in,&b,&s,1,102);bank_apply(&b,&s,103);need(b.offset==8,"explicit Bank+ can leave selected Track offscreen");
 s.tracks[2].program+=0x20000;s.tracks[2].binding++;s.tracks[2].incarnation++;s.tracks[2].program_owner++;
 bank_apply(&b,&s,104);need(b.offset==8&&b.selected.program==s.tracks[2].program&&b.selected.binding==s.tracks[2].binding,"same selected Track Program replacement refreshes identity without bank follow");
 s.selection.bits=s.tracks[3].track;bank_apply(&b,&s,105);need(b.offset==0&&b.selected.track==s.tracks[3].track,"Track4 returns to bank1 without sliding strips");
 for(unsigned cancel=0;cancel<4;cancel++){
  reset_fixture(c,&in,&b,&s,12);s.selection.bits=s.tracks[2].track;bank_apply(&b,&s,101);surface_assignment(&surface,&in,&b,&s,41,102);surface_assignment(&surface,&in,&b,&s,50,102);bank_apply(&b,&s,103);surface_encoder(&surface,&in,&b,&s,0,3,0,104);
  if(cancel==0){s.selection.bits=s.tracks[3].track;}else if(cancel==1)s.epoch++;else if(cancel==2)s.tracks[2].binding++;else{input_discard(&in);bank_disconnect(&b);}
  bank_apply(&b,&s,105);input_sync(&in,&b);input_pump(&in,&b,&s,106);need(!atomic_load(&c->published),"selected subview cancels unsent work on selection/reload/binding/disconnect");
 }
 /* Exercise the actual paired handlers and emitted two-row LCD. Destination
  * ordinals deliberately differ from copied Return Track ordering. */
 reset_fixture(c,&in,&b,&s,12);send_returns_fixture(&s);surface=(Surface){.source=3,.full={32,0}};
 s.selection.bits=s.tracks[2].track;
 uint32_t program=s.send_programs[0],owner=s.send_owners[0];s.send_programs[0]=s.send_programs[3];s.send_owners[0]=s.send_owners[3];s.send_programs[3]=program;s.send_owners[3]=owner;
 surface_assignment(&surface,&in,&b,&s,41,101);bank_apply(&b,&s,102);
 need(b.strips[0].serial==12&&b.strips[3].serial==9&&b.faders[0].identity.serial==3,"pair returns join native destination identities rather than Return order");
 surface_encoder(&surface,&in,&b,&s,0,4,0,103);surface_pitch(&surface,&in,&b,&s,0,10000,103);input_pump(&in,&b,&s,104);
 unsigned returns=0,sends=0;
 for(unsigned i=0;i<COMMAND_SLOTS;i++)if(in.flights[i].flight){InputFlight *f=in.flights+i;returns+=f->field==CF_VOLUME&&f->target.serial==12;sends+=f->field==CF_SEND1&&f->target.serial==3;need(f->field!=CF_VOLUME||f->target.serial==12,"paired Send never edits ordinary Track volume");}
 need(returns==1&&sends==1,"positive independent native request identities for return knob and send fader");
 surface_encoder(&surface,&in,&b,&s,0,0,1,104);float unity=0.707945764f;uint32_t unity_bits;memcpy(&unity_bits,&unity,4);need(in.desires[0][CF_VOLUME].bits==unity_bits&&in.desires[0][CF_VOLUME].identity.serial==12,"Send return encoder push requests qualified 0 dB unity, not silence");
 display_pass(&surface,&b,&s,104);need(!memcmp(observed_lcd,"R1-6.0 ",7)&&!memcmp(observed_lcd+56,"S1  0% ",7),"both current source values remain visible after mixed input, without optimistic values");
 surface_general(&surface,&in,&s,52,2200);display_pass(&surface,&b,&s,2200);
 need(!memcmp(observed_lcd,"Rtn1   ",7)&&!memcmp(observed_lcd+56,"Send1  ",7),"NameValue changes contents to names without swapping physical rows");
 surface_encoder(&surface,&in,&b,&s,0,1,0,2201);display_pass(&surface,&b,&s,2201);need(!memcmp(observed_lcd,"R1-6.0 ",7)&&!memcmp(observed_lcd+56,"S1  0% ",7),"encoder edit exposes both actual values during Name mode");
 display_pass(&surface,&b,&s,4202);need(!memcmp(observed_lcd,"Rtn1   ",7),"Name mode returns after two seconds without task sleeps");
 surface_assignment(&surface,&in,&b,&s,50,4203);bank_apply(&b,&s,4204);display_pass(&surface,&b,&s,4204);
 need(!memcmp(observed_lcd,"Send1  ",7)&&!memcmp(observed_lcd+56,"Rtn1   ",7)&&b.faders[0].identity.serial==12,"Flip swaps physical identities and named rows together");
 surface_pitch(&surface,&in,&b,&s,0,12000,4205);surface_encoder(&surface,&in,&b,&s,0,4,0,4205);
 need(in.desires[0][CF_VOLUME].identity.serial==12&&in.desires[0][CF_SEND1].identity.serial==3,"flipped return fader and send encoder retain exact pair");
 display_pass(&surface,&b,&s,4205);need(!memcmp(observed_lcd,"S1  0% ",7)&&!memcmp(observed_lcd+56,"R1-6.0 ",7),"flipped edits show source values in physical rows");
 for(unsigned i=4;i<8;i++){surface_pitch(&surface,&in,&b,&s,i,12000,4206);surface_encoder(&surface,&in,&b,&s,i,10,0,4206);need(b.faders[i].empty&&bank_field(&b,i)==CF_COUNT&&bank_encoder_field(&b,i)==CF_COUNT,"unused paired strips are empty and have no writable field");for(unsigned f=0;f<CF_COUNT;f++)need(!in.desires[i][f].pending,"unused strips never create hidden desires");}
 input_discard(&in);for(unsigned i=0;i<8;i++)for(unsigned f=CF_MUTE;f<=CF_SELECTION;f++)if(f==CF_MUTE||f==CF_SOLO||f==CF_ARM||f==CF_SELECTION)input_control(&in,&b,&s,i,f,0,1,4207);
 need(!in.edge_count&&!in.desires[0][CF_SELECTION].pending,"Send strip mute/solo/arm/select are disabled");s.heartbeat=4207;bank_apply(&b,&s,4207);display_pass(&surface,&b,&s,4207);for(unsigned i=0;i<24;i++)need(!observed_led[i],"Send has no misleading bank-track arm/solo/mute LEDs");for(unsigned i=0;i<8;i++)need(observed_led[24+i]==(i==2?127u:0u),"Send Select feedback identifies source Track3, not paired Return or send slot");
 surface_pitch(&surface,&in,&b,&s,0,13000,4208);s.send_programs[0]=s.tracks[8].program;s.send_owners[0]=s.tracks[8].program_owner;bank_apply(&b,&s,4209);input_sync(&in,&b);
 need(!in.desires[0][CF_VOLUME].pending&&b.faders[0].identity.serial==9,"return membership replacement cancels unsent volume despite identical field");
 s.send_programs[0]=s.send_owners[0]=0;bank_apply(&b,&s,4210);surface_pitch(&surface,&in,&b,&s,0,14000,4211);surface_encoder(&surface,&in,&b,&s,0,2,0,4211);
 need(!in.desires[0][CF_VOLUME].pending&&in.desires[0][CF_SEND1].pending&&!b.faders[0].available&&!b.faders[0].empty,"missing return disables only its side with no generic volume fallback");
 display_pass(&surface,&b,&s,4211);need(!memcmp(observed_lcd+56,"R1 --- ",7),"missing return value is explicitly unavailable");
 s.selection.bits=s.tracks[3].track;bank_apply(&b,&s,4212);input_sync(&in,&b);need(!in.desires[0][CF_SEND1].pending,"source selection change cancels old send intent");
 need(!memcmp(observed_lcd+56,"R1 --- ",7),"source navigation does not manufacture missing return feedback");
 surface_navigation(&in,&b,&s,3,4213);need(in.desires[0][CF_SELECTION].identity.serial==5,"Send Channel plus follows playable source tracks, never Return order");
 input_discard(&in);s.selection.available=0;s.send_programs[0]=s.tracks[8].program;s.send_owners[0]=s.tracks[8].program_owner;bank_apply(&b,&s,4214);
 surface_pitch(&surface,&in,&b,&s,0,11000,4214);surface_encoder(&surface,&in,&b,&s,0,8,0,4214);
 need(in.desires[0][CF_VOLUME].pending&&!in.desires[0][CF_SEND1].pending,"missing selected source disables only send while corresponding return remains usable");
 s.ready=0;bank_apply(&b,&s,4214);input_sync(&in,&b);surface_encoder(&surface,&in,&b,&s,0,8,0,4215);need(!in.desires[0][CF_SEND1].pending,"unready source admits no Send edit");
 s.ready=1;s.tracks[0].vptr=0x692fec4;surface_assignment(&surface,&in,&b,&s,64,4216);bank_apply(&b,&s,4217);
 need(b.view==BV_AUDIO&&b.assignment==BA_TRACK&&!b.flip&&b.strips[0].serial==1&&b.faders[0].identity.serial==1&&bank_field(&b,0)==CF_VOLUME,"normal Audio view exits Send pairs and restores actual audio strip binding");
 surface_pitch(&surface,&in,&b,&s,0,12000,4218);need(in.desires[0][CF_VOLUME].pending&&in.desires[0][CF_VOLUME].identity.serial==1,"Audio after Send controls its actual Track volume");
 /* Actual Aux Select -> native selection receipt -> Send recovery. */
 for(unsigned direction=0;direction<2;direction++){
  reset_fixture(c,&in,&b,&s,12);send_returns_fixture(&s);surface=(Surface){.source=3,.full={32,0}};s.selection.bits=s.tracks[2].track;
  in.jog_shift=1;surface_assignment(&surface,&in,&b,&s,66,101);in.jog_shift=0;bank_apply(&b,&s,102);input_control(&in,&b,&s,0,CF_SELECTION,0,1,103);input_pump(&in,&b,&s,104);
  need(command_request_read(c->slots,1,&r)&&r.serial==9&&r.reserved==CF_SELECTION,"Aux Select requests actual Return Track selection");
  complete(c,1,105);s.selection.bits=s.tracks[8].track;s.selected_serial=9;s.selection.revision=4;s.heartbeat=106;bank_apply(&b,&s,106);input_pump(&in,&b,&s,106);need(in.settled==1,"Aux Return selection uses normal source settlement");reclaim_fixture(c,1);input_pump(&in,&b,&s,107);
  s.tracks[8].fields[CF_SEND1].bits=0x3f800000;surface_assignment(&surface,&in,&b,&s,41,108);bank_apply(&b,&s,109);
  need(!b.selected.serial&&!b.faders[0].available&&b.strips[0].serial==9,"selected Return cannot supply Send source, independent return binding retained");
  surface_pitch(&surface,&in,&b,&s,0,12000,110);surface_encoder(&surface,&in,&b,&s,0,4,0,110);
  need(!in.desires[0][CF_SEND1].pending&&in.desires[0][CF_VOLUME].pending&&in.desires[0][CF_VOLUME].identity.serial==9,"Return source sends are unwritable while Return knob remains usable");
  display_pass(&surface,&b,&s,110);need(!memcmp(observed_lcd,"R1-6.0 ",7)&&!memcmp(observed_lcd+56,"S1 --- ",7)&&!memcmp(observed_lcd+105,"Ch +/- ",7),"Return own full-scale send never displayed as selected send; source navigation hint shown");for(unsigned i=0;i<8;i++)need(!observed_led[24+i],"ineligible Return source clears every Send Select LED");
  surface_assignment(&surface,&in,&b,&s,50,111);bank_apply(&b,&s,112);surface_encoder(&surface,&in,&b,&s,0,4,0,113);need(!in.desires[0][CF_SEND1].pending&&b.faders[0].identity.serial==9,"Flip cannot expose selected Return send encoder");
  surface_assignment(&surface,&in,&b,&s,41,114);bank_apply(&b,&s,115);surface_navigation(&in,&b,&s,direction?3:2,116);input_pump(&in,&b,&s,117);
  unsigned target=direction?1:8;need(command_request_read(c->slots,2,&r)&&r.serial==target&&r.reserved==CF_SELECTION,"Channel from Return selects first/last playable through existing command");need(!b.selected.serial&&!b.faders[0].available,"recovery request does not optimistically bind sends");
  complete(c,2,118);s.selection.bits=s.tracks[target-1].track;s.selected_serial=target;s.selection.revision=6;s.heartbeat=119;bank_apply(&b,&s,119);input_pump(&in,&b,&s,119);
  need(in.settled==2&&b.selected.serial==target&&b.faders[0].identity.serial==target&&b.faders[0].available,"actual recovered selection restores authoritative sends after settlement");
  display_pass(&surface,&b,&s,119);for(unsigned i=0;i<8;i++)need(observed_led[24+i]==(i==target-1?127u:0u),"Send Select LED follows actual Channel source recovery");
  bank_drop(&b,120);need(channel_output(&surface,&b,&s,120),"unready Send clears selected-source feedback");for(unsigned i=0;i<8;i++)need(!observed_led[24+i],"unready Send clears all Select LEDs immediately");

  s.tracks[0].vptr=0x6932150;s.selection.bits=s.tracks[0].track;bank_apply(&b,&s,120);need(b.selected.serial==1&&b.faders[0].available,"supported non-Return Submix send source remains qualified");
  s.selection.bits=0;bank_apply(&b,&s,121);input_discard(&in);surface_navigation(&in,&b,&s,direction?3:2,122);need(in.desires[0][CF_SELECTION].pending&&in.desires[0][CF_SELECTION].identity.serial==(direction?2u:8u),"no selected source recovers to first/last current playable Track");
 }
 free(c);puts("PASS production assignment request targets/MCU LEDs/LCD, channel Track/selected Send Flip, real Channel selection settlement and fixed bank paging/cancellation (native snapshot/call and ALSA output substituted)");
}
/* Raw MCU input replay. The X-Touch Scrub mode is a Surface field, so the
 * integration helper carries it beside the decoded event. */
static unsigned raw_data_wheel;
static void raw_input(snd_midi_event_t *codec,const unsigned char bytes[3],snd_seq_addr_t address,MirrorInput *in,MirrorBank *bank,CopiedMirror *s,uint32_t now){
 snd_seq_event_t e;need(snd_midi_event_encode(codec,bytes,3,&e)==3,"actual ALSA raw input codec");e.source=address;unsigned kind,channel;int value;
 need(surface_event(&e,address,&kind,&channel,&value)==1,"physical MCU event classifier");
 if(kind==1)input_touch(in,bank,s,channel,value,now);else if(kind==3){
  Surface surface={.source=3,.full=address};unsigned before=output_count,published=atomic_load(&in->commands->published),settled=atomic_load(&in->commands->slots[0].settled);
  need(surface_pitch(&surface,in,bank,s,channel,value,now)&&output_count==before,"default production pitch handler emits no raw echo");
  need(atomic_load(&in->commands->published)==published&&atomic_load(&in->commands->slots[0].settled)==settled,"servo output is neither request publication nor source settlement");
 }else if(kind==12)input_stop_button(in,s,channel,value,now);else if(kind==7)input_master_touch(in,s,value);else if(kind==10)input_master_pitch(in,s,value,now);else if(kind==8||kind==9)surface_jog(in,s,kind,channel,value,now,raw_data_wheel);else need(0,"unexpected integration MIDI kind");
}
static void toggle_midi_checks(void){
 CommandState *commands=calloc(1,sizeof(*commands));need(commands!=NULL,"toggle command fixture");MirrorInput in;MirrorBank bank;MIRROR_COPY(snapshot);
 reset_fixture(commands,&in,&bank,&snapshot,8);snapshot.record_mode=(CopiedField){.available=1,.owner_incarnation=100,.incarnation=101};snapshot.click=(CopiedField){.available=1,.owner_incarnation=102,.incarnation=103};
 Surface surface={.source=3,.full={32,0}};snd_midi_event_t *codec=NULL;need(!snd_midi_event_new(16,&codec),"toggle raw codec");
 const unsigned notes[]={95,89,86},ops[]={GLOBAL_RECORD_TOGGLE,GLOBAL_CLICK_TOGGLE,GLOBAL_LOOP_TOGGLE};
 for(unsigned i=0;i<3;i++)for(unsigned edge=0;edge<5;edge++){
  unsigned char bytes[3]={edge==4?0x80:0x90,notes[i],edge==2||edge==4?0:127};snd_seq_event_t e;need(snd_midi_event_encode(codec,bytes,3,&e)==3,"actual raw toggle MIDI encode");e.source=surface.full;
  unsigned kind,note;int down;need(surface_event(&e,surface.full,&kind,&note,&down)==1&&kind==11&&note==notes[i],"physical toggle enters general intent classifier");
  surface_general_press(&surface,&in,&snapshot,note,down,100);
 }
 need(in.global_count==6,"duplicate down and releases create no extra intent; rapid pairs remain ordered");
 for(unsigned i=0;i<6;i++)need(in.global_events[i].operation==ops[i/2]&&!in.global_events[i].bits,"MIDI press maps to typed toggle without a copied desired value");
 input_pump(&in,&bank,&snapshot,101);CommandRequest r;need(command_request_read(commands->slots,1,&r)&&r.reserved==GLOBAL_RECORD_TOGGLE&&r.global_owner==100&&r.field_incarnation==101&&!r.bits,"raw Record reaches actual CMD12 publication with RP identity only");
 snd_midi_event_free(codec);free(commands);puts("PASS raw MIDI95/89/86 -> production press dedup -> ordered native toggle intents -> CMD12 request; source snapshot and ALSA hardware substituted");
}
static void effects_policy_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"Effects policy state");MirrorInput in;MirrorBank b;MIRROR_COPY(s);reset_fixture(c,&in,&b,&s,12);s.selected_serial=1;s.selection.bits=s.tracks[0].track;bank_apply(&b,&s,100);Surface surf={.source=3,.full={32,0}};
 surface_assignment(&surf,&in,&b,&s,43,100);bank_apply(&b,&s,100);input_effects_interest(&in,&b,&s,100,1);need(b.assignment==BA_EFFECT&&!b.flip&&atomic_load(&c->effects_interest.enabled)&&atomic_load(&c->effects_interest.slot)==EFFECT_LIST,"Plug-In enters bounded selected-track insert list interest");
 s.effects_available=1;s.effects=(EffectsCopy){.epoch=1,.serial=1,.track_owner=2,.program_owner=3,.generation=7,.slot=EFFECT_LIST,.status=EF_READY,.tick=100};for(unsigned i=0;i<4;i++){s.effects.slots[i]=(EffectSlot){.key=i?0:0x2000,.ap=i?0:0x3000,.generation=7,.count=i?0:17,.presentation_count=i?0:17,.status=i?EF_EMPTY:EF_READY,.enable_valid=!i,.enable_bits=0x3f800000,.enable_revision=2,.enable_tick=100};strcpy(s.effects.slots[i].name,"Delay");}bank_apply(&b,&s,100);
 need(channel_output(&surf,&b,&s,100)&&observed_led[43]&&!observed_led[40]&&!observed_led[42]&&!memcmp(observed_lcd,"1 On   ",7)&&!memcmp(observed_lcd+56,"Delay  ",7),"actual On host state/LCD and exclusive Plug-In assignment from copied inserts");
 surface_encoder(&surf,&in,&b,&s,1,0,1,100);need(b.effects_slot==EFFECT_LIST&&in.effects[1].pending&&in.effects[1].request.reserved==EFFECT_CHOOSER&&!in.effects[1].request.bits&&in.effects[1].request.effect_slot==1,"empty insert push queues native chooser add and stays on list");in.effects[1].pending=0;surface_encoder(&surf,&in,&b,&s,0,0,1,100);need(b.effects_slot==0,"encoder push selects displayed occupied insert");
 s.effects.slot=0;for(unsigned i=0;i<8;i++){EffectParameter *p=s.effects.parameters+i;*p=(EffectParameter){.index=i,.position=i,.status=EF_READY,.tick=100,.revision=2,.bits=0x3f000000,.minimum=0,.maximum=0x3f800000};snprintf(p->name,sizeof(p->name),"Rate%u",i);strcpy(p->text,"0.5 Hz");}memset(surf.wire_valid,0,sizeof(surf.wire_valid));for(unsigned i=0;i<8;i++)need(channel_output(&surf,&b,&s,100),"parameter output strip cadence");need(!memcmp(observed_lcd,"Rate0  ",7)&&!memcmp(observed_lcd+56,"0.5Hz  ",7),"actual parameter name/native text rows reach MCU output");
 in.jog_shift=1;surface_assignment(&surf,&in,&b,&s,43,100);need(b.effects_slot==0&&in.effects[0].pending&&in.effects[0].request.reserved==EFFECT_CHOOSER&&in.effects[0].request.bits==1&&in.effects[0].request.effect_key==0x2000,"Shift Plug-In queues native replacement without leaving current slot");in.jog_shift=0;in.effects[0].pending=0;
 MotorIdentity original=b.faders[0].identity;surface_encoder(&surf,&in,&b,&s,0,0,1,100);need(!in.effects[0].pending,"parameter push does not invent default reset");surface_encoder(&surf,&in,&b,&s,0,6,0,100);input_pump(&in,&b,&s,100);CommandRequest r;need(command_request_read(c->slots,1,&r)&&r.reserved==EFFECT_PARAMETER&&r.effect_key==0x2000&&r.effect_slot==0&&r.effect_index==0&&r.effect_generation==7&&!memcmp(&original,&b.faders[0].identity,sizeof(original)),"real encoder handler publishes effect target without rebinding ordinary fader");
 s.effects.parameters[2].steps=0x7fffffff;surface_encoder(&surf,&in,&b,&s,2,0,1,100);need(!in.effects[2].pending,"continuous defaultlarge native stepcount does not enable toggle");
 s.effects.parameters[1].steps=2;s.effects.parameters[1].bits=0;surface_encoder(&surf,&in,&b,&s,1,0,1,100);need(in.effects[1].pending&&in.effects[1].request.bits==0x3f800000,"native binary push switches Off to On");surface_encoder(&surf,&in,&b,&s,1,0,1,100);need(in.effects[1].pending&&in.effects[1].request.bits==0,"second unsent push coalesces back to Off");surface_encoder(&surf,&in,&b,&s,1,0,1,100);input_pump(&in,&b,&s,100);CommandRequest toggle;unsigned toggle_at=command_find(c,2);need(toggle_at<COMMAND_SLOTS&&command_request_read(c->slots+toggle_at,2,&toggle)&&toggle.effect_index==1&&toggle.bits==0x3f800000,"binary toggle uses same typed native command path");surface_encoder(&surf,&in,&b,&s,1,0,1,100);input_pump(&in,&b,&s,100);need(in.effects[1].pending&&in.effects[1].request.bits==0&&atomic_load(&c->published)==2,"equal-to-old-source reversal retained while conflicting native toggle is outstanding");
 surface_navigation(&in,&b,&s,1,101);need(b.effects_page==1,"Bank right pages eight parameters");surface_navigation(&in,&b,&s,1,101);surface_navigation(&in,&b,&s,1,101);need(b.effects_page==2,"last partial Effects page bounds Bank right");surface_assignment(&surf,&in,&b,&s,50,101);need(b.flip&&b.assignment==BA_EFFECT,"Effects parameter page admits explicit Flip");
 surface_assignment(&surf,&in,&b,&s,43,102);need(b.effects_slot==EFFECT_LIST,"Plug-In returns list");bank_apply(&b,&s,102);surface_encoder(&surf,&in,&b,&s,0,0,1,102);s.effects.generation++;bank_apply(&b,&s,103);need(b.effects_slot==EFFECT_LIST,"replacement generation returns to insert list");
 surface_assignment(&surf,&in,&b,&s,42,104);bank_apply(&b,&s,104);input_effects_interest(&in,&b,&s,104,1);need(b.assignment==BA_PAN&&!atomic_load(&c->effects_interest.enabled),"Pan exits Effects and stops native reads");free(c);
 puts("PASS actual Effects list/page/input/LCD mode path with copied snapshots and ALSA output substituted; physical MCU and native metadata bodies unobserved");
}
static void jog_policy_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"jog policy state");MirrorInput in;MirrorBank b;MIRROR_COPY(s);reset_fixture(c,&in,&b,&s,2);
 snd_midi_event_t *codec;need(!snd_midi_event_new(32,&codec),"jog ALSA codec");snd_seq_addr_t address={32,0};
 unsigned char back[3]={0xb0,60,65},forward[3]={0xb0,60,1},shift[3]={0x90,70,127},fine[3]={0xb0,60,7};
 raw_input(codec,back,address,&in,&b,&s,101);raw_input(codec,forward,address,&in,&b,&s,101);raw_input(codec,shift,address,&in,&b,&s,101);raw_input(codec,fine,address,&in,&b,&s,101);
 need(in.jog_count==3,"one ordered event per real MCU wheel packet; opposite steps do not cancel");input_pump(&in,&b,&s,102);
 need(in.submitted==1&&!in.error&&in.jog_count==2,"one outstanding jog retains later opposite and fine demand");
 for(unsigned i=0;i<3;i++){
  unsigned at=command_find(c,i+1);CommandRequest r;need(at<COMMAND_SLOTS&&command_request_read(c->slots+at,i+1,&r),"actual ordered request decode");
  need(r.reserved==(i==2?JOG_PULSE:JOG_BEAT)&&(int32_t)r.bits==(i==0?-1:i==1?1:7),"direction and units retained");
  CommandSlot *slot=c->slots+at;
  for(unsigned n=0;n<4;n++){
   unsigned lane=n?1:0;CommandLane *l=slot->lanes+lane;if(atomic_load(&l->sequence)!=r.seq)atomic_store(&l->published,0);atomic_store(&l->sequence,r.seq);atomic_store(&l->token,n?456:123);
   CommandEvent e={.kind=n==3?CE_JOG_OUTCOME:CE_JOG_ENQUEUE+n,.request=r.seq,.tick=110+n,.token=n?456:123,.sp=n==1?0x8030:0x8000,.capture=0x9000+32*i,.counter=n==3?0:0x7008,.program=0x7000,.controller=r.reserved,.bits=r.bits,.revision=1,.reserved=r.reserved,.property=n==3?900:0};
   command_event_store(l->events+atomic_load(&l->published),&e);atomic_store(&l->published,atomic_load(&l->published)+1);
  }
  atomic_store(&slot->returned,r.seq);atomic_store(&slot->processed,r.seq);atomic_store(&slot->done,r.seq);atomic_store(&slot->sealed,r.seq);atomic_store(&c->consumed,i+1);
  s.heartbeat=120+i*2;input_pump(&in,&b,&s,s.heartbeat);if(in.settled!=i+1||in.error)fprintf(stderr,"JOG fixture iteration=%u settled=%u error=%u seen=%u outcome=%u\n",i,in.settled,in.error,in.flights[at].jog_seen,in.flights[at].jog_outcome_seen);need(in.settled==i+1&&!in.error,"sealed real receipt shape settles without fabricated position");
  reclaim_fixture(c,i+1);input_pump(&in,&b,&s,s.heartbeat+1);
 }
 need(input_drained(&in),"all ordered callbacks settle and reclaim");
 in.jog_wait=1;in.jog_wait_owner=900;in.jog_wait_epoch=1;in.jog_wait_count=7;s.playing.bits=1;
 need(!input_jog_admit(&in,&s),"playing admission requires actual progress witness");
 atomic_store(&c->navigation_revision,2);atomic_store(&c->navigation_owner,900);atomic_store(&c->navigation_epoch,1);atomic_store(&c->navigation_count,7);
 need(!input_jog_admit(&in,&s),"same progress at DONE does not reopen seek");atomic_store(&c->navigation_count,8);need(input_jog_admit(&in,&s),"later same owner and epoch progress reopens playing navigation");
 in.jog_wait=1;s.playing.bits=0;need(input_jog_admit(&in,&s),"authoritative Stop reopens navigation without waiting for advance");
 input_jog_add(&in,&s,JOG_BEAT,2,130);input_jog_add(&in,&s,JOG_BEAT,3,131);input_jog_add(&in,&s,JOG_BEAT,-1,132);need(in.jog_count==2&&in.jog_events[in.jog_head].delta==5,"only compatible adjacent same direction demand coalesces");
 input_jog_add_origin(&in,&s,JOG_BEAT,1,133,1);input_jog_release(&in,1);need(in.jog_count==2,"button release removes only unsent held-button demand");input_jog_discard(&in);
 Surface surface={.source=3,.full=address};
 /* Native IsRecordEnabled admits modes1/3; modes2/4 are Overdub only.
  * Columns: stopped, playing, unavailable transport. */
 const unsigned record_led[5][3]={{0,0,0},{1,127,0},{0,0,0},{1,1,1},{0,0,0}};
 for(unsigned mode=0;mode<5;mode++)for(unsigned transport=0;transport<3;transport++)for(unsigned available=0;available<2;available++){
  s.record_mode=(CopiedField){.available=available,.bits=mode};s.playing=(CopiedField){.available=transport!=2,.bits=transport==1};
  need(channel_output(&surface,&b,&s,s.heartbeat)&&observed_led[95]==(available?record_led[mode][transport]:0),"Record LED matches five native modes, stopped/playing and unavailable feedback; Overdub stays off");
 }

 snd_midi_event_free(codec);free(c);puts("PASS actual MCU jog/Shift/FF framing -> ordered pointer-free requests; separate native no-op evidence/epochs/closure; unsent reset/disconnect and held-release policy (native events/ALSA output substituted)");
}
/* One data-wheel flight's three producer events: dispatch, the step the observer
 * says it actually passed to the app's focus controller, and return. */
/* The producer's dispatch/step/return receipt for one counted-focus request.
 * Every event carries the request's own operation, as wheel-capture.inc emits
 * it, because the bridge settles a flight only when both fields match. */
static void counted_focus_evidence(CommandState *c,unsigned request,int delivered,uint32_t tick,unsigned operation){
 unsigned at=command_find(c,request);need(at<COMMAND_SLOTS,"data-wheel fixture slot identity");
 CommandSlot *slot=c->slots+at;CommandRequest r;need(command_request_read(slot,request,&r),"data-wheel fixture request");
 CommandLane *l=slot->lanes+1;
 if(atomic_load(&l->sequence)!=request){atomic_store(&l->published,0);atomic_store(&l->sequence,request);}
 atomic_store(&l->token,123);
 const unsigned kinds[3]={CE_DISPATCH,CE_WHEEL_STEP,CE_RETURN};
 for(unsigned n=0;n<3;n++){
  CommandEvent e={.kind=kinds[n],.request=request,.tick=tick+n,.token=123,.controller=operation,.bits=n==1?(uint32_t)(int32_t)delivered:r.bits,.revision=r.epoch,.reserved=operation};
  unsigned k=atomic_load(&l->published);command_event_store(l->events+k,&e);atomic_store(&l->published,k+1);
 }
 atomic_store(&c->consumed,atomic_load(&c->consumed)+1);
 atomic_store(&slot->dispatched,request);atomic_store(&slot->returned,request);atomic_store(&slot->processed,request);
 atomic_store_explicit(&slot->done,request,memory_order_release);atomic_store_explicit(&slot->sealed,request,memory_order_release);
}
static void wheel_evidence(CommandState *c,unsigned request,int delivered,uint32_t tick){counted_focus_evidence(c,request,delivered,tick,JOG_DATA);}
#if X_TOUCH_BLOCKING_ENABLED
/* One press flight's three producer events: dispatch, the observer's boolean
 * call receipt, and return. */
static void press_evidence(CommandState *c,unsigned request,unsigned delivered,uint32_t tick){
 unsigned at=command_find(c,request);need(at<COMMAND_SLOTS,"press fixture slot identity");
 CommandSlot *slot=c->slots+at;CommandRequest r;need(command_request_read(slot,request,&r),"press fixture request");
 CommandLane *l=slot->lanes+1;
 if(atomic_load(&l->sequence)!=request){atomic_store(&l->published,0);atomic_store(&l->sequence,request);}
 atomic_store(&l->token,123);
 const unsigned kinds[3]={CE_DISPATCH,CE_WHEEL_STEP,CE_RETURN};
 for(unsigned n=0;n<3;n++){
  CommandEvent e={.kind=kinds[n],.request=request,.tick=tick+n,.token=123,.controller=JOG_PRESS,.bits=n==1?delivered:r.bits,.revision=r.epoch,.reserved=JOG_PRESS};
  unsigned k=atomic_load(&l->published);command_event_store(l->events+k,&e);atomic_store(&l->published,k+1);
 }
 atomic_store(&c->consumed,atomic_load(&c->consumed)+1);
 atomic_store(&slot->dispatched,request);atomic_store(&slot->returned,request);atomic_store(&slot->processed,request);
 atomic_store_explicit(&slot->done,request,memory_order_release);atomic_store_explicit(&slot->sealed,request,memory_order_release);
}
#endif
/* X-Touch Scrub selects between the existing scrub behaviour and the wheel
 * acting as the MPC data wheel. Nothing about the physical wheel changes; only
 * where its detents are routed, and the mode lives in the connected Surface. */
#if X_TOUCH_BLOCKING_ENABLED
/* One Duplicate Sequence flight's three producer events: the begin, the record
 * of the two slots the observer resolved, and the end. */
static void duplicate_evidence(CommandState *c,unsigned request,uint32_t source,uint32_t destination,uint32_t result,uint32_t tick,unsigned with_state){
 unsigned at=command_find(c,request);need(at<COMMAND_SLOTS,"duplicate fixture slot identity");
 CommandSlot *slot=c->slots+at;CommandRequest r;need(command_request_read(slot,request,&r),"duplicate fixture request");
 CommandLane *l=slot->lanes+1;
 if(atomic_load(&l->sequence)!=request){atomic_store(&l->published,0);atomic_store(&l->sequence,request);}
 atomic_store(&l->token,123);
 const unsigned kinds[3]={CE_GLOBAL_BEGIN,CE_GLOBAL_STATE,CE_GLOBAL_END};
 for(unsigned n=0;n<3;n++){
  if(n==1&&!with_state)continue;
  CommandEvent e={.kind=kinds[n],.request=request,.tick=tick+n,.token=123,.program=0x90000,.controller=GLOBAL_SEQ_DUPLICATE,
   .counter=n==1?source:0,.bits=n==0?r.bits:n==1?destination:result,.revision=r.epoch,.reserved=GLOBAL_SEQ_DUPLICATE};
  unsigned k=atomic_load(&l->published);command_event_store(l->events+k,&e);atomic_store(&l->published,k+1);
 }
 atomic_store(&c->consumed,atomic_load(&c->consumed)+1);
 atomic_store(&slot->dispatched,request);atomic_store(&slot->returned,request);atomic_store(&slot->processed,request);
 atomic_store_explicit(&slot->done,request,memory_order_release);atomic_store_explicit(&slot->sealed,request,memory_order_release);
}
#endif
/* The X-Touch Replace button (note 85) as Duplicate Sequence: one press is one
 * global request against the enrolled Editor, in either wheel mode, and the
 * settlement is the observer's own three-event record of what it submitted.
 * Everything from the first press onwards is what X_TOUCH_BLOCKING_ENABLED
 * owns, so the shipped-configuration build checks the raw classification only;
 * blocking_gate_checks() below is where that build states what note 85 does. */
static void sequence_duplicate_bridge_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"duplicate bridge state");
 MirrorInput in;MirrorBank b;MIRROR_COPY(s);snd_seq_addr_t address={32,0};
 snd_midi_event_t *codec;need(!snd_midi_event_new(32,&codec),"duplicate raw codec");
 const unsigned char down[3]={0x90,85,127},up[3]={0x80,85,0};
 snd_seq_event_t ev;unsigned kind,channel;int value;
 need(snd_midi_event_encode(codec,down,3,&ev)==3,"Replace raw MIDI");ev.source=address;
 need(surface_event(&ev,address,&kind,&channel,&value)==1&&kind==11&&channel==85&&value,"Replace classifies as a general button press");
 need(snd_midi_event_encode(codec,up,3,&ev)==3,"Replace raw release");ev.source=address;
 need(surface_event(&ev,address,&kind,&channel,&value)==1&&kind==11&&channel==85&&!value,"and its release classifies with the same kind");
 reset_fixture(c,&in,&b,&s,2);s.editor_owner=77;s.zoom_owner=88;in.jog_epoch=s.epoch;
#if X_TOUCH_BLOCKING_ENABLED
 Surface surface={.source=3,.full=address};
 surface_general_press(&surface,&in,&s,85,1,200);
 need(in.global_count==1&&in.global_events[0].operation==GLOBAL_SEQ_DUPLICATE&&in.global_events[0].owner==77&&!in.global_events[0].bits&&!in.wheel_press&&!in.wheel_delta,"one Replace press queues one Duplicate Sequence operation against the enrolled Editor and no wheel work");
 surface_general_press(&surface,&in,&s,85,1,201);
 need(in.global_count==1,"a repeated down report is not a second duplicate");
 surface_general_press(&surface,&in,&s,85,0,202);
 need(in.global_count==1,"the release queues nothing");
 surface.data_wheel=1;surface_general_press(&surface,&in,&s,85,1,203);surface_general_press(&surface,&in,&s,85,0,204);
 need(in.global_count==2&&in.global_events[1].operation==GLOBAL_SEQ_DUPLICATE&&!in.wheel_press,"Replace means the same thing in data-wheel mode");
 surface.data_wheel=0;in.jog_shift=1;surface_general_press(&surface,&in,&s,85,1,205);surface_general_press(&surface,&in,&s,85,0,206);
 need(in.global_count==3&&in.global_events[2].operation==GLOBAL_SEQ_DUPLICATE,"Shift does not give the button a second meaning");
 in.jog_shift=0;input_jog_discard(&in);need(!in.global_count,"a reload or disconnect clears queued duplicates with the rest of the jog gesture");
 /* One press, published, settled and reclaimed on the real protocol. */
 reset_fixture(c,&in,&b,&s,2);s.editor_owner=77;in.jog_epoch=s.epoch;
 Surface one={.source=3,.full=address};
 surface_general_press(&one,&in,&s,85,1,300);surface_general_press(&one,&in,&s,85,0,301);
 input_pump(&in,&b,&s,302);
 CommandRequest r;unsigned seq=atomic_load(&c->published);
 need(seq==1&&command_request_read(c->slots+command_find(c,seq),seq,&r),"the press publishes exactly one request");
 need(r.reserved==GLOBAL_SEQ_DUPLICATE&&r.global_owner==77&&r.project_owner==s.project_owner&&!r.bits&&!r.track_owner&&!r.program_owner&&!r.pad_owner&&!r.field_incarnation&&r.epoch==s.epoch,"it is a Duplicate Sequence request naming the enrolled Editor and this Project and no other target");
 duplicate_evidence(c,seq,1,2,1,302,1);s.heartbeat=310;input_pump(&in,&b,&s,310);
 need(!in.error&&in.settled==1&&atomic_load(&c->slots[command_find(c,seq)].settled)==seq,"the begin/slot-record/end receipt settles the duplicate request");
 reclaim_fixture(c,seq);input_pump(&in,&b,&s,311);
 /* The receipt is checked, not assumed: a missing record, two equal slots and
  * a result that is not a boolean are each refused. */
 const unsigned char equal_slots=1,missing=2,bad_result=3;
 for(unsigned bad=equal_slots;bad<=bad_result;bad++){
  reset_fixture(c,&in,&b,&s,2);s.editor_owner=77;in.jog_epoch=s.epoch;
  Surface bad_surface={.source=3,.full=address};
  surface_general_press(&bad_surface,&in,&s,85,1,320);surface_general_press(&bad_surface,&in,&s,85,0,321);
  input_pump(&in,&b,&s,322);unsigned bad_seq=atomic_load(&c->published);
  duplicate_evidence(c,bad_seq,bad==equal_slots?2:1,2,bad==bad_result?2:1,322,bad!=missing);
  s.heartbeat=330;input_pump(&in,&b,&s,330);
  need(in.error==C_TRACE_AMBIGUOUS&&!in.settled,"a duplicate receipt that names one slot twice, omits the slot record or carries a non-boolean result is refused");
 }
#endif
 snd_midi_event_free(codec);free(c);
#if X_TOUCH_BLOCKING_ENABLED
 puts("PASS X-Touch Replace is Duplicate Sequence: note 85 classification, one queued operation per press edge with no wheel work, the same meaning in both wheel modes and under Shift, reload clearing, publication as one global request naming only the enrolled Editor and Project, settlement on the observer's begin/slot-record/end receipt, and refusal of a receipt that omits the slot record, names one slot twice or carries a non-boolean result (native calls and ALSA hardware substituted)");
#else
 puts("PASS X-Touch Replace classifies as note 85 with its release; the shipped build gates the Duplicate Sequence route, so blocking_gate_checks owns what the press does here");
#endif
}
/* What X_TOUCH_BLOCKING_ENABLED actually does to the surface, checked in both
 * configurations. mirror-input-build.sh compiles the shipped bridge with the
 * gate off and compiled every test binary with it on, so no check in this tree
 * had ever observed the configuration that reaches the device: a note named in
 * the gate's comment and left out of its expression passed everything. Save was
 * exactly that, and on hardware it killed the command source on the first press.
 * mirror-input-check.sh now runs this file a second time built the shipped way,
 * and this is the check that tells the two builds apart.
 *
 * Both halves are needed. The gated notes must publish nothing, and ungated
 * notes must still publish in the SAME session, so the gate-off half cannot
 * pass by breaking the input path wholesale. Note 81 (Undo, Shift+Undo as Redo)
 * is deliberately ungated and is checked here as an ungated route in both
 * builds: it is the nearest neighbour of the gated notes in the same
 * expression, and a mistake there would silently cost a working button. */
static void blocking_gate_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"blocking gate state");
 MirrorInput in;MirrorBank b;MIRROR_COPY(s);snd_seq_addr_t address={32,0};
 reset_fixture(c,&in,&b,&s,2);s.editor_owner=77;s.zoom_owner=88;in.jog_epoch=s.epoch;
 s.record_mode=(CopiedField){.available=1,.owner_incarnation=100,.incarnation=101};
 Surface surface={.source=3,.full=address};
 /* The three gated routes: Save (80), Replace/Duplicate Sequence (85) and the
  * data-wheel centre press (100), in both wheel modes and under Shift. */
 surface_general_press(&surface,&in,&s,80,1,200);surface_general_press(&surface,&in,&s,80,0,201);
 surface_general_press(&surface,&in,&s,85,1,202);surface_general_press(&surface,&in,&s,85,0,203);
 in.jog_shift=1;
 surface_general_press(&surface,&in,&s,80,1,204);surface_general_press(&surface,&in,&s,80,0,205);
 surface_general_press(&surface,&in,&s,85,1,206);surface_general_press(&surface,&in,&s,85,0,207);
 in.jog_shift=0;surface.data_wheel=1;
 surface_general_press(&surface,&in,&s,80,1,208);surface_general_press(&surface,&in,&s,80,0,209);
 surface_general_press(&surface,&in,&s,85,1,210);surface_general_press(&surface,&in,&s,85,0,211);
 surface_general_press(&surface,&in,&s,100,1,212);surface_general_press(&surface,&in,&s,100,0,213);
 surface.data_wheel=0;
#if X_TOUCH_BLOCKING_ENABLED
 need(in.global_count==6&&in.wheel_press==1,"with the gate compiled in, Save and Duplicate Sequence queue in both wheel modes and under Shift, and the centre press is pending");
 need(in.global_events[0].operation==GLOBAL_SAVE&&in.global_events[1].operation==GLOBAL_SEQ_DUPLICATE&&in.global_events[2].operation==GLOBAL_SAVE&&in.global_events[3].operation==GLOBAL_SEQ_DUPLICATE&&in.global_events[4].operation==GLOBAL_SAVE&&in.global_events[5].operation==GLOBAL_SEQ_DUPLICATE,"and every queued operation is the one its note means");
 input_jog_discard(&in);in.jog_epoch=s.epoch;
#else
 need(!in.global_count&&!in.wheel_press,"the shipped bridge queues nothing for Save, Duplicate Sequence or the data-wheel centre press, in either wheel mode and under Shift");
#endif
 input_pump(&in,&b,&s,214);
 need(!in.error&&!atomic_load(&c->published),"and no gated note has published a request");
 /* Same session, same fixture: the ungated neighbours still work. */
 surface_general_press(&surface,&in,&s,58,1,220);surface_general_press(&surface,&in,&s,58,0,221);
 surface_general_press(&surface,&in,&s,95,1,222);surface_general_press(&surface,&in,&s,95,0,223);
 surface_general_press(&surface,&in,&s,81,1,224);surface_general_press(&surface,&in,&s,81,0,225);
 in.jog_shift=1;surface_general_press(&surface,&in,&s,81,1,226);surface_general_press(&surface,&in,&s,81,0,227);in.jog_shift=0;
 need(in.global_count==4&&in.global_events[0].operation==GLOBAL_PAGE_MAIN+4&&in.global_events[1].operation==GLOBAL_RECORD_TOGGLE&&in.global_events[2].operation==GLOBAL_UNDO&&in.global_events[3].operation==GLOBAL_REDO,"a page button, Record, Undo and Shift+Undo are ungated and queue their own operations");
 input_pump(&in,&b,&s,228);
 need(!in.error&&!in.global_count&&atomic_load(&c->published)==4,"and all four publish in the same session, so nothing here passes by a dead input path");
 CommandRequest r;
 need(command_request_read(c->slots+command_find(c,1),1,&r)&&r.reserved==GLOBAL_PAGE_MAIN+4&&r.global_owner==77,"the page request names the enrolled Editor");
 need(command_request_read(c->slots+command_find(c,3),3,&r)&&r.reserved==GLOBAL_UNDO&&r.global_owner==77,"and Undo publishes as a history request against the same Editor");
 need(command_request_read(c->slots+command_find(c,4),4,&r)&&r.reserved==GLOBAL_REDO,"and Shift+Undo publishes as Redo");
 free(c);
#if X_TOUCH_BLOCKING_ENABLED
 puts("PASS X-Touch blocking-route gate, compiled in: Save, Duplicate Sequence and the data-wheel centre press all reach the input queue, while a page button, Record, Undo and Shift+Undo publish alongside them (native calls and ALSA hardware substituted)");
#else
 puts("PASS X-Touch blocking-route gate in the shipped configuration (-DX_TOUCH_BLOCKING_ENABLED=0): notes 80, 85 and the data-wheel centre press 100 queue nothing and publish no request in either wheel mode or under Shift, while a page button, Record, Undo and Shift+Undo still publish in the same session (native calls and ALSA hardware substituted)");
#endif
}
static void data_wheel_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"data-wheel policy state");MirrorInput in;MirrorBank b;MIRROR_COPY(s);reset_fixture(c,&in,&b,&s,2);
 snd_midi_event_t *codec;need(!snd_midi_event_new(32,&codec),"data-wheel ALSA codec");snd_seq_addr_t address={32,0};
 Surface surface={.source=3,.full=address};
 unsigned char scrub_down[3]={0x90,101,127},scrub_up[3]={0x80,101,0};
 unsigned char forward[3]={0xb0,60,1},back[3]={0xb0,60,65},shift_down[3]={0x90,70,127};
 snd_seq_event_t e;unsigned kind,note;int down;
 /* The Scrub button is a physical general button like Zoom, not a new class. */
 need(snd_midi_event_encode(codec,scrub_down,3,&e)==3,"Scrub raw MIDI");e.source=address;
 need(surface_event(&e,address,&kind,&note,&down)==1&&kind==11&&note==101&&down,"Scrub enters the general intent classifier as note 101");
 {unsigned char fs1[3]={0x90,102,127},fs2[3]={0x90,103,127},fs1_up[3]={0x80,102,0};snd_seq_event_t f;unsigned fk,fn;int fd;
  need(snd_midi_event_encode(codec,fs1,3,&f)==3,"footswitch 1 raw MIDI");f.source=address;
  need(surface_event(&f,address,&fk,&fn,&fd)==1&&fk==12&&fn==94&&fd,"footswitch 1 enters as the transport Play relay");
  need(snd_midi_event_encode(codec,fs2,3,&f)==3,"footswitch 2 raw MIDI");f.source=address;
  need(surface_event(&f,address,&fk,&fn,&fd)==1&&fk==11&&fn==95&&fd,"footswitch 2 enters as the Record toggle");
  need(snd_midi_event_encode(codec,fs1_up,3,&f)==3,"footswitch 1 release raw MIDI");f.source=address;
  need(surface_event(&f,address,&fk,&fn,&fd)==1&&fk==12&&fn==94&&!fd,"footswitch 1 release is the Play relay release");}
 need(!surface.data_wheel,"a fresh session starts in scrub mode");
 surface_general_press(&surface,&in,&s,note,down,100);need(surface.data_wheel,"Scrub press selects data-wheel mode");
 surface_general_press(&surface,&in,&s,note,down,101);need(surface.data_wheel,"a repeated down report is not a second toggle");
 need(snd_midi_event_encode(codec,scrub_up,3,&e)==3,"Scrub release raw MIDI");e.source=address;
 need(surface_event(&e,address,&kind,&note,&down)==1&&!down,"Scrub release classification");
 surface_general_press(&surface,&in,&s,note,down,102);need(surface.data_wheel,"release is not a toggle");
 need(!in.global_count&&!in.jog_count&&!in.wheel_delta,"the mode toggle publishes no command of its own");
 /* The Scrub LED shows the mode through the same state table as Zoom. */
 /* The Scrub LED shows scrub mode, which is what the button leaves when it is
  * lit; data-wheel mode is the unlit state. Zoom is a scrub-mode toggle, so in
  * data-wheel mode its LED is off whatever zoom_mode holds. */
 surface.zoom_mode=1;
 need(channel_output(&surface,&b,&s,s.heartbeat)&&!observed_led[101]&&!observed_led[100],"data-wheel mode clears the Scrub LED and the Zoom LED");
 surface_general_press(&surface,&in,&s,101,1,103);surface_general_press(&surface,&in,&s,101,0,104);
 need(!surface.data_wheel&&channel_output(&surface,&b,&s,s.heartbeat)&&observed_led[101]==127&&observed_led[100]==127,"scrub mode lights the Scrub LED and restores the Zoom LED");
 surface.zoom_mode=0;need(channel_output(&surface,&b,&s,s.heartbeat)&&!observed_led[100],"zoom off clears its own LED in scrub mode");
 /* Scrub mode: the wheel still produces bar/beat/pulse navigation. */
 raw_data_wheel=0;raw_input(codec,forward,address,&in,&b,&s,110);
 need(in.jog_count==1&&in.jog_events[in.jog_head].operation==JOG_BEAT&&!in.wheel_delta,"scrub mode still queues a native transport jog");
 raw_input(codec,shift_down,address,&in,&b,&s,111);raw_input(codec,forward,address,&in,&b,&s,111);
 need(in.jog_count==2&&in.jog_events[(in.jog_head+1)%INPUT_JOG_EVENTS].operation==JOG_PULSE&&!in.wheel_delta,"Shift still selects pulses in scrub mode");
 input_jog_discard(&in);
 /* Data-wheel mode without Shift: every detent is one data-wheel step and
  * opposite detents cancel. Shift + wheel in this mode is note selection. */
 raw_data_wheel=1;in.jog_shift=0;
 raw_input(codec,forward,address,&in,&b,&s,120);raw_input(codec,forward,address,&in,&b,&s,121);
 need(in.wheel_delta==2&&!in.jog_count,"data-wheel mode accumulates detents and queues no transport jog");
 raw_input(codec,back,address,&in,&b,&s,122);
 need(in.wheel_delta==1,"opposite detents cancel in the data-wheel accumulator");
 need(!in.jog_count,"the data wheel never queues a transport jog in data-wheel mode");
 /* One request per pump, one alive at a time, and nothing for an empty
  * accumulator. */
 input_pump(&in,&b,&s,130);
 CommandRequest r;need(in.submitted==1&&command_request_read(c->slots,1,&r)&&r.reserved==JOG_DATA&&(int32_t)r.bits==1,"one data-wheel request carries the accumulated signed count");
 need(!r.project_owner&&!r.track_owner&&!r.program_owner&&!r.global_owner&&!r.field_incarnation&&!r.pad_owner&&r.epoch==s.epoch,"the data-wheel request names no Track, Program, Project or pad target");
 need(!in.wheel_delta,"publication empties the accumulator");
 unsigned published=atomic_load(&c->published);
 input_pump(&in,&b,&s,131);need(atomic_load(&c->published)==published,"an empty accumulator publishes nothing");
 raw_input(codec,forward,address,&in,&b,&s,132);raw_input(codec,forward,address,&in,&b,&s,132);
 input_pump(&in,&b,&s,133);
 need(atomic_load(&c->published)==published&&in.wheel_delta==2,"a second request waits for the first flight and keeps collecting detents");
 /* Settlement of the new field: the producer's three events, then the seal. */
 wheel_evidence(c,1,1,120);s.heartbeat=140;input_pump(&in,&b,&s,140);
 need(!in.error&&in.settled==1&&atomic_load(&c->slots[0].settled)==1,"the dispatch/step/return receipt settles the data-wheel request");
 need(atomic_load(&c->published)==published,"settlement alone does not release the next request");
 reclaim_fixture(c,1);input_pump(&in,&b,&s,141);
 need(atomic_load(&c->published)==published+1&&!in.wheel_delta&&!in.error,"the collected detents publish once the producer reclaims the slot");
 need(command_request_read(c->slots+command_find(c,published+1),published+1,&r)&&(int32_t)r.bits==2,"and carry the count accumulated while the first was in flight");
 /* A suppressed step is a completed request, not a failure: the observer may
  * legally deliver fewer steps than published, never more and never opposite. */
 wheel_evidence(c,2,0,141);s.heartbeat=145;input_pump(&in,&b,&s,145);
 need(!in.error&&in.settled==2,"a zero delivered step still settles the request");
 reclaim_fixture(c,2);input_pump(&in,&b,&s,146);
 raw_input(codec,forward,address,&in,&b,&s,147);input_pump(&in,&b,&s,147);
 wheel_evidence(c,3,2,147);s.heartbeat=150;input_pump(&in,&b,&s,150);
 need(in.error==C_TRACE_AMBIGUOUS&&in.settled==2,"a delivered step larger than the published count is refused");
 /* A project reload clears unsent detents with the rest of the jog gesture. */
 reset_fixture(c,&in,&b,&s,2);raw_data_wheel=1;
 raw_input(codec,forward,address,&in,&b,&s,200);need(in.wheel_delta==1,"accumulator collects before the reload");
 s.epoch=2;raw_input(codec,forward,address,&in,&b,&s,201);
 need(in.wheel_delta==1&&in.jog_epoch==2,"an epoch change discards the old detents and keeps only the new one");
 raw_input(codec,back,address,&in,&b,&s,202);need(!in.wheel_delta,"and the accumulator stays signed across the reload");
 raw_input(codec,forward,address,&in,&b,&s,203);input_jog_discard(&in);need(!in.wheel_delta,"disconnect and not-ready both clear the accumulator");
 /* A hard flick banks more detents than one native call may carry. The bridge
  * publishes at most that count and keeps the rest, so the total travel is
  * complete and monotonic instead of being clamped away by the observer. */
 reset_fixture(c,&in,&b,&s,2);raw_data_wheel=1;
 for(unsigned i=0;i<10;i++)raw_input(codec,forward,address,&in,&b,&s,300);
 need(in.wheel_delta==10,"ten detents inside one pump bank as ten");
 input_pump(&in,&b,&s,301);
 need(command_request_read(c->slots,1,&r)&&(int32_t)r.bits==WHEEL_MAX_STEPS&&in.wheel_delta==10-WHEEL_MAX_STEPS,"the first request carries one native call's worth and the remainder stays banked");
 wheel_evidence(c,1,WHEEL_MAX_STEPS,301);s.heartbeat=310;input_pump(&in,&b,&s,310);need(!in.error&&in.settled==1,"the first burst request settles");
 reclaim_fixture(c,1);input_pump(&in,&b,&s,311);
 need(command_request_read(c->slots+command_find(c,2),2,&r)&&(int32_t)r.bits==WHEEL_MAX_STEPS&&in.wheel_delta==10-2*WHEEL_MAX_STEPS,"the next request carries the next four");
 wheel_evidence(c,2,WHEEL_MAX_STEPS,311);s.heartbeat=320;input_pump(&in,&b,&s,320);reclaim_fixture(c,2);input_pump(&in,&b,&s,321);
 need(command_request_read(c->slots+command_find(c,3),3,&r)&&(int32_t)r.bits==10-2*WHEEL_MAX_STEPS&&!in.wheel_delta,"and the last request carries the remaining two, so nothing is dropped");
 /* Shift + jog in data-wheel mode is note selection: the same counted-focus
  * accumulator and receipt as the data wheel, under its own operation. */
 reset_fixture(c,&in,&b,&s,2);raw_data_wheel=1;in.jog_shift=1;
 raw_input(codec,forward,address,&in,&b,&s,500);raw_input(codec,forward,address,&in,&b,&s,501);
 need(in.wheel_delta==2&&in.wheel_operation==NOTE_SELECT&&!in.jog_count,"Shift + jog banks note steps and queues no transport jog");
 input_pump(&in,&b,&s,502);
 need(in.submitted==1&&command_request_read(c->slots,1,&r)&&r.reserved==NOTE_SELECT&&(int32_t)r.bits==2&&!in.wheel_delta,"one note-selection request carries the signed count");
 need(!r.project_owner&&!r.track_owner&&!r.program_owner&&!r.global_owner&&!r.field_incarnation&&!r.pad_owner&&r.epoch==s.epoch,"the note-selection request names no Track, Program, Project or pad target");
 counted_focus_evidence(c,1,2,502,NOTE_SELECT);s.heartbeat=510;input_pump(&in,&b,&s,510);
 need(!in.error&&in.settled==1,"the note-selection receipt settles the request");
 /* At the first or last note the observer moves nothing. That is a completed
  * request, not a failure, or every turn past an end would fault the lane. */
 reclaim_fixture(c,1);raw_input(codec,forward,address,&in,&b,&s,511);input_pump(&in,&b,&s,511);
 counted_focus_evidence(c,2,0,511,NOTE_SELECT);s.heartbeat=520;input_pump(&in,&b,&s,520);
 need(!in.error&&in.settled==2,"a clamped selection that moves nothing still settles");
 /* A receipt must name the operation it answers. Data-wheel evidence cannot
  * settle a note-selection flight. */
 reclaim_fixture(c,2);raw_input(codec,forward,address,&in,&b,&s,521);input_pump(&in,&b,&s,521);
 wheel_evidence(c,3,1,521);s.heartbeat=530;input_pump(&in,&b,&s,530);
 need(in.error==C_TRACE_AMBIGUOUS&&in.settled==2,"data-wheel evidence is refused for a note-selection request");
 /* Letting go of Shift mid-turn must never deliver banked note steps as value
  * changes, and pressing it again must never turn value steps into selection. */
 reset_fixture(c,&in,&b,&s,2);raw_data_wheel=1;in.jog_shift=1;
 raw_input(codec,forward,address,&in,&b,&s,540);
 need(in.wheel_operation==NOTE_SELECT&&in.wheel_delta==1,"a note step banks under Shift");
 in.jog_shift=0;raw_input(codec,forward,address,&in,&b,&s,541);
 need(in.wheel_operation==JOG_DATA&&in.wheel_delta==1,"releasing Shift discards the banked note step instead of converting it");
 in.jog_shift=1;raw_input(codec,back,address,&in,&b,&s,542);
 need(in.wheel_operation==NOTE_SELECT&&in.wheel_delta==-1,"pressing Shift again discards the banked data-wheel step");
 input_jog_discard(&in);
 need(in.wheel_operation==JOG_DATA&&!in.wheel_delta,"disconnect returns the accumulator to the data wheel");
 /* A project reload drops the old project's banked step, never a Shift that
  * is still physically held through the load. */
 reset_fixture(c,&in,&b,&s,2);raw_data_wheel=1;in.jog_shift=1;
 raw_input(codec,forward,address,&in,&b,&s,550);
 need(in.wheel_operation==NOTE_SELECT&&in.wheel_delta==1,"the first detent after a load selects");
 s.epoch=3;raw_input(codec,forward,address,&in,&b,&s,551);
 need(in.jog_shift&&in.jog_epoch==3&&in.wheel_operation==NOTE_SELECT&&in.wheel_delta==1,"a reload discards the banked step but keeps the held Shift, so the turn keeps selecting");
 raw_input(codec,forward,address,&in,&b,&s,552);
 need(in.wheel_operation==NOTE_SELECT&&in.wheel_delta==2&&!in.jog_count,"and later detents in the same hold still select notes");
 /* The cursor cluster belongs to whichever mode is selected, never to both. */
 reset_fixture(c,&in,&b,&s,2);s.editor_owner=77;s.zoom_owner=88;raw_data_wheel=1;
 Surface cursor={.source=3,.full=address};
 const unsigned cluster[5]={96,97,98,99,100};
 for(unsigned i=0;i<5;i++){
  unsigned char bytes[3]={0x90,(unsigned char)cluster[i],127};snd_seq_event_t ev;
  need(snd_midi_event_encode(codec,bytes,3,&ev)==3,"cursor cluster raw MIDI");ev.source=address;
  unsigned k,n;int d;need(surface_event(&ev,address,&k,&n,&d)==1&&k==11&&n==cluster[i]&&d,"cursor cluster classification");
 }
 for(unsigned i=0;i<4;i++){surface_general_press(&cursor,&in,&s,96+i,1,400);surface_general_press(&cursor,&in,&s,96+i,0,401);}
 need(in.global_count==4&&in.global_events[0].operation==GLOBAL_KEY_UP&&in.global_events[1].operation==GLOBAL_KEY_DOWN&&in.global_events[2].operation==GLOBAL_KEY_LEFT&&in.global_events[3].operation==GLOBAL_KEY_RIGHT&&!in.wheel_delta,"scrub mode keeps the ordinary arrow keys");
 surface_general_press(&cursor,&in,&s,100,1,402);surface_general_press(&cursor,&in,&s,100,0,403);
 need(cursor.zoom_mode&&in.global_count==4,"scrub mode keeps note 100 as the Zoom toggle");
 for(unsigned i=0;i<4;i++){surface_general_press(&cursor,&in,&s,96+i,1,404);surface_general_press(&cursor,&in,&s,96+i,0,405);}
 need(in.global_count==8&&in.global_events[4].operation==GLOBAL_ZOOM_UP&&in.global_events[7].operation==GLOBAL_ZOOM_IN&&!in.wheel_delta,"zoom mode keeps the zoom arrows");
 cursor.zoom_mode=0;input_jog_discard(&in);
 surface_general_press(&cursor,&in,&s,101,1,410);surface_general_press(&cursor,&in,&s,101,0,411);
 need(cursor.data_wheel,"Scrub selects data-wheel mode for the cursor cluster too");
 surface_general_press(&cursor,&in,&s,96,1,412);surface_general_press(&cursor,&in,&s,96,0,413);
 need(in.wheel_delta==-1&&!in.global_count,"cursor up is one data-wheel step (up-arrow decrements, matching list navigation) and no key");
 surface_general_press(&cursor,&in,&s,97,1,414);surface_general_press(&cursor,&in,&s,97,0,415);
 need(!in.wheel_delta&&!in.global_count,"cursor down returns the accumulator to zero on the same single-flight lane");
 surface_general_press(&cursor,&in,&s,98,1,416);surface_general_press(&cursor,&in,&s,98,0,417);
 surface_general_press(&cursor,&in,&s,99,1,418);surface_general_press(&cursor,&in,&s,99,0,419);
 need(in.global_count==2&&in.global_events[0].operation==GLOBAL_KEY_BACKTAB&&in.global_events[1].operation==GLOBAL_KEY_TAB,"left and right move focus with Shift+Tab and Tab");
 /* The centre button is the data wheel's own push through the focus
  * controller, not a synthetic keyboard Return: on this firmware an injected
  * Return does nothing to a focused item. It is one of the blocking routes the
  * shipped build compiles out, so the press half belongs to the gate-on build;
  * blocking_gate_checks owns what the shipped build does with note 100. */
#if X_TOUCH_BLOCKING_ENABLED
 surface_general_press(&cursor,&in,&s,100,1,420);surface_general_press(&cursor,&in,&s,100,0,421);
 need(in.wheel_press&&in.global_count==2,"the centre button is the data wheel's push and queues no key");
 need(!cursor.zoom_mode,"note 100 does not touch zoom while the wheel is the data wheel");
 surface_general_press(&cursor,&in,&s,100,1,422);surface_general_press(&cursor,&in,&s,100,0,423);
 need(in.wheel_press==1,"a second push before the first is published stays one pending press");
#endif
 input_pump(&in,&b,&s,430);
 need(!in.error&&command_request_read(c->slots,1,&r)&&r.reserved==GLOBAL_KEY_BACKTAB&&r.global_owner==77&&!r.bits,"the data-mode cursor keys publish as ordinary global key requests against the enrolled Editor");
#if X_TOUCH_BLOCKING_ENABLED
 unsigned press_seq=atomic_load(&c->published);
 need(command_request_read(c->slots+command_find(c,press_seq),press_seq,&r)&&r.reserved==JOG_PRESS&&r.bits==FOCUS_PRESS_BUTTON,"the centre push publishes one JOG_PRESS request carrying the device-measured button id");
 need(!r.project_owner&&!r.track_owner&&!r.program_owner&&!r.global_owner&&!r.field_incarnation&&!r.pad_owner&&r.epoch==s.epoch,"the press request names no Track, Program, Project or pad target");
 need(!in.wheel_press,"publication clears the pending press");
 unsigned settled_before=in.settled;
 surface_general_press(&cursor,&in,&s,100,1,431);surface_general_press(&cursor,&in,&s,100,0,432);
 input_pump(&in,&b,&s,433);
 need(atomic_load(&c->published)==press_seq&&in.wheel_press,"a second press waits for the first flight, one request per pump");
 press_evidence(c,press_seq,1,430);s.heartbeat=440;input_pump(&in,&b,&s,440);
 need(!in.error&&in.settled==settled_before+1&&atomic_load(&c->slots[command_find(c,press_seq)].settled)==press_seq,"the dispatch/call/return receipt settles the press request");
 reclaim_fixture(c,press_seq);input_pump(&in,&b,&s,441);
 need(atomic_load(&c->published)==press_seq+1&&!in.wheel_press&&!in.error,"the waiting press publishes once the producer reclaims the slot");
 /* A suppressed press is a completed request; a second call flag is not. */
 press_evidence(c,press_seq+1,0,441);s.heartbeat=450;input_pump(&in,&b,&s,450);
 need(!in.error&&in.settled==settled_before+2,"a press the observer suppressed still settles");
 reclaim_fixture(c,press_seq+1);input_pump(&in,&b,&s,451);
 surface_general_press(&cursor,&in,&s,100,1,452);surface_general_press(&cursor,&in,&s,100,0,453);
 input_pump(&in,&b,&s,454);
 press_evidence(c,press_seq+2,2,454);s.heartbeat=460;input_pump(&in,&b,&s,460);
 need(in.error==C_TRACE_AMBIGUOUS&&in.settled==settled_before+2,"a call receipt that is not a boolean is refused");
 /* Single flight is shared with the data-wheel steps: both act on the one
  * active focus controller. */
 reset_fixture(c,&in,&b,&s,2);s.editor_owner=77;raw_data_wheel=1;
 Surface both={.source=3,.full=address};both.data_wheel=1;
 raw_input(codec,forward,address,&in,&b,&s,500);input_pump(&in,&b,&s,501);
 unsigned step_seq=atomic_load(&c->published);
 need(command_request_read(c->slots+command_find(c,step_seq),step_seq,&r)&&r.reserved==JOG_DATA,"a data-wheel step publishes first");
 surface_general_press(&both,&in,&s,100,1,502);surface_general_press(&both,&in,&s,100,0,503);
 input_pump(&in,&b,&s,504);
 need(atomic_load(&c->published)==step_seq&&in.wheel_press,"a press waits while a data-wheel step is in flight");
 wheel_evidence(c,step_seq,1,504);s.heartbeat=510;input_pump(&in,&b,&s,510);
 reclaim_fixture(c,step_seq);input_pump(&in,&b,&s,511);
 unsigned press_after=atomic_load(&c->published);
 need(press_after==step_seq+1&&command_request_read(c->slots+command_find(c,press_after),press_after,&r)&&r.reserved==JOG_PRESS&&!in.wheel_press,"and publishes once the step is reclaimed");
 raw_input(codec,forward,address,&in,&b,&s,512);input_pump(&in,&b,&s,513);
 need(atomic_load(&c->published)==press_after&&in.wheel_delta==1,"a data-wheel step then waits while the press is in flight");
 press_evidence(c,press_after,1,513);s.heartbeat=520;input_pump(&in,&b,&s,520);
 need(!in.error,"the press settles against the same three-event receipt");
 reclaim_fixture(c,press_after);input_pump(&in,&b,&s,521);
 need(atomic_load(&c->published)==press_after+1&&!in.wheel_delta&&!in.error,"and the banked detent publishes once the press is reclaimed");
 input_jog_discard(&in);need(!in.wheel_press&&!in.wheel_delta,"a reload or disconnect clears a pending press with the rest of the jog gesture");
#endif
 /* Reconnect: the mode is session state and is not persisted. */
 Surface reconnect={.source=3,.full=address};reconnect.data_wheel=1;
 surface_close(&reconnect,&b);need(!reconnect.data_wheel,"a controller reconnect returns the wheel to scrub mode");
 raw_data_wheel=0;snd_midi_event_free(codec);free(c);
#if X_TOUCH_BLOCKING_ENABLED
#define CENTRE_PRESS_CLAIM "with the centre button publishing the data wheel's push as one JOG_PRESS request per press against the same single flight, "
#else
#define CENTRE_PRESS_CLAIM "with the centre button compiled out of this shipped-configuration build, "
#endif
 puts("PASS X-Touch Scrub selects data-wheel mode: note 101 classification, footswitch 1/2 as Play relay and Record toggle, press dedup, Scrub/Zoom LED sense, unchanged scrub and Shift routing, signed accumulation and cancellation, one bounded request per pump with single flight and a banked remainder, dispatch/step/return settlement, oversized-step refusal, cursor cluster in both modes " CENTRE_PRESS_CLAIM "reload clearing and reconnect reset (native calls and ALSA hardware substituted)");
}
static void stop_type_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"Stop/Type state");MirrorInput in;MirrorBank b;MIRROR_COPY(s);reset_fixture(c,&in,&b,&s,2);
 snd_midi_event_t *codec;need(!snd_midi_event_new(32,&codec),"Stop raw codec");snd_seq_addr_t addr={32,0};
 const unsigned char down[]={0x90,93,127},up[]={0x80,93,0},play[]={0x90,94,127},play_up[]={0x80,94,0};
 s.position_available=1;s.bar=7;s.position_tick=100;s.playing.bits=1;s.editor_owner=77;
 raw_input(codec,down,addr,&in,&b,&s,101);need(in.global_count==1&&in.global_events[0].operation==GLOBAL_STOP&&in.global_events[0].owner==77&&!in.stop_home,"Stop while playing queues direct native Stop");
 raw_input(codec,down,addr,&in,&b,&s,103);need(in.global_count==1,"held Stop creates no duplicate native intent");raw_input(codec,up,addr,&in,&b,&s,110);
 input_pump(&in,&b,&s,111);CommandRequest r;need(in.submitted==1&&command_request_read(c->slots,1,&r)&&r.reserved==GLOBAL_STOP&&r.global_owner==77,"direct Stop publishes through editor-owned command lane");
 reset_fixture(c,&in,&b,&s,2);s.position_available=1;s.bar=7;s.position_tick=100;s.playing.bits=0;s.editor_owner=77;
 raw_input(codec,down,addr,&in,&b,&s,1500);input_pump(&in,&b,&s,1501);need(in.submitted==1&&command_request_read(c->slots,1,&r)&&r.reserved==JOG_BAR&&(int32_t)r.bits==-8,"Stop while already stopped queues bar-derived home without native Stop");
 reset_fixture(c,&in,&b,&s,2);s.position_available=1;s.bar=5000;s.position_tick=100;s.editor_owner=77;
 raw_input(codec,down,addr,&in,&b,&s,101);raw_input(codec,up,addr,&in,&b,&s,110);raw_input(codec,down,addr,&in,&b,&s,250);input_stop_home(&in,&s,251);need(in.jog_count==1&&in.stop_home&&in.jog_events[0].delta==-4096,"large positions use bounded native chunks");raw_input(codec,up,addr,&in,&b,&s,251);raw_input(codec,play,addr,&in,&b,&s,252);need(!in.jog_count&&!in.stop_home,"Play cancels unsent home intent");
 need(in.global_count==1&&in.global_events[0].operation==GLOBAL_PLAY,"Play queues direct native Play");raw_input(codec,play,addr,&in,&b,&s,253);need(in.global_count==1,"held Play creates no duplicate native intent");raw_input(codec,play_up,addr,&in,&b,&s,254);
 raw_input(codec,play,addr,&in,&b,&s,255);raw_input(codec,play_up,addr,&in,&b,&s,256);raw_input(codec,down,addr,&in,&b,&s,257);need(in.global_count==3&&in.global_events[1].operation==GLOBAL_PLAY&&in.global_events[2].operation==GLOBAL_STOP&&!in.stop_home,"Play then Stop remains ordered before Playing readback catches up");
 input_stop_button(&in,&s,93,0,260);s.playing.available=0;input_stop_button(&in,&s,93,1,700);need(!in.stop_home,"unavailable transport cannot navigate");input_jog_discard(&in);need(!in.stop_down&&!in.stop_home,"project/reconnect reset clears gesture");
 s.selected_serial=1;s.editor_owner=77;Surface surface={0};in.jog_shift=1;surface_assignment(&surface,&in,&b,&s,40,1300);need(in.global_count==1&&in.global_events[0].operation==GLOBAL_TRACK_TYPE&&in.global_events[0].field==1,"Shift Track submits selected serial Type opener directly");
 s.selected_serial=2;input_pump(&in,&b,&s,1301);need(!in.global_count&&!in.submitted,"selected-track turnover discards unopened stale intent");
 reset_fixture(c,&in,&b,&s,2);s.selected_serial=0;s.editor_owner=77;in.jog_shift=0;
 const unsigned char instrument[]={0x90,45,127};snd_seq_event_t e;unsigned kind,note;int value;
 need(snd_midi_event_encode(codec,instrument,3,&e)==3,"Instrument raw MIDI");e.source=addr;
 need(surface_event(&e,addr,&kind,&note,&value)==1&&kind==6&&note==45&&value,"Instrument assignment button classification");
 unsigned assignment=b.assignment,view=b.view,flip=b.flip;surface_assignment(&surface,&in,&b,&s,note,101);
 need(in.global_count==1&&in.global_events[0].operation==GLOBAL_TRACK_NEW&&in.global_events[0].owner==77&&!in.global_events[0].field,"Add Track uses editor owner without selected track");
 need(b.assignment==assignment&&b.view==view&&b.flip==flip,"Add Track popup leaves mixer assignment intact");
 s.editor_owner=78;input_pump(&in,&b,&s,102);need(!in.global_count&&!in.submitted,"editor turnover discards unopened Add Track intent");
 reset_fixture(c,&in,&b,&s,2);s.editor_owner=77;surface=(Surface){.full=addr};
 const unsigned char undo[]={0x90,81,127},undo_up[]={0x80,81,0},shift[]={0x90,70,127};
 need(snd_midi_event_encode(codec,undo,3,&e)==3,"Undo raw MIDI");e.source=addr;need(surface_event(&e,addr,&kind,&note,&value)==1&&kind==11&&note==81&&value,"Undo direct classifier");surface_general_press(&surface,&in,&s,note,value,800);need(in.global_count==1&&in.global_events[0].operation==GLOBAL_UNDO,"Undo queues direct Editor history intent");
 need(snd_midi_event_encode(codec,undo_up,3,&e)==3,"Undo release MIDI");e.source=addr;need(surface_event(&e,addr,&kind,&note,&value)==1,"Undo release direct classifier");surface_general_press(&surface,&in,&s,note,value,801);
 raw_input(codec,shift,addr,&in,&b,&s,802);need(snd_midi_event_encode(codec,undo,3,&e)==3,"Shift Undo raw MIDI");e.source=addr;need(surface_event(&e,addr,&kind,&note,&value)==1,"Shift Undo direct classifier");surface_general_press(&surface,&in,&s,note,value,803);need(in.global_count==2&&in.global_events[1].operation==GLOBAL_REDO,"Shift+Undo queues direct Editor Redo intent");
 snd_midi_event_free(codec);free(c);puts("PASS direct Play/Stop/Undo/Redo actual MIDI classification and stopped-home/Type consumer policy; native bodies and USB are not simulated acceptance");
}
static void stop_relay_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"relay fixture state");MirrorInput in;MirrorBank b;MIRROR_COPY(s);reset_fixture(c,&in,&b,&s,2);s.position_available=1;s.bar=7;s.position_tick=100;
 /* Reproduce both old schedules with the actual old classifier boundary. */
 s.playing.bits=1;input_stop_button(&in,&s,93,1,101);need(!in.stop_home,"old bridge-first schedule stops in place");input_jog_discard(&in);s.playing.bits=0;input_stop_button(&in,&s,93,1,101);need(in.stop_home,"old adapter-first source update makes same press home");input_jog_discard(&in);
 Surface surface={.source=7,.stop_mpc={130,2}};s.playing.bits=1;stop_on_output=&s;unsigned before=output_count;
 need(surface_stop_relay(&surface,&in,&s,1,1,110)&&output_count==before+1&&!in.stop_home&&!s.playing.bits,"real relay decides before output can stop native source");
 need(surface_stop_relay(&surface,&in,&s,1,1,111)&&output_count==before+1,"held duplicate emits no second Stop");
 need(surface_stop_relay(&surface,&in,&s,1,0,112),"relay release");need(surface_stop_relay(&surface,&in,&s,1,1,900)&&in.stop_home,"later stopped press requests home without timer");
 input_jog_discard(&in);before=output_count;need(surface_stop_relay(&surface,&in,&s,0,1,1000)&&output_count==before+1&&!in.stop_home,"missing copied state still forwards ordinary Stop");
 surface_stop_relay(&surface,&in,&s,0,0,1001);input_stop_button(&in,&s,94,1,1010);surface_stop_relay(&surface,&in,&s,1,1,1011);need(!in.stop_home&&!in.stop_play_wait,"Play intent fences and is consumed by this Stop");
 surface_stop_relay(&surface,&in,&s,1,0,1012);surface_stop_relay(&surface,&in,&s,1,1,1013);need(in.stop_home,"later stopped Stop works even if transient Playing1 was never sampled");
 input_jog_discard(&in);stop_on_output=NULL;free(c);puts("PASS original race schedules and ordered relay with state update during actual output; native state update/ALSA substituted");
}
static void master_policy_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"master input fixture");MirrorInput in;MirrorBank bank;MIRROR_COPY(s);reset_fixture(c,&in,&bank,&s,2);
 s.master=(CopiedField){.property=0x800a8,.owner_incarnation=100,.incarnation=101,.bits=0x3f000000,.revision=2,.available=1,.seed=1};
 snd_midi_event_t *codec;need(!snd_midi_event_new(16,&codec),"master raw MIDI codec");snd_seq_addr_t full={32,0};unsigned char down[]={0x90,112,127},pitch[]={0xe8,0,96},travel[]={0xe8,0,80},up[]={0x90,112,0};unsigned outputs=output_count;
 raw_input(codec,down,full,&in,&bank,&s,101);raw_input(codec,pitch,full,&in,&bank,&s,102);input_pump(&in,&bank,&s,102);
 need(in.master.held&&!in.master.pending&&!in.submitted&&input_master_blocked(&in)&&output_count==outputs,"first touched master pitch establishes stale physical baseline without source write or echo");
 raw_input(codec,down,full,&in,&bank,&s,102);raw_input(codec,pitch,full,&in,&bank,&s,102);input_pump(&in,&bank,&s,102);need(!in.submitted,"duplicate touch and stationary baseline do not create master travel");
 raw_input(codec,travel,full,&in,&bank,&s,103);need(in.master.pending,"later touched master travel is admitted");input_pump(&in,&bank,&s,103);
 CommandRequest r;need(command_request_read(c->slots,1,&r)&&r.reserved==CF_MASTER&&r.global_owner==100&&r.field_incarnation==101&&!r.serial&&!r.binding&&!r.incarnation&&!r.track_owner&&!r.program_owner,"master request uses actual global owner, never a fabricated Track identity");
 raw_input(codec,up,full,&in,&bank,&s,104);need(input_master_blocked(&in),"call submission and touch release never certify master outcome");
 reset_fixture(c,&in,&bank,&s,9);s.master=(CopiedField){.property=0x800a8,.owner_incarnation=100,.incarnation=101,.bits=0x3f000000,.revision=2,.available=1,.seed=1};
 raw_input(codec,down,full,&in,&bank,&s,101);raw_input(codec,pitch,full,&in,&bank,&s,102);raw_input(codec,travel,full,&in,&bank,&s,102);input_bank_discard(&in);bank_navigate(&bank,1,103);input_pump(&in,&bank,&s,104);need(in.submitted==1,"bank remap preserves global master intent");
 reset_fixture(c,&in,&bank,&s,2);s.master=(CopiedField){.property=0x800a8,.owner_incarnation=100,.incarnation=101,.bits=0x3f000000,.revision=2,.available=1,.seed=1};raw_input(codec,down,full,&in,&bank,&s,101);raw_input(codec,pitch,full,&in,&bank,&s,102);raw_input(codec,travel,full,&in,&bank,&s,102);s.epoch++;input_pump(&in,&bank,&s,103);need(!in.submitted&&!in.master.valid&&!in.master.pending,"reload cancels unsent held master and forbids retargeting");
 raw_input(codec,pitch,full,&in,&bank,&s,104);need(!in.master.pending,"held old master remains invalid until physical release");raw_input(codec,up,full,&in,&bank,&s,105);raw_input(codec,pitch,full,&in,&bank,&s,106);need(in.master.pending,"fresh touchless movement binds current master after release");input_master_discard(&in);need(!in.master.held&&!in.master.pending&&!in.master.valid,"disconnect drops unsent master state");
 snd_midi_event_free(codec);free(c);puts("PASS raw master E8/touch112 stale baseline, later held travel and touchless input to typed request, no echo, pending motor suppression, bank independence and reload/disconnect cancellation (source snapshot/ALSA output substituted)");
}
static void drum_surface_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"pad surface command state");MirrorInput in;MirrorBank b;MIRROR_COPY(s);reset_fixture(c,&in,&b,&s,1);s.selection.bits=s.tracks[0].track;s.selected_serial=s.tracks[0].serial;s.pad_count=128;
 for(unsigned i=0;i<128;i++){CopiedTrack *p=s.pads+i;*p=s.tracks[0];p->pad_owner=100+i;p->pad_index=i;p->pad_generation=1;memset(p->fields,0,sizeof(p->fields));for(unsigned f=0;f<PF_COUNT;f++){unsigned field=f==PF_VOLUME?CF_VOLUME:f==PF_PAN?CF_PAN:f==PF_MUTE?CF_MUTE:f==PF_SOLO?CF_SOLO:CF_SOLO_AUDIO;p->fields[field]=(CopiedField){.available=1,.incarnation=p->pad_owner*PF_COUNT+f,.bits=f<PF_MUTE?0x3f000000:0,.revision=2,.seed=1};}}
 Surface surface={0};in.jog_shift=0;surface_assignment(&surface,&in,&b,&s,66,100);bank_apply(&b,&s,101);need(b.view==BV_DRUM_PADS&&b.strips[0].pad_owner==100,"Aux enters distinct selected Drum pad view");
 in.jog_shift=0;surface_assignment(&surface,&in,&b,&s,42,101);bank_apply(&b,&s,102);need(b.view==BV_DRUM_PADS&&b.assignment==BA_PAN,"Pan stays in pad view");surface_assignment(&surface,&in,&b,&s,50,102);bank_apply(&b,&s,103);need(bank_field(&b,0)==CF_PAN&&bank_encoder_field(&b,0)==CF_VOLUME&&b.faders[0].identity.pad_owner==100,"pad Flip preserves Instrument identity and exchanges real parameters");
 surface.next_wire=0;need(channel_output(&surface,&b,&s,s.heartbeat),"actual pad LCD output encoder");need(!memcmp(observed_lcd,"A01",3)&&observed_led[24]==127&&!observed_led[0]&&observed_led[42]&&!observed_led[40]&&observed_led[66]&&!observed_led[63]&&!observed_led[65],"pad label plus Pan and parent Track Select LED; no pad arm feedback");
 for(unsigned j=0;j<15;j++){surface_navigation(&in,&b,&s,1,103+j);}
 bank_apply(&b,&s,120);need(b.strips[7].pad_index==127,"eight-strip bank buttons reach H16");
 input_control(&in,&b,&s,0,CF_SELECTION,0,1,120);input_control(&in,&b,&s,0,CF_ARM,0,1,120);need(!in.desires[0][CF_SELECTION].pending&&!in.edge_count,"unqualified pad selection and record arm issue no command");
 surface_assignment(&surface,&in,&b,&s,40,121);bank_apply(&b,&s,122);need(b.view==BV_TRACK&&b.assignment==BA_TRACK&&!b.strips[0].pad_owner,"Track returns unchanged normal mixer");
 surface_assignment(&surface,&in,&b,&s,65,123);need(b.view==BV_INSTRUMENT,"unshifted Instruments retains existing Track filter");in.jog_shift=1;surface_assignment(&surface,&in,&b,&s,63,124);need(b.view==BV_INPUT,"Shift Input retains diagnostic input filter");surface_assignment(&surface,&in,&b,&s,65,125);need(b.view==BV_DRUM_PADS,"Shift Instruments remains a Drum Mix alias");
 const float positions[]={0.707945764f,1.0f,0.5f,0.0f,0.0028f};const char *texts[]={"0.00   ","+6.00  ","-6.04  ","-INF   ","-INF   "};
 for(unsigned k=0;k<5;k++){memcpy(&s.tracks[0].bits,positions+k,4);ChannelWire w=channel_wire(&s,s.tracks,CF_VOLUME);need(!memcmp(w.value,texts[k],7),"native dB default/max/half/zero/floor LCD values");}

 /* Sparse serials and an excluded Return establish the same playable ordinal
  * used by Channel navigation: the ninth playable parent is bank2/slot1. */
 snapshot_fixture(&s,10);s.tracks[0].vptr=0x6931f70;for(unsigned i=0;i<10;i++)s.tracks[i].serial=200+i*3;
 s.selection.bits=s.tracks[9].track;s.selected_serial=s.tracks[9].serial;s.tracks[9].vptr=0x6932a48;s.pad_count=0;s.heartbeat=200;
 b.view=BV_DRUM_PADS;bank_apply(&b,&s,200);for(unsigned i=0;i<8;i++)need(channel_output(&surface,&b,&s,200),"parent Select output for non-Drum blank pad view");
 for(unsigned i=0;i<8;i++)need(observed_led[24+i]==(i==0?127u:0u),"ninth playable parent uses fixed-bank slot1 rather than serial or pad index");
 for(unsigned i=0;i<112;i++)need(observed_lcd[i]==' ',"non-Drum parent retains blank pad labels and values");
 s.selection.bits=s.tracks[8].track;s.selected_serial=s.tracks[8].serial;s.heartbeat=210;bank_apply(&b,&s,210);for(unsigned i=0;i<8;i++)need(channel_output(&surface,&b,&s,210),"parent selection output refresh");
 for(unsigned i=0;i<8;i++)need(observed_led[24+i]==(i==7?127u:0u),"eighth playable parent uses slot8");
 b.offset=120;for(unsigned i=0;i<8;i++)need(channel_output(&surface,&b,&s,220),"pad offset presentation refresh");need(observed_led[31]==127,"pad bank offset cannot move parent Select LED");
 s.selection.available=0;for(unsigned i=0;i<8;i++)need(channel_output(&surface,&b,&s,220),"unavailable parent selection clears output");for(unsigned i=0;i<8;i++)need(!observed_led[24+i],"missing parent selection clears all Select LEDs");
 s.selection.available=1;for(unsigned i=0;i<8;i++)need(channel_output(&surface,&b,&s,221),"parent indication returns from copied selection");bank_drop(&b,222);need(channel_output(&surface,&b,&s,222),"unavailable DrumMix source clears parent indication immediately");for(unsigned i=0;i<8;i++)need(!observed_led[24+i],"unavailable bank clears all parent LEDs");
 surface_assignment(&surface,&in,&b,&s,40,230);s.heartbeat=230;bank_apply(&b,&s,230);for(unsigned i=0;i<8;i++)need(channel_output(&surface,&b,&s,230),"normal Track Select feedback restored");need(observed_led[31]==127,"normal Track bank retains existing selected-strip LED");
 free(c);puts("PASS production Drum Mix mode/Pan/Flip/16 banks/LCD/unsupported controls/normal Track return; source snapshot and ALSA device substituted");
}
/* Channel navigation remains Track-owned even when its pad projection is empty. */
static void drum_channel_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"drum navigation state");MirrorInput in;MirrorBank b;MIRROR_COPY(s);Surface surface={0};CommandRequest r;
 reset_fixture(c,&in,&b,&s,3);s.tracks[1].vptr=0x693147c;s.selection.bits=s.tracks[0].track;s.selected_serial=1;
 s.pad_count=1;s.pads[0]=s.tracks[0];s.pads[0].pad_owner=100;s.pads[0].pad_generation=1;
 surface_assignment(&surface,&in,&b,&s,66,101);bank_apply(&b,&s,102);
 surface_navigation(&in,&b,&s,3,103);input_pump(&in,&b,&s,104);
 need(command_request_read(c->slots,1,&r)&&r.reserved==CF_SELECTION&&r.serial==2&&!r.pad_owner,"Drum Mix Channel+ targets next non-Drum Track with normal selection command");
 complete(c,1,105);s.selection.bits=s.tracks[1].track;s.selected_serial=2;s.selection.revision=4;s.pad_count=0;s.heartbeat=106;bank_apply(&b,&s,106);input_pump(&in,&b,&s,106);
 need(in.settled==1&&b.view==BV_DRUM_PADS&&b.ready&&!b.playable_count,"source selection settles while Drum Mix stays open and empty");
 reclaim_fixture(c,1);input_pump(&in,&b,&s,107);s.heartbeat=120;bank_apply(&b,&s,120);
 for(unsigned j=0;j<8;j++){need(b.faders[j].empty&&bank_due(&b,j,120)==0,"non-Drum parks each empty pad fader");need(channel_output(&surface,&b,&s,s.heartbeat),"empty pad presentation");}
 need(!memcmp(observed_lcd,"                                                        ",56)&&!memcmp(observed_lcd+56,"                                                        ",56),"non-Drum clears all eight pad names and values");
 surface_navigation(&in,&b,&s,3,121);input_pump(&in,&b,&s,122);
 need(command_request_read(c->slots,2,&r)&&r.serial==3&&!r.pad_owner,"Channel+ works from empty pad view");
 complete(c,2,123);s.selection.bits=s.tracks[2].track;s.selected_serial=3;s.selection.revision=6;s.pad_count=1;s.pads[0]=s.tracks[2];s.pads[0].pad_owner=101;s.pads[0].pad_generation=1;s.heartbeat=124;bank_apply(&b,&s,124);input_pump(&in,&b,&s,124);
 need(in.settled==2&&b.view==BV_DRUM_PADS&&b.strips[0].pad_owner==101&&!b.offset,"next Drum Track restores its own pad bank after native selection");
 reclaim_fixture(c,2);input_pump(&in,&b,&s,125);surface_navigation(&in,&b,&s,2,126);input_pump(&in,&b,&s,127);need(command_request_read(c->slots,3,&r)&&r.serial==2,"Channel- navigates back from Drum pad view");
 free(c);puts("PASS Drum Mix native-selection policy across Drum/plugin/Drum, empty LCD/parked motors and reverse selection; native/ALSA substituted");
}
/* Runs the same two-stage strip eligibility/send path as the main loop.
 * File/process/USB discovery and snapshots are substitutes, not motor policy. */
static void fixture_motor_pass(Surface *surface,MirrorBank *bank,CopiedMirror *snapshot,uint32_t now){
 bank_apply(bank,snapshot,now);
 need(channel_output(surface,bank,snapshot,now),"master/presentation production output pass");
 for(unsigned i=0;i<MIRROR_BANK;i++)if(strip_motor_ready(surface,bank,i,now))need(strip_motor_output(surface,bank,i,now)>=0,"production strip output pass");
}
static void binding_sync_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"binding sync command fixture");MirrorBank bank;MIRROR_COPY(snapshot);Surface surface={.source=3,.full={32,0}};
 reset_fixture(c,&input,&bank,&snapshot,0);snapshot.ready=0;snapshot.epoch=0;bank_apply(&bank,&snapshot,100);
 memset(observed_pitch_count,0,sizeof(observed_pitch_count));snapshot.heartbeat=110;fixture_motor_pass(&surface,&bank,&snapshot,110);snapshot.heartbeat=111;fixture_motor_pass(&surface,&bank,&snapshot,111);
 for(unsigned i=0;i<9;i++)need(observed_pitch_count[i]==1&&observed_pitch[i]==0&&following.motors[i].last_sent==0,"chooser establishes actual emitted zero");
 bank_touch(&bank,0,1,115);input.desires[1][CF_VOLUME].pending=1;surface.master_touch=1;following.enabled=0;
 snapshot_fixture(&snapshot,16);snapshot.master=(CopiedField){.available=1,.bits=0x3f000000,.owner_incarnation=700,.incarnation=701,.revision=2};
 for(unsigned i=8;i<16;i++)snapshot.tracks[i].bits=0x3e800000;
 snapshot.heartbeat=120;fixture_motor_pass(&surface,&bank,&snapshot,120);snapshot.heartbeat=130;fixture_motor_pass(&surface,&bank,&snapshot,130);
 for(unsigned i=0;i<9;i++)need(observed_pitch_count[i]==1&&following.motors[i].last_sent==0,"fresh binding preserves known origin and Off suppresses final synchronization");
 following.enabled=1;motor_follow_report(following.motors+3,0,135,1);snapshot.heartbeat=140;fixture_motor_pass(&surface,&bank,&snapshot,140);
 for(unsigned i=0;i<9;i++){
  if(i==0||i==1||i==3||i==8)need(observed_pitch_count[i]==1,"fresh-binding sync respects touch, pending, observed travel quiet and held master");
  else need(observed_pitch_count[i]==2&&observed_pitch[i]==8192,"chooser-to-template sends one final target instead of new software staircase");
 }
 bank_touch(&bank,0,0,145);input.desires[1][CF_VOLUME].pending=0;surface.master_touch=0;snapshot.heartbeat=150;fixture_motor_pass(&surface,&bank,&snapshot,150);
 for(unsigned i=0;i<9;i++)if(i!=3)need(observed_pitch_count[i]==2&&observed_pitch[i]==8192,"each eligible channel and master synchronizes once after protection clears");
 snapshot.heartbeat=300;fixture_motor_pass(&surface,&bank,&snapshot,300);snapshot.heartbeat=310;fixture_motor_pass(&surface,&bank,&snapshot,310);
 for(unsigned i=0;i<9;i++)need(observed_pitch_count[i]==2&&observed_pitch[i]==8192,"constant project target produces no trailing staircase after one final sync");
 for(unsigned i=0;i<8;i++)need(!strip_motor_ready(&surface,&bank,i,310),"idle production eligibility gate skips per-channel refresh work");
 /* Native r3 idle defect: a bank reset cleared its sent position while the
  * physical motor owner retained the same identity and emitted target. */
 bank_drop(&bank,311);snapshot.heartbeat=314;bank_apply(&bank,&snapshot,314);snapshot.heartbeat=315;bank_apply(&bank,&snapshot,315);
 for(unsigned i=0;i<8;i++){
  need(bank.faders[i].last_sent==-1&&!following.motors[i].sync&&following.motors[i].target==following.motors[i].last_sent,"reproduce observed split bank/motor sent-state after same-identity drop");
  need(bank_due(&bank,i,315)==8192&&!strip_motor_ready(&surface,&bank,i,315),"bank alone appears due but actual early motor gate rejects redundant copy work");
  need(bank.faders[i].last_sent==-1,"readiness does not fake bank output acknowledgement");
 }
 unsigned idle_messages=output_count;
 for(unsigned tick=316;tick<320;tick++)for(unsigned i=0;i<8;i++)need(!strip_motor_ready(&surface,&bank,i,tick),"repeated idle iterations stay in cheap negative gate");
 need(output_count==idle_messages&&!atomic_load(&c->published),"idle readiness emits neither motor echo nor musical command");
 bank_navigate(&bank,1,320);snapshot.heartbeat=330;fixture_motor_pass(&surface,&bank,&snapshot,330);snapshot.heartbeat=340;fixture_motor_pass(&surface,&bank,&snapshot,340);
 for(unsigned i=0;i<8;i++)need(observed_pitch_count[i]==3&&observed_pitch[i]==4096,"new bank sends exactly one final target per channel");
 need(observed_pitch_count[8]==2&&!atomic_load(&c->published),"bank sync neither moves unchanged master nor issues native input");
 for(unsigned i=8;i<16;i++)snapshot.tracks[i].bits=0x3f400000;
 snapshot.heartbeat=345;fixture_motor_pass(&surface,&bank,&snapshot,345);for(unsigned i=0;i<8;i++)need(observed_pitch_count[i]==3,"production same-binding update respects 10ms output ceiling");
 for(unsigned i=8;i<16;i++)snapshot.tracks[i].bits=0x3f600000;
 snapshot.heartbeat=350;bank_apply(&bank,&snapshot,350);for(unsigned i=0;i<8;i++)need(strip_motor_ready(&surface,&bank,i,350),"next iteration source change immediately reopens final-validation path");
 fixture_motor_pass(&surface,&bank,&snapshot,350);snapshot.heartbeat=360;fixture_motor_pass(&surface,&bank,&snapshot,360);
 for(unsigned i=0;i<8;i++)need(observed_pitch_count[i]==4&&observed_pitch[i]==14335,"production output coalesces to latest authoritative source and never emits intermediate positions");
 memset(&input,0,sizeof(input));free(c);following.enabled=1;motor_follow_reset(&following);
 puts("PASS production chooser-to-project/bank final-target sync for eight strips and master; known zero retained, no staircase, touch/pending/quiet/Off precedence; source/process/USB/ALSA substituted");
}
static void binding_reversal_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"binding reversal command fixture");MirrorBank bank;MIRROR_COPY(snapshot);Surface surface={.source=3,.full={32,0}};
 reset_fixture(c,&input,&bank,&snapshot,16);for(unsigned i=0;i<16;i++)snapshot.tracks[i].bits=i<8?0:0x3f800000;
 memset(observed_pitch_count,0,sizeof(observed_pitch_count));snapshot.heartbeat=110;fixture_motor_pass(&surface,&bank,&snapshot,110);
 for(unsigned turn=1;turn<=6;turn++){
  unsigned tick=100+turn*50;bank_navigate(&bank,turn%2?1:-1,tick);snapshot.heartbeat=tick;fixture_motor_pass(&surface,&bank,&snapshot,tick);snapshot.heartbeat=tick+11;fixture_motor_pass(&surface,&bank,&snapshot,tick+11);
  for(unsigned i=0;i<8;i++){
   if(turn<5)need(observed_pitch_count[i]==turn+1&&observed_pitch[i]==(turn%2?16383:0),"ordinary fresh bank targets synchronize directly before sustained reversal threshold");
   else need(observed_pitch_count[i]==5&&following.motors[i].hold_until==tick+250,"four meaningful opposite fresh-binding reversals hold physical motors and later reversal extends hold");
  }
 }
 snapshot.heartbeat=649;fixture_motor_pass(&surface,&bank,&snapshot,649);for(unsigned i=0;i<8;i++)need(observed_pitch_count[i]==5,"new binding sync cannot bypass active reversal hold");
 snapshot.heartbeat=650;fixture_motor_pass(&surface,&bank,&snapshot,650);for(unsigned i=0;i<8;i++)need(observed_pitch_count[i]==6&&observed_pitch[i]==0,"expired bounded hold resumes only latest fresh binding target");
 need(!atomic_load(&c->published),"physical reversal hold creates no native request");memset(&input,0,sizeof(input));free(c);following.enabled=1;motor_follow_reset(&following);
 puts("PASS production bank reversal protection: repeated opposite fresh bindings retain and extend physical hold, then synchronize latest target; source/process/USB/ALSA substituted");
}
static void input_page_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"Input I/O fixture");MirrorInput in;MirrorBank bank;MIRROR_COPY(snapshot);Surface surface={.source=3,.full={32,0}};
 reset_fixture(c,&in,&bank,&snapshot,8);surface_assignment(&surface,&in,&bank,&snapshot,41,101);bank_apply(&bank,&snapshot,102);
 in.desires[0][CF_VOLUME].pending=1;surface_assignment(&surface,&in,&bank,&snapshot,63,103);
 need(bank.assignment==BA_IO&&bank.view==BV_TRACK&&!bank.flip&&!in.desires[0][CF_VOLUME].pending&&!atomic_load(&c->published),"Input enters dedicated I/O page and cancels prior unsent Send intent");
 bank_apply(&bank,&snapshot,104);
 for(unsigned i=0;i<8;i++)need(channel_output(&surface,&bank,&snapshot,105),"I/O actual unavailable-field output");
 need(!memcmp(observed_lcd+56,"N/A    ",7)&&observed_led[63],"unqualified I/O source displays unavailable while dedicated Input mode is active");
 snapshot.io_available=1;snapshot.io.status=IO_READY;snapshot.io.serial=bank.selected.serial;
 snapshot.io.fields[4]=(IOField){.status=IO_READY,.tick=105,.choice_count=56,.choice_index=3};strcpy(snapshot.io.fields[4].text,"Submix 4");
 surface.next_wire=0;for(unsigned i=0;i<8;i++)need(channel_output(&surface,&bank,&snapshot,105),"I/O native named destination output");
 need(!memcmp(observed_lcd+4*7,"AudOut ",7)&&!memcmp(observed_lcd+56+4*7,"Sub 4  ",7),"I/O field label stays visible and Submix number survives LCD encoding");
 snapshot.selected_serial=snapshot.tracks[1].serial;bank_apply(&bank,&snapshot,106);surface.next_wire=0;
 for(unsigned i=0;i<8;i++)need(channel_output(&surface,&bank,&snapshot,106),"I/O output after selecting different track");
 need(!memcmp(observed_lcd+4*7,"AudOut ",7)&&!memcmp(observed_lcd,"Monitr ",7),"track selection never replaces I/O labels with a temporary track name");
 snapshot.io_available=0;
 surface_assignment(&surface,&in,&bank,&snapshot,50,106);need(!bank.flip&&bank.assignment==BA_IO,"I/O Flip does not move routing to track-volume faders");
 for(unsigned i=0;i<8;i++){surface_encoder(&surface,&in,&bank,&snapshot,i,1,0,107);surface_encoder(&surface,&in,&bank,&snapshot,i,0,1,108);need(!in.io_delta[i]&&bank_field(&bank,i)==CF_VOLUME,"unavailable routing and encoder pushes produce no guessed request; faders remain track volume");}
 surface_assignment(&surface,&in,&bank,&snapshot,66,2105);need(bank.view==BV_DRUM_PADS,"unshifted Aux enters Drum Mix");surface_assignment(&surface,&in,&bank,&snapshot,66,2106);need(bank.view==BV_TRACK,"unshifted Aux toggles back to Track");
 free(c);following.enabled=1;motor_follow_reset(&following);
 puts("PASS Input I/O entry, unavailable fields and no guessed push/Flip routing, with normal track faders and Aux Drum toggle (copied source/ALSA substituted)");
}
static void qlink_mode_routing_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"mode routing command fixture");MirrorBank bank;MIRROR_COPY(snapshot);Surface surface={.source=3,.full={32,0}};
 reset_fixture(c,&input,&bank,&snapshot,16);surface_assignment(&surface,&input,&bank,&snapshot,44,101);
 snapshot.qlinks_available=1;snapshot.qlinks=(QLinkCopy){.root=0x71000,.mode_valid=1,.mode_controller=0x72000,.mode_generation=3,.mode_id=8,.mode_count=14,.mode_revision=2,.mode_tick=110};snapshot.heartbeat=110;bank_apply(&bank,&snapshot,110);
 need(bank.assignment==BA_QLINK&&!bank.flip&&!bank.qlink_page,"actual EQ enters first Q-Link bank");
 surface_navigation(&input,&bank,&snapshot,1,111);need(bank.qlink_page==1&&!input.mode_delta&&!atomic_load(&c->published),"ordinary Bank Right selects logical9-16 without native mode command");
 snapshot.heartbeat=112;bank_apply(&bank,&snapshot,112);surface_assignment(&surface,&input,&bank,&snapshot,50,113);need(bank.flip,"ordinary Flip retained on Q-Link page");
 surface_jog(&input,&snapshot,7,70,1,114,0);unsigned page=bank.qlink_page,offset=bank.offset;
 input.qlinks[3].pending=1;surface_navigation(&input,&bank,&snapshot,1,115);surface_navigation(&input,&bank,&snapshot,1,116);surface_navigation(&input,&bank,&snapshot,0,117);
 need(input.mode_delta==1&&bank.qlink_page==page&&bank.offset==offset&&bank.flip&&!input.qlinks[3].pending&&!atomic_load(&c->published),"actual Shift Bank coalesces signed presses without changing bank/Flip; revokes only unsent Q-Link value");
 surface_navigation(&input,&bank,&snapshot,0,118);need(!input.mode_delta&&!atomic_load(&c->published),"opposite unsent mode directions cancel");
 surface_navigation(&input,&bank,&snapshot,0,119);surface_navigation(&input,&bank,&snapshot,0,120);snapshot.heartbeat=121;bank_apply(&bank,&snapshot,121);
 input_mode_submit(&input,&bank,&snapshot,121);CommandRequest r;
 need(command_request_read(c->slots,1,&r)&&r.reserved==QLINK_MODE&&(int32_t)r.bits==-2&&r.global_owner==snapshot.qlinks.mode_controller&&r.field_incarnation==3&&r.before_bits==8,"physical surface directions publish signed count with native controller identity, not a guessed selected ID");
 need(input.flights[0].flight==1&&!input.mode_delta,"published mode request retains existing flight owner");
 surface_jog(&input,&snapshot,7,70,0,122,0);surface_navigation(&input,&bank,&snapshot,0,123);
 need(bank.qlink_page==0&&bank.flip&&input.flights[0].flight==1&&!input.mode_delta,"ordinary bank switch after Shift release preserves published mode obligation and Flip");
 snapshot.heartbeat=124;bank_apply(&bank,&snapshot,124);surface_jog(&input,&snapshot,7,70,1,125,0);snapshot.qlinks.mode_valid=0;surface_navigation(&input,&bank,&snapshot,1,126);
 need(!input.mode_delta&&atomic_load(&c->published)==1,"unavailable mode copy never creates a mode request");
 memset(&input,0,sizeof(input));free(c);following.enabled=1;motor_follow_reset(&following);
 puts("PASS actual EQ/Shift/Bank/Flip surface routing and signed mode coalescing/publication; bank-only navigation, cancellation and retained flight; native selected mode/callbacks not simulated here");
}
static void following_reenable_checks(void){
 char directory[]="/tmp/mpclearn-resync.XXXXXX",path[256];need(mkdtemp(directory)!=NULL,"re-enable preference directory");snprintf(path,sizeof(path),"%s/surface-preferences",directory);
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"re-enable command fixture");MirrorBank bank;MIRROR_COPY(snapshot);Surface surface={.source=3,.full={32,0}};
 reset_fixture(c,&input,&bank,&snapshot,8);snapshot.master=(CopiedField){.available=1,.bits=0x3f000000,.owner_incarnation=700,.incarnation=701,.revision=2};
 memset(observed_pitch_count,0,sizeof(observed_pitch_count));snapshot.heartbeat=110;fixture_motor_pass(&surface,&bank,&snapshot,110);
 for(unsigned i=0;i<9;i++)need(observed_pitch_count[i]==1&&observed_pitch[i]==8192,"all channels establish initial authoritative output through production gates");
 preferences_path=path;input.jog_shift=1;surface_assignment(&surface,&input,&bank,&snapshot,50,120);need(!following.enabled,"actual Shift Flip turns following off");
 for(unsigned i=0;i<9;i++){motor_follow_report(following.motors+i,2000+300*i,125,1);if(i<8)bank.faders[i].physical=2000+300*i;}
 snapshot.heartbeat=130;fixture_motor_pass(&surface,&bank,&snapshot,130);
 for(unsigned i=0;i<9;i++)need(observed_pitch_count[i]==1,"physical travel while Off emits no motor response");
 surface_assignment(&surface,&input,&bank,&snapshot,50,140);need(following.enabled,"actual Shift Flip restores following");fixture_motor_pass(&surface,&bank,&snapshot,140);
 for(unsigned i=0;i<9;i++)need(observed_pitch_count[i]==1,"On waits for post-toggle source heartbeat");
 snapshot.heartbeat=150;fixture_motor_pass(&surface,&bank,&snapshot,150);
 for(unsigned i=0;i<9;i++)need(observed_pitch_count[i]==1,"On preserves observed manual travel quiet for every motor");
 bank_touch(&bank,0,1,160);input.desires[1][CF_VOLUME].pending=1;input.master.pending=1;snapshot.heartbeat=300;fixture_motor_pass(&surface,&bank,&snapshot,300);
 need(observed_pitch_count[0]==1&&observed_pitch_count[1]==1&&observed_pitch_count[8]==1,"re-enable preserves channel touch and channel/master pending suppression");
 for(unsigned i=2;i<8;i++)need(observed_pitch_count[i]==2&&observed_pitch[i]==8192,"unchanged authoritative target catches up directly through bank_due without intermediates");
 bank_touch(&bank,0,0,310);input.desires[1][CF_VOLUME].pending=0;input.master.pending=0;surface.master_touch=1;snapshot.heartbeat=320;fixture_motor_pass(&surface,&bank,&snapshot,320);
 need(observed_pitch_count[0]==2&&observed_pitch_count[1]==2&&observed_pitch_count[8]==1,"released channels resume while held master remains protected");
 surface.master_touch=0;
 for(unsigned now=330;now<2400;now+=10){snapshot.heartbeat=now;fixture_motor_pass(&surface,&bank,&snapshot,now);}
 for(unsigned i=0;i<9;i++)need(observed_pitch_count[i]>1&&observed_pitch[i]==8192,"all nine motors reacquire exact unchanged source after Off travel");
 need(!atomic_load(&c->published)&&snapshot.master.bits==0x3f000000,"follow toggle/output creates no native request or source value");
 for(unsigned i=0;i<8;i++)need(snapshot.tracks[i].bits==0x3f000000,"track source remains unchanged across following toggle");
 need(!unlink(path)&&!rmdir(directory),"re-enable preference cleanup");preferences_path=NULL;preference_notice_until=0;memset(&input,0,sizeof(input));free(c);following.enabled=1;motor_follow_reset(&following);
 puts("PASS production strip eligibility/send loop and master output: Off travel -> On fresh-source catch-up, all nine motors, manual quiet and touch/pending gates; source/process/USB/ALSA substituted");
}
static void milestone_checks(void){
 Surface surface={.source=3,.full={32,0}};following.enabled=1;motor_follow_reset(&following);
 for(unsigned channel=0;channel<9;channel++){
  uint32_t key[16]={channel+1};unsigned before=output_count;
  need(motor_output(&surface,channel,key,0,100,1)&&output_count==before+1&&last_output.data.control.channel==channel,"each of nine motors uses shared first sync");
  before=output_count;need(motor_output(&surface,channel,key,5000,105,1)&&output_count==before,"same-binding output preserves 10ms ceiling");
  need(motor_output(&surface,channel,key,16383,110,1)&&output_count==before+1&&last_output.data.control.value+8192==16383,"all nine motors receive latest source target without fabricated intermediates");
  need(motor_output(&surface,channel,key,16383,120,1)&&output_count==before+1,"unchanged same-binding target is deduplicated");
  motor_follow_report(following.motors+channel,8000,4000,1);before=output_count;
  need(motor_output(&surface,channel,key,0,4149,1)&&output_count==before,"manual travel quiet overrides source output");
  need(motor_output(&surface,channel,key,0,4150,0)&&output_count==before,"touch/pending gate overrides elapsed quiet");
 }
 MotorFollow m={.physical=-1,.last_sent=-1};uint32_t key[16]={42};motor_follow_target(&m,key,0,0);
 for(unsigned i=1;i<=6;i++)motor_follow_target(&m,key,i%2?10000:0,i*50);
 need(m.hold_until==550&&motor_follow_due(&m,400,1)<0,"four meaningful reversals suspend and continued reversal extends");
 motor_follow_target(&m,key,10000,400);need(m.hold_until==650,"oscillation extends during suspension");key[0]++;motor_follow_target(&m,key,4000,410);need(m.hold_until==660&&m.turn_count==4&&motor_follow_due(&m,659,1)<0,"binding change retains and extends physical reversal hold");
 following.enabled=0;unsigned before=output_count;for(unsigned i=0;i<9;i++)need(motor_output(&surface,i,key,0,5000,1),"off handles all nine targets");need(output_count==before,"off suppresses every motor including master parking");
 char directory[]="/tmp/mpclearn-surface.XXXXXX",path[256];need(mkdtemp(directory)!=NULL,"private preference fixture directory");snprintf(path,sizeof(path),"%s/surface-preferences",directory);unsigned enabled;
 need(surface_preferences_read(path,&enabled)&&enabled,"missing preference defaults on");need(surface_preferences_write(path,0)&&surface_preferences_read(path,&enabled)&&!enabled,"real atomic preference stores off");
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"milestone command fixture");MirrorInput in;MirrorBank bank;MIRROR_COPY(snapshot);reset_fixture(c,&in,&bank,&snapshot,2);
 following.enabled=0;preferences_path=path;in.jog_shift=1;unsigned assignment=bank.assignment,flip=bank.flip;
 surface_assignment(&surface,&in,&bank,&snapshot,50,5100);need(following.enabled&&surface_preferences_read(path,&enabled)&&enabled&&bank.assignment==assignment&&bank.flip==flip&&!atomic_load(&c->published),"Shift Flip persists motor intent without musical assignment or command changes");
 int fd=open(path,O_WRONLY|O_TRUNC);need(fd>=0&&write(fd,"broken",6)==6&&!close(fd),"malformed preference fixture");surface_assignment(&surface,&in,&bank,&snapshot,50,5200);need(following.enabled&&preference_notice_error&&!surface_preferences_read(path,&enabled),"malformed preference refuses toggle and never enables silently");
 preferences_path=NULL;need(!unlink(path)&&!rmdir(directory),"preference fixture cleaned");preference_notice_until=0;in.jog_shift=0;snapshot.editor_owner=500;
 snd_midi_event_t *codec;need(!snd_midi_event_new(16,&codec),"F key codec");
 for(unsigned i=0;i<8;i++){unsigned char bytes[]={0x90,54+i,127};snd_seq_event_t event;need(snd_midi_event_encode(codec,bytes,3,&event)==3,"F key raw MIDI");event.source=surface.full;unsigned kind,note;int down;need(surface_event(&event,surface.full,&kind,&note,&down)==1&&kind==11,"F key classification");surface_general_press(&surface,&in,&snapshot,note,down,5300);}
 need(in.global_count==8,"eight F keys enter existing native command queue");for(unsigned i=0;i<8;i++)need(in.global_events[i].operation==GLOBAL_PAGE_MAIN+i&&in.global_events[i].owner==500,"each F key preserves qualified Editor owner");
 snd_midi_event_free(codec);page_notice_until=0;free(c);following.enabled=1;motor_follow_reset(&following);
 puts("PASS actual bridge output policy across nine ALSA channels, bounded reversal/quiet/disabled output, real atomic preference files and F-key MIDI routing; hardware, source values and native Editor call substituted");
}
static void native_text_wire_checks(void){
 const struct {const char *native,*cell;} values[]={
  {"-12.81dB","-12.81 "},{"+0.19dB","+0.19  "},{"-0.99dB","-0.99  "},
  {"0.000001ms","1e-06  "},{"-0.000001ms","-1e-06 "},{"-inf dB","-infdB "},
  {"Off","Off    "},{"C","C      "},{"1L","1L     "},{"Ping Pong","PingP~ "}
 };
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){
  unsigned char cell[7],wire[15];channel_value(cell,values[i].native,strlen(values[i].native));
  need(!memcmp(cell,values[i].cell,7),"native numeric text preserves sign/decimal or explicit rounded precision; long enums mark truncation");
  channel_lcd(wire,3,1,cell);
  need(!memcmp(wire,(unsigned char[]){0xf0,0,0,0x66,0x14,0x12,77},7)&&!memcmp(wire+7,values[i].cell,7)&&wire[14]==0xf7,"actual lower-row MCU wire preserves all six value characters and separator");
 }
 const struct {const char *native,*cell;unsigned index;} labels[]={
  {"Out 1/2 (Volume)","1/2Vol ",0},{"Out 3/4 (Volume)","3/4Vol ",1},
  {"Out 1/2 (Pan)","1/2Pan ",2},{"Out 1/2 (Mute)","1/2Mut ",3},
  {"Out 10/11 (Volume)","10/11V ",4},{"Drum 001","Dru001 ",5},{"","Q16    ",15}
 };
 for(unsigned i=0;i<sizeof(labels)/sizeof(labels[0]);i++){
  unsigned char cell[7],wire[15];channel_qlink_label(cell,labels[i].native,strlen(labels[i].native),labels[i].index);
  need(!memcmp(cell,labels[i].cell,7),"Q-Link labels distinguish native output destinations/properties and retain empty logical-index fallback");
  channel_lcd(wire,6,0,cell);
  need(wire[6]==42&&!memcmp(wire+7,labels[i].cell,7)&&wire[14]==0xf7,"actual upper-row MCU wire retains qualified display-only Q-Link abbreviation");
 }
 puts("PASS native-observed Q-Link/Effects text formatter and LCD bytes: decimals/signs, tiny values, infinity, enums, output destinations/properties and empty Q16 (native strings supplied; no physical device)");
}
static uint32_t float_bits(float v){uint32_t b;memcpy(&b,&v,4);return b;}
/* Outbound cost policy. Three separate claims, each pinned in both
 * directions: a meter level that is already zero is not re-sent while every
 * nonzero level still is; a bank strip resolves its track through the row
 * bank_apply recorded, and revalidates it; and the playhead is formatted and
 * compared only when one of the four words channel_position reads changed. */
static void outbound_update_checks(void){
 CommandState *c=calloc(1,sizeof(*c));need(c!=NULL,"synthetic command memory");MirrorInput in;MirrorBank b;MIRROR_COPY(s);
 reset_fixture(c,&in,&b,&s,8);
 Surface surf={.source=3,.full={32,0}};
 for(unsigned i=0;i<8;i++)s.tracks[i].meter=(CopiedMeter){.incarnation=i+1,.available=1,.tick=100,.revision=2,.enabled=1};
 bank_apply(&b,&s,100);
 memset(observed_press_count,0,sizeof(observed_press_count));
 need(meter_output(&surf,&b,&s),"first meter output arms every enabled strip");
 for(unsigned i=0;i<8;i++)need(observed_press_count[i]==2&&!observed_press_level[i],"enabling a strip clears its overload and publishes its first level");
 s.heartbeat=200;memset(observed_press_count,0,sizeof(observed_press_count));
 need(meter_output(&surf,&b,&s),"silent cadence still runs");
 for(unsigned i=0;i<8;i++)need(!observed_press_count[i],"a level that is already zero is not re-sent: silence costs no USB traffic");
 s.tracks[0].meter.left=float_bits(0.5f);s.heartbeat=300;memset(observed_press_count,0,sizeof(observed_press_count));
 need(meter_output(&surf,&b,&s),"first audible cadence");
 need(observed_press_count[0]==1&&observed_press_level[0]==8,"a level that rose off zero is published");
 for(unsigned i=1;i<8;i++)need(!observed_press_count[i],"the other silent strips stay suppressed while one plays");
 s.heartbeat=400;memset(observed_press_count,0,sizeof(observed_press_count));
 need(meter_output(&surf,&b,&s),"sustained cadence");
 need(observed_press_count[0]==1&&observed_press_level[0]==8,"an unchanged NONZERO level is still refreshed every cadence, because the hardware decays it on its own");
 s.heartbeat=430;memset(observed_press_count,0,sizeof(observed_press_count));
 need(meter_output(&surf,&b,&s)&&!observed_press_count[0],"a tick inside the 50ms cadence sends nothing");
 s.tracks[0].meter.left=0;s.heartbeat=500;memset(observed_press_count,0,sizeof(observed_press_count));
 need(meter_output(&surf,&b,&s),"fall to silence");
 need(observed_press_count[0]==1&&!observed_press_level[0],"the fall to zero is published once so the bar drops");
 s.heartbeat=600;memset(observed_press_count,0,sizeof(observed_press_count));
 need(meter_output(&surf,&b,&s)&&!observed_press_count[0],"and the bar is then left down rather than re-zeroed every cadence");
 s.tracks[0].meter.incarnation=99;s.heartbeat=700;memset(observed_press_count,0,sizeof(observed_press_count));
 need(meter_output(&surf,&b,&s),"meter identity change");
 need(observed_press_count[0]==2&&!observed_press_level[0],"a new meter identity leaves the hardware level unknown and republishes it");
 s.tracks[0].meter.available=0;s.heartbeat=800;memset(observed_press_count,0,sizeof(observed_press_count));
 need(meter_output(&surf,&b,&s),"meter source lost");
 need(observed_press_count[0]==2&&!observed_press_level[0],"a lost meter source disables the strip and clears its bar");

 reset_fixture(c,&in,&b,&s,12);
 for(unsigned i=0;i<8;i++){
  need(b.strip_row[i]==(int)i,"an ordinary track bank records the snapshot row each strip was bound from");
  need(bank_strip_track(&s,&b,i)==input_track(&s,b.strips+i),"the recorded row resolves the same track the scan resolves");
 }
 b.offset=8;bank_apply(&b,&s,110);
 for(unsigned i=0;i<4;i++)need(b.strip_row[i]==(int)(8+i)&&bank_strip_track(&s,&b,i)==input_track(&s,b.strips+i),"a paged bank records its own rows");
 for(unsigned i=4;i<8;i++)need(b.strip_row[i]<0&&!bank_strip_track(&s,&b,i)&&!input_track(&s,b.strips+i),"a strip past the inventory has no row and resolves to nothing");
 reset_fixture(c,&in,&b,&s,12);
 CopiedTrack moved=s.tracks[0];s.tracks[0]=s.tracks[5];s.tracks[5]=moved;
 for(unsigned i=0;i<8;i++)need(bank_strip_track(&s,&b,i)==input_track(&s,b.strips+i),"rows that moved under an unapplied bank still resolve by identity");
 need(bank_strip_track(&s,&b,0)==s.tracks+5&&bank_strip_track(&s,&b,5)==s.tracks+0,"a stale row is rejected and the swapped identities resolve to their new rows");
 reset_fixture(c,&in,&b,&s,12);
 s.tracks[10].vptr=s.tracks[11].vptr=0x6931f70;
 for(unsigned i=0;i<2;i++){s.send_programs[i]=s.tracks[10+i].program;s.send_owners[i]=s.tracks[10+i].program_owner;}
 b.assignment=BA_SEND;bank_apply(&b,&s,120);
 for(unsigned i=0;i<2;i++)need(b.strip_row[i]==(int)(10+i)&&bank_strip_track(&s,&b,i)==input_track(&s,b.strips+i),"a Send bank records the return rows it resolved");
 for(unsigned i=2;i<8;i++)need(b.strip_row[i]<0&&!bank_strip_track(&s,&b,i),"an absent send destination never borrows an ordinary strip row");
 reset_fixture(c,&in,&b,&s,12);
 s.pad_count=8;
 for(unsigned i=0;i<8;i++){s.pads[i]=s.tracks[0];s.pads[i].pad_owner=100+i;s.pads[i].pad_index=i;s.pads[i].pad_generation=1;}
 bank_view(&b,BV_DRUM_PADS,130);bank_apply(&b,&s,130);
 for(unsigned i=0;i<8;i++){
  need(b.strip_row[i]<0,"a pad bank records no track row, because a pad identity already resolves by index");
  need(bank_strip_track(&s,&b,i)==input_track(&s,b.strips+i)&&bank_strip_track(&s,&b,i)==s.pads+i,"pad strips resolve through the same helper");
 }

 reset_fixture(c,&in,&b,&s,8);
 Surface play={.source=3,.full={32,0}};
 s.position_available=1;s.bar=0;s.beat=0;s.clock=0;
 observed_position_count=0;need(channel_output(&play,&b,&s,100),"first channel output");
 need(observed_position_count==10,"the first playhead publication sends all ten display controllers");
 observed_position_count=0;need(channel_output(&play,&b,&s,100),"repeated channel output");
 need(!observed_position_count,"an unchanged playhead sends nothing");
 need(play.position_valid&&play.position_source==1&&!play.position_bar&&!play.position_beat&&!play.position_clock,"the gate records exactly the four source words channel_position reads");
 s.clock=10;observed_position_count=0;need(channel_output(&play,&b,&s,101),"clock advance");
 need(observed_position_count&&play.position_clock==10,"a changed pulse still reaches the display");
 s.beat=1;observed_position_count=0;need(channel_output(&play,&b,&s,102),"beat advance");
 need(observed_position_count&&play.position_beat==1,"a changed beat still reaches the display");
 s.bar=1;observed_position_count=0;need(channel_output(&play,&b,&s,103),"bar advance");
 need(observed_position_count&&play.position_bar==1,"a changed bar still reaches the display");
 s.position_available=0;observed_position_count=0;need(channel_output(&play,&b,&s,104),"position lost");
 need(observed_position_count&&!play.position_source&&!observed_led[114],"a lost position blanks the display and its beats LED");
 observed_position_count=0;need(channel_output(&play,&b,&s,105),"position still lost");
 need(!observed_position_count,"an unchanged unavailable playhead is not reformatted or resent");
 free(c);
 puts("PASS outbound update policy: repeated zero meter levels suppressed while sustained nonzero levels keep refreshing, enable/identity/source changes re-arm the bar, bank strips resolve through a revalidated recorded row in track/paged/Send/pad banks, and the playhead is formatted only when its bar, beat, pulse or availability changed (ALSA hardware and source values substituted)");
}
#include "novation-production-test.inc"
static unsigned admission_interrupt_after,admission_write_count;
static void admission_write_checkpoint(void){if(admission_interrupt_after&&++admission_write_count==admission_interrupt_after)raise(SIGKILL);}
#include "control-program-production-test.inc"
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--profiles")){profile_policy_checks();return 0;}
 if(argc==2&&!strcmp(argv[1],"--programs")){control_program_production_checks();return 0;}
 if(argc==2&&!strcmp(argv[1],"--novation")){novation_production_checks();return 0;}
 control_program_production_checks();
 novation_production_checks();
 if(argc==2&&!strcmp(argv[1],"--policy")){alarm(20);profile_policy_checks();runtime_cycle_checks();collection_production_checks();hui_unready_liveness_checks();outbound_update_checks();native_text_wire_checks();binding_sync_checks();binding_reversal_checks();qlink_mode_routing_checks();input_page_checks();following_reenable_checks();milestone_checks();drum_channel_checks();drum_surface_checks();policy_checks();toggle_midi_checks();assignment_checks();effects_policy_checks();jog_policy_checks();data_wheel_checks();sequence_duplicate_bridge_checks();blocking_gate_checks();stop_type_checks();stop_relay_checks();master_policy_checks();return 0;}
 if(argc!=3)return 2;
 alarm(20);profile_policy_checks();runtime_cycle_checks();collection_production_checks();hui_unready_liveness_checks();outbound_update_checks();native_text_wire_checks();binding_sync_checks();binding_reversal_checks();qlink_mode_routing_checks();input_page_checks();following_reenable_checks();milestone_checks();drum_channel_checks();drum_surface_checks();policy_checks();toggle_midi_checks();assignment_checks();effects_policy_checks();jog_policy_checks();data_wheel_checks();sequence_duplicate_bridge_checks();blocking_gate_checks();stop_type_checks();stop_relay_checks();master_policy_checks();
 char *defaults[]={"mirror-input","/volume","/command"};need(servo_arguments(3,defaults)==3&&servo_disabled,"separate-process composition uses default no-echo policy");
 int fd=open(argv[1],O_RDONLY|O_CLOEXEC);struct stat st;need(fd>=0&&!fstat(fd,&st)&&st.st_size==sizeof(MirrorState),"actual producer mirror file");
 const MirrorState *state=mmap(NULL,sizeof(*state),PROT_READ,MAP_SHARED,fd,0);need(state!=MAP_FAILED,"actual read-only mirror mapping");
 uint64_t start=start_time(state->pid);int opened=start&&input_open_file(argv[2],state,start);
 if(!opened){fprintf(stderr,"OPEN failed errno=%d fd=%d mapped=%d start=%llu mirror_pid=%u uid=%u file_uid=%u bytes=%lld\n",errno,input_fd,input_mapping!=MAP_FAILED,(unsigned long long)start,state->pid,geteuid(),input_file.st_uid,(long long)input_file.st_size);if(input_mapping!=MAP_FAILED)fprintf(stderr,"CMD magic=%x version=%u bytes=%u cap=%u seconds=%u owner=%u pid=%u start=%u:%u origin=%u:%u expected_origin=%u:%u error=%u input_error=%u\n",input_mapping->magic,input_mapping->version,input_mapping->bytes,input_mapping->capacity,input_mapping->seconds,input_mapping->owner_token,input_mapping->pid,input_mapping->start_hi,input_mapping->start_lo,input_mapping->origin_sec,input_mapping->origin_nsec,state->origin_sec,state->origin_nsec,atomic_load(&input_mapping->error),input.error);}
 need(opened,"production CMD12 identity/map/cooperating writer lock");
 int competing=open(argv[2],O_RDWR|O_CLOEXEC);need(competing>=0&&flock(competing,LOCK_EX|LOCK_NB)<0,"second cooperating writer excluded");close(competing);
 MirrorBank bank;bank_init(&bank);MIRROR_COPY(s);uint32_t now;need(fresh_copy(state,&s,&now)==1&&s.ready,"fresh actual producer snapshot");bank_apply(&bank,&s,now);
 snd_midi_event_t *codec;need(!snd_midi_event_new(16,&codec),"ALSA MIDI codec");snd_seq_addr_t address={32,0};
 for(unsigned strip=0;strip<8;strip++){
  unsigned char bytes[3]={(unsigned char)(0xe0+strip),127,127};snd_seq_event_t e;need(snd_midi_event_encode(codec,bytes,3,&e)==3,"eight raw pitch-bend channels");e.source=address;unsigned kind,channel;int value;
  need(surface_event(&e,address,&kind,&channel,&value)==1&&kind==3&&channel==strip&&value==16383,"all eight MCU inputs retain full14-bit value");
  bytes[0]=0x90;bytes[1]=104+strip;bytes[2]=127;need(snd_midi_event_encode(codec,bytes,3,&e)==3,"eight raw touch notes");e.source=address;
  need(surface_event(&e,address,&kind,&channel,&value)==1&&kind==1&&channel==strip&&value,"all eight MCU touch identities");
  e.source.client++;need(!surface_event(&e,address,&kind,&channel,&value),"foreign hardware messages ignored");
 }

 const unsigned char down[3]={0x90,104,127},baseline[3]={0xe0,0,64},stationary[3]={0xe0,0,64},up[3]={0x90,104,0};
 raw_input(codec,down,address,&input,&bank,&s,now);raw_input(codec,stationary,address,&input,&bank,&s,now);raw_input(codec,up,address,&input,&bank,&s,now);input_pump(&input,&bank,&s,now);need(!input.submitted,"unchanged current position creates no native request");
 raw_input(codec,down,address,&input,&bank,&s,now);raw_input(codec,baseline,address,&input,&bank,&s,now);
 uint32_t until=now+10000;
 for(unsigned request=1;request<=20;request++){
  unsigned char pitch[3]={0xe0,0,request&1?96:32};raw_input(codec,pitch,address,&input,&bank,&s,now);
  while(input.settled<request){
   need(fresh_copy(state,&s,&now)==1&&now<=until,"bounded actual continuous source response");bank_apply(&bank,&s,now);input_sync(&input,&bank);input_pump(&input,&bank,&s,now);need(!input.error,"actual sequence-qualified source evidence");
   need(bank.faders[0].touch==MT_DOWN&&bank_due(&bank,0,now)<0,"every source update occurs while touch held and motor suppressed");
   struct timespec delay={0,1000000};nanosleep(&delay,NULL);
  }
 }
 need(input.submitted==20&&input.settled==20&&input.flights[0].acknowledged,"one held gesture exceeds sixteen settled requests");raw_input(codec,up,address,&input,&bank,&s,now);
 do{need(fresh_copy(state,&s,&now)==1&&now<=until,"fresh final source and owner reclamation");bank_apply(&bank,&s,now);input_drain(&input,&bank,&s,now);struct timespec delay={0,1000000};nanosleep(&delay,NULL);}while(atomic_load_explicit(&input.commands->reclaimed,memory_order_acquire)!=20||s.heartbeat<=bank.faders[0].barrier);
 need(s.tracks[0].bits==1048576512&&bank_due(&bank,0,now)==4096,"no-echo release retains authoritative settled-source motor feedback");unsigned char wire[3],expected[3]={0xe0,0,32};Surface final_surface={.source=3,.full=address};snd_seq_event_t final_event;motor_event(&final_event,&final_surface,0,4096);snd_midi_event_reset_decode(codec);need(snd_midi_event_decode(codec,wire,3,&final_event)==3&&!memcmp(wire,expected,3),"actual authoritative MOTOR encoding matches final E0 00 20");
 snd_midi_event_free(codec);input_close_file();competing=open(argv[2],O_RDWR|O_CLOEXEC);need(competing>=0&&!flock(competing,LOCK_EX|LOCK_NB),"explicit unlock releases mapped mailbox writer lease");flock(competing,LOCK_UN);close(competing);munmap((void*)state,sizeof(*state));close(fd);
 puts("PASS default no-echo production pitch handler/ALSA codec;20 continuous held movements -> actual CMD12 files/producer/atomic reuse -> sealed source settlement -> authoritative final motor MIDI; MPC process hash and physical ALSA device substituted");return 0;
}
