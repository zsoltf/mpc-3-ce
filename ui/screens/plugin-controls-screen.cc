/* Presentation only: no host addresses, parameter storage or command writer. */
#include "plugin-controls-screen.h"
extern "C" void *memset(void *, int, __SIZE_TYPE__) noexcept;
extern "C" int memcmp(const void *, const void *, __SIZE_TYPE__) noexcept;
namespace {
void action(void *owner, uint32_t intent) {
    auto *s=(MpclearnPluginControlsScreen*)owner;
    if (intent!=MPCLEARN_PLUGIN_BACK && !s->shown.navigation) return;
    s->action(s->owner,intent);
}
void slider(void *owner,uint32_t row,uint32_t event,double value) {
    auto *s=(MpclearnPluginControlsScreen*)owner;
    if(row>=8) return;
    if(event==MPC_UI_SLIDER_DRAG_STARTED) {
        if(s->held[row]) return;
        s->held[row]=s->ready[row]?s->binding:0;
    }
    if(s->ready[row]&&s->held[row]&&s->held[row]==s->binding)
        s->slider(s->owner,row,event,value);
    if(event==MPC_UI_SLIDER_DRAG_ENDED) s->held[row]=0;
}
}
extern "C" int mpclearn_plugin_controls_open(MpclearnPluginControlsScreen *s,
    MpcUiContext *context,MpcUiParent *parent,MpcUiRect bounds,
    MpcUiIntentCallback action_callback,MpcUiSliderCallback slider_callback,void *owner) {
    if(!s||s->screen||!action_callback||!slider_callback||bounds.width<1000||bounds.height<620) return 0;
    s->action=action_callback;s->slider=slider_callback;s->owner=owner;
    s->screen=mpc_ui_screen_create(context,parent,bounds);
    if(!s->screen) return 0;
    MpcUiLabelStyle label={0xff202126u,0xffffffffu,36};
    s->title=mpc_ui_label_create(s->screen,"PLUGIN CONTROLS");
    s->status=mpc_ui_label_create(s->screen,"Waiting for selected track");
    bool ok=s->title&&s->status&&mpc_ui_label_style(s->title,label)&&
        mpc_ui_label_style(s->status,label)&&
        mpc_ui_control_place(s->title,{24,8,bounds.width-48,40})&&
        mpc_ui_control_place(s->status,{24,48,bounds.width-48,36});
    int column=(bounds.width-72)/4;
    for(unsigned i=0;i<6&&ok;i++) {
        int x=i<4?24+(int)i*(column+8):24;
        int y=i<4?96:bounds.height-252+(int)(i-4)*80;
        s->names[i]=mpc_ui_label_create(s->screen,"");
        s->values[i]=mpc_ui_label_create(s->screen,"");
        s->sliders[i]=mpc_ui_slider_create(s->screen,0,1,0,0,
            i<4?MPC_UI_SLIDER_VERTICAL:MPC_UI_SLIDER_HORIZONTAL,i,slider,s);
        ok=s->names[i]&&s->values[i]&&s->sliders[i]&&
            mpc_ui_label_style(s->names[i],label)&&mpc_ui_label_style(s->values[i],label)&&
            mpc_ui_control_place(s->names[i],{x,y,i<4?column:260,40})&&
            mpc_ui_slider_place(s->sliders[i],i<4?MpcUiRect{x+column/4,y+48,column/2,bounds.height-420}:MpcUiRect{296,y,bounds.width-580,64})&&
            mpc_ui_control_place(s->values[i],i<4?MpcUiRect{x,bounds.height-292,column,40}:MpcUiRect{bounds.width-272,y,248,64})&&
            mpc_ui_slider_visible(s->sliders[i],0);
    }
    const char *names[]={"PREV INSERT","NEXT INSERT","PREV PAGE","NEXT PAGE","BACK"};
    int width=(bounds.width-72)/5;
    for(unsigned i=0;i<5&&ok;i++) {
        s->buttons[i]=mpc_ui_button_create(s->screen,names[i],i+1,action,s);
        ok=s->buttons[i]&&mpc_ui_button_style(s->buttons[i],{0xff2f2f34u,0xffd31145u,0xffffffffu})&&
            mpc_ui_control_place(s->buttons[i],{24+(int)i*(width+6),bounds.height-76,width,56});
    }
    if(ok&&mpc_ui_screen_show(s->screen)) return 1;
    (void)mpclearn_plugin_controls_close(s);return 0;
}
extern "C" void mpclearn_plugin_controls_cancel(MpclearnPluginControlsScreen *s) {
    if(!s) return;
    /* A held old pointer must release before it can start a new binding. */
    for(unsigned i=0;i<8;i++) if(s->held[i]) s->held[i]=0xffffffffu;
    s->binding=0;
}
extern "C" int mpclearn_plugin_controls_refresh(MpclearnPluginControlsScreen *s,
    const MpclearnPluginControlsSnapshot *v) {
    if(!s||!s->screen||!v) return 0;
    if(s->binding!=v->binding) mpclearn_plugin_controls_cancel(s);
    s->binding=v->binding;
    bool ok=true;
    if(memcmp(s->shown.title,v->title,sizeof(v->title))) ok=mpc_ui_label_text(s->title,v->title)&&ok;
    if(memcmp(s->shown.status,v->status,sizeof(v->status))) ok=mpc_ui_label_text(s->status,v->status)&&ok;
    for(unsigned i=0;i<6;i++) {
        if(memcmp(s->shown.rows[i].name,v->rows[i].name,32)) ok=mpc_ui_label_text(s->names[i],v->rows[i].name)&&ok;
        if(memcmp(s->shown.rows[i].text,v->rows[i].text,32)) ok=mpc_ui_label_text(s->values[i],v->rows[i].text)&&ok;
        if(s->ready[i]!=v->rows[i].ready) ok=mpc_ui_slider_visible(s->sliders[i],v->rows[i].ready)&&ok;
        s->ready[i]=v->rows[i].ready;
        if(v->rows[i].ready&&!s->held[i]) ok=mpc_ui_slider_set_value(s->sliders[i],v->rows[i].value)&&ok;
    }
    if(s->shown.navigation!=v->navigation) for(unsigned i=0;i<4;i++)
        ok=mpc_ui_control_visible(s->buttons[i],v->navigation)&&ok;
    s->shown=*v;return ok;
}
extern "C" int mpclearn_plugin_controls_close(MpclearnPluginControlsScreen *s) {
    if(!s||!s->screen) return 0;
    mpclearn_plugin_controls_cancel(s);
    if(!mpc_ui_screen_close(&s->screen)) return 0;
    memset(s,0,sizeof(*s));return 1;
}
