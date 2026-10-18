#ifndef MPCLEARN_MCU_SIMULATOR_CODEC_H
#define MPCLEARN_MCU_SIMULATOR_CODEC_H

#include <stddef.h>
#include <stdint.h>

enum {
 MCU_WIRE_MAX=6,
 MCU_LCD_BYTES=112,
 MCU_STRIPS=8
};

typedef struct {
 unsigned char bytes[MCU_WIRE_MAX];
 size_t length;
} McuWire;

typedef enum {
 MCU_FEEDBACK_OTHER=0,
 MCU_FEEDBACK_FADER,
 MCU_FEEDBACK_LED,
 MCU_FEEDBACK_RING,
 MCU_FEEDBACK_LCD,
 MCU_FEEDBACK_HEARTBEAT,
 MCU_FEEDBACK_MALFORMED
} McuFeedbackKind;

typedef struct {
 McuFeedbackKind kind;
 unsigned channel;
 unsigned value;
 unsigned offset;
 unsigned count;
 unsigned zone,port;
} McuFeedbackEvent;

typedef struct {
 int fader[9];
 int led[128];
 int ring[MCU_STRIPS];
 int hui_led[16][16];
 unsigned hui_zone,hui_zone_valid,heartbeat;
 unsigned char lcd[MCU_LCD_BYTES];
 unsigned counts[MCU_FEEDBACK_MALFORMED+1];
 unsigned total;
} McuFeedbackState;

int mcu_wire_fader(McuWire *wire,unsigned channel,unsigned position);
int mcu_wire_note(McuWire *wire,unsigned note,int down);
int mcu_wire_encoder(McuWire *wire,unsigned strip,int delta);
int mcu_wire_jog(McuWire *wire,int delta);
int hui_wire_fader(McuWire *wire,unsigned strip,unsigned position);
int hui_wire_switch(McuWire *wire,unsigned zone,unsigned port,int down);
int hui_wire_encoder(McuWire *wire,unsigned strip,int delta);
int hui_wire_heartbeat_reply(McuWire *wire);
void mcu_feedback_init(McuFeedbackState *state);
McuFeedbackEvent mcu_feedback_decode(McuFeedbackState *state,const unsigned char *bytes,size_t length);
McuFeedbackEvent hui_feedback_decode(McuFeedbackState *state,const unsigned char *bytes,size_t length);
const char *mcu_feedback_kind_name(McuFeedbackKind kind);

#endif
