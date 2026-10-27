#define _GNU_SOURCE
#include "abi.h"
#include "midi.h"
#include "engine.h"
#include <dlfcn.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define N 16384
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static Factory factory;
static intptr_t host(Effect *e,int32_t op,int32_t i,intptr_t v,void *p,float f){(void)e;(void)i;(void)v;(void)p;(void)f;return op==1?2400:0;}
static void midi(Effect *e,unsigned status,unsigned n,unsigned vel,int delta) {
    MidiEvent event={.type=1,.byte_size=32,.delta=delta,.data={status,n,vel,0}};
    struct {int32_t count;void *reserved;const MidiEvent *events[1];} list={1,NULL,{&event}};
    REQUIRE(e->dispatch(e,25,0,0,&list,0)==1);
}
static Effect *create(int rate) {
    Effect *e=factory(host);REQUIRE(e);REQUIRE(e->magic==0x56737450&&e->uid==0x50644431&&e->delay==64&&e->params==PARAMS);
    e->dispatch(e,10,0,0,NULL,(float)rate);e->dispatch(e,12,0,1,NULL,0);return e;
}
static void close_effect(Effect *e){e->dispatch(e,1,0,0,NULL,0);}
static void process(Effect *e,float *audio,unsigned start,unsigned count) {
    float left[1024],right[1024],*out[]={left,right};REQUIRE(count<=1024);
    e->replacing(e,NULL,out,(int)count);
    for(unsigned i=0;i<count;i++){REQUIRE(isfinite(left[i])&&isfinite(right[i]));audio[2*(start+i)]=left[i];audio[2*(start+i)+1]=right[i];}
}
static const unsigned notes[]={36,38,42,46,39};
static const unsigned times[]={0,1,63,64,65,127,128,513,1000,2047,3001,6000};
static void render(Effect *e,float *audio,unsigned pattern,unsigned variant) {
    unsigned pos=0,next=0,k=0;const unsigned partition[]={1,63,64,257,17,511};
    while(pos<N) {
        unsigned count=pattern?partition[k++%6]:256;if(count>N-pos)count=N-pos;
        while(next<sizeof(times)/sizeof(times[0]) && times[next]<pos+count){
            midi(e,0x90,notes[(next+variant)%5],100,(int)(times[next]-pos));next++;
        }
        process(e,audio,pos,count);pos+=count;
    }
}
static double energy(const float *a,unsigned count) {double sum=0;for(unsigned i=0;i<2*count;i++)sum+=(double)a[i]*a[i];return sum;}
static void chunk(Effect *e,unsigned char bytes[STATE_BYTES],State *s) {
    void *p=NULL;REQUIRE(e->dispatch(e,23,0,0,&p,0)==STATE_BYTES&&p);memcpy(bytes,p,STATE_BYTES);REQUIRE(state_decode(s,bytes,STATE_BYTES));
}
static void signal_handler(int sig){(void)sig;}
static void voices_check(int rate) {
    float a[N*2];
    for(unsigned n=0;n<5;n++) {
        Effect *e=create(rate);midi(e,0x90,notes[n],100,0);
        for(unsigned p=0;p<N;p+=256)process(e,a,p,256);
        double sum=energy(a,N);REQUIRE(sum>1e-5);
        unsigned first=0;while(first<N && a[2*first]==0 && a[2*first+1]==0)first++;
        REQUIRE(first>=64);
        float peak=0;for(unsigned i=0;i<N*2;i++)if(fabsf(a[i])>peak)peak=fabsf(a[i]);
        printf("rate %d note %u energy %.9g peak %.6f onset %u\n",rate,notes[n],sum,peak,first);
        close_effect(e);
    }
    Effect *e=create(rate);midi(e,0x80,36,100,0);midi(e,0x90,38,0,1);
    process(e,a,0,1024);REQUIRE(energy(a,1024)==0);close_effect(e);
}
static void direct_check(int rate,const char *directory) {
    float a[N*2],b[N*2],reference[N*2];
    Effect *first=create(rate);render(first,a,0,0);close_effect(first);
    Effect *partitioned=create(rate);render(partitioned,b,1,0);close_effect(partitioned);
    REQUIRE(memcmp(a,b,sizeof(a))==0);
    Engine direct;REQUIRE(engine_open(&direct,directory,rate,NULL));
    memset(reference,0,sizeof(reference));unsigned next=0;
    for(unsigned frame=0;frame<N-64;frame+=64) {
        while(next<sizeof(times)/sizeof(times[0]) && ((times[next]+63)/64)*64<=frame) {
            engine_trigger(&direct,notes[next%5]);next++;
        }
        engine_tick(&direct,reference+(frame+64)*2);
    }
    REQUIRE(direct.errors==0);engine_close(&direct);
    REQUIRE(memcmp(a,reference,sizeof(a))==0);
    printf("rate %d: irregular partitions and direct libpd PCM match exactly; ceil-to-tick events + 64-frame buffering\n",rate);
}
static void recall_check(int rate) {
    float audio[N*2],a[N*2],b[N*2];unsigned char bytes[STATE_BYTES],again[STATE_BYTES];State state,before,after;
    Effect *e=create(rate);
    for(unsigned i=0;i<5;i++)midi(e,0x90,notes[i],100,i*128);
    for(unsigned p=0;p<1024;p+=256)process(e,audio,p,256);
    chunk(e,bytes,&before);
    for(unsigned voice=0;voice<4;voice++)REQUIRE(before.held[voice*7]>.2f);
    for(unsigned voice=0;voice<4;voice++)e->set(e,15+voice,1);
    e->set(e,0,.81f);e->set(e,8,.1f);
    for(unsigned i=0;i<5;i++)midi(e,0x90,notes[i],100,i*128);
    for(unsigned p=0;p<1024;p+=256)process(e,audio,p,256);
    chunk(e,bytes,&state);REQUIRE(memcmp(state.held,before.held,sizeof(state.held))==0);
    close_effect(e);
    Effect *r=create(rate);
    REQUIRE(r->dispatch(r,24,0,STATE_BYTES,bytes,0)==0); /* active loading forbidden */
    r->dispatch(r,12,0,0,NULL,0);REQUIRE(r->dispatch(r,24,0,STATE_BYTES,bytes,0)==1);
    process(r,audio,0,1024);REQUIRE(energy(audio,1024)==0); /* restore paused */
    chunk(r,again,&after);REQUIRE(!memcmp(bytes,again,STATE_BYTES));
    r->dispatch(r,12,0,1,NULL,0);process(r,audio,0,1024);REQUIRE(energy(audio,1024)==0);
    /* Two freshly recalled instances start from the same declared seed/history
       policy. Idle DSP advancement is held equal before their musical script. */
    Effect *r2=create(rate);r2->dispatch(r2,12,0,0,NULL,0);REQUIRE(r2->dispatch(r2,24,0,STATE_BYTES,bytes,0)==1);
    r2->dispatch(r2,12,0,1,NULL,0);process(r2,audio,0,1024);
    render(r,a,1,0);render(r2,b,0,0);REQUIRE(!memcmp(a,b,sizeof(a)));REQUIRE(energy(a,N)>1e-5);
    chunk(r,again,&after);REQUIRE(!memcmp(state.held,after.held,sizeof(state.held)));
    for(unsigned voice=0;voice<4;voice++)r->set(r,15+voice,0);
    for(unsigned i=0;i<5;i++)midi(r,0x90,notes[i],100,i*128);
    for(unsigned p=0;p<1024;p+=256)process(r,audio,p,256);
    chunk(r,again,&after);
    for(unsigned voice=0;voice<4;voice++)REQUIRE(after.held[voice*7]>=.81f && after.held[voice*7]!=state.held[voice*7]);
    /* Invalid package identity never mutates a live state. */
    r->dispatch(r,12,0,0,NULL,0);
    bytes[12]^=1;REQUIRE(r->dispatch(r,24,0,STATE_BYTES,bytes,0)==0);
    close_effect(r);close_effect(r2);
    printf("rate %d: all four locks latch/hold/unlock; coherent fresh chunk restore stays paused and silent, then musical PCM repeats\n",rate);
}
static void isolation_check(int rate) {
    float a[N*2],b[N*2],isolated[N*2];Effect *left=create(rate),*right=create(rate);
    right->set(right,0,.73f);right->set(right,6,.57f);
    unsigned next=0;
    for(unsigned p=0;p<N;p+=256) {
        while(next<sizeof(times)/sizeof(times[0])&&times[next]<p+256) {
            midi(left,0x90,notes[next%5],100,times[next]-p);
            midi(right,0x90,notes[(next+2)%5],100,times[next]-p);next++;
        }
        process(left,a,p,256);process(right,b,p,256);
    }
    close_effect(left);close_effect(right);
    Effect *alone=create(rate);render(alone,isolated,0,0);REQUIRE(!memcmp(a,isolated,sizeof(a)));close_effect(alone);
    alone=create(rate);alone->set(alone,0,.73f);alone->set(alone,6,.57f);render(alone,isolated,0,2);REQUIRE(!memcmp(b,isolated,sizeof(b)));close_effect(alone);
    printf("rate %d: differently controlled interleaved instances equal their isolated executions\n",rate);
}
static void pending_panic_check(int rate) {
    float audio[N*2];Effect *e=create(rate);
    /* Host event offset can outlive a short process call. */
    midi(e,0x90,36,100,65);process(e,audio,0,1);process(e,audio,1,62);process(e,audio,63,129);
    REQUIRE(energy(audio,192)==0);process(e,audio,192,1024);REQUIRE(energy(audio+192*2,1024)>1e-5);
    midi(e,0x90,38,100,700);midi(e,0xb0,123,0,0);process(e,audio,0,1024);
    /* Next tick boundary may be up to 63 frames away; after it output is silent
       and the future note was cleared, without invoking the open-hat receiver. */
    REQUIRE(energy(audio+64*2,960)==0);
    process(e,audio,0,1024);REQUIRE(energy(audio,1024)==0);close_effect(e);
    printf("rate %d: pending offset survives small blocks; panic silences and clears future notes\n",rate);
}
int main(int argc,char **argv) {
    if(argc!=3){fprintf(stderr,"usage: component-host MODULE PATCH_DIRECTORY\n");return 2;}
    struct sigaction action={0},observed;action.sa_handler=signal_handler;sigemptyset(&action.sa_mask);REQUIRE(!sigaction(SIGFPE,&action,NULL));
    void *module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!module){fprintf(stderr,"%s\n",dlerror());return 1;}
    factory=(Factory)dlsym(module,"VSTPluginMain");REQUIRE(factory);
    for(unsigned rate=44100;rate<=48000;rate+=3900){voices_check(rate);direct_check(rate,argv[2]);recall_check(rate);isolation_check(rate);pending_panic_check(rate);}
    REQUIRE(!sigaction(SIGFPE,NULL,&observed)&&observed.sa_handler==signal_handler);
    dlclose(module);printf("PASS: real libpd/VST2 component checks; SIGFPE owner preserved. No MPC host exercised.\n");return 0;
}
