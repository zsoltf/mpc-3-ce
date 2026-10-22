#ifndef MPCLEARN_PD_RACK_CONSUMER_H
#define MPCLEARN_PD_RACK_CONSUMER_H
#include "../include/mpclearn-ui.h"
enum MpclearnToolkitExampleIntent {
    MPCLEARN_TOOLKIT_EXAMPLE_ACTION_ONE=1,MPCLEARN_TOOLKIT_EXAMPLE_ACTION_TWO=2,
    MPCLEARN_TOOLKIT_EXAMPLE_SLIDER=3,MPCLEARN_TOOLKIT_EXAMPLE_BACK=4,
    MPCLEARN_TOOLKIT_EXAMPLE_GLOBAL_LEARN=5
};
struct MpclearnToolkitExampleApp {
    MpcUiAppHost* host;
    MpcUiScreen* screen;
    MpcUiControl* status;
    void* session;
    MpcUiIntentCallback callback;
    void* owner;
};
extern "C" {
int mpclearn_toolkit_example_app_init(MpclearnToolkitExampleApp*,double,MpcUiIntentCallback,MpcUiSliderCallback,void*);
const MpcUiAppCallbacks* mpclearn_toolkit_example_app_callbacks();
int mpclearn_toolkit_example_set_title(MpclearnToolkitExampleApp*,const char*);
int mpclearn_toolkit_example_set_value(MpclearnToolkitExampleApp*,double);
}
#endif
