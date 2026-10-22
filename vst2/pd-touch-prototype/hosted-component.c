/* Actual ARM VST2 composition; this finite offline host substitutes for MPC. */
#define _GNU_SOURCE
#include "../chunk-probe/abi.h"
#include "hosted-api.h"
#include <dlfcn.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
static void require(int okay,const char* message){if(!okay){fprintf(stderr,"FAIL %s\n",message);exit(1);}}
static intptr_t host(Effect* e,int32_t op,int32_t index,intptr_t value,void* data,float option){(void)e;(void)index;(void)value;(void)data;(void)option;return op==1?2400:op==16?44100:op==17?256:0;}
static unsigned get32(const unsigned char* p){return (unsigned)p[0]|(unsigned)p[1]<<8|(unsigned)p[2]<<16|(unsigned)p[3]<<24;}
static const unsigned blocks[]={113,256,17,512,64,127};
static void cached_frames(const MpcPdHostedApi* api,const MpcPdHostedApi* legacy,uint64_t token){
    const unsigned bytes=960*480*4;
    unsigned char* pixels=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    unsigned char* original=malloc(bytes);require(pixels!=MAP_FAILED&&original,"frame buffers");
    uint32_t revision=0;require(api->pixels(token,pixels,bytes,&revision)&&revision,"first complete cached frame");
    memcpy(original,pixels,bytes);uint32_t retained=revision;
    require(!mprotect(pixels,bytes,PROT_READ),"protect caller's retained pixels");
    require(api->pixels(token,pixels,bytes,&revision)&&revision==retained,"unchanged frame performs no destination writes");
    require(!mprotect(pixels,bytes,PROT_READ|PROT_WRITE),"restore writable destination");
    memset(pixels,0xa5,bytes);require(legacy->pixels(token,pixels,bytes,&revision)&&revision==retained&&!memcmp(pixels,original,bytes),"legacy ABI5 still exports unconditionally");
    require(api->view(token,MPC_EDITOR_VIEW_PAN,17,13,0)&&api->pixels(token,pixels,bytes,&revision)&&revision!=retained&&memcmp(pixels,original,bytes),"pan publishes a changed complete frame");
    api->close(token);require(api->open(token,960,480),"cached view reopen");
    memset(pixels,0xa5,bytes);revision=0;require(api->pixels(token,pixels,bytes,&revision)&&revision&&pixels[3]==255,"reopen starts with fresh pixels");
    require(api->open(token,640,400),"cached view resize");
    memset(pixels,0xa5,bytes);revision=0;require(api->pixels(token,pixels,640*400*4,&revision)&&revision&&pixels[3]==255&&pixels[640*400*4]==0xa5,"resize refreshes exact destination bounds");
    require(api->open(token,960,480),"restore initial dimensions");
    require(!munmap(pixels,bytes),"release frame buffer");free(original);
}
static void render(Effect* effect,float* samples,int rate,pthread_barrier_t* gate){
    unsigned done=0,iteration=0;float left[512],right[512];float* out[]={left,right};
    while(done<8192){unsigned count=blocks[iteration++%6];if(count>8192-done)count=8192-done;
        if(gate)pthread_barrier_wait(gate);
        effect->replacing(effect,NULL,out,count);
        if(gate)pthread_barrier_wait(gate);
        for(unsigned i=0;i<count;++i){require(isfinite(left[i])&&isfinite(right[i]),"finite audio");samples[2*(done+i)]=left[i];samples[2*(done+i)+1]=right[i];}done+=count;}
    double maximum=0;
    for(unsigned i=0;i<8192;++i){double expected=i<64?0:.05*cos(2*3.14159265358979323846*440*(i-64)/rate);double error=fabs(samples[2*i]-expected);if(error>maximum)maximum=error;require(samples[2*i+1]==0,"upstream A01 left-only output preserved");}
    printf("A01 %dHz: actual8192frames irregular host blocks, advertised64-frame delay, cosine maxerror=%.9g\n",rate,maximum);
    require(maximum<0.00002,"unchanged upstream oscillator amplitude/frequency and 64-frame routing");
}
typedef struct {Effect* effect;float* samples;int rate;pthread_barrier_t gate;} AudioTrial;
static void* audio_trial(void* opaque){AudioTrial* trial=opaque;render(trial->effect,trial->samples,trial->rate,&trial->gate);return NULL;}
static void render_with_view(const MpcPdHostedApi* api,uint64_t token,Effect* effect,float* samples,int rate){
    AudioTrial trial={.effect=effect,.samples=samples,.rate=rate};pthread_t thread;
    unsigned char* pixels=malloc(960*480*4);require(pixels!=NULL,"view capture allocation");
    require(!pthread_barrier_init(&trial.gate,NULL,2)&&!pthread_create(&thread,NULL,audio_trial,&trial),"concurrent actual audio/view threads");
    unsigned done=0,iteration=0;uint32_t revision=0;
    while(done<8192){unsigned count=blocks[iteration%6];if(count>8192-done)count=8192-done;
        pthread_barrier_wait(&trial.gate);
        if(iteration==11){api->close(token);require(api->open(token,960,480),"reopen the same live view during audio");revision=0;}
        require(api->view(token,MPC_EDITOR_VIEW_PAN,iteration%2?3:-3,2,0)&&
            api->pixels(token,pixels,960*480*4,&revision)&&revision,"pan and raster while actual DSP runs");
        uint32_t retained=revision;
        require(api->pixels(token,pixels,960*480*4,&revision)&&revision==retained,"unchanged owner pixels retain revision");
        pthread_barrier_wait(&trial.gate);done+=count;++iteration;
    }
    require(!pthread_join(thread,NULL)&&!pthread_barrier_destroy(&trial.gate),"audio/view trial closes");free(pixels);
    printf("VIEW %u concurrent block/raster pairs, one live view close/reopen; no native scheduling claim\n",iteration);
}
int main(int argc,char** argv){
    require(argc==4,"usage: hosted-component module.so unchanged-A01.pd output-prefix");alarm(60);setvbuf(stdout,NULL,_IONBF,0);
    void* module=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);if(!module)fprintf(stderr,"%s\n",dlerror());require(module!=NULL,"actual module load");
    Factory factory=(Factory)dlsym(module,"VSTPluginMain");const MpcPdHostedApi*(*get)(uint32_t)=(const MpcPdHostedApi*(*)(uint32_t))dlsym(module,"mpclearn_pd_hosted_api");
    require(factory&&get,"exports");const MpcPdHostedApi* api=get(MPC_PD_HOSTED_ABI);require(api&&api->size==sizeof(*api)&&!get(0)&&!api->instances(NULL,0),"empty discovery creates no Pd owner");
    float *original=malloc(8192*2*sizeof(float)),*recalled=malloc(8192*2*sizeof(float));require(original&&recalled,"audio capture allocation");
    for(unsigned pass=0;pass<2;++pass){int rate=pass?48000:44100;
        Effect* first=factory(host);require(first&&!(first->flags&1)&&first->delay==64&&(first->inputs==0||first->inputs==2)&&first->outputs==2,"actual synth/FX ABI, no custom editor");
        first->dispatch(first,0,0,0,NULL,0);require(first->dispatch(first,10,0,0,NULL,rate),"host sample rate");
        MpcPdInstance rows[4];require(api->instances(rows,4)==1,"one owner");uint64_t token=rows[0].token;
        require(api->open(token,960,480)&&api->load(token,argv[2]),"borrowed canvas loads unchanged real sketch");
        require(get(5)&&get(5)->size==sizeof(*api),"legacy ABI5 remains available");cached_frames(api,get(5),token);
        char status[512];require(api->status(token,status,sizeof(status)),"owner caption");printf("OWNER %s\n",status);
        require(api->action(token,5),"canonical save intent");
        void* state=NULL;intptr_t size=first->dispatch(first,23,0,0,&state,0);require(size>20&&state&&!memcmp(state,"MPPDST1",8),"canonical host chunk");
        unsigned char* copy=malloc(size);require(copy!=NULL,"retained owner chunk");memcpy(copy,state,size);
        {char path[512];snprintf(path,sizeof(path),"/tmp/mpclearn-pd-touch/state/instance.%llu/prototype.pd",(unsigned long long)(token*2+(first->inputs?1:0)));
         FILE* f=fopen(path,"rb");require(f!=NULL,"canonical save file exists");unsigned length=get32(copy+16),pathLength=get32(copy+12);
         unsigned char* saved=malloc(length);require(saved&&fread(saved,1,length,f)==length&&fgetc(f)==EOF&&!fclose(f),"complete canonical save file");
         require(!memcmp(saved,copy+20+pathLength,length),"owner save and host payload byte-identical");free(saved);}
        if(!pass){char path[1024];snprintf(path,sizeof(path),"%s.chunk",argv[3]);FILE* f=fopen(path,"wbx");require(f&&fwrite(copy,1,size,f)==(size_t)size&&!fclose(f),"exclusive chunk evidence");
            unsigned pathLength=get32(copy+12),length=get32(copy+16);require(20u+pathLength+length==(unsigned)size,"canonical payload bounds");
            snprintf(path,sizeof(path),"%s.pd",argv[3]);f=fopen(path,"wbx");require(f&&fwrite(copy+20+pathLength,1,length,f)==length&&!fclose(f),"unchanged canonical payload evidence");}
        require(first->dispatch(first,12,0,1,NULL,0),"host activates DSP");render(first,original,rate,NULL);first->dispatch(first,12,0,0,NULL,0);
        Effect* second=factory(host);require(second!=NULL,"fresh second owner");second->dispatch(second,0,0,0,NULL,0);require(second->dispatch(second,10,0,0,NULL,rate),"fresh host rate");
        require(api->instances(rows,4)==2,"explicit two owners");uint64_t secondToken=rows[0].token==token?rows[1].token:rows[0].token;
        require(secondToken!=token&&second->dispatch(second,24,0,size,copy,0),"fresh owner restores untouched chunk");
        void* after=NULL;intptr_t afterSize=second->dispatch(second,23,0,0,&after,0);require(afterSize==size&&!memcmp(after,copy,size),"canonical state roundtrip unchanged");
        api->close(token);first->dispatch(first,1,0,0,NULL,0);require(api->instances(rows,4)==1&&!api->open(token,960,480),"destroy retires exact token");
        require(api->open(secondToken,960,480),"fresh same-owner canvas");require(second->dispatch(second,12,0,1,NULL,0),"fresh host activates DSP");render_with_view(api,secondToken,second,recalled,rate);
        require(!memcmp(original,recalled,8192*2*sizeof(float)),"fresh canonical recall audio byte-identical");
        afterSize=second->dispatch(second,23,0,0,&after,0);require(afterSize==size&&!memcmp(after,copy,size),"view navigation/raster/reopen preserve canonical sound state");free(copy);
        second->dispatch(second,12,0,0,NULL,0);api->close(secondToken);second->dispatch(second,1,0,0,NULL,0);require(!api->instances(NULL,0),"no retained owner after close");
    }
    free(original);free(recalled);require(!dlclose(module),"module unload");puts("PASS real Pd sketch/audio/canonical recall in finite ARM host; MPC, physical audio and native UI remain unobserved");return 0;
}
