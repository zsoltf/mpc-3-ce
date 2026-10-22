// Native projection of the bound VST host parameters, separate from Rack's
// selected-module Q-Links. Values always come back through the DPF owner.
void hostControlsClose(MpclearnToolkitExampleApp* app){
    auto* s=session(app);auto* engine=api(app);
    if(engine&&s->hostValues.binding)engine->host_parameters(s->token,MPC_CARDINAL_HOST_CLOSE,&s->hostValues);
    s->hostValues={};s->hostDragging=false;
    mpc_ui_list_visible(s->hostList,0);mpc_ui_slider_visible(s->hostSlider,0);mpc_ui_control_visible(s->hostLabel,0);
}
void hostControlsDisplay(MpclearnToolkitExampleApp* app){
    auto* s=session(app);if(s->hostIndex>=s->hostValues.count)return;
    auto& value=s->hostValues.parameters[s->hostIndex];
    char label[160];std::snprintf(label,sizeof(label),"%s: %s",value.name,value.display);
    mpc_ui_label_text(s->hostLabel,label);mpc_ui_slider_set_value(s->hostSlider,value.value);
}
void hostControlsRead(MpclearnToolkitExampleApp* app){
    auto* s=session(app);auto* engine=api(app);if(!engine||!s->hostValues.binding)return;
    auto values=s->hostValues;
    if(!engine->host_parameters(s->token,MPC_CARDINAL_HOST_READ,&values)){
        hostControlsClose(app);s->shell.show(mpc_editor::ACTIONS);status(app,"Host parameters unavailable");return;
    }
    if(std::memcmp(&values,&s->hostValues,sizeof(values))){
        s->hostValues=values;mpc_ui_list_update(s->hostList,values.count,s->hostIndex);hostControlsDisplay(app);
    }
}
const char* hostRow(void* owner,uint32_t row){
    auto* s=session(static_cast<MpclearnToolkitExampleApp*>(owner));
    return row<s->hostValues.count?s->hostValues.parameters[row].name:"";
}
void hostSelected(void* owner,uint32_t row){
    auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);auto* engine=api(app);
    if(!engine||s->shell.page!=mpc_editor::HOST_PARAMETERS||!s->hostValues.binding||row>=s->hostValues.count)return;
    if(s->hostDragging)engine->host_parameter(s->token,s->hostValues.binding,s->hostIndex,MPC_EDITOR_PARAMETER_END,0);
    s->hostDragging=false;s->hostIndex=row;hostControlsRead(app);hostControlsDisplay(app);
}
void hostSlider(void* owner,uint32_t,uint32_t event,double value){
    auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);auto* engine=api(app);
    if(!engine||s->shell.page!=mpc_editor::HOST_PARAMETERS||!s->hostValues.binding||!std::isfinite(value))return;
    unsigned operation=0;
    if(event==MPC_UI_SLIDER_DRAG_STARTED){if(s->hostDragging)return;s->hostDragging=true;operation=MPC_EDITOR_PARAMETER_BEGIN;}
    else if(event==MPC_UI_SLIDER_VALUE_CHANGED){if(!s->hostDragging)return;operation=MPC_EDITOR_PARAMETER_SET;}
    else if(event==MPC_UI_SLIDER_DRAG_ENDED){if(!s->hostDragging)return;s->hostDragging=false;operation=MPC_EDITOR_PARAMETER_END;}
    else return;
    if(!engine->host_parameter(s->token,s->hostValues.binding,s->hostIndex,operation,value)){
        hostControlsClose(app);s->shell.show(mpc_editor::ACTIONS);status(app,"Host parameter owner changed");return;
    }
    hostControlsRead(app);
}
bool hostControlsCreate(MpclearnToolkitExampleApp* app){
    auto* s=session(app);const auto rail=s->shell.rail;
    s->hostList=mpc_ui_list_create(app->screen,"HOST PARAMETERS",0,56,hostRow,hostSelected,app);
    s->hostLabel=mpc_ui_label_create(app->screen,"Host parameters");
    s->hostSlider=mpc_ui_slider_create(app->screen,0,1,0,0,MPC_UI_SLIDER_HORIZONTAL,0,hostSlider,app);
    if(!s->hostList||!s->hostLabel||!s->hostSlider)return false;
    if(!mpc_ui_list_style(s->hostList,{0xff2f2f34,0xffd31145,0xffe1e2e5})||
       !mpc_ui_list_place(s->hostList,{rail.x,rail.y+64,rail.width,rail.height-192})||
       !mpc_ui_list_activate_on_click(s->hostList,1)||
       !mpc_ui_label_style(s->hostLabel,{0xff202126,0xffe1e2e5,18})||
       !mpc_ui_control_place(s->hostLabel,{rail.x,rail.y+rail.height-120,rail.width,48})||
       !mpc_ui_slider_place(s->hostSlider,{rail.x,rail.y+rail.height-64,rail.width,56}))return false;
    hostControlsClose(app);return true;
}
void hostControlsOpen(MpclearnToolkitExampleApp* app){
    auto* s=session(app);s->hostValues={};
    if(!api(app)->host_parameters(s->token,MPC_CARDINAL_HOST_OPEN,&s->hostValues)){
        status(app,"Host parameters unavailable");return;
    }
    s->hostIndex=0;s->shell.show(mpc_editor::HOST_PARAMETERS);
    mpc_ui_list_update(s->hostList,s->hostValues.count,0);hostControlsDisplay(app);
    mpc_ui_list_visible(s->hostList,1);mpc_ui_control_visible(s->hostLabel,1);mpc_ui_slider_visible(s->hostSlider,1);
    status(app,"Host parameters: choose a parameter, then adjust its value");
}
