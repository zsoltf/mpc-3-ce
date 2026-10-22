#pragma once
#include <stdint.h>
namespace cardinal_presenter {
// One producer's CPU backing obligation and one borrowed paint acknowledgement.
// No Graphics/native object pointers survive the callback.
struct CpuPaint {
 uint64_t generation=0,serial=0,ticket=0,readyTicket=0;
 int target=-1,readySlot=-1;uint32_t readyRevision=0;
 bool baseline=false,owed=false,required=false,queued=false;
 bool painting=false,admitted=false,exported=false,omitted=false,skipped=false;
 void open(uint64_t gen){uint64_t sequence=serial;*this=CpuPaint{};serial=sequence;generation=gen;owed=required=true;}
 void close(){uint64_t sequence=serial;*this=CpuPaint{};serial=sequence;}
 void revoke(){omitted=false;if(owed)required=true;}
 uint64_t begin(bool valid){
  if(painting){revoke();painting=false;return 0;}
  painting=true;ticket=++serial;admitted=valid;exported=omitted=skipped=false;target=readySlot=-1;readyTicket=0;
  if(!valid)revoke();
  return ticket;
 }
 bool matches(uint64_t gen,uint64_t paint)const{return painting&&paint&&paint==ticket&&gen==generation;}
 bool accept(uint64_t gen,uint64_t paint,int slot,uint32_t revision){
  if(!matches(gen,paint)||!admitted||target!=slot)return false;
  readyTicket=paint;readySlot=slot;readyRevision=revision;return true;
 }
 bool omit(uint64_t gen,uint64_t paint,int slot,uint32_t revision){
  if(!matches(gen,paint)||!baseline||required||!admitted||readyTicket!=paint||readySlot!=slot||readyRevision!=revision)return false;
  omitted=true;owed=true;return true;
 }
 bool end(uint64_t paint,bool drew,bool okay){
  if(!painting||paint!=ticket||!paint)return false;
  bool discharged=false;
  if(drew&&okay&&exported){discharged=owed;baseline=true;owed=required=queued=false;}
  else if(!okay){owed=required=true;omitted=false;}
  else if(owed&&!skipped){required=true;}
  painting=false;admitted=false;omitted=false;readyTicket=0;return discharged;
 }
};
}
