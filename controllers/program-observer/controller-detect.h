/* Detect: recognise connected known controllers by their fixed ALSA names and
 * append rows to a collection. Pure; the session owner persists the result.
 * Generic MCU/HUI and the Akai CE-program profiles are never recognised: their
 * ports do not identify the protocol or the loaded program. */
#ifndef MPCLEARN_CONTROLLER_DETECT_H
#define MPCLEARN_CONTROLLER_DETECT_H

#include "mcu-controller-preferences.h"
#include "native-preferences.h"

/* Names observed on the MPC's ALSA sequencer. LCXL3 numbers each unit in its
 * client name, and its DAW port repeats that name. */
static const McuProfile *controller_detect_profile(const char *client,const char *port){
 static const McuProfile *const fixed[]={&mcu_profile_xtouch,&mcu_profile_xtouch_mini};
 for(unsigned i=0;i<sizeof(fixed)/sizeof(fixed[0]);i++)
  if(!strcmp(client,fixed[i]->default_client)&&!strcmp(port,fixed[i]->default_port))return fixed[i];
 if(!strcmp(client,"Launchpad X")&&!strcmp(port,"LPX MIDI In"))return &mcu_profile_lpx;
 if(!strcmp(client,"Launchpad Pro MK3")&&!strcmp(port,"LPProMK3 MIDI"))return &mcu_profile_lpp3;
 size_t n=strlen(client);
 if(n>6&&!strncmp(client,"LCXL3 ",6)&&!strncmp(port,client,n)&&!strcmp(port+n," DAW In"))return &mcu_profile_lcxl3;
 return NULL;
}

static int controller_detect_uses(const McuControllerPreference *record,const char *client,const char *port){
 return (!strcmp(record->input.client,client)&&!strcmp(record->input.port,port))||
        (!strcmp(record->output.client,client)&&!strcmp(record->output.port,port));
}

typedef struct {unsigned added,full;} ControllerDetectResult;

/* A port already used by any row, enabled or not, is configured. A name that
 * appears twice is ambiguous and skipped. Existing rows are never changed. */
static ControllerDetectResult controller_detect(McuControllerCollection *collection,const NativePreferencesEndpoint *rows,unsigned count){
 ControllerDetectResult result={0,0};
 for(unsigned i=0;i<count;i++){
  const NativePreferencesEndpoint *row=rows+i;
  const char *client=(const char*)row->client,*port=(const char*)row->port;
  if(row->endpoint_type!=NATIVE_ENDPOINT_KERNEL||
     (row->capabilities&(NATIVE_ENDPOINT_FROM_CONTROLLER|NATIVE_ENDPOINT_TO_CONTROLLER))!=(NATIVE_ENDPOINT_FROM_CONTROLLER|NATIVE_ENDPOINT_TO_CONTROLLER)||
     !memchr(row->client,0,sizeof(row->client))||!memchr(row->port,0,sizeof(row->port)))continue;
  const McuProfile *profile=controller_detect_profile(client,port);if(!profile)continue;
  unsigned duplicate=0,configured=0;
  for(unsigned j=0;j<count;j++)if(j!=i&&!strcmp((const char*)rows[j].client,client)&&!strcmp((const char*)rows[j].port,port))duplicate=1;
  for(unsigned j=0;j<collection->count;j++)if(controller_detect_uses(collection->records+j,client,port))configured=1;
  if(duplicate||configured)continue;
  if(collection->count>=MCU_CONTROLLER_CAPACITY){result.full=1;break;}
  McuControllerPreference found;memset(&found,0,sizeof(found));found.enabled=1;
  McuEndpoint endpoint={.profile=profile,.client=client,.port=port,.physical_only=MCU_ENDPOINT_KERNEL};
  found.input=found.output=endpoint;
  if(!mcu_controller_preference_copy(collection->records+collection->count,&found))continue;
  collection->count++;result.added++;
 }
 return result;
}

#endif
