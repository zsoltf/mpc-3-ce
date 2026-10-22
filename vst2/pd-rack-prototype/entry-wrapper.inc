#include "entry-api.h"
namespace { namespace entry_wrapper {
void* create(MpcUiIntentCallback callback,void* owner) {
    auto* app=new ENTRY_APP{};
    if(!ENTRY_INIT(app,64.0,callback,nullptr,owner)){delete app;return nullptr;}
    return app;
}
int visible(void* owner,int shown) {
    auto* app=static_cast<ENTRY_APP*>(owner);
    return shown?mpc_ui_screen_show(app->screen):mpc_ui_screen_hide(app->screen);
}
int close(void* owner) {
    auto* app=static_cast<ENTRY_APP*>(owner);
    if(app->screen)return ENTRY_CALLBACKS()->close(app);
    // An allocation can fail before the consumer creates its first screen.
    // Session is that existing consumer's presentation object, not its engine.
    delete session(app);app->session=nullptr;app->host=nullptr;return 1;
}
void destroy(void* owner){delete static_cast<ENTRY_APP*>(owner);}
int title(void* owner,const char* text){return ENTRY_TITLE(static_cast<ENTRY_APP*>(owner),text);}
int bind(void* owner,const void* effect,const char* path){
#ifdef ENTRY_BIND
    return ENTRY_BIND(static_cast<ENTRY_APP*>(owner),effect,path);
#else
    (void)owner;(void)effect;(void)path;return 0;
#endif
}
int parameters(void* owner,MpcEditorParameters* output){
    auto* app=static_cast<ENTRY_APP*>(owner);auto* s=session(app);auto* engine=::api(app);
    return engine&&s&&engine->parameters(s->token,output);
}
int parameter(void* owner,uint64_t binding,uint32_t index,uint32_t event,float value){
    auto* app=static_cast<ENTRY_APP*>(owner);auto* s=session(app);auto* engine=::api(app);
    if(!engine||!s)return 0;
    return engine->parameter(s->token,binding,index,event,value);
}
void present(void* owner){refresh(static_cast<ENTRY_APP*>(owner));}
void idle(void* owner){
#ifdef ENTRY_IDLE
    ENTRY_IDLE(static_cast<ENTRY_APP*>(owner));
#else
    (void)owner;
#endif
}
const MpcEditorEntryApi api={create,ENTRY_CALLBACKS(),visible,close,destroy,title,bind,parameters,parameter,present,idle};
} }
const MpcEditorEntryApi* ENTRY_EXPORT(){return &entry_wrapper::api;}
