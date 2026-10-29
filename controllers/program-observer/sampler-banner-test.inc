#ifdef MODEL_SAMPLER_PHYSICAL
static unsigned banner_images,banner_releases,banner_handles,banner_shows,banner_throw;
static void banner_image(void *p){*(uint32_t*)p=0;banner_images++;}
static void banner_release(void *){banner_releases++;}
static void banner_handle(void *){banner_handles++;}
static void banner_show(void *out,void *overlay,const void *text,const void *icon,unsigned duration){
 require(out&&overlay&&duration==2000&&*(const uint32_t*)icon==0,"banner uses native empty image and bounded duration");
 const char *const *strings=(const char *const*)text;
 require(!strcmp(strings[0],"Sampler")&&(!strcmp(strings[1],"Recording")||!strcmp(strings[1],"Stopped")||!strcmp(strings[1],"Sampling unavailable")),"banner presents native-state outcome, not button intent");
 if(banner_throw)throw 1;
 banner_shows++;
}
static void sampler_banner_checks(){
 sampler_banner_host={test_string_ctor,test_string_dtor,banner_image,banner_release,banner_show,banner_handle};
 alignas(4) unsigned char overlay[0xa4]={};*(uint32_t*)overlay=MODEL_SAMPLER_BANNER_VPTR;
 /* Match command-capture's real token() ABI rather than the adapter helper. */
 unsigned thread;__asm__ volatile("mrc p15,0,%0,c13,c0,3":"=r"(thread));
 unsigned tid=(unsigned)syscall(224),before_ctor=string_ctors,before_dtor=string_dtors;
 require(thread!=tid,"ARM command owner token differs from Linux TID");
 require(!native_sampler_banner((uint32_t)(uintptr_t)overlay,0,tid,1)&&!banner_shows,"Linux TID cannot substitute for the command owner's ARM token");
 for(unsigned message=1;message<=3;message++)require(native_sampler_banner((uint32_t)(uintptr_t)overlay,0,thread,message)==1,"production native banner adapter accepts observed status");
 require(banner_shows==3&&banner_handles==3&&banner_images==3&&banner_releases==3&&string_ctors-before_ctor==6&&string_dtors-before_dtor==6,"banner transfers native ownership and balances only temporary objects");
 banner_throw=1;require(!native_sampler_banner((uint32_t)(uintptr_t)overlay,0,thread,1),"factory failure does not report displayed feedback");banner_throw=0;
 require(banner_handles==3&&banner_images==4&&banner_releases==4&&string_ctors-before_ctor==8&&string_dtors-before_dtor==8,"failed factory cleans inputs without destroying unconstructed result");
 require(!native_sampler_banner((uint32_t)(uintptr_t)overlay,0,thread+1,1),"foreign lane refuses before native UI work");
 *(uint32_t*)overlay=0;require(!native_sampler_banner((uint32_t)(uintptr_t)overlay,0,thread,1)&&banner_shows==3,"stale overlay refuses before native UI work");
 printf("PASS native sampler banner adapter: thread/type guards, copied text/native image,2000ms and temporary/exception cleanup; actual Akai widgets/render/timer substituted\n");
}
#else
static void sampler_banner_checks(){}
#endif
