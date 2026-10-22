/* Touch intent only. Rack owns the preview, cable, history and real widgets. */
std::string portLabel(rack::app::PortWidget* port) {
    auto* info = port ? port->getPortInfo() : nullptr;
    if (!info) return "Port";
    return info->getFullName();
}

bool visibleInRack(rack::widget::Widget* widget, rack::app::RackWidget* root) {
    for (auto* at=widget;at;at=at->parent) {
        if (!at->visible) return false;
        if (at==root) return true;
    }
    return false;
}

rack::app::ParamWidget* nearbyControl(Instance& instance, rack::math::Vec position, bool& ambiguous) {
    auto* rack=instance.context->scene->rack;
    rack::app::ParamWidget* result=nullptr;
    ambiguous=false;
    for (auto* module : rack->getModules()) for (auto* param : module->getParams()) {
        if (!param->getParamQuantity() || !visibleInRack(param,rack)) continue;
        const auto center=param->getAbsoluteOffset(param->box.size.div(2));
        if (position.minus(center).norm()>22.f) continue;
        if (result && result!=param) {ambiguous=true;return nullptr;}
        result=param;
    }
    return result;
}

rack::app::PortWidget* hitPort(Instance& instance, rack::math::Vec position, bool& ambiguous) {
    auto* rack=instance.context->scene->rack;
    rack::app::PortWidget* exact=nullptr;
    rack::app::PortWidget* enlarged=nullptr;
    unsigned near=0;
    ambiguous=false;
    const auto viewport=instance.context->scene->rackScroll->box;
    if (!viewport.contains(position)) return nullptr;
    for (auto* module : rack->getModules()) for (auto* port : module->getPorts()) {
        if (!port->module || !visibleInRack(port,rack)) continue;
        const auto center=port->getAbsoluteOffset(port->box.size.div(2));
        if (!viewport.contains(center)) continue;
        const float radius=std::min(port->box.size.x,port->box.size.y)*port->getAbsoluteZoom()*.5f;
        const float distance=position.minus(center).norm();
        if (distance<=radius) {
            if (exact && exact!=port) {ambiguous=true;return nullptr;}
            exact=port;
        }
        if (distance<=std::max(22.f,radius)) {enlarged=port;++near;}
    }
    if (exact) return exact;
    if (near==1) return enlarged;
    ambiguous=near>1;
    return nullptr;
}

void cancelWire(Instance& instance) {
    auto* rack=instance.context->scene->rack;
    if (instance.wirePreview && rack->getIncompleteCable()==instance.wirePreview.get())
        delete rack->releaseIncompleteCable();
    instance.wirePreview=nullptr;instance.wireOrigin=nullptr;
    instance.pressedPort=nullptr;instance.portContact=false;instance.portMoved=false;
}

void startWire(Instance& instance, rack::app::PortWidget* port) {
    cancelWire(instance);
    auto* rack=instance.context->scene->rack;
    auto* cable=new rack::app::CableWidget;
    cable->outputPort=port;
    cable->color=rack->getNextCableColor();
    cable->updateCable(); // Incomplete: no engine cable exists.
    rack->setIncompleteCable(cable);
    instance.wirePreview=cable;instance.wireOrigin=port;
    instance.notice.clear();
}

void finishWire(Instance& instance, rack::app::PortWidget* port) {
    auto* rack=instance.context->scene->rack;
    if (!instance.wirePreview || !instance.wireOrigin ||
        rack->getIncompleteCable()!=instance.wirePreview.get()) {cancelWire(instance);return;}
    if (!port || port->type!=rack::engine::Port::INPUT) {
        instance.notice="Tap an input; empty space cancels";return;
    }
    if (rack->getTopCable(port)) {
        instance.notice="Input is connected; Tools > Remove cable, or choose another input";return;
    }
    rack::widget::Widget::DragDropEvent drop;
    drop.button=GLFW_MOUSE_BUTTON_LEFT;drop.origin=instance.wireOrigin.get();
    port->onDragDrop(drop);
    rack::widget::Widget::DragEndEvent end;
    end.button=GLFW_MOUSE_BUTTON_LEFT;
    instance.wireOrigin->onDragEnd(end); // Rack adds the cable and one Undo action.
    instance.wirePreview=nullptr;instance.wireOrigin=nullptr;
    instance.notice="Connected; Undo removes the cable";
}

bool beginWireContact(Instance& instance, rack::math::Vec position) {
    auto* events=instance.context->event;
    events->handleHover(position,{});
    // A Rack menu/browser can cover modules without changing their geometry.
    // Let that actual topmost widget receive the tap.
    if (!visibleInRack(events->getHoveredWidget(),instance.context->scene->rack)) return false;
    bool ambiguous=false;
    auto* port=hitPort(instance,position,ambiguous);
    if (!port && !ambiguous && !instance.wirePreview) return false;
    instance.portContact=true;instance.portMoved=false;instance.portStart=position;
    instance.pressedPort=port;
    instance.last=position;
    if (port) instance.selectedPort=port;
    instance.notice=ambiguous?"Ports overlap here; zoom in to choose one":"";
    events->handleHover(position,{});
    return true;
}

void updateWireContact(Instance& instance, uint32_t event, rack::math::Vec position) {
    if (event!=2 && event!=3) return;
    if (position.minus(instance.portStart).norm()>8.f) instance.portMoved=true;
    auto* events=instance.context->event;
    events->handleHover(position,position.minus(instance.last));
    instance.last=position;
    bool ambiguous=false;
    auto* port=hitPort(instance,position,ambiguous);
    if (event==2 && instance.portMoved && !instance.wirePreview && instance.pressedPort &&
        instance.pressedPort->type==rack::engine::Port::OUTPUT) {
        auto* source=instance.pressedPort.get();
        startWire(instance,source);
        // Starting a preview does not end this physical contact.
        instance.pressedPort=source;instance.portContact=true;instance.portMoved=true;
    }
    if (instance.wirePreview) {
        instance.wirePreview->hoveredInputPort=port && port->type==rack::engine::Port::INPUT &&
            !instance.context->scene->rack->getTopCable(port)?port:nullptr;
    }
    if (event!=3) return;
    instance.portContact=false;
    if (ambiguous) {instance.notice="Ports overlap here; zoom in to choose one";return;}
    if (!port) {cancelWire(instance);instance.notice="Cable cancelled";return;}
    instance.selectedPort=port;
    if (instance.wirePreview) {
        if (port==instance.wireOrigin.get() && !instance.portMoved) {
            cancelWire(instance);instance.notice="Cable cancelled";
        } else finishWire(instance,port);
    } else if (port->type==rack::engine::Port::OUTPUT) {
        startWire(instance,port);
    } else instance.notice=portLabel(port)+": tap an output to start a cable";
}

void drawFocusedModule(Instance& instance, NVGcontext* vg) {
    auto* module=instance.focusModule.get();
    if (!module || instance.context->scene->browser->visible ||
        !visibleInRack(module,instance.context->scene->rack)) return;
    const auto viewport=instance.context->scene->rackScroll->box;
    const auto origin=module->getAbsoluteOffset({0.f,0.f});
    const auto size=module->box.size.mult(module->getAbsoluteZoom());
    if (size.x<=6.f || size.y<=6.f) return;
    // Presentation-only focus: Rack's selected state intercepts knob input.
    // Keep a constant screen-width outline while zooming, without selecting
    // the module in Rack or changing its canonical patch state.
    nvgSave(vg);nvgResetTransform(vg);
    nvgIntersectScissor(vg,viewport.pos.x,viewport.pos.y,viewport.size.x,viewport.size.y);
    nvgBeginPath(vg);nvgRect(vg,origin.x+3.f,origin.y+3.f,size.x-6.f,size.y-6.f);
    nvgStrokeColor(vg,nvgRGBA(12,14,18,230));nvgStrokeWidth(vg,6.f);nvgStroke(vg);
    nvgStrokeColor(vg,nvgRGB(255,72,104));nvgStrokeWidth(vg,3.f);nvgStroke(vg);
    nvgRestore(vg);
}

void drawWireTargets(Instance& instance, NVGcontext* vg) {
    if (!instance.wirePreview || !instance.wireOrigin || instance.context->scene->browser->visible) return;
    auto* rack=instance.context->scene->rack;
    const auto viewport=instance.context->scene->rackScroll->box;
    nvgSave(vg);nvgResetTransform(vg);
    nvgIntersectScissor(vg,viewport.pos.x,viewport.pos.y,viewport.size.x,viewport.size.y);
    const auto sourceCenter=instance.wireOrigin->getAbsoluteOffset(instance.wireOrigin->box.size.div(2));
    if (!instance.portContact && instance.last.minus(sourceCenter).norm()<28.f) {
        // Make a just-armed cable visible even before the next finger arrives.
        nvgBeginPath(vg);nvgMoveTo(vg,sourceCenter.x,sourceCenter.y);
        nvgBezierTo(vg,sourceCenter.x+12,sourceCenter.y-6,sourceCenter.x+12,sourceCenter.y-48,
            sourceCenter.x+40,sourceCenter.y-48);
        nvgStrokeColor(vg,instance.wirePreview->color);nvgStrokeWidth(vg,4);nvgStroke(vg);
        nvgBeginPath(vg);nvgCircle(vg,sourceCenter.x+40,sourceCenter.y-48,5);
        nvgStrokeWidth(vg,2);nvgStroke(vg);
    }
    for (auto* module : rack->getModules()) for (auto* port : module->getPorts()) {
        const bool source=port==instance.wireOrigin.get();
        if (!source && (port->type!=rack::engine::Port::INPUT || rack->getTopCable(port))) continue;
        if (!visibleInRack(port,rack)) continue;
        const auto center=port->getAbsoluteOffset(port->box.size.div(2));
        if (!viewport.contains(center)) continue;
        const float radius=std::max(12.f,std::min(port->box.size.x,port->box.size.y)*port->getAbsoluteZoom()*.5f+3.f);
        nvgBeginPath(vg);nvgCircle(vg,center.x,center.y,radius);
        nvgStrokeColor(vg,source?nvgRGB(255,211,90):nvgRGB(82,213,191));
        nvgStrokeWidth(vg,source?3.f:2.f);nvgStroke(vg);
    }
    nvgRestore(vg);
}
