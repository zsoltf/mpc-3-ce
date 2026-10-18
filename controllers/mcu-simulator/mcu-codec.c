#include "mcu-codec.h"

#include <string.h>

static int wire_three(McuWire *wire,unsigned a,unsigned b,unsigned c){
 if(!wire||a>255||b>127||c>127)return 0;
 wire->bytes[0]=(unsigned char)a;wire->bytes[1]=(unsigned char)b;wire->bytes[2]=(unsigned char)c;wire->length=3;return 1;
}
static int wire_six(McuWire *wire,unsigned a,unsigned b,unsigned c,unsigned d,unsigned e,unsigned f){
 if(!wire||a>255||b>127||c>127||d>255||e>127||f>127)return 0;
 unsigned char bytes[6]={(unsigned char)a,(unsigned char)b,(unsigned char)c,(unsigned char)d,(unsigned char)e,(unsigned char)f};memcpy(wire->bytes,bytes,6);wire->length=6;return 1;
}

int mcu_wire_fader(McuWire *wire,unsigned channel,unsigned position){
 if(channel>8||position>16383)return 0;
 return wire_three(wire,0xe0u+channel,position&0x7f,position>>7);
}

int mcu_wire_note(McuWire *wire,unsigned note,int down){
 if(note>127||(down!=0&&down!=1))return 0;
 /* MCU state is carried by Note On velocity. A zero-velocity Note On is the
  * release form accepted by the bridge and emitted by the observed Mini. */
 return wire_three(wire,0x90,note,down?0x7f:0);
}

static int relative(McuWire *wire,unsigned control,int delta){
 if(!delta||delta < -63||delta>63)return 0;
 unsigned value=delta>0?(unsigned)delta:0x40u+(unsigned)(-delta);
 return wire_three(wire,0xb0,control,value);
}

int mcu_wire_encoder(McuWire *wire,unsigned strip,int delta){
 if(strip>=MCU_STRIPS)return 0;
 return relative(wire,0x10u+strip,delta);
}

int mcu_wire_jog(McuWire *wire,int delta){return relative(wire,0x3c,delta);}

int hui_wire_fader(McuWire *wire,unsigned strip,unsigned position){
 if(strip>=MCU_STRIPS||position>16383)return 0;
 return wire_six(wire,0xb0,strip,position>>7,0xb0,0x20u+strip,position&0x7f);
}
int hui_wire_switch(McuWire *wire,unsigned zone,unsigned port,int down){
 if(zone>0x7f||port>0x0f||(down!=0&&down!=1))return 0;
 return wire_six(wire,0xb0,0x0f,zone,0xb0,0x2f,(down?0x40u:0)+port);
}
int hui_wire_encoder(McuWire *wire,unsigned strip,int delta){
 if(strip>=MCU_STRIPS||!delta||delta<-63||delta>63)return 0;
 unsigned value=delta>0?0x40u+(unsigned)delta:(unsigned)(-delta);return wire_three(wire,0xb0,0x40u+strip,value);
}
int hui_wire_heartbeat_reply(McuWire *wire){return wire_three(wire,0x90,0,0x7f);}

void mcu_feedback_init(McuFeedbackState *state){
 if(!state)return;
 memset(state,0,sizeof(*state));
 for(unsigned i=0;i<9;i++)state->fader[i]=-1;
 for(unsigned i=0;i<128;i++)state->led[i]=-1;
 for(unsigned i=0;i<MCU_STRIPS;i++)state->ring[i]=-1;
 for(unsigned zone=0;zone<16;zone++)for(unsigned port=0;port<16;port++)state->hui_led[zone][port]=-1;
 memset(state->lcd,' ',sizeof(state->lcd));
}

static McuFeedbackEvent result(McuFeedbackState *state,McuFeedbackKind kind,unsigned channel,unsigned value,unsigned offset,unsigned count){
 McuFeedbackEvent event={.kind=kind,.channel=channel,.value=value,.offset=offset,.count=count};
 if(state){state->total++;state->counts[kind]++;}
 return event;
}

McuFeedbackEvent hui_feedback_decode(McuFeedbackState *state,const unsigned char *bytes,size_t length){
 if(!state||!bytes||length!=3||bytes[1]>127||bytes[2]>127)return result(state,MCU_FEEDBACK_MALFORMED,0,0,0,0);
 if(bytes[0]==0x90&&bytes[1]==0&&!bytes[2]){state->heartbeat++;return result(state,MCU_FEEDBACK_HEARTBEAT,0,state->heartbeat,0,1);}
 if(bytes[0]!=0xb0)return result(state,MCU_FEEDBACK_OTHER,bytes[0]&0x0f,bytes[2],bytes[1],1);
 if(bytes[1]==0x0c){state->hui_zone=bytes[2];state->hui_zone_valid=1;return result(state,MCU_FEEDBACK_OTHER,0,bytes[2],0x0c,1);}
 if(bytes[1]==0x2c){
  if(!state->hui_zone_valid||(bytes[2]&0xb0))return result(state,MCU_FEEDBACK_MALFORMED,0,0,0,0);
  unsigned zone=state->hui_zone,port=bytes[2]&0x0f,on=(bytes[2]&0x40)!=0;state->hui_zone_valid=0;
  if(zone<16)state->hui_led[zone][port]=(int)on;
  McuFeedbackEvent event=result(state,MCU_FEEDBACK_LED,zone,on,port,1);event.zone=zone;event.port=port;return event;
 }
 if(bytes[1]>=0x10&&bytes[1]<0x18){unsigned strip=bytes[1]-0x10;state->ring[strip]=bytes[2];return result(state,MCU_FEEDBACK_RING,strip,bytes[2],0,1);}
 return result(state,MCU_FEEDBACK_OTHER,0,bytes[2],bytes[1],1);
}

McuFeedbackEvent mcu_feedback_decode(McuFeedbackState *state,const unsigned char *bytes,size_t length){
 if(!state||!bytes||!length)return result(state,MCU_FEEDBACK_MALFORMED,0,0,0,0);
 unsigned status=bytes[0];
 if((status&0xf0)==0xe0){
  if(length!=3||(status&0x0f)>8||bytes[1]>127||bytes[2]>127)return result(state,MCU_FEEDBACK_MALFORMED,0,0,0,0);
  unsigned channel=status&0x0f,value=(unsigned)bytes[1]|((unsigned)bytes[2]<<7);state->fader[channel]=(int)value;
  return result(state,MCU_FEEDBACK_FADER,channel,value,0,1);
 }
 if((status&0xf0)==0x90||(status&0xf0)==0x80){
  if(length!=3||(status&0x0f)!=0||bytes[1]>127||bytes[2]>127)return result(state,MCU_FEEDBACK_MALFORMED,0,0,0,0);
  unsigned note=bytes[1],on=(status&0xf0)==0x90&&bytes[2]>0;state->led[note]=(int)on;
  return result(state,MCU_FEEDBACK_LED,note,on,0,1);
 }
 if((status&0xf0)==0xb0){
  if(length!=3||bytes[1]>127||bytes[2]>127)return result(state,MCU_FEEDBACK_MALFORMED,0,0,0,0);
  if((status&0x0f)==0&&bytes[1]>=0x30&&bytes[1]<=0x37){
   unsigned strip=bytes[1]-0x30;state->ring[strip]=bytes[2];
   return result(state,MCU_FEEDBACK_RING,strip,bytes[2],0,1);
  }
  return result(state,MCU_FEEDBACK_OTHER,status&0x0f,bytes[2],bytes[1],1);
 }
 if(status==0xf0){
  if(length<9||bytes[length-1]!=0xf7||bytes[1]!=0||bytes[2]!=0||bytes[3]!=0x66||bytes[4]!=0x14||bytes[5]!=0x12||bytes[6]>=MCU_LCD_BYTES)
   return result(state,MCU_FEEDBACK_MALFORMED,0,0,0,0);
  unsigned offset=bytes[6];size_t count=length-8;
  if(count>MCU_LCD_BYTES-offset)return result(state,MCU_FEEDBACK_MALFORMED,0,0,0,0);
  for(size_t i=0;i<count;i++)if(bytes[7+i]>127)return result(state,MCU_FEEDBACK_MALFORMED,0,0,0,0);
  memcpy(state->lcd+offset,bytes+7,count);
  return result(state,MCU_FEEDBACK_LCD,offset/56,0,offset,(unsigned)count);
 }
 return result(state,MCU_FEEDBACK_OTHER,status&0x0f,0,0,0);
}

const char *mcu_feedback_kind_name(McuFeedbackKind kind){
 static const char *names[]={"other","fader","led","ring","lcd","heartbeat","malformed"};
 return kind<=MCU_FEEDBACK_MALFORMED?names[kind]:"invalid";
}
