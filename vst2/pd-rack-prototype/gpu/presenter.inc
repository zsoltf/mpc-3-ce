#ifndef CARDINAL_PRESENTER_DECL
#define CARDINAL_PRESENTER_DECL
#include <stdint.h>
#include "overlay-api.h"
extern "C" void cardinal_presenter_init(uint32_t);
extern "C" void cardinal_presenter_ui(unsigned,uintptr_t,uint32_t);
extern "C" void cardinal_presenter_hook(void*,unsigned);
extern "C" uint64_t cardinal_presenter_open(uintptr_t);
extern "C" void cardinal_presenter_close(uint64_t);
extern "C" int cardinal_presenter_paint(uintptr_t,uintptr_t,unsigned,unsigned,uint64_t* =nullptr);
extern "C" uint64_t cardinal_presenter_paint_ticket(uintptr_t);
extern "C" int cardinal_presenter_omit(uint64_t,uint64_t,unsigned,uint32_t);
extern "C" void cardinal_presenter_cpu(uint64_t,uint64_t,int,uint32_t);
extern "C" int cardinal_presenter_skip_draw(uintptr_t,uint64_t);
extern "C" void cardinal_presenter_paint_done(uintptr_t,uint64_t,int,int);
extern "C" int cardinal_presenter_refresh_allowed(uint64_t);
extern "C" int cardinal_presenter_target(uint64_t,MpcCardinalOverlayTarget*);
extern "C" void cardinal_presenter_ready(uint64_t,unsigned,int,uint32_t);
extern "C" uintptr_t cardinal_presenter_root_candidate();
struct MpcUiControl;
extern "C" int mpc_ui_image_invalidate(MpcUiControl*);
#endif
#ifdef CARDINAL_PRESENTER_IMPLEMENTATION
#include "presenter-state.hpp"
#include "cpu-paint.hpp"
#include "atomic-compose.hpp"
#include "physical-buffers.hpp"
#include "plane-admission.hpp"
#include "visibility.hpp"
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/syscall.h>
namespace cardinal_presenter {
static State pool;static CpuPaint cpu;static bool enabled=false;
static void requestCpuRefresh();
static Plane plane;static AtomicFunctions atomic;static BufferFunctions buffer;static PlaneFunctions queries;
static uint32_t bias,tid;static uintptr_t root,producer,manager,screen,flipper;static int nativeFd=-1;static uint32_t crtc;
static bool active,text,retired,attemptedAllocation,failed,disableFailed;static uint64_t allocationGeneration=1;static Geometry geometry;
static bool refreshAfterAdmission=true,recoveryClear=false;
static AtomicAttempt attempt;static uint64_t displaySerial,pendingDisplaySerial;static bool currentCommitModified;
struct PreserveErrno{int value=errno;~PreserveErrno(){errno=value;}};
static void suppress(){pool.suppress();refreshAfterAdmission=true;cpu.revoke();}
static void fail(){failed=true;suppress();requestCpuRefresh();}
template<class F> static bool load(F& f,const char* name){f=reinterpret_cast<F>(dlsym(RTLD_DEFAULT,name));return f!=nullptr;}
static bool functions(){bool okay=true;
#define LOAD(group,member,name) okay=load(group.member,name)&&okay
 LOAD(atomic,duplicate,"drmModeAtomicDuplicate");LOAD(atomic,free,"drmModeAtomicFree");LOAD(atomic,add,"drmModeAtomicAddProperty");LOAD(atomic,commit,"drmModeAtomicCommit");
 LOAD(buffer,isMaster,"drmIsMaster");LOAD(buffer,importHandle,"drmPrimeFDToHandle");LOAD(buffer,addFramebuffer,"drmModeAddFB2");LOAD(buffer,removeFramebuffer,"drmModeRmFB");
 LOAD(queries,resources,"drmModeGetResources");LOAD(queries,freeResources,"drmModeFreeResources");LOAD(queries,planes,"drmModeGetPlaneResources");LOAD(queries,freePlanes,"drmModeFreePlaneResources");LOAD(queries,plane,"drmModeGetPlane");LOAD(queries,freePlane,"drmModeFreePlane");LOAD(queries,properties,"drmModeObjectGetProperties");LOAD(queries,freeProperties,"drmModeFreeObjectProperties");LOAD(queries,property,"drmModeGetProperty");LOAD(queries,freeProperty,"drmModeFreeProperty");
#undef LOAD
 return okay;}
static bool owner(uintptr_t candidate=0){
 if(pool.ownerLost)return false;
 uintptr_t current=word(uintptr_t(bias)+0x06b220d4u);
 if(manager&&current!=manager){pool.loseOwner();fail();return false;}
 if(!readableShape(current,0x24))return false;
 uint32_t count=word(current,0x20),capacity=word(current,0x1c);uintptr_t list=word(current,0x18);
 if(!count||count>8||count>capacity||!readableShape(list,count*4))return false;
 if(!candidate)candidate=screen;
 bool found=false;for(unsigned i=0;i<count;++i)found|=word(list,i*4)==candidate;
 if(!found||!readableShape(candidate,0x119)||word(candidate)!=bias+0x0688e7a0u)return false;
 int fd=word(candidate,0xc);uintptr_t flip=word(candidate,0x10);uint32_t controller=word(candidate,0x18);
 if(fd!=int(word(current,4))||!readableShape(flip,4)||word(flip)!=bias+0x0688e7e0u)return false;
 if(manager&&(screen!=candidate||nativeFd!=fd||crtc!=controller||flipper!=flip)){pool.loseOwner();fail();return false;}
 manager=current;screen=candidate;nativeFd=fd;crtc=controller;flipper=flip;return true;
}
static bool modalClear(){uintptr_t m=word(uintptr_t(bias)+0x06b220e0u);if(!readableShape(m,0x18)||word(m)!=bias+0x0688ea3cu||word(m,8)!=bias+0x0688ea50u)return false;
 unsigned count=word(m,0x14),capacity=word(m,0x10);uintptr_t list=word(m,0xc);if(count>8||count>capacity||(count&&!readableShape(list,count*4)))return false;
 for(unsigned i=0;i<count;++i){uintptr_t item=word(list,i*4);if(!readableShape(item,0x44)||word(item)!=bias+0x0688bf0cu||*(const unsigned char*)(item+0x40))return false;}return true;
}
static bool visible(){return enabled&&!failed&&pool.producerLive&&clearVisibility(bias,root,active,text,retired,geometry)&&modalClear();}
static void requestCpuRefresh(){
 if(!enabled)cpu.revoke();
 if(!cpu.owed||!cpu.required||cpu.queued||cpu.painting||!pool.producerLive||!producer||!active||retired)return;
 cpu.queued=true; // one canonical request until a successful CPU draw
 (void)mpc_ui_image_invalidate(reinterpret_cast<MpcUiControl*>(producer));
}
static void requestRecovery(){
 requestCpuRefresh();
 // Retained bounds are used only to request normal full-control repaint.
 // Presentation still requires a NEW exact Graphics/clip admission in paint.
 Geometry bounds=geometry;bounds.known=bounds.width&&bounds.height;
 const bool clear=enabled&&!failed&&pool.producerLive&&producer&&
     clearVisibility(bias,root,active,text,retired,bounds)&&modalClear();
 if(!clear){recoveryClear=false;return;}
 if(recoveryClear)return;
 recoveryClear=true; // before normal repaint, including possible reentry
 
 (void)mpc_ui_image_invalidate(reinterpret_cast<MpcUiControl*>(producer));
}
static void resetReleasedAllocation(){
 if(!attemptedAllocation||!pool.detached())return;
 for(const auto& b:pool.buffers)if(b.use!=Use::Free||b.allocationFd>=0||b.dmaFd>=0||b.handle||b.nativeHandle||b.framebuffer)return;
 attemptedAllocation=false;++allocationGeneration;
}
static void releaseFree(){if(pool.ownerLost||!owner())return;for(unsigned i=0;i<3;++i)if(pool.releasable(i)&&pool.buffers[i].allocationFd>=0){if(!releaseBuffer(pool.buffers[i],nativeFd,buffer))fail();}}
}
extern "C" void cardinal_presenter_init(uint32_t b){using namespace cardinal_presenter;PreserveErrno preserve;bias=b;tid=syscall(SYS_gettid);enabled=functions();}
extern "C" uint64_t cardinal_presenter_open(uintptr_t control){using namespace cardinal_presenter;PreserveErrno preserve;if(!tid||unsigned(syscall(SYS_gettid))!=tid)return 0;resetReleasedAllocation();uint64_t generation=pool.open();if(generation){cpu.open(generation);producer=control;geometry.known=false;refreshAfterAdmission=true;recoveryClear=false;}return generation;}
extern "C" void cardinal_presenter_close(uint64_t generation){using namespace cardinal_presenter;PreserveErrno preserve;if(!tid||unsigned(syscall(SYS_gettid))!=tid||generation!=pool.producerGeneration)return;cpu.close();pool.close();producer=0;geometry.known=false;if(pool.detached())releaseFree();}
extern "C" void cardinal_presenter_ui(unsigned kind,uintptr_t object,uint32_t a){using namespace cardinal_presenter;PreserveErrno preserve;if(!tid||unsigned(syscall(SYS_gettid))!=tid)return;(void)object;
 if(kind==25||kind==13){retired=true;active=false;root=0;cpu.close();pool.close();producer=0;geometry.known=false;}
 if(kind==20){active=a!=0;if(active&&!retired)root=cardinal_presenter_root_candidate();}
 if(kind==21||kind==22||kind==24||kind==30||kind==34)active=false;
 if(kind==31&&a)active=true;
 if(kind==40)text=true;
 if(kind==42||kind==43)text=false;
 if(!visible())suppress();
 requestRecovery();
}
extern "C" int cardinal_presenter_paint(uintptr_t control,uintptr_t graphics,unsigned width,unsigned height,uint64_t* ticket){using namespace cardinal_presenter;PreserveErrno preserve;if(!tid||unsigned(syscall(SYS_gettid))!=tid||control!=producer)return 0;
 const bool admitted=owner()&&canvasGeometry(bias,graphics,width,height,nativeFd,geometry);
 if(!admitted){geometry.known=false;suppress();}
 pool.visible=admitted&&visible();if(!pool.visible)suppress();
 if(!enabled||failed)cpu.revoke();
 uint64_t paint=cpu.begin(pool.visible);if(ticket)*ticket=paint;
 if(!paint){suppress();return 0;}
 // Admission can recover after the normal refresh consumed image_pending.
 // Request the existing producer exactly on that recovery, in this paint.
 const bool refresh=pool.visible&&refreshAfterAdmission;
 if(refresh){refreshAfterAdmission=false;recoveryClear=true;}
 const bool cpuRefresh=cpu.required;
 return refresh||cpuRefresh||(enabled&&pool.visible&&cpu.baseline);
}
extern "C" int cardinal_presenter_refresh_allowed(uint64_t generation){using namespace cardinal_presenter;
 if(!tid||unsigned(syscall(SYS_gettid))!=tid||generation!=pool.producerGeneration||
    !pool.producerLive||!producer||!active||retired)return 0;
 // Same native visibility owner as GPU admission; CPU fallback also animates.
 Geometry bounds=geometry;bounds.known=bounds.width&&bounds.height;
 return clearVisibility(bias,root,active,text,retired,bounds)&&modalClear();
}
extern "C" int cardinal_presenter_target(uint64_t generation,MpcCardinalOverlayTarget* target){using namespace cardinal_presenter;PreserveErrno preserve;if(!tid||!target||unsigned(syscall(SYS_gettid))!=tid||generation!=pool.producerGeneration||!visible()||!owner())return 0;
 if(!attemptedAllocation){attemptedAllocation=true;if(!plane.admit(nativeFd,crtc,queries)){fail();return 0;}
  for(auto& b:pool.buffers){if(!allocateBuffer(b,nativeFd,geometry.height,geometry.width,buffer)){fail();releaseFree();return 0;}}
 }
 if(failed)return 0;
 int slot=pool.acquire(generation);if(slot==NONE){return 0;}
 auto& b=pool.buffers[slot];if(b.width!=geometry.height||b.height!=geometry.width||!b.framebuffer){pool.abandon(slot);fail();return 0;}
 if(cpu.painting)cpu.target=slot;
 *target={sizeof(*target),unsigned(slot),b.width,b.height,b.pitch,0x34325241u,b.dmaFd,allocationGeneration};return 1;
}
extern "C" void cardinal_presenter_ready(uint64_t generation,unsigned slot,int okay,uint32_t revision){using namespace cardinal_presenter;PreserveErrno preserve;if(!tid||unsigned(syscall(SYS_gettid))!=tid)return;if(!okay){pool.abandon(slot);fail();return;}if(!pool.ready(slot,generation)){fail();return;}pool.visible=visible();if(!pool.visible)suppress();pool.buffers[slot].revision=revision;if(pool.visible)cpu.accept(generation,cpu.ticket,slot,revision);}
extern "C" void cardinal_presenter_hook(void* saved,unsigned kind){using namespace cardinal_presenter;PreserveErrno preserve;if(!tid||unsigned(syscall(SYS_gettid))!=tid||!saved)return;
 uint32_t* regs=reinterpret_cast<uint32_t*>(static_cast<unsigned char*>(saved)+264);uintptr_t sp=uintptr_t(saved)+320;
 if(kind==13){cardinal_presenter_ui(13,regs[0],0);return;}
 uintptr_t which=kind==12?word(sp,0x28):regs[4];if(!owner(which)){return;}
 if(kind==10){
  if(attempt.composed){fail();return;}currentCommitModified=false;
  requestRecovery();
  pool.visible=visible();if(!pool.visible)suppress();int target=pool.desired();
  if(disableFailed||!pool.needsCommit(target)){return;}
  Property properties[10];uint32_t fb=target==NONE?0:pool.buffers[target].framebuffer;
  unsigned n=plane.compose(properties,fb,geometry.y,1280-geometry.x-int(geometry.width),geometry.height,geometry.width);
  if(!attempt.prepare(atomic,reinterpret_cast<drmModeAtomicReqPtr>(regs[5]),nativeFd,regs[10],which,sp,properties,n,target)){fail();if(target==NONE)disableFailed=true;return;}
  regs[5]=uintptr_t(attempt.composed);currentCommitModified=true;
 }else if(kind==11){
  ++displaySerial;
  if(currentCommitModified){
   if(attempt.composed!=reinterpret_cast<drmModeAtomicReqPtr>(regs[5])||attempt.hookSp!=sp||attempt.userdata!=which){fail();return;}
   int target=attempt.target;auto original=attempt.original;bool accepted=false;int result=attempt.result(atomic,int32_t(regs[0]),&accepted);int commitErrno=errno;
   regs[5]=uintptr_t(original);regs[0]=result;attempt=AtomicAttempt{};currentCommitModified=false;
   if(accepted){if(!pool.submitted(target)){fail();return;}pendingDisplaySerial=displaySerial;}
   else{if(target==NONE){disableFailed=true;}fail();}
   preserve.value=commitErrno;
  }
 }else if(kind==12){
  if(pool.pendingCommit&&pendingDisplaySerial==displaySerial){if(pool.completed(pool.pendingGeneration)&&!pool.producerLive&&pool.detached())releaseFree();pendingDisplaySerial=0;}
 }
 
}
extern "C" uint64_t cardinal_presenter_paint_ticket(uintptr_t control){using namespace cardinal_presenter;return tid&&unsigned(syscall(SYS_gettid))==tid&&control==producer&&cpu.painting?cpu.ticket:0;}
extern "C" int cardinal_presenter_omit(uint64_t generation,uint64_t ticket,unsigned slot,uint32_t revision){using namespace cardinal_presenter;PreserveErrno preserve;
 if(!tid||unsigned(syscall(SYS_gettid))!=tid)return 0;
 if(!enabled||!visible()||!owner()||slot>=BUFFER_COUNT||pool.buffers[slot].use!=Use::Ready||pool.buffers[slot].producerGeneration!=generation||pool.buffers[slot].revision!=revision||!cpu.omit(generation,ticket,slot,revision)){return 0;}
 return 1;
}
extern "C" void cardinal_presenter_cpu(uint64_t generation,uint64_t ticket,int okay,uint32_t revision){using namespace cardinal_presenter;PreserveErrno preserve;
 if(!tid||unsigned(syscall(SYS_gettid))!=tid||!cpu.matches(generation,ticket))return;
 if(okay){cpu.exported=true;(void)revision;}else{cpu.owed=cpu.required=true;cpu.revoke();}
}
extern "C" int cardinal_presenter_skip_draw(uintptr_t control,uint64_t ticket){using namespace cardinal_presenter;PreserveErrno preserve;
 if(!tid||unsigned(syscall(SYS_gettid))!=tid||control!=producer||!cpu.matches(pool.producerGeneration,ticket))return 0;
 if(cpu.omitted&&enabled&&visible()&&owner()){cpu.skipped=true;return 1;}
 if(cpu.omitted){cpu.revoke();}return 0;
}
extern "C" void cardinal_presenter_paint_done(uintptr_t control,uint64_t ticket,int drew,int okay){using namespace cardinal_presenter;PreserveErrno preserve;
 if(!tid||unsigned(syscall(SYS_gettid))!=tid||control!=producer||!cpu.matches(pool.producerGeneration,ticket))return;
 cpu.end(ticket,drew,okay);
 requestCpuRefresh();
}
#endif
