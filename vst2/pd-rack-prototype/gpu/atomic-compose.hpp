#pragma once
#include <stdint.h>
#include <errno.h>
#include "include/xf86drmMode.h"
namespace cardinal_presenter {
// Uses public libdrm operations only, never a private atomic-request layout.
struct AtomicFunctions {
 decltype(&drmModeAtomicDuplicate) duplicate=nullptr;
 decltype(&drmModeAtomicFree) free=nullptr;
 decltype(&drmModeAtomicAddProperty) add=nullptr;
 decltype(&drmModeAtomicCommit) commit=nullptr;
};
struct Property {uint32_t object,id;uint64_t value;};
struct AtomicAttempt {
 drmModeAtomicReqPtr original=nullptr,composed=nullptr;
 int fd=-1;uint32_t flags=0;uintptr_t userdata=0,hookSp=0;int target=-1;
 // Duplicate first: failure never changes a single original native property.
 bool prepare(const AtomicFunctions& f,drmModeAtomicReqPtr native,int displayFd,uint32_t nativeFlags,uintptr_t screen,uintptr_t sp,const Property* properties,unsigned count,int slot){
  if(composed||!native||displayFd<0||!f.duplicate||!f.free||!f.add||!f.commit||!properties||count==0||count>10)return false;
  drmModeAtomicReqPtr copy=f.duplicate(native);if(!copy)return false;
  for(unsigned i=0;i<count;++i)if(!properties[i].object||!properties[i].id||f.add(copy,properties[i].object,properties[i].id,properties[i].value)<0){f.free(copy);return false;}
  original=native;composed=copy;fd=displayFd;flags=nativeFlags;userdata=screen;hookSp=sp;target=slot;return true;
 }
 // Called immediately after native commit; restore original r5 for MPC's own
 // free. On failure perform the ORIGINAL unmodified native commit, same flags
 // and userdata. The caller publishes fallback return/errno in the saved frame.
 int result(const AtomicFunctions& f,int nativeResult,bool* overlayAccepted){
  *overlayAccepted=nativeResult==0;
  int result=nativeResult,saved=errno;
  if(nativeResult!=0){result=f.commit(fd,original,flags,reinterpret_cast<void*>(userdata));saved=errno;}
  f.free(composed);composed=nullptr;errno=saved;return result;
 }
};
}
