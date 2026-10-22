#pragma once
#include "atomic-compose.hpp"
#include <cstring>
namespace cardinal_presenter {
struct PlaneFunctions {
 decltype(&drmModeGetResources) resources=nullptr;decltype(&drmModeFreeResources) freeResources=nullptr;
 decltype(&drmModeGetPlaneResources) planes=nullptr;decltype(&drmModeFreePlaneResources) freePlanes=nullptr;
 decltype(&drmModeGetPlane) plane=nullptr;decltype(&drmModeFreePlane) freePlane=nullptr;
 decltype(&drmModeObjectGetProperties) properties=nullptr;decltype(&drmModeFreeObjectProperties) freeProperties=nullptr;
 decltype(&drmModeGetProperty) property=nullptr;decltype(&drmModeFreeProperty) freeProperty=nullptr;
};
struct Plane {
 uint32_t id=0,crtc=0,property[10]={};
 static const char* name(unsigned i){static const char* names[]={"FB_ID","CRTC_ID","SRC_X","SRC_Y","SRC_W","SRC_H","CRTC_X","CRTC_Y","CRTC_W","CRTC_H"};return names[i];}
 bool admit(int fd,uint32_t expectedCrtc,const PlaneFunctions& f){
  if(!f.resources||!f.freeResources||!f.planes||!f.freePlanes||!f.plane||!f.freePlane||!f.properties||!f.freeProperties||!f.property||!f.freeProperty)return false;
  auto* resources=f.resources(fd);if(!resources)return false;int index=-1;
  if(resources->count_crtcs>0&&resources->count_crtcs<=16)for(int i=0;i<resources->count_crtcs;++i)if(resources->crtcs[i]==expectedCrtc)index=i;
  f.freeResources(resources);if(index<0)return false;
  auto* planes=f.planes(fd);if(!planes)return false;
  if(planes->count_planes<=16)for(unsigned i=0;i<planes->count_planes&&!id;++i){
   auto* p=f.plane(fd,planes->planes[i]);if(!p)continue;
   bool format=false;if(p->count_formats<=64)for(unsigned j=0;j<p->count_formats;++j)format|=p->formats[j]==0x34325241u;
   bool okay=format&&!p->crtc_id&&!p->fb_id&&(p->possible_crtcs&(1u<<index));uint32_t candidate=p->plane_id;f.freePlane(p);if(!okay)continue;
   auto* props=f.properties(fd,candidate,DRM_MODE_OBJECT_PLANE);if(!props)continue;
   bool overlay=false;unsigned found=0;uint32_t ids[10]={};
   if(props->count_props<=64)for(unsigned j=0;j<props->count_props;++j){
    auto* property=f.property(fd,props->props[j]);if(!property){okay=false;break;}
    if(!strcmp(property->name,"type"))overlay=props->prop_values[j]==DRM_PLANE_TYPE_OVERLAY;
    for(unsigned k=0;k<10;++k)if(!strcmp(property->name,name(k))){if(ids[k])okay=false;ids[k]=property->prop_id;++found;if(k<2&&props->prop_values[j]!=0)okay=false;}
    f.freeProperty(property);
   }
   f.freeProperties(props);if(okay&&overlay&&found==10){id=candidate;crtc=expectedCrtc;for(unsigned k=0;k<10;++k)property[k]=ids[k];}
  }
  f.freePlanes(planes);return id!=0;
 }
 unsigned compose(Property* out,uint32_t framebuffer,int x,int y,unsigned width,unsigned height)const{
  if(!id||!crtc)return 0;
  uint64_t values[10]={framebuffer,framebuffer?crtc:0,0,0,uint64_t(width)<<16,uint64_t(height)<<16,uint64_t(int64_t(x)),uint64_t(int64_t(y)),width,height};
  unsigned count=framebuffer?10:2;
  for(unsigned i=0;i<count;++i)out[i]={id,property[i],values[i]};
  return count;
 }
};
}
