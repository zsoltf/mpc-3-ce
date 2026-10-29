/* Included by midi-mute.inc. One existing native audio lane owns these derived
 * transport records; setter hooks publish only atomic transition generations.
 * Hash chains index semantic release keys. No allocation, expiry timer, native
 * musical state, or full-table search on a note/release. */
#ifndef MPC_MIDI_MUTE_ACCOUNTING_H
#define MPC_MIDI_MUTE_ACCOUNTING_H
#define MM_BUCKETS 32768u
#define MM_PADS 8u
#define MM_BUCKET_LIMIT 64u
enum {MM_FREE,MM_CONSTRUCTING,MM_ACTIVE,MM_RETIRED,MM_BLOCKED,MM_ABANDONED};
typedef struct {uint32_t epoch,kind,owner,w[23];} MidiMuteKey;
typedef struct {
 uint32_t state,next,previous,hash,root,epoch,track,owner,program,program_owner;
 uint32_t sink,route,status,note,velocity,route_generation,source_next,source_previous,source_hash,unmanaged,timestamp,sample_offset;
 uint32_t pads,count,member[MM_PADS],generation[MM_PADS],pad_next[MM_PADS],pad_previous[MM_PADS];
 MidiMuteKey key;
} MidiMuteEntry;
typedef struct {
 MidiMuteEntry entries[MIDI_MUTE_CAPACITY];
 uint32_t buckets[MM_BUCKETS],sources[MM_BUCKETS],pad_heads[PAD_OWNERS],free_head,used,active;
} MidiMuteLedger;
static MidiMuteLedger midi_mute_ledger;
/* One coherent native Property projection: bit0 known, bit1 value, upper
 * 30 bits mute-transition generation. Instrument remains the musical owner. */
static _Atomic uint32_t midi_mute_native_value[PAD_OWNERS];
static _Atomic uint32_t midi_mute_dirty[PAD_OWNERS/32];
static MidiMuteState *midi_mute_projection(void){return &mirror_state->midi_mute;}
static void midi_mute_fault(unsigned error){
 unsigned expected=0;atomic_compare_exchange_strong(&midi_mute_projection()->error,&expected,error);
}
static uint32_t midi_mute_hash(const MidiMuteKey *key){
 uint32_t h=2166136261u;const uint32_t *p=(const uint32_t*)key;
 for(unsigned i=0;i<sizeof(*key)/4;i++){h^=p[i];h*=16777619u;h^=h>>13;}return h&(MM_BUCKETS-1);
}
static int midi_mute_key_equal(const MidiMuteKey *a,const MidiMuteKey *b){return !memcmp(a,b,sizeof(*a));}
static void midi_mute_pending_key(MidiMuteKey *key,uint32_t epoch,uint32_t owner,uint32_t sequencer,const uint32_t p[22]){
 memset(key,0,sizeof(*key));key->epoch=epoch;key->owner=owner;key->kind=3;key->w[0]=sequencer;
 memcpy(key->w+1,p,22*4);key->w[4]=0;key->w[14]&=255;key->w[17]&=255;key->w[22]&=255;
}
static void midi_mute_live_key(MidiMuteKey *key,uint32_t epoch,uint32_t owner,uint32_t track,uint32_t note,uint32_t channel,uint32_t source){
 memset(key,0,sizeof(*key));key->epoch=epoch;key->owner=owner;key->kind=1;
 key->w[0]=track;key->w[1]=note;key->w[2]=channel;key->w[3]=source;
}
static void midi_mute_account_initialize(void){
 memset(&midi_mute_ledger,0,sizeof(midi_mute_ledger));
 for(unsigned i=0;i<MIDI_MUTE_CAPACITY;i++)midi_mute_ledger.entries[i].next=i+1<MIDI_MUTE_CAPACITY?i+2:0;
 midi_mute_ledger.free_head=1;
 atomic_store(&midi_mute_projection()->slot_bytes,sizeof(MidiMuteEntry));
 atomic_store(&midi_mute_projection()->storage_bytes,sizeof(midi_mute_ledger)+sizeof(midi_mute_native_value)+sizeof(midi_mute_dirty));
}
static MidiMuteEntry *midi_mute_entry(uint32_t id){return id&&id<=MIDI_MUTE_CAPACITY?midi_mute_ledger.entries+id-1:NULL;}
static int midi_mute_index_room(const MidiMuteKey *key){
 uint32_t at=midi_mute_ledger.buckets[midi_mute_hash(key)];unsigned n=0;
 while(at&&n<MM_BUCKET_LIMIT){MidiMuteEntry *e=midi_mute_entry(at);if(!e)return 0;at=e->next;n++;}
 return !at&&n<MM_BUCKET_LIMIT;
}
static void midi_mute_index(uint32_t id,const MidiMuteKey *key){
 MidiMuteEntry *e=midi_mute_entry(id);e->key=*key;e->hash=midi_mute_hash(key);e->previous=0;e->next=midi_mute_ledger.buckets[e->hash];
 if(e->next)midi_mute_entry(e->next)->previous=id;
 midi_mute_ledger.buckets[e->hash]=id;
}
static void midi_mute_unindex(uint32_t id){
 MidiMuteEntry *e=midi_mute_entry(id);
 if(e->previous)midi_mute_entry(e->previous)->next=e->next;else midi_mute_ledger.buckets[e->hash]=e->next;
 if(e->next)midi_mute_entry(e->next)->previous=e->previous;
 e->previous=e->next=0;
}
static void midi_mute_pad_link(uint32_t id,unsigned index){
 MidiMuteEntry *e=midi_mute_entry(id);unsigned member=e->member[index];uint32_t node=(id-1)*MM_PADS+index+1,next=midi_mute_ledger.pad_heads[member-1];
 e->pad_next[index]=next;
 if(next){unsigned n=(next-1)/MM_PADS,j=(next-1)%MM_PADS;midi_mute_ledger.entries[n].pad_previous[j]=node;}
 midi_mute_ledger.pad_heads[member-1]=node;
}
static void midi_mute_pad_unlink(uint32_t id,unsigned index){
 MidiMuteEntry *e=midi_mute_entry(id);uint32_t next=e->pad_next[index],previous=e->pad_previous[index];
 if(previous)midi_mute_ledger.entries[(previous-1)/MM_PADS].pad_next[(previous-1)%MM_PADS]=next;
 else midi_mute_ledger.pad_heads[e->member[index]-1]=next;
 if(next)midi_mute_ledger.entries[(next-1)/MM_PADS].pad_previous[(next-1)%MM_PADS]=previous;
 e->pad_next[index]=e->pad_previous[index]=0;
}
static uint32_t midi_mute_reserve(const MidiMuteEntry *source){
 MidiMuteState *p=midi_mute_projection();
 /* Fault fallback preserves native unmuted output, but never overwrites an
  * existing release/tombstone. Its accounting loss is explicit. */
 if(atomic_load(&p->error)||!midi_mute_ledger.free_head||!midi_mute_index_room(&source->key)){if(!atomic_load(&p->error))midi_mute_fault(MM_CAPACITY);atomic_fetch_add(&p->unaccounted,1);return 0;}
 unsigned id=midi_mute_ledger.free_head;MidiMuteEntry *e=midi_mute_entry(id);midi_mute_ledger.free_head=e->next;*e=*source;e->next=e->previous=0;
 e->source_hash=((e->track>>2)^e->owner*2654435761u)&(MM_BUCKETS-1);e->source_previous=0;e->source_next=midi_mute_ledger.sources[e->source_hash];
 if(e->source_next)midi_mute_entry(e->source_next)->source_previous=id;
 midi_mute_ledger.sources[e->source_hash]=id;
 midi_mute_index(id,&source->key);if(e->state==MM_CONSTRUCTING){for(unsigned i=0;i<e->pads;i++)midi_mute_pad_link(id,i);e->count=1;}
 midi_mute_ledger.used++;atomic_store(&p->retained,midi_mute_ledger.used);
 if(midi_mute_ledger.used>atomic_load(&p->high_water))atomic_store(&p->high_water,midi_mute_ledger.used);
 return id;
}
static void midi_mute_activate(uint32_t id){
 MidiMuteEntry *e=midi_mute_entry(id);if(!e||e->state!=MM_CONSTRUCTING){midi_mute_fault(MM_ASSOCIATION);return;}
 e->state=MM_ACTIVE;midi_mute_ledger.active++;atomic_store(&midi_mute_projection()->active,midi_mute_ledger.active);atomic_fetch_add(&midi_mute_projection()->emitted,1);
}
static void midi_mute_deactivate(uint32_t id,unsigned state){
 MidiMuteEntry *e=midi_mute_entry(id);if(e->state!=MM_ACTIVE)return;
 for(unsigned i=0;i<e->pads;i++)midi_mute_pad_unlink(id,i);
 e->count=0;
 e->state=state;midi_mute_ledger.active--;atomic_store(&midi_mute_projection()->active,midi_mute_ledger.active);
}
static void midi_mute_retire(uint32_t id){midi_mute_deactivate(id,MM_RETIRED);atomic_fetch_add(&midi_mute_projection()->terminated,1);}
static void midi_mute_release(uint32_t id){
 MidiMuteEntry *e=midi_mute_entry(id);if(!e||!e->state){midi_mute_fault(MM_ASSOCIATION);return;}
 if(e->state==MM_ACTIVE)midi_mute_ledger.active--;
 midi_mute_unindex(id);if(e->count)for(unsigned i=0;i<e->pads;i++)midi_mute_pad_unlink(id,i);
 if(e->source_previous)midi_mute_entry(e->source_previous)->source_next=e->source_next;else midi_mute_ledger.sources[e->source_hash]=e->source_next;
 if(e->source_next)midi_mute_entry(e->source_next)->source_previous=e->source_previous;
 memset(e,0,sizeof(*e));e->next=midi_mute_ledger.free_head;midi_mute_ledger.free_head=id;midi_mute_ledger.used--;
 MidiMuteState *p=midi_mute_projection();atomic_store(&p->retained,midi_mute_ledger.used);atomic_store(&p->active,midi_mute_ledger.active);atomic_fetch_add(&p->released,1);
}
/* Caller supplies at most the bounded native fanout. A repeated semantic key
 * from different roots is ambiguous; no FIFO or heap-address guess is made. */
static unsigned midi_mute_find(const MidiMuteKey *key,uint32_t ids[MM_BUCKET_LIMIT]){
 uint32_t at=midi_mute_ledger.buckets[midi_mute_hash(key)],root_id=0;unsigned n=0,walk=0;
 while(at&&walk++<MM_BUCKET_LIMIT){MidiMuteEntry *e=midi_mute_entry(at);if(!e){midi_mute_fault(MM_ASSOCIATION);return 0;}
  if(midi_mute_key_equal(&e->key,key)){
   if(n==MM_BUCKET_LIMIT||(root_id&&key->kind==3&&root_id!=e->root)){midi_mute_fault(MM_ASSOCIATION);return 0;}
   ids[n++]=at;root_id=e->root;
  }at=e->next;
 }
 if(at)midi_mute_fault(MM_ASSOCIATION);
 if(walk>atomic_load(&midi_mute_projection()->max_lookup))atomic_store(&midi_mute_projection()->max_lookup,walk);
 return n;
}
/* An unaccounted positive with this native release identity makes a later
 * grouped off necessary even if every tracked predecessor was retired. */
static void midi_mute_contaminate(const MidiMuteKey *key){
 uint32_t at=midi_mute_ledger.buckets[midi_mute_hash(key)];unsigned walked=0;
 while(at&&walked++<MM_BUCKET_LIMIT){MidiMuteEntry *e=midi_mute_entry(at);if(midi_mute_key_equal(&e->key,key))e->unmanaged=1;at=e->next;}
}
static void midi_mute_bind_pending(uint32_t root_id,const MidiMuteKey *old,const MidiMuteKey *pending){
 uint32_t ids[MM_BUCKET_LIMIT];unsigned n=midi_mute_find(old,ids);
 for(unsigned i=0;i<n;i++){MidiMuteEntry *e=midi_mute_entry(ids[i]);if(e->root!=root_id){midi_mute_fault(MM_ASSOCIATION);continue;}if(midi_mute_hash(pending)!=e->hash&&!midi_mute_index_room(pending)){midi_mute_fault(MM_CAPACITY);atomic_fetch_add(&midi_mute_projection()->unaccounted,1);midi_mute_contaminate(pending);midi_mute_release(ids[i]);continue;}midi_mute_unindex(ids[i]);midi_mute_index(ids[i],pending);}
}
#endif
