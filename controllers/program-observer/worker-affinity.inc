/* Az01's worker-initialization virtual call; admitted once before main.
 * Keep the native call for every ordinal. Only worker zero gains eligibility
 * on all four cores, so it can leave the USB/host core when that core is busy. */
#include <sched.h>
/* Az01 vtable, its slot 7 and the slot's thunk; typeinfo, name, slot 6,
 * the thunk's branch word and the end of GNU_RELRO for each MPC profile. */
#ifdef MODEL_FIRMWARE_310
#define WORKER_VTABLE 0x06d33240u
#define WORKER_SLOT 0x06d3325cu
#define WORKER_METHOD 0x022c88a8u
#define WORKER_TYPEINFO 0x06d3322cu
#define WORKER_NAME 0x0510efc4u
#define WORKER_PEER 0x022c88a0u
#define WORKER_BRANCH 0xea00c2abu
#define WORKER_RELRO_END 0x06eb9000u
#else
#define WORKER_VTABLE 0x06917844u
#define WORKER_SLOT 0x06917860u
#define WORKER_METHOD 0x01f69098u
#define WORKER_TYPEINFO 0x06917830u
#define WORKER_NAME 0x04e1a854u
#define WORKER_PEER 0x01f69090u
#define WORKER_BRANCH 0xea00c747u
#define WORKER_RELRO_END 0x06aa7000u
#endif
static uint32_t worker_bias;
static pid_t worker_process;
static int worker_installed;
typedef void (*WorkerAffinityMethod)(void *,unsigned);
static WorkerAffinityMethod worker_original;
static void worker_affinity(void *platform,unsigned ordinal){
 worker_original(platform,ordinal);
 if(ordinal||getpid()!=worker_process||!platform||
    *(const uint32_t*)platform!=worker_bias+WORKER_VTABLE)return;
 cpu_set_t wanted,actual;CPU_ZERO(&wanted);
 for(unsigned i=0;i<4;i++)CPU_SET(i,&wanted);
 if(pthread_setaffinity_np(pthread_self(),sizeof(wanted),&wanted)||
    pthread_getaffinity_np(pthread_self(),sizeof(actual),&actual)||
    !CPU_EQUAL(&wanted,&actual))worker_original(platform,ordinal);
}
/* The executable's PT_LOAD is RW, but this page is inside GNU_RELRO. Require
 * the actual relocated image mapping to be read-only before opening it. */
static int worker_readonly_page(uintptr_t page){
 FILE *f=fopen("/proc/self/maps","r");if(!f)return 0;
 char line[512],perms[5];unsigned long lo,hi;int used,ok=0;unsigned rows=0;
 while(rows++<4096&&fgets(line,sizeof(line),f)){
  used=0;
  if(sscanf(line,"%lx-%lx %4s %*s %*s %*s %n",&lo,&hi,perms,&used)==3&&
     used>0&&lo<=page&&page+4096<=hi&&!strcmp(perms,"r--p")&&
     !strcmp(line+used,"/usr/bin/MPC\n")){ok=1;break;}
 }
 fclose(f);return ok;
}
static void worker_affinity_restore(void){
 if(!worker_installed)return;
 uintptr_t page=((uintptr_t)worker_bias+WORKER_SLOT)&~(uintptr_t)4095;
 if(mprotect((void*)page,4096,PROT_READ|PROT_WRITE))_exit(125);
 *(uint32_t*)(uintptr_t)(worker_bias+WORKER_SLOT)=(uint32_t)(uintptr_t)worker_original;
 if(mprotect((void*)page,4096,PROT_READ))_exit(125);
 worker_installed=0;
}
static int worker_affinity_install(uint32_t bias){
 if(worker_installed||!bias||(bias&4095)||bias>UINT32_MAX-WORKER_RELRO_END)return 0;
 uintptr_t page=((uintptr_t)bias+WORKER_SLOT)&~(uintptr_t)4095;
 if(!worker_readonly_page(page))return 0;
 if(*(const uint32_t*)(uintptr_t)(bias+WORKER_SLOT)!=bias+WORKER_METHOD||
    *(const uint32_t*)(uintptr_t)(bias+WORKER_VTABLE-8)!=0||
    *(const uint32_t*)(uintptr_t)(bias+WORKER_VTABLE-4)!=bias+WORKER_TYPEINFO||
    *(const uint32_t*)(uintptr_t)(bias+WORKER_TYPEINFO+4)!=bias+WORKER_NAME||
    memcmp((const void*)(uintptr_t)(bias+WORKER_NAME),"4Az01",6)||
    *(const uint32_t*)(uintptr_t)(bias+WORKER_VTABLE+0x18)!=bias+WORKER_PEER||
    *(const uint32_t*)(uintptr_t)(bias+WORKER_METHOD)!=WORKER_BRANCH)return 0;
 worker_bias=bias;worker_process=getpid();
 worker_original=(WorkerAffinityMethod)(uintptr_t)(bias+WORKER_METHOD);
 if(mprotect((void*)page,4096,PROT_READ|PROT_WRITE))return 0;
 *(uint32_t*)(uintptr_t)(bias+WORKER_SLOT)=(uint32_t)(uintptr_t)worker_affinity;
 worker_installed=1;
 if(mprotect((void*)page,4096,PROT_READ)){worker_affinity_restore();return 0;}
 return 1;
}
