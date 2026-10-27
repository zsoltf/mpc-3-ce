/* MIT adapter; private libpd and Durm retain their upstream licenses. */
#include "engine.h"
#include "identity.h"
#include "sha256.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
const char *const control_names[PARAMS]={"Start","Size","Density","Overlap","Blend","Window","Pitch","Speed","St Rand","Sz Rand","Dn Rand","Ov Rand","Bl Rand","FX Mode","K Pitch","K Lock","S Lock","H Lock","F Lock"};
const char *const receivers[PARAMS]={"start","size","density","overlap","blend","window","pitch","speed","start_alt","size_alt","density_alt","overlap_alt","blend_alt","window_alt","pitch_alt","pd-durm-lock-bd","pd-durm-lock-sd","pd-durm-lock-hh","pd-durm-lock-fm"};
const float defaults[PARAMS]={.23f,.3f,.24f,.51f,.54f,.72f,.3f,.5f,0,0,0,0,0,.6f,.33f,0,0,0,0};
static const char *const voices[]={"bd","sd","hh","fm"};
static void print_hook(const char *s) {
    Engine *e=libpd_get_instancedata();
    if(e && (strstr(s,"error") || strstr(s,"couldn't create") || strstr(s,"connection failed"))) e->errors++;
    if(e && e->loading)fputs(s,stderr);
    /* Deliberately no audio-thread logging. Opening failures are counted. */
}
static void float_hook(const char *name,float value) {
    Engine *e=libpd_get_instancedata();
    for(unsigned v=0;v<4;v++) for(unsigned j=0;j<7;j++) {
        char key[40];snprintf(key,sizeof(key),"pd-durm-held-%s-%u",voices[v],j);
        if(!strcmp(name,key)) {e->state.held[v*7+j]=value;return;}
    }
}
static pthread_once_t once=PTHREAD_ONCE_INIT;
static void initialize(void) { libpd_init(); }
void engine_global_init(void) { pthread_once(&once,initialize); }
void engine_close(Engine *e) {
    if(!e->pd)return;
    libpd_set_instance(e->pd);
    if(e->patch)libpd_closefile(e->patch);
    for(unsigned i=0;i<HELD;i++)if(e->bindings[i])libpd_unbind(e->bindings[i]);
    libpd_free_instance(e->pd);e->pd=NULL;e->patch=NULL;
}
void engine_control(Engine *e,unsigned i,float value) {
    if(i>=PARAMS)return;
    libpd_set_instance(e->pd);e->state.controls[i]=value;
    libpd_float(receivers[i],value);
}
static int fixed_patch_matches(const char *directory) {
    char path[4096];
    int size=snprintf(path,sizeof(path),"%s/durm.pd",directory);
    if(size<0 || (unsigned)size>=sizeof(path))return 0;
    FILE *file=fopen(path,"rb");if(!file)return 0;
    Sha sha;sha_init(&sha);unsigned char buffer[4096],digest[32];size_t count;
    while((count=fread(buffer,1,sizeof(buffer),file))!=0)sha_add(&sha,buffer,count);
    int ok=!ferror(file);fclose(file);sha_end(&sha,digest);
    return ok&&!memcmp(digest,PATCH_ID,32);
}
int engine_open(Engine *e,const char *directory,int rate,const State *saved) {
    State state={0};
    if(saved)state=*saved;else memcpy(state.controls,defaults,sizeof(defaults));
    memset(e,0,sizeof(*e));
    if(!fixed_patch_matches(directory))return 0;
    engine_global_init();
    e->pd=libpd_new_instance();if(!e->pd)return 0;
    libpd_set_instance(e->pd);libpd_set_instancedata(e,NULL);
    libpd_set_printhook(print_hook);libpd_set_floathook(float_hook);
    if(libpd_init_audio(0,2,rate))goto fail;
    e->rate=rate;e->loading=1;
    for(unsigned v=0;v<4;v++)for(unsigned j=0;j<7;j++) {
        char key[40];snprintf(key,sizeof(key),"pd-durm-held-%s-%u",voices[v],j);
        e->bindings[v*7+j]=libpd_bind(key);
    }
    e->patch=libpd_openfile("durm.pd",directory);
    if(!e->patch || e->errors)goto fail;
    /* Hardware delayed loadbangs are removed. Explicit initialization precedes
       control and held-value restore and never enters a drum trigger inlet. */
    libpd_bang("initbang");
    for(unsigned i=0;i<PARAMS;i++)engine_control(e,i,state.controls[i]);
    for(unsigned v=0;v<4;v++)for(unsigned j=0;j<7;j++) {
        char key[40];snprintf(key,sizeof(key),"pd-durm-restore-%s-%u",voices[v],j);
        libpd_float(key,state.held[v*7+j]);
    }
    libpd_bang("pd-durm-seed");
    libpd_start_message(1);libpd_add_float(1);libpd_finish_message("pd","dsp");
    if(e->errors)goto fail;
    e->loading=0;return 1;
fail: engine_close(e);return 0;
}
void engine_trigger(Engine *e,unsigned note) {
    const char *recv=note==36?"record":note==38?"file":note==42?"source":note==46?"reset":note==39?"freeze":NULL;
    if(!recv)return;
    libpd_set_instance(e->pd);
    /* The hardware graph uses change/sel 1. Only a complete rising pulse is
       delivered for note-on; MIDI release is not a hardware edge. */
    libpd_float(recv,0);libpd_float(recv,1);
}
void engine_panic(Engine *e) {
    libpd_set_instance(e->pd);libpd_bang("pd-durm-panic");libpd_bang("kill_burst");
}
void engine_tick(Engine *e,float stereo[128]) {
    libpd_set_instance(e->pd);libpd_process_float(1,NULL,stereo);
}
static void put32(unsigned char *p,uint32_t n) {for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(8*i));}
static uint32_t get32(const unsigned char *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
void state_encode(const State *s,unsigned char out[STATE_BYTES]) {
    memcpy(out,"PDDURM1",8);put32(out+8,1);memcpy(out+12,PATCH_ID,32);put32(out+44,1);
    for(unsigned i=0;i<PARAMS+HELD;i++) {
        float f=i<PARAMS?s->controls[i]:s->held[i-PARAMS];uint32_t bits;memcpy(&bits,&f,4);put32(out+48+4*i,bits);
    }
}
int state_decode(State *s,const unsigned char *p,unsigned length) {
    if(length!=STATE_BYTES || memcmp(p,"PDDURM1",8) || get32(p+8)!=1 || memcmp(p+12,PATCH_ID,32) || get32(p+44)!=1)return 0;
    State candidate;
    for(unsigned i=0;i<PARAMS+HELD;i++) {
        uint32_t bits=get32(p+48+4*i);float f;memcpy(&f,&bits,4);
        if(!isfinite(f)||f<0||f>(i<PARAMS?1:2))return 0;
        if(i>=15 && i<PARAMS && f!=0 && f!=1)return 0;
        if(i<PARAMS)candidate.controls[i]=f;else candidate.held[i-PARAMS]=f;
    }
    *s=candidate;return 1;
}
