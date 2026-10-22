/* A view of an existing Cardinal plug-in. No engine, clock or audio-device owner. */
#include "full-touch-owner.hpp"
#include "full-touch-api.h"
#include "overlay-api.h"
#include <context.hpp>
#include <app/Scene.hpp>
#include <app/Browser.hpp>
#include <app/ModuleWidget.hpp>
#include <app/ParamWidget.hpp>
#include <app/Switch.hpp>
#include <ui/MenuOverlay.hpp>
#include <app/PortWidget.hpp>
#include <engine/Engine.hpp>
#include <engine/ParamQuantity.hpp>
#include <engine/PortInfo.hpp>
#include <window/Window.hpp>
#include <widget/ZoomWidget.hpp>
#include <patch.hpp>
#include <history.hpp>
#include <system.hpp>
#include <plugin.hpp>
#include <plugin/Plugin.hpp>
#include <plugin/Model.hpp>
#include <tag.hpp>
#include <cstddef>
#include <map>
#include <memory>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace rack { namespace window {
bool mpcWindowRender(Window*, uint32_t, uint32_t);
bool mpcWindowOverlay(Window*,uint32_t,uint32_t,bool,const MpcCardinalOverlayTarget*,uint32_t*);
void mpcWindowOverlayRelease(Window*);
bool mpcWindowUpdate(Window*, uint32_t, uint32_t);
bool mpcWindowOperation(Window*, bool (*)(void*), void*);
bool mpcWindowPixels(Window*, unsigned char*, uint32_t, uint32_t*);
void mpcWindowReleaseView(Window*);
void mpcWindowSetViewportMoving(Window*, bool);
} }
namespace rack { namespace app { namespace browser {
ModuleWidget* chooseModel(plugin::Model*);
} } }
namespace {
struct Instance {
    void* plugin;
    void* effect = nullptr;
    MpcTouchHostParameter hostParameter = nullptr;
    uint64_t hostBinding = 0;
    bool hostHeld[MPC_CARDINAL_HOST_PARAMETERS]{};
    rack::Context* context;
    uint64_t token;
    std::string name;
    unsigned calls = 0;
    uint32_t width = 0, height = 0;
    bool retired = false, contact = false, panning = false, overview = false, framePending = false;
    uint32_t mode = MPC_EDITOR_CONTROLS;
    rack::history::Action* historyCheckpoint = nullptr; // comparison only, never dereferenced
    rack::math::Vec last;
    rack::WeakPtr<rack::app::PortWidget> wireOrigin, pressedPort, selectedPort;
    rack::WeakPtr<rack::app::CableWidget> wirePreview;
    rack::WeakPtr<rack::app::ParamWidget> lastControl;
    bool parameterReadout = false;
    rack::WeakPtr<rack::app::ModuleWidget> focusModule;
    rack::WeakPtr<rack::app::ModuleWidget> parameterModule;
    rack::WeakPtr<rack::app::ParamWidget> parameterWidgets[MPC_EDITOR_PARAMETERS];
    uint64_t parameterBinding = 0;
    unsigned parameterCount = 0;
    bool parameterHeld[MPC_EDITOR_PARAMETERS]{};
    float parameterStart[MPC_EDITOR_PARAMETERS]{};
    bool portContact = false, portMoved = false;
    rack::math::Vec portStart;
    std::string notice;
    void* wrapper = nullptr;
    void (*destroy)(void*) = nullptr;
};
std::map<uint64_t, std::shared_ptr<Instance>> instances;
uint64_t nextToken = 0, nextHostBinding = 0;
std::recursive_mutex mutex;

#include "full-touch-interaction.inc"

void finishParameter(Instance& instance,unsigned index) {
    if (!instance.parameterHeld[index]) return;
    instance.parameterHeld[index]=false;
    auto* widget=instance.parameterWidgets[index].get();
    auto* quantity=widget?widget->getParamQuantity():nullptr;
    if (!quantity || !quantity->module) return;
    const float value=quantity->getValue();
    if (value==instance.parameterStart[index]) return;
    auto* action=new rack::history::ParamChange;
    action->moduleId=quantity->module->id; action->paramId=quantity->paramId;
    action->oldValue=instance.parameterStart[index]; action->newValue=value;
    instance.context->history->push(action);
}
void finishParameters(Instance& instance) {
    for (unsigned i=0;i<MPC_EDITOR_PARAMETERS;++i) finishParameter(instance,i);
}

void cancel(Instance& instance, bool endNavigation = true) {
    finishParameters(instance);
    if (endNavigation)
        rack::window::mpcWindowSetViewportMoving(instance.context->window, false);
    cancelWire(instance);
    auto* event = instance.context->event;
    bool musical = false;
    for (auto* widget = event->getDraggedWidget(); widget; widget = widget->parent)
        if (dynamic_cast<rack::app::ParamWidget*>(widget)) { musical = true; break; }
    const bool rollback = instance.contact && !musical;
    event->setDragHoveredWidget(nullptr);
    event->setDraggedWidget(nullptr, 0);
    event->setHoveredWidget(nullptr);
    event->handleLeave();
    instance.contact = false;
    instance.panning = false;
    if (rollback) {
        auto* history = instance.context->history;
        if (instance.historyCheckpoint) {
            auto checkpoint = std::find(history->actions.begin(),history->actions.end(),instance.historyCheckpoint);
            // A menu can load a new patch, clear history or invoke Undo during
            // the contact. Never roll back unrelated earlier history then.
            if (checkpoint == history->actions.end() ||
                std::distance(history->actions.begin(),checkpoint) >= history->actionIndex)
                return;
        }
        // Rack finalizes gestures in onDragEnd. Undo only this contact's
        // owner-written actions, including a cable disconnected on press.
        // Compare the preceding action rather than its shifting index: Rack
        // drops the oldest action at its 500-entry history limit.
        while (history->canUndo() && history->actions[history->actionIndex-1] != instance.historyCheckpoint)
            history->undo();
    }
}

struct Lease {
    std::unique_lock<std::recursive_mutex> lock{mutex};
    std::shared_ptr<Instance> instance;
    rack::Context* previous = nullptr;
    explicit Lease(uint64_t token) {
        auto found = instances.find(token);
        if (found == instances.end() || found->second->retired)
            return;
        instance = found->second;
        ++instance->calls;
        previous = rack::mpcContextPeek();
        rack::contextSet(instance->context);
    }
    ~Lease() {
        if (!instance)
            return;
        rack::contextSet(previous);
        if (--instance->calls == 0 && instance->destroy) {
            auto destroy = instance->destroy;
            auto wrapper = instance->wrapper;
            instance->destroy = nullptr;
            if (rack::mpcContextPeek() == instance->context)
                rack::contextSet(nullptr);
            destroy(wrapper);
            rack::contextSet(previous == instance->context ? nullptr : previous);
        }
    }
    bool live() const { return instance && !instance->retired; }
};

void closeHostParameters(Instance& instance) {
    instance.hostBinding=0;
    for(unsigned i=0;i<MPC_CARDINAL_HOST_PARAMETERS;++i)if(instance.hostHeld[i]){
        instance.hostHeld[i]=false;
        MpcEditorParameter value{};
        if(!instance.retired&&instance.hostParameter)
            instance.hostParameter(instance.effect,i,MPC_EDITOR_PARAMETER_END,&value);
    }
}
int hostParameters(uint64_t token,uint32_t event,MpcCardinalHostParameters* output) {
    if(!output)return 0;
    Lease lease(token);if(!lease.live()||!lease.instance->hostParameter)return 0;
    auto& instance=*lease.instance;
    if(event==MPC_CARDINAL_HOST_OPEN){
        closeHostParameters(instance);if(!lease.live())return 0;
        instance.hostBinding=++nextHostBinding;
    }else if(!output->binding||output->binding!=instance.hostBinding)return 0;
    if(event==MPC_CARDINAL_HOST_CLOSE){closeHostParameters(instance);*output={};return lease.live();}
    if(event!=MPC_CARDINAL_HOST_OPEN&&event!=MPC_CARDINAL_HOST_READ)return 0;
    MpcCardinalHostParameters result{};result.binding=instance.hostBinding;
    for(unsigned i=0;i<MPC_CARDINAL_HOST_PARAMETERS;++i){
        if(!instance.hostParameter(instance.effect,i,0,&result.parameters[i])||!lease.live())return 0;
        ++result.count;
    }
    *output=result;return 1;
}
int hostParameter(uint64_t token,uint64_t binding,uint32_t index,uint32_t event,float value) {
    if(index>=MPC_CARDINAL_HOST_PARAMETERS||!std::isfinite(value)||
       event<MPC_EDITOR_PARAMETER_BEGIN||event>MPC_EDITOR_PARAMETER_END)return 0;
    Lease lease(token);if(!lease.live())return 0;auto& instance=*lease.instance;
    if(!binding||binding!=instance.hostBinding||!instance.hostParameter)return 0;
    if(event==MPC_EDITOR_PARAMETER_BEGIN){if(instance.hostHeld[index])return 0;instance.hostHeld[index]=true;}
    else if(!instance.hostHeld[index])return 0;
    if(event==MPC_EDITOR_PARAMETER_END)instance.hostHeld[index]=false;
    MpcEditorParameter parameter{};parameter.value=std::max(0.f,std::min(1.f,value));
    return instance.hostParameter(instance.effect,index,event,&parameter)&&lease.live();
}

template<class Operation> int invoke(uint64_t token, Operation operation, bool render = true, std::string* failure = nullptr) {
    Lease lease(token);
    if (!lease.live()) return 0;
    try {
        auto& instance = *lease.instance;
        struct Call { Operation* operation; Instance* instance; bool update; } call{&operation,&instance,render};
        if (!rack::window::mpcWindowOperation(instance.context->window, [](void* argument) {
            auto* call = static_cast<Call*>(argument);
            if (!(*call->operation)(*call->instance)) return false;
            // Keep Rack's widget geometry current for the next hit test, but
            // rasterize only the latest state when the host paints this view.
            return !call->update || rack::window::mpcWindowUpdate(call->instance->context->window,
                call->instance->width, call->instance->height);
        }, &call) || !lease.live()) {
            if(failure)*failure="graphics operation, context restoration or owner lifetime failed";
            return 0;
        }
        if (render) instance.framePending = true;
        return 1;
    } catch (const std::exception& error) {
        if(failure)*failure=std::string(error.what()).substr(0,400);
        return 0;
    }
}

uint32_t list(MpcCardinalInstance* output, uint32_t capacity) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    uint32_t count = 0;
    for (auto& item : instances) {
        auto& instance = *item.second;
        if (instance.retired) continue;
        if (output && count < capacity) {
            auto& row = output[count];
            row.token = instance.token;
            std::snprintf(row.label, sizeof(row.label), "%s #%llu / %s", instance.name.c_str(),
                (unsigned long long)instance.token, instance.context->patch->path.empty() ? "Untitled" :
                rack::system::getFilename(instance.context->patch->path).c_str());
            row.modules = instance.context->engine->getModuleIds().size();
            row.cables = instance.context->engine->getCableIds().size();
        }
        ++count;
    }
    return count;
}
int open(uint64_t token, uint32_t width, uint32_t height) {
    if (width < 640 || width > 1280 || height < 400 || height > 800) return 0;
    return invoke(token, [=](Instance& instance) {
        instance.width = width; instance.height = height; return true;
    });
}
int pointer(uint64_t token, uint32_t event, float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return 0;
    return invoke(token, [=](Instance& instance) {
        auto* events = instance.context->event;
        if (event == 4 || x < 2 || y < 2 || x >= instance.width-2 || y >= instance.height-2) {
            cancel(instance); return true;
        }
        rack::math::Vec position(x,y);
        // Rack's own menu overlay owns its pointer stream, regardless of the
        // underlying canvas mode. Do not hit-test cables or pan through it.
        bool menu=false;
        for(auto* child:instance.context->scene->children)
            if(child->visible&&dynamic_cast<rack::ui::MenuOverlay*>(child)){menu=true;break;}
        if(menu){
            if(event==1){cancel(instance);instance.contact=true;instance.last=position;
                events->handleHover(position,{});events->handleButton(position,GLFW_MOUSE_BUTTON_LEFT,GLFW_PRESS,0);}
            else if(instance.contact){events->handleHover(position,position.minus(instance.last));instance.last=position;
                if(event==3){events->handleButton(position,GLFW_MOUSE_BUTTON_LEFT,GLFW_RELEASE,0);instance.contact=false;}}
            return true;
        }
        if (instance.portContact && event != 1) {
            updateWireContact(instance, event, position);
            return true;
        }
        if (event == 1) {
            auto* scroll = instance.context->scene->rackScroll;
            if (instance.overview) {
                cancel(instance);
                scroll->setZoom(1.f, position.minus(scroll->box.pos));
                instance.overview = false;return true;
            }
            // A pending tap cable belongs to Rack but has no engine cable yet.
            // The next tap can finish it without a synthetic held mouse drag.
            if (instance.mode == MPC_EDITOR_EDIT && beginWireContact(instance, position))
                return true;
            cancel(instance);
            instance.notice.clear();
            instance.parameterReadout = false;
            if (instance.mode == MPC_EDITOR_CONTROLS) instance.context->scene->rack->deselectAll();
            events->handleHover(position, {});
            auto* hit = events->getHoveredWidget();
            rack::math::Vec pressPosition=position;
            if (instance.mode==MPC_EDITOR_CONTROLS &&
                (hit==instance.context->scene->rack || dynamic_cast<rack::app::ModuleWidget*>(hit))) {
                bool ambiguous=false;
                if (auto* param=nearbyControl(instance,position,ambiguous)) {
                    pressPosition=param->getAbsoluteOffset(param->box.size.div(2));
                    events->handleHover(pressPosition,{});
                    hit=events->getHoveredWidget();
                } else if (ambiguous) {
                    instance.notice="Controls overlap here; zoom in to choose one";
                    return true;
                }
            }
            // Rack's desktop selection needs Shift-click. Touch Edit selects
            // the real module directly; Controls must release that selection
            // because a selected ModuleWidget consumes its children's clicks.
            rack::app::ModuleWidget* module = nullptr;
            for (auto* widget = hit; widget; widget = widget->parent)
                if ((module = dynamic_cast<rack::app::ModuleWidget*>(widget))) break;
            if (module && instance.mode != MPC_EDITOR_PAN) {
                instance.focusModule = module;
                auto* rack = instance.context->scene->rack;
                if (instance.mode == MPC_EDITOR_EDIT) {
                    if (!rack->isSelected(module)) {rack->deselectAll();rack->select(module);}
                }
            } else if (hit == instance.context->scene->rack && instance.mode == MPC_EDITOR_EDIT) {
                instance.focusModule = nullptr;
            }
            for (auto* widget = hit; widget; widget = widget->parent) {
                if (auto* param = dynamic_cast<rack::app::ParamWidget*>(widget)) {
                    instance.lastControl = param;
                    break;
                }
            }
            bool background = hit == instance.context->scene->rack || dynamic_cast<rack::app::ModuleWidget*>(hit);
            bool port = false;
            for (auto* widget = hit; widget; widget = widget->parent)
                if (dynamic_cast<rack::app::PortWidget*>(widget)) { port = true; break; }
            if (instance.mode == MPC_EDITOR_PAN || (instance.mode == MPC_EDITOR_CONTROLS && (background || port))) {
                rack::window::mpcWindowSetViewportMoving(instance.context->window, true);
                instance.last = position;instance.panning = true;return true;
            }
            auto* history = instance.context->history;
            instance.historyCheckpoint = history->canUndo() ? history->actions[history->actionIndex-1] : nullptr;
            instance.last = position;
            instance.contact = true;
            events->handleButton(pressPosition, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0);
            // Hover and button consumers can differ (for example a module's
            // display). Controls must never inherit a structural module drag.
            // Cancel before its first movement; Rack owns the empty drag end.
            if (instance.mode == MPC_EDITOR_CONTROLS &&
                dynamic_cast<rack::app::ModuleWidget*>(events->getDraggedWidget())) {
                cancel(instance);
                rack::window::mpcWindowSetViewportMoving(instance.context->window, true);
                instance.last = position;
                instance.panning = true;
            }
        } else if (instance.panning && (event == 2 || event == 3)) {
            instance.context->scene->rackScroll->offset -= position.minus(instance.last);
            instance.last = position;
            if (event == 3) {
                instance.panning = false;
                rack::window::mpcWindowSetViewportMoving(instance.context->window, false);
            }
        } else if (instance.contact && (event == 2 || event == 3)) {
            events->handleHover(position, position.minus(instance.last));
            instance.last = position;
            if (event == 3) {
                events->handleButton(position, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
                instance.contact = false;
            }
        }
        return true;
    });
}
int scroll(uint64_t token,float x,float y,float dx,float dy) {
    if (!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(dx)||!std::isfinite(dy)) return 0;
    return invoke(token,[=](Instance& instance) {
        instance.context->event->handleScroll({x,y},{dx,dy}); return true;
    });
}
int text(uint64_t token,uint32_t codepoint) {
    return invoke(token,[=](Instance& instance) {
        if (codepoint == 8 || codepoint == 13) {
            int key = codepoint == 8 ? GLFW_KEY_BACKSPACE : GLFW_KEY_ENTER;
            instance.context->event->handleKey(instance.last,key,0,GLFW_PRESS,0);
            instance.context->event->handleKey(instance.last,key,0,GLFW_RELEASE,0);
        } else instance.context->event->handleText(instance.last,codepoint);
        return true;
    });
}
int action(uint64_t token,uint32_t action,const char* path) {
    if(action==MPC_CARDINAL_LOAD){
        if(!path||!*path)return 0;
        bool started=false,loaded=false;std::string failure;
        const int result=invoke(token,[&](Instance& instance){
            closeHostParameters(instance);if(instance.retired)return false;
            cancel(instance);instance.notice.clear();started=true;
            instance.context->patch->load(path);loaded=true;instance.overview=false;
            return true;
        },true,&failure);
        if(!result){
            Lease lease(token);
            if(lease.live())lease.instance->notice=std::string(loaded?"Patch loaded; presentation failed: ":
                started?"Patch load failed: ":"Patch load not started: ")+failure;
        }
        return result;
    }
    return invoke(token,[=](Instance& instance) {
        cancel(instance);
        instance.notice.clear();
        auto* context = instance.context;
        switch (action) {
        case MPC_CARDINAL_BROWSER: context->scene->browser->show(); break;
        case MPC_CARDINAL_MODULE_OPTIONS: {
            auto* module=instance.focusModule.get();
            if(!module||!visibleInRack(module,context->scene->rack)){
                instance.notice="Tap a module first, then choose Module options";return false;
            }
            module->createContextMenu();break;
        }
        case MPC_CARDINAL_SAVE:
            rack::system::createDirectories("/tmp/mpclearn-cardinal-touch/state");
            context->patch->save("/tmp/mpclearn-cardinal-touch/state/prototype.vcv"); break;
        case MPC_CARDINAL_UNDO: context->history->undo(); break;
        case MPC_CARDINAL_REDO: context->history->redo(); break;
        case MPC_CARDINAL_DELETE: context->scene->rack->deleteSelectionAction(); break;
        case MPC_CARDINAL_REMOVE_CABLE:
            if (!instance.selectedPort || !context->scene->rack->getTopCable(instance.selectedPort.get())) return false;
            instance.selectedPort->deleteTopCableAction();
            instance.notice="Cable removed; Undo restores it";break;
        case MPC_CARDINAL_ZOOM_IN: case MPC_CARDINAL_ZOOM_OUT:
            context->scene->rackScroll->setZoom(context->scene->rackScroll->zoomWidget->getZoom() *
                (action == MPC_CARDINAL_ZOOM_IN ? 1.25f : 0.8f)); break;
        default: return false;
        }
        return true;
    });
}
int pixels(uint64_t token,unsigned char* output,uint32_t bytes,uint32_t* revision) {
    Lease lease(token);
    if (!lease.live()) return 0;
    auto& instance = *lease.instance;
    if (!output || !revision || bytes < instance.width * instance.height * 4) return 0;
    try {
        if (instance.framePending) {
            if (!rack::window::mpcWindowRender(instance.context->window,instance.width,instance.height)) return 0;
            instance.framePending = false;
        }
        return rack::window::mpcWindowPixels(instance.context->window,output,bytes,revision);
    } catch (const std::exception&) { return 0; }
}
int overlayRender(uint64_t token,const MpcCardinalOverlayTarget* target,uint32_t* revision) {
    Lease lease(token);if(!lease.live()||!target||!revision)return 0;
    auto& instance=*lease.instance;
    try {
        if(!rack::window::mpcWindowOverlay(instance.context->window,instance.width,instance.height,instance.framePending,target,revision))return 0;
        instance.framePending=false;return 1;
    } catch(const std::exception&){return 0;}
}
void overlayRelease(uint64_t token) {
    invoke(token,[](Instance& instance){rack::window::mpcWindowOverlayRelease(instance.context->window);return true;},false);
}
void close(uint64_t token) {
    invoke(token,[](Instance& instance) {
        cancel(instance);
        closeHostParameters(instance);
        if(instance.retired)return false;
        instance.context->event->setSelectedWidget(nullptr);
        rack::window::mpcWindowReleaseView(instance.context->window);
        instance.framePending = false;
        return true;
    },false);
}
void catalog(uint32_t* plugins,uint32_t* models) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (plugins) *plugins = rack::plugin::plugins.size();
    uint32_t count = 0;
    for (auto* plugin : rack::plugin::plugins) count += plugin->models.size();
    if (models) *models = count;
}
uint64_t bind(const void* effect) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    for (auto& item : instances) if (!item.second->retired && item.second->effect == effect) return item.first;
    return 0;
}
int viewport(uint64_t token,MpcEditorViewport* state) {
    if (!state) return 0;
    Lease lease(token);if (!lease.live()) return 0;
    auto& instance=*lease.instance;
    auto* scroll=instance.context->scene->rackScroll;
    *state={scroll->getZoom(),scroll->offset.x,scroll->offset.y,instance.mode,uint32_t(instance.overview)};
    return 1;
}
int view(uint64_t token,uint32_t intent,float x,float y,float amount) {
    if (!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(amount)) return 0;
    return invoke(token,[=](Instance& instance) {
        cancel(instance, intent != MPC_EDITOR_VIEW_PAN && intent != MPC_EDITOR_VIEW_ZOOM);
        instance.notice.clear();
        auto* scroll = instance.context->scene->rackScroll;
        switch (intent) {
        case MPC_EDITOR_VIEW_NAVIGATION_BEGIN:
            rack::window::mpcWindowSetViewportMoving(instance.context->window, true);break;
        case MPC_EDITOR_VIEW_NAVIGATION_END:
            rack::window::mpcWindowSetViewportMoving(instance.context->window, false);break;
        case MPC_EDITOR_VIEW_MODE:
            if (amount<0||amount>2) return false;
            instance.mode=uint32_t(amount);
            if (instance.mode == MPC_EDITOR_CONTROLS) instance.context->scene->rack->deselectAll();
            break;
        case MPC_EDITOR_VIEW_PAN: scroll->offset += rack::math::Vec(x,y);break;
        case MPC_EDITOR_VIEW_ZOOM_SELECTION: {
            if (amount<=0) return false;
            auto point=scroll->box.size.div(2);
            const auto& selected=instance.context->scene->rack->getSelected();
            if (!selected.empty()) {
                auto bounds=(*selected.begin())->box;
                for (auto* module : selected) bounds=bounds.expand(module->box);
                point=instance.context->scene->rack->getAbsoluteOffset(bounds.getCenter())-scroll->box.pos;
            } else if (auto* module=instance.focusModule.get()) {
                point=module->getAbsoluteOffset(module->box.size.div(2))-scroll->box.pos;
            }
            scroll->setZoom(scroll->getZoom()*amount,point);
            instance.overview=false;break;
        }
        case MPC_EDITOR_VIEW_ZOOM:
            if (amount<=0) return false;
            scroll->setZoom(scroll->getZoom()*amount,rack::math::Vec(x,y)-scroll->box.pos);
            instance.overview=false;break;
        case MPC_EDITOR_VIEW_FIT: {
            auto bounds=instance.context->scene->rack->getModuleContainer()->getChildrenBoundingBox();
            if (!bounds.size.isFinite()||bounds.size.x<=0||bounds.size.y<=0) {scroll->reset();break;}
            float zoom=std::min((scroll->box.size.x-48)/bounds.size.x,(scroll->box.size.y-48)/bounds.size.y);
            scroll->setZoom(zoom);
            scroll->offset=bounds.pos*scroll->getZoom()-rack::math::Vec(24,24);
            instance.overview=true;break;
        }
        case MPC_EDITOR_VIEW_FOCUS: {
            auto& selected=instance.context->scene->rack->getSelected();
            auto* focus=instance.focusModule.get();
            if (selected.empty()&&!focus) return false;
            auto bounds=selected.empty()?focus->box:(*selected.begin())->box;
            for (auto* module : selected) bounds=bounds.expand(module->box);
            scroll->setZoom(1.f);
            scroll->offset=bounds.getCenter()*scroll->getZoom()-scroll->box.size.div(2);
            instance.overview=false;break;
        }
        default:return false;
        }
        return true;
    });
}
uint32_t items(MpcEditorItem* output,uint32_t capacity) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    uint32_t index=0;
    for (auto* plugin : rack::plugin::plugins) for (auto* model : plugin->models) {
        if (model->hidden) continue; // Same public catalog as Rack's own browser.
        if (output&&index<capacity) {
            auto& item=output[index];item={};item.id=index;
            std::string tags;
            for (int tag : model->tagIds) {
                auto name=rack::tag::getTag(tag);
                if (name=="Controller"||name=="Attenuator"||name=="Clock generator"||
                    name=="Envelope generator"||name=="Function generator"||
                    name=="Low-frequency oscillator"||name=="MIDI"||name=="Random"||
                    name=="Sequencer"||name=="Switch"||name=="Utility") item.control=1;
                if (!tags.empty()) tags+=", ";
                tags+=name;
            }
            std::snprintf(item.name,sizeof(item.name),"%s / %s",plugin->brand.c_str(),model->name.c_str());
            std::snprintf(item.detail,sizeof(item.detail),"%s %s",tags.c_str(),model->description.c_str());
        }
        ++index;
    }
    return index;
}
int create(uint64_t token,uint32_t id,float x,float y) {
    if (!std::isfinite(x)||!std::isfinite(y)) return 0;
    return invoke(token,[=](Instance& instance) {
        if (x<2||y<2||x>=instance.width-2||y>=instance.height-2) return false;
        cancel(instance);
        const rack::math::Vec position(x,y);
        if (!instance.context->scene->rackScroll->box.contains(position)) return false;
        instance.context->event->handleHover(position,{});
        if (!visibleInRack(instance.context->event->getHoveredWidget(),instance.context->scene->rack)) return false;
        uint32_t index=0;
        for (auto* plugin : rack::plugin::plugins) for (auto* model : plugin->models) {
            if (model->hidden) continue;
            if (index++!=id) continue;
            // The canonical browser transaction reads Rack's hovered position,
            // adds the engine/widget pair, loads defaults and pushes history.
            instance.context->event->handleHover({x,y},{});
            auto* module=rack::app::browser::chooseModel(model);
            if (!module) return false;
            instance.context->scene->rack->deselectAll();
            instance.context->scene->rack->select(module);
            instance.focusModule=module;
            instance.notice="Added "+model->name+"; Undo removes it";
            instance.mode=MPC_EDITOR_EDIT;instance.overview=false;return true;
        }
        return false;
    });
}
bool parameterSnapshot(Instance& instance,MpcEditorParameters& output) {
    output={};
    auto* module=instance.width?instance.focusModule.get():nullptr;
    rack::app::ParamWidget* widgets[MPC_EDITOR_PARAMETERS]{};
    unsigned count=0;
    if (module && module->module) {
        for (auto* widget:module->getParams()) {
            auto* quantity=widget->getParamQuantity();
            auto* button=dynamic_cast<rack::app::Switch*>(widget);
            if (!quantity || !quantity->module || !quantity->isBounded() ||
                !(quantity->getMaxValue()>quantity->getMinValue()) ||
                (button && button->momentary)) continue;
            bool duplicate=false;
            for (unsigned i=0;i<count;++i)
                duplicate |= widgets[i]->paramId==widget->paramId;
            if (duplicate) continue;
            widgets[count++]=widget;
            if (count==MPC_EDITOR_PARAMETERS) break;
        }
    }
    bool changed=instance.parameterModule.get()!=module || instance.parameterCount!=count;
    for (unsigned i=0;i<count;++i) changed |= instance.parameterWidgets[i].get()!=widgets[i];
    if (changed || !instance.parameterBinding) {
        finishParameters(instance);
        ++instance.parameterBinding;
        instance.parameterModule=module; instance.parameterCount=count;
        for (unsigned i=0;i<MPC_EDITOR_PARAMETERS;++i) instance.parameterWidgets[i]=widgets[i];
    }
    output.binding=instance.parameterBinding; output.count=count;
    std::snprintf(output.owner,sizeof(output.owner),"%s",module&&module->model?module->model->name.c_str():"Select a module");
    for (unsigned i=0;i<count;++i) {
        auto* q=widgets[i]->getParamQuantity();auto& out=output.parameters[i];
        out.id=q->paramId; out.value=q->getScaledValue();
        if (!std::isfinite(out.value)) return false;
        const float steps=q->getMaxValue()-q->getMinValue()+1;
        out.steps=q->snapEnabled&&steps>=2&&steps<=65536?unsigned(steps):0;
        std::snprintf(out.name,sizeof(out.name),"%s",q->getLabel().c_str());
        std::snprintf(out.display,sizeof(out.display),"%s%s",q->getDisplayValueString().c_str(),q->getUnit().c_str());
    }
    return true;
}
int parameters(uint64_t token,MpcEditorParameters* output) {
    if (!output) return 0;
    Lease lease(token);if (!lease.live()) return 0;
    try {return parameterSnapshot(*lease.instance,*output);} catch (...) {return 0;}
}
int parameter(uint64_t token,uint64_t binding,uint32_t index,uint32_t event,float value) {
    if (index>=MPC_EDITOR_PARAMETERS || event<MPC_EDITOR_PARAMETER_BEGIN ||
        event>MPC_EDITOR_PARAMETER_END || !std::isfinite(value)) return 0;
    return invoke(token,[=](Instance& instance) {
        MpcEditorParameters current{};
        if (!parameterSnapshot(instance,current) || current.binding!=binding || index>=current.count) return false;
        if (event==MPC_EDITOR_PARAMETER_END) {finishParameter(instance,index);return true;}
        auto* widget=instance.parameterWidgets[index].get();auto* quantity=widget->getParamQuantity();
        if (!instance.parameterHeld[index]) {
            instance.parameterHeld[index]=true;instance.parameterStart[index]=quantity->getValue();
        }
        if (event==MPC_EDITOR_PARAMETER_SET) {
            quantity->setScaledValue(std::max(0.f,std::min(1.f,value)));
            instance.lastControl=widget;instance.parameterReadout=true;instance.notice.clear();
        }
        return true;
    });
}
int status(uint64_t token,char* output,uint32_t capacity) {
    if (!output || !capacity) return 0;
    Lease lease(token);if (!lease.live()) return 0;
    try {
        auto* context=lease.instance->context;
        auto& instance=*lease.instance;
        if (!instance.notice.empty()) {
            std::snprintf(output,capacity,"%s",instance.notice.c_str());return 1;
        }
        if (instance.wirePreview && instance.wireOrigin) {
            std::snprintf(output,capacity,"%s: tap an input; empty space cancels",
                portLabel(instance.wireOrigin.get()).c_str());return 1;
        }
        rack::widget::Widget* hit=instance.parameterReadout?instance.lastControl.get():context->event->getDraggedWidget();
        if (!hit) hit=context->event->getHoveredWidget();
        if (!hit) hit=instance.lastControl.get();
        for (auto* widget=hit;widget;widget=widget->parent) {
            auto* param=dynamic_cast<rack::app::ParamWidget*>(widget);
            if (!param) continue;
            auto* quantity=param->getParamQuantity();
            if (!quantity) break;
            std::snprintf(output,capacity,"%s: %s%s",quantity->getLabel().c_str(),
                quantity->getDisplayValueString().c_str(),quantity->getUnit().c_str());
            return 1;
        }
        const auto patch=context->patch->path.empty()?std::string("Untitled"):
            rack::system::getFilename(context->patch->path);
        const auto& selected=context->scene->rack->getSelected();
        if (auto* focused=instance.focusModule.get())
            std::snprintf(output,capacity,"%s / %s",patch.c_str(),focused->model?focused->model->name.c_str():"Module");
        else if (selected.size()==1 && (*selected.begin())->model)
            std::snprintf(output,capacity,"%s / %s",patch.c_str(),(*selected.begin())->model->name.c_str());
        else std::snprintf(output,capacity,"%s",patch.c_str());
        return 1;
    } catch (const std::exception&) { return 0; }
}
void drawTouchOverlay(rack::window::Window* window, NVGcontext* vg) {
    for (auto& item : instances) {
        auto& instance=*item.second;
        if (!instance.retired && instance.context->window == window) {
            drawFocusedModule(instance, vg);drawWireTargets(instance, vg);return;
        }
    }
}
int advance(uint64_t token) {
    return invoke(token,[](Instance& instance){return instance.width&&instance.height;});
}
const MpcCardinalTouchApi api = {MPC_CARDINAL_TOUCH_ABI,sizeof(MpcCardinalTouchApi),list,open,pointer,scroll,text,action,pixels,close,catalog,bind,view,items,create,viewport,status,parameters,parameter,advance,hostParameters,hostParameter};
const MpcCardinalTouchApi legacy5 = {5,offsetof(MpcCardinalTouchApi,advance),list,open,pointer,scroll,text,action,pixels,close,catalog,bind,view,items,create,viewport,status,parameters,parameter,nullptr,nullptr,nullptr};
const MpcCardinalTouchApi legacy4 = {4,offsetof(MpcCardinalTouchApi,parameters),list,open,pointer,scroll,text,action,pixels,close,catalog,bind,view,items,create,viewport,status,nullptr,nullptr,nullptr,nullptr,nullptr};
const MpcCardinalTouchApi legacy3 = {3,offsetof(MpcCardinalTouchApi,status),list,open,pointer,scroll,text,action,pixels,close,catalog,bind,view,items,create,viewport,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
const MpcCardinalTouchApi legacy2 = {2,offsetof(MpcCardinalTouchApi,viewport),list,open,pointer,scroll,text,action,pixels,close,catalog,bind,view,items,create,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
const MpcCardinalTouchApi legacy = {1,offsetof(MpcCardinalTouchApi,bind),list,open,pointer,scroll,text,action,pixels,close,catalog,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
}
void mpcTouchDrawOverlay(rack::window::Window* window, NVGcontext* vg) {
    // Window paint is already inside the selected owner's Lease.
    drawTouchOverlay(window, vg);
}
std::recursive_mutex& mpcTouchMutex() { return mutex; }
void mpcTouchRegister(void* plugin,rack::Context* context,const char* name) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto instance = std::make_shared<Instance>();
    instance->plugin = plugin; instance->context = context; instance->name = name;
    instance->token = ++nextToken;
    instances.emplace(instance->token,instance);
}
void mpcTouchRetire(void* plugin) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    for (auto item = instances.begin(); item != instances.end(); ++item) {
        if (item->second->plugin == plugin) {
            item->second->retired = true;
            instances.erase(item); return;
        }
    }
}
extern "C" void mpcTouchBindEffect(void* plugin,void* effect,MpcTouchHostParameter callback) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    for (auto& item : instances) if (item.second->plugin==plugin) {item.second->effect=effect;item.second->hostParameter=callback;return;}
}
extern "C" bool mpcTouchDeferDestroy(void* plugin,void* wrapper,void(*destroy)(void*)) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    for (auto& item : instances) {
        auto& instance = *item.second;
        if (instance.plugin == plugin && instance.calls) {
            instance.retired = true;
            instance.wrapper = wrapper; instance.destroy = destroy;
            return true;
        }
    }
    return false;
}
extern "C" __attribute__((visibility("default")))
const MpcCardinalTouchApi* mpclearn_cardinal_touch_api(uint32_t abi) {
    return abi == MPC_CARDINAL_TOUCH_ABI ? &api : abi == 5 ? &legacy5 : abi == 4 ? &legacy4 : abi == 3 ? &legacy3 : abi == 2 ? &legacy2 : abi == 1 ? &legacy : nullptr;
}

extern "C" __attribute__((visibility("default")))
const MpcCardinalOverlayApi* mpclearn_cardinal_overlay_api(uint32_t abi) {
    static const MpcCardinalOverlayApi api={sizeof(MpcCardinalOverlayApi),1,overlayRender,overlayRelease};
    return abi==1?&api:nullptr;
}
