/* Explicit CE saved-program assignments, not device defaults or discovery.
 * Setup and primary references: docs/controller-programs.md. No MIDI owner. */
#ifndef MPC_CONTROL_PROGRAM_CODEC_H
#define MPC_CONTROL_PROGRAM_CODEC_H
#include "mcu-profile.h"
enum {CP_ABSOLUTE=1,CP_BUTTON,CP_TRANSPORT};
enum {CP_VOLUME,CP_PAN,CP_SEND1,CP_SEND2,CP_QLINK1,CP_QLINK2};
typedef struct {unsigned kind,index,bank,value;} ControlProgramEvent;
/* MMC simple-command subset; destination is a device ID, not a channel. */
static inline int control_program_mmc(unsigned protocol,const unsigned char *bytes,unsigned length,ControlProgramEvent *event){
 *event=(ControlProgramEvent){0};
 if(protocol!=MCU_PROTOCOL_MPK249||!bytes||length!=6||bytes[0]!=0xf0||bytes[1]!=0x7f||bytes[2]>127||bytes[3]!=6||bytes[5]!=0xf7)return 0;
 unsigned command=bytes[4];if(command!=1&&command!=2&&command!=3&&command!=6)return 0;
 *event=(ControlProgramEvent){CP_TRANSPORT,command,0,1};return 1;
}
static inline int control_program_decode(unsigned protocol,unsigned status,unsigned cc,unsigned value,ControlProgramEvent *event){
 *event=(ControlProgramEvent){0};if((status&0xf0)!=0xb0||status>255||cc>127||value>127)return 0;
 unsigned channel=status&15;
 if(protocol==MCU_PROTOCOL_MPK_MINI3||protocol==MCU_PROTOCOL_LPD8_MK2){
  if(channel==(protocol==MCU_PROTOCOL_MPK_MINI3?0u:15u)&&cc>=70&&cc<=77){*event=(ControlProgramEvent){CP_ABSOLUTE,cc-70,0,value};return 1;}
  if(channel==9&&cc>=40&&cc<=47){*event=(ControlProgramEvent){CP_BUTTON,cc-40,0,value};return 1;}
 }else if(protocol==MCU_PROTOCOL_MPK249){
  if(channel>2)return 0;
  if(cc>=16&&cc<=31){*event=(ControlProgramEvent){CP_ABSOLUTE,cc-16,channel,value};return 1;}
  if(cc>=40&&cc<=47&&(value==0||value==127)){*event=(ControlProgramEvent){CP_BUTTON,cc-40,channel,value};return 1;}

 }
 return 0;
}
#endif
