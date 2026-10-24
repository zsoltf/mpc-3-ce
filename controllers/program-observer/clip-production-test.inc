#include "clip-request.h"
static uint32_t clip_fixture_identity,clip_fixture_states[16][9],clip_fixture_rows[9],clip_fixture_emitted[32][3],clip_fixture_emits;
static uint32_t clip_fixture_track_lookup(uint32_t lanes,unsigned column){(void)lanes;return column<16?tracks[0].track:0;}
static ClipNativeShared clip_fixture_lookup(uint32_t lanes,unsigned column,unsigned row){(void)lanes;return (ClipNativeShared){column<16&&row<9&&clip_fixture_states[column][row]>=CLIP_READY?clip_fixture_identity:0,0};}
static unsigned clip_fixture_state(uint32_t lanes,unsigned column,unsigned row){(void)lanes;return column<16&&row<9?clip_fixture_states[column][row]:0;}
static unsigned clip_fixture_scene(uint32_t lanes,unsigned row){(void)lanes;return row<9?clip_fixture_rows[row]:0;}
static unsigned clip_fixture_emit(unsigned kind,uint32_t lanes,unsigned column,unsigned row){(void)lanes;require(clip_fixture_emits<32,"bounded fixture native calls");unsigned *v=clip_fixture_emitted[clip_fixture_emits++];v[0]=kind;v[1]=column;v[2]=row;return 1;}
#ifdef MODEL_FIRMWARE_310
static uint32_t edit_objects[16][9][0x400/4],edit_present[16][9],edit_payload[16][9],edit_calls,edit_record_checks,edit_recording_column,edit_native_result,edit_audio_type;
static ClipNativeShared edit_lookup(uint32_t lanes,unsigned column,unsigned row){(void)lanes;return (ClipNativeShared){column<16&&row<9&&edit_present[column][row]?ptr(edit_objects[column][row]):0,0};}
static unsigned edit_audio(uint32_t address,uint32_t track){require(address==0x29c9bdc&&track==tracks[0].track,"real native audio-compatibility target and track ABI");return edit_audio_type;}
static unsigned edit_recording(uint32_t address,uint32_t lanes,unsigned column){require(address==0xe65e4c&&lanes==word(root+MODEL_ROOT_LANES),"real native recording predicate target/receiver");edit_record_checks++;return column==edit_recording_column;}
static unsigned edit_call(uint32_t address,uint32_t lanes,unsigned sc,unsigned sr,unsigned dc,unsigned dr){
 require(lanes==word(root+MODEL_ROOT_LANES)&&dr>0&&dr<9,"edit facade exact native lanes and row");edit_calls++;
 if(edit_native_result){require(address==0x101ca8c&&sc==0&&dc==0&&sr==dr,"same-row request reaches exact native CopyRow target and arguments");return edit_native_result;}
 if(address==0x1019b40){require(sc<16&&dc<16&&sr>0&&sr<9,"CopyClip real target and full source/destination ABI");edit_present[dc][dr]=edit_present[sc][sr];edit_payload[dc][dr]=edit_payload[sc][sr];}
 else if(address==0x101ca8c){require(sc==0&&dc==0&&sr>0&&sr<9,"CopyRow real dedicated target, row arguments");for(unsigned c=0;c<16;c++){edit_present[c][dr]=edit_present[c][sr];edit_payload[c][dr]=edit_payload[c][sr];}}
 else {require(address==0x101ded0&&dc<16,"DeleteClip real native target and destination ABI");edit_present[dc][dr]=0;}
 return C_OK;
}
static void edit_delete_row(uint32_t address,uint32_t lanes,unsigned row){
 require(address==0x1020224&&lanes==word(root+MODEL_ROOT_LANES)&&row==2,"DeleteRow uses exact native void facade with lanes and positive model row");edit_calls++;
}
static unsigned edit_insert_row(uint32_t address,uint32_t lanes,unsigned row,unsigned count){
 require(lanes==word(root+MODEL_ROOT_LANES)&&row==2&&((address==0x1019a3c&&count==1)||(address==0x101a1d0&&!count)),"InsertRows count1 or InsertAndCapture exact native target/receiver/row ABI");edit_calls++;return edit_native_result;
}
static void edit_snapshot(uint32_t address,uint32_t receiver,unsigned row){
 require((address==0x101a820||address==0x101a840)&&receiver==word(root+MODEL_ROOT_LANES)+0xd0&&row==2,"Snapshot/Clear uses exact native secondary subobject and void row ABI");edit_calls++;
}
static uint64_t edit_expected_loop;
static unsigned edit_length(uint32_t address,uint32_t sequence,uint32_t track,unsigned row,uint32_t factory,uint32_t manager,uint32_t monitor,uint32_t track_pool,uint32_t clip_pool){
 uint32_t lanes=word(root+MODEL_ROOT_LANES);require(sequence==word(project+0xe4)&&track==tracks[0].track&&row==1&&factory==word(lanes+0x17c)&&clip_pool==word(lanes+0x1c0),"explicit clip length action uses rooted sequence, target Track/row, factory and pool");
 if(address==0x29d89c4)require(manager==word(lanes+0x180)&&monitor==word(lanes+0x1b0)&&!track_pool,"Double with events exact seven-argument native ABI");
 else if(address==0x29d7b8c)require(track_pool==word(project+0xb1c)&&!manager&&!monitor,"native /2 exact six-argument ABI");
 else require(address==0x29d7874&&!manager&&!monitor&&!track_pool,"native 2X exact five-argument ABI");
 edit_calls++;return edit_native_result;
}
static void edit_loop_shift(uint32_t address,uint32_t factory,unsigned margin,uint64_t start,uint32_t sequence,uint32_t name,unsigned row){
 require(address==0x1ab3184&&factory==word(word(root+MODEL_ROOT_LANES)+0x17c)&&margin==2&&start==edit_expected_loop&&sequence==word(project+0xe4)&&name==tracks[0].track+0x468&&row==1,"native SetClipMargin exact64-bit ABI, target name/row and native arrow payload");edit_calls++;
}
static void edit_quantize_midi(uint32_t address,uint32_t program,uint32_t quantiser,uint32_t events,uint32_t manager){
 require(address==0x1fd35c8&&program==word(tracks[0].track+0x648)&&quantiser==word(root+0x23c)&&events==ptr(edit_objects[0][1])+0x3b8&&manager==word(word(root+MODEL_ROOT_LANES)+0x180),"native MIDI TC Do It receives chosen clip event list and shared current Quantiser, never selected clip");edit_calls++;
}
static void edit_quantize_audio(uint32_t address,uint32_t quantiser,uint32_t track,const ClipNativeShared *clip,uint32_t tempo,uint32_t manager){
 require(address==0x1fd3d2c&&quantiser==word(root+0x23c)&&track==tracks[0].track&&clip->object==ptr(edit_objects[0][1])&&tempo==word(word(root+MODEL_ROOT_AUDIO)+MODEL_AUDIO_TIME)+0xe8&&manager==word(word(root+MODEL_ROOT_LANES)+0x180),"native audio TC Do It receives retained target Clip and rooted SequencerTempo");edit_calls++;
}
static void edit_clip_actions(ClipCopy *snapshot,unsigned now){
 static unsigned char sequence[0x400],quantiser[0x208],division[16],audio[0x400],time[0x200];static uint32_t owners[5];
 uint32_t lanes=word(root+MODEL_ROOT_LANES),saved[]={word(project+0xe4),word(root+0x23c),word(root+0x1a0),word(root+MODEL_ROOT_AUDIO)};
 uint32_t offsets[]={lanes+0x17c,lanes+0x180,lanes+0x1b0,lanes+0x1c0,project+0xb1c},saved_owners[5];for(unsigned i=0;i<5;i++){saved_owners[i]=word(offsets[i]);put(offsets[i],ptr(owners+i));}
 put(ptr(sequence),MODEL_SEQUENCE_VPTR);put(project+0xe4,ptr(sequence));put(root+0x23c,ptr(quantiser));put(root+0x1a0,ptr(division));put(ptr(quantiser)+0x204,ptr(division));put(ptr(audio),MODEL_AUDIO_VPTR);put(root+MODEL_ROOT_AUDIO,ptr(audio));put(ptr(audio)+MODEL_AUDIO_TIME,ptr(time));put(ptr(time),MODEL_CLIP_QUANTIZE_TIME_VPTR);
 component_clip_length=edit_length;component_clip_loop_shift=edit_loop_shift;component_clip_quantize_midi=edit_quantize_midi;component_clip_quantize_audio=edit_quantize_audio;edit_native_result=1;edit_audio_type=0;
 unsigned intents[]={CLIP_DOUBLE_EVENTS,CLIP_QUANTIZE,CLIP_LOOP_HALF,CLIP_LOOP_DOUBLE,CLIP_LOOP_EARLIER,CLIP_LOOP_LATER};
 uint32_t clip=ptr(edit_objects[0][1]);put(clip+0x218,96);put(clip+0x21c,0);put(clip+0x258,128);put(clip+0x25c,0);put(clip+0x260,1);
 for(unsigned i=0;i<6;i++){
  CommandRequest r;require(clip_request_bind(&r,snapshot,0,intents[i],0,0,now),"explicit clip edit binds target identity");unsigned calls=edit_calls,checks=edit_record_checks;edit_expected_loop=i==4?0:224;unsigned at=submit_fixture(r);
  require(edit_calls==calls+1&&edit_record_checks==checks&&!atomic_load(&command_state->slots[at].rejected),"explicit clip action reaches native once without CE recording policy");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 }
 CommandRequest r;require(clip_request_bind(&r,snapshot,0,CLIP_LOOP_LATER,0,0,now),"loop no-op binds target");put(clip+0x260,0);edit_expected_loop=96;unsigned calls=edit_calls,at=submit_fixture(r);require(edit_calls==calls+1,"absent optional loop length preserves native same-start no-op dispatch");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 require(clip_request_bind(&r,snapshot,0,CLIP_QUANTIZE,0,0,now),"audio quantize binds target");edit_audio_type=1;calls=edit_calls;at=submit_fixture(r);require(edit_calls==calls+1,"audio quantize reaches separate native history owner");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();edit_audio_type=0;
 component_clip_length=NULL;component_clip_loop_shift=NULL;component_clip_quantize_midi=NULL;component_clip_quantize_audio=NULL;
 for(unsigned i=0;i<5;i++)put(offsets[i],saved_owners[i]);
 put(project+0xe4,saved[0]);put(root+0x23c,saved[1]);put(root+0x1a0,saved[2]);put(root+MODEL_ROOT_AUDIO,saved[3]);
}
static void clip_edit_owner_checks(void){
 for(unsigned c=0;c<16;c++)for(unsigned r=0;r<9;r++)edit_objects[c][r][0]=MODEL_CLIP_VPTR;
 memset(edit_present,0,sizeof(edit_present));memset(edit_payload,0,sizeof(edit_payload));edit_calls=edit_record_checks=edit_native_result=0;edit_recording_column=UINT32_MAX;
 component_clip_lookup=edit_lookup;component_clip_edit=edit_call;component_clip_audio=edit_audio;component_clip_recording=edit_recording;
 uint32_t lanes=word(root+MODEL_ROOT_LANES);unsigned now=atomic_load(&mirror_state->heartbeat);
 ClipCopy snapshot={.status=CLIP_SOURCE_READY,.tick=now,.epoch=epoch,.project_owner=channel_owner_id(project),.lanes=lanes,.columns=16,.rows=9};
 ClipProjection *v=snapshot.views;v->status=CLIP_SOURCE_READY;v->tick=now;v->first_row=1;for(unsigned c=0;c<8;c++)v->track_owner[c]=channel_owner_id(tracks[0].track);
 edit_present[0][1]=1;edit_payload[0][1]=11;v->cells[0]=(ClipCell){CLIP_READY,ptr(edit_objects[0][1])};
 ClipEditSource source;CommandRequest r;require(clip_edit_source(&source,&snapshot,0,0,0,0,now)&&clip_copy_request(&r,&snapshot,0,&source,1,0,now),"production copy source and destination request bind");
 /* Destination press is complete; release clears only the retained source.
  * Delayed native consumption reads the intervening source edit exactly once. */
 source=(ClipEditSource){0};edit_payload[0][1]=77;unsigned at=submit_fixture(r);
 require(!atomic_load(&command_state->slots[at].rejected)&&edit_calls==1&&edit_present[1][1]&&edit_payload[1][1]==77&&edit_payload[0][1]==77,"delayed consumption after release copies execution-time content once through production service");
 atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 unsigned calls=edit_calls;at=submit_fixture(r);require(atomic_load(&command_state->slots[at].rejected)==C_SOURCE_CHANGED&&edit_calls==calls,"destination occupied since submission cannot overwrite");command_retire();
 r.pad_index=2;edit_present[2][1]=1;edit_payload[2][1]=0;at=submit_fixture(r);require(atomic_load(&command_state->slots[at].rejected)==C_SOURCE_CHANGED&&edit_calls==calls,"allocated note-empty destination is occupied regardless of note count");command_retire();edit_present[2][1]=0;
 uint32_t clip=r.pad_owner;r.pad_owner^=4;at=submit_fixture(r);require(atomic_load(&command_state->slots[at].rejected)==C_SOURCE_CHANGED&&edit_calls==calls,"replaced source identity cannot silently retarget");command_retire();r.pad_owner=clip;
 r.project_owner++;at=submit_fixture(r);require(atomic_load(&command_state->slots[at].rejected)==C_IDENTITY&&edit_calls==calls,"project identity invalidates queued paste");command_retire();r.project_owner--;
 edit_recording_column=2;at=submit_fixture(r);require(atomic_load(&command_state->slots[at].rejected)==C_BUSY&&edit_calls==calls,"clip copy respects native track recording predicate before dispatch");command_retire();edit_recording_column=UINT32_MAX;
 v->cells[1]=(ClipCell){CLIP_READY,ptr(edit_objects[1][1])};require(clip_request_bind(&r,&snapshot,0,CLIP_DELETE,1,0,now),"Delete request binds exact existing clip");at=submit_fixture(r);require(edit_calls==calls+1&&!edit_present[1][1]&&edit_present[0][1],"production Delete reaches native facade and confirms slot removal only");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 edit_present[10][1]=1;edit_payload[10][1]=101;edit_present[15][2]=1;edit_payload[15][2]=999;
 require(clip_edit_source(&source,&snapshot,0,0,0,1,now)&&clip_copy_request(&r,&snapshot,0,&source,0,1,now),"row intent uses native rows independent of visible columns");unsigned checks=edit_record_checks;calls=edit_calls;edit_recording_column=15;at=submit_fixture(r);
 require(edit_calls==calls+1&&edit_record_checks==checks&&edit_present[10][2]&&edit_payload[10][2]==101&&!edit_present[15][2],"one dedicated native CopyRow delegates occupied offscreen replacement and recording eligibility, no CE per-column policy");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 require(clip_copy_request(&r,&snapshot,0,&source,0,0,now),"same-row producer delegates handling to native CopyRow");
 edit_native_result=C_NOT_READY;calls=edit_calls;at=submit_fixture(r);require(edit_calls==calls+1&&atomic_load(&command_state->slots[at].done),"same-row native refusal is one delivered intent, never replayed");
 MirrorInput in={.commands=command_state};InputFlight *tx=in.flights+at;tx->flight=atomic_load(&command_state->slots[at].published);tx->field=CLIP_ACTION;tx->bits=r.bits;tx->expires=r.expires;tx->step_request=r;tx->step_request.seq=tx->flight;MirrorBank bank;bank_init(&bank);MIRROR_COPY(copy);memset(&copy,0,sizeof(copy));copy.alive=copy.ready=1;copy.epoch=epoch;copy.heartbeat=now+1;
 command_retire();input_events(&in,tx,at);input_settle(&in,&bank,&copy,now+1);require(!in.error&&in.refused==1&&tx->acknowledged,"ordinary native refusal retires without claiming success or disabling controller bridge");command_retire();
 /* DeleteRow returns void: delivery must not depend on a guessed status
  * register or a CE scan of clip contents. Native mutation is substituted. */
 component_clip_delete_row=edit_delete_row;calls=edit_calls;checks=edit_record_checks;
 require(clip_request_bind(&r,&snapshot,0,CLIP_DELETE_ROW,0,1,now)&&!r.track_owner&&!r.effect_generation,"DeleteRow binds row identity without per-column content policy");
 CommandRequest bad=r;bad.project_owner++;at=submit_fixture(bad);require(atomic_load(&command_state->slots[at].rejected)==C_IDENTITY&&edit_calls==calls,"stale project refuses row deletion before native call");command_retire();
 bad=r;bad.pid++;at=submit_fixture(bad);require(atomic_load(&command_state->slots[at].rejected)==C_IDENTITY&&edit_calls==calls,"stale process refuses row deletion before native call");command_retire();
 bad=r;bad.effect_index=9;at=submit_fixture(bad);require(atomic_load(&command_state->slots[at].rejected)==C_IDENTITY&&edit_calls==calls,"removed row outside current native extent refuses before dispatch");command_retire();
 v->first_row=0;require(!clip_request_bind(&bad,&snapshot,0,CLIP_DELETE_ROW,0,0,now),"Arrangement never reaches native DeleteRow clear branch");v->first_row=1;
 at=submit_fixture(r);require(edit_calls==calls+1&&edit_record_checks==checks&&!atomic_load(&command_state->slots[at].rejected)&&atomic_load(&command_state->slots[at].done),"one void native DeleteRow call delivers without CE recording or per-clip checks");
 in=(MirrorInput){.commands=command_state};tx=in.flights+at;tx->flight=atomic_load(&command_state->slots[at].published);tx->field=CLIP_ACTION;tx->bits=r.bits;tx->expires=r.expires;tx->step_request=r;tx->step_request.seq=tx->flight;
 command_retire();input_events(&in,tx,at);input_settle(&in,&bank,&copy,now+1);require(!in.error&&in.settled==1&&tx->acknowledged&&edit_calls==calls+1,"void DeleteRow settles one native receipt without invented content completion or replay");command_retire();
 component_clip_insert_row=edit_insert_row;component_clip_delete_row=edit_snapshot;edit_native_result=0;
 for(unsigned intent=CLIP_INSERT_ROW;intent<=CLIP_CLEAR_SNAPSHOT;intent++){
  require(clip_request_bind(&r,&snapshot,0,intent,0,1,now),"native extended scene action binds ordinary row intent");calls=edit_calls;checks=edit_record_checks;at=submit_fixture(r);
  require(edit_calls==calls+1&&edit_record_checks==checks&&!atomic_load(&command_state->slots[at].rejected)&&atomic_load(&command_state->slots[at].done),"extended scene action dispatched once without CE recording/content policy");
  atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 }
 require(clip_request_bind(&r,&snapshot,0,CLIP_INSERT_CAPTURE,0,1,now),"capture refusal binds ordinary intent");edit_native_result=3;calls=edit_calls;at=submit_fixture(r);
 in=(MirrorInput){.commands=command_state};tx=in.flights+at;tx->flight=atomic_load(&command_state->slots[at].published);tx->field=CLIP_ACTION;tx->bits=r.bits;tx->expires=r.expires;tx->step_request=r;tx->step_request.seq=tx->flight;
 command_retire();input_events(&in,tx,at);input_settle(&in,&bank,&copy,now+1);require(!in.error&&in.refused==1&&tx->acknowledged&&edit_calls==calls+1,"native InsertAndCapture memory-admission refusal settles without replay");command_retire();
 edit_clip_actions(&snapshot,now);
 component_clip_insert_row=NULL;component_clip_delete_row=NULL;
 component_clip_edit=NULL;component_clip_audio=NULL;component_clip_recording=NULL;
 puts("PASS production clip editing: exact native dispatch targets/arguments, delayed execution-time source after release, destination/source/project identity checks, note-empty occupancy, Delete readback, single native whole-row replacement/recording/same-row delegation, native refusal settlement, void DeleteRow delivery/lifetime/Arrangement refusal, native row insert/capture/snapshot ABI and refusal, native clip Double/TC/loop target ABIs; native facades/content substituted");
}
#endif
static void clip_owner_checks(void){
 fixture=calloc(1,sizeof(*fixture));command_state=calloc(1,sizeof(*command_state));require(fixture&&command_state,"clip owner fixture allocation");
 static unsigned char lanes[0x250],clip[0x400];initial(1);command_initialize();observer_running=1;observer_image_bias=0;atomic_store(&command_state->alive,1);seed_fixture(0,0,0x3f000000,1);load_fixture();
 command_state->magic=COMMAND_MAGIC;command_state->version=COMMAND_VERSION;command_state->bytes=sizeof(*command_state);command_state->capacity=COMMAND_SLOTS;
 put(root+MODEL_ROOT_LANES,ptr(lanes));put(ptr(lanes),MODEL_LANES_VPTR);put(ptr(lanes)+0x178,root);put(ptr(lanes)+0x184,project);put(ptr(lanes)+0x34,16);put(ptr(lanes)+0x48,9);put(ptr(clip),MODEL_CLIP_VPTR);clip_fixture_identity=ptr(clip);
 component_clip_track=clip_fixture_track_lookup;component_clip_lookup=clip_fixture_lookup;component_clip_state=clip_fixture_state;component_clip_scene=clip_fixture_scene;component_clip_emit=clip_fixture_emit;
 memset(clip_fixture_states,0,sizeof(clip_fixture_states));clip_fixture_states[0][1]=CLIP_READY;clip_fixture_rows[1]=CLIP_PLAYING;clip_fixture_emits=0;clip_next_tick=0;memset(&clip_current,0,sizeof(clip_current));
 atomic_store(&command_state->clip_interest.epoch,epoch);atomic_store(&command_state->clip_interest.until,1000);atomic_store(&command_state->clip_interest.enabled,1);atomic_store(command_state->clip_interest.row,1);clip_service();
 require(clip_current.views[0].status==CLIP_SOURCE_READY&&clip_current.views[0].cells[0].state==CLIP_READY&&clip_current.views[0].scene_state[0]==CLIP_PLAYING,"real projection distinguishes cell from native whole-scene state");
 CommandRequest r;require(clip_request_bind(&r,&clip_current,0,CLIP_LAUNCH,0,0,0),"bind native clip identity");unsigned at=submit_fixture(r);
 require(clip_fixture_emits==1&&clip_fixture_emitted[0][0]==CLIP_LAUNCH&&clip_fixture_emitted[0][2]==1&&atomic_load(&command_state->slots[at].done),"normal launch reaches native facade once at scene row1");command_retire();
 MirrorInput in={.commands=command_state};InputFlight *tx=in.flights+at;tx->flight=r.seq=1;tx->field=CLIP_ACTION;tx->bits=r.bits;tx->expires=r.expires;tx->step_request=r;MirrorBank bank;bank_init(&bank);MIRROR_COPY(copy);copy.alive=copy.ready=1;copy.epoch=epoch;copy.heartbeat=1;
 input_events(&in,tx,at);input_settle(&in,&bank,&copy,1);require(!in.error&&tx->acknowledged&&clip_fixture_states[0][1]==CLIP_READY,"queue receipt settles without inventing playing state");command_retire();
 r.effect_generation=0;r.before_bits=CLIP_EMPTY;r.bits=CLIP_RECORD_TOGGLE;clip_fixture_states[0][1]=CLIP_EMPTY;put(tracks[0].track+0x494,0x100);at=submit_fixture(r);require(atomic_load(&command_state->slots[at].rejected)==C_NOT_READY&&clip_fixture_emits==1,"native byte arm remains off even with nonzero adjacent byte");command_retire();
 put(tracks[0].track+0x494,1);at=submit_fixture(r);require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emits==2&&clip_fixture_emitted[1][0]==CLIP_RECORD_TOGGLE,"explicit empty-slot recording uses independent native record intent");
 /* Retire this fixture receipt before another intent; there is no retry. */
 atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 r.effect_generation=clip_fixture_identity;r.before_bits=CLIP_RECORDING;clip_fixture_states[0][1]=CLIP_RECORDING;put(tracks[0].track+0x494,0);at=submit_fixture(r);require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emits==3&&clip_fixture_emitted[2][0]==CLIP_RECORD_TOGGLE,"state7 uses native RecordTo finish action without rearming");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 r.before_bits=CLIP_OVERDUBBING;clip_fixture_states[0][1]=CLIP_OVERDUBBING;at=submit_fixture(r);require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emits==4&&clip_fixture_emitted[3][0]==CLIP_LAUNCH,"state10 exits overdub through native launch, not record no-op");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 for(unsigned pending=CLIP_WILL_PLAY;pending<=CLIP_WILL_STOP_OVERDUBBING;pending++)if(pending!=CLIP_PLAYING&&pending!=CLIP_RECORDING&&pending!=CLIP_OVERDUBBING){r.before_bits=pending;clip_fixture_states[0][1]=pending;at=submit_fixture(r);require(atomic_load(&command_state->slots[at].rejected)==C_BUSY&&clip_fixture_emits==4,"queued recording transitions never dispatch another state-dependent action");command_retire();}
 r.before_bits=CLIP_EMPTY;at=submit_fixture(r);require(atomic_load(&command_state->slots[at].rejected)==C_SOURCE_CHANGED&&clip_fixture_emits==4,"stale record press cannot become another record toggle");command_retire();
 memset(clip_fixture_states,0,sizeof(clip_fixture_states));clip_fixture_states[2][1]=CLIP_PLAYING;clip_fixture_states[10][1]=CLIP_WILL_RECORD;clip_fixture_states[12][2]=CLIP_PLAYING;clip_fixture_states[14][1]=CLIP_READY;
 r.bits=CLIP_SCENE_STOP;r.track_owner=0;r.effect_generation=0;r.before_bits=CLIP_PLAYING;at=submit_fixture(r);
 require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emits==6&&clip_fixture_emitted[4][1]==2&&clip_fixture_emitted[5][1]==10,"scene stop spans all native columns and only scene participants, not visible8 or StopAll");
 atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 memset(clip_fixture_states,0,sizeof(clip_fixture_states));clip_fixture_states[0][0]=CLIP_READY;clip_fixture_states[0][7]=CLIP_PLAYING;clip_fixture_rows[0]=CLIP_READY;
 atomic_store(command_state->clip_interest.row,0);atomic_store(&fixture->heartbeat,20);clip_service();
 require(clip_current.views[0].status==CLIP_SOURCE_READY&&clip_current.views[0].first_row==0&&clip_current.views[0].cells[0].identity==clip_fixture_identity&&clip_current.views[0].cells[56].state==CLIP_PLAYING,"production first page includes native Arrangement and scene7");
 require(clip_request_bind(&r,&clip_current,0,CLIP_LAUNCH,0,0,20),"Arrangement launch binds native row0 identity");unsigned before=clip_fixture_emits;at=submit_fixture(r);
 require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emits==before+1&&clip_fixture_emitted[before][0]==CLIP_LAUNCH&&clip_fixture_emitted[before][2]==0,"Arrangement launch delegates row0 unchanged to native facade");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 r.bits=CLIP_STOP;at=submit_fixture(r);require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emitted[clip_fixture_emits-1][0]==CLIP_STOP,"Arrangement Stop retains native whole-track semantics");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 r.bits=CLIP_RECORD_TOGGLE;require(!command_clip_format(&r)&&!clip_request_bind(&r,&clip_current,0,CLIP_RECORD_TOGGLE,0,0,20),"Arrangement cannot form or publish an explicit clip-record request");
 require(clip_request_bind(&r,&clip_current,0,CLIP_LAUNCH,0,0,20),"stale Arrangement request starts with exact native identity");uint32_t identity=clip_fixture_identity;clip_fixture_identity=0;before=clip_fixture_emits;at=submit_fixture(r);require(atomic_load(&command_state->slots[at].rejected)==C_SOURCE_CHANGED&&clip_fixture_emits==before,"changed current Sequence Arrangement refuses the stale prior Clip identity");command_retire();clip_fixture_identity=identity;
 require(clip_request_bind(&r,&clip_current,0,CLIP_SCENE_LAUNCH,0,0,20),"whole Arrangement row binds through native row owner");at=submit_fixture(r);require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emitted[clip_fixture_emits-1][0]==CLIP_SCENE_LAUNCH&&clip_fixture_emitted[clip_fixture_emits-1][2]==0,"Arrangement row launch delegates native row0 participation");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 clip_fixture_states[2][0]=CLIP_PLAYING;clip_fixture_states[10][0]=CLIP_WILL_PLAY;clip_fixture_states[12][1]=CLIP_PLAYING;r.bits=CLIP_SCENE_STOP;before=clip_fixture_emits;at=submit_fixture(r);require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emits==before+2&&clip_fixture_emitted[before][1]==2&&clip_fixture_emitted[before+1][1]==10,"Arrangement row Stop visits all and only native row0 participants");atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
 atomic_store(command_state->clip_interest.row,8);atomic_store(&fixture->heartbeat,40);clip_fixture_states[0][8]=CLIP_READY;clip_service();require(clip_current.views[0].first_row==8&&clip_current.views[0].cells[0].state==CLIP_READY&&clip_current.views[0].cells[8].state==CLIP_NO_SLOT,"continued page begins at scene8 and excludes rows beyond native extent");
 {
  /* Pick: CLIP_SELECT reaches only the native SelectClip facade, never a launch,
     for clips and empty slots; it needs a real track. */
  unsigned before=clip_fixture_emits;clip_fixture_states[0][8]=CLIP_READY;
  require(clip_request_bind(&r,&clip_current,0,CLIP_SELECT,0,0,40)&&r.bits==CLIP_SELECT&&r.track_owner&&command_clip_format(&r),"pick binds a track-owned select request");
  at=submit_fixture(r);require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emits==before+1&&clip_fixture_emitted[before][0]==CLIP_SELECT&&clip_fixture_emitted[before][1]==0&&clip_fixture_emitted[before][2]==8,"pick calls native SelectClip once with the exact column/row");
  atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
  clip_fixture_states[1][8]=CLIP_EMPTY;
  require(clip_request_bind(&r,&clip_current,0,CLIP_SELECT,1,0,40),"pick on an empty slot binds");before=clip_fixture_emits;at=submit_fixture(r);
  require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emits==before+2&&clip_fixture_emitted[before][0]==CLIP_EMIT_CREATE&&clip_fixture_emitted[before][1]==1&&clip_fixture_emitted[before][2]==8&&clip_fixture_emitted[before+1][0]==CLIP_SELECT&&clip_fixture_emitted[before+1][1]==1&&clip_fixture_emitted[before+1][2]==8,"picking an empty slot creates a clip there with native CreateClip, then selects it");
  atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
  CommandRequest arrangement=r;arrangement.effect_index=0;clip_fixture_states[1][0]=CLIP_EMPTY;before=clip_fixture_emits;at=submit_fixture(arrangement);
  require(!atomic_load(&command_state->slots[at].rejected)&&clip_fixture_emits==before+1&&clip_fixture_emitted[before][0]==CLIP_SELECT&&!clip_fixture_emitted[before][2],"an empty Arrangement slot is only selected, never created");
  atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();clip_fixture_states[1][0]=clip_fixture_states[1][8]=0;
  CommandRequest bad=r;bad.track_owner=0;require(!command_clip_format(&bad),"a select request without a track owner is never published");
  bad=r;bad.pad_index=40;before=clip_fixture_emits;at=submit_fixture(bad);require(atomic_load(&command_state->slots[at].rejected)==C_IDENTITY&&clip_fixture_emits==before,"out-of-range column is refused before the native call");
  atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
  bad=r;bad.qlink_root^=4;before=clip_fixture_emits;at=submit_fixture(bad);require(atomic_load(&command_state->slots[at].rejected)==C_IDENTITY&&clip_fixture_emits==before,"a different lanes object is refused before the native call");
  atomic_store(&command_state->slots[at].settled,atomic_load(&command_state->slots[at].published));command_retire();
  puts("PASS pick: CLIP_SELECT reaches native SelectClip, an empty clip slot first gets native CreateClip (never the Arrangement row), never a launch; track, lanes and bounds refused first (native facades substituted)");
 }
 puts("PASS native Arrangement row0 projection/launch/track-stop/whole-row participation, explicit-record refusal, stale Sequence target refusal and scene8 page boundary; native lookup/facades substituted");
#ifdef MODEL_FIRMWARE_310
 clip_edit_owner_checks();
#endif
 component_clip_track=NULL;component_clip_lookup=NULL;component_clip_state=NULL;component_clip_scene=NULL;component_clip_emit=NULL;free(fixture);free(command_state);fixture=NULL;command_state=NULL;mirror_state=NULL;
 puts("PASS production clip projection/command/receipt: native cell and whole-row states, exact target, explicit armed empty recording/finish, stale record refusal and all-column participant scene stop; native lookup/facades and state substituted");
}
