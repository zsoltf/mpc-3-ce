/* Actual observer dispatch and client shared records; synthetic native objects,
 * execution contexts, sink, scheduling and image pages. No MPC or MIDI output. */
#define main command_suite_not_called
#include "command-component.c"
#undef main
#ifndef MODEL_MIDI_PROBE
#error "explicit MIDI diagnostic profile required"
#endif
static unsigned char drum_instrument[MODEL_INSTRUMENT_SIZE] __attribute__((aligned(8)));
static uint32_t drum_pad_vector[PAD_SLOTS],drum_note_event[14],drum_pending[22],drum_sink[4];
static struct {Context c;uint32_t stack[128];} drum_ctx;
static void drum_pair_replay(void){
 uint32_t pages[16];unsigned count=0,checked=0;static uint32_t vtable[128];
 unsigned char *stub=mmap(NULL,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);require(stub!=MAP_FAILED,"MIDI replay stub page");
 for(unsigned n=0;n<PATCH_COUNT;n++)if(hook_ids[n]>=DMP_ROOT&&hook_ids[n]<=DMP_CLEANUP_END&&hook_ids[n]!=DMP_MUTE){
  uint32_t page=anchor[n]&~4095u;unsigned k;for(k=0;k<count;k++)if(pages[k]==page)break;
  if(k==count){require(count<16,"MIDI replay page cap");require(mmap((void*)(uintptr_t)page,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void*)(uintptr_t)page,"MIDI replay exact page");pages[count++]=page;}
  Context in=context(),original,patched;in.r[0]=ptr(drum_pending);in.r[3]=ptr(vtable);in.r[4]=ptr(track_objects[0]);in.r[8]=ptr(program_objects[0]);in.r[10]=ptr(program_objects[0]);
  memcpy((void*)(uintptr_t)anchor[n],expected[n],8);uint32_t *out=(uint32_t*)(uintptr_t)(anchor[n]+8);jump(&out,ptr(capture_a));
  __builtin___clear_cache((char*)(uintptr_t)anchor[n],(char*)(uintptr_t)(anchor[n]+16));
  exercise(&in,(void*)(uintptr_t)anchor[n],&original);uint32_t shift=captured_sp-saved_sp;
  require(build_stub(stub,n,0)<=4096,"MIDI actual stub bound");
  unsigned offset=capture_after_pair[n]?8:0;*(uint32_t*)(stub+offset+(gate_handler-gate_begin))=ptr(probe_context);
  __builtin___clear_cache((char*)stub,(char*)stub+STUB_BYTES);exercise(&in,stub,&patched);
  require(!memcmp(&original,&patched,sizeof(original))&&captured_sp-saved_sp==shift,"MIDI detour preserves GPR/APSR/FPSCR/d0-d31/SP");
  require(!memcmp(&probe_saved,capture_after_pair[n]?&original:&in,sizeof(in)),"MIDI exact pre/post-pair capture ABI");checked++;
 }
 require(checked==19,"all MIDI pairs replayed");require(!munmap(stub,4096),"MIDI replay stub retired");
 for(unsigned k=0;k<count;k++)require(!munmap((void*)(uintptr_t)pages[k],4096),"MIDI replay page retired");
 puts("PASS all19 exact ARM MIDI pairs through production stub/gate: preserved registers, flags and SP; exact capture order (native continuations and callback substituted)");
}
static void drum_fixture_reset(void){
 memset(&command_state->drum_midi,0,sizeof(command_state->drum_midi));drum_midi_depth=0;memset(&drum_midi_last_root,0,sizeof(drum_midi_last_root));
 atomic_store(&mirror_state->heartbeat,100);atomic_store(&command_state->drum_midi.started,1);atomic_store(&command_state->drum_midi.until,200);
}
static void drum_hook(unsigned site){call(&drum_ctx.c,site);}
static void drum_root(unsigned track){
 drum_ctx.c=context();drum_ctx.c.r[4]=ptr(track_objects[track]);drum_ctx.c.r[2]=ptr(drum_note_event);drum_ctx.c.r[5]=0x11110000;drum_ctx.c.r[6]=12;
 drum_ctx.stack[0x138/4]=2;drum_ctx.stack[0x13c/4]=0x7ffffff8;drum_ctx.stack[0x134/4]=0x1fe46b4;
 drum_hook(DMP_ROOT);
}
static void drum_map(unsigned track){
 drum_ctx.c.r[5]=ptr(track_objects[track]);drum_ctx.c.r[2]=ptr(drum_note_event);drum_hook(DMP_MAP);
 memcpy((void*)(uintptr_t)(source_sp(&drum_ctx.c)+0xb8),drum_note_event,sizeof(drum_note_event));
 put(source_sp(&drum_ctx.c)+0xf0,1);drum_hook(DMP_MAP_END);
}
static void drum_output(int off){
 drum_ctx.c.r[1]=off?0x81:0x91;drum_ctx.c.r[2]=72;drum_ctx.c.r[3]=off?0:100;drum_ctx.c.r[4]=0x22220000;drum_ctx.c.r[5]=13;drum_ctx.c.r[6]=off?0x3456:13;drum_ctx.c.r[7]=0x3456;
 drum_hook(off?DMP_OFF_BYTES:DMP_ON_BYTES);
 drum_ctx.c.r[0]=ptr(drum_sink);drum_ctx.c.r[1]=0x3456;drum_ctx.c.r[2]=0x22220000;drum_ctx.c.r[8]=ptr(drum_sink);drum_ctx.c.r[9]=MODEL_PROGRAM_CONTROL;
 drum_hook(off?DMP_OFF_SINK:DMP_ON_SINK);
}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 alarm(30);exercise_adjust=4096;drum_pair_replay();fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"fixture allocation");
 initial(2);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);
 seed_fixture(0,0,0x3f000000,1);seed_fixture(1,0,0x3f000000,1);load_fixture();
 put(ptr(program_objects[1]),MODEL_MIDIPROGRAM_VPTR);
 uint32_t d=ptr(program_objects[0]),i=ptr(drum_instrument);
 put(d+0x1fec,ptr(drum_pad_vector));put(d+0x1ff0,ptr(drum_pad_vector)+sizeof(drum_pad_vector));put(d+0x1ff4,ptr(drum_pad_vector)+sizeof(drum_pad_vector));
 put(i,MODEL_INSTRUMENT_VPTR);put(i+MODEL_INSTRUMENT_PARENT,d);put(i+MODEL_INSTRUMENT_SLOT,5);put(d+0x1370+36*4,5);put(i+0x13a0,72);put(i+0x1374,1);
 pad_birth(i,d);PadOwner *o=pad_owner(i);require(o!=NULL,"retained Instrument fixture");atomic_store(&o->complete,1);
 require(pad_membership_begin(d,5,0,0,0x8000),"native membership entry");drum_pad_vector[5]=i;pad_membership_end(0x8000-0x58,0);require(!atomic_load(&fixture->pads.error),"native membership publication");
 /* A sink vptr may belong to a separate native library, not MPC text. */
 drum_sink[0]=0x76543210;
 drum_note_event[2]=3|(1<<12);drum_note_event[4]=0x3f800000;drum_note_event[12]=36;
 drum_fixture_reset();drum_root(0);drum_map(0);
 drum_ctx.c.r[5]=ptr(track_objects[0]);drum_ctx.c.r[0]=ptr(track_objects[0])+0x41c;put(drum_ctx.c.r[0]+0x18,ptr(track_objects[1])+0x43c);drum_ctx.c.r[2]=ptr(drum_note_event);drum_hook(DMP_SEND);
 drum_output(0);drum_hook(DMP_SEND_END);drum_hook(DMP_ROOT_END);
 memcpy(drum_pending,drum_note_event,sizeof(drum_note_event));drum_pending[14]=999;drum_pending[16]=36;drum_pending[17]=0x7ffffff8;drum_pending[18]=2;drum_pending[19]=1;drum_pending[20]=ptr(track_objects[0]);
 drum_ctx.c.r[4]=ptr(track_objects[0]);drum_ctx.c.r[5]=0x11110000;drum_ctx.c.r[1]=ptr(drum_pending);drum_hook(DMP_PENDING);
 require(!atomic_load(&command_state->drum_midi.error),"causal Drum to MIDI, mapped Instrument, pending association");
 /* Exact native Event move does not preserve these padding bytes. */
 drum_pending[3]=0xbd0cf79a;drum_pending[13]=0xbf800000;drum_pending[16]|=0x1c050800;drum_pending[21]|=0x646e00;
 drum_ctx.c.r[0]=ptr(drum_pending);drum_ctx.c.r[8]=0x11110000;drum_hook(DMP_DUE);
 drum_ctx.c.r[4]=ptr(track_objects[0]);drum_ctx.c.r[2]=36;drum_ctx.c.r[3]=1;drum_ctx.c.r[6]=0x11110000;drum_ctx.stack[0x40/4]=0x7ffffff8;drum_ctx.stack[0x48/4]=2;drum_hook(DMP_OFF_ROOT);
 drum_output(1);drum_hook(DMP_OFF_END);drum_hook(DMP_DUE_END);
 drum_ctx.c.r[5]=i+0x14;drum_ctx.c.r[7]=1;drum_hook(M_MUTE);drum_ctx.c.r[5]=i+0x148;drum_hook(M_MUTE);
 require(!atomic_load(&command_state->drum_midi.error)&&!drum_midi_depth,"duration expiry/off closes and both mute properties observed");
 unsigned first_count=atomic_load(&command_state->drum_midi.count);require(first_count==17,"complete first fixture receipt count");
 /* Same capture: physical output can precede downstream Drum mapping. */
 drum_root(1);drum_output(0);drum_map(1);
 drum_ctx.c.r[5]=ptr(track_objects[1]);drum_ctx.c.r[0]=ptr(track_objects[1])+0x41c;put(drum_ctx.c.r[0]+0x18,ptr(track_objects[0])+0x43c);drum_ctx.c.r[2]=ptr(drum_note_event);drum_hook(DMP_SEND);drum_map(0);drum_hook(DMP_SEND_END);drum_hook(DMP_ROOT_END);
 require(!atomic_load(&command_state->drum_midi.error)&&!drum_midi_depth,"MIDI to Drum preserves output-before-pad common invocation");
 /* Drain copies the pending value before the native route guard. Its closing
  * seam is the common join after either dispatch or the skipped route. */
 drum_root(0);drum_map(0);drum_output(0);drum_hook(DMP_ROOT_END);
 drum_pending[14]=1000;drum_ctx.c.r[4]=ptr(track_objects[0]);drum_ctx.c.r[5]=0x11110000;drum_ctx.c.r[1]=ptr(drum_pending);drum_hook(DMP_PENDING);
 drum_ctx.c.r[3]=ptr(drum_pending);drum_ctx.c.r[10]=0x11110000;drum_hook(DMP_DRAIN);drum_output(1);drum_hook(DMP_DRAIN_END);
 drum_ctx.c.r[0]=ptr(track_objects[0]);drum_ctx.c.r[10]=12;drum_ctx.c.r[11]=0;drum_hook(DMP_CLEANUP);drum_output(1);
 drum_note_event[2]=2;drum_note_event[12]=255;drum_map(0);drum_note_event[2]=3|(1<<12);drum_note_event[12]=36;
 drum_hook(DMP_CLEANUP_END);
 require(!atomic_load(&command_state->drum_midi.error)&&!drum_midi_depth,"pending drain and source-wide Track cleanup retain release provenance");
 unsigned good_count=atomic_load(&command_state->drum_midi.count);DrumMidiProbe *saved=malloc(sizeof(*saved));require(saved!=NULL,"fixture saved capture");memcpy(saved,&command_state->drum_midi,sizeof(*saved));
 drum_fixture_reset();uint32_t bias=observer_image_bias,payload[23]={0};observer_image_bias=0x400000;
 require(drum_midi_emit(&drum_ctx.c,DMP_MUTE,NULL,payload,0)==1&&atomic_load(&command_state->drum_midi.bias)==0x400000,"first receipt publishes actual runtime image bias after startup");observer_image_bias=bias;
 drum_fixture_reset();drum_ctx.c.r[3]=ptr(drum_pending);drum_ctx.c.r[10]=0x11110000;drum_hook(DMP_DRAIN);drum_hook(DMP_DRAIN_END);require(!atomic_load(&command_state->drum_midi.error)&&!drum_midi_depth&&atomic_load(&command_state->drum_midi.count)==2,"native skipped drain route closes without fabricated output");
 drum_fixture_reset();drum_hook(DMP_ROOT_END);require(atomic_load(&command_state->drum_midi.error)==DMP_NESTING&&atomic_load(&observer_running),"unmatched closure stops only diagnostic");
 drum_fixture_reset();unsigned lane=local_lane;local_lane=0;drum_root(0);require(atomic_load(&command_state->drum_midi.error)==DMP_UNENROLLED&&!local_lane,"diagnostic never enrolls thread");local_lane=lane;
 drum_fixture_reset();atomic_store(&command_state->drum_midi.count,DMP_RECORDS);drum_root(0);require(atomic_load(&command_state->drum_midi.error)==DMP_FULL&&atomic_load(&command_state->drum_midi.count)==DMP_RECORDS,"saturation stops without wrapping rows");
 drum_fixture_reset();for(unsigned n=0;n<DMP_FRAMES+1;n++)drum_root(0);require(atomic_load(&command_state->drum_midi.error)==DMP_NESTING&&drum_midi_depth==DMP_FRAMES,"sixteen-frame hard limit");
 drum_fixture_reset();drum_root(0);atomic_fetch_add(&fixture->epoch,1);drum_hook(DMP_ROOT_END);require(atomic_load(&command_state->drum_midi.error)==DMP_EPOCH,"project change cannot inherit frame identity");
 drum_fixture_reset();atomic_store(&fixture->heartbeat,201);drum_root(0);require(!atomic_load(&command_state->drum_midi.count),"elapsed capture does no native work");
 memcpy(&command_state->drum_midi,saved,sizeof(*saved));free(saved);atomic_store(&command_state->drum_midi.until,0);
 command_state->magic=COMMAND_MAGIC;command_state->version=COMMAND_VERSION;command_state->bytes=sizeof(*command_state);command_state->capacity=COMMAND_SLOTS;command_state->pid=getpid();command_state->seconds=0;
 int fd=open(argv[1],O_WRONLY|O_CREAT|O_TRUNC,0600);require(fd>=0&&write(fd,command_state,sizeof(*command_state))==(ssize_t)sizeof(*command_state)&&!close(fd),"fixture state exported for real client");
 printf("PASS %u production-dispatch diagnostic records: both route orders, mapped pad, both mute properties, pending/expiry/off, drain and Track cleanup; bounds, epoch, no-enrollment, expiry failures (native objects, scheduling and output substituted)\n",good_count);return 0;
}
