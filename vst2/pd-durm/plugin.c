#define _GNU_SOURCE
#include "abi.h"
#include "midi.h"
#include "engine.h"
#include <dlfcn.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define QUEUE 1024
#define UID 0x50644431
/* VST2 callbacks for one instance are serialized here, including coherent
   chunk snapshots. No same-instance Pd call can overlap another API access. */
typedef struct {uint64_t due;unsigned note;} Pending;
typedef struct {
    Effect effect;pthread_mutex_t mutex;Engine engine;
    char directory[PATH_MAX];
    int rate,active,failed,muted;
    unsigned phase,count;
    uint64_t frame;
    Pending pending[QUEUE];
    float delayed[128],tick[128];
    unsigned char chunk[STATE_BYTES];
} Plugin;
static void silence(Plugin *p) {
    p->count=0;p->muted=1;memset(p->delayed,0,sizeof(p->delayed));memset(p->tick,0,sizeof(p->tick));
    if(p->engine.pd)engine_panic(&p->engine);
}
static int reopen(Plugin *p,const State *saved) {
    State copy=*saved;engine_close(&p->engine);
    p->active=0;p->phase=0;p->frame=0;p->count=0;p->muted=1;
    memset(p->delayed,0,sizeof(p->delayed));memset(p->tick,0,sizeof(p->tick));
    p->failed=!engine_open(&p->engine,p->directory,p->rate,&copy);return !p->failed;
}
static float get(Effect *e,uint32_t i) {
    Plugin *p=(Plugin*)e;if(i>=PARAMS)return 0;
    pthread_mutex_lock(&p->mutex);float f=p->engine.state.controls[i];pthread_mutex_unlock(&p->mutex);return f;
}
static void set(Effect *e,uint32_t i,float f) {
    if(i>=PARAMS||!isfinite(f))return;
    if(f<0)f=0;if(f>1)f=1;if(i>=15)f=f>=.5f;
    Plugin *p=(Plugin*)e;pthread_mutex_lock(&p->mutex);
    if(!p->failed)engine_control(&p->engine,i,f);
    pthread_mutex_unlock(&p->mutex);
}
static int events(Plugin *p,const MidiEvents *in) {
    if(!p->active||p->failed)return 0;
    if(!in||in->count<0||in->count>QUEUE)goto fail;
    for(int i=0;i<in->count;i++) {
        const MidiEvent *m=in->events[i];if(!m)goto fail;
        if(m->type!=1)continue;
        if(m->byte_size!=32||m->delta<0||m->delta>1048576)goto fail;
        unsigned op=m->data[0]&0xf0,n=m->data[1];
        if(op==0xb0&&(n==120||n==123)) {
            /* Preserve the panic sample offset through the same bounded queue. */
            n=0;
        } else if(op!=0x90||!m->data[2]||(n!=36&&n!=38&&n!=42&&n!=46&&n!=39))continue;
        if(p->count==QUEUE)goto fail;
        Pending event={(p->frame+(unsigned)m->delta+63)/64*64,n};
        unsigned j=p->count++;
        while(j && p->pending[j-1].due>event.due){p->pending[j]=p->pending[j-1];--j;}
        p->pending[j]=event;
    }
    return 1;
fail:silence(p);p->failed=1;return 0;
}
static void process(Effect *e,const float *const *in,float **out,int32_t frames) {
    (void)in;if(frames<=0)return;
    Plugin *p=(Plugin*)e;pthread_mutex_lock(&p->mutex);
    for(int32_t i=0;i<frames;i++) {
        if(p->active&&!p->failed) {
            if(p->phase==0) {
                while(p->count&&p->pending[0].due<=p->frame) {
                    unsigned note=p->pending[0].note;
                    memmove(p->pending,p->pending+1,(--p->count)*sizeof(Pending));
                    if(!note)silence(p);
                    else {engine_trigger(&p->engine,note);p->muted=0;}
                }
                engine_tick(&p->engine,p->tick);
            }
            for(unsigned c=0;c<2;c++) {
                unsigned j=p->phase*2+c;
                if(out&&out[c])out[c][i]=p->muted?0:p->delayed[j];
                p->delayed[j]=p->tick[j];
            }
            p->phase=(p->phase+1)%64;++p->frame;
        }else for(unsigned c=0;c<2;c++)if(out&&out[c])out[c][i]=0;
    }
    pthread_mutex_unlock(&p->mutex);
}
static intptr_t dispatch(Effect *e,int32_t op,int32_t index,intptr_t value,void *ptr,float opt) {
    Plugin *p=(Plugin*)e;intptr_t result=0;
    pthread_mutex_lock(&p->mutex);
    switch(op) {
    case 0:result=!p->failed;break;
    case 1:engine_close(&p->engine);pthread_mutex_unlock(&p->mutex);pthread_mutex_destroy(&p->mutex);free(p);return 0;
    case 2:silence(p);break;
    case 3:break;
    case 5:if(ptr)snprintf(ptr,24,"Durm");break;
    case 6:if(ptr&&index>=0&&index<PARAMS)snprintf(ptr,8,"%s",index>=15?"":"%");break;
    case 7:if(ptr&&index>=0&&index<PARAMS) {
        if(index>=15)snprintf(ptr,8,"%s",p->engine.state.controls[index]?"Locked":"Free");
        else snprintf(ptr,8,"%.1f",100.0*p->engine.state.controls[index]);
        }break;
    case 8:if(ptr&&index>=0&&index<PARAMS)snprintf(ptr,8,"%s",control_names[index]);break;
    case 10:
        if(opt!=44100&&opt!=48000){silence(p);p->failed=1;break;}
        if(p->rate!=(int)opt && !p->active){p->rate=(int)opt;result=reopen(p,&p->engine.state);}break;
    case 11:break; /* Streaming adapter has no maximum host block size. */
    case 12:silence(p);p->active=value!=0&&!p->failed;break;
    case 23:if(ptr){state_encode(&p->engine.state,p->chunk);*(void**)ptr=p->chunk;result=STATE_BYTES;}break;
    case 24:if(!p->active&&ptr&&value==STATE_BYTES) {
        State candidate;if(state_decode(&candidate,ptr,(unsigned)value))result=reopen(p,&candidate);
        }break;
    case 25:result=events(p,ptr);break;
    case 26:result=index>=0&&index<PARAMS;break;
    case 29:if(!index&&ptr){snprintf(ptr,24,"Durm");result=1;}break;
    case 35:result=2;break;
    case 45:if(ptr)snprintf(ptr,32,"Pd Durm");result=1;break;
    case 47:if(ptr)snprintf(ptr,64,"mpclearn / Qu-Bit");result=1;break;
    case 48:if(ptr)snprintf(ptr,64,"Pd Durm");result=1;break;
    case 49:result=1;break;
    case 51:result=ptr&&(!strcmp(ptr,"receiveVstEvents")||!strcmp(ptr,"receiveVstMidiEvent"));break;
    case 58:result=2400;break;
    }
    pthread_mutex_unlock(&p->mutex);return result;
}
__attribute__((visibility("default"))) Effect *VSTPluginMain(Host host) {
    (void)host;Dl_info location;
    if(!dladdr((void*)VSTPluginMain,&location)||!location.dli_fname)return NULL;
    Plugin *p=calloc(1,sizeof(*p));if(!p)return NULL;
    if(strlen(location.dli_fname)>=sizeof(p->directory)){free(p);return NULL;}
    strcpy(p->directory,location.dli_fname);char *slash=strrchr(p->directory,'/');
    if(slash)*slash=0;else strcpy(p->directory,".");
    pthread_mutex_init(&p->mutex,NULL);p->rate=44100;p->muted=1;
    if(!engine_open(&p->engine,p->directory,p->rate,NULL)){pthread_mutex_destroy(&p->mutex);free(p);return NULL;}
    p->effect=(Effect){.magic=0x56737450,.dispatch=dispatch,.set=set,.get=get,.programs=1,.params=PARAMS,.inputs=0,.outputs=2,.flags=0x130,.delay=64,.reserved_float=1,.object=p,.uid=UID,.version=1,.replacing=process};
    return &p->effect;
}
