#ifndef MPCLEARN_FULL_CARDINAL_CONSUMER_H
#define MPCLEARN_FULL_CARDINAL_CONSUMER_H
#include "../include/mpclearn-ui.h"
enum MpclearnToolkitExampleIntent {
    MPCLEARN_TOOLKIT_EXAMPLE_ACTION_ONE=1,MPCLEARN_TOOLKIT_EXAMPLE_ACTION_TWO=2,
    MPCLEARN_TOOLKIT_EXAMPLE_SLIDER=3,MPCLEARN_TOOLKIT_EXAMPLE_BACK=4
};
struct MpclearnToolkitExampleApp {
    MpcUiAppHost* host;
    MpcUiScreen* screen;
    MpcUiControl *image,*status;
    MpcUiList* list;
    MpcUiControl* keys[40];
    void* session;
    MpcUiIntentCallback callback;
    void* owner;
    uint32_t width,height;
    const void* effect;
    char modulePath[512];
};
extern "C" {
int mpclearn_toolkit_example_app_init(MpclearnToolkitExampleApp*,double,MpcUiIntentCallback,MpcUiSliderCallback,void*);
const MpcUiAppCallbacks* mpclearn_toolkit_example_app_callbacks(void);
int mpclearn_toolkit_example_set_title(MpclearnToolkitExampleApp*,const char*);
int mpclearn_toolkit_example_set_value(MpclearnToolkitExampleApp*,double);
int mpclearn_toolkit_example_bind(MpclearnToolkitExampleApp*,const void*,const char*);
}
#endif
