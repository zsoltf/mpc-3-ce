#ifndef MPCLEARN_PLUGIN_CONTROLS_SCREEN_H
#define MPCLEARN_PLUGIN_CONTROLS_SCREEN_H
#include "../include/mpclearn-ui.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MPCLEARN_PLUGIN_ROWS 8u
enum { MPCLEARN_PLUGIN_PREVIOUS_INSERT=1, MPCLEARN_PLUGIN_NEXT_INSERT,
       MPCLEARN_PLUGIN_PREVIOUS_PAGE, MPCLEARN_PLUGIN_NEXT_PAGE,
       MPCLEARN_PLUGIN_BACK };
typedef struct {
    uint32_t binding, navigation;
    char title[96], status[96];
    struct { uint32_t ready; double value; char name[32], text[32]; } rows[8];
} MpclearnPluginControlsSnapshot;
typedef struct {
    MpcUiScreen *screen;
    MpcUiControl *title, *status, *names[8], *values[8], *buttons[5];
    MpcUiSlider *sliders[8];
    MpcUiIntentCallback action;
    MpcUiSliderCallback slider;
    void *owner;
    uint32_t held[8], ready[8], binding;
    MpclearnPluginControlsSnapshot shown;
} MpclearnPluginControlsScreen;
int mpclearn_plugin_controls_open(MpclearnPluginControlsScreen *, MpcUiContext *,
    MpcUiParent *, MpcUiRect, MpcUiIntentCallback, MpcUiSliderCallback, void *);
int mpclearn_plugin_controls_refresh(MpclearnPluginControlsScreen *,
    const MpclearnPluginControlsSnapshot *);
void mpclearn_plugin_controls_cancel(MpclearnPluginControlsScreen *);
int mpclearn_plugin_controls_close(MpclearnPluginControlsScreen *);
#ifdef __cplusplus
}
#endif
#endif
