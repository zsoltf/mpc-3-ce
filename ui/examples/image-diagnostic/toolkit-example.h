#ifndef MPCLEARN_IMAGE_DIAGNOSTIC_H
#define MPCLEARN_IMAGE_DIAGNOSTIC_H
/* Build-only consumer with an explicit, diagnostic-only host receipt branch. */
#define MPCLEARN_IMAGE_DIAGNOSTIC_RECEIPTS 1
#include "../../include/mpclearn-ui.h"
enum MpclearnToolkitExampleIntent {
    MPCLEARN_TOOLKIT_EXAMPLE_ACTION_ONE=1, MPCLEARN_TOOLKIT_EXAMPLE_ACTION_TWO=2,
    MPCLEARN_TOOLKIT_EXAMPLE_SLIDER=3, MPCLEARN_TOOLKIT_EXAMPLE_BACK=4
};
enum MpclearnImageDiagnosticReceipt {
    MPCLEARN_IMAGE_DIAGNOSTIC_DOWN = 0x201,
    MPCLEARN_IMAGE_DIAGNOSTIC_MOVE = 0x202,
    MPCLEARN_IMAGE_DIAGNOSTIC_UP = 0x203,
    MPCLEARN_IMAGE_DIAGNOSTIC_CANCEL = 0x204,
};
struct MpclearnToolkitExampleApp {
    MpcUiAppHost *host;
    MpcUiScreen *screen;
    MpcUiControl *image, *title, *status;
    MpcUiIntentCallback action_callback;
    void *callback_owner;
    uint32_t revision, down, move, up, cancel;
    int x, y, marked;
    unsigned char pixels[321 * 241 * 4];
};
extern "C" {
int mpclearn_toolkit_example_app_init(MpclearnToolkitExampleApp *, double,
    MpcUiIntentCallback, MpcUiSliderCallback, void *);
const MpcUiAppCallbacks *mpclearn_toolkit_example_app_callbacks(void);
int mpclearn_toolkit_example_set_title(MpclearnToolkitExampleApp *, const char *);
int mpclearn_toolkit_example_set_value(MpclearnToolkitExampleApp *, double);
}
#endif
