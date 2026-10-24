#pragma once
// Selected by the same admitted model profile as the native track host.
// Coordinates are admitted by prepare-gpu.py before presenter installation.
#ifdef MODEL_FIRMWARE_310
#define GPU_RENDERER_VPTR 0x6c931f4u
#define GPU_RENDERER_READBACK 0xae4e44u
#define GPU_CLIP_VPTR 0x6c9300cu
#define GPU_IMAGE_VPTR 0x6cd200cu
#define GPU_ROOT_SPAN 0x884u
#define GPU_ROOT_VPTR 0x6ce5650u
#define GPU_ROOT_TMI 0x880u
#define GPU_TMI_SPAN 0x3740u
#define GPU_TMI_VPTR 0x6dafdd0u
#define GPU_CONTAINER_VPTR 0x6dd3d3cu
#define GPU_CONTAINER_COMPONENT_VPTR 0x6dd3de4u
#define GPU_TMI_PANEL 0x3738u
#define GPU_PANEL_VPTR 0x6db1130u
#define GPU_TMI_SHORTCUT 0x373cu
#define GPU_SHORTCUT_VPTR 0x6ddcd50u
#define GPU_MANAGER_SLOT 0x6f35fd4u
#define GPU_SCREEN_VPTR 0x6c95ba8u
#define GPU_FLIPPER_VPTR 0x6c95be8u
#define GPU_MODAL_SLOT 0x6f35fe0u
#define GPU_MODAL_VPTR 0x6c95e44u
#define GPU_MODAL_SECONDARY_VPTR 0x6c95e58u
#define GPU_MODAL_ITEM_VPTR 0x6c93314u
#else
#define GPU_RENDERER_VPTR 0x688bdecu
#define GPU_RENDERER_READBACK 0xac21f4u
#define GPU_CLIP_VPTR 0x688bc04u
#define GPU_IMAGE_VPTR 0x68c8b94u
#define GPU_ROOT_SPAN 0x840u
#define GPU_ROOT_VPTR 0x68d7bf8u
#define GPU_ROOT_TMI 0x83cu
#define GPU_TMI_SPAN 0x3710u
#define GPU_TMI_VPTR 0x699efd0u
#define GPU_CONTAINER_VPTR 0x69c3090u
#define GPU_CONTAINER_COMPONENT_VPTR 0x69c3138u
#define GPU_TMI_PANEL 0x3708u
#define GPU_PANEL_VPTR 0x69a03c4u
#define GPU_TMI_SHORTCUT 0x370cu
#define GPU_SHORTCUT_VPTR 0x69cc03cu
#define GPU_MANAGER_SLOT 0x6b220d4u
#define GPU_SCREEN_VPTR 0x688e7a0u
#define GPU_FLIPPER_VPTR 0x688e7e0u
#define GPU_MODAL_SLOT 0x6b220e0u
#define GPU_MODAL_VPTR 0x688ea3cu
#define GPU_MODAL_SECONDARY_VPTR 0x688ea50u
#define GPU_MODAL_ITEM_VPTR 0x688bf0cu
#endif
