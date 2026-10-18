#include <stdio.h>
#include <stdlib.h>
#include "novation-codec.h"
static void require(int yes,const char *message){if(!yes){fprintf(stderr,"FAIL %s\n",message);exit(1);}}
static void wire(NovationWire actual,const unsigned char *expected,unsigned length){require(actual.length==length&&!memcmp(actual.bytes,expected,length),"exact official wire bytes");}
#define W(actual,...) do{const unsigned char expected[]={__VA_ARGS__};wire(actual,expected,sizeof(expected));}while(0)
static int decode(NovationState *s,NovationEvent *e,unsigned a,unsigned b,unsigned c){unsigned char bytes[]={(unsigned char)a,(unsigned char)b,(unsigned char)c};return novation_decode(s,bytes,3,e);}
static void step_translation_checks(void){
 for(unsigned protocol=MCU_PROTOCOL_LPX;protocol<=MCU_PROTOCOL_LPP3;protocol++){
  NovationState state;NovationEvent event;NovationStepEvent out;novation_reset(&state,protocol,1);unsigned pro=protocol==MCU_PROTOCOL_LPP3;
  const unsigned buttons[]={pro?97u:96u,pro?79u:97u,pro?93u:95u,pro?80u:91u,pro?70u:92u,pro?91u:93u,pro?92u:94u};
  const unsigned kinds[]={NOV_STEP_VIEW,NOV_STEP_VIEW,NOV_STEP_VIEW,NOV_STEP_SOUND,NOV_STEP_SOUND,NOV_STEP_PAGE,NOV_STEP_PAGE};
  for(unsigned i=0;i<7;i++){
   require(decode(&state,&event,0xb0,buttons[i],127)&&novation_step_decode(protocol,NOV_STEP64,&event,&out)&&out.kind==kinds[i],"model-specific documented button reaches step translation");
   if(i<3)require(out.view==(i==0?NOV_STEP64:i==1?NOV_STEP16:NOV_SESSION),"physical mode buttons choose exact view");
   else require(out.direction==((i==4||i==6)?1:-1),"sound/time arrows retain direction");
   require(decode(&state,&event,0xb0,buttons[i],0)&&!novation_step_decode(protocol,NOV_STEP64,&event,&out),"button release does not become another intent");
  }
  for(unsigned layer=NOV_STEP64;layer<=NOV_STEP16;layer++){
   unsigned visited[4][64]={{0}};
   for(unsigned note=11;note<=88;note++){
    if(note%10<1||note%10>8)continue;
    require(decode(&state,&event,0x90,note,64)&&novation_step_decode(protocol,layer,&event,&out)&&out.kind==NOV_STEP_CELL,"all64 Programmer grid notes translate");
    require(out.sound<(layer==NOV_STEP64?1u:4u)&&out.step<(layer==NOV_STEP64?64u:16u)&&!visited[out.sound][out.step]++,"grid maps uniquely into native sound/time offsets");
    require(decode(&state,&event,0x80,note,64)&&!novation_step_decode(protocol,layer,&event,&out),"grid release is not a step edit");
   }
   for(unsigned sound=0;sound<(layer==NOV_STEP64?1u:4u);sound++)for(unsigned step=0;step<(layer==NOV_STEP64?64u:16u);step++)require(visited[sound][step]==1,"each native cell offset is reachable once");
   require(decode(&state,&event,0x90,81,127)&&novation_step_decode(protocol,layer,&event,&out)&&!out.sound&&!out.step,"top-left begins first sound/step");
   require(decode(&state,&event,0x90,18,127)&&novation_step_decode(protocol,layer,&event,&out)&&out.sound==(layer==NOV_STEP64?0u:3u)&&out.step==(layer==NOV_STEP64?63u:15u),"bottom-right ends exact view");
  }
  require(decode(&state,&event,0x90,81,127)&&!novation_step_decode(protocol,NOV_SESSION,&event,&out),"existing mixer grid stays outside step translation");
  require(decode(&state,&event,0xb0,39,127)&&!novation_step_decode(protocol,NOV_STEP64,&event,&out),"transport retains existing owner");
 }
 NovationState x;NovationEvent e;novation_reset(&x,MCU_PROTOCOL_LPX,1);require(!decode(&x,&e,0xb0,80,127)&&!decode(&x,&e,0xb0,70,127),"Pro left arrows are not invented on X");
 puts("PASS documented Launchpad step grid/button translation for both models; native step owner, state, MIDI device and output remain untested here");
}
static void clip_translation_checks(void){
 for(unsigned protocol=MCU_PROTOCOL_LPX;protocol<=MCU_PROTOCOL_LPP3;protocol++){
  NovationState state;NovationEvent event;NovationClipEvent out;novation_reset(&state,protocol,1);unsigned pro=protocol==MCU_PROTOCOL_LPP3;
  for(unsigned stop=0;stop<2;stop++){
   require(decode(&state,&event,0xb0,pro?93:95,127)&&novation_clip_decode(protocol,NOV_STEP64,&event,stop,&out)&&out.kind==NOV_CLIP_VIEW&&out.view==(stop?NOV_SESSION:NOV_CLIPS),"Session reaches clips and modified Session retains mixer");
   for(unsigned row=0;row<8;row++){
    for(unsigned column=0;column<8;column++){
     unsigned note=(8-row)*10+column+1;
     require(decode(&state,&event,0x90,note,127)&&novation_clip_decode(protocol,NOV_CLIPS,&event,stop,&out)&&out.kind==(stop?NOV_CLIP_STOP:NOV_CLIP_TRIGGER)&&out.row==row&&out.column==column,"all64 clip coordinates preserve top-left track/scene orientation");
     require(decode(&state,&event,0x80,note,64)&&!novation_clip_decode(protocol,NOV_CLIPS,&event,stop,&out),"release cannot repeat clip intent");
    }
    require(decode(&state,&event,0xb0,(8-row)*10+9,127)&&novation_clip_decode(protocol,NOV_CLIPS,&event,stop,&out)&&out.kind==(stop?NOV_SCENE_STOP:NOV_SCENE_TRIGGER)&&out.row==row,"all eight right buttons reach scene launch/stop");
   }
  }
  require(decode(&state,&event,0x90,81,127)&&novation_clip_decode(protocol,NOV_CLIPS,&event,2,&out)&&out.kind==NOV_CLIP_RECORD,"held Session requests explicit native recording without global preference changes");
  require(novation_clip_decode(protocol,NOV_CLIPS,&event,3,&out)&&out.kind==NOV_CLIP_STOP,"stop modifier wins over simultaneous record chord");
  unsigned arrows[]={pro?91u:93u,pro?92u:94u,pro?80u:91u,pro?70u:92u};
  for(unsigned i=0;i<4;i++)require(decode(&state,&event,0xb0,arrows[i],127)&&novation_clip_decode(protocol,NOV_CLIPS,&event,0,&out)&&out.kind==(i<2?NOV_CLIP_COLUMN:NOV_CLIP_ROW)&&out.direction==(i%2?1:-1),"physical clip arrows preserve track/scene axes");
  require(decode(&state,&event,0x90,81,127)&&!novation_clip_decode(protocol,NOV_STEP64,&event,0,&out)&&!novation_clip_decode(protocol,NOV_SESSION,&event,0,&out),"clip translation cannot steal drum or mixer cells");
 }
 puts("PASS documented Launchpad Session/scene/stop-modifier translation; native clip state/actions and hardware untested");
}
int main(void){
 step_translation_checks();clip_translation_checks();
 NovationState s,peer;NovationEvent e;
 novation_reset(&s,MCU_PROTOCOL_LCXL3,7);novation_reset(&peer,MCU_PROTOCOL_LPX,8);
 W(novation_command(s.protocol,2,127),0xf0,0,0x20,0x29,2,0x15,2,0x7f,0xf7);
 W(novation_teardown(&s),0xf0,0,0x20,0x29,2,0x15,2,0,0xf7);
 for(unsigned i=0;i<32;i++)require(decode(&s,&e,0xbf,5+i,127)&&e.kind==NOV_ABSOLUTE&&e.index==i&&e.value==127,"all native LCXL3 absolute faders/encoders");
 for(unsigned i=0;i<24;i++)require(decode(&s,&e,0xbf,77+i,63)&&e.kind==NOV_RELATIVE&&e.index==i+8&&e.value==-1,"LCXL3 pivot64 relative rows");
 for(unsigned i=0;i<24;i++)require(!decode(&s,&e,0xb6,77+i,63),"LCXL3 host feature channel is not encoder input");
 for(unsigned i=0;i<32;i++)for(unsigned down=0;down<2;down++)require(decode(&s,&e,0xbe,5+i,down?127:0)&&e.kind==NOV_TOUCH&&e.index==i&&e.value==(int)down,"all LCXL3 absolute touch indices and edges");
 for(unsigned i=0;i<24;i++)for(unsigned down=0;down<2;down++)require(decode(&s,&e,0xbe,77+i,down?127:0)&&e.kind==NOV_TOUCH&&e.index==i+8&&e.value==(int)down,"relative touch normalizes to the same physical encoder");
 for(unsigned cc=0;cc<128;cc++)if(!((cc>=5&&cc<=36)||(cc>=77&&cc<=100)))require(!decode(&s,&e,0xbe,cc,127),"unmapped touch index rejected");
 require(!decode(&s,&e,0xbe,85,1)&&!decode(&s,&e,0xb0,85,127)&&!decode(&s,&e,0xbd,93,127),"relative touch rejects invalid edge/channel");
 require(!decode(&s,&e,0xbe,5,1)&&!decode(&s,&e,0xb0,5,127)&&!decode(&s,&e,0xbf,37,1),"wrong channel/range/touch rejection");
 for(unsigned row=0;row<3;row++)for(unsigned down=0;down<2;down++){unsigned cc=row==0?69:row==1?72:73;require(decode(&s,&e,0xb6,cc,down?127:0)&&e.kind==NOV_FEATURE&&e.index==row&&e.value==(int)down,"LCXL exact documented feature readback");}
 require(!decode(&s,&e,0xb7,69,127)&&!decode(&s,&e,0xb6,69,1),"feature query channel and malformed readback rejected");
 require(decode(&s,&e,0xb6,30,2)&&e.kind==NOV_MODE,"LCXL3 native mode report");novation_observe(&s,&e);require(s.observed_mode==2&&peer.observed_mode==255,"mode state is per instance");
 require(!decode(&s,&e,0xb0,37,64)&&decode(&s,&e,0xb0,37,127),"buttons only documented edges");
 for(unsigned protocol=MCU_PROTOCOL_LPX;protocol<=MCU_PROTOCOL_LPP3;protocol++){
  novation_reset(&s,protocol,9);unsigned char reply[]={0xf0,0,0x20,0x29,2,(unsigned char)novation_device(protocol),0,0,0,0,0xf7};
  unsigned size=protocol==MCU_PROTOCOL_LPX?9:11;if(size==9)reply[8]=0xf7;
  require(novation_decode(&s,reply,size,&e)&&e.kind==NOV_LAYOUT,"exact layout readback");novation_observe(&s,&e);require(s.pre_valid==(protocol==MCU_PROTOCOL_LPX?2u:3u)&&s.pre_layout==0,"pre-layout recorded only before admission");
  for(unsigned length=0;length<size;length++)require(!novation_decode(&s,reply,length,&e),"truncated SysEx rejects");
  reply[5]^=1;require(!novation_decode(&s,reply,size,&e),"other Novation product ignored");reply[5]^=1;reply[7]=0x80;require(!novation_decode(&s,reply,size,&e),"status byte in SysEx rejects");
  require(decode(&s,&e,0x90,11,127)&&e.kind==NOV_BUTTON&&e.index==11&&e.value,"Programmer pad press");
  require(decode(&s,&e,0x80,11,64)&&!e.value,"Programmer real note-off");
  require(!decode(&s,&e,0xb4,0,64),"Programmer ignores DAW fader channel5");
  require(!decode(&s,&e,0x98,11,127)&&!decode(&s,&e,0x9f,11,127)&&!decode(&s,&e,0xb5,0,64),"no drum/musical/injection or fader-colour input");
  require(!decode(&s,&e,0x90,0,127)&&!decode(&s,&e,0x90,99,127),"unmapped pad rejection");
  s.held[11]=s.touch[1]=s.led_valid[11]=1;novation_reset(&s,protocol,10);require(s.generation==10&&!s.held[11]&&!s.touch[1]&&!s.led_valid[11]&&!s.pre_valid,"connection reset drops physical/output/mode state");
 }
 W(novation_command(MCU_PROTOCOL_LPX,0x0e,-1),0xf0,0,0x20,0x29,2,0x0c,0x0e,0xf7);
 W(novation_command(MCU_PROTOCOL_LPP3,0x0e,1),0xf0,0,0x20,0x29,2,0x0e,0x0e,1,0xf7);
 W(novation_layout_value(MCU_PROTOCOL_LPX,4,0),0xf0,0,0x20,0x29,2,0x0c,0,4,0xf7);
 W(novation_layout_value(MCU_PROTOCOL_LPP3,17,0),0xf0,0,0x20,0x29,2,0x0e,0,17,0,0,0xf7);
 W(novation_led(MCU_PROTOCOL_LPX,11,2,21),0x92,11,21);
 W(novation_led(MCU_PROTOCOL_LPP3,39,1,5),0xb1,39,5);
 unsigned char indices[17]={11,12},values[17]={5,21};W(novation_led_batch(MCU_PROTOCOL_LPX,indices,values,2),0xf0,0,0x20,0x29,2,0x0c,3,0,11,5,0,12,21,0xf7);
 require(novation_led_batch(MCU_PROTOCOL_LPP3,indices,values,17).length==56,"Programmer LED batch bounded at16 entries");
 NovationWire one=novation_cc(0,1,2);novation_budget(&s);s.cycle_messages=16;require(!novation_fits(&s,&one),"message budget");s.cycle_messages=0;s.cycle_bytes=127;require(!novation_fits(&s,&one),"byte budget");
 puts("PASS native Novation codec exact bytes, malformed input, Programmer channel exclusions, per-instance mode/reset and output bounds; no device substituted as observed hardware");return 0;
}
