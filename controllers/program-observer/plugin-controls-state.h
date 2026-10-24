/* Bounded app intent inside the existing command domain. UI owns demand/intent
 * cells; the sole bridge owns arbitration/status. Neither is parameter state. */
#ifndef MPC_PLUGIN_CONTROLS_STATE_H
#define MPC_PLUGIN_CONTROLS_STATE_H
typedef struct {uint32_t session,active,epoch,serial,slot,page;} PluginUiDemand;
typedef struct {uint32_t session,arbitration,epoch,serial,generation,slot,page,index,key,ap,bits;} PluginUiIntent;
typedef struct {uint32_t arbitration,controller,epoch,serial,slot,page,submitted,settled,refused,error;} PluginUiReply;
typedef struct {_Atomic uint32_t revision,words[11];} PluginUiCell;
typedef struct {
 PluginUiCell demand,intents[8],reply;
 _Atomic uint32_t until,reply_until;
} PluginUiState;
static inline int plugin_ui_read(const PluginUiCell *c,void *value,unsigned bytes){
 uint32_t rev=atomic_load_explicit(&c->revision,memory_order_acquire),words[11];
 if(!rev||(rev&1)||bytes>sizeof(words))return 0;
 for(unsigned i=0;i<bytes/4;i++)words[i]=atomic_load_explicit(c->words+i,memory_order_relaxed);
 atomic_thread_fence(memory_order_acquire);
 if(rev!=atomic_load_explicit(&c->revision,memory_order_relaxed))return 0;
 memcpy(value,words,bytes);return 1;
}
/* Only the cell's declared producer calls this; no cross-owner CAS or wait. */
static inline int plugin_ui_write(PluginUiCell *c,const void *value,unsigned bytes){
 uint32_t rev=atomic_load_explicit(&c->revision,memory_order_relaxed),words[11];
 if((rev&1)||rev>UINT32_MAX-2||bytes>sizeof(words))return 0;
 memcpy(words,value,bytes);atomic_store_explicit(&c->revision,rev+1,memory_order_release);
 atomic_thread_fence(memory_order_release);
 for(unsigned i=0;i<bytes/4;i++)atomic_store_explicit(c->words+i,words[i],memory_order_relaxed);
 atomic_store_explicit(&c->revision,rev+2,memory_order_release);return 1;
}
#endif
