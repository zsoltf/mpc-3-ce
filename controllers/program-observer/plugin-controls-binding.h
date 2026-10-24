#ifndef MPC_PLUGIN_CONTROLS_BINDING_H
#define MPC_PLUGIN_CONTROLS_BINDING_H
#include "../../ui/screens/plugin-controls-screen.h"
#ifdef __cplusplus
extern "C" {
#endif
/* All calls on the existing admitted UI thread; no native getter in callbacks. */
void plugin_controls_active(int);
void plugin_controls_navigate(uint32_t);
void plugin_controls_gesture(uint32_t,uint32_t,uint32_t,double);
int plugin_controls_snapshot(MpclearnPluginControlsSnapshot *);
#ifdef __cplusplus
}
#endif
#endif
