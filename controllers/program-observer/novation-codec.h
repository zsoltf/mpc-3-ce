/* Native LCXL3 DAW and Launchpad Programmer codecs. Official protocol references and hardware
 * limits: docs/plans/2026-09-29__multi-controller-profiles.md. No MIDI owner. */
#ifndef MPC_NOVATION_CODEC_H
#define MPC_NOVATION_CODEC_H
#include <stdint.h>
#include <string.h>
#include "mcu-profile.h"
enum {NOV_MESSAGE_LIMIT=16,NOV_BYTE_LIMIT=128,NOV_WIRE_MAX=64,NOV_LED_COUNT=128};
enum {NOV_NONE,NOV_ABSOLUTE,NOV_RELATIVE,NOV_TOUCH,NOV_BUTTON,NOV_MODE,NOV_LAYOUT,NOV_PROGRAMMER,NOV_FEATURE};
enum {NOV_SESSION,NOV_VOLUME,NOV_PAN,NOV_SEND,NOV_DEVICE,NOV_STEP64,NOV_STEP16,NOV_CLIPS};
typedef struct {unsigned kind,index;int value;} NovationEvent;
typedef struct {unsigned length;unsigned char bytes[NOV_WIRE_MAX];} NovationWire;
typedef struct {
 uint32_t generation,query_tick;
 unsigned protocol,stage,query_mask,observed_mode,observed_layout,observed_page;
 unsigned pre_mode,pre_layout,pre_page,pre_valid,layer,control_page,layout_dirty;
 unsigned teardown_pending,admission_reported,shift,solo_arm,mute_select,bank;
 unsigned step_pad,step_first,step_epoch,step_program,step_bind_generation;
 unsigned clip_column,clip_row,clip_epoch;
 unsigned lc_round,lc_step,lc_received,lc_on,lc_queried,lc_correction,lc_display;
 unsigned char held[128],touch[32],led_valid[128],led_value[128],led_style[128];
 /* Encoder position 255 caches the delivered unavailable marker. */
 unsigned char position_valid[32],position[32];
 unsigned char fader_color_valid[8],fader_color[8];
 unsigned char text_valid[32],text[32][17];
 unsigned led_cursor,position_cursor,text_cursor;
 unsigned cycle_messages,cycle_bytes;
} NovationState;
static inline unsigned novation_device(unsigned protocol){return protocol==MCU_PROTOCOL_LCXL3?0x15:protocol==MCU_PROTOCOL_LPX?0x0c:protocol==MCU_PROTOCOL_LPP3?0x0e:0;}
static inline NovationWire novation_sysex(unsigned protocol,unsigned command){
 NovationWire w={.length=7,.bytes={0xf0,0,0x20,0x29,2,0,0}};w.bytes[5]=novation_device(protocol);w.bytes[6]=command;return w;
}
static inline void novation_append(NovationWire *w,unsigned value){if(w->length<NOV_WIRE_MAX)w->bytes[w->length++]=(unsigned char)value;}
static inline NovationWire novation_command(unsigned protocol,unsigned command,int value){NovationWire w=novation_sysex(protocol,command);if(value>=0)novation_append(&w,(unsigned)value);novation_append(&w,0xf7);return w;}
static inline NovationWire novation_cc(unsigned channel,unsigned index,unsigned value){NovationWire w={.length=3,.bytes={(unsigned char)(0xb0|channel),(unsigned char)index,(unsigned char)value}};return w;}
static inline NovationWire novation_led(unsigned protocol,unsigned index,unsigned style,unsigned value){
 NovationWire w=novation_cc(style,index,value);
 if(protocol!=MCU_PROTOCOL_LCXL3&&index>=11&&index<=88&&index%10>=1&&index%10<=8)w.bytes[0]=(unsigned char)(0x90|style);
 return w;
}
/* Programmer mode also supports bounded RGB/palette batches. Production uses
 * the smaller per-cell channel messages under the same shared byte budget. */
static inline NovationWire novation_led_batch(unsigned protocol,const unsigned char *indices,const unsigned char *values,unsigned count){
 NovationWire w=novation_sysex(protocol,3);if(count>16)count=16;
 for(unsigned i=0;i<count;i++){novation_append(&w,0);novation_append(&w,indices[i]&127);novation_append(&w,values[i]&127);}novation_append(&w,0xf7);return w;
}
static inline NovationWire novation_layout_value(unsigned protocol,unsigned layout,unsigned page){
 /* Restore the exact previously reported layout and page. */
 NovationWire w=novation_sysex(protocol,0);novation_append(&w,layout);
 if(protocol==MCU_PROTOCOL_LPP3){novation_append(&w,page);novation_append(&w,0);}novation_append(&w,0xf7);return w;
}
static inline void novation_reset(NovationState *s,unsigned protocol,uint32_t generation){memset(s,0,sizeof(*s));s->protocol=protocol;s->generation=generation;s->observed_mode=s->observed_layout=255;s->layer=protocol==MCU_PROTOCOL_LCXL3?NOV_SESSION:NOV_CLIPS;s->clip_row=0;s->step_pad=UINT32_MAX;}
static inline void novation_budget(NovationState *s){s->cycle_messages=s->cycle_bytes=0;}
static inline int novation_fits(const NovationState *s,const NovationWire *w){return w->length&&w->length<=NOV_WIRE_MAX&&s->cycle_messages<NOV_MESSAGE_LIMIT&&s->cycle_bytes+w->length<=NOV_BYTE_LIMIT;}
static inline int novation_decode(NovationState *s,const unsigned char *b,unsigned length,NovationEvent *event){
 *event=(NovationEvent){0};if(!b||!length)return 0;
 if(b[0]==0xf0){
  if(length<8||length>NOV_WIRE_MAX||memcmp(b,(unsigned char[]){0xf0,0,0x20,0x29,2},5)||b[5]!=novation_device(s->protocol)||b[length-1]!=0xf7)return 0;
  for(unsigned i=1;i+1<length;i++)if(b[i]>127)return 0;
  if(s->protocol==MCU_PROTOCOL_LCXL3)return 0;
  if(b[6]==0x0e&&length==9&&b[7]<=1&&s->protocol==MCU_PROTOCOL_LPX){event->kind=NOV_PROGRAMMER;event->value=b[7];return 1;}
  if(b[6]==0&&((s->protocol==MCU_PROTOCOL_LPX&&length==9)||(s->protocol==MCU_PROTOCOL_LPP3&&length==11&&b[9]==0))){event->kind=NOV_LAYOUT;event->index=length==11?b[8]:0;event->value=b[7];return 1;}return 0;
 }
 if(length!=3||b[1]>127||b[2]>127)return 0;
 unsigned type=b[0]&0xf0,channel=b[0]&15,index=b[1],value=b[2];
 if(s->protocol==MCU_PROTOCOL_LCXL3){
  if(type!=0xb0)return 0;
  if(channel==15&&index>=5&&index<=36){event->kind=NOV_ABSOLUTE;event->index=index-5;event->value=value;return 1;}
  if(channel==15&&index>=77&&index<=100){event->kind=NOV_RELATIVE;event->index=index-77+8;event->value=(int)value-64;return 1;}
  if(channel==14&&(value==0||value==127)){
   /* Relative mode offsets encoder touch CCs by64, just like movement. */
   if(index>=77&&index<=100)index-=64;
   if(index>=5&&index<=36){event->kind=NOV_TOUCH;event->index=index-5;event->value=value!=0;return 1;}
  }
  if(channel==6&&(index==69||index==72||index==73)&&(value==0||value==127)){event->kind=NOV_FEATURE;event->index=index==69?0:index==72?1:2;event->value=value!=0;return 1;}
  if(channel==6&&index==30){event->kind=NOV_MODE;event->value=value;return 1;}
  if((channel==0&&((index>=37&&index<=52)||index==65||index==66||index==102||index==103||index==106||index==107||index==116||index==118))||(channel==6&&index==63)){
   if(value!=0&&value!=127)return 0;
   event->kind=NOV_BUTTON;event->index=index;event->value=value!=0;return 1;
  }return 0;
 }
 if(channel==0&&(type==0x90||type==0x80||type==0xb0)){
  unsigned grid=index>=11&&index<=88&&index%10>=1&&index%10<=8;
  unsigned edge=(index>=91&&index<=98)||(index>=19&&index<=89&&index%10==9)||(s->protocol==MCU_PROTOCOL_LPP3&&(index==80||index==70||index==20||index==10||(index>=1&&index<=8)));
  if((grid&&(type==0x90||type==0x80))||(edge&&type==0xb0)){event->kind=NOV_BUTTON;event->index=index;event->value=type!=0x80&&value!=0;return 1;}
 }return 0;
}
/* Profile translation only: a sound/step offset, never a native pattern copy.
 * The command owner must supply and validate actual drum/time identities. */
enum {NOV_STEP_VIEW=1,NOV_STEP_SOUND,NOV_STEP_PAGE,NOV_STEP_CELL};
typedef struct {unsigned kind,view,sound,step;int direction;} NovationStepEvent;
static inline int novation_step_view(unsigned layer){return layer==NOV_STEP64||layer==NOV_STEP16;}
static inline int novation_step_decode(unsigned protocol,unsigned layer,const NovationEvent *event,NovationStepEvent *out){
 *out=(NovationStepEvent){0};
 if((protocol!=MCU_PROTOCOL_LPX&&protocol!=MCU_PROTOCOL_LPP3)||event->kind!=NOV_BUTTON||!event->value)return 0;
 unsigned index=event->index,pro=protocol==MCU_PROTOCOL_LPP3;
 if(index==(pro?93u:95u)){out->kind=NOV_STEP_VIEW;out->view=NOV_SESSION;return 1;}
 if((!pro&&(index==96||index==97))||(pro&&index==97)){
  out->kind=NOV_STEP_VIEW;out->view=!pro&&index==97?NOV_STEP16:NOV_STEP64;return 1;
 }
 if(!novation_step_view(layer))return 0;
 if(pro&&index==79){out->kind=NOV_STEP_VIEW;out->view=layer==NOV_STEP64?NOV_STEP16:NOV_STEP64;return 1;}
 unsigned previous=pro?80:91,next=pro?70:92;
 if(index==previous||index==next){out->kind=NOV_STEP_SOUND;out->direction=index==next?1:-1;return 1;}
 previous=pro?91:93;next=pro?92:94;
 if(index==previous||index==next){out->kind=NOV_STEP_PAGE;out->direction=index==next?1:-1;return 1;}
 if(index<11||index>88||index%10<1||index%10>8)return 0;
 unsigned row=8-index/10,column=index%10-1;
 out->kind=NOV_STEP_CELL;out->view=layer;out->sound=layer==NOV_STEP16?row/2:0;
 out->step=(layer==NOV_STEP16?row%2:row)*8+column;return 1;
}
/* Session translation chooses native intent only. Slot existence, launch/
 * recording state and quantization belong to the MPC clip owner. */
enum {NOV_CLIP_VIEW=1,NOV_CLIP_COLUMN,NOV_CLIP_ROW,NOV_CLIP_TRIGGER,NOV_CLIP_STOP,NOV_CLIP_RECORD,NOV_SCENE_TRIGGER,NOV_SCENE_STOP};
typedef struct {unsigned kind,view,column,row;int direction;} NovationClipEvent;
static inline int novation_clip_decode(unsigned protocol,unsigned layer,const NovationEvent *event,unsigned modifiers,NovationClipEvent *out){
 *out=(NovationClipEvent){0};
 if((protocol!=MCU_PROTOCOL_LPX&&protocol!=MCU_PROTOCOL_LPP3)||event->kind!=NOV_BUTTON||!event->value)return 0;
 unsigned index=event->index,pro=protocol==MCU_PROTOCOL_LPP3,stop=modifiers&1u;
 if(index==(pro?93u:95u)){out->kind=NOV_CLIP_VIEW;out->view=stop?NOV_SESSION:NOV_CLIPS;return 1;}
 if(layer!=NOV_CLIPS)return 0;
 if(index>=11&&index<=88&&index%10>=1&&index%10<=8){out->kind=stop?NOV_CLIP_STOP:(modifiers&2u)?NOV_CLIP_RECORD:NOV_CLIP_TRIGGER;out->row=8-index/10;out->column=index%10-1;return 1;}
 if(index>=19&&index<=89&&index%10==9){out->kind=stop?NOV_SCENE_STOP:NOV_SCENE_TRIGGER;out->row=8-index/10;return 1;}
 unsigned previous=pro?91:93,next=pro?92:94;
 if(index==previous||index==next){out->kind=NOV_CLIP_COLUMN;out->direction=index==next?1:-1;return 1;}
 previous=pro?80:91;next=pro?70:92;
 if(index==previous||index==next){out->kind=NOV_CLIP_ROW;out->direction=index==next?1:-1;return 1;}
 return 0;
}
static inline void novation_observe(NovationState *s,const NovationEvent *e){
 if(e->kind==NOV_PROGRAMMER){s->observed_mode=(unsigned)e->value;if(s->stage<=2){s->pre_mode=(unsigned)e->value;s->pre_valid|=1;}s->query_mask|=1;}
 if(e->kind==NOV_LAYOUT){if(s->protocol==MCU_PROTOCOL_LPP3)s->observed_mode=e->value==17;s->observed_layout=(unsigned)e->value;s->observed_page=e->index;if(s->stage<=2){s->pre_layout=(unsigned)e->value;s->pre_page=e->index;s->pre_valid|=2;if(s->protocol==MCU_PROTOCOL_LPP3){s->pre_mode=e->value==17;s->pre_valid|=1;}}s->query_mask|=2;}
 if(e->kind==NOV_MODE)s->observed_mode=(unsigned)e->value;
 if(e->kind==NOV_FEATURE&&e->index<3&&(s->lc_queried&(1u<<e->index))){unsigned bit=1u<<e->index;s->lc_received|=bit;if(e->value)s->lc_on|=bit;else s->lc_on&=~bit;}

}
static inline int novation_admitted(const NovationState *s){return s->stage>=16&&(s->protocol==MCU_PROTOCOL_LCXL3?s->lc_received==7&&s->lc_on==7:s->observed_mode==1);}
/* Only these pre-states have an accepted clean-exit contract. No mode change
 * or teardown is allowed while an unanswered/unsupported query stays here. */
static inline int novation_restorable(const NovationState *s){
 if(s->protocol==MCU_PROTOCOL_LCXL3)return 1;
 if(s->protocol==MCU_PROTOCOL_LPX)return s->pre_valid==3&&((s->pre_mode==1&&s->pre_layout==127)||(s->pre_mode==0&&(s->pre_layout==0||s->pre_layout==1||(s->pre_layout>=4&&s->pre_layout<=7)||s->pre_layout==13)));
 if(s->protocol!=MCU_PROTOCOL_LPP3||s->pre_valid!=3||s->pre_layout>19)return 0;
 unsigned limit=s->pre_layout==1||(s->pre_layout>=6&&s->pre_layout<=16)?4:s->pre_layout==3||s->pre_layout==19?8:s->pre_layout==18?5:1;
 return s->pre_page<limit;

}
static inline NovationWire novation_teardown(const NovationState *s){return novation_command(s->protocol,s->protocol==MCU_PROTOCOL_LCXL3?2:0x0e,0);}
#endif
