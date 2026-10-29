/* Existing Akai timed banner. Inputs are copied by native constructors; no CE
 * callback or UI pointer survives this synchronous qualified-UI call. */
#ifdef MODEL_FIRMWARE_310
#include "model-layout.h"
#endif
#ifdef MODEL_SAMPLER_PHYSICAL
namespace {
static uint32_t sampler_banner_thread_token(){
 uint32_t token;__asm__ volatile("mrc p15,0,%0,c13,c0,3":"=r"(token));return token;
}
struct SamplerBannerHost {
 void (*string_ctor)(void*,const char*);
 void (*string_dtor)(void*);
 void (*image_ctor)(void*);
 void (*image_dtor)(void*);
 void (*show)(void*,void*,const void*,const void*,unsigned);
 void (*tracking_dtor)(void*);
};
#ifdef SAMPLER_BANNER_COMPONENT
static SamplerBannerHost sampler_banner_host;
#endif
}
#endif
extern "C" unsigned native_sampler_banner(uint32_t overlay,uint32_t bias,uint32_t owner_token,unsigned message){
#ifdef MODEL_SAMPLER_PHYSICAL
 /* COMMAND_DRAIN supplies its ARM thread-pointer token, not a Linux TID. */
 if(!overlay||overlay>0xffffff5bu||message<1||message>3||owner_token!=sampler_banner_thread_token()||!__atomic_load_n(&observer_running,__ATOMIC_ACQUIRE))return 0;
 if(*(const uint32_t*)(uintptr_t)overlay!=bias+MODEL_SAMPLER_BANNER_VPTR)return 0;
#ifndef SAMPLER_BANNER_COMPONENT
 SamplerBannerHost h={
  (void(*)(void*,const char*))(uintptr_t)(bias+MPC_UI_RVA_STRING_CTOR),
  (void(*)(void*))(uintptr_t)(bias+MPC_UI_RVA_STRING_DTOR),
  (void(*)(void*))(uintptr_t)(bias+MODEL_SAMPLER_BANNER_IMAGE_CTOR),
  (void(*)(void*))(uintptr_t)(bias+MODEL_SAMPLER_BANNER_IMAGE_DTOR),
  (void(*)(void*,void*,const void*,const void*,unsigned))(uintptr_t)(bias+MODEL_SAMPLER_BANNER_SHOW),
  (void(*)(void*))(uintptr_t)(bias+MODEL_SAMPLER_BANNER_TRACKING_DTOR)};
#else
 SamplerBannerHost h=sampler_banner_host;
#endif
 alignas(4) unsigned char text[8]={},icon[4]={},tracking[16]={};unsigned live=0,result=0;
 const char *messages[]={"","Recording","Stopped","Sampling unavailable"};
 try{
  h.string_ctor(text,"Sampler");live|=1;h.string_ctor(text+4,messages[message]);live|=2;
  h.image_ctor(icon);live|=4;
  h.show(tracking,(void*)(uintptr_t)overlay,text,icon,2000u);live|=8;result=1;
 }catch(...){result=0;}
 /* Native complete destructors only: the returned handle is stack storage,
  * and unregistering it does not destroy the overlay-owned timed banner. */
 if(live&8)try{h.tracking_dtor(tracking);}catch(...){result=0;}
 if(live&4)try{h.image_dtor(icon);}catch(...){result=0;}
 if(live&2)try{h.string_dtor(text+4);}catch(...){result=0;}
 if(live&1)try{h.string_dtor(text);}catch(...){result=0;}
 return result;
#else
 (void)overlay;(void)bias;(void)owner_token;(void)message;return 0;
#endif
}
