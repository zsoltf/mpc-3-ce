/* One admitted native app entry, delegating to the two existing editor views. */
#include "toolkit-example.h"
#include "entry-api.h"
namespace {
struct Session {
    const MpcEditorEntryApi* api[2]={mpclearn_pd_editor_entry(),mpclearn_cardinal_editor_entry()};
    void* child[2]={nullptr,nullptr};
    int selected=-1;
};
Session* session(MpclearnToolkitExampleApp* app){return static_cast<Session*>(app->session);}
void action(void* owner,uint32_t intent) {
    auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);
    auto* state=session(app);
    if(intent==10){if(mpc_ui_app_request_close(app->host))app->callback(app->owner,MPCLEARN_TOOLKIT_EXAMPLE_BACK);return;}
    if(intent==22&&state&&state->selected<0){app->callback(app->owner,MPCLEARN_TOOLKIT_EXAMPLE_GLOBAL_LEARN);return;}
    if(!state||state->selected>=0||intent<20||intent>21)return;
    int selected=intent-20;
    if(!mpc_ui_screen_hide(app->screen))return;
    if(!state->api[selected]->visible(state->child[selected],1)){
        mpc_ui_screen_show(app->screen);
        mpc_ui_label_text(app->status,"Editor view unavailable; Back closes this entry");
        return;
    }
    state->selected=selected;
}
int open(void* owner,MpcUiAppHost* host,MpcUiContext* context,MpcUiParent* parent,MpcUiRect bounds) {
    auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);
    app->host=host;app->session=new Session;
    auto* state=session(app);
    app->screen=mpc_ui_screen_create(context,parent,bounds);
    if(!app->screen)return 0;
    auto* title=mpc_ui_label_create(app->screen,"PATCH EDITORS / MPC PLUG-IN INSTANCES");
    app->status=mpc_ui_label_create(app->screen,"Choose an editor, then explicitly select its live MPC plug-in instance");
    auto* pd=mpc_ui_button_create(app->screen,"PD SKETCHES",20,action,app);
    auto* cardinal=mpc_ui_button_create(app->screen,"CARDINAL MODULAR",21,action,app);
    auto* learn=mpc_ui_button_create(app->screen,"GLOBAL MIDI LEARN",22,action,app);
    auto* back=mpc_ui_button_create(app->screen,"BACK",10,action,app);
    MpcUiLabelStyle label={0xff202126,0xffffffff,30};
    MpcUiButtonStyle buttons={0xff2f2f34,0xffd31145,0xffffffff};
    int width=(bounds.width-96)/2;
    bool okay=title&&app->status&&pd&&cardinal&&learn&&back&&
        mpc_ui_label_style(title,label)&&mpc_ui_label_style(app->status,label)&&
        mpc_ui_control_place(title,{32,24,bounds.width-64,64})&&
        mpc_ui_control_place(app->status,{32,bounds.height-96,bounds.width-64,64})&&
        mpc_ui_button_style(pd,buttons)&&mpc_ui_button_style(cardinal,buttons)&&mpc_ui_button_style(back,buttons)&&
        mpc_ui_control_place(pd,{32,160,width,180})&&
        mpc_ui_control_place(cardinal,{64+width,160,width,180})&&
        mpc_ui_button_style(learn,buttons)&&mpc_ui_control_place(learn,{32,370,bounds.width-64,80})&&
        mpc_ui_control_place(back,{32,bounds.height-180,180,64});
    if(!okay)return 0;
    // The native adapter defers app open to its existing UI drain, where screen
    // cleanup is permitted. Selection callbacks only change visibility; they
    // never destroy a screen while its widget callback is still on the stack.
    // Discovery creates no plug-in, Pd canvas or Rack context.
    for(int i=0;i<2;++i){
        state->child[i]=state->api[i]->create(app->callback,app->owner);
        if(!state->child[i]||!state->api[i]->callbacks->open(state->child[i],host,context,parent,bounds)||
           !state->api[i]->visible(state->child[i],0))return 0;
    }
    return mpc_ui_screen_show(app->screen);
}
int prepare(void* owner,int forced) {
    auto* state=session(static_cast<MpclearnToolkitExampleApp*>(owner));
    if(!state)return MPC_UI_APP_CLOSE_READY;
    for(int i=0;i<2;++i)if(state->child[i]){
        int result=state->api[i]->callbacks->prepare_close(state->child[i],forced);
        if(result!=MPC_UI_APP_CLOSE_READY)return result;
    }
    return MPC_UI_APP_CLOSE_READY;
}
int close(void* owner) {
    auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);
    auto* state=session(app);
    if(state)for(int i=1;i>=0;--i)if(state->child[i]){
        if(!state->api[i]->close(state->child[i]))return 0;
        state->api[i]->destroy(state->child[i]);state->child[i]=nullptr;
    }
    if(app->screen&&!mpc_ui_screen_close(&app->screen))return 0;
    delete state;app->session=nullptr;app->status=nullptr;app->host=nullptr;return 1;
}
const MpcUiAppCallbacks callbacks={sizeof(MpcUiAppCallbacks),open,prepare,close};
}
extern "C" int mpclearn_toolkit_example_app_init(MpclearnToolkitExampleApp* app,double,MpcUiIntentCallback callback,MpcUiSliderCallback,void* owner){
    if(!app||app->host||app->screen||!callback)return 0;
    app->callback=callback;app->owner=owner;return 1;
}
extern "C" const MpcUiAppCallbacks* mpclearn_toolkit_example_app_callbacks(){return &callbacks;}
extern "C" int mpclearn_toolkit_example_set_title(MpclearnToolkitExampleApp* app,const char* text){
    auto* state=session(app);
    return state&&state->selected>=0?state->api[state->selected]->title(state->child[state->selected],text):mpc_ui_label_text(app->status,text);
}
extern "C" int mpclearn_toolkit_example_set_value(MpclearnToolkitExampleApp*,double){return 1;}
