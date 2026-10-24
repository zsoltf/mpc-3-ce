#include "transport-queue.h"
#include "model-layout.h"
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

/* MPC's queue callable is not a libstdc++ std::function. The observed ARM ABI
 * is two function words followed by one 12-byte inline capture. */
typedef struct {uint32_t owner,job,callback;} TransportQueueCapture;
typedef struct {uint32_t invoke,manager;TransportQueueCapture capture;} TransportFunction;
_Static_assert(sizeof(TransportQueueCapture)==12,"native queue capture ABI");
_Static_assert(sizeof(TransportFunction)==20,"native queue function ABI");
#ifdef COMMAND_COMPONENT
static unsigned manager_count[3];
#endif
static unsigned transport_manager(unsigned operation,const void *source,void *destination){
 /* Exact native callable manager contract: 0 copies inline capture, 1 destroys,
  * and 2 reports a trivially movable capture. The queue owns no allocation. */
#ifdef COMMAND_COMPONENT
 if(operation<3)manager_count[operation]++;
#endif
 if(operation==0){memcpy(destination,source,sizeof(TransportQueueCapture));return 0;}
 return operation==2;
}
static void release(uint32_t owner){atomic_fetch_sub_explicit((_Atomic uint32_t *)(uintptr_t)(owner+4),1,memory_order_seq_cst);}
static void transport_invoke(const void *storage){
 const TransportQueueCapture *capture=storage;
 ((TransportAudioCallback)(uintptr_t)capture->callback)((void*)(uintptr_t)capture->job);release(capture->owner);
}

#ifdef COMMAND_COMPONENT
static TransportFunction pending,claimed;static int full,has_claimed,canceled,force_failure;
static unsigned empty_manager(unsigned operation,const void *source,void *destination){(void)source;(void)destination;return operation==2;}
void transport_queue_component_reset(void){memset(&pending,0,sizeof(pending));memset(&claimed,0,sizeof(claimed));memset(manager_count,0,sizeof(manager_count));full=has_claimed=canceled=force_failure=0;}
void transport_queue_component_fail(int fail){force_failure=fail!=0;}
unsigned transport_queue_component_pending(void){return full?1u:0u;}
unsigned transport_queue_component_manager_count(unsigned operation){return operation<3?manager_count[operation]:0;}
unsigned transport_queue_component_trivial(void){TransportQueueCapture capture={0};return transport_manager(2,&capture,0);}
unsigned transport_queue_component_cancel(void){
 /* Native pre-claim cancellation calls neither invoke nor manager. The native
  * Editor drain subtracts the canceled count from its holder reference word. */
 if(!full)return 0;
 uint32_t owner=pending.capture.owner;full=0;canceled=1;release(owner);return 1;
}
void transport_queue_component_reclaim(void){
 /* Native cancellation retains the moved header/capture until slot reuse or
  * ring destruction. Its later manager call must not settle/release twice. */
 if(!canceled)return;
 ((unsigned (*)(unsigned,const void *,void *))(uintptr_t)pending.manager)(1,&pending.capture,0);
 memset(&pending,0,sizeof(pending));canceled=0;
}
unsigned transport_queue_component_claim(void){
 if(!full||has_claimed)return 0;
 claimed=pending;memset(&pending,0,sizeof(pending));full=0;has_claimed=1;return 1;
}
void transport_queue_component_run_claimed(void){
 if(!has_claimed)return;
 TransportFunction current=claimed;memset(&claimed,0,sizeof(claimed));has_claimed=0;
 ((void (*)(const void *))(uintptr_t)current.invoke)(&current.capture);
 ((unsigned (*)(unsigned,const void *,void *))(uintptr_t)current.manager)(1,&current.capture,0);
}
void transport_queue_component_run(void){
 if(transport_queue_component_claim())transport_queue_component_run_claimed();
}
#endif

unsigned transport_queue_submit(uint32_t image_bias,uint32_t owner,void *job,TransportAudioCallback callback){
 if(!owner||!job||!callback)return 0;
 TransportFunction function={(uint32_t)(uintptr_t)transport_invoke,(uint32_t)(uintptr_t)transport_manager,{owner,(uint32_t)(uintptr_t)job,(uint32_t)(uintptr_t)callback}};
 unsigned accepted=0;
 atomic_fetch_add_explicit((_Atomic uint32_t *)(uintptr_t)(owner+4),1,memory_order_seq_cst);
#ifdef COMMAND_COMPONENT
 (void)image_bias;
 if(!force_failure&&!full){
  transport_queue_component_reclaim();
  pending.invoke=function.invoke;pending.manager=function.manager;
  ((unsigned (*)(unsigned,const void *,void *))(uintptr_t)function.manager)(0,&function.capture,&pending.capture);
  function.invoke=0;function.manager=(uint32_t)(uintptr_t)empty_manager;
  full=accepted=1;
 }
#else
 uint32_t queue=*(const volatile uint32_t *)(uintptr_t)owner;
 if(!queue){release(owner);return 0;}
 typedef void (*Lock)(void *);typedef unsigned (*Push)(void *,void *,void *);
 Lock lock=(Lock)(uintptr_t)(image_bias+MODEL_QUEUE_LOCK),unlock=(Lock)(uintptr_t)(image_bias+MODEL_QUEUE_UNLOCK);
 Push push=(Push)(uintptr_t)(image_bias+MODEL_QUEUE_PUSH);
 lock((void *)(uintptr_t)(queue+MODEL_QUEUE_MUTEX));
 accepted=push((void *)(uintptr_t)(queue+MODEL_QUEUE_AUDIO),&function,(void *)(uintptr_t)owner);
 ((unsigned (*)(unsigned,const void *,void *))(uintptr_t)function.manager)(1,&function.capture,0);
 unlock((void *)(uintptr_t)(queue+MODEL_QUEUE_MUTEX));
#endif
 /* Push swaps the headers. Destroy through the returned manager, as native
  * producers do, rather than applying our capture manager to a moved value. */
#ifdef COMMAND_COMPONENT
 ((unsigned (*)(unsigned,const void *,void *))(uintptr_t)function.manager)(1,&function.capture,0);
#endif
 if(!accepted)release(owner);
 return accepted;
}
