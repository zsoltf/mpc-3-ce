// Inside the existing DPF PluginVst owner. Touch input uses the same parameter
// normalization/storage as host automation and notifies its existing host.
    int mpcTouchHostParameter(uint32_t index,uint32_t event,MpcEditorParameter* out) {
        static_assert(CARDINAL_NUM_PARAMETERS == MPC_CARDINAL_HOST_PARAMETERS,
                      "Native host parameter projection must match this variant");
        if(index>=CARDINAL_NUM_PARAMETERS||!out)return 0;
        if(event==0){
            *out={};out->id=index;out->value=vst_getParameter(index);
            std::snprintf(out->name,sizeof(out->name),"%s",fPlugin.getParameterName(index).buffer());
            std::snprintf(out->display,sizeof(out->display),"%.3f V",fPlugin.getParameterValue(index));
            return 1;
        }
        if(event==MPC_EDITOR_PARAMETER_BEGIN||event==MPC_EDITOR_PARAMETER_END){
            hostCallback(event==MPC_EDITOR_PARAMETER_BEGIN?VST_HOST_OPCODE_2B:VST_HOST_OPCODE_2C,index);
            return 1;
        }
        if(event!=MPC_EDITOR_PARAMETER_SET)return 0;
        vst_setParameter(index,out->value);
        hostCallback(VST_HOST_OPCODE_00,index,0,nullptr,vst_getParameter(index));
        return 1;
    }
