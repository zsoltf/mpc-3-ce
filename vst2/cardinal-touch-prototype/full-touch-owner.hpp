#pragma once
#include <mutex>
#include "full-touch-api.h"
namespace rack { struct Context; Context* mpcContextPeek(); }
namespace rack { namespace window { struct Window; void* mpcWindowEnter(Window*); void mpcWindowLeave(void*); } }
struct MpcTouchGraphics {
    void* scope;
    explicit MpcTouchGraphics(rack::window::Window* window):scope(rack::window::mpcWindowEnter(window)){}
    ~MpcTouchGraphics(){rack::window::mpcWindowLeave(scope);}
};
std::recursive_mutex& mpcTouchMutex();
void mpcTouchRegister(void* plugin, rack::Context* context, const char* name);
void mpcTouchRetire(void* plugin);
typedef int (*MpcTouchHostParameter)(void*,uint32_t,uint32_t,MpcEditorParameter*);
extern "C" void mpcTouchBindEffect(void* plugin, void* effect, MpcTouchHostParameter);
extern "C" bool mpcTouchDeferDestroy(void* plugin, void* wrapper, void (*destroy)(void*));
