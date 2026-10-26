/* Finite MCU bank over MMV4; optional concurrent CMD9 input. Original MIT. */
#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include "mirror-motor-core.h"
#include "mirror-fresh.h"
#include "mcu-profile.h"
#include "hui-codec.h"
#include "novation-codec.h"
#include "control-program-codec.h"
/* The centre-button focus-press and Replace Duplicate Sequence retain their
 * shipped blocking-route gate. Save uses the existing GLOBAL_SAVE owner:
 * command-capture.c defers nested owning-thread drains while native Save pumps
 * messages, preserving the outer request. MPC owns file names and dialogs. */
#ifndef X_TOUCH_BLOCKING_ENABLED
#define X_TOUCH_BLOCKING_ENABLED 1
#endif
#include "sha256.h"
#include "mpc-identity.h"
static volatile sig_atomic_t stopping;
static McuEndpoint controller={.profile=&mcu_profile_xtouch,.client="X-Touch",.port="X-TOUCH_INT",.physical_only=1};
static McuEndpoint controller_output={.profile=&mcu_profile_xtouch,.client="X-Touch",.port="X-TOUCH_INT",.physical_only=1};
static const char *endpoint_type_name(unsigned type){return type==MCU_ENDPOINT_KERNEL?"kernel":type==MCU_ENDPOINT_USER?"user":"any";}
#ifdef MIRROR_INPUT
#include <sys/file.h>
#include "../stop-route.h"
#define INPUT_STOPPED() (stopping!=0)
static unsigned bridge_verbose;
static unsigned bridge_log_count;
static void bridge_log(const char *format,...){
 if(!bridge_verbose&&bridge_log_count++>=128)return;
 va_list args;va_start(args,format);vprintf(format,args);va_end(args);
}
#define BRIDGE_LOG(...) bridge_log(__VA_ARGS__)
#define INPUT_LOG(...) do{if(bridge_verbose)BRIDGE_LOG(__VA_ARGS__);}while(0)
#include "mirror-input-core.h"
static InputCoordinator command_coordinator;
#define coordinator_flights command_coordinator.flights
static MirrorInput input={.flights=coordinator_flights};
#include "motor-follow.h"
#include "surface-preferences.h"
#include "mcu-controller-preferences.h"
#include "native-preferences.h"
#include "controller-detect.h"
static MotorFollowing following={.enabled=1};
static const char *preferences_path,*stop_adapter_exe;
static uint32_t preference_notice_until;
static unsigned preference_notice_error;
static uint32_t page_notice_until;static unsigned requested_page;
static uint32_t effect_notice_until,effect_notice_epoch,effect_notice_generation,effect_notice_serial,effect_notice_slot=EFFECT_LIST,effect_notice_page;
static uint32_t mode_notice_until,mode_notice_controller,mode_notice_generation,mode_notice_epoch;static unsigned mode_notice_id;
static unsigned inherited_holds;
static unsigned servo_disabled=1; /* Profile policy; X-Touch default remains off. */
static int servo_override_requested=-1;
static unsigned controller_hui_policy=MCU_HUI_NONE;
/* Only codec/gesture/projection state is local. Every context points at the
 * same coordinator flights and command mapping once admitted. */
typedef struct {
 McuEndpoint controller,controller_output;
 MirrorInput input;
 MotorFollowing following;
 unsigned servo_disabled,controller_hui_policy,inherited_holds;
 uint32_t preference_notice_until,preference_notice_error,page_notice_until,requested_page;
 uint32_t effect_notice_until,effect_notice_epoch,effect_notice_generation,effect_notice_serial,effect_notice_slot,effect_notice_page;
 uint32_t mode_notice_until,mode_notice_controller,mode_notice_generation,mode_notice_epoch,mode_notice_id;
 uint64_t received,encoded,sent;
 uint32_t connection_generation;
} SurfaceContext;
/* Null is used only by the old single-surface diagnostic composition. */
#define SURFACE_VALUE(surface,field) (*( (surface)->context ? &(surface)->context->field : &field ))
typedef struct {
 const char *profile,*input_client,*input_port,*output_client,*output_port,*preferences;
 unsigned input_type,output_type,hui_policy,selection_options,profile_set,legacy_endpoint_options,split_endpoint_options;
} ControllerOptions;
static McuControllerPreference saved_controller;
static McuControllerCollection runtime_collection;
static int controller_type_option(const char *arg,const char *prefix,unsigned *type){
 size_t length=strlen(prefix);if(strncmp(arg,prefix,length))return 0;
 const char *value=arg+length;
 if(!strcmp(value,"kernel"))*type=MCU_ENDPOINT_KERNEL;
 else if(!strcmp(value,"user"))*type=MCU_ENDPOINT_USER;
 else if(!strcmp(value,"any"))*type=MCU_ENDPOINT_ANY;
 else return -1;
 return 1;
}
static int controller_option(ControllerOptions *options,const char *arg){
 if(!strncmp(arg,"--profile=",10)){options->profile=arg+10;options->selection_options++;options->profile_set=1;return 1;}
 if(!strncmp(arg,"--endpoint-client=",18)){options->input_client=options->output_client=arg+18;options->selection_options++;options->legacy_endpoint_options=1;return 1;}
 if(!strncmp(arg,"--endpoint-port=",16)){options->input_port=options->output_port=arg+16;options->selection_options++;options->legacy_endpoint_options=1;return 1;}
 int type=controller_type_option(arg,"--endpoint-type=",&options->input_type);
 if(type){if(type<0)return 0;options->output_type=options->input_type;options->selection_options++;options->legacy_endpoint_options=1;return 1;}
 if(!strncmp(arg,"--input-client=",15)){options->input_client=arg+15;options->selection_options++;options->split_endpoint_options=1;return 1;}
 if(!strncmp(arg,"--input-port=",13)){options->input_port=arg+13;options->selection_options++;options->split_endpoint_options=1;return 1;}
 type=controller_type_option(arg,"--input-type=",&options->input_type);
 if(type){if(type<0)return 0;options->selection_options++;options->split_endpoint_options=1;return 1;}
 if(!strncmp(arg,"--output-client=",16)){options->output_client=arg+16;options->selection_options++;options->split_endpoint_options=1;return 1;}
 if(!strncmp(arg,"--output-port=",14)){options->output_port=arg+14;options->selection_options++;options->split_endpoint_options=1;return 1;}
 type=controller_type_option(arg,"--output-type=",&options->output_type);
 if(type){if(type<0)return 0;options->selection_options++;options->split_endpoint_options=1;return 1;}
 if(!strcmp(arg,"--hui-mode=pickup")){options->hui_policy=MCU_HUI_PICKUP;options->selection_options++;return 1;}
 if(!strcmp(arg,"--hui-mode=motorized")){options->hui_policy=MCU_HUI_MOTORIZED;options->selection_options++;return 1;}
 if(!strncmp(arg,"--controller-preferences=",25)&&arg[25]=='/'){options->preferences=arg+25;return 1;}
 return 0;
}
static int controller_resolve(const ControllerOptions *options,McuEndpoint *input,McuEndpoint *output,unsigned *hui_policy){
 if(options->preferences){
  if(options->selection_options||!mcu_controller_preferences_read(options->preferences,&saved_controller))return 0;
  if(!saved_controller.enabled)return 0;
  *input=saved_controller.input;*output=saved_controller.output;*hui_policy=saved_controller.hui_policy;return 1;
 }
 const McuProfile *profile=mcu_profile_named(options->profile);if(!profile)return 0;
 if(options->legacy_endpoint_options&&options->split_endpoint_options)return 0;
 *input=(McuEndpoint){.profile=profile,.client=options->input_client?options->input_client:profile->default_client,.port=options->input_port?options->input_port:profile->default_port,.physical_only=options->input_type};
 *output=(McuEndpoint){.profile=profile,.client=options->output_client?options->output_client:profile->default_client,.port=options->output_port?options->output_port:profile->default_port,.physical_only=options->output_type};
 if(profile->default_client&&(options->input_client||options->input_port||options->output_client||options->output_port||options->input_type!=MCU_ENDPOINT_KERNEL||options->output_type!=MCU_ENDPOINT_KERNEL))return 0;
 if(profile==&mcu_profile_hui&&((options->input_type!=MCU_ENDPOINT_KERNEL&&options->input_type!=MCU_ENDPOINT_USER)||(options->output_type!=MCU_ENDPOINT_KERNEL&&options->output_type!=MCU_ENDPOINT_USER)))return 0;
 *hui_policy=profile==&mcu_profile_hui?(options->hui_policy?options->hui_policy:MCU_HUI_PICKUP):MCU_HUI_NONE;
 if(profile!=&mcu_profile_hui&&options->hui_policy)return 0;
 return mcu_endpoint_valid(input)&&mcu_endpoint_valid(output);
}
static int servo_arguments(int argc,char **argv){
 int servo_override=-1;ControllerOptions options={.profile="xtouch",.input_type=MCU_ENDPOINT_KERNEL,.output_type=MCU_ENDPOINT_KERNEL};
 servo_disabled=1;bridge_verbose=0;
 while(argc>=4){
  const char *arg=argv[argc-1];
  if(!strncmp(arg,"--stop-adapter-exe=",19)&&arg[19]=='/')stop_adapter_exe=arg+19;
  else if(!strcmp(arg,"--servo=off"))servo_override=1;
  else if(!strcmp(arg,"--servo=on"))servo_override=0;
  else if(controller_option(&options,arg)){}
  else if(!strcmp(arg,"--verbose"))bridge_verbose=1;
  else if(!strcmp(arg,"--motors=on"))following.enabled=1;
  else if(!strcmp(arg,"--motors=off"))following.enabled=0;
  else if(!strncmp(arg,"--preferences=",14)&&arg[14]=='/')preferences_path=arg+14;
  else if(!strncmp(arg,"--held-mask=",12)){char *end;unsigned long mask=strtoul(arg+12,&end,10);if(!arg[12]||*end||mask>511)return -1;inherited_holds=(unsigned)mask;}
  else break;
 argc--;
 }
 if(options.preferences){
  if(options.selection_options||!mcu_controller_preferences_read_collection(options.preferences,&runtime_collection))return -1;
  unsigned first=0;while(first<runtime_collection.count&&!runtime_collection.records[first].enabled)first++;
  if(first<runtime_collection.count){controller=runtime_collection.records[first].input;controller_output=runtime_collection.records[first].output;controller_hui_policy=runtime_collection.records[first].hui_policy;}
 }else{
  if(!controller_resolve(&options,&controller,&controller_output,&controller_hui_policy))return -1;
  memset(&runtime_collection,0,sizeof(runtime_collection));runtime_collection.count=1;
  runtime_collection.records[0]=(McuControllerPreference){.enabled=1,.input=controller,.output=controller_output,.hui_policy=controller_hui_policy};
 }
 servo_disabled=!mcu_has(&controller,MCU_CAP_STRIP_MOTOR)||(servo_override>=0?(unsigned)servo_override:!controller.profile->raw_fader_echo);
 servo_override_requested=servo_override;
 return argc;
}
static int input_fd=-1;
static CommandState *input_mapping=MAP_FAILED;
static struct stat input_file;
static inline int fresh_copy(const MirrorState*,CopiedMirror*,uint32_t*);
static int fresh_copy_view(const MirrorState*,CopiedMirror*,uint32_t*,int);
#include "controller-runtime.h"
#endif
#ifndef MIRROR_INPUT
typedef struct ControllerRuntime ControllerRuntime;
#define SURFACE_VALUE(surface,field) (field)
#endif
#ifndef BRIDGE_LOG
#define BRIDGE_LOG(...) printf(__VA_ARGS__)
#endif
#ifndef INPUT_LOG
#define INPUT_LOG(...) BRIDGE_LOG(__VA_ARGS__)
#endif
static const char expected_sha[]=MPC_EXECUTABLE_SHA256;
static void signal_stop(int sig){(void)sig;stopping=1;}
static uint64_t monotonic_ms(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return UINT64_MAX;return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;}
/* Same signed producer-relative arithmetic, including fractional second edges. */
static uint32_t relative_time(int64_t sec,int64_t nsec,uint32_t began_sec,uint32_t began_nsec){
 if(sec<(int64_t)began_sec||(sec==began_sec&&nsec<began_nsec))return UINT32_MAX;
 int64_t n=(sec-began_sec)*1000+(nsec-began_nsec)/1000000;return n<0||n>=UINT32_MAX?UINT32_MAX:(uint32_t)n;
}
static uint32_t source_now(const MirrorState *s){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return UINT32_MAX;return relative_time(t.tv_sec,t.tv_nsec,s->origin_sec,s->origin_nsec);}
static int number(const char *text,uint64_t max,uint64_t *value){char *end;errno=0;if(!*text||*text=='-'||*text=='+')return 0;unsigned long long n=strtoull(text,&end,10);if(errno||*end||!n||n>max)return 0;*value=n;return 1;}
static uint64_t start_time(unsigned pid){
 char path[64],data[4096];snprintf(path,sizeof(path),"/proc/%u/stat",pid);int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return 0;
 ssize_t n=read(fd,data,sizeof(data)-1);close(fd);if(n<=0)return 0;data[n]=0;char *end=strrchr(data,')');if(!end||end[1]!=' ')return 0;
 char *save=NULL,*part=strtok_r(end+2," ",&save);for(unsigned field=3;part;field++,part=strtok_r(NULL," ",&save))if(field==22){uint64_t value;return number(part,UINT64_MAX,&value)?value:0;}return 0;
}
static int same_stat(const struct stat *a,const struct stat *b){return a->st_dev==b->st_dev&&a->st_ino==b->st_ino;}
static int process_file(unsigned pid,struct stat *identity,int hash){
 char proc[64],path[128];snprintf(proc,sizeof(proc),"/proc/%u/exe",pid);ssize_t n=readlink(proc,path,sizeof(path)-1);if(n<0)return 0;path[n]=0;if(strcmp(path,"/usr/bin/MPC"))return 0;
 int fd=open(proc,O_RDONLY|O_CLOEXEC);if(fd<0)return 0;struct stat st;int ok=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size==MPC_EXECUTABLE_SIZE;
 if(hash&&ok){Sha sha;sha_init(&sha);unsigned char bytes[65536],sum[32];size_t total=0;
  while(ok){ssize_t got=read(fd,bytes,sizeof(bytes));if(got<0){ok=0;break;}if(!got)break;total+=(size_t)got;if(total>MPC_EXECUTABLE_SIZE){ok=0;break;}sha_add(&sha,bytes,(size_t)got);}
  sha_end(&sha,sum);char hex[65];for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",sum[i]);ok=ok&&total==MPC_EXECUTABLE_SIZE&&!strcmp(hex,expected_sha);
  struct stat after;if(fstat(fd,&after)||!same_stat(&after,&st)||after.st_size!=st.st_size)ok=0;if(ok)*identity=st;
 }else if(ok)ok=same_stat(identity,&st);
 close(fd);return ok;
}
static int address_equal(snd_seq_addr_t a,snd_seq_addr_t b){return a.client==b.client&&a.port==b.port;}
static int endpoint_client_matches(const McuEndpoint *endpoint,const char *client,unsigned kernel){
 return endpoint&&!strcmp(endpoint->client,client)&&(endpoint->physical_only==MCU_ENDPOINT_ANY||
  (endpoint->physical_only==MCU_ENDPOINT_KERNEL&&kernel)||(endpoint->physical_only==MCU_ENDPOINT_USER&&!kernel));
}
static int discover_endpoints(snd_seq_t *seq,const McuEndpoint *input,unsigned input_required,snd_seq_addr_t *input_address,const McuEndpoint *output,unsigned output_required,snd_seq_addr_t *output_address){
 snd_seq_client_info_t *ci;snd_seq_port_info_t *pi;snd_seq_client_info_alloca(&ci);snd_seq_port_info_alloca(&pi);snd_seq_client_info_set_client(ci,-1);int rc;
 int input_count=0,output_count=0;
 while((rc=snd_seq_query_next_client(seq,ci))>=0){
  const char *client=snd_seq_client_info_get_name(ci);unsigned kernel=snd_seq_client_info_get_type(ci)==SND_SEQ_KERNEL_CLIENT;
  if(!endpoint_client_matches(input,client,kernel)&&!endpoint_client_matches(output,client,kernel))continue;
  snd_seq_port_info_set_client(pi,snd_seq_client_info_get_client(ci));snd_seq_port_info_set_port(pi,-1);int pr;
  while((pr=snd_seq_query_next_port(seq,pi))>=0){
   unsigned caps=snd_seq_port_info_get_capability(pi),abstract=0;
   if(caps&SND_SEQ_PORT_CAP_READ)abstract|=MCU_ENDPOINT_READ;
   if(caps&SND_SEQ_PORT_CAP_SUBS_READ)abstract|=MCU_ENDPOINT_SUBS_READ;
   if(caps&SND_SEQ_PORT_CAP_WRITE)abstract|=MCU_ENDPOINT_WRITE;
   const char *port=snd_seq_port_info_get_name(pi);
   if(input&&mcu_endpoint_match_caps(input,client,kernel,port,abstract,input_required)){*input_address=*snd_seq_port_info_get_addr(pi);input_count++;}
   if(output&&mcu_endpoint_match_caps(output,client,kernel,port,abstract,output_required)){*output_address=*snd_seq_port_info_get_addr(pi);output_count++;}
  }if(pr!=-ENOENT)return 0;
 }
 return rc==-ENOENT&&(!input||input_count==1)&&(!output||output_count==1);
}

#ifdef MIRROR_INPUT
static void list_text(const char *text){
 putchar('"');for(const unsigned char *p=(const unsigned char*)text;*p;p++){if(*p=='"'||*p=='\\')putchar('\\');if(*p>=32&&*p<127)putchar(*p);else printf("\\x%02x",*p);}putchar('"');
}
static int list_midi_endpoints(void){
 snd_seq_t *seq=NULL;if(snd_seq_open(&seq,"default",SND_SEQ_OPEN_DUPLEX,SND_SEQ_NONBLOCK)<0){fputs("MIDI endpoint listing unavailable\n",stderr);return 1;}
 snd_seq_client_info_t *ci;snd_seq_port_info_t *pi;snd_seq_client_info_alloca(&ci);snd_seq_port_info_alloca(&pi);snd_seq_client_info_set_client(ci,-1);int rc;
 while((rc=snd_seq_query_next_client(seq,ci))>=0){
  int client=snd_seq_client_info_get_client(ci);snd_seq_port_info_set_client(pi,client);snd_seq_port_info_set_port(pi,-1);int pr;
  while((pr=snd_seq_query_next_port(seq,pi))>=0){
   unsigned caps=snd_seq_port_info_get_capability(pi);
   if(!(caps&(SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_WRITE)))continue;
   printf("address=%d:%d type=%s capabilities=%s%s%s%s client=",client,snd_seq_port_info_get_port(pi),snd_seq_client_info_get_type(ci)==SND_SEQ_KERNEL_CLIENT?"kernel":"user",caps&SND_SEQ_PORT_CAP_READ?"read,":"",caps&SND_SEQ_PORT_CAP_SUBS_READ?"subs-read,":"",caps&SND_SEQ_PORT_CAP_WRITE?"write,":"",caps&SND_SEQ_PORT_CAP_SUBS_WRITE?"subs-write":"");
   list_text(snd_seq_client_info_get_name(ci));fputs(" port=",stdout);list_text(snd_seq_port_info_get_name(pi));putchar('\n');
  }
  if(pr!=-ENOENT){snd_seq_close(seq);return 1;}
 }
 snd_seq_close(seq);return rc==-ENOENT?0:1;
}

enum {NATIVE_APPLY_NONE=10,NATIVE_APPLY_INVALID=11,NATIVE_APPLY_UNAVAILABLE=12,NATIVE_APPLY_AMBIGUOUS=13,NATIVE_APPLY_WRITE=14,NATIVE_APPLY_STALE=15};

static int native_state_open(const char *path,NativePreferencesState **mapped){
 if(!path||path[0]!='/')return -1;
 int fd=open(path,O_RDWR|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return -1;
 struct stat st={0};int ok=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_uid==geteuid()&&(st.st_mode&0777)==0600&&st.st_size==NATIVE_PREFERENCES_BYTES;
 void *page=ok?mmap(NULL,NATIVE_PREFERENCES_BYTES,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0):MAP_FAILED;close(fd);
 if(page==MAP_FAILED)return -1;
 NativePreferencesState *s=page;
 if(s->magic!=NATIVE_PREFERENCES_MAGIC||s->version!=NATIVE_PREFERENCES_VERSION||s->bytes!=sizeof(*s)){munmap(page,NATIVE_PREFERENCES_BYTES);return -1;}
 *mapped=s;return 0;
}
static void native_state_endpoint(NativePreferencesState *s,const McuControllerPreference *selection,int active){
 const McuEndpoint *endpoint=selection?&selection->endpoint:NULL;
 size_t client=endpoint&&endpoint->client?strlen(endpoint->client):0,port=endpoint&&endpoint->port?strlen(endpoint->port):0;
 if(client>NATIVE_PREFERENCES_ENDPOINT_MAX)client=NATIVE_PREFERENCES_ENDPOINT_MAX;
 if(port>NATIVE_PREFERENCES_ENDPOINT_MAX)port=NATIVE_PREFERENCES_ENDPOINT_MAX;
 __atomic_add_fetch(&s->result_revision,1u,__ATOMIC_RELEASE);
 memset(s->endpoint_client,0,sizeof(s->endpoint_client));memset(s->endpoint_port,0,sizeof(s->endpoint_port));
 if(client)memcpy(s->endpoint_client,endpoint->client,client);
 if(port)memcpy(s->endpoint_port,endpoint->port,port);
 s->endpoint_client_length=(uint32_t)client;s->endpoint_port_length=(uint32_t)port;
 s->saved_endpoint_type=endpoint?endpoint->physical_only:0;s->saved_hui_policy=selection?selection->hui_policy:0;
 if(active){s->active_endpoint_type=s->saved_endpoint_type;s->active_hui_policy=s->saved_hui_policy;}
 __atomic_add_fetch(&s->result_revision,1u,__ATOMIC_RELEASE);
}
static void native_state_controller(NativePreferencesController *out,const McuControllerPreference *selection,unsigned status){
 memset(out,0,sizeof(*out));if(!selection)return;
 out->enabled=selection->enabled;out->profile=mcu_controller_profile_id(selection->input.profile);out->hui_policy=selection->hui_policy;out->status=status;
 out->input_type=selection->input.physical_only;out->output_type=selection->output.physical_only;
 size_t ic=strlen(selection->input.client),ip=strlen(selection->input.port),oc=strlen(selection->output.client),op=strlen(selection->output.port);
 if(ic>NATIVE_PREFERENCES_ENDPOINT_NAME_MAX)ic=NATIVE_PREFERENCES_ENDPOINT_NAME_MAX;
 if(ip>NATIVE_PREFERENCES_ENDPOINT_NAME_MAX)ip=NATIVE_PREFERENCES_ENDPOINT_NAME_MAX;
 if(oc>NATIVE_PREFERENCES_ENDPOINT_NAME_MAX)oc=NATIVE_PREFERENCES_ENDPOINT_NAME_MAX;
 if(op>NATIVE_PREFERENCES_ENDPOINT_NAME_MAX)op=NATIVE_PREFERENCES_ENDPOINT_NAME_MAX;
 out->input_client_length=(uint32_t)ic;out->input_port_length=(uint32_t)ip;out->output_client_length=(uint32_t)oc;out->output_port_length=(uint32_t)op;
 memcpy(out->input_client,selection->input.client,ic);memcpy(out->input_port,selection->input.port,ip);
 memcpy(out->output_client,selection->output.client,oc);memcpy(out->output_port,selection->output.port,op);
}
static void native_state_status(NativePreferencesState *s,unsigned status,unsigned detail,unsigned complete){
 __atomic_store_n(&s->result_detail,detail,__ATOMIC_RELAXED);__atomic_store_n(&s->status,status,__ATOMIC_RELEASE);
 if(complete)__atomic_store_n(&s->completed_sequence,__atomic_load_n(&s->claimed_sequence,__ATOMIC_RELAXED),__ATOMIC_RELEASE);
}
/* Exclude internal/session ports, then retain each external port's independent
 * input/output roles. Generic profiles always use explicitly selected names. */
static unsigned external_capabilities(const char *client,int kernel,const char *port,unsigned caps){
 (void)kernel;
 if(!client||!port||!*client||!*port||!strcmp(client,"System")||!strcmp(client,"MPC")||!strcmp(client,"MPC 3")||!strcmp(client,"MPC Live II")||!strcmp(client,"JUCE")||
    !strncmp(client,"mpclearn-",9)||!strncmp(port,"mpclearn-",9))return 0;
 unsigned roles=0,from=SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ;
 if((caps&from)==from)roles|=NATIVE_ENDPOINT_FROM_CONTROLLER;
 if(caps&SND_SEQ_PORT_CAP_WRITE)roles|=NATIVE_ENDPOINT_TO_CONTROLLER;
 return roles;
}
typedef struct {NativePreferencesEndpoint rows[NATIVE_PREFERENCES_ENDPOINTS];unsigned count,overflow;} NativeEndpointInventory;
static int enumerate_external(snd_seq_t *seq,NativeEndpointInventory *inventory){
 snd_seq_client_info_t *ci;snd_seq_port_info_t *pi;snd_seq_client_info_alloca(&ci);snd_seq_port_info_alloca(&pi);snd_seq_client_info_set_client(ci,-1);
 int rc;memset(inventory,0,sizeof(*inventory));
 while((rc=snd_seq_query_next_client(seq,ci))>=0){
  const char *client=snd_seq_client_info_get_name(ci);int kernel=snd_seq_client_info_get_type(ci)==SND_SEQ_KERNEL_CLIENT;
  snd_seq_port_info_set_client(pi,snd_seq_client_info_get_client(ci));snd_seq_port_info_set_port(pi,-1);int pr;
  while((pr=snd_seq_query_next_port(seq,pi))>=0){
   const char *port=snd_seq_port_info_get_name(pi);unsigned caps=snd_seq_port_info_get_capability(pi);
   unsigned roles=external_capabilities(client,kernel,port,caps);if(!roles)continue;
   size_t cn=strlen(client),pn=strlen(port);if(cn>=NATIVE_PREFERENCES_ENDPOINT_NAME_MAX||pn>=NATIVE_PREFERENCES_ENDPOINT_NAME_MAX)return -1;
   if(inventory->count>=NATIVE_PREFERENCES_ENDPOINTS){inventory->overflow=1;continue;}
   NativePreferencesEndpoint *row=inventory->rows+inventory->count++;
   row->client_number=(uint32_t)snd_seq_client_info_get_client(ci);row->port_number=(uint32_t)snd_seq_port_info_get_port(pi);
   row->endpoint_type=kernel?NATIVE_ENDPOINT_KERNEL:NATIVE_ENDPOINT_USER;row->capabilities=roles;
   memcpy(row->client,client,cn+1);memcpy(row->port,port,pn+1);
  }
  if(pr!=-ENOENT)return -1;
 }
 return rc==-ENOENT?0:-1;
}
static void native_inventory_publish(NativePreferencesState *s,const NativeEndpointInventory *inventory,unsigned status){
 __atomic_add_fetch(&s->inventory_revision,1u,__ATOMIC_RELEASE);
 memset(s->endpoints,0,sizeof(s->endpoints));
 if(inventory&&inventory->count)memcpy(s->endpoints,inventory->rows,inventory->count*sizeof(inventory->rows[0]));
 s->inventory_count=inventory?inventory->count:0;s->inventory_status=status;s->inventory_generation++;
 __atomic_add_fetch(&s->inventory_revision,1u,__ATOMIC_RELEASE);
}
static int native_inventory_refresh(NativePreferencesState *s,NativeEndpointInventory *inventory){
 /* The caller inspects overflow even when ALSA cannot be opened. */
 memset(inventory,0,sizeof(*inventory));
 snd_seq_t *seq=NULL;int result=snd_seq_open(&seq,"default",SND_SEQ_OPEN_DUPLEX,SND_SEQ_NONBLOCK)<0?-1:enumerate_external(seq,inventory);
 if(seq)snd_seq_close(seq);
 unsigned status=result<0?NATIVE_PREFERENCES_UNAVAILABLE:inventory->overflow?NATIVE_PREFERENCES_INVALID:NATIVE_PREFERENCES_ACTIVE;
 native_inventory_publish(s,result<0?NULL:inventory,status);return result<0||inventory->overflow?0:1;
}
static int inventory_exact_role(const NativeEndpointInventory *inventory,const NativePreferencesEndpoint *expected,unsigned role){
 int matches=0;
 for(unsigned i=0;i<inventory->count;i++){
  const NativePreferencesEndpoint *row=inventory->rows+i;
  if(!(row->capabilities&role)||row->endpoint_type!=expected->endpoint_type||strcmp((const char*)row->client,(const char*)expected->client)||strcmp((const char*)row->port,(const char*)expected->port))continue;
  matches++;
 }
 return matches;
}
static int inventory_has_endpoint(const NativeEndpointInventory *inventory,const McuEndpoint *endpoint,unsigned role){
 unsigned matches=0;
 for(unsigned i=0;i<inventory->count;i++)if((inventory->rows[i].capabilities&role)&&
   (!endpoint->physical_only||inventory->rows[i].endpoint_type==endpoint->physical_only)&&!strcmp((const char*)inventory->rows[i].client,endpoint->client)&&!strcmp((const char*)inventory->rows[i].port,endpoint->port))matches++;
 return matches==1;
}
static void native_state_collection(NativePreferencesState *s,const McuControllerCollection *collection,const NativeEndpointInventory *inventory,unsigned active_index){
 __atomic_add_fetch(&s->collection_revision,1u,__ATOMIC_RELEASE);
 unsigned count=collection?collection->count:0;if(count>MCU_CONTROLLER_CAPACITY)count=MCU_CONTROLLER_CAPACITY;
 unsigned changed=count!=s->collection_count;
 for(unsigned i=0;i<count;i++){
  const McuControllerPreference *record=collection->records+i;unsigned status=NATIVE_CONTROLLER_DISABLED;
  if(record->enabled){int input=inventory&&inventory_has_endpoint(inventory,&record->input,NATIVE_ENDPOINT_FROM_CONTROLLER),output=inventory&&inventory_has_endpoint(inventory,&record->output,NATIVE_ENDPOINT_TO_CONTROLLER);
   status=input&&output?NATIVE_CONTROLLER_READY:input?NATIVE_CONTROLLER_OUTPUT_MISSING:output?NATIVE_CONTROLLER_INPUT_MISSING:NATIVE_CONTROLLER_DISCONNECTED;
   /* Runtime admission, not endpoint presence, owns ACTIVE. */
  }
  NativePreferencesController next;native_state_controller(&next,record,s->controllers[i].status);
  if(memcmp(&next,s->controllers+i,sizeof(next)))changed=1;
  next.status=status;s->controllers[i]=next;
 }
 memset(s->controllers+count,0,(MCU_CONTROLLER_CAPACITY-count)*sizeof(s->controllers[0]));
 s->collection_count=count;s->active_controller_index=active_index<count?active_index:NATIVE_CONTROLLER_NEW;if(changed)s->collection_generation++;
 __atomic_add_fetch(&s->collection_revision,1u,__ATOMIC_RELEASE);
}
static unsigned controller_first_enabled(const McuControllerCollection *collection){
 if(collection)for(unsigned i=0;i<collection->count;i++)if(collection->records[i].enabled)return i;
 return NATIVE_CONTROLLER_NEW;
}
static void native_state_compatibility(NativePreferencesState *s,const McuControllerCollection *collection,unsigned active_index){
 const McuControllerPreference *saved=collection&&collection->count?collection->records:NULL,*active=collection&&active_index<collection->count?collection->records+active_index:NULL;
 s->saved_profile=saved?mcu_controller_profile_id(saved->input.profile):0;s->active_profile=active?mcu_controller_profile_id(active->input.profile):0;
 native_state_endpoint(s,saved,active&&saved==active);
 s->active_endpoint_type=active?active->input.physical_only:0;s->active_hui_policy=active?active->hui_policy:0;
}
static int native_preferences_watch(const char *path){
 NativePreferencesState *s;if(native_state_open(path,&s))return 1;
 pid_t owner=getppid();uint64_t owner_start=start_time((unsigned)owner);uint32_t seen=__atomic_load_n(&s->completed_sequence,__ATOMIC_ACQUIRE);
 if(owner<=1||!owner_start){munmap(s,sizeof(*s));return 1;}
 for(;;){
  uint32_t requested=__atomic_load_n(&s->request_sequence,__ATOMIC_ACQUIRE);
  if(requested!=seen){if(getppid()!=owner||start_time((unsigned)owner)!=owner_start||kill(owner,SIGUSR2)){munmap(s,sizeof(*s));return 1;}seen=requested;}
  long result=syscall(SYS_futex,&s->request_sequence,0,requested,NULL,NULL,0);
  if(result<0&&errno!=EAGAIN&&errno!=EINTR){munmap(s,sizeof(*s));return 1;}
 }
}
static int native_preferences_sync(const char *state_path,const char *preference_path,const char *active_name){
 NativePreferencesState *s;McuControllerCollection collection;if(native_state_open(state_path,&s))return 1;
 if(!mcu_controller_preferences_read_collection(preference_path,&collection)){munmap(s,sizeof(*s));return 1;}
 unsigned active=controller_first_enabled(&collection);const McuProfile *override=NULL;
 if(strcmp(active_name,"saved")){override=mcu_profile_named(active_name);if(!override){munmap(s,sizeof(*s));return 1;}active=NATIVE_CONTROLLER_NEW;}
 NativeEndpointInventory inventory;int listed=native_inventory_refresh(s,&inventory);
 native_state_collection(s,&collection,listed?&inventory:NULL,active);native_state_compatibility(s,&collection,active);
 if(override)s->active_profile=mcu_controller_profile_id(override);
 native_state_status(s,NATIVE_PREFERENCES_ACTIVE,0,0);munmap(s,sizeof(*s));return 0;
}
static int native_request_inventory(NativePreferencesState *s,NativePreferencesEndpoint *input,NativePreferencesEndpoint *output,
 uint32_t input_generation,uint32_t input_index,uint32_t input_type,uint32_t output_generation,uint32_t output_index,uint32_t output_type){
 uint32_t revision=__atomic_load_n(&s->inventory_revision,__ATOMIC_ACQUIRE);if(revision&1)return NATIVE_APPLY_STALE;
 if((input_index!=NATIVE_ENDPOINT_UNCHANGED&&input_generation!=s->inventory_generation)||
    (output_index!=NATIVE_ENDPOINT_UNCHANGED&&output_generation!=s->inventory_generation))return NATIVE_APPLY_STALE;
 if(input_index!=NATIVE_ENDPOINT_UNCHANGED){
  if(input_index>=s->inventory_count||input_index>=NATIVE_PREFERENCES_ENDPOINTS)return NATIVE_APPLY_INVALID;
  *input=s->endpoints[input_index];
  if(input->endpoint_type!=input_type||!(input->capabilities&NATIVE_ENDPOINT_FROM_CONTROLLER))return NATIVE_APPLY_INVALID;
 }
 if(output_index!=NATIVE_ENDPOINT_UNCHANGED){
  if(output_index>=s->inventory_count||output_index>=NATIVE_PREFERENCES_ENDPOINTS)return NATIVE_APPLY_INVALID;
  *output=s->endpoints[output_index];
  if(output->endpoint_type!=output_type||!(output->capabilities&NATIVE_ENDPOINT_TO_CONTROLLER))return NATIVE_APPLY_INVALID;
 }
 uint32_t after=__atomic_load_n(&s->inventory_revision,__ATOMIC_ACQUIRE);return after==revision&&!(after&1)?0:NATIVE_APPLY_STALE;
}
static int native_endpoint_from_request(McuEndpoint *endpoint,char *client,char *port,const McuProfile *profile,
 const NativePreferencesEndpoint *row,const McuEndpoint *unchanged,unsigned role,const NativeEndpointInventory *fresh){
 if(row){int found=inventory_exact_role(fresh,row,role);if(found!=1)return found?NATIVE_APPLY_AMBIGUOUS:NATIVE_APPLY_UNAVAILABLE;
  McuEndpoint selected={.profile=profile,.client=(const char*)row->client,.port=(const char*)row->port,.physical_only=row->endpoint_type};
  return mcu_controller_endpoint_copy(endpoint,client,port,&selected)?0:NATIVE_APPLY_INVALID;
 }
 if(unchanged){McuEndpoint selected={.profile=profile,.client=unchanged->client,.port=unchanged->port,.physical_only=unchanged->physical_only};
  return mcu_controller_endpoint_copy(endpoint,client,port,&selected)?0:NATIVE_APPLY_INVALID;
 }
 if(profile->default_client){McuEndpoint selected={.profile=profile,.client=profile->default_client,.port=profile->default_port,.physical_only=MCU_ENDPOINT_KERNEL};
  return mcu_controller_endpoint_copy(endpoint,client,port,&selected)?0:NATIVE_APPLY_INVALID;
 }
 return NATIVE_APPLY_INVALID;
}
static int native_preferences_apply(const char *state_path,const char *preference_path){
 NativePreferencesState *s;if(native_state_open(state_path,&s))return NATIVE_APPLY_INVALID;
 uint32_t requested=__atomic_load_n(&s->request_sequence,__ATOMIC_ACQUIRE),claimed=__atomic_load_n(&s->claimed_sequence,__ATOMIC_RELAXED);
 if(!requested||requested==claimed){munmap(s,sizeof(*s));return NATIVE_APPLY_NONE;}
 uint32_t kind=__atomic_load_n(&s->request_kind,__ATOMIC_RELAXED),profile_id=__atomic_load_n(&s->request_profile,__ATOMIC_RELAXED);
 uint32_t controller_index=__atomic_load_n(&s->request_controller_index,__ATOMIC_RELAXED),enabled=__atomic_load_n(&s->request_enabled,__ATOMIC_RELAXED);
 uint32_t input_generation=__atomic_load_n(&s->request_input_inventory_generation,__ATOMIC_RELAXED),input_index=__atomic_load_n(&s->request_input_endpoint_index,__ATOMIC_RELAXED),input_type=__atomic_load_n(&s->request_input_endpoint_type,__ATOMIC_RELAXED);
 uint32_t output_generation=__atomic_load_n(&s->request_output_inventory_generation,__ATOMIC_RELAXED),output_index=__atomic_load_n(&s->request_output_endpoint_index,__ATOMIC_RELAXED),output_type=__atomic_load_n(&s->request_output_endpoint_type,__ATOMIC_RELAXED);
 uint32_t hui_policy=__atomic_load_n(&s->request_hui_policy,__ATOMIC_RELAXED);
 uint32_t collection_generation=__atomic_load_n(&s->request_collection_generation,__ATOMIC_RELAXED);
 __atomic_store_n(&s->claimed_sequence,requested,__ATOMIC_RELEASE);native_state_status(s,NATIVE_PREFERENCES_APPLYING,0,0);
 McuControllerCollection current;if(!mcu_controller_preferences_read_collection(preference_path,&current)){native_state_status(s,NATIVE_PREFERENCES_INVALID,0,1);munmap(s,sizeof(*s));return NATIVE_APPLY_INVALID;}
 if(kind==NATIVE_REQUEST_REFRESH){NativeEndpointInventory refreshed;unsigned active=s->active_controller_index;int ok=native_inventory_refresh(s,&refreshed);native_state_collection(s,&current,ok?&refreshed:NULL,active);native_state_status(s,ok?NATIVE_PREFERENCES_REFRESHED:refreshed.overflow?NATIVE_PREFERENCES_INVALID:NATIVE_PREFERENCES_UNAVAILABLE,0,1);munmap(s,sizeof(*s));return NATIVE_APPLY_NONE;}
 McuControllerCollection replacement;memset(&replacement,0,sizeof(replacement));replacement.count=current.count;
 int result=collection_generation==s->collection_generation?0:NATIVE_APPLY_STALE;for(unsigned i=0;i<current.count&&!result;i++)if(!mcu_controller_preference_copy(replacement.records+i,current.records+i))result=NATIVE_APPLY_INVALID;
 if(!result&&kind==NATIVE_REQUEST_REMOVE){
  if(controller_index>=replacement.count)result=NATIVE_APPLY_INVALID;
  else {for(unsigned i=controller_index;i+1<replacement.count;i++)if(!mcu_controller_preference_copy(replacement.records+i,replacement.records+i+1)){result=NATIVE_APPLY_INVALID;break;}if(!result){replacement.count--;memset(replacement.records+replacement.count,0,sizeof(replacement.records[0]));}}
 }else if(!result&&kind==NATIVE_REQUEST_TOGGLE){
  if(controller_index>=replacement.count||enabled>1)result=NATIVE_APPLY_INVALID;else replacement.records[controller_index].enabled=enabled;
 }else if(!result&&kind==NATIVE_REQUEST_APPLY){
  const McuProfile *profile=mcu_controller_profile(profile_id);unsigned adding=controller_index==NATIVE_CONTROLLER_NEW;
  if(!profile||enabled>1||(adding&&replacement.count>=MCU_CONTROLLER_CAPACITY)||(!adding&&controller_index>=replacement.count)||!mcu_controller_policy_valid(profile,hui_policy))result=NATIVE_APPLY_INVALID;
  if(!result&&!profile->default_client&&
     (adding||profile!=current.records[controller_index].input.profile)&&
     (input_index==NATIVE_ENDPOINT_UNCHANGED||output_index==NATIVE_ENDPOINT_UNCHANGED))result=NATIVE_APPLY_INVALID;
  NativePreferencesEndpoint input_row={0},output_row={0};const NativePreferencesEndpoint *input_expected=NULL,*output_expected=NULL;NativeEndpointInventory fresh={0};
  if(!result){result=native_request_inventory(s,&input_row,&output_row,input_generation,input_index,input_type,output_generation,output_index,output_type);if(input_index!=NATIVE_ENDPOINT_UNCHANGED)input_expected=&input_row;if(output_index!=NATIVE_ENDPOINT_UNCHANGED)output_expected=&output_row;}
  if(!result&&(input_expected||output_expected)&&!native_inventory_refresh(s,&fresh))result=fresh.overflow?NATIVE_APPLY_INVALID:NATIVE_APPLY_UNAVAILABLE;
  if(!result){McuControllerPreference updated;memset(&updated,0,sizeof(updated));updated.enabled=enabled;updated.hui_policy=hui_policy;
   const McuControllerPreference *prior=adding?NULL:current.records+controller_index;
   result=native_endpoint_from_request(&updated.input,updated.input_client,updated.input_port,profile,input_expected,prior?&prior->input:NULL,NATIVE_ENDPOINT_FROM_CONTROLLER,&fresh);
   if(!result)result=native_endpoint_from_request(&updated.output,updated.output_client,updated.output_port,profile,output_expected,prior?&prior->output:NULL,NATIVE_ENDPOINT_TO_CONTROLLER,&fresh);
   if(!result){updated.endpoint=updated.input;memcpy(updated.client,updated.input_client,sizeof(updated.client));memcpy(updated.port,updated.input_port,sizeof(updated.port));unsigned destination=adding?replacement.count++:controller_index;if(!mcu_controller_preference_copy(replacement.records+destination,&updated))result=NATIVE_APPLY_INVALID;}
  }
 }else if(!result&&kind==NATIVE_REQUEST_DETECT){
  /* Detect only appends rows; nothing found completes without a restart. */
  NativeEndpointInventory fresh;
  if(!native_inventory_refresh(s,&fresh))result=fresh.overflow?NATIVE_APPLY_INVALID:NATIVE_APPLY_UNAVAILABLE;
  else {ControllerDetectResult found=controller_detect(&replacement,fresh.rows,fresh.count);
   if(!found.added){native_state_collection(s,&current,&fresh,s->active_controller_index);native_state_status(s,NATIVE_PREFERENCES_NONE_DETECTED,found.full,1);munmap(s,sizeof(*s));return NATIVE_APPLY_NONE;}
  }
 }else if(!result)result=NATIVE_APPLY_INVALID;
 if(result){unsigned status=result==NATIVE_APPLY_UNAVAILABLE?NATIVE_PREFERENCES_UNAVAILABLE:result==NATIVE_APPLY_AMBIGUOUS?NATIVE_PREFERENCES_AMBIGUOUS:result==NATIVE_APPLY_STALE?NATIVE_PREFERENCES_STALE:NATIVE_PREFERENCES_INVALID;native_state_status(s,status,0,1);munmap(s,sizeof(*s));return result;}
 if(!mcu_controller_preferences_write_collection(preference_path,&replacement)){native_state_status(s,NATIVE_PREFERENCES_WRITE_FAILED,errno,1);munmap(s,sizeof(*s));return NATIVE_APPLY_WRITE;}
 NativeEndpointInventory listed;int list_ok=native_inventory_refresh(s,&listed);native_state_collection(s,&replacement,list_ok?&listed:NULL,NATIVE_CONTROLLER_NEW);native_state_compatibility(s,&replacement,NATIVE_CONTROLLER_NEW);munmap(s,sizeof(*s));return 0;
}
static int native_preferences_invalidate(const char *path,uint32_t expected){
 NativePreferencesState *s;if(native_state_open(path,&s))return 1;
 if(expected){uint32_t prior=expected;(void)__atomic_compare_exchange_n(&s->controller_lifetime,&prior,expected+1,0,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);}
 else printf("%u\n",__atomic_add_fetch(&s->controller_lifetime,1u,__ATOMIC_ACQ_REL));
 munmap(s,sizeof(*s));return 0;
}
static int native_preferences_finish(const char *state_path,int active){
 NativePreferencesState *s;if(native_state_open(state_path,&s))return 1;
 if(active){unsigned selected=NATIVE_CONTROLLER_NEW;__atomic_add_fetch(&s->collection_revision,1u,__ATOMIC_RELEASE);for(unsigned i=0;i<s->collection_count;i++)if(s->controllers[i].enabled){selected=i;break;}s->active_controller_index=selected;s->active_profile=selected<s->collection_count?s->controllers[selected].profile:0;s->active_endpoint_type=selected<s->collection_count?s->controllers[selected].input_type:0;s->active_hui_policy=selected<s->collection_count?s->controllers[selected].hui_policy:0;__atomic_add_fetch(&s->collection_revision,1u,__ATOMIC_RELEASE);native_state_status(s,NATIVE_PREFERENCES_ACTIVE,0,1);}else native_state_status(s,NATIVE_PREFERENCES_RESTART_FAILED,0,1);
 munmap(s,sizeof(*s));return 0;
}
#endif
/* Only the discovered physical surface enters policy. Optional input accepts
 * channel pitch-bend; transport and arbitrary MIDI never become commands. */
static int surface_event_profile(const snd_seq_event_t *event,snd_seq_addr_t full,const McuEndpoint *endpoint,unsigned *kind,unsigned *channel,int *value){
 if(!address_equal(event->source,full))return 0;
#ifdef MIRROR_INPUT
 if(mcu_has(endpoint,MCU_CAP_JOG)&&event->type==SND_SEQ_EVENT_CONTROLLER&&event->data.control.channel==0&&event->data.control.param==60){
  int raw=event->data.control.value;if(raw<0||raw>127)return -1;*kind=8;*channel=0;*value=(raw&64)?-(raw&63):(raw&63);return *value!=0;
 }
 if(event->type==SND_SEQ_EVENT_CONTROLLER&&event->data.control.channel==0&&event->data.control.param>=0x10&&event->data.control.param<0x18){
  int raw=event->data.control.value;if(raw<0||raw>127)return -1;*kind=4;*channel=event->data.control.param-0x10;*value=(raw&64)?-(raw&63):(raw&63);return *value!=0;
 }
 if(event->type==SND_SEQ_EVENT_PITCHBEND){
  if(event->data.control.channel>MIRROR_BANK)return 0;
  if(event->data.control.value< -8192||event->data.control.value>8191)return -1;
  unsigned position=(unsigned)(event->data.control.value+8192);*channel=event->data.control.channel;
  if(*channel==MIRROR_BANK){if(!mcu_has(endpoint,MCU_CAP_MASTER_INPUT))return 0;if(!mcu_master_input_position(endpoint,position,&position))return -1;*kind=10;}
  else{if(!mcu_has(endpoint,MCU_CAP_STRIP_FADER))return 0;*kind=3;}
  *value=(int)position;return 1;
 }
#endif
 if((event->type!=SND_SEQ_EVENT_NOTEON&&event->type!=SND_SEQ_EVENT_NOTEOFF)||event->data.note.channel!=0)return 0;
 if(event->data.note.velocity>127)return -1;
 unsigned note=event->data.note.note;int down=event->type==SND_SEQ_EVENT_NOTEON&&event->data.note.velocity>0;
 /* Footswitch 1 is Play and footswitch 2 is Record, for hands-free looping:
  * they enter as the transport Play relay and the Record toggle themselves. */
 if(note==102)note=94;else if(note==103)note=95;
 if(mcu_has(endpoint,MCU_CAP_GLOBAL)&&(note==93||note==94)){*kind=12;*channel=note;*value=down;return 1;}
 if((mcu_has(endpoint,MCU_CAP_JOG)&&note==70)||(mcu_has(endpoint,MCU_CAP_GLOBAL)&&(note==91||note==92))){*kind=9;*channel=note;*value=down;return 1;}
 if(mcu_has(endpoint,MCU_CAP_MASTER_INPUT)&&mcu_has(endpoint,MCU_CAP_TOUCH)&&note==112){*kind=7;*channel=8;*value=down;return 1;}
 if(mcu_has(endpoint,MCU_CAP_TOUCH)&&note>=104&&note<=111){*kind=1;*channel=note-104;*value=down;return 1;}
 if(note>=46&&note<=49){*kind=2;*channel=note-46;*value=down;return 1;}
#ifdef MIRROR_INPUT
 if(endpoint->profile==&mcu_profile_xtouch_mini&&(note==84||note==87||note==88||note==90)){*kind=11;*channel=note;*value=down;return 1;}
 if((mcu_has(endpoint,MCU_CAP_GLOBAL)&&((note>=54&&note<=61)||note==95||note==86||note==89||note==52||note==74||note==75||note==79||note==80||note==81||note==82||note==83||note==85))||(mcu_has(endpoint,MCU_CAP_JOG)&&note>=96&&note<=101)){*kind=11;*channel=note;*value=down;return 1;}
 if(mcu_has(endpoint,MCU_CAP_GLOBAL)&&(note==40||note==41||note==42||note==43||note==44||note==45||note==50||note==51||(note>=62&&note<=69))){*kind=6;*channel=note;*value=down;return 1;}
 if(note<40){*kind=5;*channel=note;*value=down;return 1;}
#endif
 return 0;
}
static int lost_events(snd_seq_t *seq){snd_seq_client_info_t *info;snd_seq_client_info_alloca(&info);return snd_seq_get_client_info(seq,info)<0||snd_seq_client_info_get_event_lost(info)!=0;}
static __attribute__((unused)) int surface_event(const snd_seq_event_t *event,snd_seq_addr_t full,unsigned *kind,unsigned *channel,int *value){return surface_event_profile(event,full,&controller,kind,channel,value);}
#ifdef MIRROR_INPUT
#include "channel-wire.h"
#endif
typedef struct {
 MotorIdentity identity;
 uint32_t source_revision,source_bits,field_incarnation;
 int target,physical;
 unsigned field,source_valid,physical_valid,acquired;
} HuiPickup;
typedef struct {
 uint32_t key[7],source_bits;
 int physical;
 unsigned valid,physical_valid,acquired;
} ProgramQLinkPickup;
typedef struct {unsigned layer;unsigned char held[68];ProgramQLinkPickup qlinks[16];} ControlProgramState;
typedef struct {snd_seq_t *seq;snd_seq_addr_t full,output,ingress;int sink,source;unsigned bank_down[4],button_down[40],display_field[8],general_down[128];
#ifdef MIRROR_INPUT
 SurfaceContext *context;
 unsigned shared_connection,mini_layer_consumed,mini_button_mode,mini_navigation_locked;
 snd_seq_addr_t stop_adapter,stop_mpc;int stop_sink;
 HuiInput hui_input;HuiPickup hui_pickup[HUI_STRIPS];uint32_t hui_connected_tick,hui_heartbeat_tick,hui_reply_tick;unsigned hui_heartbeat_valid,hui_reply_count;
 unsigned char hui_led[HUI_STRIPS][4],hui_led_valid[HUI_STRIPS],hui_transport[3];unsigned hui_transport_valid;
 MotorIdentity hui_ring_identity[HUI_STRIPS];uint32_t hui_ring_incarnation[HUI_STRIPS];
 unsigned char hui_ring_position[HUI_STRIPS],hui_ring_valid[HUI_STRIPS];
 NovationState novation;HuiPickup native_pickup[32];
 ControlProgramState program;
 MotorIdentity native_touch_identity[32];
 ChannelWire wire[8];unsigned char colors[8];unsigned wire_valid[8],colors_valid,next_wire,mode_valid,mode_led[128],playing_valid,playing_value,master_owner,master_incarnation,master_touch,master_tick;int master_sent;unsigned char position_text[10];unsigned position_valid,chooser_cleared;
 /* Last formatted playhead source, so an unchanged BBT skips formatting and
  * the ten-controller comparison instead of recomputing the same bytes. */
 unsigned position_bar,position_beat,position_clock,position_source;
 unsigned state_valid[8];
 unsigned meter_enabled[8],meter_identity[8],meter_valid,meter_tick,zoom_mode,data_wheel,name_value,display_until[8];
 /* Last channel-pressure level sent per strip, -1 when unknown. The hardware
  * decays on its own, so a sustained nonzero level is refreshed every cadence
  * while a repeated zero is suppressed until the level changes. */
 int meter_last[8];
#endif
} Surface;
#ifdef MIRROR_INPUT
typedef struct {
 SurfaceContext contexts[MCU_CONTROLLER_CAPACITY];
 Surface surfaces[MCU_CONTROLLER_CAPACITY];
 MirrorBank banks[MCU_CONTROLLER_CAPACITY];
 unsigned enabled[MCU_CONTROLLER_CAPACITY],count,focus,connected,interest_active,submit_cursor;
 NativePreferencesState *preferences;uint32_t preferences_lifetime,preferences_generation;
 snd_seq_t *seq;int sink,source,stop_sink;
 uint64_t topology_scans,topology_rows,routed,unrouted,encoded,sent,motor_messages,interest_visits,connections,disconnections;
} SurfaceCollection;
static SurfaceCollection surfaces;
#include "controller-admission.inc"
/* The adapter's local port precedes MPC's asynchronous subscription. Creating
 * our private relay is not admission to deliver: the session owner arms only
 * after route readiness, and every received Stop revalidates the exact route. */
static int collection_stop_ingress(SurfaceCollection *all){
 if(!stop_adapter_exe)return 1;
 all->stop_sink=snd_seq_create_simple_port(all->seq,STOP_RELAY_PORT,SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_NO_EXPORT,SND_SEQ_PORT_TYPE_APPLICATION);
 return all->stop_sink>=0;
}
#endif
/* Legacy same-port fixtures initialize only full; production discovery always
 * stores the independently resolved output address. */
static snd_seq_addr_t surface_output_address(const Surface *surface){
 return surface->output.client||surface->output.port?surface->output:surface->full;
}
static void motor_event(snd_seq_event_t*,const Surface*,unsigned,int);
#ifdef MIRROR_INPUT
static int parent_led_clear(Surface*);
static int channel_send(Surface*,snd_seq_event_t*);
static void native_teardown(Surface*);
static int native_consume(Surface*,MirrorBank*,const MirrorState*,ControllerRuntime*,const snd_seq_event_t*,int);
static int control_program_consume(Surface*,MirrorBank*,const MirrorState*,ControllerRuntime*,const snd_seq_event_t*,int);
#endif
#ifdef MIRROR_INPUT
static int surface_hui_event(Surface *surface,const snd_seq_event_t *event,uint32_t now,unsigned *kind,unsigned *channel,int *value){
 if(!address_equal(event->source,surface->full))return 0;
 unsigned char bytes[3];
 if(event->type==SND_SEQ_EVENT_CONTROLLER){
  if(event->data.control.channel||event->data.control.param>127||event->data.control.value<0||event->data.control.value>127)return 0;
  bytes[0]=0xb0;bytes[1]=(unsigned char)event->data.control.param;bytes[2]=(unsigned char)event->data.control.value;
 }else if(event->type==SND_SEQ_EVENT_NOTEON||event->type==SND_SEQ_EVENT_NOTEOFF){
  if(event->data.note.channel||event->data.note.note>127||event->data.note.velocity>127)return 0;
  bytes[0]=0x90;bytes[1]=(unsigned char)event->data.note.note;bytes[2]=(unsigned char)(event->type==SND_SEQ_EVENT_NOTEON?event->data.note.velocity:0);
 }else return 0;
 HuiIntent intent;int decoded=hui_decode(&surface->hui_input,bytes,now,&intent);if(decoded<=0)return decoded;
 *channel=intent.strip;*value=intent.value;
 switch(intent.kind){
  case HUI_INTENT_FADER:*kind=3;return 1;
  case HUI_INTENT_TOUCH:*kind=1;return 1;
  case HUI_INTENT_ENCODER:*kind=4;return 1;
  case HUI_INTENT_STRIP_BUTTON:{static const unsigned group[8]={0,3,2,1,0,0,0,0};unsigned g=group[intent.control];if(intent.control==HUI_BUTTON_ARM)g=0;*kind=5;*channel=g*8+intent.strip;return 1;}
  case HUI_INTENT_NAVIGATION:{static const unsigned nav[4]={2,0,3,1};*kind=2;*channel=nav[intent.control];return 1;}
  case HUI_INTENT_TRANSPORT:
   if(intent.control==HUI_TRANSPORT_STOP||intent.control==HUI_TRANSPORT_PLAY){*kind=12;*channel=intent.control==HUI_TRANSPORT_STOP?93:94;return 1;}
   if(intent.control==HUI_TRANSPORT_RECORD){*kind=11;*channel=95;return 1;}
   *kind=9;*channel=intent.control==HUI_TRANSPORT_REWIND?91:92;return 1;
  case HUI_INTENT_HEARTBEAT_REPLY:*kind=13;*channel=0;return 1;
  default:return 0;
 }
}

static int hui_source_owned(const MirrorInput *in,const MirrorBank *bank,unsigned strip,uint32_t bits){
 unsigned field=bank_field(bank,strip);if(field>=CF_COUNT)return 0;
 const InputGesture *g=&in->desires[strip][field];
 if(g->pending&&g->bits==bits&&!memcmp(&g->identity,&bank->faders[strip].identity,sizeof(g->identity)))return 1;
 for(unsigned i=0;i<COMMAND_SLOTS;i++){const InputFlight *f=in->flights+i;if((!f->origin||f->origin==in)&&f->flight&&f->strip==strip&&f->field==field&&f->bits==bits&&!memcmp(&f->target,&bank->faders[strip].identity,sizeof(f->target)))return 1;}
 return 0;
}

/* Exact segment crossing is the current nonmotor HUI pickup baseline.
 * The first physical report establishes a baseline and never acquires alone. */
static void hui_pickup_sync_one(Surface *surface,const MirrorInput *in,const MirrorBank *bank,unsigned strip,uint32_t now){
 HuiPickup *p=surface->hui_pickup+strip;const MirrorFader *f=bank->faders+strip;unsigned field=bank_field(bank,strip);int target=bank_target(bank,strip,now);
 if(!bank->ready||bank->chooser||f->empty||!f->available||field>=CF_COUNT||target<0){p->source_valid=p->acquired=0;return;}
 unsigned binding=memcmp(&p->identity,&f->identity,sizeof(p->identity))||p->field!=field||p->field_incarnation!=f->field_incarnation;
 if(binding){int physical=p->physical;unsigned physical_valid=p->physical_valid;memset(p,0,sizeof(*p));p->physical=physical;p->physical_valid=physical_valid;p->identity=f->identity;p->field=field;p->field_incarnation=f->field_incarnation;}
 if(!p->source_valid){p->source_valid=1;p->source_revision=f->revision;p->source_bits=f->bits;p->target=target;p->acquired=0;return;}
 if(p->source_bits!=f->bits){unsigned own=hui_source_owned(in,bank,strip,f->bits);p->source_bits=f->bits;p->target=target;if(!own)p->acquired=0;}
 p->source_revision=f->revision;
}

static void hui_pickup_sync(Surface *surface,const MirrorInput *in,const MirrorBank *bank,uint32_t now){
 if(SURFACE_VALUE(surface,controller).profile->protocol!=MCU_PROTOCOL_HUI||SURFACE_VALUE(surface,controller_hui_policy)!=MCU_HUI_PICKUP)return;
 for(unsigned i=0;i<HUI_STRIPS;i++)hui_pickup_sync_one(surface,in,bank,i,now);
}

static int hui_pickup_position(Surface *surface,unsigned strip,int position){
 if(SURFACE_VALUE(surface,controller_hui_policy)==MCU_HUI_MOTORIZED)return 1;
 HuiPickup *p=surface->hui_pickup+strip;int previous=p->physical;unsigned had=p->physical_valid;p->physical=position;p->physical_valid=1;
 if(!p->source_valid)return 0;
 if(p->acquired)return 1;
 if(!had||position==previous)return 0;
 if(previous==p->target||position==p->target||(previous<p->target&&position>p->target)||(previous>p->target&&position<p->target))p->acquired=1;
 return p->acquired;
}

enum {HUI_HEARTBEAT_INTERVAL_MS=1000,HUI_STARTUP_GRACE_MS=3000,HUI_REPLY_TIMEOUT_MS=2500};
static int hui_liveness_current(const Surface *surface,uint32_t now){
 if(SURFACE_VALUE(surface,controller).profile->protocol!=MCU_PROTOCOL_HUI)return 1;
 uint32_t since=surface->hui_reply_count?surface->hui_reply_tick:surface->hui_connected_tick;
 uint32_t limit=surface->hui_reply_count?HUI_REPLY_TIMEOUT_MS:HUI_STARTUP_GRACE_MS;
 return now>=since&&now-since<limit;
}

/* Closed-loop servo acknowledgement follows physical input immediately. It
 * changes neither MMV4 nor request settlement and is distinct from MPC output. */
static int surface_servo(Surface *s,MirrorBank *bank,unsigned strip,int position,uint32_t now){
 if(stopping||!SURFACE_VALUE(s,following).enabled)return 1;
 if(SURFACE_VALUE(s,servo_disabled)){INPUT_LOG("SERVO_SKIPPED fader=%u position=%d tick=%u; %s raw echo disabled\n",strip+1,position,now,SURFACE_VALUE(s,controller).profile->name);return 1;}
 snd_seq_event_t echo;motor_event(&echo,s,strip,position);
 if(!channel_send(s,&echo))return 0;
 bank->faders[strip].last_sent=position;bank->faders[strip].last_tick=now;
 INPUT_LOG("SERVO fader=%u position=%d tick=%u; physical acknowledgement only\n",strip+1,position,now);return 1;
}
/* A Mini has eight encoders and no strip faders. Its Track assignment owns
 * Volume directly; full-size MCU Track/Flip behavior stays in the bank core. */
static unsigned surface_encoder_field(const Surface *s,const MirrorBank *bank,unsigned strip){
 return SURFACE_VALUE(s,controller).profile==&mcu_profile_xtouch_mini&&bank->assignment==BA_TRACK?bank->volume_fields[strip]:bank_encoder_field(bank,strip);
}
#include "mini-surface.inc"
static void surface_encoder(Surface *s,MirrorInput *in,const MirrorBank *bank,const CopiedMirror *snapshot,unsigned strip,int delta,int push,uint32_t now){
 if(SURFACE_VALUE(s,controller).profile==&mcu_profile_xtouch_mini){
  if(bank->assignment==BA_MINI_SEND){mini_send_encoder(in,bank,snapshot,strip,delta,push,now);return;}
  if(push&&(bank->assignment==BA_TRACK||bank->assignment==BA_PAN)){input_control_bound(in,bank,snapshot,strip,CF_SELECTION,0,1,now,bank->strips+strip,3);return;}
 }
 if(bank->assignment==BA_IO){if(!push)input_io_edit(in,bank,snapshot,strip,delta,now);return;}
 if(bank->assignment==BA_QLINK){
  if(bank->flip){unsigned field=bank->volume_fields[strip];input_control_bound(in,bank,snapshot,strip,field,delta,push,now,&bank->strips[strip],3);s->display_field[strip]=field;s->display_until[strip]=now+2000;}
  else if(!push)input_qlink_edit(in,bank,snapshot,strip,delta,-1,now);
  return;
 }
 if(bank->assignment==BA_EFFECT){
  const EffectsCopy *e=&snapshot->effects;
  if(!snapshot->effects_available||e->serial!=bank->selected.serial||e->status!=EF_READY)return;
  if(bank->effects_slot==EFFECT_LIST){if(push&&in->jog_shift)input_effect_enable(in,bank,snapshot,strip,now);if(push&&!in->jog_shift&&strip<EFFECT_SLOTS&&e->slots[strip].status==EF_EMPTY)input_effect_chooser(in,bank,snapshot,strip,0,now);if(push&&!in->jog_shift&&strip<EFFECT_SLOTS&&e->slots[strip].status==EF_READY&&e->slots[strip].count){MirrorBank *mutable_bank=(MirrorBank*)bank;mutable_bank->effects_slot=strip;mutable_bank->effects_page=0;memset(s->wire_valid,0,sizeof(s->wire_valid));}}
  else if(bank->flip){
   unsigned field=bank->volume_fields[strip];input_control_bound(in,bank,snapshot,strip,field,delta,push,now,&bank->strips[strip],3);s->display_field[strip]=field;s->display_until[strip]=now+2000;
  }else input_effect_edit(in,bank,snapshot,strip,delta,push&&in->jog_shift?2:push,now);
  return;
 }
 unsigned field=surface_encoder_field(s,bank,strip);if(field>=CF_COUNT)return;
 unsigned selected=bank->assignment==BA_SEND&&!bank->flip;
 input_control_bound(in,bank,snapshot,strip,field,delta,push,now,bank_encoder_identity(bank,strip),selected?1:3);
 s->display_until[strip]=now>UINT32_MAX-2000?UINT32_MAX:now+2000;
 s->display_field[strip]=bank->assignment==BA_TRACK&&!bank->flip?CF_VOLUME:field;
}
static void surface_assignment(Surface *s,MirrorInput *in,MirrorBank *bank,const CopiedMirror *snapshot,unsigned note,uint32_t now){
 if(note==43&&in->jog_shift&&bank->assignment==BA_EFFECT&&bank->effects_slot<EFFECT_SLOTS){input_effect_chooser(in,bank,snapshot,bank->effects_slot,1,now);return;}
 if(note==50&&in->jog_shift){
  unsigned next=!SURFACE_VALUE(s,following).enabled;SURFACE_VALUE(s,preference_notice_error)=!preferences_path||!surface_preferences_write(preferences_path,next);
  if(!SURFACE_VALUE(s,preference_notice_error)){
   SURFACE_VALUE(s,following).enabled=next;
   following.enabled=next;
   if(next){
    SURFACE_VALUE(s,following).barrier=now;
    /* Old emitted positions cannot suppress reacquisition after movement
     * while Off. Keep observed travel/quiet and all touch/request ownership. */
    for(unsigned i=0;i<9;i++){
     SURFACE_VALUE(s,following).motors[i].last_sent=-1;
    }
    for(unsigned i=0;i<MIRROR_BANK;i++){bank->faders[i].last_sent=-1;bank->faders[i].wait=1;bank->faders[i].barrier=now;}
   }
  }
  SURFACE_VALUE(s,preference_notice_until)=now>UINT32_MAX-2000?UINT32_MAX:now+2000;memset(s->wire_valid,0,sizeof(s->wire_valid));
  fprintf(stderr,"MOTOR_FOLLOW %s preference_write=%s\n",SURFACE_VALUE(s,following).enabled?"ON":"OFF",SURFACE_VALUE(s,preference_notice_error)?"failed":"ok");return;
 }
 if(note==45){input_global_add(in,snapshot,GLOBAL_TRACK_NEW,0,now);return;}
 if(note==40&&in->jog_shift){input_global_add(in,snapshot,GLOBAL_TRACK_TYPE,0,now);return;}
 if(note==63&&!in->jog_shift){bank->assignment=BA_IO;bank->flip=0;bank_view(bank,BV_TRACK,now);}
 else if(note==50&&bank->assignment==BA_IO)return;
 else if(note==44){bank->assignment=BA_QLINK;bank->flip=0;bank->qlink_page=0;bank_view(bank,BV_TRACK,now);}
 else if(note==43){bank->assignment=BA_EFFECT;bank->flip=0;bank->effects_slot=EFFECT_LIST;bank->effects_page=0;bank_view(bank,BV_TRACK,now);}
 else if(note==50&&bank->assignment==BA_EFFECT){if(bank->effects_slot==EFFECT_LIST)return;bank->flip=!bank->flip;}
 else if((note==66&&!in->jog_shift)||(note==65&&in->jog_shift)){bank_view(bank,bank->view==BV_DRUM_PADS?BV_TRACK:BV_DRUM_PADS,now);bank->assignment=BA_TRACK;bank->flip=0;}
 else if(note==40||note==41||note==42){bank->assignment=note==40?BA_TRACK:note==41?BA_SEND:BA_PAN;bank->flip=note==41;if(note==41)bank_view(bank,BV_TRACK,now);if(note==40)bank_view(bank,BV_TRACK,now);}
 else if(note==50)bank->flip=!bank->flip;
 else{static const unsigned views[8]={BV_MIDI,BV_INPUT,BV_AUDIO,BV_INSTRUMENT,BV_RETURN,BV_SUBMIX,BV_OUTPUT,BV_ALL};bank_view(bank,note==51?BV_ALL:views[note-62],now);if(note==66||bank->assignment==BA_SEND||bank->assignment==BA_EFFECT||bank->assignment==BA_QLINK||bank->assignment==BA_IO){bank->assignment=BA_TRACK;bank->flip=0;}}
 bank->fader_field=bank->flip&&bank->assignment!=BA_SEND&&bank->assignment!=BA_EFFECT&&bank->assignment!=BA_QLINK?CF_PAN:CF_VOLUME;
 bank_drop(bank,now);input_discard(in);memset(s->display_until,0,sizeof(s->display_until));
 for(unsigned j=0;j<8;j++)s->display_field[j]=bank->assignment==BA_TRACK?CF_VOLUME:bank_parameter(bank,j);
 BRIDGE_LOG("MODE view=%u assignment=%u flip=%u; unsent work canceled\n",bank->view,bank->assignment,bank->flip);
}
/* In data-wheel mode Shift selects notes in the focused Grid; ordinary turns
 * retain the native data wheel. Rewind/Forward are unchanged. */
static void surface_jog(MirrorInput *in,const CopiedMirror *snapshot,unsigned kind,unsigned key,int value,uint32_t now,unsigned data_wheel){
 input_jog_resync(in,snapshot);
 if(kind==8){if(data_wheel){if(in->jog_shift)input_note_add(in,snapshot,value);else input_wheel_add(in,snapshot,value);}else input_jog_add(in,snapshot,in->jog_shift?JOG_PULSE:JOG_BEAT,value,now);}
 else{if(key==70)in->jog_shift=value;else if(key==91)in->jog_rewind=value;else in->jog_forward=value;if(!value){in->jog_repeat_tick=now;if(key==91||key==92)input_jog_release(in,key==92?1:2);}}
}
static void surface_navigation(MirrorInput *in,MirrorBank *bank,const CopiedMirror *snapshot,unsigned button,uint32_t now){
 if(button<2&&bank->assignment==BA_QLINK){if(in->jog_shift){input_mode_add(in,snapshot,button?1:-1,now);return;}if(bank->qlink_page!=button){bank->qlink_page=button;bank_drop(bank,now);input_discard(in);}return;}
 if(button<2&&bank->assignment==BA_EFFECT){
  const EffectsCopy *e=&snapshot->effects;if(bank->effects_slot==EFFECT_LIST||!snapshot->effects_available||bank->effects_slot>=EFFECT_SLOTS)return;
  unsigned pages=(e->slots[bank->effects_slot].presentation_count+7)/8;if(button&&bank->effects_page+1<pages)bank->effects_page++;else if(!button&&bank->effects_page)bank->effects_page--;return;
 }
 if(button<2&&bank->assignment==BA_SEND)return;
 if(button<2){unsigned before=bank->offset;bank_navigate(bank,button?1:-1,now);if(before!=bank->offset)input_bank_discard(in);}
 else input_channel(in,bank,snapshot,button==3?1:-1,now);
}
static void surface_general(Surface *surface,MirrorInput *in,const CopiedMirror *snapshot,unsigned note,uint32_t now){
 if(note>=54&&note<=61){input_global_add(in,snapshot,GLOBAL_PAGE_MAIN+note-54,0,now);SURFACE_VALUE(surface,requested_page)=note-54;SURFACE_VALUE(surface,page_notice_until)=now+2000;memset(surface->wire_valid,0,sizeof(surface->wire_valid));return;}
 if(note==52){surface->name_value=!surface->name_value;memset(surface->wire_valid,0,sizeof(surface->wire_valid));return;}
 /* In data-wheel mode the cursor cluster works the focused control with the
  * wheel instead of moving the playhead view: up/down ride the same
  * single-flight data-wheel lane one step at a time, left/right move focus with
  * Shift+Tab and Tab, and the centre button is the data wheel's own push, which
  * is what Enter means for a focused item on this firmware. An injected
  * keyboard Return does not reach that path, so the centre button rides the
  * focus-controller lane beside the steps instead of the key lane. Zoom keeps
  * note 100 in scrub mode, so the two modes never both own a button. */
 if(surface->data_wheel&&note>=96&&note<=100){
  if(note==96||note==97){input_wheel_add(in,snapshot,note==96?-1:1);return;}
  if(note==100){if(X_TOUCH_BLOCKING_ENABLED)input_press_add(in,snapshot);return;}
  input_global_add(in,snapshot,note==98?GLOBAL_KEY_BACKTAB:GLOBAL_KEY_TAB,0,now);return;
 }
 if(note==100){surface->zoom_mode=!surface->zoom_mode;return;}
 /* Scrub: the jog wheel becomes the MPC data wheel. Session state only; a
  * reconnect clears it with the rest of the Surface. */
 if(note==101){surface->data_wheel=!surface->data_wheel;return;}
 /* Replace (note 85) was unassigned. It is Duplicate Sequence: one press copies
  * the current sequence into the first unused slot under the application's own
  * default name, as one undoable Copy Sequence command. Shift is not consumed
  * here, so the button keeps one meaning in both modes. */
 unsigned op=note==95?GLOBAL_RECORD_TOGGLE:note==89?GLOBAL_CLICK_TOGGLE:note==86?GLOBAL_LOOP_TOGGLE:note==74||note==75||note==79?CF_AUTOMATION:note==80?GLOBAL_SAVE:note==81?(in->jog_shift?GLOBAL_REDO:GLOBAL_UNDO):note==82?GLOBAL_KEY_CANCEL:note==83?GLOBAL_KEY_ENTER:(note==85&&X_TOUCH_BLOCKING_ENABLED)?GLOBAL_SEQ_DUPLICATE:0;
 if(note>=96&&note<=99){
  static const unsigned ordinary[]={GLOBAL_KEY_UP,GLOBAL_KEY_DOWN,GLOBAL_KEY_LEFT,GLOBAL_KEY_RIGHT};
  static const unsigned zoom[]={GLOBAL_ZOOM_UP,GLOBAL_ZOOM_DOWN,GLOBAL_ZOOM_OUT,GLOBAL_ZOOM_IN};
  op=(surface->zoom_mode?zoom:ordinary)[note-96];
 }
 if(op)input_global_add(in,snapshot,op,note==74?1:note==75?2:0,now);
}
static void surface_general_press(Surface *surface,MirrorInput *in,const CopiedMirror *snapshot,unsigned note,int down,uint32_t now){
 if(down&&!surface->general_down[note])surface_general(surface,in,snapshot,note,now);
 surface->general_down[note]=down;
}
static int surface_pitch(Surface *s,MirrorInput *in,MirrorBank *bank,const CopiedMirror *snapshot,unsigned strip,int position,uint32_t now){
 if(stopping)return 1;
 if(bank->assignment==BA_QLINK&&bank->flip)input_qlink_pitch(in,bank,snapshot,strip,position,now);else if(bank->assignment==BA_EFFECT&&bank->flip)input_effect_pitch(in,bank,snapshot,strip,position,now);else input_pitch(in,bank,strip,position,now);motor_follow_report(&SURFACE_VALUE(s,following).motors[strip],position,now,bank->assignment==BA_QLINK&&bank->flip?in->qlinks[bank->qlink_page*8+strip].pending:bank->assignment==BA_EFFECT&&bank->flip?in->effects[strip].pending:bank_field(bank,strip)<CF_COUNT&&in->desires[strip][bank_field(bank,strip)].pending);s->display_field[strip]=bank_field(bank,strip);s->display_until[strip]=now>UINT32_MAX-2000?UINT32_MAX:now+2000;
 return surface_servo(s,bank,strip,position,now);
}
#endif
static void surface_close(Surface *s,MirrorBank *bank){
#ifdef MIRROR_INPUT
 SurfaceContext *context=s->context;
 if(s->seq&&mcu_profile_native(SURFACE_VALUE(s,controller).profile))native_teardown(s);
 if(s->seq&&!s->shared_connection&&stop_adapter_exe&&s->stop_sink>=0){snd_seq_delete_simple_port(s->seq,s->stop_sink);s->stop_sink=-1;}
#endif
#ifdef MIRROR_INPUT
 if(s->seq&&!mcu_profile_native(SURFACE_VALUE(s,controller).profile)&&!mcu_profile_program(SURFACE_VALUE(s,controller).profile)&&(bank->view==BV_DRUM_PADS||bank->assignment==BA_SEND)&&!stopping)(void)parent_led_clear(s);
#endif
#ifdef MIRROR_INPUT
 input_clip_feedback_clear(&SURFACE_VALUE(s,input));input_discard(&SURFACE_VALUE(s,input));input_jog_discard(&SURFACE_VALUE(s,input));input_master_discard(&SURFACE_VALUE(s,input));
#endif
#ifdef MIRROR_INPUT
 motor_follow_reset(&SURFACE_VALUE(s,following));
#endif
 if(s->seq){
#ifdef MIRROR_INPUT
  if(s->shared_connection)snd_seq_disconnect_from(s->seq,s->sink,s->full.client,s->full.port);
  else
#endif
  snd_seq_close(s->seq);
 }
 memset(s,0,sizeof(*s));
#ifdef MIRROR_INPUT
 s->context=context;
#endif
 bank_disconnect(bank);bank->fader_field=CF_VOLUME;bank->assignment=BA_TRACK;bank->flip=0;}
/* 1 drained, 0 connection/input continuity lost. No output is queued. */
#ifdef MIRROR_INPUT
static int surface_stop_route(Surface *s,const MirrorState *state){
 int adapter_pid=0,mpc_pid=0;
 if(!stop_adapter_exe||stop_find_port(s->seq,"mpclearn-controls","mpclearn-controls MIDI",stop_adapter_exe,SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,SND_SEQ_PORT_CAP_WRITE,&s->stop_adapter,&adapter_pid)!=1||stop_find_port(s->seq,"MPC","mpclearn-controls MIDI","/usr/bin/MPC",SND_SEQ_PORT_CAP_WRITE,SND_SEQ_PORT_CAP_READ,&s->stop_mpc,&mpc_pid)!=1||(unsigned)mpc_pid!=state->pid)return 0;
 snd_seq_port_subscribe_t *sub;snd_seq_port_subscribe_alloca(&sub);snd_seq_port_subscribe_set_sender(sub,&s->stop_adapter);snd_seq_port_subscribe_set_dest(sub,&s->stop_mpc);
 return snd_seq_get_port_subscription(s->seq,sub)>=0;
}
/* Called only for the exact adapter's private relay. Classify before sending;
 * no post-send source sample can reinterpret this same physical press. */
static int surface_stop_relay(Surface *s,MirrorInput *in,const CopiedMirror *snapshot,int copied,int down,uint32_t now){
 if(down&&in->stop_down)return 1;
 if(copied&&snapshot->ready&&snapshot->alive&&!snapshot->error)input_stop_button(in,snapshot,93,down,now);
 else{in->stop_home=0;input_jog_release(in,3);in->stop_down=down;}
 snd_seq_event_t out;snd_seq_ev_clear(&out);
 if(down)snd_seq_ev_set_noteon(&out,0,93,127);else snd_seq_ev_set_noteoff(&out,0,93,0);
 snd_seq_ev_set_source(&out,s->source);snd_seq_ev_set_dest(&out,s->stop_mpc.client,s->stop_mpc.port);snd_seq_ev_set_direct(&out);
 int rc=snd_seq_event_output_direct(s->seq,&out);
 if(rc<0){in->stop_home=0;input_jog_release(in,3);fprintf(stderr,"Relayed Stop output failed: %s; no replay\n",snd_strerror(rc));return 0;}
 return 1;
}
#endif
static int surface_consume(Surface *s,MirrorBank *bank,const MirrorState *state,int discard,ControllerRuntime *runtime,const snd_seq_event_t *provided){
 if(stopping)return 1;
#ifndef MIRROR_INPUT
 (void)runtime;
#endif
#ifdef MIRROR_INPUT
 MIRROR_COPY(batch_snapshot);const CopiedMirror *snapshot=&batch_snapshot;int batch_valid=0,batch_pads=0;
#endif
 for(unsigned n=0;n<256;n++){
  snd_seq_event_t *received;const snd_seq_event_t *event=provided;
  if(provided){if(n)return 1;}
  else{int rc=snd_seq_event_input(s->seq,&received);if(rc==-EAGAIN)return !lost_events(s->seq);if(rc<0)return 0;event=received;}
  if(event->source.client==SND_SEQ_CLIENT_SYSTEM&&event->source.port==SND_SEQ_PORT_SYSTEM_ANNOUNCE){
   if(event->type==SND_SEQ_EVENT_CLIENT_EXIT&&(event->data.addr.client==s->full.client||event->data.addr.client==s->output.client))return 0;
   if(event->type==SND_SEQ_EVENT_PORT_EXIT&&(address_equal(event->data.addr,s->full)||address_equal(event->data.addr,s->output)))return 0;
   if(event->type==SND_SEQ_EVENT_PORT_UNSUBSCRIBED&&address_equal(event->data.connect.dest,s->ingress))return 0;
  }
#ifdef MIRROR_INPUT
  if(stop_adapter_exe&&event->dest.port==s->stop_sink){
   if(discard)continue;
   if(!surface_stop_route(s,state))return 0;
   if(!address_equal(event->source,s->stop_adapter)||(event->type!=SND_SEQ_EVENT_NOTEON&&event->type!=SND_SEQ_EVENT_NOTEOFF)||event->data.note.channel||event->data.note.note!=93||event->data.note.velocity>127)continue;
   uint32_t at;MIRROR_COPY(before);const CopiedMirror *stop_snapshot=&before;int fresh;
   if(runtime){fresh=runtime_snapshot(runtime,state,bank->view==BV_DRUM_PADS);at=runtime->now;stop_snapshot=runtime->snapshot;}
   else{at=source_now(state);fresh=at==UINT32_MAX?0:fresh_copy(state,&before,&at);}
   if(!surface_stop_relay(s,&SURFACE_VALUE(s,input),stop_snapshot,fresh>0&&!atomic_load(&SURFACE_VALUE(s,input).commands->new_project_intent),event->type==SND_SEQ_EVENT_NOTEON&&event->data.note.velocity,at))return 0;
   if(fresh<0)return 0;
   continue;
  }
#endif
  unsigned kind,channel;int value,match;
#ifdef MIRROR_INPUT
  if(mcu_profile_program(SURFACE_VALUE(s,controller).profile)){if(!control_program_consume(s,bank,state,runtime,event,discard))return 0;continue;}
  if(mcu_profile_native(SURFACE_VALUE(s,controller).profile)){if(!native_consume(s,bank,state,runtime,event,discard))return 0;continue;}
  uint32_t wire_now=0;if(SURFACE_VALUE(s,controller).profile->protocol==MCU_PROTOCOL_HUI){wire_now=source_now(state);if(wire_now==UINT32_MAX)return 0;}
  match=SURFACE_VALUE(s,controller).profile->protocol==MCU_PROTOCOL_HUI?surface_hui_event(s,event,wire_now,&kind,&channel,&value):surface_event_profile(event,s->full,&SURFACE_VALUE(s,controller),&kind,&channel,&value);
#else
  match=surface_event(event,s->full,&kind,&channel,&value);
#endif
  if(match<0)return 0;
  if(stopping)return 1;
#ifdef MIRROR_INPUT
  /* HUI liveness belongs to the exact MIDI source, not to musical snapshot
   * readiness. Record only this protocol reply before chooser/unready gates;
   * every control intent still follows the normal source/command checks. */
  if(match&&!discard&&SURFACE_VALUE(s,controller).profile->protocol==MCU_PROTOCOL_HUI&&kind==13){s->hui_reply_tick=wire_now;s->hui_reply_count++;continue;}
#endif
  if(match&&!discard){
   uint32_t now=source_now(state);if(now==UINT32_MAX)return 0;
#ifdef MIRROR_INPUT
   int pads=bank->view==BV_DRUM_PADS;
   /* A bounded MIDI batch shares the runtime cycle snapshot. Desires coalesce
    * in order and native execution still checks every target and revision. */
   int copied=runtime?runtime_snapshot(runtime,state,pads):batch_valid;
   if(runtime){snapshot=runtime->snapshot;now=runtime->now;}
   else if(!batch_valid||batch_pads!=pads||!mirror_selection_current(state,snapshot)||snapshot->revision!=atomic_load_explicit(&state->revision,memory_order_acquire)||snapshot->epoch!=atomic_load(&state->epoch)){
    copied=fresh_copy_view(state,&batch_snapshot,&now,pads);if(copied<0)return 0;batch_valid=copied;batch_pads=pads;
   }
   uint32_t heartbeat=atomic_load_explicit(&state->heartbeat,memory_order_acquire);now=source_now(state);
   if(now==UINT32_MAX||!atomic_load(&state->alive)||atomic_load(&state->error)||now<heartbeat||now-heartbeat>=1000)return 0;
   if(copied&&(now<snapshot->heartbeat||now-snapshot->heartbeat>=1000))return 0;
   if(copied)bank_apply(bank,snapshot,now);else bank_drop(bank,now);
   input_sync(&SURFACE_VALUE(s,input),bank);
   hui_pickup_sync(s,&SURFACE_VALUE(s,input),bank,now);
   if(!copied||!snapshot->ready||atomic_load(&SURFACE_VALUE(s,input).commands->new_project_intent)){
    unsigned held_shift=SURFACE_VALUE(s,input).jog_shift;input_discard(&SURFACE_VALUE(s,input));input_jog_discard(&SURFACE_VALUE(s,input));SURFACE_VALUE(s,input).jog_shift=held_shift;input_master_invalidate(&SURFACE_VALUE(s,input));
    if(SURFACE_VALUE(s,controller).profile==&mcu_profile_xtouch_mini&&(kind==6||kind==9||kind==11||kind==12)){
     mini_unready_button(s,&SURFACE_VALUE(s,input),snapshot,channel,value,now);continue;
    }
    if(kind==1)bank_touch(bank,channel,value,now);
    else if(kind==7){s->master_touch=value;s->master_tick=now;}
    else if(kind==3){if(SURFACE_VALUE(s,controller).profile->protocol==MCU_PROTOCOL_HUI)(void)hui_pickup_position(s,channel,value);else{bank->faders[channel].physical=value;motor_follow_report(&SURFACE_VALUE(s,following).motors[channel],value,now,0);}}
    else if(kind==10&&mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_MASTER_MOTOR))motor_follow_report(&SURFACE_VALUE(s,following).motors[8],value,now,0);
    else if(kind==9&&channel==70)SURFACE_VALUE(s,input).jog_shift=value;
    else if(kind==6&&channel==50){if(value&&!s->general_down[channel]&&SURFACE_VALUE(s,input).jog_shift)surface_assignment(s,&SURFACE_VALUE(s,input),bank,snapshot,channel,now);s->general_down[channel]=value;}
    else if(kind==5)s->button_down[channel]=value;
    else if(kind==6||kind==11){s->general_down[channel]=value;if(SURFACE_VALUE(s,controller).profile==&mcu_profile_xtouch_mini&&(channel==84||channel==85))s->mini_layer_consumed|=1u<<(channel-84);}
    else if(kind==2)s->bank_down[channel]=value;
    continue;
   }
   if((kind==6||kind==9||kind==11||kind==12)&&mini_button(s,&SURFACE_VALUE(s,input),bank,snapshot,channel,value,now))continue;
   if(kind==1){s->display_until[channel]=now>UINT32_MAX-2000?UINT32_MAX:now+2000;input_touch(&SURFACE_VALUE(s,input),bank,snapshot,channel,value,now);INPUT_LOG("TOUCH fader=%u %s; channel motion input=%u tick=%u\n",channel+1,value?"down":"up",SURFACE_VALUE(s,input).gestures[channel].held,now);}
   else if(kind==7){s->master_touch=value;input_master_touch(&SURFACE_VALUE(s,input),snapshot,value);}
   else if(kind==10){input_master_pitch(&SURFACE_VALUE(s,input),snapshot,value,now);if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_MASTER_MOTOR))motor_follow_report(&SURFACE_VALUE(s,following).motors[8],value,now,SURFACE_VALUE(s,input).master.pending);}
   else if(kind==12)input_stop_button(&SURFACE_VALUE(s,input),snapshot,channel,value,now);
   else if(kind==8||kind==9)surface_jog(&SURFACE_VALUE(s,input),snapshot,kind,channel,value,now,s->data_wheel);
   else if(kind==11)surface_general_press(s,&SURFACE_VALUE(s,input),snapshot,channel,value,now);
   else if(kind==3){if(SURFACE_VALUE(s,controller).profile->protocol!=MCU_PROTOCOL_HUI||hui_pickup_position(s,channel,value))if(!surface_pitch(s,&SURFACE_VALUE(s,input),bank,snapshot,channel,value,now))return 0;}
   else if(kind==4)surface_encoder(s,&SURFACE_VALUE(s,input),bank,snapshot,channel,value,0,now);
   else if(kind==5){if(value&&!s->button_down[channel]){
    unsigned strip=channel%8;
    if(channel>=32)surface_encoder(s,&SURFACE_VALUE(s,input),bank,snapshot,strip,0,1,now);
    else{unsigned fields[4]={CF_ARM,CF_SOLO,CF_MUTE,CF_SELECTION};unsigned field=fields[channel/8];input_control(&SURFACE_VALUE(s,input),bank,snapshot,strip,field,0,1,now);s->display_field[strip]=field;s->display_until[strip]=now>UINT32_MAX-2000?UINT32_MAX:now+2000;}
   }s->button_down[channel]=value;}
   else if(kind==6){if(value&&!s->general_down[channel])surface_assignment(s,&SURFACE_VALUE(s,input),bank,snapshot,channel,now);s->general_down[channel]=value;}
   else{if(value&&!s->bank_down[channel])surface_navigation(&SURFACE_VALUE(s,input),bank,snapshot,channel,now);s->bank_down[channel]=(unsigned)value;}
#else
   if(kind==1){bank_touch(bank,channel,value,now);INPUT_LOG("TOUCH fader=%u %s state=%d tick=%u\n",channel+1,value?"down":"up",bank->faders[channel].touch,now);}
   else if(kind==2){if(value&&!s->bank_down[channel]){if(channel<2)bank_navigate(bank,channel?1:-1,now);else bank_channel(bank,channel==3?1:-1,now);BRIDGE_LOG("BANK first_playable=%u\n",bank->offset+1);}s->bank_down[channel]=(unsigned)value;}
#endif
  }
 }
 return snd_seq_event_input_pending(s->seq,1)==0&&!lost_events(s->seq);
}
static int surface_drain(Surface *s,MirrorBank *bank,const MirrorState *state,int discard,ControllerRuntime *runtime){return surface_consume(s,bank,state,discard,runtime,NULL);}
static __attribute__((unused)) int surface_open(Surface *s,MirrorBank *bank,const MirrorState *state,snd_seq_t *discovery){
 /* Enumerate through a retained portless client. An absent controller must
  * not create/destroy ALSA clients and broadcast topology changes at4Hz. */
 snd_seq_addr_t found_input,found_output;
 if(!discover_endpoints(discovery,&SURFACE_VALUE(s,controller),MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ,&found_input,&SURFACE_VALUE(s,controller_output),MCU_ENDPOINT_WRITE,&found_output))return 0;
#ifdef MIRROR_INPUT
 s->stop_sink=-1;
#endif
 if(snd_seq_open(&s->seq,"default",SND_SEQ_OPEN_DUPLEX,SND_SEQ_NONBLOCK)<0)return 0;
 if(snd_seq_set_client_name(s->seq,"mpclearn-mirror-motors")<0||
    !discover_endpoints(s->seq,&SURFACE_VALUE(s,controller),MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ,&s->full,&SURFACE_VALUE(s,controller_output),MCU_ENDPOINT_WRITE,&s->output)||
    snd_seq_set_client_pool_input(s->seq,256)<0||snd_seq_set_client_pool_output(s->seq,16)<0)goto fail;
 s->sink=snd_seq_create_simple_port(s->seq,"private MCU touch and bank",SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_NO_EXPORT,SND_SEQ_PORT_TYPE_APPLICATION);
 s->source=snd_seq_create_simple_port(s->seq,"MCU eight motor output",SND_SEQ_PORT_CAP_READ,SND_SEQ_PORT_TYPE_APPLICATION);
 if(s->sink<0||s->source<0)goto fail;
 s->ingress=(snd_seq_addr_t){.client=snd_seq_client_id(s->seq),.port=s->sink};
 if(snd_seq_connect_from(s->seq,s->sink,s->full.client,s->full.port)<0||snd_seq_connect_from(s->seq,s->sink,SND_SEQ_CLIENT_SYSTEM,SND_SEQ_PORT_SYSTEM_ANNOUNCE)<0)goto fail;
#ifdef MIRROR_INPUT
 if(stop_adapter_exe&&!surface_stop_route(s,state))goto fail;
#endif
 if(!surface_drain(s,bank,state,1,NULL))goto fail;
#ifdef MIRROR_INPUT
 if(SURFACE_VALUE(s,controller).profile->protocol==MCU_PROTOCOL_HUI){hui_input_reset(&s->hui_input);s->hui_connected_tick=source_now(state);if(s->hui_connected_tick==UINT32_MAX)goto fail;}
 /* Publish takeover only after the startup discard. Until then the adapter
  * forwards ordinary Stop itself, so no routed press is intentionally dropped. */
 if(stop_adapter_exe){
  s->stop_sink=snd_seq_create_simple_port(s->seq,STOP_RELAY_PORT,SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_NO_EXPORT,SND_SEQ_PORT_TYPE_APPLICATION);
  if(s->stop_sink<0)goto fail;
 }
#endif
 bank_disconnect(bank);
#ifdef MIRROR_INPUT
 for(unsigned i=0;i<8;i++)if(SURFACE_VALUE(s,inherited_holds)&(1u<<i))bank->faders[i].touch=MT_DOWN;
 s->master_touch=(SURFACE_VALUE(s,inherited_holds)>>8)&1;SURFACE_VALUE(s,inherited_holds)=0;
#endif
 BRIDGE_LOG("CONNECTED profile=%s from=%s/%s address=%u:%u type=%s to=%s/%s address=%u:%u type=%s; automatic fresh-source sync unless touch-down observed; physical touch on connection unknown\n",SURFACE_VALUE(s,controller).profile->name,SURFACE_VALUE(s,controller).client,SURFACE_VALUE(s,controller).port,s->full.client,s->full.port,endpoint_type_name(SURFACE_VALUE(s,controller).physical_only),SURFACE_VALUE(s,controller_output).client,SURFACE_VALUE(s,controller_output).port,s->output.client,s->output.port,endpoint_type_name(SURFACE_VALUE(s,controller_output).physical_only));return 1;
fail:surface_close(s,bank);return 0;
}
static void motor_event(snd_seq_event_t *event,const Surface *surface,unsigned channel,int position){snd_seq_addr_t output=surface_output_address(surface);snd_seq_ev_clear(event);snd_seq_ev_set_pitchbend(event,channel,position-8192);snd_seq_ev_set_source(event,surface->source);snd_seq_ev_set_dest(event,output.client,output.port);snd_seq_ev_set_direct(event);}
#ifdef MIRROR_INPUT
static int channel_send(Surface *s,snd_seq_event_t *e){
 if(stopping)return 0;
 /* Apply physical LED capability at the common output boundary, including
  * chooser/disconnect clears. Suppressed notes never reach ALSA or counters. */
 if((e->type==SND_SEQ_EVENT_NOTEON||e->type==SND_SEQ_EVENT_NOTEOFF)&&!mcu_led_note_supported(SURFACE_VALUE(s,controller).profile,e->data.note.channel,e->data.note.note))return 1;
 snd_seq_addr_t output=surface_output_address(s);snd_seq_ev_set_source(e,s->source);snd_seq_ev_set_dest(e,output.client,output.port);snd_seq_ev_set_direct(e);surfaces.encoded++;if(s->context)s->context->encoded++;int ok=snd_seq_event_output_direct(s->seq,e)>=0;surfaces.sent+=ok;if(s->context)s->context->sent+=ok;return ok;}
/* TouchMCU9ea4881 protocol thresholds; Ardour e0d72575b1 confirms the
 * channel-pressure framing and per-strip enable. Scaling is host-specific.
 * Use max(L,R) native envelope amplitude, no invented held-overload latch. */
static unsigned meter_level(const CopiedMeter *m){
 float l,r;memcpy(&l,&m->left,4);memcpy(&r,&m->right,4);float peak=l>r?l:r;
 if(!(peak>0))return 0;
 float db=20.0f*log10f(peak);static const float threshold[]={-60,-50,-40,-30,-20,-14,-10,-8,-6,-4,-2,0};
 unsigned level=0;while(level<12&&db>=threshold[level])level++;
 return db>0?13:level;
}
static int meter_output(Surface *s,const MirrorBank *bank,const CopiedMirror *snapshot){
 if(!mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_METERS)){s->meter_valid=0;return 1;}
 if(!bank->ready&&!s->meter_valid)return 1;
 unsigned due=!s->meter_valid||snapshot->heartbeat<s->meter_tick||snapshot->heartbeat-s->meter_tick>=50;
 for(unsigned i=0;i<8;i++){
  const CopiedTrack *t=bank->ready?bank_strip_track(snapshot,bank,i):NULL;
  unsigned enabled=t&&t->meter.available,id=enabled?t->meter.incarnation:0;
  snd_seq_event_t e;
  if(!s->meter_valid||s->meter_enabled[i]!=enabled||s->meter_identity[i]!=id){
   unsigned char bytes[]={0xf0,0,0,0x66,0x14,0x20,(unsigned char)i,enabled?7:0,0xf7};
   snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,sizeof(bytes),bytes);if(!channel_send(s,&e))return 0;
   snd_seq_ev_clear(&e);snd_seq_ev_set_chanpress(&e,0,(i<<4)|0xf);if(!channel_send(s,&e))return 0;
   if(!enabled){snd_seq_ev_clear(&e);snd_seq_ev_set_chanpress(&e,0,i<<4);if(!channel_send(s,&e))return 0;}
   s->meter_enabled[i]=enabled;s->meter_identity[i]=id;s->meter_last[i]=-1; /* enable/identity change leaves the hardware level unknown */
  }
  /* Sustained native levels refresh hardware's own decay. A stale source is
   * disabled rather than refreshed with an invented zero or synthetic decay.
   * A repeated zero is the one level the hardware cannot decay further, so it
   * is sent once and then suppressed until the level changes; every nonzero
   * level is still re-sent on each cadence. */
  if(enabled&&due){
   int level=(int)meter_level(&t->meter);
   if(level||s->meter_last[i]!=0){snd_seq_ev_clear(&e);snd_seq_ev_set_chanpress(&e,0,(i<<4)|(unsigned)level);if(!channel_send(s,&e))return 0;s->meter_last[i]=level;}
  }
 }
 s->meter_valid=1;if(due)s->meter_tick=snapshot->heartbeat;return 1;
}
static int chooser_output(Surface *s){
 if(s->chooser_cleared)return 1;
 snd_seq_event_t e;unsigned char bytes[15],blank[7];memset(blank,' ',7);
 /* Presentation only: no source values or native input are manufactured. */
 for(unsigned note=0;note<40;note++){snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,note,0);if(!channel_send(s,&e))return 0;}
 if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_GLOBAL))for(unsigned note=40;note<=103;note++){if((note==70||(note>=96&&note<=101))&&!mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_JOG))continue;snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,note,0);if(!channel_send(s,&e))return 0;}
 if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_TIME)){
  for(unsigned note=113;note<=114;note++){snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,note,0);if(!channel_send(s,&e))return 0;}
  for(unsigned i=0;i<10;i++){snd_seq_ev_clear(&e);snd_seq_ev_set_controller(&e,0,0x49-i,' ');if(!channel_send(s,&e))return 0;}
 }
 for(unsigned i=0;i<8;i++){
  if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_LCD))for(unsigned row=0;row<2;row++){snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,channel_lcd(bytes,i,row,blank),bytes);if(!channel_send(s,&e))return 0;}
  snd_seq_ev_clear(&e);snd_seq_ev_set_controller(&e,0,0x30+i,0);if(!channel_send(s,&e))return 0;
  if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_METERS)){
   unsigned char off[]={0xf0,0,0,0x66,0x14,0x20,(unsigned char)i,0,0xf7};
   snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,sizeof(off),off);if(!channel_send(s,&e))return 0;
   snd_seq_ev_clear(&e);snd_seq_ev_set_chanpress(&e,0,(i<<4)|15);if(!channel_send(s,&e))return 0;
   snd_seq_ev_clear(&e);snd_seq_ev_set_chanpress(&e,0,i<<4);if(!channel_send(s,&e))return 0;
  }
 }
 if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_COLOR)){memset(s->colors,0,sizeof(s->colors));snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,channel_colors(bytes,s->colors),bytes);if(!channel_send(s,&e))return 0;}
 memset(s->wire_valid,0,sizeof(s->wire_valid));memset(s->state_valid,0,sizeof(s->state_valid));
 s->colors_valid=s->mode_valid=s->position_valid=s->playing_valid=s->meter_valid=0;s->chooser_cleared=1;return 1;
}
static int motor_output(Surface *s,unsigned channel,const uint32_t key[16],int target,uint32_t now,int allowed){
 MotorFollow *m=SURFACE_VALUE(s,following).motors+channel;motor_follow_target(m,key,target,now);
 int position=motor_follow_due(m,now,allowed&&SURFACE_VALUE(s,following).enabled&&!stopping);if(position<0)return 1;
 snd_seq_event_t e;motor_event(&e,s,channel,position);if(!channel_send(s,&e))return 0;motor_follow_sent(m,position,now);return 1;
}
static int strip_motor_ready(Surface *s,MirrorBank *bank,unsigned channel,uint32_t now){
 if(!mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_STRIP_MOTOR))return 0;
 uint32_t key[16]={0};MirrorFader *f=bank->faders+channel;
 memcpy(key,&f->identity,sizeof(MotorIdentity));if(f->effect[0])memcpy(key+11,f->effect,sizeof(f->effect));else{key[11]=f->field_incarnation;key[12]=bank_field(bank,channel);key[15]=bank->chooser?2:f->empty?3:1;}
 int target=bank_target(bank,channel,now);if(target>=0)motor_follow_target(&SURFACE_VALUE(s,following).motors[channel],key,target,now);
 if(bank->assignment==BA_QLINK&&bank->flip){
  if(input_mode_busy(&SURFACE_VALUE(s,input)))return 0;
  unsigned index=bank->qlink_page*8+channel;
  InputQLink *g=SURFACE_VALUE(s,input).qlinks+index;
  unsigned same=g->request.epoch==bank->epoch&&g->request.qlink_generation==f->effect[0]&&g->request.qlink_wrapper==f->effect[2]&&g->request.qlink_provider==f->effect[3]&&g->request.qlink_root==f->effect[4];
  if(same&&g->wait_tick&&f->qlink_kind)for(unsigned lane=0;lane<COMMAND_LANES;lane++)if(f->qlink_matches[lane]>g->barriers[lane]){g->wait_tick=0;break;}
  if(same&&(g->pending||g->wait_tick))return 0;
  for(unsigned i=0;i<COMMAND_SLOTS;i++){const InputFlight *tx=SURFACE_VALUE(s,input).flights+i;if(tx->flight&&!tx->acknowledged&&command_qlink(tx->field)&&tx->qlink_request.qlink_index==index&&tx->qlink_request.qlink_wrapper==f->effect[2])return 0;}
 }
 if(bank->assignment==BA_EFFECT&&bank->flip){
  if(SURFACE_VALUE(s,input).effects[channel].pending)return 0;
  for(unsigned i=0;i<COMMAND_SLOTS;i++){const InputFlight *tx=SURFACE_VALUE(s,input).flights+i;if(tx->flight&&!tx->acknowledged&&tx->field==EFFECT_PARAMETER&&tx->effect_request.program_owner==f->identity.program_owner&&tx->effect_request.effect_slot==f->effect[1]&&tx->effect_request.effect_index==f->effect[2])return 0;}
 }
 return !input_blocked_field(&SURFACE_VALUE(s,input),channel,bank_field(bank,channel))&&bank_due(bank,channel,now)>=0&&motor_follow_due(&SURFACE_VALUE(s,following).motors[channel],now,SURFACE_VALUE(s,following).enabled&&!stopping)>=0;
}
/* Called again after the production loop's immediate source/USB revalidation. */
static int strip_motor_output(Surface *s,MirrorBank *bank,unsigned channel,uint32_t now){
 if(!strip_motor_ready(s,bank,channel,now))return 0;
 int position=motor_follow_due(&SURFACE_VALUE(s,following).motors[channel],now,SURFACE_VALUE(s,following).enabled&&!stopping);if(position<0)return 0;
 snd_seq_event_t event;motor_event(&event,s,channel,position);
 if(!channel_send(s,&event))return -1;
 motor_follow_sent(&SURFACE_VALUE(s,following).motors[channel],position,now);
 MirrorFader *f=bank->faders+channel;f->last_sent=position;f->last_tick=now;
 /* A command invalidates the preceding physical observation. */
 f->physical=-1;return 1;
}
static int parent_led_clear(Surface *s){
 for(unsigned i=0;i<8;i++)if(!s->wire_valid[i]||s->wire[i].led[3]){
  snd_seq_event_t e;snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,24+i,0);if(!channel_send(s,&e))return 0;s->wire[i].led[3]=0;
 }
 return 1;
}
/* Each Send strip is a physical encoder/fader pair. Six readable characters
 * leave the normal LCD separator; neither Name/Value nor edits swap rows. */
static void send_pair_text(unsigned char out[7],const CopiedMirror *snapshot,const CopiedTrack *t,unsigned field,unsigned slot,int ret,int names){
 char text[24];MIRROR_FIELD(volume);const CopiedField *f=t?copied_field(snapshot,t,field,&volume):NULL;
 float value=0;if(f)memcpy(&value,&f->bits,4);
 if(!t||!mixable(t->vptr)||!f||!f->available||!isfinite(value)||value<0||value>1)snprintf(text,sizeof(text),"%c%u ---",ret?'R':'S',slot+1);
 else if(names)snprintf(text,sizeof(text),"%s%u",ret?"Rtn":"Send",slot+1);
 else if(!ret){if(value>=0.995f)snprintf(text,sizeof(text),"S%u100%%",slot+1);else snprintf(text,sizeof(text),"S%u %2.0f%%",slot+1,value*100.0f);}
 else{
  float db=value>0?20.0f*log10f(value*value*1.9952623844146729f):-INFINITY;
  if(db<=-96)snprintf(text,sizeof(text),"R%u-INF",slot+1);
  else if(db<=-9.95f)snprintf(text,sizeof(text),"R%u%.0f",slot+1,db);
  else snprintf(text,sizeof(text),"R%u%+.1f",slot+1,fabsf(db)<0.05f?0:db);
 }
 channel_ascii(out,text,strlen(text));
}
#include "novation-runtime.inc"
#include "control-program-runtime.inc"
static int hui_send_wire(Surface *surface,HuiWire wire){
 if(wire.length!=3&&wire.length!=6)return 0;
 for(unsigned at=0;at<wire.length;at+=3){snd_seq_event_t event;snd_seq_ev_clear(&event);
  if(wire.bytes[at]==0xb0)snd_seq_ev_set_controller(&event,0,wire.bytes[at+1],wire.bytes[at+2]);
  else if(wire.bytes[at]==0x90)snd_seq_ev_set_noteon(&event,0,wire.bytes[at+1],wire.bytes[at+2]);
  else return 0;
  if(!channel_send(surface,&event))return 0;
 }
 return 1;
}
static int hui_channel_output(Surface *surface,MirrorBank *bank,const CopiedMirror *snapshot,uint32_t now){
 if(!surface->hui_heartbeat_valid||now<surface->hui_heartbeat_tick||now-surface->hui_heartbeat_tick>=HUI_HEARTBEAT_INTERVAL_MS){
  if(!hui_send_wire(surface,hui_heartbeat()))return 0;
  surface->hui_heartbeat_tick=now;surface->hui_heartbeat_valid=1;
 }
 if(SURFACE_VALUE(surface,controller_hui_policy)==MCU_HUI_MOTORIZED)for(unsigned strip=0;strip<HUI_STRIPS;strip++){
  int position=bank_due(bank,strip,now);if(position<0)continue;
  if(!hui_send_wire(surface,hui_fader(strip,(unsigned)position)))return 0;
  bank->faders[strip].last_sent=position;bank->faders[strip].last_tick=now;bank->faders[strip].physical=-1;
 }
 static const unsigned ports[4]={HUI_BUTTON_ARM,HUI_BUTTON_SOLO,HUI_BUTTON_MUTE,HUI_BUTTON_SELECT};
 /* HUI uses the same authoritative pan projection as MCU, without MCU's
  * separate centre LED bit. Cache the binding as well as its visible value:
  * a fresh bank/property must reassert feedback even at an unchanged pan. */
 for(unsigned strip=0;strip<HUI_STRIPS;strip++){
  const CopiedTrack *track=bank->ready&&!bank->chooser?bank_strip_track(snapshot,bank,strip):NULL;
  ChannelWire wire=channel_wire(snapshot,track,CF_VOLUME);
  unsigned char position=wire.ring&0x3f;
  MotorIdentity identity=track?bank_identity(snapshot,track):(MotorIdentity){0};
  uint32_t incarnation=track?track->fields[CF_PAN].incarnation:0;
  if(!surface->hui_ring_valid[strip]||surface->hui_ring_position[strip]!=position||
     surface->hui_ring_incarnation[strip]!=incarnation||memcmp(surface->hui_ring_identity+strip,&identity,sizeof(identity))){
   if(!hui_send_wire(surface,hui_ring(strip,position)))return 0;
   surface->hui_ring_position[strip]=position;surface->hui_ring_identity[strip]=identity;
   surface->hui_ring_incarnation[strip]=incarnation;surface->hui_ring_valid[strip]=1;
  }
  if(!bank->ready&&!bank->chooser)continue; /* retain unavailable-source LED policy */
  unsigned char values[4]={0};for(unsigned i=0;i<4;i++)values[i]=wire.led[i]!=0;
  for(unsigned i=0;i<4;i++)if(!surface->hui_led_valid[strip]||surface->hui_led[strip][i]!=values[i]){
   if(!hui_send_wire(surface,hui_led(strip,ports[i],values[i])))return 0;
   surface->hui_led[strip][i]=values[i];
  }
  surface->hui_led_valid[strip]=1;
 }
 if(!bank->ready&&!bank->chooser)return 1;
 unsigned char transport[3]={0};
 if(bank->ready&&!bank->chooser){
  if(snapshot->playing.available){transport[0]=!snapshot->playing.bits;transport[1]=snapshot->playing.bits!=0;}
  if(snapshot->record_mode.available&&(snapshot->record_mode.bits==3||snapshot->record_mode.bits==1))transport[2]=1;
 }
 static const unsigned transport_ports[3]={HUI_TRANSPORT_STOP,HUI_TRANSPORT_PLAY,HUI_TRANSPORT_RECORD};
 for(unsigned i=0;i<3;i++)if(!surface->hui_transport_valid||surface->hui_transport[i]!=transport[i]){
  if(!hui_send_wire(surface,hui_led(0x0e,transport_ports[i],transport[i])))return 0;
  surface->hui_transport[i]=transport[i];
 }
 surface->hui_transport_valid=1;return 1;
}
static int channel_output(Surface *s,MirrorBank *bank,const CopiedMirror *snapshot,uint32_t now){
 if(mcu_profile_program(SURFACE_VALUE(s,controller).profile))return 1; /* Transmit-only CE mapping; no feedback or mode writes. */
 if(mcu_profile_native(SURFACE_VALUE(s,controller).profile))return native_output(s,bank,snapshot,now);
 if(SURFACE_VALUE(s,controller).profile->protocol==MCU_PROTOCOL_HUI)return hui_channel_output(s,bank,snapshot,now);
 if(bank->assignment!=BA_QLINK||bank->chooser||!bank->ready){SURFACE_VALUE(s,mode_notice_until)=SURFACE_VALUE(s,mode_notice_controller)=SURFACE_VALUE(s,mode_notice_generation)=SURFACE_VALUE(s,mode_notice_epoch)=0;}
 if(bank->chooser){
  if(!s->chooser_cleared){s->master_sent=-1;s->master_tick=bank->heartbeat;}
  if(!chooser_output(s))return 0;
  uint32_t key[16]={0};key[15]=2;
  if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_MASTER_MOTOR)&&!motor_output(s,8,key,0,now,!s->master_touch&&bank->heartbeat>s->master_tick&&bank->heartbeat>SURFACE_VALUE(s,following).barrier))return 0;
  return 1;
 }
 s->chooser_cleared=0;
 if(SURFACE_VALUE(s,controller).profile==&mcu_profile_xtouch_mini)return mini_output(s,bank,snapshot,now);
 if(!meter_output(s,bank,snapshot))return 0;
 if(!bank->ready){
  if((bank->view==BV_DRUM_PADS||bank->assignment==BA_SEND)&&!parent_led_clear(s))return 0;
  for(unsigned i=0;i<9;i++)SURFACE_VALUE(s,following).motors[i].bound=0;
  return 1;
 } /* unavailable topology is not an empty bank */
 snd_seq_event_t global;
 /* channel_position reads exactly these four words, so an unchanged source
  * produces the identical ten characters: skip formatting and comparing them.
  * The beats LED depends on the same availability word and is skipped with
  * them; mode_led[113]/[114] have no other writer while position_valid holds. */
 if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_TIME)&&(!s->position_valid||s->position_source!=snapshot->position_available||s->position_bar!=snapshot->bar||s->position_beat!=snapshot->beat||s->position_clock!=snapshot->clock)){
  unsigned char position[10];channel_position(position,snapshot);
  for(unsigned i=0;i<10;i++)if(!s->position_valid||position[i]!=s->position_text[i]){snd_seq_ev_clear(&global);snd_seq_ev_set_controller(&global,0,0x49-i,position[i]);if(!channel_send(s,&global))return 0;}
  unsigned beats=snapshot->position_available?127:0;
  if(!s->position_valid||s->mode_led[114]!=beats){for(unsigned note=113;note<=114;note++){snd_seq_ev_clear(&global);snd_seq_ev_set_noteon(&global,0,note,note==114?beats:0);if(!channel_send(s,&global))return 0;}s->mode_led[113]=0;s->mode_led[114]=beats;}
  memcpy(s->position_text,position,10);s->position_valid=1;
  s->position_source=snapshot->position_available;s->position_bar=snapshot->bar;s->position_beat=snapshot->beat;s->position_clock=snapshot->clock;
 }
 unsigned notes[]={40,41,42,43,44,50,51,62,63,64,65,66,67,68,69};
 unsigned modes[]={bank->assignment==BA_TRACK,bank->assignment==BA_SEND,bank->assignment==BA_PAN,bank->assignment==BA_EFFECT,bank->assignment==BA_QLINK,bank->flip,bank->view==BV_ALL,bank->view==BV_MIDI,bank->view==BV_INPUT||bank->assignment==BA_IO,bank->view==BV_AUDIO,bank->view==BV_INSTRUMENT,bank->view==BV_RETURN||bank->view==BV_DRUM_PADS,bank->view==BV_SUBMIX,bank->view==BV_OUTPUT,bank->view==BV_ALL};
 if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_GLOBAL))for(unsigned i=0;i<sizeof(notes)/sizeof(notes[0]);i++)if(!s->mode_valid||s->mode_led[notes[i]]!=modes[i]){snd_seq_ev_clear(&global);snd_seq_ev_set_noteon(&global,0,notes[i],modes[i]?127:0);if(!channel_send(s,&global))return 0;s->mode_led[notes[i]]=modes[i];}
 s->mode_valid=1;
 if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_GLOBAL)){
  if(snapshot->playing.available){unsigned playing=snapshot->playing.bits;if(!s->playing_valid||s->playing_value!=playing){for(unsigned i=0;i<2;i++){snd_seq_ev_clear(&global);snd_seq_ev_set_noteon(&global,0,93+i,(i?playing:!playing)?127:0);if(!channel_send(s,&global))return 0;}s->playing_valid=1;s->playing_value=playing;}}
  else if(!s->playing_valid||s->playing_value!=2){for(unsigned i=0;i<2;i++){snd_seq_ev_clear(&global);snd_seq_ev_set_noteon(&global,0,93+i,0);if(!channel_send(s,&global))return 0;}s->playing_valid=1;s->playing_value=2;}
 }
 unsigned auto_led=snapshot->automation_detail_available&&!snapshot->automation_mixed?127:1;
 unsigned record=0;
 if(snapshot->record_mode.available){
  if(snapshot->record_mode.bits==3)record=1;
  else if(snapshot->record_mode.bits==1&&snapshot->playing.available)record=snapshot->playing.bits?127:1;
 }
 unsigned state_notes[8]={74,75,79,86,95,89,100,101},state_values[8]={snapshot->automation.available&&snapshot->automation.bits==1?auto_led:0,snapshot->automation.available&&snapshot->automation.bits==2?auto_led:0,snapshot->automation.available&&snapshot->automation.bits==0?auto_led:0,snapshot->loop.available&&snapshot->loop.bits?127:0,record,snapshot->click.available&&snapshot->click.bits?127:0,s->zoom_mode&&!s->data_wheel?127:0,s->data_wheel?0:127};
 if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_GLOBAL))for(unsigned i=0;i<8;i++){if(i>=6&&!mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_JOG))continue;if(!s->mode_valid||s->mode_led[state_notes[i]]!=state_values[i]||!s->state_valid[i]){snd_seq_ev_clear(&global);snd_seq_ev_set_noteon(&global,0,state_notes[i],state_values[i]);if(!channel_send(s,&global))return 0;s->mode_led[state_notes[i]]=state_values[i];s->state_valid[i]=1;}}
 /* Master input/feedback belongs to the distinct rooted MpcMixer owner.
  * Pending native source settlement inhibits only its motor. */
 const CopiedField *master=&snapshot->master;
 if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_MASTER_MOTOR)){
 if(s->master_owner!=master->owner_incarnation||s->master_incarnation!=master->incarnation){s->master_owner=master->owner_incarnation;s->master_incarnation=master->incarnation;s->master_sent=-1;}
 if(master->available){float v;memcpy(&v,&master->bits,4);if(isfinite(v)&&v>=0&&v<=1){uint32_t key[16]={0};key[0]=snapshot->epoch;key[1]=master->owner_incarnation;key[2]=master->incarnation;key[15]=1;
  if(!motor_output(s,8,key,(int)(v*16383.0f+0.5f),now,!s->master_touch&&!input_master_blocked(&SURFACE_VALUE(s,input))&&bank->heartbeat>SURFACE_VALUE(s,following).barrier))return 0;
 }}else SURFACE_VALUE(s,following).motors[8].bound=0;
 }else SURFACE_VALUE(s,following).motors[8].bound=0;

 if(bank->assignment!=BA_EFFECT){SURFACE_VALUE(s,effect_notice_until)=0;SURFACE_VALUE(s,effect_notice_slot)=EFFECT_LIST;}
 unsigned strip=s->next_wire++%8;const CopiedTrack *t=input_track(snapshot,&bank->strips[strip]);
 ChannelWire w=channel_wire(snapshot,t,s->display_field[strip]);
 if(bank->view==BV_DRUM_PADS&&!t){
  if(bank->offset+strip<snapshot->pad_count){char label[8];unsigned index=bank->offset+strip;snprintf(label,sizeof(label),"%c%02u",'A'+index/16,index%16+1);channel_ascii(w.name,label,strlen(label));channel_ascii(w.value,"------",6);w.color=7;}
 }
 const CopiedTrack *encoder=input_track(snapshot,bank_encoder_identity(bank,strip));
 unsigned ring_field=surface_encoder_field(s,bank,strip);w.ring=0;
 if(encoder&&ring_field<CF_COUNT){MIRROR_FIELD(volume);const CopiedField *rf=copied_field(snapshot,encoder,ring_field,&volume);if(rf&&rf->available&&(ring_field==CF_MIDI_VOLUME?midi_volume(encoder->vptr):mixable(encoder->vptr))){float x;memcpy(&x,&rf->bits,4);w.ring=(unsigned)(x*10.0f+0.5f)+1;if(ring_field==CF_PAN&&x==0.5f)w.ring|=64;}}
 if(bank->assignment==BA_EFFECT){
  const EffectsCopy *e=&snapshot->effects;if(!bank->flip)w.ring=0;memset(w.name,' ',7);memset(w.value,' ',7);
  int current=snapshot->effects_available&&e->serial==bank->selected.serial&&e->slot==bank->effects_slot&&e->page==bank->effects_page;
  if(bank->effects_slot==EFFECT_LIST){
   if(strip<4){char label[8];const EffectSlot *slot=e->slots+strip;const char *enabled=current&&slot->enable_valid&&slot->enable_tick<=now&&now-slot->enable_tick<EFFECT_FRESH_MS?(slot->enable_bits?"On":"Off"):"---";snprintf(label,sizeof(label),"%u %s",strip+1,enabled);channel_ascii(w.name,label,strlen(label));const char *name=current&&e->slots[strip].status==EF_READY?e->slots[strip].name:current&&e->slots[strip].status==EF_EMPTY?"Empty":e->serial==bank->selected.serial&&e->status>=EF_UNSUPPORTED?"N/A":"Wait";channel_label(w.value,name,strlen(name));}
   else if(strip==6){channel_ascii(w.name,"Select",6);channel_ascii(w.value,"Push",4);}
   else if(strip==7){const CopiedTrack *selected=input_track(snapshot,&bank->selected);channel_ascii(w.name,"Source",6);if(selected&&selected->fields[CF_NAME].available)channel_label(w.value,selected->fields[CF_NAME].text,selected->fields[CF_NAME].length);}
  }else if(current&&e->status==EF_READY){const EffectParameter *p=e->parameters+strip;if(p->status==EF_READY&&p->tick<=snapshot->heartbeat&&snapshot->heartbeat-p->tick<EFFECT_FRESH_MS){channel_label(w.name,p->name,strnlen(p->name,EFFECT_TEXT));channel_value(w.value,p->text,strnlen(p->text,EFFECT_TEXT));float value;memcpy(&value,&p->bits,4);if(!bank->flip&&value>=0&&value<=1)w.ring=(unsigned)(value*10+0.5f)+1;}else if(p->status!=EF_EMPTY){channel_ascii(w.name,p->status>=EF_UNSUPPORTED?"N/A":"Wait",p->status>=EF_UNSUPPORTED?3:4);}}
  else if(strip==0){channel_ascii(w.name,"Effect",6);channel_ascii(w.value,e->serial==bank->selected.serial&&e->status>=EF_UNSUPPORTED?"N/A":"Wait",e->serial==bank->selected.serial&&e->status>=EF_UNSUPPORTED?3:4);}
  if(current&&e->status==EF_READY&&e->page_count&&bank->effects_slot<EFFECT_SLOTS){
   if(SURFACE_VALUE(s,effect_notice_epoch)!=e->epoch||SURFACE_VALUE(s,effect_notice_generation)!=e->generation||SURFACE_VALUE(s,effect_notice_serial)!=e->serial||SURFACE_VALUE(s,effect_notice_slot)!=e->slot||SURFACE_VALUE(s,effect_notice_page)!=e->page){SURFACE_VALUE(s,effect_notice_epoch)=e->epoch;SURFACE_VALUE(s,effect_notice_generation)=e->generation;SURFACE_VALUE(s,effect_notice_serial)=e->serial;SURFACE_VALUE(s,effect_notice_slot)=e->slot;SURFACE_VALUE(s,effect_notice_page)=e->page;SURFACE_VALUE(s,effect_notice_until)=now+2000;}
   if(SURFACE_VALUE(s,effect_notice_until)>now&&!(SURFACE_VALUE(s,page_notice_until)>now)&&!(SURFACE_VALUE(s,preference_notice_until)>now)){
    if(strip<6){unsigned length=strnlen(e->page_name,EFFECT_TEXT),start=strip*6;channel_ascii(w.name,start<length?e->page_name+start:"",start<length?length-start:0);}
    else if(strip==6)channel_ascii(w.name,"Page",4);
    else{char page[16];snprintf(page,sizeof(page),"%u/%u",e->page+1,e->page_count);channel_ascii(w.name,page,strlen(page));}
   }
  }else{SURFACE_VALUE(s,effect_notice_until)=0;SURFACE_VALUE(s,effect_notice_slot)=EFFECT_LIST;}
  if(current&&e->chooser.serial==e->serial&&e->chooser.request&&e->chooser.status==EC_OPEN&&strip==6){channel_ascii(w.name,"Choose",6);channel_ascii(w.value,"On MPC",6);}
  w.color=memcmp(w.name,"       ",7)||memcmp(w.value,"       ",7)?7:0;
  /* Track faders/buttons retain their normal targets and temporary feedback. */
  if((now<s->display_until[strip]&&s->display_field[strip]<CF_COUNT)||(!bank->flip&&bank->faders[strip].touch==MT_DOWN)){ChannelWire track=channel_wire(snapshot,t,s->display_field[strip]);memcpy(w.name,track.name,7);memcpy(w.value,track.value,7);w.color=track.color;}
 }
 else if(bank->assignment==BA_IO){
  static const char *labels[]={"Monitr","InPort","InChan","SendTo","AudOut","OutPrt","OutChn","AudIn"};const IOField *f=snapshot->io.fields+strip;
  channel_ascii(w.name,labels[strip],strlen(labels[strip]));memset(w.value,' ',7);w.ring=0;w.color=7;
  if(snapshot->io_available&&snapshot->io.serial==bank->selected.serial&&f->status==IO_READY&&f->tick<=now&&now-f->tick<IO_FRESH_MS){channel_routing(w.value,f->text,strnlen(f->text,IO_TEXT));if(f->choice_count>1)w.ring=1+(10*f->choice_index)/(f->choice_count-1);}
  else{const char *text=snapshot->io_available&&f->status==IO_NOT_APPLICABLE?"--":"N/A";channel_ascii(w.value,text,strlen(text));}
  if((now<s->display_until[strip]&&s->display_field[strip]<CF_COUNT)||bank->faders[strip].touch==MT_DOWN){ChannelWire track=channel_wire(snapshot,t,s->display_field[strip]);memcpy(w.name,track.name,7);memcpy(w.value,track.value,7);w.color=track.color;}
 }
 else if(bank->assignment==BA_QLINK){
  SURFACE_VALUE(s,effect_notice_slot)=EFFECT_LIST;
  const QLinkCopy *q=&snapshot->qlinks;unsigned index=bank->qlink_page*8+strip;
  if(snapshot->qlinks_available&&q->mode_valid&&q->mode_tick<=now&&now-q->mode_tick<QLINK_FRESH_MS){
   if(SURFACE_VALUE(s,mode_notice_controller)!=q->mode_controller||SURFACE_VALUE(s,mode_notice_generation)!=q->mode_generation||SURFACE_VALUE(s,mode_notice_epoch)!=snapshot->epoch||SURFACE_VALUE(s,mode_notice_id)!=q->mode_id){SURFACE_VALUE(s,mode_notice_controller)=q->mode_controller;SURFACE_VALUE(s,mode_notice_generation)=q->mode_generation;SURFACE_VALUE(s,mode_notice_epoch)=snapshot->epoch;SURFACE_VALUE(s,mode_notice_id)=q->mode_id;SURFACE_VALUE(s,mode_notice_until)=now+2000;}
  }else SURFACE_VALUE(s,mode_notice_until)=0;
  memset(w.name,' ',7);memset(w.value,' ',7);if(!bank->flip)w.ring=0;
  const QLinkSlot *p=q->slots+index;
  int current=snapshot->qlinks_available&&q->epoch==snapshot->epoch&&q->status==QL_SAMPLED&&p->sampled&&p->normalized_valid&&p->tick<=now&&now-p->tick<QLINK_FRESH_MS;
  if(current){
   channel_qlink_label(w.name,p->name,strnlen(p->name,QLINK_TEXT),index);channel_value(w.value,p->text,strnlen(p->text,QLINK_TEXT));
   if(!bank->flip){float value;memcpy(&value,&p->bits,4);if(value>=0&&value<=1)w.ring=(unsigned)(value*10+0.5f)+1;}
  }else{char label[8];snprintf(label,sizeof(label),"Q%u",index+1);channel_ascii(w.name,label,strlen(label));channel_ascii(w.value,"Wait",4);}
  w.color=memcmp(w.name,"       ",7)||memcmp(w.value,"       ",7)?7:0;
  if((now<s->display_until[strip]&&s->display_field[strip]<CF_COUNT)||(!bank->flip&&bank->faders[strip].touch==MT_DOWN)){ChannelWire track=channel_wire(snapshot,t,s->display_field[strip]);memcpy(w.name,track.name,7);memcpy(w.value,track.value,7);w.color=track.color;}
 }
 else if(bank->assignment==BA_SEND){
  const CopiedTrack *selected=input_track(snapshot,&bank->selected);
  memset(w.name,' ',7);memset(w.value,' ',7);memset(w.led,0,sizeof(w.led));
  if(strip<4){
   unsigned char send[7],ret[7];
   unsigned names=s->name_value&&now>=s->display_until[strip]&&bank->faders[strip].touch!=MT_DOWN;
   send_pair_text(send,snapshot,selected,CF_SEND1+strip,strip,0,names);
   send_pair_text(ret,snapshot,t,CF_VOLUME,strip,1,names);
   memcpy(w.name,bank->flip?ret:send,7);memcpy(w.value,bank->flip?send:ret,7);
  }else{w.ring=0;if(strip==7){channel_ascii(w.name,"Source",6);if(selected&&selected->fields[CF_NAME].available)channel_label(w.value,selected->fields[CF_NAME].text,selected->fields[CF_NAME].length);else channel_ascii(w.value,"Ch +/-",6);}}
  w.color=selected&&selected->fields[CF_COLOR].available?channel_palette(selected->fields[CF_COLOR].bits):7;
 }else if(s->name_value&&now>=s->display_until[strip]&&bank->faders[strip].touch!=MT_DOWN&&t){
  unsigned field=s->display_field[strip];const char *name=field==CF_PAN?"Pan":field==CF_MUTE?"Mute":field==CF_SOLO?"Solo":field==CF_ARM?"Record":field==CF_SELECTION?"Select":"Volume";
  channel_ascii(w.value,name,strlen(name));
 }
 if(bank->assignment==BA_QLINK&&SURFACE_VALUE(s,mode_notice_until)>now&&strip<2&&!(SURFACE_VALUE(s,page_notice_until)>now)&&!(SURFACE_VALUE(s,preference_notice_until)>now)){
  const char *name=qlink_mode_name(SURFACE_VALUE(s,mode_notice_id));unsigned length=strlen(name),start=strip*6;w.color=7;channel_ascii(w.name,strip?"Mode":"Q-Link",strip?4:6);channel_ascii(w.value,start<length?name+start:"",start<length?length-start:0);
 }
 if(SURFACE_VALUE(s,page_notice_until)){
  if(snapshot->heartbeat<SURFACE_VALUE(s,page_notice_until)){if(strip==0){static const char *labels[]={"Main","Arrange","Clip","Trk Mix","Pad Mix","TrkEdit","SmpEdit","StepSeq"};if(bank->assignment==BA_EFFECT)w.color=7;channel_ascii(w.name,"Open",4);channel_ascii(w.value,labels[SURFACE_VALUE(s,requested_page)],strlen(labels[SURFACE_VALUE(s,requested_page)]));}}
  else{SURFACE_VALUE(s,page_notice_until)=0;memset(s->wire_valid,0,sizeof(s->wire_valid));}
 }
 if(SURFACE_VALUE(s,preference_notice_until)){
  if(snapshot->heartbeat<SURFACE_VALUE(s,preference_notice_until)){if(strip==0){if(bank->assignment==BA_EFFECT)w.color=7;channel_ascii(w.name,"Motors",6);channel_ascii(w.value,SURFACE_VALUE(s,preference_notice_error)?"SaveErr":SURFACE_VALUE(s,following).enabled?"ON":"OFF",SURFACE_VALUE(s,preference_notice_error)?7:SURFACE_VALUE(s,following).enabled?2:3);}}
  else{SURFACE_VALUE(s,preference_notice_until)=0;memset(s->wire_valid,0,sizeof(s->wire_valid));}
 }
 /* Drum Mix and Send Select LEDs identify the source Track in its normal
  * fixed bank, independently of the pad or send/return strip bindings. */
 if(bank->view==BV_DRUM_PADS||bank->assignment==BA_SEND){
  w.led[3]=0;
  if(bank->ready&&snapshot->ready&&snapshot->alive&&!snapshot->error&&snapshot->selection.available&&now>=snapshot->heartbeat&&now-snapshot->heartbeat<1000){
   const CopiedTrack *parent=input_track(snapshot,&bank->selected);unsigned ordinal=0;
   for(unsigned i=0;i<snapshot->count;i++)if(bank_role(BV_TRACK,snapshot->tracks[i].vptr)){
    if(snapshot->tracks+i==parent){w.led[3]=ordinal%8==strip?127:0;break;}ordinal++;
   }
  }
 }
 ChannelWire *old=s->wire+strip;unsigned valid=s->wire_valid[strip];snd_seq_event_t e;unsigned char bytes[15];
 if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_LCD))for(unsigned row=0;row<2;row++){unsigned char *line=row?w.value:w.name,*before=row?old->value:old->name;if(!valid||memcmp(line,before,7)){snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,channel_lcd(bytes,strip,row,line),bytes);if(!channel_send(s,&e))return 0;}}
 for(unsigned i=0;i<4;i++)if(!valid||w.led[i]!=old->led[i]){snd_seq_ev_clear(&e);snd_seq_ev_set_noteon(&e,0,i*8+strip,w.led[i]);if(!channel_send(s,&e))return 0;}
 if(!valid||w.ring!=old->ring){snd_seq_ev_clear(&e);snd_seq_ev_set_controller(&e,0,0x30+strip,w.ring);if(!channel_send(s,&e))return 0;}
 s->colors[strip]=w.color;
 if(mcu_has(&SURFACE_VALUE(s,controller),MCU_CAP_COLOR)&&(!s->colors_valid||!valid||w.color!=old->color)){snd_seq_ev_clear(&e);snd_seq_ev_set_sysex(&e,channel_colors(bytes,s->colors),bytes);if(!channel_send(s,&e))return 0;s->colors_valid=1;}
 *old=w;s->wire_valid[strip]=1;return 1;
}
#endif
static int source_identity(const char *path,int fd,const struct stat *file,const MirrorState *s,unsigned pid,uint64_t start,struct stat *exe){
 struct stat live,opened;
 return !lstat(path,&live)&&!fstat(fd,&opened)&&S_ISREG(live.st_mode)&&same_stat(file,&live)&&same_stat(file,&opened)&&live.st_size==(off_t)sizeof(*s)&&opened.st_size==live.st_size&&s->magic==MIRROR_MAGIC&&s->version==MIRROR_VERSION&&s->bytes==sizeof(*s)&&s->pid==pid&&s->capacity==MIRROR_CELLS&&start_time(pid)==start&&process_file(pid,exe,0);
}
#ifdef MIRROR_INPUT
static int input_file_current(const char *path,const MirrorState *state,uint64_t start){
 struct stat at,fd;
 return input_mapping!=MAP_FAILED&&!lstat(path,&at)&&!fstat(input_fd,&fd)&&S_ISREG(at.st_mode)&&same_stat(&at,&input_file)&&same_stat(&fd,&input_file)&&at.st_size==(off_t)sizeof(CommandState)&&fd.st_size==at.st_size&&input_mapping->magic==COMMAND_MAGIC&&input_mapping->version==COMMAND_VERSION&&input_mapping->bytes==sizeof(CommandState)&&input_mapping->capacity==COMMAND_SLOTS&&command_duration_valid(input_mapping->seconds)&&input_mapping->owner_token&&input_mapping->pid==state->pid&&input_mapping->start_lo==(uint32_t)start&&input_mapping->start_hi==(uint32_t)(start>>32)&&input_mapping->origin_sec==state->origin_sec&&input_mapping->origin_nsec==state->origin_nsec;
}
static int input_open_file(const char *path,const MirrorState *state,uint64_t start){
 input_fd=open(path,O_RDWR|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(input_fd<0||fstat(input_fd,&input_file)||!S_ISREG(input_file.st_mode)||input_file.st_size!=(off_t)sizeof(CommandState)||input_file.st_uid!=geteuid()||flock(input_fd,LOCK_EX|LOCK_NB))return 0;
 input_mapping=mmap(NULL,sizeof(*input_mapping),PROT_READ|PROT_WRITE,MAP_SHARED,input_fd,0);
 return input_file_current(path,state,start)&&input_begin(&input,input_mapping,coordinator_flights);
}
static void input_close_file(void){
 /* Native MPC kernel retains flock through mmap: explicitly unlock. */
 if(input_fd>=0)flock(input_fd,LOCK_UN);
 if(input_mapping!=MAP_FAILED)munmap(input_mapping,sizeof(*input_mapping));
 if(input_fd>=0)close(input_fd);
 input_mapping=MAP_FAILED;input_fd=-1;
}
#endif
/* -1 invalid/stale source, 0 bounded copy retry exhausted, 1 fresh snapshot. */
static int fresh_copy_view(const MirrorState *state,CopiedMirror *snapshot,uint32_t *now,int pads){
 int copied=copy_mirror_view(state,snapshot,pads);
 uint32_t heartbeat=atomic_load_explicit(&state->heartbeat,memory_order_acquire);
 *now=source_now(state); /* clock after publication reads, never before them */
 if(*now==UINT32_MAX||!atomic_load_explicit(&state->alive,memory_order_acquire)||atomic_load_explicit(&state->error,memory_order_acquire)||*now<heartbeat||*now-heartbeat>=1000)return -1;
 if(!copied)return 0;
 return snapshot->alive&&!snapshot->error&&*now>=snapshot->heartbeat&&*now-snapshot->heartbeat<1000?1:-1;
}

static inline int fresh_copy(const MirrorState *state,CopiedMirror *snapshot,uint32_t *now){return fresh_copy_view(state,snapshot,now,0);}

#ifdef MIRROR_INPUT
static void collection_disconnect(SurfaceCollection *all,unsigned i){
 if(!all->surfaces[i].seq)return;
 surface_close(all->surfaces+i,all->banks+i);all->connected--;all->disconnections++;
 collection_admission_publish(all);
}
#include "plugin-controls-bridge.inc"
static void collection_init(SurfaceCollection *all,const McuControllerCollection *saved){
 memset(all,0,sizeof(*all));all->count=saved->count;all->sink=all->source=all->stop_sink=-1;
 command_coordinator.count=1;command_coordinator.origins[0]=&input;input.coordinator=&command_coordinator;
 for(unsigned i=0;i<all->count;i++){
  const McuControllerPreference *record=saved->records+i;SurfaceContext *context=all->contexts+i;
  all->enabled[i]=record->enabled;context->controller=record->input;context->controller_output=record->output;context->controller_hui_policy=record->hui_policy;
  context->following.enabled=following.enabled;motor_follow_reset(&context->following);
  context->servo_disabled=!mcu_has(&record->input,MCU_CAP_STRIP_MOTOR)||(servo_override_requested>=0?(unsigned)servo_override_requested:!record->input.profile->raw_fader_echo);context->effect_notice_slot=EFFECT_LIST;
  context->input.commands=input.commands;context->input.flights=coordinator_flights;context->input.coordinator=&command_coordinator;context->input.origin_bank=all->banks+i;
  bank_init(all->banks+i);all->surfaces[i].context=context;all->surfaces[i].stop_sink=-1;
  if(record->enabled){if(command_coordinator.count==1){context->inherited_holds=inherited_holds;inherited_holds=0;}command_coordinator.origins[command_coordinator.count++]=&context->input;}
 }
 plugin_bridge_init();
}
/* One inventory traversal resolves all configured roles. Names/types remain
 * exact; duplicate logical identities or overlapping routes do not pair. */
static int collection_topology(SurfaceCollection *all,const MirrorState *state){
 snd_seq_addr_t from[MCU_CONTROLLER_CAPACITY]={{0}},to[MCU_CONTROLLER_CAPACITY]={{0}};
 unsigned inputs[MCU_CONTROLLER_CAPACITY]={0},outputs[MCU_CONTROLLER_CAPACITY]={0},valid[MCU_CONTROLLER_CAPACITY]={0};
 snd_seq_client_info_t *ci;snd_seq_port_info_t *pi;snd_seq_client_info_alloca(&ci);snd_seq_port_info_alloca(&pi);snd_seq_client_info_set_client(ci,-1);int rc;
 all->topology_scans++;
 while((rc=snd_seq_query_next_client(all->seq,ci))>=0){
  const char *client=snd_seq_client_info_get_name(ci);unsigned kernel=snd_seq_client_info_get_type(ci)==SND_SEQ_KERNEL_CLIENT,interested=0;
  for(unsigned i=0;i<all->count;i++)if(all->enabled[i])interested|=endpoint_client_matches(&all->contexts[i].controller,client,kernel)||endpoint_client_matches(&all->contexts[i].controller_output,client,kernel);
  if(!interested)continue;
  snd_seq_port_info_set_client(pi,snd_seq_client_info_get_client(ci));snd_seq_port_info_set_port(pi,-1);int pr;
  while((pr=snd_seq_query_next_port(all->seq,pi))>=0){
   all->topology_rows++;unsigned caps=snd_seq_port_info_get_capability(pi),abstract=0;
   if(caps&SND_SEQ_PORT_CAP_READ)abstract|=MCU_ENDPOINT_READ;
   if(caps&SND_SEQ_PORT_CAP_SUBS_READ)abstract|=MCU_ENDPOINT_SUBS_READ;
   if(caps&SND_SEQ_PORT_CAP_WRITE)abstract|=MCU_ENDPOINT_WRITE;
   const char *port=snd_seq_port_info_get_name(pi);
   for(unsigned i=0;i<all->count;i++)if(all->enabled[i]){
    if(mcu_endpoint_match_caps(&all->contexts[i].controller,client,kernel,port,abstract,MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ)){from[i]=*snd_seq_port_info_get_addr(pi);inputs[i]++;}
    if(mcu_endpoint_match_caps(&all->contexts[i].controller_output,client,kernel,port,abstract,MCU_ENDPOINT_WRITE)){to[i]=*snd_seq_port_info_get_addr(pi);outputs[i]++;}
   }
  }if(pr!=-ENOENT)return 0;
 }if(rc!=-ENOENT)return 0;
 for(unsigned i=0;i<all->count;i++)valid[i]=all->enabled[i]&&inputs[i]==1&&outputs[i]==1;
 for(unsigned i=0;i<all->count;i++)if(all->enabled[i]&&inputs[i]==1&&outputs[i]==1)for(unsigned j=i+1;j<all->count;j++)if(all->enabled[j]&&inputs[j]==1&&outputs[j]==1&&(address_equal(from[i],from[j])||address_equal(to[i],to[j]))){valid[i]=valid[j]=0;}
 for(unsigned i=0;i<all->count;i++){
  Surface *surface=all->surfaces+i;
  if(surface->seq&&(!valid[i]||!address_equal(from[i],surface->full)||!address_equal(to[i],surface->output)))collection_disconnect(all,i);
  if(!valid[i]||surface->seq)continue;
  if(snd_seq_connect_from(all->seq,all->sink,from[i].client,from[i].port)<0)continue;
  surface->seq=all->seq;surface->sink=all->sink;surface->source=all->source;surface->stop_sink=-1;surface->shared_connection=1;
  surface->full=from[i];surface->output=to[i];surface->ingress=(snd_seq_addr_t){.client=snd_seq_client_id(all->seq),.port=all->sink};
  surface->hui_connected_tick=source_now(state);hui_input_reset(&surface->hui_input);bank_disconnect(all->banks+i);
  if(mcu_profile_native(surface->context->controller.profile))novation_reset(&surface->novation,surface->context->controller.profile->protocol,++surface->context->connection_generation);
  for(unsigned strip=0;strip<8;strip++)if(surface->context->inherited_holds&(1u<<strip))all->banks[i].faders[strip].touch=MT_DOWN;
  surface->master_touch=(surface->context->inherited_holds>>8)&1;surface->context->inherited_holds=0;
  all->connected++;all->connections++;
  BRIDGE_LOG("CONNECTED controller=%u profile=%s from=%u:%u to=%u:%u\n",i,surface->context->controller.profile->name,from[i].client,from[i].port,to[i].client,to[i].port);
 }
 return 1;
}
static unsigned collection_pads(const SurfaceCollection *all){unsigned pads=0;for(unsigned i=0;i<all->count;i++)if(all->surfaces[i].seq)pads|=all->banks[i].view==BV_DRUM_PADS;return pads;}
static int collection_needs_snapshot(const SurfaceCollection *all){return all->connected||!input_drained(&input);}
static int collection_health(SurfaceCollection *all){
 if(!input_health(&input))return 0;
 if(plugin_input.error){input.error=plugin_input.error;return 0;}
 for(unsigned i=0;i<all->count;i++)if(all->enabled[i]&&all->contexts[i].input.error){input.error=all->contexts[i].input.error;return 0;}
 return 1;
}
static int collection_drain(SurfaceCollection *all,const MirrorState *state,ControllerRuntime *runtime){
 for(unsigned n=0;n<256;n++){
  snd_seq_event_t *event;int rc=snd_seq_event_input(all->seq,&event);if(rc==-EAGAIN)return !lost_events(all->seq);if(rc<0)return 0;
  if(event->source.client==SND_SEQ_CLIENT_SYSTEM&&event->source.port==SND_SEQ_PORT_SYSTEM_ANNOUNCE){
   for(unsigned i=0;i<all->count;i++){
    Surface *s=all->surfaces+i;if(!s->seq)continue;
    if((event->type==SND_SEQ_EVENT_CLIENT_EXIT&&(event->data.addr.client==s->full.client||event->data.addr.client==s->output.client))||
       (event->type==SND_SEQ_EVENT_PORT_EXIT&&(address_equal(event->data.addr,s->full)||address_equal(event->data.addr,s->output)))||
       (event->type==SND_SEQ_EVENT_PORT_UNSUBSCRIBED&&address_equal(event->data.connect.sender,s->full)&&address_equal(event->data.connect.dest,s->ingress)))collection_disconnect(all,i);
   }continue;
  }
  unsigned found=all->count;
  if(stop_adapter_exe&&event->dest.port==all->stop_sink){
   Surface relay={.seq=all->seq,.source=all->source};
   if(!surface_stop_route(&relay,state))return 0;
   if(!address_equal(event->source,relay.stop_adapter)||(event->type!=SND_SEQ_EVENT_NOTEON&&event->type!=SND_SEQ_EVENT_NOTEOFF)||event->data.note.channel||event->data.note.note!=93||event->data.note.velocity>127)continue;
   int copied=runtime_snapshot(runtime,state,collection_pads(all));
   if(copied<0||!surface_stop_relay(&relay,&input,runtime->snapshot,copied&&!atomic_load(&input.commands->new_project_intent),event->type==SND_SEQ_EVENT_NOTEON&&event->data.note.velocity,runtime->now))return 0;
   continue;
  }
  for(unsigned i=0;i<all->count;i++)if(all->surfaces[i].seq&&address_equal(all->surfaces[i].full,event->source)){found=i;break;}
  if(found==all->count){all->unrouted++;continue;}
  all->routed++;all->contexts[found].received++;
  if(!(all->contexts[found].controller.profile->protocol==MCU_PROTOCOL_HUI&&event->type==SND_SEQ_EVENT_NOTEON&&event->data.note.channel==0&&event->data.note.note==0&&event->data.note.velocity==0x7f))all->focus=found;
  if(!surface_consume(all->surfaces+found,all->banks+found,state,0,runtime,event))collection_disconnect(all,found);
 }
 /* Budget exhaustion leaves queued work for the next shared cycle. */
 return !lost_events(all->seq);
}
static MirrorBank *collection_interest_view(SurfaceCollection *all,unsigned assignment){
 if(all->focus<all->count&&all->surfaces[all->focus].seq&&all->banks[all->focus].assignment==assignment)return all->banks+all->focus;
 for(unsigned i=0;i<all->count;i++)if(all->surfaces[i].seq&&all->banks[i].assignment==assignment)return all->banks+i;
 return NULL;
}
static void collection_clip_interest(SurfaceCollection *all,const CopiedMirror *snapshot,uint32_t now){
 ClipInterest *d=&input.commands->clip_interest;unsigned mask=0,column[CLIP_VIEWS]={0},row[CLIP_VIEWS]={0};
 for(unsigned i=0;i<all->count;i++){Surface *surface=all->surfaces+i;NovationState *n=&surface->novation;
  if(!surface->seq||n->layer!=NOV_CLIPS||!novation_admitted(n)||!snapshot->ready)continue;
  if(n->clip_epoch!=snapshot->epoch){n->clip_epoch=snapshot->epoch;n->clip_column=0;n->clip_row=0;}
  mask|=1u<<i;column[i]=n->clip_column;row[i]=n->clip_row;
 }
 unsigned changed=atomic_load(&d->enabled)!=mask||atomic_load(&d->epoch)!=snapshot->epoch;
 for(unsigned i=0;i<CLIP_VIEWS;i++)changed|=atomic_load(d->column+i)!=column[i]||atomic_load(d->row+i)!=row[i];
 unsigned until=atomic_load(&d->until);if(!changed&&(!mask||(until>now&&until-now>500)))return;
 if(!command_writer_enter(input.commands))return;
 unsigned rev=atomic_load(&d->revision);if(rev>UINT32_MAX-3){atomic_store(&d->until,0);command_writer_leave(input.commands);return;}rev&=~1u;
 atomic_store_explicit(&d->revision,rev+1,memory_order_release);atomic_store(&d->enabled,mask);atomic_store(&d->epoch,snapshot->epoch);
 for(unsigned i=0;i<CLIP_VIEWS;i++){atomic_store(d->column+i,column[i]);atomic_store(d->row+i,row[i]);}
 atomic_store(&d->until,mask&&now<=UINT32_MAX-1000?now+1000:0);atomic_store_explicit(&d->revision,rev+2,memory_order_release);command_writer_leave(input.commands);
}
static void collection_step_interest(SurfaceCollection *all,const CopiedMirror *snapshot,uint32_t now){
 StepInterest *d=&input.commands->step_interest;unsigned mask=0,pad[STEP_VIEWS]={0},first[STEP_VIEWS]={0},bind[STEP_VIEWS]={0};
 for(unsigned i=0;i<all->count;i++){Surface *surface=all->surfaces+i;NovationState *n=&surface->novation;
  if(!surface->seq||!novation_step_view(n->layer)||!novation_admitted(n)||!snapshot->ready)continue;
  if(n->step_epoch!=snapshot->epoch){n->step_epoch=snapshot->epoch;n->step_program=snapshot->steps_available?snapshot->steps.program_owner:0;n->step_pad=UINT32_MAX;n->step_first=0;}
  const StepProjection *v=native_step_source(surface,snapshot,now);if(n->step_pad==UINT32_MAX&&v)n->step_pad=v->pad;
  mask|=1u<<i;pad[i]=n->step_pad;first[i]=n->step_first;bind[i]=n->step_bind_generation;
 }
 unsigned changed=atomic_load(&d->enabled)!=mask||atomic_load(&d->epoch)!=snapshot->epoch;
 for(unsigned i=0;i<STEP_VIEWS;i++)changed|=atomic_load(d->pad+i)!=pad[i]||atomic_load(d->first_step+i)!=first[i]||atomic_load(d->bind_generation+i)!=bind[i];
 unsigned until=atomic_load(&d->until);if(!changed&&(!mask||(until>now&&until-now>500)))return;
 if(!command_writer_enter(input.commands))return;
 unsigned rev=atomic_load(&d->revision);if(rev>UINT32_MAX-3){atomic_store(&d->until,0);command_writer_leave(input.commands);return;}rev&=~1u;
 atomic_store_explicit(&d->revision,rev+1,memory_order_release);atomic_store(&d->enabled,mask);atomic_store(&d->epoch,snapshot->epoch);
 for(unsigned i=0;i<STEP_VIEWS;i++){atomic_store(d->pad+i,pad[i]);atomic_store(d->first_step+i,first[i]);atomic_store(d->bind_generation+i,bind[i]);}
 atomic_store(&d->until,mask&&now<=UINT32_MAX-1000?now+1000:0);atomic_store_explicit(&d->revision,rev+2,memory_order_release);command_writer_leave(input.commands);
}
static void collection_interests(SurfaceCollection *all,const CopiedMirror *snapshot,uint32_t now){
 unsigned focus=all->focus;
 if(focus>=all->count||!all->surfaces[focus].seq){focus=0;while(focus<all->count&&!all->surfaces[focus].seq)focus++;}
 MirrorBank empty;bank_init(&empty);
 unsigned connected=focus<all->count;
 all->interest_visits++;all->interest_active=connected;
 collection_step_interest(all,snapshot,now);collection_clip_interest(all,snapshot,now);
 /* Meters belong to a metered surface: input on a Launchpad or XL3 (focus)
  * must not withdraw the X-Touch meter demand. */
 unsigned metered=connected&&mcu_has(&all->contexts[focus].controller,MCU_CAP_METERS)?focus:all->count;
 for(unsigned i=0;metered==all->count&&i<all->count;i++)if(all->surfaces[i].seq&&mcu_has(&all->contexts[i].controller,MCU_CAP_METERS))metered=i;
 input_meter_interest(&input,metered<all->count?all->banks+metered:&empty,snapshot,now,metered<all->count);
 /* Each selected-context family has one shared demand. Interaction chooses
  * among competing pages, but a peer's ordinary mixer use must not revoke an
  * otherwise visible effects/Q-Link/I/O view. */
 MirrorBank *io=collection_interest_view(all,BA_IO),*effects=collection_interest_view(all,BA_EFFECT),*qlinks=collection_interest_view(all,BA_QLINK);
 effects=plugin_bridge_interest(snapshot,now,effects);
 input_io_interest(&input,io?io:&empty,snapshot,now,io!=NULL);input_effects_interest(&input,effects?effects:&empty,snapshot,now,effects!=NULL);input_qlink_interest(&input,qlinks?qlinks:&empty,snapshot,now,qlinks!=NULL);
}
/* Shared source is already copied. There is one settlement scan and one
 * interest visit regardless of the number of connected controller contexts. */
static int collection_cycle(SurfaceCollection *all,const MirrorState *state,ControllerRuntime *runtime){
 CopiedMirror *snapshot=runtime->snapshot;uint32_t now=runtime->now;
 for(unsigned i=0;i<all->count;i++)if(all->surfaces[i].seq){
  SurfaceContext *context=all->contexts+i;
  if(context->following.enabled!=following.enabled){context->following.enabled=following.enabled;context->following.barrier=now;for(unsigned strip=0;strip<9;strip++)context->following.motors[strip].last_sent=-1;}
  if(all->banks[i].view==BV_DRUM_PADS&&!runtime->snapshot_pads)continue;
  bank_apply(all->banks+i,snapshot,now);input_sync(&context->input,all->banks+i);hui_pickup_sync(all->surfaces+i,&context->input,all->banks+i,now);
 }
 runtime_settle(runtime,&input,all->count?all->banks:NULL);
 if(!collection_health(all))return 0;
 collection_interests(all,snapshot,now);
 input_effect_submit(&plugin_input,&plugin_bank,snapshot,now);
 for(unsigned step=0;step<all->count;step++){unsigned i=(all->submit_cursor+step)%all->count;if(all->surfaces[i].seq&&(all->banks[i].view!=BV_DRUM_PADS||runtime->snapshot_pads)){runtime->submission_visits++;input_submit(&all->contexts[i].input,all->banks+i,snapshot,now);}}
 /* When the shared mailbox fills, rotate first admission next cycle so a
  * continuously busy lower-index origin cannot starve its peers. */
 if(all->count)all->submit_cursor=(all->submit_cursor+1)%all->count;
 /* The optional mapping adapter's relay uses the same coordinator and never
  * creates another snapshot or command writer. */
 if(stop_adapter_exe){runtime->submission_visits++;input_submit(&input,all->banks,snapshot,now);}
 unsigned navigation=0;for(unsigned i=0;i<command_coordinator.count;i++){MirrorInput *origin=command_coordinator.origins[i];navigation|=origin->jog_count||origin->jog_rewind||origin->jog_forward;}
 if(!navigation&&atomic_load(&input.commands->navigation_watch)&&input_jog_admit(&input,snapshot))(void)command_navigation_watch(input.commands,0);
 if(!collection_health(all))return 0;
 /* A bounded drain may leave another touch packet queued. Do not project any
  * actuator output (including master/HUI) until that shared ingress is empty. */
 if(snd_seq_event_input_pending(all->seq,1)>0)return 1;
 for(unsigned i=0;i<all->count;i++){
  Surface *surface=all->surfaces+i;MirrorBank *bank=all->banks+i;if(!surface->seq||(bank->view==BV_DRUM_PADS&&!runtime->snapshot_pads))continue;
  if(!hui_liveness_current(surface,now)||!channel_output(surface,bank,snapshot,now)){collection_disconnect(all,i);continue;}
  for(unsigned strip=0;strip<MIRROR_BANK;strip++)if(strip_motor_ready(surface,bank,strip,now)){
   /* Input was drained once for the whole collection, not once per motor.
    * Never emit while another queued touch/input batch remains. */
   if(snd_seq_event_input_pending(all->seq,1)>0)break;
   uint32_t current=source_now(state);if(!mirror_motor_current(state,snapshot,bank,strip,current))continue;
   int emitted=strip_motor_output(surface,bank,strip,current);if(emitted<0){collection_disconnect(all,i);break;}all->motor_messages+=emitted>0;
  }
 }
 return 1;
}
#endif

int main(int argc,char **argv){
#ifdef MIRROR_INPUT
 if(argc==2&&!strcmp(argv[1],"--list-midi"))return list_midi_endpoints();
 if(argc==3&&!strcmp(argv[1],"--native-preferences-watch"))return native_preferences_watch(argv[2]);
 if(argc==5&&!strcmp(argv[1],"--native-preferences-sync"))return native_preferences_sync(argv[2],argv[3],argv[4]);
 if(argc==4&&!strcmp(argv[1],"--native-preferences-apply"))return native_preferences_apply(argv[2],argv[3]);
 if((argc==3||argc==4)&&!strcmp(argv[1],"--native-preferences-invalidate")){uint64_t expected=0;if(argc==4&&!number(argv[3],UINT32_MAX,&expected))return 2;return native_preferences_invalidate(argv[2],(uint32_t)expected);}
 if(argc==4&&!strcmp(argv[1],"--native-preferences-finish"))return native_preferences_finish(argv[2],!strcmp(argv[3],"active"));
 if(argc==5&&!strcmp(argv[1],"--controller-restore")){
  if(strcmp(argv[2],"/data/mpclearn/controller-preferences")||strcmp(argv[3],"/run/mpclearn/controller-preferences.rollback")||(strcmp(argv[4],"present")&&strcmp(argv[4],"missing")))return 2;
  return mcu_controller_preferences_restore(argv[2],argv[3],!strcmp(argv[4],"missing"))?0:1;
 }
 if(argc==3&&!strcmp(argv[1],"--surface-status")){unsigned enabled;if(!surface_preferences_read(argv[2],&enabled))return 1;puts(enabled?"on":"off");return 0;}
 if(argc==3&&!strcmp(argv[1],"--controller-status")){
  McuControllerCollection collection;if(!mcu_controller_preferences_read_collection(argv[2],&collection))return 1;
  unsigned selected=controller_first_enabled(&collection);if(selected==NATIVE_CONTROLLER_NEW){printf("controllers=%u enabled=0 profile=none\n",collection.count);return 0;}
  McuControllerPreference selection;if(!mcu_controller_preference_copy(&selection,collection.records+selected))return 1;
  printf("profile=%s endpoint-type=%s",selection.endpoint.profile->name,endpoint_type_name(selection.endpoint.physical_only));
  printf(" client=%s port=%s",selection.endpoint.client,selection.endpoint.port);
  if(selection.endpoint.profile==&mcu_profile_hui)printf(" hui-mode=%s",selection.hui_policy==MCU_HUI_MOTORIZED?"motorized":"pickup");
  printf(" to-endpoint-type=%s",endpoint_type_name(selection.output.physical_only));
  printf(" to-client=%s to-port=%s",selection.output.client,selection.output.port);
  putchar('\n');return 0;
 }
 if(argc==3&&!strcmp(argv[1],"--controller-any-enabled")){
  McuControllerCollection collection;if(!mcu_controller_preferences_read_collection(argv[2],&collection))return 2;
  return controller_first_enabled(&collection)==NATIVE_CONTROLLER_NEW?3:0;
 }
 if(argc>=4&&!strcmp(argv[1],"--controller-set")){
  ControllerOptions options={.profile="xtouch",.input_type=MCU_ENDPOINT_KERNEL,.output_type=MCU_ENDPOINT_KERNEL};
  for(int i=3;i<argc;i++)if(!controller_option(&options,argv[i])||options.preferences)return 2;
  McuEndpoint input_endpoint,output_endpoint;unsigned hui_policy;
  if(!options.profile_set||!controller_resolve(&options,&input_endpoint,&output_endpoint,&hui_policy))return 2;
  return mcu_controller_preferences_write(argv[2],&input_endpoint,&output_endpoint,hui_policy)?0:1;
 }
#endif
 setvbuf(stdout,NULL,_IOLBF,0);uint64_t seconds=120;
#ifdef MIRROR_INPUT
 int arguments=servo_arguments(argc,argv);
 if((arguments!=3&&arguments!=4)||argv[1][0]!='/'||argv[2][0]!='/'||(arguments==4&&strcmp(argv[3],"manual")&&!number(argv[3],900,&seconds))){fputs("mirror-input /absolute/volume.state /absolute/command.state [SECONDS(1..900), default120, or manual] [--profile=xtouch|xtouch-mini|generic|hui|lcxl3-daw|launchpad-x-programmer|launchpad-pro-mk3-programmer|mpk-mini-mk3|lpd8-mk2|mpk249] [--endpoint-client=NAME --endpoint-port=NAME --endpoint-type=kernel|user|any | --input-client=NAME --input-port=NAME --input-type=kernel|user|any --output-client=NAME --output-port=NAME --output-type=kernel|user|any] [--hui-mode=pickup|motorized] [--controller-preferences=/absolute/path] [--servo=off|on] [--verbose]\nmirror-input --list-midi\nmirror-input --controller-status /absolute/controller-preferences\nmirror-input --controller-set /absolute/controller-preferences --profile=xtouch|xtouch-mini|generic|hui|lcxl3-daw|launchpad-x-programmer|launchpad-pro-mk3-programmer|mpk-mini-mk3|lpd8-mk2|mpk249 [endpoint options]\n",stderr);return 2;}
 if(arguments==4&&!strcmp(argv[3],"manual"))seconds=0;
 if(preferences_path){unsigned enabled;if(!surface_preferences_read(preferences_path,&enabled)||enabled!=following.enabled){fputs("Invalid or changed surface preference; no motors started\n",stderr);return 1;}}
 motor_follow_reset(&following);
#else
 if((argc!=2&&argc!=3)||argv[1][0]!='/'||(argc==3&&!number(argv[2],900,&seconds))){fputs("mirror-motor /absolute/volume.state [SECONDS(1..900), default120]\n",stderr);return 2;}
#endif
 unsigned pid=0;uint64_t start=0;
 snd_seq_t *discovery=NULL;
#ifndef MIRROR_INPUT
 int disconnected_idle=0;
#endif
 int result=1,fd=-1,lockfd=-1;const MirrorState *state=MAP_FAILED;Surface surface={0};MirrorBank bank;bank_init(&bank);const char *reason="setup failure";
#ifndef MIRROR_INPUT
 unsigned sent=0;
#endif
#ifndef MIRROR_INPUT
 ControllerRuntime *active_runtime=NULL;
#endif
#ifdef MIRROR_INPUT
 ControllerRuntime runtime={0};
#endif
 signal(SIGINT,signal_stop);signal(SIGTERM,signal_stop);signal(SIGALRM,signal_stop);if(seconds)alarm((unsigned)seconds+15);
 /* Share the proven one-motor bridge's abstract exclusion key: the old
  * diagnostic and this bank cannot drive the same hardware concurrently. */
 lockfd=socket(AF_UNIX,SOCK_DGRAM|SOCK_CLOEXEC,0);struct sockaddr_un lock={.sun_family=AF_UNIX};memcpy(lock.sun_path+1,"mpclearn-motor-one",18);
 if(lockfd<0||bind(lockfd,(struct sockaddr*)&lock,sizeof(sa_family_t)+19))goto done;
 fd=open(argv[1],O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);struct stat file,exe;
 if(fd<0||fstat(fd,&file)||!S_ISREG(file.st_mode)||file.st_size!=(off_t)sizeof(MirrorState))goto done;
 state=mmap(NULL,sizeof(*state),PROT_READ,MAP_SHARED,fd,0);if(state==MAP_FAILED)goto done;
 if(state->magic!=MIRROR_MAGIC||state->version!=MIRROR_VERSION||state->bytes!=sizeof(*state)||state->capacity!=MIRROR_CELLS||!state->pid||state->pid>INT_MAX||state->origin_nsec>=1000000000)goto done;
 pid=state->pid;start=start_time(pid);long hz=sysconf(_SC_CLK_TCK);
 uint32_t began_sec=state->origin_sec,began_nsec=state->origin_nsec;
 if(!start||hz<=0||(uint64_t)began_sec*(unsigned long)hz+(uint64_t)began_nsec*(unsigned long)hz/1000000000<start||!process_file(pid,&exe,1)||!source_identity(argv[1],fd,&file,state,pid,start,&exe))goto done;
 MIRROR_COPY(snapshot);uint32_t now;
#ifdef MIRROR_INPUT
 runtime.snapshot=&snapshot;
#endif
 if(fresh_copy_view(state,&snapshot,&now,bank.view==BV_DRUM_PADS)<0){reason="mirror unavailable or stale at start";goto done;}
#ifdef MIRROR_INPUT
 if(!input_open_file(argv[2],state,start)){reason=input.error==C_BUSY?"command mailbox not idle at bridge start":input.error?"command producer unavailable at bridge start":"command mailbox identity, format or lock unavailable at bridge start";goto done;}
 collection_init(&surfaces,&runtime_collection);
 if(!collection_admission_open(&surfaces,argv[1],pid)){reason="Preferences page admission failed";goto done;}
 BRIDGE_LOG("INPUT channel-bank source commands; continuous fader coalescing, independent bounded control transactions, fair owner-drain batches; first/touchless positions accepted; raw SERVO is not source settlement\n");
 BRIDGE_LOG("PROFILE %s capabilities=0x%x from=%s/%s type=%s to=%s/%s type=%s%s%s\n",controller.profile->name,controller.profile->capabilities,controller.client,controller.port,endpoint_type_name(controller.physical_only),controller_output.client,controller_output.port,endpoint_type_name(controller_output.physical_only),controller.profile->protocol==MCU_PROTOCOL_HUI?" hui-mode=":"",controller.profile->protocol==MCU_PROTOCOL_HUI?(controller_hui_policy==MCU_HUI_MOTORIZED?"motorized":"pickup"):"");
 BRIDGE_LOG("SERVO_MODE %s; profile raw-echo policy, MPC command/source settlement and authoritative motor policy unchanged\n",servo_disabled?"OFF":"ON");
#endif
 if(snd_seq_open(&discovery,"default",SND_SEQ_OPEN_DUPLEX,SND_SEQ_NONBLOCK)<0){reason="MIDI discovery unavailable";goto done;}
#ifdef MIRROR_INPUT
 surfaces.seq=discovery;
 if(snd_seq_set_client_name(discovery,"mpclearn-mirror-motors")<0||snd_seq_set_client_pool_input(discovery,4096)<0||snd_seq_set_client_pool_output(discovery,16)<0){reason="shared MIDI client unavailable";goto done;}
 surfaces.sink=snd_seq_create_simple_port(discovery,"private MCU touch and bank",SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_NO_EXPORT,SND_SEQ_PORT_TYPE_APPLICATION);
 surfaces.source=snd_seq_create_simple_port(discovery,"MCU eight motor output",SND_SEQ_PORT_CAP_READ,SND_SEQ_PORT_TYPE_APPLICATION);
 if(surfaces.sink<0||surfaces.source<0||snd_seq_connect_from(discovery,surfaces.sink,SND_SEQ_CLIENT_SYSTEM,SND_SEQ_PORT_SYSTEM_ANNOUNCE)<0){reason="shared MIDI ports unavailable";goto done;}
 if(!collection_stop_ingress(&surfaces)){reason="Stop relay unavailable";goto done;}
#else
 if(snd_seq_set_client_name(discovery,"mpclearn-mcu-discovery")<0){reason="MIDI discovery unavailable";goto done;}
#endif
 uint64_t began_wall=monotonic_ms();if(began_wall==UINT64_MAX||began_wall>UINT64_MAX-seconds*1000){reason="clock overflow";goto done;}
 uint64_t until=seconds?began_wall+seconds*1000:UINT64_MAX,last_identity=0,last_discovery=0;
#ifndef MIRROR_INPUT
 uint64_t last_connect=0;
#endif
#ifdef MIRROR_INPUT
 BRIDGE_LOG("BRIDGE_READY pid=%u\n",(unsigned)getpid());
#endif
 BRIDGE_LOG("SOURCE pid=%u start=%llu epoch=%u; automatic eight-playable-track volume bank\n",pid,(unsigned long long)start,snapshot.epoch);
#ifdef MIRROR_INPUT
 while(!stopping){
  runtime_cycle_begin(&runtime);
  uint64_t wall=monotonic_ms();if(wall==UINT64_MAX){reason="clock failure";break;}if(wall>=until){result=0;reason="finite duration";break;}
  if(state->origin_sec!=began_sec||state->origin_nsec!=began_nsec){reason="source origin changed";break;}
  if(wall-last_identity>=100){last_identity=wall;if(!source_identity(argv[1],fd,&file,state,pid,start,&exe)||!input_file_current(argv[2],state,start)){reason="source/mailbox identity changed";break;}}
  uint32_t heartbeat=atomic_load_explicit(&state->heartbeat,memory_order_acquire);now=source_now(state);
  if(now==UINT32_MAX||!atomic_load(&state->alive)||atomic_load(&state->error)||now<heartbeat||now-heartbeat>=1000||!collection_health(&surfaces)){reason="source failure while parked or active";break;}
  /* App-only demand uses the same snapshot/settlement owner with a narrow
   * projection. Any surviving controller flight retains the ordinary copy. */
  if(!surfaces.connected&&plugin_only_flights()&&(plugin_ui_live(now)||!input_drained(&input))){
   runtime.snapshot_attempted=1;runtime.now=now;runtime.snapshot_result=plugin_copy(state,&snapshot,now);
  }
  /* Drain exits before rediscovery, so reused numeric addresses never inherit
   * a previous connection's gesture/cache state. */
  if(collection_needs_snapshot(&surfaces)){
   int copied=runtime_snapshot(&runtime,state,collection_pads(&surfaces));now=runtime.now;
   if(copied<0){reason="source unavailable before shared input";break;}
  }
  if(!collection_drain(&surfaces,state,&runtime)){reason="shared MIDI continuity lost";break;}
  if(wall-last_discovery>=(surfaces.connected?100u:250u)){last_discovery=wall;if(!collection_topology(&surfaces,state)){reason="shared MIDI topology unavailable";break;}collection_admission_publish(&surfaces);}
  if(!collection_needs_snapshot(&surfaces)&&!plugin_ui_live(now)){if(surfaces.interest_active||plugin_reply.slot!=EFFECT_LIST)collection_interests(&surfaces,&snapshot,now);poll(NULL,0,10);continue;}
  int copied=runtime.snapshot_attempted?runtime.snapshot_result:runtime_snapshot(&runtime,state,collection_pads(&surfaces));now=runtime.now;
  if(copied<0){reason="source unavailable before shared feedback";break;}
  if(!copied){for(unsigned i=0;i<surfaces.count;i++)if(surfaces.surfaces[i].seq){bank_drop(surfaces.banks+i,now);input_discard(&surfaces.contexts[i].input);}poll(NULL,0,10);continue;}
  if(!collection_cycle(&surfaces,state,&runtime)){reason="shared command or feedback failure";break;}
  collection_admission_publish(&surfaces);
  poll(NULL,0,10);
 }
#else
 while(!stopping){
#ifdef MIRROR_INPUT
  runtime_cycle_begin(&runtime);
#endif
  uint64_t wall=monotonic_ms();if(wall==UINT64_MAX){reason="clock failure";break;}if(wall>=until){result=0;reason="finite duration";break;}
  if(state->origin_sec!=began_sec||state->origin_nsec!=began_nsec){reason="source origin changed";break;}
  if(wall-last_identity>=100){last_identity=wall;if(!source_identity(argv[1],fd,&file,state,pid,start,&exe)){reason="source/process identity changed";break;}
#ifdef MIRROR_INPUT
   if(!input_file_current(argv[2],state,start)){reason="mailbox identity changed";break;}
#endif
  }
  if(!surface.seq&&disconnected_idle){
   /* No hardware consumer and no unsettled command: do not clear/copy the
    * entire mixer at100Hz. Lifetime and freshness still fail closed. */
   uint32_t heartbeat=atomic_load_explicit(&state->heartbeat,memory_order_acquire);now=source_now(state);
   if(now==UINT32_MAX||!atomic_load_explicit(&state->alive,memory_order_acquire)||atomic_load_explicit(&state->error,memory_order_acquire)||now<heartbeat||now-heartbeat>=1000){reason="mirror failure or stale heartbeat while disconnected";break;}
#ifdef MIRROR_INPUT
   if(!input_health(&input)){reason="command source unavailable while disconnected";break;}
   if(!input_drained(&input))disconnected_idle=0;
#endif
   if(disconnected_idle){
    snd_seq_addr_t found_input,found_output;
    if(wall-last_connect<250){poll(NULL,0,10);continue;}
    last_connect=wall;
    if(!discover_endpoints(discovery,&controller,MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ,&found_input,&controller_output,MCU_ENDPOINT_WRITE,&found_output)){poll(NULL,0,10);continue;}
    disconnected_idle=0;last_connect=0; /* normal fresh-copy path before attach */
   }
  }
  int copied=0;
  if(!surface.seq){
#ifdef MIRROR_INPUT
   copied=runtime_snapshot(&runtime,state,bank.view==BV_DRUM_PADS);now=runtime.now;
#else
   copied=fresh_copy_view(state,&snapshot,&now,bank.view==BV_DRUM_PADS);
#endif
   if(copied<0){reason="mirror failure or stale heartbeat";break;}
   if(!copied){
#ifdef MIRROR_INPUT
    if(surface.seq&&(bank.view==BV_DRUM_PADS||bank.assignment==BA_SEND))(void)parent_led_clear(&surface);
#endif
    bank_drop(&bank,now);
#ifdef MIRROR_INPUT
    input_discard(&input);input_jog_discard(&input);input_master_invalidate(&input);
#endif
    poll(NULL,0,10);continue;}
   bank_apply(&bank,&snapshot,now);
#ifdef MIRROR_INPUT

   input_sync(&input,&bank);hui_pickup_sync(&surface,&input,&bank,now);input_health(&input);
   if(input.error){reason="input rejected, ambiguous, expired or unavailable (see error code)";break;}
#endif
 #ifdef MIRROR_INPUT
   runtime_settle(&runtime,&input,&bank); /* also settle while USB is absent */
   if(input.error){reason="published transaction drain failed";break;}
#endif
   if(stopping)break;

#ifdef MIRROR_INPUT
   if(!surface.seq){input_meter_interest(&input,&bank,&snapshot,now,0);input_io_interest(&input,&bank,&snapshot,now,0);input_effects_interest(&input,&bank,&snapshot,now,0);input_qlink_interest(&input,&bank,&snapshot,now,0);}
#endif
  }
  if(!surface.seq){
   if(wall-last_connect>=250){last_connect=wall;if(surface_open(&surface,&bank,state,discovery))last_identity=0;}
#ifdef MIRROR_INPUT
   disconnected_idle=!surface.seq&&input_drained(&input);
#else
   disconnected_idle=!surface.seq;
#endif
   poll(NULL,0,10);continue;
  }
  snd_seq_addr_t current_input,current_output;
  if(wall-last_discovery>=100){last_discovery=wall;if(!discover_endpoints(surface.seq,&controller,MCU_ENDPOINT_READ|MCU_ENDPOINT_SUBS_READ,&current_input,&controller_output,MCU_ENDPOINT_WRITE,&current_output)||!address_equal(current_input,surface.full)||!address_equal(current_output,surface.output)){BRIDGE_LOG("DISCONNECTED; pending output discarded\n");surface_close(&surface,&bank);continue;}}
#ifdef MIRROR_INPUT
  copied=runtime_snapshot(&runtime,state,bank.view==BV_DRUM_PADS);now=runtime.now;
  if(copied<0){reason="source unavailable before input publication";break;}
#endif
  if(!surface_drain(&surface,&bank,state,0,active_runtime)){BRIDGE_LOG("INPUT continuity lost; all touch states UNKNOWN\n");surface_close(&surface,&bank);continue;}
#ifdef MIRROR_INPUT
  copied=runtime.snapshot_result;now=runtime.now;
  if(controller.profile->protocol==MCU_PROTOCOL_HUI){uint32_t live_now=source_now(state);if(live_now==UINT32_MAX){reason="clock failure during HUI liveness";break;}if(!hui_liveness_current(&surface,live_now)){BRIDGE_LOG("HUI heartbeat reply timeout; protocol state cleared and endpoint rediscovery required\n");surface_close(&surface,&bank);continue;}}
  /* The cycle snapshot was taken before draining input and is shared by
   * decode, settlement, submission and feedback. Native execution still
   * revalidates every target and revision. */
  if(!copied){
#ifdef MIRROR_INPUT
   if(surface.seq&&(bank.view==BV_DRUM_PADS||bank.assignment==BA_SEND))(void)parent_led_clear(&surface);
#endif
   bank_drop(&bank,now);input_discard(&input);input_master_invalidate(&input);continue;}
  bank_apply(&bank,&snapshot,now);input_sync(&input,&bank);hui_pickup_sync(&surface,&input,&bank,now);
  runtime_settle(&runtime,&input,&bank);
  if(input.error){reason="published transaction drain failed";break;}
  if(stopping)break;
  input_meter_interest(&input,&bank,&snapshot,now,surface.seq!=NULL&&mcu_has(&controller,MCU_CAP_METERS));input_io_interest(&input,&bank,&snapshot,now,surface.seq!=NULL);input_effects_interest(&input,&bank,&snapshot,now,surface.seq!=NULL);input_qlink_interest(&input,&bank,&snapshot,now,surface.seq!=NULL);
  runtime_submit(&runtime,&input,&bank);
  if(input.error){reason="input rejected, ambiguous, expired or unavailable (see error code)";break;}
  if(!channel_output(&surface,&bank,&snapshot,now)){surface_close(&surface,&bank);continue;}
#else
  copied=fresh_copy_view(state,&snapshot,&now,bank.view==BV_DRUM_PADS);
  if(copied<0){reason="source unavailable before motor output";break;}
  if(!copied){bank_drop(&bank,now);continue;}
  bank_apply(&bank,&snapshot,now);
#endif
  if(mcu_has(&controller,MCU_CAP_STRIP_MOTOR))for(unsigned channel=0;channel<MIRROR_BANK;channel++){
   /* The iteration already copied source state and serviced input. An idle
    * strip need not repeat full copies; a prospective send still takes every
    * original fresh source/USB/touch/command check below. */
#ifdef MIRROR_INPUT
   if(!strip_motor_ready(&surface,&bank,channel,now))continue;
#else
   if(bank_due(&bank,channel,now)<0)continue;
#endif
   /* Keep event continuity and touch suppression at the send boundary. Only
    * the target scalar/projection is revalidated; no full mixer or ALSA scan. */
   if(!surface_drain(&surface,&bank,state,0,active_runtime)||lost_events(surface.seq)){surface_close(&surface,&bank);break;}
   now=source_now(state);
   if(!mirror_motor_current(state,&snapshot,&bank,channel,now))continue;
#ifdef MIRROR_INPUT
   input_sync(&input,&bank);
   if(input_blocked_field(&input,channel,bank_field(&bank,channel)))continue;
   if(!input_health(&input)){reason="command source unavailable at motor boundary";goto done;}
#endif
#ifdef MIRROR_INPUT
   int emitted=strip_motor_output(&surface,&bank,channel,now);if(emitted<0){surface_close(&surface,&bank);break;}if(!emitted)continue;
   MirrorFader *f=bank.faders+channel;int position=f->last_sent;sent++;
#else
   int position=bank_due(&bank,channel,now);if(position<0)continue;
   snd_seq_event_t event;motor_event(&event,&surface,channel,position);
   if(stopping)break;
   if(snd_seq_event_output_direct(surface.seq,&event)<0){surface_close(&surface,&bank);break;}
   MirrorFader *f=bank.faders+channel;f->last_sent=position;f->last_tick=now;f->physical=-1;sent++;
#endif
#ifdef MIRROR_INPUT
   if(bridge_verbose)
#endif
   BRIDGE_LOG("MOTOR fader=%u position=%d epoch=%u track=%u binding=%u incarnation=%u volume_revision=%u heartbeat=%u empty=%d\n",channel+1,position,bank.epoch,f->identity.serial,f->identity.binding,f->identity.incarnation,f->revision,bank.heartbeat,f->empty);
  }
  poll(NULL,0,10);
 }
#endif
 if(stopping){result=0;reason="signal stop";}
done:
#ifdef MIRROR_INPUT
 unsigned final_holds=0,first=0;while(first<surfaces.count&&!surfaces.enabled[first])first++;
 if(first<surfaces.count&&surfaces.surfaces[first].seq){for(unsigned i=0;i<8;i++)if(surfaces.banks[first].faders[i].touch==MT_DOWN)final_holds|=1u<<i;if(surfaces.surfaces[first].master_touch)final_holds|=256;fprintf(stderr,"TOUCH_FINAL pid=%ld mask=%u\n",(long)getpid(),final_holds);}
 for(unsigned i=0;i<surfaces.count;i++){
  collection_disconnect(&surfaces,i);
 }
 if(surfaces.preferences){munmap(surfaces.preferences,sizeof(*surfaces.preferences));surfaces.preferences=NULL;}
 fprintf(stderr,"COLLECTION configured=%u origins=%u topology_scans=%llu topology_rows=%llu routed=%llu unrouted=%llu encoded=%llu sent=%llu interest_visits=%llu connections=%llu disconnections=%llu\n",surfaces.count,command_coordinator.count,(unsigned long long)surfaces.topology_scans,(unsigned long long)surfaces.topology_rows,(unsigned long long)surfaces.routed,(unsigned long long)surfaces.unrouted,(unsigned long long)surfaces.encoded,(unsigned long long)surfaces.sent,(unsigned long long)surfaces.interest_visits,(unsigned long long)surfaces.connections,(unsigned long long)surfaces.disconnections);
 /* Stop all hardware traffic first, retain exclusive mailbox writer until the
  * already published transaction is source-settled AND owner-reclaimed. */
 surface_close(&surface,&bank);input_discard(&input);
 if(input.commands&&state!=MAP_FAILED&&!input_drained(&input)){
  uint64_t deadline=monotonic_ms();
  if(deadline!=UINT64_MAX&&deadline<=UINT64_MAX-10000)deadline+=10000;
  else deadline=0;
  while(deadline&&!input.error&&monotonic_ms()<deadline){
   runtime_cycle_begin(&runtime);
   if(!source_identity(argv[1],fd,&file,state,pid,start,&exe)||!input_file_current(argv[2],state,start)||runtime_snapshot(&runtime,state,bank.view==BV_DRUM_PADS)!=1)break;
   now=runtime.now;bank_apply(&bank,&snapshot,now);runtime_settle(&runtime,&input,&bank);if(!collection_health(&surfaces))break;
   if(input_drained(&input))break;
   poll(NULL,0,10);
  }
  if(!input_drained(&input)){result=1;reason="incomplete published transaction at bridge stop";}
 }
 for(unsigned i=0;i<surfaces.count;i++){
  SurfaceContext *context=surfaces.contexts+i;
  fprintf(stderr,"SURFACE controller=%u enabled=%u received=%llu encoded=%llu sent=%llu bank=%u submitted=%u settled=%u refused=%u error=%u\n",i,surfaces.enabled[i],(unsigned long long)context->received,(unsigned long long)context->encoded,(unsigned long long)context->sent,surfaces.banks[i].offset,context->input.submitted,context->input.settled,context->input.refused,context->input.error);
  input.submitted+=context->input.submitted;input.settled+=context->input.settled;input.refused+=context->input.refused;
 }
 if(input_mapping!=MAP_FAILED)fprintf(stderr,"MAILBOX published=%u consumed=%u reclaimed=%u pending=%u source_error=%u trace_error=%u\n",atomic_load(&input_mapping->published),atomic_load(&input_mapping->consumed),atomic_load(&input_mapping->reclaimed),command_pending(input_mapping),atomic_load(&input_mapping->error),atomic_load(&input_mapping->trace_error));
 fprintf(stderr,"RUNTIME iterations=%llu snapshot_requests=%llu snapshot_copies=%llu settlement_requests=%llu settlement_scans=%llu submission_visits=%llu deferred_pad_views=%llu\n",(unsigned long long)runtime.iterations,(unsigned long long)runtime.snapshot_requests,(unsigned long long)runtime.snapshot_copies,(unsigned long long)runtime.settlement_requests,(unsigned long long)runtime.settlement_scans,(unsigned long long)runtime.submission_visits,(unsigned long long)runtime.deferred_pad_views);
 fprintf(stderr,"INPUT STOP error=%u reason=%s submitted=%u settled=%u refused=%u pending_request=%u; timeout/return never acknowledged\n",input.error,command_error_name(input.error),input.submitted,input.settled,input.refused,input_pending(&input));input_close_file();
#endif
 if(discovery)snd_seq_close(discovery);
 surface_close(&surface,&bank);if(state!=MAP_FAILED)munmap((void*)state,sizeof(*state));if(fd>=0)close(fd);if(lockfd>=0)close(lockfd);
#ifdef MIRROR_INPUT
 fprintf(stderr,"STOP %s; strip_motor_messages=%llu; no reset sent\n",reason,(unsigned long long)surfaces.motor_messages);
#else
 fprintf(stderr,"STOP %s; motor_messages=%u; no reset sent\n",reason,sent);
#endif
 return result;
}
