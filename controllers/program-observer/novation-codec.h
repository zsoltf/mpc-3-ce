/* Native LCXL3 DAW and Launchpad Programmer codecs. Official protocol references and hardware
 * limits: docs/plans/2026-09-29__multi-controller-profiles.md. No MIDI owner. */
#ifndef MPC_NOVATION_CODEC_H
#define MPC_NOVATION_CODEC_H
#include <stdint.h>
#include <string.h>
#include "mcu-profile.h"
#include "clip-request.h"
enum {NOV_MESSAGE_LIMIT=16,NOV_BYTE_LIMIT=128,NOV_WIRE_MAX=64,NOV_LED_COUNT=128};
enum {NOV_NONE,NOV_ABSOLUTE,NOV_RELATIVE,NOV_TOUCH,NOV_BUTTON,NOV_MODE,NOV_LAYOUT,NOV_PROGRAMMER,NOV_FEATURE};
enum {NOV_SESSION,NOV_VOLUME,NOV_PAN,NOV_SEND,NOV_DEVICE,NOV_STEP64,NOV_STEP16,NOV_CLIPS,NOV_STEP_ENTER};
typedef struct {unsigned kind,index;int value;} NovationEvent;
typedef struct {unsigned length;unsigned char bytes[NOV_WIRE_MAX];} NovationWire;
typedef struct {
 uint32_t generation,query_tick;
 unsigned protocol,stage,query_mask,observed_mode,observed_layout,observed_page;
 unsigned pre_mode,pre_layout,pre_page,pre_valid,layer,control_page,layout_dirty;
 unsigned teardown_pending,admission_reported,shift,solo_arm,mute_select,bank;
 unsigned step_pad,step_first,step_epoch,step_program,step_bind_generation;
 unsigned clip_column,clip_row,clip_epoch;
 ClipEditSource copy_source;
 unsigned copy_used,copy_held;
 unsigned lc_round,lc_step,lc_received,lc_on,lc_queried,lc_correction,lc_display;
 unsigned char held[128],touch[32],led_valid[128],led_value[128],led_style[128];
 /* Encoder position 255 caches the delivered unavailable marker. */
 unsigned char position_valid[32],position[32];
 unsigned char fader_color_valid[8],fader_color[8];
 /* Clip column colour: the MPC colour seen and its palette entry. */
 uint32_t column_rgb[8];unsigned char column_palette[8];
 /* Pick: whether a pad was picked during the current step-view button hold,
  * and the step view opened once the native selection has been processed. */
 unsigned pick_used,pick_pending;uint32_t pick_tick;
 /* The step view this surface last used (NOV_STEP64 or NOV_STEP16; 0 none). */
 unsigned step_mode;
 /* XL3: the Track mixer bank offset kept while the Pad mixer is shown, and
  * a Shift+Select mixer change waiting for its track selection (track, view
  * wanted: 1 Pad mixer, 2 Track mixer). */
 unsigned track_offset,shift_view;uint32_t shift_track,shift_tick;
 /* Mixer pages: the page Custom reopens (a mixer layer, 0 none yet). Pro row
  * modes for the Track Select row (mixer_page is that layer + 1): the latched mode (NOV_ROW_*), the mode
  * button held now (its index, 0 none), when it was pressed and whether a
  * track or pad used it (then its release latches nothing). */
 unsigned mixer_page,row_mode,row_hold,row_used;uint32_t row_tick;
 /* Encoder LED colour sent per knob: palette index, or NOV_RGB|RGB. */
 uint32_t knob_led[24];
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
/* A Launchpad style with NOV_FLASH_FROM set flashes palette colour B (the
 * value) against an explicit colour A (the low seven bits) with the documented
 * LED lighting SysEx (command 03, lighting type 1: B then A). A channel-2
 * flash would alternate with whatever colour the pad showed before. */
enum {NOV_FLASH_FROM=0x80};
static inline NovationWire novation_led(unsigned protocol,unsigned index,unsigned style,unsigned value){
 if((style&NOV_FLASH_FROM)&&protocol!=MCU_PROTOCOL_LCXL3){
  NovationWire w=novation_sysex(protocol,3);novation_append(&w,1);novation_append(&w,index&127);novation_append(&w,value&127);novation_append(&w,style&127);novation_append(&w,0xf7);return w;
 }
 NovationWire w=novation_cc(style,index,value);
 if(protocol!=MCU_PROTOCOL_LCXL3&&index>=11&&index<=88&&index%10>=1&&index%10<=8)w.bytes[0]=(unsigned char)(0x90|style);
 return w;
}
/* MPC track colour to Novation LED colour. A track colour is the MPC's ARGB
 * word; its 8-bit channels map to the nearest 7-bit LED value. Zero means no
 * usable colour, so the caller keeps its palette default. */
enum {NOV_RGB=1u<<24};
static inline uint32_t novation_track_rgb(uint32_t argb){
 uint32_t r=((argb>>16)&255)>>1,g=((argb>>8)&255)>>1,b=(argb&255)>>1;
 return r|g|b?NOV_RGB|r<<16|g<<8|b:0;
}
/* Launchpad X / Pro Mk3 Programmer palette (identical "Colour palette" image
 * in both programmer's references). The image draws each LED channel v as
 * 0x61+v*0x9e; these entries invert that to 7-bit LED values. */
static const unsigned char novation_palette[128][3]={
 {0,0,0},{66,66,66},{100,100,100},{127,127,127},{127,66,66},{127,0,0},{100,0,0},{66,0,0},
 {127,117,93},{127,66,0},{100,35,0},{66,17,0},{127,113,51},{127,127,0},{100,100,0},{66,66,0},
 {100,127,51},{78,127,0},{51,100,0},{26,66,0},{78,127,66},{0,127,0},{0,100,0},{0,66,0},
 {78,127,78},{0,127,35},{0,100,17},{0,66,8},{78,127,86},{0,127,86},{0,100,51},{0,66,26},
 {78,127,117},{0,127,109},{0,100,78},{0,66,43},{78,117,127},{0,113,127},{0,82,100},{0,51,66},
 {78,100,127},{0,82,127},{0,51,100},{0,26,66},{51,35,127},{0,0,127},{0,0,100},{0,0,66},
 {86,66,127},{51,0,127},{26,0,100},{17,0,66},{127,66,127},{127,0,127},{100,0,100},{66,0,66},
 {127,66,93},{127,0,78},{100,0,51},{66,0,35},{127,17,0},{109,66,0},{100,78,0},{51,51,0},
 {0,66,0},{0,66,35},{0,35,93},{0,0,127},{0,66,66},{35,0,117},{86,66,78},{35,17,26},
 {127,0,0},{117,127,51},{113,125,0},{86,127,0},{17,100,0},{0,127,86},{0,109,127},{0,51,127},
 {35,0,127},{86,0,125},{113,35,100},{51,17,0},{127,51,0},{100,122,0},{93,127,35},{0,127,0},
 {66,127,51},{86,125,93},{66,127,120},{86,105,127},{51,78,120},{93,78,122},{122,35,127},{127,0,86},
 {127,78,0},{117,113,0},{105,127,0},{100,86,0},{66,51,0},{0,72,17},{17,78,35},{26,26,51},
 {26,35,86},{86,59,26},{100,0,0},{122,66,51},{122,72,17},{127,117,35},{109,122,51},{93,113,17},
 {26,26,51},{122,122,93},{100,125,105},{109,109,127},{105,93,127},{66,66,66},{93,93,93},{122,127,127},
 {109,0,0},{59,0,0},{26,120,0},{0,66,0},{117,113,0},{66,51,0},{113,78,0},{78,17,0}
};
/* Flashing and pulsing take palette entries only, so track colours use the
 * nearest non-dark entry (lowest index on a tie) in every clip state. */
static inline unsigned novation_palette_nearest(uint32_t rgb){
 int r=(int)((rgb>>16)&127),g=(int)((rgb>>8)&127),b=(int)(rgb&127);unsigned best=1;int best_distance=-1;
 for(unsigned i=1;i<128;i++){int dr=r-novation_palette[i][0],dg=g-novation_palette[i][1],db=b-novation_palette[i][2],d=dr*dr+dg*dg+db*db;if(best_distance<0||d<best_distance){best=i;best_distance=d;}}
 return best;
}
/* LCXL3 DAW-mode custom colour, programmer's reference v1.0 "RGB Colour":
 * F0 00 20 29 02 15 01 53 <control index> <R> <G> <B> F7. Encoder LEDs use
 * their absolute-mode index; components are 7-bit MIDI data. */
static inline NovationWire novation_lcxl3_led(unsigned index,uint32_t colour){
 if(!(colour&NOV_RGB))return novation_led(MCU_PROTOCOL_LCXL3,index,0,colour&127);
 NovationWire w=novation_sysex(MCU_PROTOCOL_LCXL3,1);novation_append(&w,0x53);novation_append(&w,index&127);
 novation_append(&w,(colour>>16)&127);novation_append(&w,(colour>>8)&127);novation_append(&w,colour&127);novation_append(&w,0xf7);return w;
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
  unsigned edge=(index>=91&&index<=98)||(index>=19&&index<=89&&index%10==9)||(s->protocol==MCU_PROTOCOL_LPP3&&(index==80||index==70||index==50||index==40||index==60||index==90||index==20||index==10||(index>=1&&index<=8)||(index>=101&&index<=108)));
  if((grid&&(type==0x90||type==0x80))||(edge&&type==0xb0)){event->kind=NOV_BUTTON;event->index=index;event->value=type!=0x80&&value!=0;return 1;}
 }return 0;
}
/* Profile translation only: a sound/step offset, never a native pattern copy.
 * The command owner must supply and validate actual drum/time identities. */
enum {NOV_STEP_VIEW=1,NOV_STEP_SOUND,NOV_STEP_PAGE,NOV_STEP_CELL};
typedef struct {unsigned kind,view,sound,step;int direction;} NovationStepEvent;
static inline int novation_step_view(unsigned layer){return layer==NOV_STEP64||layer==NOV_STEP16;}
/* Mixer pages: Volume, Pan, Sends, Device and (X only) the mixer grid. */
static inline int novation_mixer_page(unsigned layer){return layer==NOV_VOLUME||layer==NOV_PAN||layer==NOV_SEND||layer==NOV_DEVICE||layer==NOV_SESSION;}
enum {NOV_ROW_SELECT,NOV_ROW_ARM,NOV_ROW_MUTE,NOV_ROW_SOLO,NOV_ROW_STOP};
/* Pro row-mode buttons: Record Arm 1, Mute 2, Solo 3, Stop Clip 8. */
static inline unsigned novation_row_mode(unsigned index){return index==1?NOV_ROW_ARM:index==2?NOV_ROW_MUTE:index==3?NOV_ROW_SOLO:index==8?NOV_ROW_STOP:NOV_ROW_SELECT;}
/* Pan pads, bottom to top: both middle pads are the exact centre. */
static inline float novation_pan_value(unsigned row){static const float v[8]={0.0f,1.0f/6,2.0f/6,0.5f,0.5f,4.0f/6,5.0f/6,1.0f};return row>=1&&row<=8?v[row-1]:0.5f;}
static inline int novation_pan_lit(unsigned row,float pan){
 if(row<1||row>8||!(pan>=0&&pan<=1)){return 0;}
 float v=novation_pan_value(row),e=1e-4f;
 if(pan>=0.5f-e&&pan<=0.5f+e)return row==4||row==5;
 return pan<0.5f?row<=4&&pan<=v+e:row>=5&&pan>=v-e;
}
/* Programmer-mode LED addresses (programmer's references, "Programmer mode
 * layout"): the grid and right column 11-89 on both; X top row 91-98; Pro top
 * row 90-98, left column 10-80 and bottom rows 1-8 and 101-108. The logo (99)
 * is left alone. */
static inline unsigned novation_led_index(unsigned protocol,unsigned i){
 if(protocol!=MCU_PROTOCOL_LPX&&protocol!=MCU_PROTOCOL_LPP3)return 0;
 if(i>=11&&i<=89&&i%10>=1)return 1;
 if(i>=91&&i<=98)return 1;
 return protocol==MCU_PROTOCOL_LPP3&&(i==90||(i>=10&&i<=80&&i%10==0)||(i>=1&&i<=8)||(i>=101&&i<=108));
}
/* The step view a surface enters: the one it used last, else single sound. */
static inline unsigned novation_step_entry(const NovationState *n){return novation_step_view(n->step_mode)?n->step_mode:NOV_STEP64;}
static inline int novation_step_decode(unsigned protocol,unsigned layer,const NovationEvent *event,NovationStepEvent *out){
 *out=(NovationStepEvent){0};
 if((protocol!=MCU_PROTOCOL_LPX&&protocol!=MCU_PROTOCOL_LPP3)||event->kind!=NOV_BUTTON||!event->value)return 0;
 unsigned index=event->index,pro=protocol==MCU_PROTOCOL_LPP3;
 if(index==(pro?93u:95u)){out->kind=NOV_STEP_VIEW;out->view=NOV_SESSION;return 1;}
 /* The step-view button (X Note, Pro Sequencer) enters the step
  * view; pressed again inside it, it switches single (64 steps, one sound) and
  * four-sound (16 steps) views. Pro Steps switches them too. NOV_STEP_ENTER is an
  * entry, which takes the surface's last view. */
 if((!pro&&index==96)||(pro&&index==97)||(pro&&index==79&&novation_step_view(layer))){
  out->kind=NOV_STEP_VIEW;out->view=novation_step_view(layer)?(layer==NOV_STEP64?NOV_STEP16:NOV_STEP64):NOV_STEP_ENTER;return 1;
 }
 if(!novation_step_view(layer))return 0;
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
/* Modifier bits: 1 stop (X Capture MIDI / Pro Projects), 2 record (Session),
 * 4 pick (a held step-view button: X Note or Custom, Pro Sequencer). Pick makes
 * a clip the programming target without launching it. */
enum {NOV_CLIP_VIEW=1,NOV_CLIP_COLUMN,NOV_CLIP_ROW,NOV_CLIP_TRIGGER,NOV_CLIP_STOP,NOV_CLIP_RECORD,NOV_SCENE_TRIGGER,NOV_SCENE_STOP,NOV_CLIP_PICK};
/* The step-view button: X Note, Pro Sequencer. X Custom is the mixer pages. */
static inline unsigned novation_pick_button(unsigned protocol,unsigned index){return protocol==MCU_PROTOCOL_LPP3?index==97:protocol==MCU_PROTOCOL_LPX&&index==96;}
static inline unsigned novation_pick_held(unsigned protocol,const unsigned char *held){for(unsigned i=96;i<=97;i++)if(held[i]&&novation_pick_button(protocol,i))return 1;return 0;}
typedef struct {unsigned kind,view,column,row;int direction;} NovationClipEvent;
static inline int novation_clip_decode(unsigned protocol,unsigned layer,const NovationEvent *event,unsigned modifiers,NovationClipEvent *out){
 *out=(NovationClipEvent){0};
 if((protocol!=MCU_PROTOCOL_LPX&&protocol!=MCU_PROTOCOL_LPP3)||event->kind!=NOV_BUTTON||!event->value)return 0;
 unsigned index=event->index,pro=protocol==MCU_PROTOCOL_LPP3,stop=modifiers&1u;
 if(index==(pro?93u:95u)){out->kind=NOV_CLIP_VIEW;out->view=NOV_CLIPS;return 1;}
 if(layer!=NOV_CLIPS)return 0;
 if(index>=11&&index<=88&&index%10>=1&&index%10<=8){out->kind=stop?NOV_CLIP_STOP:(modifiers&2u)?NOV_CLIP_RECORD:(modifiers&4u)?NOV_CLIP_PICK:NOV_CLIP_TRIGGER;out->row=8-index/10;out->column=index%10-1;return 1;}
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
