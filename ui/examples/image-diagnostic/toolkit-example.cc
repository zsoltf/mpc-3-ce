/* Public API only. Staged as examples/toolkit-example.cc for the existing
 * admitted development app slot. No file, engine, timer or paint callback. */
#include "toolkit-example.h"
extern "C" int snprintf(char *, __SIZE_TYPE__, const char *, ...);
namespace {
int refresh(MpclearnToolkitExampleApp *a) {
    for (int y=0;y<241;++y) for (int x=0;x<321;++x) {
        unsigned char *p=a->pixels+4*(321*y+x);
        p[0]=x<107 ? 255 : 0; p[1]=x>=107 && x<214 ? 255 : 0;
        p[2]=x>=214 ? 255 : 0; p[3]=255;
        if(y>160) {p[0]/=3;p[1]/=3;p[2]/=3;}
        if(x<17 && y<31) p[0]=p[1]=p[2]=255;
        if(a->marked && x>=a->x-5 && x<=a->x+5 && y>=a->y-5 && y<=a->y+5)
            p[0]=p[1]=p[2]=255;
    }
    return mpc_ui_image_update(a->image,a->pixels,321*4,sizeof(a->pixels),++a->revision);
}
void pointer(void *owner,uint32_t event,float x,float y) {
    auto *a=(MpclearnToolkitExampleApp *)owner;
    uint32_t receipt;
    switch(event) {
    case MPC_UI_POINTER_DOWN: receipt=MPCLEARN_IMAGE_DIAGNOSTIC_DOWN;break;
    case MPC_UI_POINTER_MOVE: receipt=MPCLEARN_IMAGE_DIAGNOSTIC_MOVE;break;
    case MPC_UI_POINTER_UP: receipt=MPCLEARN_IMAGE_DIAGNOSTIC_UP;break;
    case MPC_UI_POINTER_CANCEL: receipt=MPCLEARN_IMAGE_DIAGNOSTIC_CANCEL;break;
    default:return;
    }
    a->action_callback(a->callback_owner,receipt);
    if(event==MPC_UI_POINTER_DOWN) ++a->down;
    if(event==MPC_UI_POINTER_MOVE) ++a->move;
    if(event==MPC_UI_POINTER_UP) ++a->up;
    if(event==MPC_UI_POINTER_CANCEL) ++a->cancel;
    char text[64];
    snprintf(text,sizeof(text),"D%u M%u U%u C%u  x%d y%d",a->down,a->move,a->up,a->cancel,(int)x,(int)y);
    (void)mpc_ui_label_text(a->status,text);
    int changed=!a->marked || a->x!=(int)x || a->y!=(int)y;
    a->x=(int)x;a->y=(int)y;a->marked=1;
    if(changed && event!=MPC_UI_POINTER_CANCEL && !refresh(a))
        (void)mpc_ui_label_text(a->status,"IMAGE UPDATE FAILED");
    // Rightmost strip is a deterministic hide-during-contact discriminator.
    if(event==MPC_UI_POINTER_DOWN && x>=301) (void)mpc_ui_control_visible(a->image,0);
}
void action(void *owner,uint32_t intent) {
    auto *a=(MpclearnToolkitExampleApp *)owner;
    if(intent==4) {
        if(mpc_ui_app_request_close(a->host)) a->action_callback(a->callback_owner,4);
    } else (void)mpc_ui_control_visible(a->image,1);
}
int open(void *owner,MpcUiAppHost *host,MpcUiContext *context,MpcUiParent *parent,MpcUiRect bounds) {
    auto *a=(MpclearnToolkitExampleApp *)owner;
    a->host=host;a->screen=mpc_ui_screen_create(context,parent,bounds);
    if(!a->screen)return 0;
    a->title=mpc_ui_label_create(a->screen,"IMAGE + TOUCH ABI DIAGNOSTIC");
    a->status=mpc_ui_label_create(a->screen,"White top-left; RGB left-right; dark bottom");
    a->image=mpc_ui_image_create(a->screen,321,241,pointer,a);
    auto *show=mpc_ui_button_create(a->screen,"SHOW IMAGE / RIGHT EDGE HIDES",1,action,a);
    auto *back=mpc_ui_button_create(a->screen,"BACK",4,action,a);
    MpcUiLabelStyle label={0xff202126,0xffffffff,36};
    MpcUiButtonStyle button={0xff2f2f34,0xffd31145,0xffffffff};
    if(!a->title||!a->status||!a->image||!show||!back ||
       !mpc_ui_label_style(a->title,label)||!mpc_ui_label_style(a->status,label)||
       !mpc_ui_button_style(show,button)||!mpc_ui_button_style(back,button)||
       !mpc_ui_control_place(a->title,{24,24,bounds.width-48,64})||
       !mpc_ui_control_place(a->status,{24,96,bounds.width-48,64})||
       !mpc_ui_control_place(a->image,{80,180,321,241})||
       !mpc_ui_control_place(show,{440,180,bounds.width-464,96})||
       !mpc_ui_control_place(back,{24,bounds.height-96,bounds.width-48,72})) {
        (void)mpc_ui_screen_close(&a->screen);a->host=nullptr;return 0;
    }
    a->revision=0;a->marked=0;
    if(refresh(a) && mpc_ui_screen_show(a->screen)) return 1;
    (void)mpc_ui_screen_close(&a->screen);a->host=nullptr;return 0;
}
int prepare(void *,int) {return MPC_UI_APP_CLOSE_READY;}
int close(void *owner) {
    auto *a=(MpclearnToolkitExampleApp *)owner;
    if(!mpc_ui_screen_close(&a->screen))return 0;
    a->host=nullptr;a->image=a->title=a->status=nullptr;return 1;
}
const MpcUiAppCallbacks callbacks={sizeof(MpcUiAppCallbacks),open,prepare,close};
}
extern "C" int mpclearn_toolkit_example_app_init(MpclearnToolkitExampleApp *a,double,
    MpcUiIntentCallback action_callback,MpcUiSliderCallback,void *owner) {
    if(!a||!action_callback||a->host||a->screen)return 0;
    a->action_callback=action_callback;a->callback_owner=owner;return 1;
}
extern "C" const MpcUiAppCallbacks *mpclearn_toolkit_example_app_callbacks(){return &callbacks;}
extern "C" int mpclearn_toolkit_example_set_title(MpclearnToolkitExampleApp *a,const char *text){return mpc_ui_label_text(a->title,text);}
extern "C" int mpclearn_toolkit_example_set_value(MpclearnToolkitExampleApp *,double){return 1;}
