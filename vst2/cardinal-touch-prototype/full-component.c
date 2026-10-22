/* Finite independent public VST2 driver. No PCM, transport thread or scheduler.
 * The native MPC host remains the musical acceptance boundary. */
#define _GNU_SOURCE
#include "../chunk-probe/abi.h"
#include "full-touch-api.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

static void require(int okay,const char* message) {
    if (!okay) { fprintf(stderr,"FAIL %s\n",message); exit(1); }
}
static unsigned hostBegins,hostSets,hostEnds;
static Effect* changedOwner;static float changedValue;
static intptr_t host(Effect* effect,int32_t op,int32_t index,intptr_t value,void* data,float option) {
    (void)effect;(void)index;(void)value;(void)data;(void)option;
    if(op==43)++hostBegins;
    if(op==44)++hostEnds;
    if(op==0){++hostSets;changedOwner=effect;changedValue=option;}
    return op==1?2400:op==16?44100:op==17?256:0;
}
static void image(const MpcCardinalTouchApi* api,uint64_t token,const char* path) {
    unsigned char* pixels=malloc(960*480*4);uint32_t revision=0;
    require(pixels!=NULL,"pixel allocation");
    require(api->pixels(token,pixels,960*480*4,&revision),"owner image");
    FILE* file=fopen(path,"wbx");require(file!=NULL,"exclusive image output");
    require(fprintf(file,"P6\n960 480\n255\n")>0,"image header");
    for(unsigned i=0;i<960*480;++i)require(fwrite(pixels+i*4,1,3,file)==3,"image row");
    require(!fclose(file),"image close");free(pixels);
    printf("IMAGE %s revision=%u\n",path,revision);
}
int main(int argc,char** argv) {
    if(argc!=3&&argc!=7){fprintf(stderr,"Usage: full-component plugin.so --load-only | plugin.so patch.vcv rack.ppm browser.ppm expected-modules expected-cables\n");return 2;}
    alarm(180);setvbuf(stdout,NULL,_IONBF,0);
    const struct rlimit core={0,0};require(!setrlimit(RLIMIT_CORE,&core),"no core files");
    void* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library)fprintf(stderr,"%s\n",dlerror());
    require(library!=NULL,"actual module load");
    Factory factory=(Factory)dlsym(library,"VSTPluginMain");
    const MpcCardinalTouchApi*(*get)(uint32_t)=(const MpcCardinalTouchApi*(*)(uint32_t))dlsym(library,"mpclearn_cardinal_touch_api");
    require(factory&&get,"VST2 and view exports");
    const MpcCardinalTouchApi* api=get(MPC_CARDINAL_TOUCH_ABI);
    require(api&&api->size==sizeof(*api)&&!get(0),"exact view ABI");
    require(api->instances(NULL,0)==0,"API discovery creates no owner");
    if(argc==3){require(!strcmp(argv[2],"--load-only"),"load-only argument");require(!dlclose(library),"module unload");puts("PASS load-only; no factory, GPU or MPC boundary exercised");return 0;}
    Effect* first=factory(host);require(first&&first->magic==0x56737450,"actual first factory");
    require(!(first->flags&1),"no desktop custom-editor advertisement");
    printf("VST uid=0x%08x version=%d inputs=%d outputs=%d parameters=%d flags=0x%x\n",
        (unsigned)first->uid,first->version,first->inputs,first->outputs,first->params,(unsigned)first->flags);
    first->dispatch(first,0,0,0,NULL,0);
    uint32_t plugins=0,models=0;api->catalog(&plugins,&models);
    printf("CATALOG plugins=%u models=%u\n",plugins,models);
    require(plugins>50&&models>500,"full bundled catalog initialized, not Mini or test modules");
    MpcCardinalInstance rows[4];require(api->instances(rows,4)==1,"one registered owner");
    const uint64_t token=rows[0].token;printf("OWNER %llu %s modules=%u cables=%u\n",(unsigned long long)token,rows[0].label,rows[0].modules,rows[0].cables);
    require(api->open(token,960,480),"same-owner view open");
    require(api->action(token,MPC_CARDINAL_LOAD,argv[2]),"canonical patch load");
    char *moduleEnd=NULL,*cableEnd=NULL;
    unsigned long expectedModules=strtoul(argv[5],&moduleEnd,10),expectedCables=strtoul(argv[6],&cableEnd,10);
    require(*argv[5]&&*argv[6]&&moduleEnd&&!*moduleEnd&&cableEnd&&!*cableEnd&&expectedModules>2&&expectedModules<=10000&&expectedCables<=100000,"independently supplied patch counts");
    require(api->instances(rows,4)==1&&rows[0].modules==expectedModules&&rows[0].cables==expectedCables,"complete source patch graph, no silently missing modules/cables");
    const uint32_t modules=rows[0].modules,cables=rows[0].cables;
    MpcCardinalHostParameters hp={0};
    require(api->host_parameters(token,MPC_CARDINAL_HOST_OPEN,&hp)&&hp.binding&&hp.count==24,"actual host parameter page");
    uint64_t oldBinding=hp.binding;
    first->set(first,0,0.2f);
    require(api->host_parameters(token,MPC_CARDINAL_HOST_READ,&hp)&&fabsf(hp.parameters[0].value-.2f)<1e-6f,"external host value reads through existing owner");
    require(!strcmp(hp.parameters[0].name,"Parameter 1"),"canonical host parameter name");
    require(!api->host_parameter(token,hp.binding,0,2,.5f)&&!api->host_parameter(token,hp.binding,0,1,NAN),"unstarted and nonfinite input refused");
    unsigned begins=hostBegins,sets=hostSets,ends=hostEnds;
    require(api->host_parameter(token,hp.binding,0,1,0)&&api->host_parameter(token,hp.binding,0,2,.7f)&&api->host_parameter(token,hp.binding,0,3,0),"begin/set/end through existing DPF wrapper");
    require(hostBegins==begins+1&&hostSets==sets+1&&hostEnds==ends+1&&changedOwner==first&&fabsf(changedValue-.7f)<1e-6f&&fabsf(first->get(first,0)-.7f)<1e-6f,"canonical normalized storage and host notifications");
    require(api->host_parameters(token,MPC_CARDINAL_HOST_CLOSE,&hp)&&!api->host_parameter(token,oldBinding,0,1,0),"closed page rejects stale gesture");
    require(api->host_parameters(token,MPC_CARDINAL_HOST_OPEN,&hp)&&hp.binding!=oldBinding&&!api->host_parameter(token,oldBinding,0,1,0),"new page rejects preceding binding");
    require(api->host_parameters(token,MPC_CARDINAL_HOST_CLOSE,&hp),"host page settled");
    image(api,token,argv[3]);
    require(api->action(token,MPC_CARDINAL_SAVE,NULL),"canonical save");
    require(api->action(token,MPC_CARDINAL_BROWSER,NULL),"canonical module browser");
    image(api,token,argv[4]);
    void* chunk=NULL;intptr_t size=first->dispatch(first,23,0,0,&chunk,0);
    require(size>0&&size<128*1024*1024&&chunk,"owner host chunk");
    void* copy=malloc(size);require(copy!=NULL,"chunk copy");memcpy(copy,chunk,size);
    Effect* second=factory(host);require(second&&second!=first,"independent second owner");second->dispatch(second,0,0,0,NULL,0);
    require(api->instances(rows,4)==2,"explicit two-owner list");
    uint64_t secondToken=rows[0].token==token?rows[1].token:rows[0].token;
    require(secondToken!=token&&api->open(secondToken,960,480),"explicit second selection");
    require(fabsf(second->get(second,0))<1e-6f&&fabsf(first->get(first,0)-.7f)<1e-6f,"two actual plugin parameter owners stay separate");
    second->dispatch(second,24,0,size,copy,0);free(copy);
    require(fabsf(second->get(second,0)-.7f)<1e-6f,"canonical VST chunk recalls host parameter");
    require(api->instances(rows,4)==2,"both owners after host chunk recall");
    unsigned secondIndex=rows[0].token==secondToken?0:1;
    require(rows[secondIndex].modules==modules&&rows[secondIndex].cables==cables,"host chunk canonical graph recall");
    require(api->host_parameters(token,MPC_CARDINAL_HOST_OPEN,&hp),"open page before plugin close");oldBinding=hp.binding;
    api->close(token);first->dispatch(first,1,0,0,NULL,0);
    require(!api->host_parameter(token,oldBinding,0,1,0)&&!api->host_parameters(token,MPC_CARDINAL_HOST_READ,&hp),"retired exact owner rejects page and input");
    require(api->instances(rows,4)==1&&!api->open(token,960,480),"destroy retires exact token");
    require(api->action(secondToken,MPC_CARDINAL_LOAD,"/tmp/mpclearn-cardinal-touch/state/prototype.vcv"),"unchanged owner file fresh owner recall");
    require(api->instances(rows,4)==1&&rows[0].modules==modules&&rows[0].cables==cables,"fresh owner graph counts");
    api->close(secondToken);second->dispatch(second,1,0,0,NULL,0);
    require(api->instances(NULL,0)==0&&!api->open(secondToken,960,480),"all retired");
    require(!dlclose(library),"final library release");
    printf("PASS actual plugin graph modules=%u cables=%u, browser, two owners, host chunk and fresh owner file recall; component substitutes for MPC\n",modules,cables);
    return 0;
}
