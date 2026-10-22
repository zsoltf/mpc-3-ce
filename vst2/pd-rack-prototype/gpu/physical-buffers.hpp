#pragma once
#include "presenter-state.hpp"
#include "include/xf86drmMode.h"
#include <xf86drm.h>
#include <drm.h>
#include <drm_mode.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
namespace cardinal_presenter {
struct BufferFunctions {
 decltype(&drmIsMaster) isMaster=nullptr;
 decltype(&drmPrimeFDToHandle) importHandle=nullptr;
 decltype(&drmModeAddFB2) addFramebuffer=nullptr;
 decltype(&drmModeRmFB) removeFramebuffer=nullptr;
};
inline bool rockchip(int fd){char name[32]={};drm_version version{};version.name=name;version.name_len=31;return ioctl(fd,DRM_IOCTL_VERSION,&version)==0&&version.name_len==8&&!memcmp(name,"rockchip",8);}
// Resources are created by us. The native fd is used only while the caller
// revalidates its same manager/screen generation. It is neither closed nor duped.
inline bool releaseBuffer(Buffer& b,int nativeFd,const BufferFunctions& f){
 if(b.use!=Use::Free)return false;
 if(b.framebuffer){if(nativeFd<0||f.removeFramebuffer(nativeFd,b.framebuffer))return false;b.framebuffer=0;}
 if(b.nativeHandle){drm_gem_close d{};d.handle=b.nativeHandle;if(nativeFd<0||ioctl(nativeFd,DRM_IOCTL_GEM_CLOSE,&d))return false;b.nativeHandle=0;}
 if(b.dmaFd>=0){close(b.dmaFd);b.dmaFd=-1;}
 if(b.handle){drm_mode_destroy_dumb drop{};drop.handle=b.handle;if(ioctl(b.allocationFd,DRM_IOCTL_MODE_DESTROY_DUMB,&drop))return false;b.handle=0;}
 if(b.allocationFd>=0){close(b.allocationFd);b.allocationFd=-1;}
 b=Buffer{};return true;
}
inline bool allocateBuffer(Buffer& b,int nativeFd,uint32_t width,uint32_t height,const BufferFunctions& f){
 if(b.allocationFd>=0||b.framebuffer||b.use!=Use::Free||!width||!height||width>800||height>1280||!f.isMaster||!f.importHandle||!f.addFramebuffer||!f.removeFramebuffer)return false;
 struct stat native{};if(fstat(nativeFd,&native)||!S_ISCHR(native.st_mode)||major(native.st_rdev)!=226||minor(native.st_rdev)>15||!rockchip(nativeFd)||f.isMaster(nativeFd)!=1)return false;
 char path[32];snprintf(path,sizeof path,"/dev/dri/card%u",unsigned(minor(native.st_rdev)));
 b.allocationFd=open(path,O_RDWR|O_CLOEXEC|O_NOFOLLOW);if(b.allocationFd<0)return false;
 struct stat own{};bool okay=!fstat(b.allocationFd,&own)&&own.st_rdev==native.st_rdev&&S_ISCHR(own.st_mode)&&f.isMaster(b.allocationFd)==0&&rockchip(b.allocationFd);
 uint32_t& imported=b.nativeHandle;drm_mode_create_dumb create{};create.width=width;create.height=height;create.bpp=32;
 if(okay){okay=ioctl(b.allocationFd,DRM_IOCTL_MODE_CREATE_DUMB,&create)==0;b.handle=create.handle;}
 if(okay){okay=create.handle&&create.pitch>=width*4&&create.pitch<=8192&&create.size>=uint64_t(create.pitch)*height&&create.size<=8u*1024u*1024u;b.pitch=create.pitch;b.bytes=create.size;b.width=width;b.height=height;}
 if(okay){drm_prime_handle prime{};prime.handle=b.handle;prime.flags=DRM_CLOEXEC|DRM_RDWR;prime.fd=-1;okay=ioctl(b.allocationFd,DRM_IOCTL_PRIME_HANDLE_TO_FD,&prime)==0;b.dmaFd=prime.fd;}
 if(okay)okay=f.importHandle(nativeFd,b.dmaFd,&imported)==0&&imported!=0;
 if(okay){uint32_t handles[4]={imported},pitches[4]={b.pitch},offsets[4]={};okay=f.addFramebuffer(nativeFd,width,height,0x34325241u,handles,pitches,offsets,&b.framebuffer,0)==0&&b.framebuffer!=0;}
 // The FB owns its reference after AddFB. Keep no GEM handle in MPC's namespace.
 if(imported){drm_gem_close closeHandle{};closeHandle.handle=imported;if(ioctl(nativeFd,DRM_IOCTL_GEM_CLOSE,&closeHandle))okay=false;else imported=0;}
 if(!okay)releaseBuffer(b,nativeFd,f);
 return okay;
}
}
