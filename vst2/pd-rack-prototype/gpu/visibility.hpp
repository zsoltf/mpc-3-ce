#pragma once
#include <stdint.h>
#include <cstring>
namespace cardinal_presenter {
inline bool readableShape(uintptr_t p,unsigned extent){return p&&p%4==0&&p<=UINT32_MAX-extent;}
inline uint32_t word(uintptr_t p,unsigned offset=0){uint32_t v;std::memcpy(&v,reinterpret_cast<const void*>(p+offset),4);return v;}
struct Geometry {int x=0,y=0;unsigned width=0,height=0;bool known=false;};
// Two disjoint native damage rectangles may exactly partition the full canvas.
// Admit that equivalent representation, never an area estimate or partial clip.
inline bool completeCanvasClip(uintptr_t rectangles,unsigned count,int x,int y,unsigned width,unsigned height){
 if(count==1)return int32_t(word(rectangles))==x&&int32_t(word(rectangles,4))==y&&word(rectangles,8)==width&&word(rectangles,12)==height;
 if(count!=2)return false;
 int64_t left[2],top[2],right[2],bottom[2];uint64_t area=0;
 for(unsigned i=0;i<2;++i){
  uintptr_t r=rectangles+i*16;left[i]=int32_t(word(r));top[i]=int32_t(word(r,4));
  int64_t w=int32_t(word(r,8)),h=int32_t(word(r,12));
  if(w<=0||h<=0)return false;
  right[i]=left[i]+w;bottom[i]=top[i]+h;
  if(left[i]<x||top[i]<y||right[i]>int64_t(x)+width||bottom[i]>int64_t(y)+height)return false;
  area+=uint64_t(w)*uint64_t(h);
 }
 const bool overlap=left[0]<right[1]&&left[1]<right[0]&&top[0]<bottom[1]&&top[1]<bottom[0];
 return !overlap&&area==uint64_t(width)*height;
}
inline bool canvasGeometry(uint32_t bias,uintptr_t graphics,unsigned width,unsigned height,int expectedFd,Geometry& out,unsigned* reason=nullptr){
 auto reject=[&](unsigned code){if(reason)*reason=code;return false;};
 if(reason)*reason=0;
 out.known=false;if(!readableShape(graphics,8))return reject(2);
 if(width<640||width>1280||height<400||height>800)return reject(3);
 uintptr_t context=word(graphics,4);if(!readableShape(context,8))return reject(4);
 if(word(context)!=bias+0x0688bdecu)return reject(5);
 if(word(word(context),0x64)!=bias+0x00ac21f4u)return reject(6);
 uintptr_t saved=word(context,4);if(!readableShape(saved,0x58))return reject(7);
 if((word(saved,0x24)&0xffff)!=1)return reject(8);
 if((word(saved,0x28)>>24)!=255)return reject(9);
 int x=int32_t(word(saved,0x1c)),y=int32_t(word(saved,0x20));if(x<0||y<0||uint64_t(x)+width>1280||uint64_t(y)+height>800)return reject(10);
 uintptr_t clip=word(saved);if(!readableShape(clip,0x14))return reject(11);
 if(word(clip)!=bias+0x0688bc04u)return reject(12);
 unsigned count=word(clip,0x10);if(count<1||count>2)return reject(13);
 if(word(clip,0xc)<count)return reject(14);
 uintptr_t rectangle=word(clip,8);if(!readableShape(rectangle,count*16))return reject(15);
 if(!completeCanvasClip(rectangle,count,x,y,width,height))return reject(16);
 uintptr_t destination=word(saved,0x54);if(!readableShape(destination,0x30))return reject(17);
 if(word(destination)!=bias+0x068c8b94u)return reject(18);
 uintptr_t backing=word(destination,0x2c);if(!readableShape(backing,0x2c))return reject(19);
 if(int(word(backing))!=expectedFd)return reject(20);
 if(word(backing,4)!=1280||word(backing,8)!=800)return reject(21);
 if(word(backing,0x10)!=4)return reject(22);
 out={x,y,width,height,true};return true;
}
// Source-qualified scalar owners. Reads are permitted only in the active
// Engine/TMI lifetime, never merely because an old pointer still has a vptr.
inline bool clearVisibility(uint32_t bias,uintptr_t root,bool active,bool text,bool retired,const Geometry& canvas){
 if(!active||text||retired||!canvas.known||!readableShape(root,0x840)||word(root)!=bias+0x068d7bf8u)return false;
 uintptr_t tmi=word(root,0x83c);if(!readableShape(tmi,0x3710)||word(tmi)!=bias+0x0699efd0u||word(tmi,0x124)!=root)return false;
 uintptr_t stack=word(tmi,0xa0),container=word(tmi,0xa4);
 if(!readableShape(stack,12)||!readableShape(container,0x138)||word(container)!=bias+0x069c3090u||word(container,0x10)!=bias+0x069c3138u||word(container,0x134)!=stack||word(stack)!=stack||word(stack,4)!=stack||word(stack,8)!=0)return false;
 uintptr_t panel=word(tmi,0x3708);if(!readableShape(panel,0xb8)||word(panel)!=bias+0x069a03c4u||word(panel,0xc)!=tmi+4||word(panel,0x24)||word(panel,0x94)||word(panel,0xa0)||*(const unsigned char*)(panel+0xa4))return false;
 const int x=int32_t(word(panel,0x10)),y=int32_t(word(panel,0x14)),w=int32_t(word(panel,0x18)),h=int32_t(word(panel,0x1c));
 if(x<0||y<0||w<0||h<0||int64_t(x)+w>1280||int64_t(y)+h>800)return false;
 if((*(const unsigned char*)(panel+0x68)&2)&&x<canvas.x+int(canvas.width)&&x+w>canvas.x&&y<canvas.y+int(canvas.height)&&y+h>canvas.y)return false;
 // Native shortcut handle is a separate TMI child, drawn after the editor.
 // Its collapsed30px width is outside the Cardinal-only32px canvas inset.
 // Expansion, animation or unknown geometry must still hide GPU output.
 uintptr_t shortcut=word(tmi,0x370c);
 if(!readableShape(shortcut,0xb8)||word(shortcut)!=bias+0x069cc03cu||word(shortcut,0xc)!=tmi+4||word(shortcut,0x24)||word(shortcut,0x94)||word(shortcut,0xa0)!=2||*(const unsigned char*)(shortcut+0xa4))return false;
 const int sx=int32_t(word(shortcut,0x10)),sy=int32_t(word(shortcut,0x14)),sw=int32_t(word(shortcut,0x18)),sh=int32_t(word(shortcut,0x1c));
 if(sx<0||sy<0||sw<0||sh<0||int64_t(sx)+sw>1280||int64_t(sy)+sh>800)return false;
 if((*(const unsigned char*)(shortcut+0x68)&2)&&sx<canvas.x+int(canvas.width)&&sx+sw>canvas.x&&sy<canvas.y+int(canvas.height)&&sy+sh>canvas.y)return false;
 return true;
}
}
