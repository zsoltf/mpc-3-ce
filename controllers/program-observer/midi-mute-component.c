/* Production native hook adapter and ledger with synthetic native owners,
 * event contexts, queue scheduler and sink; actual ARM detour pairs and native
 * 16-byte constructor execute. Native NoteOff body/physical output substituted. */
#define main command_suite_not_called
#include "command-component.c"
#undef main
#ifndef MODEL_MIDI_MUTE
#error "native MIDI product profile required"
#endif
static unsigned char *drum_instrument;
#define MUTE_INSTRUMENT_PAGES ((MODEL_INSTRUMENT_SIZE+4095u)&~4095u)
static uint32_t drum_pad_vector[PAD_SLOTS],drum_note_event[14],drum_pending[22],drum_sink[0x120/4];
static struct {Context c;uint32_t stack[128];} drum_ctx;
static void drum_pair_replay(void){
 uint32_t pages[20];unsigned count=0,checked=0;static uint32_t vtable[128];
 unsigned char *stub=mmap(NULL,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);require(stub!=MAP_FAILED,"MIDI replay stub page");
 for(unsigned n=0;n<PATCH_COUNT;n++)if(((hook_ids[n]>=DMP_ROOT&&hook_ids[n]<=DMP_CLEANUP_END&&hook_ids[n]!=DMP_MUTE)||(hook_ids[n]>=MM_ON_RETURN&&hook_ids[n]<=MM_INSTRUMENT_MAP))){
  uint32_t page=anchor[n]&~4095u;unsigned k;for(k=0;k<count;k++)if(pages[k]==page)break;
  if(k==count){require(count<20,"MIDI replay page cap");require(mmap((void*)(uintptr_t)page,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)(uintptr_t)page,"MIDI replay exact page");pages[count++]=page;}
  Context in=context(),original,patched;in.r[0]=ptr(drum_pending);in.r[3]=ptr(vtable);in.r[4]=ptr(track_objects[0]);in.r[8]=ptr(program_objects[0]);in.r[10]=ptr(program_objects[0]);in.r[9]=ptr(program_objects[0]);
  memcpy((void*)(uintptr_t)anchor[n],expected[n],8);for(unsigned j=0;j<2;j++)if(relocated[n][j]){uint32_t instruction;memcpy(&instruction,expected[n]+4*j,4);put(anchor[n]+4*j+8+(instruction&0xfff),relocated[n][j]);}uint32_t *out=(uint32_t*)(uintptr_t)(anchor[n]+8);jump(&out,ptr(capture_a));
  __builtin___clear_cache((char*)(uintptr_t)anchor[n],(char*)(uintptr_t)(anchor[n]+16));
  exercise(&in,(void*)(uintptr_t)anchor[n],&original);uint32_t shift=captured_sp-saved_sp;
  require(build_stub(stub,n,0)<=4096,"MIDI actual stub bound");
  unsigned offset=capture_after_pair[n]?8:0;*(uint32_t*)(stub+offset+(gate_handler-gate_begin))=ptr(probe_context);
  __builtin___clear_cache((char*)stub,(char*)stub+STUB_BYTES);exercise(&in,stub,&patched);
  require(!memcmp(&original,&patched,sizeof(original))&&captured_sp-saved_sp==shift,"MIDI detour preserves GPR/APSR/FPSCR/d0-d31/SP");
  require(!memcmp(&probe_saved,capture_after_pair[n]?&original:&in,sizeof(in)),"MIDI exact pre/post-pair capture ABI");checked++;
 }
 require(checked==24,"all MIDI pairs replayed");require(!munmap(stub,4096),"MIDI replay stub retired");
 for(unsigned k=0;k<count;k++)require(!munmap((void*)(uintptr_t)pages[k],4096),"MIDI replay page retired");
 puts("PASS all24 exact ARM MIDI pairs through production stub/gate: preserved registers, flags and SP; exact capture order (native continuations and callback substituted)");
}
static unsigned char peer_instrument[MODEL_INSTRUMENT_SIZE] __attribute__((aligned(8)));
static unsigned char mute_editor[0x500],mute_audio[0x400],mute_clock[0xb0],mute_time[0x80];
static uint32_t wire_count,early_count,last_status,last_note,last_route;
static uint32_t packet[8],current_enqueuer[4],output_offset,stamp_mismatch,native_time=100000,native_off_calls;
static uint32_t wire_status[32768],wire_note[32768],wire_stamp[32768],wire_offset[32768],native_off_offsets[32768];
static void mute_wire(uint32_t sink,uint32_t route_key,uint32_t message){
 const uint32_t *p=(const uint32_t*)(uintptr_t)message;require(sink==ptr(drum_sink),"observed native sink identity");
 require(wire_count<32768,"finite fixture wire");wire_status[wire_count]=p[0];wire_note[wire_count]=p[1];wire_stamp[wire_count]=p[6];wire_offset[wire_count]=p[7];wire_count++;last_status=p[0];last_note=p[1];last_route=route_key;
}
static void mute_native_off(void *opaque,uint32_t offset,uint32_t route_key,uint32_t channel,uint32_t note){
 uint32_t *e=opaque;require(e[0]==0x6cb6db0&&e[1]==ptr(mute_clock)&&e[2]==ptr(mute_time)+0x20&&e[3]==ptr(drum_sink),"actual native constructor and five-argument ARM off ABI");
 require(native_off_calls<32768,"finite native call fixture");native_off_offsets[native_off_calls++]=offset;
 uint32_t message[8]={0x80|channel,note,0,0,0,0,native_time+offset+stamp_mismatch,offset};
 Context c=context();c.r[0]=e[3];c.r[1]=route_key;c.r[2]=ptr(message);c.r[9]=MODEL_MIDI_SINK_ENQUEUE;call(&c,DMP_OFF_SINK);
 ((void(*)(uint32_t,uint32_t,uint32_t))(uintptr_t)c.r[9])(c.r[0],c.r[1],c.r[2]);early_count++;
 call(&c,MM_OFF_RETURN);
}
static void mute_commit_admission(void){
 unsigned seen=0;for(unsigned i=0;i<PATCH_COUNT;i++)if(hook_ids[i]==M_MUTE){
  require(anchor[i]==0xe4d9c4&&!capture_after_pair[i],"effective mute actual generated commit hook");seen++;
 }
 const unsigned char *store=guarded_bytes(0xe4d9c0,4);const unsigned char expected_store[4]={0x28,0x70,0xc5,0xe5};
 require(seen==1&&store&&!memcmp(store,expected_store,4),"native effective byte store precedes generation hook");
}
static void mute_native_pages(void){
 const uint32_t pages[]={MODEL_MIDI_ENQUEUER_CTOR&~4095u,MODEL_MIDI_NOTE_OFF&~4095u,MODEL_MIDI_SINK_ENQUEUE&~4095u,MODEL_MIDI_DEFAULT_ENQUEUER_VPTR&~4095u};
 for(unsigned i=0;i<4;i++)require(mmap((void*)(uintptr_t)pages[i],4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)(uintptr_t)pages[i],"native helper fixture page");
 const unsigned char *ctor=guarded_bytes(MODEL_MIDI_ENQUEUER_CTOR,28),*dtor=guarded_bytes(MODEL_MIDI_ENQUEUER_DTOR,4);require(ctor&&dtor,"guarded native constructor/destructor");
 memcpy((void*)(uintptr_t)MODEL_MIDI_ENQUEUER_CTOR,ctor,28);memcpy((void*)(uintptr_t)MODEL_MIDI_ENQUEUER_DTOR,dtor,4);
 put(MODEL_MIDI_DEFAULT_ENQUEUER_VPTR+0xc,MODEL_MIDI_OFF_MAPPER);
 uint32_t *p=(uint32_t*)(uintptr_t)MODEL_MIDI_NOTE_OFF;jump(&p,ptr(mute_native_off));p=(uint32_t*)(uintptr_t)MODEL_MIDI_SINK_ENQUEUE;jump(&p,ptr(mute_wire));
 for(unsigned i=0;i<4;i++)__builtin___clear_cache((char*)(uintptr_t)pages[i],(char*)(uintptr_t)(pages[i]+4096));
}
static void mute_hook(unsigned site){call(&drum_ctx.c,site);}
static void mute_root_source(unsigned note,unsigned mode,uint32_t source){
 memset(drum_note_event,0,sizeof(drum_note_event));drum_note_event[2]=3;drum_note_event[4]=0x3f800000;drum_note_event[12]=note;
 drum_ctx.c=context();drum_ctx.c.r[4]=ptr(track_objects[0]);drum_ctx.c.r[2]=ptr(drum_note_event);drum_ctx.c.r[5]=0x11110000;
 drum_ctx.stack[0x138/4]=mode;drum_ctx.stack[0x13c/4]=source;mute_hook(DMP_ROOT);
}
static void mute_root(unsigned note,unsigned mode){mute_root_source(note,mode,mode==3?0x7ffffff8:77);}
static void mute_map(void){drum_ctx.c.r[5]=ptr(track_objects[0]);drum_ctx.c.r[2]=ptr(drum_note_event);mute_hook(DMP_MAP);drum_ctx.c.r[7]=ptr(drum_note_event[12]==36?drum_instrument:peer_instrument);mute_hook(MM_INSTRUMENT_MAP);mute_hook(DMP_MAP_END);}
static void mute_output(unsigned note,int off){
 packet[0]=off?0x80:0x90;packet[1]=note;packet[2]=off?0:127;packet[6]=native_time+output_offset;packet[7]=output_offset;
 drum_ctx.c.r[1]=packet[0];drum_ctx.c.r[2]=packet[1];drum_ctx.c.r[3]=packet[2];drum_ctx.c.r[4]=ptr(current_enqueuer);drum_ctx.c.r[5]=output_offset;drum_ctx.c.r[6]=off?0x3456:output_offset;drum_ctx.c.r[7]=0x3456;
 mute_hook(off?DMP_OFF_BYTES:DMP_ON_BYTES);
 drum_ctx.c.r[0]=drum_ctx.c.r[8]=ptr(drum_sink);drum_ctx.c.r[1]=0x3456;drum_ctx.c.r[2]=ptr(packet);drum_ctx.c.r[9]=MODEL_MIDI_SINK_ENQUEUE;
 mute_hook(off?DMP_OFF_SINK:DMP_ON_SINK);
 ((void(*)(uint32_t,uint32_t,uint32_t))(uintptr_t)drum_ctx.c.r[9])(drum_ctx.c.r[0],drum_ctx.c.r[1],drum_ctx.c.r[2]);
 mute_hook(off?MM_OFF_RETURN:MM_ON_RETURN);
}
static void mute_sequence(unsigned note,unsigned expiry,uint32_t pending[22]){
 mute_root(note,3);mute_map();mute_output(note,0);mute_hook(DMP_ROOT_END);
 memset(pending,0,22*4);memcpy(pending,drum_note_event,sizeof(drum_note_event));pending[14]=expiry;pending[16]=note;pending[17]=0x7ffffff8;pending[18]=3;pending[20]=ptr(track_objects[0]);
 drum_ctx.c.r[4]=ptr(track_objects[0]);drum_ctx.c.r[5]=0x11110000;drum_ctx.c.r[1]=ptr(pending);mute_hook(DMP_PENDING);
}
static void mute_due(uint32_t pending[22],int drain){
 /* Native move leaves these padding bytes indeterminate. */
 pending[3]=0xdeadbeef;pending[13]|=0xbeef0000;pending[16]|=0xdead0000;pending[21]|=0xabcd0000;
 drum_ctx.c=context();drum_ctx.c.r[drain?3:0]=ptr(pending);drum_ctx.c.r[drain?10:8]=0x11110000;mute_hook(drain?DMP_DRAIN:DMP_DUE);
 if(!drain){drum_ctx.c.r[4]=ptr(track_objects[0]);drum_ctx.c.r[2]=pending[16]&255;drum_ctx.c.r[3]=0;mute_hook(DMP_OFF_ROOT);}
 mute_output(pending[16]&255,1);
 if(!drain)mute_hook(DMP_OFF_END);
 mute_hook(drain?DMP_DRAIN_END:DMP_DUE_END);
}
static void mute_live(unsigned note){mute_root(note,1);mute_map();mute_output(note,0);mute_hook(DMP_ROOT_END);}
static void mute_live_source_event(unsigned note,unsigned mode,uint32_t source){mute_root_source(note,mode,source);mute_map();mute_output(note,0);mute_hook(DMP_ROOT_END);}
static void mute_live_off_source(unsigned note,uint32_t source){
 drum_ctx.c=context();drum_ctx.c.r[4]=ptr(track_objects[0]);drum_ctx.c.r[2]=note;drum_ctx.c.r[3]=0;drum_ctx.stack[0x40/4]=source;drum_ctx.stack[0x48/4]=2;mute_hook(DMP_OFF_ROOT);mute_output(note,1);mute_hook(DMP_OFF_END);
}
static void mute_live_off(unsigned note){mute_live_off_source(note,77);}
static void mute_cleanup(unsigned note){
 drum_ctx.c=context();drum_ctx.c.r[0]=ptr(track_objects[0]);mute_hook(DMP_CLEANUP);mute_output(note,1);mute_hook(DMP_CLEANUP_END);
}
static void mute_orphan_send(void){
 drum_ctx.c=context();mute_hook(DMP_MAP);mute_hook(DMP_MAP_END);
 mute_hook(DMP_SEND);require(!midi_mute_depth&&midi_mute_skip==1,"orphan Send has no invented epoch frame");
 mute_hook(DMP_SEND);
 /* Native Stop forwards sustain release (type1 CC64/value0) after its
  * pending-note drain, with no source-note or cleanup ancestor. */
 memset(packet,0,sizeof(packet));packet[0]=0xb0;packet[1]=64;packet[6]=native_time;mute_wire(ptr(drum_sink),0x3456,ptr(packet));
 mute_hook(DMP_SEND_END);mute_hook(DMP_SEND_END);
 require(!midi_mute_depth&&!midi_mute_skip,"orphan nested Send subtree balances");
}
static void *mute_foreign(void *unused){(void)unused;mute_live(36);return NULL;}
static void mute_set(unsigned enabled){
 *(unsigned char*)(drum_instrument+0x170)=enabled;drum_ctx.c=context();drum_ctx.c.r[5]=ptr(drum_instrument)+0x148;drum_ctx.c.r[7]=enabled;mute_hook(M_MUTE);
}
static void mute_flush(void){midi_mute_service();require(transport_queue_component_pending(),"existing Editor queue accepted dirty mute");transport_queue_component_run();require(midi_mute_quiescent()&&!atomic_load(&midi_mute_projection()->pending),"retained callback and publication complete");}
static void mute_setup(void){
 drum_instrument=mmap(NULL,MUTE_INSTRUMENT_PAGES,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);require(drum_instrument!=MAP_FAILED,"separate native Instrument fixture pages");
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"fixture allocation");initial(2);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 seed_fixture(0,0,0x3f000000,1);seed_fixture(1,0,0x3f000000,1);load_fixture();put(ptr(program_objects[1]),MODEL_MIDIPROGRAM_VPTR);atomic_store(&mirror_state->sequencer,0x11110000);
 uint32_t d=ptr(program_objects[0]);put(d+0x1fec,ptr(drum_pad_vector));put(d+0x1ff0,ptr(drum_pad_vector)+sizeof(drum_pad_vector));put(d+0x1ff4,ptr(drum_pad_vector)+sizeof(drum_pad_vector));
 for(unsigned n=0;n<2;n++){uint32_t i=ptr(n?peer_instrument:drum_instrument);put(i,MODEL_INSTRUMENT_VPTR);put(i+MODEL_INSTRUMENT_PARENT,d);put(i+MODEL_INSTRUMENT_SLOT,n);put(d+0x1370+(36+n*2)*4,n);pad_birth(i,d);PadOwner *o=pad_owner(i);require(o!=NULL,"Instrument owner");atomic_store(&o->complete,1);require(pad_membership_begin(d,n,0,0,0x8000),"membership entry");drum_pad_vector[n]=i;pad_membership_end(0x8000-0x58,0);}
 uint32_t r=ptr(root_object),ed=ptr(mute_editor);put(r,MODEL_MIDI_ENGINE_VPTR);put(r+0x30c,ptr(mute_audio));put(r+0x110,ptr(mute_clock));put(r+0x198,ptr(drum_sink));put(r+0xfc,ptr(queue_object));put(ptr(mute_audio),MODEL_MIDI_AUDIO_VPTR);put(ptr(mute_audio)+0x3b0,ptr(mute_time));put(ptr(mute_time),MODEL_MIDI_TIME_VPTR);drum_sink[0]=MODEL_MIDI_SINK_VPTR;drum_sink[0x24/4]=ptr(queue_object);
 put(ed,MODEL_EDITOR_VPTR);put(ed+0x230,MODEL_EDITOR_SECONDARY_VPTR);put(r-0xd0+MODEL_APP_EDITOR,ed);put(ed+0x37c,r);put(ed+0x2c8,ptr(queue_object));channel_owner_birth(ed,CO_EDITOR);atomic_store(&mirror_state->general.editor_root,r);atomic_store(&mirror_state->general.editor,ed);atomic_store(&mirror_state->general.editor_owner,channel_owner_id(ed));
 transport_queue_component_reset();mute_native_pages();
 ((void(*)(void*,uint32_t,uint32_t,uint32_t))(uintptr_t)MODEL_MIDI_ENQUEUER_CTOR)(current_enqueuer,ptr(mute_clock),ptr(mute_time)+0x20,ptr(drum_sink));
}
int main(int argc,char **argv){
 require(argc==2,"one new fixture output path");alarm(60);exercise_adjust=4096;drum_pair_replay();mute_commit_admission();mute_setup();uint32_t old[22],peer[22],blocked[22],newer[22];
 mute_sequence(36,1000,old);mute_sequence(38,1100,peer);require(wire_count==2&&midi_mute_ledger.active==2&&midi_mute_ledger.used==2,"baseline two distinct native output keys");
 mute_set(1);require(wire_count==2,"setter hook emits no MIDI under Property mutex");mute_flush();require(wire_count==3&&early_count==1&&last_status==0x80&&last_note==36&&midi_mute_ledger.active==1,"queued targeted off preserves distinct peer");
 mute_sequence(36,1200,blocked);require(wire_count==3&&midi_mute_ledger.used==3,"muted positive blocked, release identity retained");mute_due(blocked,0);require(wire_count==3&&midi_mute_ledger.used==2,"blocked positive's native off suppressed");
 mute_set(0);midi_mute_service();require(wire_count==3&&!transport_queue_component_pending(),"unmute has no replay");
 mute_sequence(36,1300,newer);require(wire_count==4&&midi_mute_ledger.active==2,"re-hit after unmute reaches native sink");mute_due(old,0);require(wire_count==4&&midi_mute_ledger.active==2,"old scheduled off cannot cut re-hit");
 mute_due(peer,0);require(wire_count==5&&last_note==38&&midi_mute_ledger.active==1,"peer keeps its necessary native release");mute_due(newer,1);require(wire_count==6&&!midi_mute_ledger.used,"pending drain preserves current release and frees accounting");
 unsigned before=wire_count,tracked=atomic_load(&midi_mute_projection()->emitted);mute_orphan_send();
 require(wire_count==before+1&&last_status==0xb0&&last_note==64&&!atomic_load(&midi_mute_projection()->error)&&atomic_load(&midi_mute_projection()->emitted)==tracked&&!midi_mute_ledger.used,"orphan forwarding preserves native output without causal accounting");
 mute_sequence(36,1340,old);before=wire_count;mute_cleanup(36);mute_orphan_send();mute_due(old,1);
 require(wire_count==before+2&&!midi_mute_ledger.used&&!atomic_load(&midi_mute_projection()->error),"Stop cleanup, orphan Send and pending drain leave no false epoch fault");
 mute_root(36,1);atomic_fetch_add(&fixture->epoch,1);epoch=atomic_load(&fixture->epoch);mute_map();mute_hook(DMP_ROOT_END);
 require(atomic_load(&midi_mute_projection()->error)==(MM_IDENTITY|(4u<<8))&&!midi_mute_depth&&!midi_mute_skip,"real epoch change across open root still faults and balances");
 mute_live(36);mute_set(1);mute_flush();mute_set(0);mute_live(36);before=wire_count;mute_live_off(36);require(wire_count==before+1&&!midi_mute_ledger.used,"live native key release consumes active/retires same-key history without FIFO invention");
 /* Poison the native Instrument after its mapper borrow; sink must consume
  * the native commit projection without dereferencing the borrowed address. */
 mute_root(36,1);mute_map();before=wire_count;
 require(!mprotect(drum_instrument,MUTE_INSTRUMENT_PAGES,PROT_NONE),"end native Instrument borrow fixture");mute_output(36,0);
 require(!mprotect(drum_instrument,MUTE_INSTRUMENT_PAGES,PROT_READ|PROT_WRITE),"restore native fixture pages");mute_hook(DMP_ROOT_END);
 require(wire_count==before+1,"OnSink uses copied native mute after Instrument borrow ends");mute_live_off(36);
 /* Physical PadPerform has mode2/token7ffffffa, with the same token on
  * its live release. This fixture supplies the lane; device proof must show
  * real wire output AND tracking counters under the unchanged lane guard. */
 before=wire_count;mute_live_source_event(36,2,0x7ffffffa);mute_live_source_event(38,2,0x7ffffffa);
 require(wire_count==before+2&&midi_mute_ledger.active==2,"qualified physical source accounts both emitted pads");
 mute_set(1);mute_flush();require(wire_count==before+3&&last_note==36&&midi_mute_ledger.active==1,"physical pad mute chokes only affected output");
 mute_live_source_event(36,2,0x7ffffffa);mute_live_off_source(36,0x7ffffffa);
 require(wire_count==before+3&&midi_mute_ledger.used==1,"physical muted positive and grouped retired release remain suppressed");
 mute_live_source_event(36,2,77);mute_live_off_source(36,77);
 require(wire_count==before+5&&midi_mute_ledger.used==1,"unqualified mode2 source preserves native on and off while pad muted");
 mute_set(0);midi_mute_service();require(wire_count==before+5,"physical unmute has no replay");mute_live_source_event(36,2,0x7ffffffa);mute_live_off_source(38,0x7ffffffa);mute_live_off_source(36,0x7ffffffa);
 require(wire_count==before+8&&!midi_mute_ledger.used,"physical re-hit and distinct peer retain their native releases");
 /* Same native source re-hit must flush before the UI choke job runs. */
 mute_sequence(36,1350,old);drum_ctx.c=context();drum_ctx.c.r[3]=ptr(drum_sink);mute_hook(MM_ROUTES_REPLACE);mute_set(1);mute_set(0);before=wire_count;
 unsigned calls_before=native_off_calls;output_offset=37;mute_sequence(36,1360,newer);output_offset=0;
 require(wire_count==before+2&&wire_status[before]==0x80&&wire_status[before+1]==0x90&&native_off_calls==calls_before+1&&native_off_offsets[calls_before]==37&&wire_stamp[before]==wire_stamp[before+1],"unserved mute emits off before new on at current offset");
 before=wire_count;mute_flush();require(wire_count==before,"late queued choke cannot cut new hit");mute_due(old,0);require(wire_count==before,"old expiry after inline choke suppressed");mute_due(newer,0);require(wire_count==before+1&&!midi_mute_ledger.used,"new inline-order hit retains own release");
 mute_sequence(36,1370,old);before=wire_count;mute_set(1);midi_mute_service();require(transport_queue_component_cancel(),"native preclaim cancellation fixture");
 require(!midi_mute_quiescent(),"canceled native capture still pins until Editor post destructor");
 midi_mute_editor_retired(ptr(mute_editor));require(midi_mute_quiescent()&&!atomic_load(&midi_mute_projection()->pending)&&wire_count==before,"Editor retirement releases canceled flight without emitting");
 transport_queue_component_reclaim();mute_flush();require(wire_count==before+1,"dirty marker survives cancellation fixture");mute_set(0);mute_due(old,0);
 mute_sequence(36,1380,old);mute_set(1);midi_mute_service();require(transport_queue_component_claim(),"existing queue claims retained callback");before=wire_count;
 observer_running=0;transport_queue_component_run_claimed();require(wire_count==before&&midi_mute_quiescent(),"closed owner refuses native helper and releases callback");observer_running=1;
 mute_flush();mute_set(0);mute_due(old,0);
 mute_sequence(36,1390,old);before=wire_count;mute_cleanup(36);require(wire_count==before+1&&!midi_mute_ledger.active,"native source-wide cleanup preserves necessary release");
 mute_due(old,1);require(wire_count==before+1&&!midi_mute_ledger.used,"cleanup leaves expiry tombstone until native drain");
 mute_set(1);before=wire_count;pthread_t foreign;require(!pthread_create(&foreign,NULL,mute_foreign,NULL)&&!pthread_join(foreign,NULL),"foreign fixture lane");require(wire_count==before+1&&!midi_mute_ledger.used,"unenrolled native path retains original output");mute_set(0);mute_flush();
 /* A Mini discovery publishes the output map while the DIN key survives. */
 mute_sequence(36,1400,old);mute_sequence(38,1401,peer);drum_ctx.c=context();drum_ctx.c.r[3]=ptr(drum_sink);mute_hook(MM_ROUTES_REPLACE);before=wire_count;mute_set(1);mute_flush();
 require(wire_count==before+1&&last_note==36&&midi_mute_ledger.active==1&&!atomic_load(&midi_mute_projection()->error),"unrelated route publication preserves targeted choke and distinct peer");
 mute_set(0);mute_sequence(36,1402,newer);before=wire_count;mute_due(old,0);require(wire_count==before,"old expiry remains suppressed across publication and re-hit");mute_due(peer,0);mute_due(newer,0);
 require(wire_count==before+2&&!midi_mute_ledger.used,"peer and re-hit retain their own releases across route publication");
 mute_sequence(36,1410,old);drum_ctx.c=context();drum_ctx.c.r[9]=ptr(drum_sink);mute_hook(MM_ROUTES_CLEAR);before=wire_count;mute_due(old,1);
 require(wire_count==before+1&&!midi_mute_ledger.used&&!atomic_load(&midi_mute_projection()->error),"native drain delegates current route meaning to native sink after publication");
 mute_sequence(36,1420,old);drum_ctx.c=context();drum_ctx.c.r[3]=ptr(drum_sink);mute_hook(MM_ROUTES_REPLACE);before=wire_count;mute_cleanup(36);mute_due(old,1);
 require(wire_count==before+1&&!midi_mute_ledger.used,"native cleanup after publication emits once then keeps a drain tombstone");
 /* A different rooted MidiDevices owner still cannot service a captured sink. */
 mute_sequence(36,1430,old);before=wire_count;put(ptr(root_object)+0x198,ptr(peer_instrument));mute_set(1);mute_flush();put(ptr(root_object)+0x198,ptr(drum_sink));
 require(wire_count==before&&atomic_load(&midi_mute_projection()->error)==MM_ROUTE_CHANGED&&atomic_load(&midi_mute_projection()->route_lost)==1,"changed rooted sink still refuses CE choke");
 mute_due(old,0);require(wire_count==before+1&&!midi_mute_ledger.used,"refused sink-identity choke keeps original native expiry");
 mute_set(0);atomic_fetch_add(&fixture->epoch,1);epoch=atomic_load(&fixture->epoch);
 mute_sequence(36,1440,old);before=wire_count;put(ptr(root_object)+0x198,ptr(peer_instrument));mute_set(1);mute_flush();put(ptr(root_object)+0x198,ptr(drum_sink));mute_cleanup(36);mute_due(old,1);
 require(wire_count==before+1&&!midi_mute_ledger.used,"abandoned choke keeps native cleanup then suppresses duplicate drain");
 mute_set(0);atomic_fetch_add(&fixture->epoch,1);epoch=atomic_load(&fixture->epoch);
 mute_sequence(36,1450,old);mute_set(1);mute_set(0);before=wire_count;stamp_mismatch=1;mute_sequence(36,1460,newer);stamp_mismatch=0;
 require(wire_count==before+1&&last_status==0x90&&atomic_load(&midi_mute_projection()->ordering_lost)==1,"timestamp mismatch withholds stale off and preserves new native on");
 mute_flush();require(wire_count==before+1,"timestamp refusal prevents late queued choke");mute_due(old,0);require(wire_count==before+2,"refused timestamp choke keeps necessary original expiry");mute_due(newer,0);require(wire_count==before+3,"new unaccounted native release preserved after timestamp fault");
 mute_set(0);atomic_fetch_add(&fixture->epoch,1);epoch=atomic_load(&fixture->epoch);
 /* Native input may append a future-offset On before this SAME block drains
  * Q+100. The first offset0 Off must not reach the consumer before that On. */
 native_time=100000;output_offset=37;before=wire_count;mute_live(36);output_offset=0;calls_before=native_off_calls;mute_set(1);mute_flush();
 require(wire_count==before+2&&wire_status[before]==0x90&&wire_status[before+1]==0x80&&wire_stamp[before+1]>=wire_stamp[before],"same-block future On remains before targeted Off at native sink");
 require(native_off_calls==calls_before+2&&native_off_offsets[calls_before]==0&&native_off_offsets[calls_before+1]==37&&wire_offset[before+1]==37&&!midi_mute_ledger.active,"withhold early offset0, retry recorded offset, then retire once");
 mute_set(0);before=wire_count;mute_live_off(36);require(wire_count==before&&!midi_mute_ledger.used,"corrected early Off retains old live release pairing");
 output_offset=69;before=wire_count;mute_live(36);output_offset=0;native_time+=100;calls_before=native_off_calls;mute_set(1);mute_flush();
 require(native_off_calls==calls_before+1&&native_off_offsets[calls_before]==0&&wire_count==before+2&&wire_stamp[before+1]>wire_stamp[before],"later-block mute stays immediate at native offset0");mute_set(0);mute_live_off(36);
 native_time=100000;output_offset=37;mute_live(36);output_offset=0;native_time=99900;before=wire_count;calls_before=native_off_calls;mute_set(1);mute_flush();
 require(wire_count==before&&native_off_calls==calls_before+2&&midi_mute_ledger.active==1&&midi_mute_ledger.used==1&&atomic_load(&midi_mute_projection()->error)==MM_NATIVE,"two too-early packets withheld, active ownership and explicit fault retained");
 native_time=100100;mute_flush();require(wire_count==before+1&&!midi_mute_ledger.active,"retained dirty work succeeds once native time reaches original On");mute_set(0);mute_live_off(36);
 atomic_fetch_add(&fixture->epoch,1);epoch=atomic_load(&fixture->epoch);
 output_offset=69;mute_live(36);output_offset=0;before=wire_count;calls_before=native_off_calls;mute_set(1);mute_set(0);mute_live(36);
 require(wire_count==before+1&&native_off_calls==calls_before&&atomic_load(&midi_mute_projection()->ordering_lost)==2,"earlier re-hit timestamp refuses an off before future old On");
 mute_flush();require(wire_count==before+1,"refused reverse-time flush cannot become late queued choke");mute_live_off(36);require(wire_count==before+2&&!midi_mute_ledger.used,"unaccounted live re-hit retains grouped native release after refusal");
 atomic_fetch_add(&fixture->epoch,1);epoch=atomic_load(&fixture->epoch);mute_root(36,3);
 drum_ctx.c.r[5]=ptr(track_objects[0]);drum_ctx.c.r[2]=ptr(drum_note_event);mute_hook(DMP_MAP);drum_ctx.c.r[7]=ptr(peer_instrument);mute_hook(MM_INSTRUMENT_MAP);
 require(atomic_load(&midi_mute_projection()->error)==(MM_IDENTITY|(17u<<8)),"native first-fault detail identifies mismatched borrowed Instrument");
 midi_mute_identity(31);require(atomic_load(&midi_mute_projection()->error)==(MM_IDENTITY|(17u<<8)),"later identity failure cannot overwrite first branch");mute_hook(DMP_MAP_END);mute_hook(DMP_ROOT_END);
 atomic_fetch_add(&fixture->epoch,1);epoch=atomic_load(&fixture->epoch);mute_sequence(36,1500,old);mute_due(old,0);require(!atomic_load(&midi_mute_projection()->error),"native epoch boundary resets fault accounting");
 for(unsigned i=0;i<MM_BUCKET_LIMIT;i++)mute_live(36);
 mute_set(1);mute_flush();mute_set(0);before=wire_count;mute_live(36);
 require(wire_count==before+1&&atomic_load(&midi_mute_projection()->error)==MM_CAPACITY,"unaccounted live re-hit passes after key bucket exhaustion");
 mute_live_off(36);require(wire_count==before+2&&!midi_mute_ledger.used,"contaminated retired live group keeps necessary native off");
 atomic_fetch_add(&fixture->epoch,1);epoch=atomic_load(&fixture->epoch);
 uint32_t first[22];for(unsigned i=0;i<MIDI_MUTE_CAPACITY;i++){mute_sequence(36,2000+i,newer);if(!i)memcpy(first,newer,sizeof(first));}
 require(midi_mute_ledger.used==MIDI_MUTE_CAPACITY&&!atomic_load(&midi_mute_projection()->error),"8192 indexed emission/release records");before=wire_count;mute_sequence(36,20000,newer);require(wire_count==before+1&&midi_mute_ledger.used==MIDI_MUTE_CAPACITY&&atomic_load(&midi_mute_projection()->error)==MM_CAPACITY,"capacity preserves unmuted native output and retained entries");mute_due(first,0);require(wire_count==before+2&&midi_mute_ledger.used==MIDI_MUTE_CAPACITY-1,"capacity preserves an already-accounted release");mute_set(1);before=wire_count;mute_sequence(36,20001,newer);for(unsigned i=before;i<wire_count;i++)require(wire_status[i]==0x80,"capacity still blocks positive while flushing stale notes");mute_set(0);
 printf("PASS production MIDI adapter: baseline/mute/block/unmute/re-hit/peer/live/drain/route generation/capacity/same-block future timestamp correction/orphan Send/real epoch fence; entry_bytes=%zu storage_bytes=%u high_water=%u max_key_bucket_walk=%u; native owners/scheduler/NoteOff body/sink substituted, physical MIDI unobserved\n",sizeof(MidiMuteEntry),atomic_load(&midi_mute_projection()->storage_bytes),atomic_load(&midi_mute_projection()->high_water),atomic_load(&midi_mute_projection()->max_lookup));
 FILE *out=fopen(argv[1],"wx");require(out&&fwrite(fixture,sizeof(*fixture),1,out)==1&&!fclose(out),"export actual production counter tail in fixture MirrorState");return 0;
}
