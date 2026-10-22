#ifndef MPC_CARDINAL_OVERLAY_API_H
#define MPC_CARDINAL_OVERLAY_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Private temporary contract, separate from the unchanged public touch ABI.
// fd and allocation generation identify observer-owned, linear AR24 storage.
// render returns after GPU completion. The owner retains fd through release.
typedef struct {uint32_t size,slot,width,height,pitch,fourcc;int32_t dma_fd;uint64_t allocation_generation;} MpcCardinalOverlayTarget;
typedef struct {
 uint32_t size,abi;
 int (*render)(uint64_t token,const MpcCardinalOverlayTarget*,uint32_t* revision);
 void (*release)(uint64_t token);
} MpcCardinalOverlayApi;
const MpcCardinalOverlayApi* mpclearn_cardinal_overlay_api(uint32_t abi);
#ifdef __cplusplus
}
#endif
#endif
