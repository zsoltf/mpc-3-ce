/* The production ledger, with only its mirror publication storage substituted.
 * No native dispatch, scheduling, native calls or physical MIDI are exercised. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mirror-state.h"
static MirrorState fixture;
MirrorState *mirror_state=&fixture;
#include "midi-mute-accounting.h"
static void need(int ok,const char *why){if(!ok){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static MidiMuteKey pending(unsigned n){
 uint32_t p[22]={0};p[2]=3;p[12]=36;p[14]=n;p[16]=36;p[17]=0x7ffffff8;p[18]=3;p[20]=0x12340000;
 MidiMuteKey key;midi_mute_pending_key(&key,1,77,0x23450000,p);return key;
}
static void reset(void){memset(&fixture.midi_mute,0,sizeof(fixture.midi_mute));midi_mute_account_initialize();}
int main(void){
 reset();MidiMuteEntry e={.state=MM_CONSTRUCTING,.epoch=1,.track=0x12340000,.owner=77,.pads=1,.member={1},.status=0x90,.note=36};
 uint32_t ids[MM_BUCKET_LIMIT];
 for(unsigned i=1;i<=MIDI_MUTE_CAPACITY;i++){
  e.root=i;e.key=(MidiMuteKey){.epoch=1,.owner=77};e.key.w[0]=i;uint32_t id=midi_mute_reserve(&e);need(id!=0,"reserve full capacity");midi_mute_activate(id);MidiMuteKey key=pending(i);midi_mute_bind_pending(i,&e.key,&key);
 }
 need(midi_mute_ledger.used==MIDI_MUTE_CAPACITY&&!atomic_load(&fixture.midi_mute.error),"all records indexed without fault");
 unsigned maximum=0;
 for(unsigned i=1;i<=MIDI_MUTE_CAPACITY;i++){MidiMuteKey key=pending(i);need(midi_mute_find(&key,ids)==1&&midi_mute_entry(ids[0])->root==i,"exact indexed expiry identity");}
 maximum=atomic_load(&fixture.midi_mute.max_lookup);
 need(!midi_mute_reserve(&e)&&atomic_load(&fixture.midi_mute.error)==MM_CAPACITY&&midi_mute_ledger.used==MIDI_MUTE_CAPACITY,"exhaustion retains every existing record");
 MidiMuteKey first=pending(1),newer=pending(2);need(midi_mute_find(&first,ids)==1,"old emission retained");midi_mute_retire(ids[0]);need(midi_mute_find(&newer,ids)==1&&midi_mute_entry(ids[0])->state==MM_ACTIVE,"retirement preserves newer same physical key");
 need(midi_mute_find(&first,ids)==1&&midi_mute_entry(ids[0])->state==MM_RETIRED,"old release matches tombstone");midi_mute_release(ids[0]);need(!midi_mute_find(&first,ids)&&midi_mute_ledger.used==MIDI_MUTE_CAPACITY-1,"release consumes only old identity");
 need(!midi_mute_reserve(&e),"fault does not silently restart accounting after one release");
 reset();uint32_t p[22]={0},q[22]={0};p[2]=q[2]=3;p[12]=q[12]=36;p[13]=0x12;q[13]=0xcafe0012;p[16]=36;q[16]=0xabcd0024;p[21]=1;q[21]=0x123401;p[3]=0xdeadbeef;
 MidiMuteKey a,b;midi_mute_pending_key(&a,1,77,88,p);midi_mute_pending_key(&b,1,77,88,q);need(midi_mute_key_equal(&a,&b),"native move padding excluded");q[13]^=1;midi_mute_pending_key(&b,1,77,88,q);need(!midi_mute_key_equal(&a,&b),"semantic byte34 retained");
 e.root=1;e.key=pending(1);uint32_t old=midi_mute_reserve(&e);e.root=2;uint32_t other=midi_mute_reserve(&e);need(old&&other&&!midi_mute_find(&e.key,ids)&&atomic_load(&fixture.midi_mute.error)==MM_ASSOCIATION,"duplicate pending roots remain ambiguous");midi_mute_release(old);midi_mute_release(other);
 reset();midi_mute_live_key(&e.key,1,77,0x12340000,36,0,9);
 for(unsigned i=0;i<MM_BUCKET_LIMIT;i++){e.root=i+1;need(midi_mute_reserve(&e)!=0,"bounded repeated live key group");}
 need(!midi_mute_reserve(&e)&&midi_mute_find(&e.key,ids)==MM_BUCKET_LIMIT,"bucket bound faults new accounting and retains existing live releases");
 midi_mute_contaminate(&e.key);
 for(unsigned i=0;i<MM_BUCKET_LIMIT;i++){need(midi_mute_entry(ids[i])->unmanaged,"unaccounted same-key live emission preserves grouped off");midi_mute_release(ids[i]);}
 need(!midi_mute_ledger.used&&!midi_mute_ledger.pad_heads[0],"all live entries and intrusive pad links retired");
 atomic_store(midi_mute_native_value,7);atomic_store(midi_mute_dirty,1);
 printf("PASS production accounting: slot_bytes=%zu storage_bytes=%zu full_capacity=%u actual_max_lookup=%u bounded_bucket=%u; native/MIDI boundaries unobserved\n",sizeof(MidiMuteEntry),sizeof(midi_mute_ledger)+sizeof(midi_mute_native_value)+sizeof(midi_mute_dirty),MIDI_MUTE_CAPACITY,maximum,MM_BUCKET_LIMIT);return 0;
}
