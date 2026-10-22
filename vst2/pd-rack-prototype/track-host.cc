/* MPC 3.9.1 standalone Instrument Edit attachment. The existing native UI
 * admission owns the hooks and thread. The native Instrument Edit page or
 * insert overlay owns the view; its selected processor retains the VST. */
#include "../private/mpclearn-ui-private.h"
#include "../private/mpc-3.9.1-profile.h"
#include "entry-api.h"
#ifdef MPCLEARN_CARDINAL_GPU
#define CARDINAL_PRESENTER_IMPLEMENTATION
#include "../private/cardinal-gpu/presenter.inc"
#define CARDINAL_GPU_EVENT(kind,object,arg) cardinal_presenter_ui(kind,uintptr_t(object),arg)
#else
#define CARDINAL_GPU_EVENT(kind,object,arg) ((void)0)
#endif
#include <cstdint>
#include <cstring>
#include <atomic>
#include <cmath>
#include <ctime>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
extern "C" int mpclearn_track_editor_main();
extern "C" void* mpclearn_track_text_owner();

namespace {
struct Trace {
    uint32_t magic,version,bytes,pid,bias,layouts,destroys,opens,closes,failures;
    uint32_t editor,parent,processor,processorVptr,effect,uid;
    int bounds[4];uint32_t lastError,live,program,audioProcessor,changes;
};
struct Slot {
    void* editor=nullptr;void* effect=nullptr;void* component=nullptr;void* inserts=nullptr;
    MpcUiContext* context=nullptr;MpcUiParent* parent=nullptr;MpcUiAppHost* host=nullptr;
    const MpcEditorEntryApi* api=nullptr;void* view=nullptr;
    uint32_t editorComponentVptr=0,editorComponentTable[46]{};
    bool attempted=false,counted=false,active=false,backRequested=false,insert=false;
};
uint32_t imageBias=0;
Trace* trace=nullptr;
Slot slots[8];
uint32_t word(const void* object,unsigned offset){uint32_t result;std::memcpy(&result,static_cast<const char*>(object)+offset,4);return result;}
void* pointer(const void* object,unsigned offset){return reinterpret_cast<void*>(uintptr_t(word(object,offset)));}
void failure(uint32_t code){if(trace){++trace->failures;trace->lastError=code;}}
#include "screen-qlinks.inc"
void intent(void* owner,uint32_t){static_cast<Slot*>(owner)->backRequested=true;}
void editorQlinkPaint(void*,void*){}
void close(Slot& slot){
    CARDINAL_GPU_EVENT(22,slot.editor,slot.active);
    screenRelease(slot);
    if(slot.editorComponentVptr){
        void* component=static_cast<char*>(slot.editor)+8;
        if(word(component,0)==uint32_t(uintptr_t(slot.editorComponentTable+2)))
            std::memcpy(component,&slot.editorComponentVptr,4);
        else failure(10);
        slot.editorComponentVptr=0;
    }
    if(slot.host&&!mpc_ui_private_app_host_destroy(&slot.host,1)){failure(6);return;}
    if(slot.view){slot.api->close(slot.view);slot.api->destroy(slot.view);slot.view=nullptr;
        if(trace&&slot.counted){++trace->closes;--trace->live;}}
    if(slot.parent&&!mpc_ui_private_parent_destroy(&slot.parent)){failure(7);return;}
    if(slot.context&&!mpc_ui_private_context_destroy(&slot.context)){failure(8);return;}
    void* editor=slot.editor;bool active=slot.active,insert=slot.insert;
    slot=Slot{};slot.editor=editor;slot.active=active;slot.insert=insert;
}
void* resolve(Slot& slot,void*& component,void*& inserts){
    void* page=slot.editor;void* program=nullptr;void* processor=nullptr;void* audioProcessor=nullptr;
    if(!page)return nullptr;
    if(slot.insert){
        if(word(page,0)!=imageBias+0x069c9f24u||!*(static_cast<unsigned char*>(page)+0xc4))return nullptr;
        void* service=pointer(page,0x1ac);if(!service)return nullptr;
        void* session=pointer(service,0x5c);
        if(!session||word(session,0)!=imageBias+0x0689e674u)return nullptr;
        void* owner=pointer(session,0x34);unsigned index=word(session,0x38);
        if(!owner||index>=4)return nullptr;
        inserts=pointer(owner,0x5a4);
        if(!inserts||word(inserts,0)!=imageBias+0x068ac370u)return nullptr;
        void* ui=pointer(inserts,0x60);void* audio=pointer(inserts,0x94);
        if(!ui||!audio||word(inserts,0x64)-word(inserts,0x60)!=16||
           word(inserts,0x98)-word(inserts,0x94)!=16)return nullptr;
        void* selected=pointer(ui,index*4);void* playing=pointer(audio,index*4);
        if(!selected||selected!=playing||word(selected,0)!=imageBias+0x0692e180u)return nullptr;
        void* tuple=pointer(selected,0x70);
        if(!tuple||pointer(tuple,0)!=static_cast<char*>(owner)-0x3c||
           word(tuple,4)!=0x101||word(tuple,8)!=index)return nullptr;
        program=pointer(tuple,0);
        processor=pointer(selected,0x68);audioProcessor=pointer(playing,0x68);
        void* content=pointer(page,0x134);if(!content)return nullptr;
        component=static_cast<char*>(content)+8;
        if(word(component,0)!=imageBias+0x069cad70u||
           pointer(component,0x0c)!=static_cast<char*>(page)+8)return nullptr;
    }else{
        if(word(page,0)!=imageBias+0x069eeae4u)return nullptr;
        component=static_cast<char*>(page)+8;
        void* mode=pointer(page,0xe0);if(!mode)return nullptr;
        program=pointer(mode,0x5c);
        if(!program||word(program,0)!=imageBias+0x06932a48u)return nullptr;
        processor=pointer(program,0x1b68);audioProcessor=pointer(program,0x1b98);
    }
    if(trace){trace->editor=uint32_t(uintptr_t(page));trace->parent=word(component,0x0c);
        trace->program=uint32_t(uintptr_t(program));std::memcpy(trace->bounds,static_cast<char*>(component)+0x10,16);}
    if(trace){trace->audioProcessor=uint32_t(uintptr_t(audioProcessor));
        trace->processor=uint32_t(uintptr_t(processor));trace->processorVptr=processor?word(processor,0):0;
    }
    // UI/audio shared pointers may temporarily disagree during replacement.
    // Wait for the host's own handoff; never bind to a retired processor.
    if(!processor||processor!=audioProcessor||word(processor,0)!=imageBias+0x06889888u)return nullptr;
    void* effect=pointer(processor,0xcc);if(!effect||word(effect,0)!=0x56737450)return nullptr;
    uint32_t uid=word(effect,0x48); // ARM32 AEffect uid, abi.h static_assert.
    if(trace){trace->effect=uint32_t(uintptr_t(effect));trace->uid=uid;}
    if(uid!=0x4d705053u&&uid!=0x4d705046u&&uid!=0x64436e53u&&uid!=0x64436e46u)return nullptr;
    return effect;
}
int attachChild(void* owner,void* child){
    if(!owner||!child)return 0;
    auto add=reinterpret_cast<void(*)(void*,void*,int)>(uintptr_t(imageBias)+MPC_UI_RVA_ADD_CHILD);
    auto onTop=reinterpret_cast<void(*)(void*,int)>(uintptr_t(imageBias)+MPC_UI_RVA_SET_ALWAYS_ON_TOP);
    add(owner,child,-1);
    if(pointer(child,0x0c)!=owner)return 0;
    // TabIndex re-adds its native page after these individually hosted controls.
    // Keep only our children in the native top group, in attachment order.
    onTop(child,1);
    return pointer(child,0x0c)==owner;
}
int detachChild(void* owner,void* child){
    if(!owner||!child)return 0;
    auto remove=reinterpret_cast<void(*)(void*,void*)>(uintptr_t(imageBias)+MPC_UI_RVA_REMOVE_CHILD);
    remove(owner,child);
    return pointer(child,0x0c)==nullptr;
}
void attach(Slot& slot){
    void* component=slot.component;
    if(slot.attempted||!pointer(component,0x0c))return;
    int width=int(word(component,0x18)),height=int(word(component,0x1c));
    if(width<920||height<580||width>1280||height>800)return;
    slot.attempted=true;
    if(word(slot.editor,8)!=imageBias+(slot.insert?0x069c9f98u:0x069eeb34u)){failure(10);return;}
    Dl_info library{};
    if(!dladdr(pointer(slot.effect,4),&library)||!library.dli_fname){failure(2);return;}
    uint32_t uid=word(slot.effect,0x48);
    slot.api=(uid==0x4d705053u||uid==0x4d705046u)?mpclearn_pd_editor_entry():mpclearn_cardinal_editor_entry();
    slot.view=slot.api->create(intent,&slot);
    if(!slot.view||!slot.api->bind(slot.view,slot.effect,library.dli_fname)){failure(3);return;}
    MpcUiAdmittedProfile profile={sizeof(profile),MPC_UI_PROFILE_MPC_3_9_1,imageBias,uint32_t(syscall(SYS_gettid))};
    slot.context=mpc_ui_private_context_from_admitted(&profile);
    if(slot.context)mpc_ui_private_context_text_owner(slot.context,mpclearn_track_text_owner());
    if(slot.context)slot.parent=mpc_ui_private_parent_create(slot.context,component,attachChild,detachChild,nullptr);
    if(slot.parent)slot.host=mpc_ui_private_app_host_create(slot.context,slot.parent,{0,0,width,height});
    if(!slot.host||!mpc_ui_app_register(slot.host,slot.api->callbacks,slot.view)||!mpc_ui_app_open(slot.host)){failure(4);return;}
    {
        // Both admitted Component tables contain 44 virtual entries. FX
        // paints its Q-Link outline over children; Instrument Edit paints it
        // beneath them, leaving orange fragments around our opaque canvas.
        // Its two surrounding page-paint hooks are admitted native no-ops.
        // Suppress only that exact page's paint while our view is attached.
        // Header/child painting and native input/parameter owners stay active.
        slot.editorComponentVptr=word(slot.editor,8);
        std::memcpy(slot.editorComponentTable,
                    reinterpret_cast<const void*>(uintptr_t(slot.editorComponentVptr-8)),
                    sizeof(slot.editorComponentTable));
        slot.editorComponentTable[2+(slot.insert?27:26)]=uint32_t(uintptr_t(&editorQlinkPaint));
        uint32_t table=uint32_t(uintptr_t(slot.editorComponentTable+2));
        std::memcpy(static_cast<char*>(slot.editor)+8,&table,4);
    }
    slot.counted=true;if(trace){++trace->opens;++trace->live;}
}
}
extern "C" void mpclearn_track_editor_configure(uint32_t bias){
    imageBias=bias;
#ifdef MPCLEARN_CARDINAL_GPU
    cardinal_presenter_init(bias);
#endif
    int fd=::open("/tmp/mpclearn-track-editor.state",O_RDWR|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0)return;
    if(!ftruncate(fd,4096)){
        void* memory=mmap(nullptr,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
        if(memory!=MAP_FAILED){trace=static_cast<Trace*>(memory);*trace={};trace->magic=0x54454431;trace->version=1;trace->bytes=sizeof(*trace);trace->pid=getpid();trace->bias=bias;}}
    ::close(fd);
}
extern "C" void mpclearn_track_editor_active(void* editor,int active){
    CARDINAL_GPU_EVENT(20,editor,active);
    if(trace)++trace->layouts;
    if(!editor)return;
    bool insert=word(editor,0)==imageBias+0x069c9f24u;
    if(!insert&&word(editor,0)!=imageBias+0x069eeae4u)return;
    for(auto& slot:slots)if(slot.editor==editor){slot.active=active!=0;if(!active)close(slot);return;}
    if(active)for(auto& slot:slots)if(!slot.editor){slot.editor=editor;slot.active=true;slot.insert=insert;return;}
}
extern "C" void mpclearn_insert_editor_active(void* editor){
    if(editor&&word(editor,0)==imageBias+0x069c9f24u)
        mpclearn_track_editor_active(editor,*(static_cast<unsigned char*>(editor)+0xc4)!=0);
}
extern "C" void mpclearn_insert_editor_changed(void* editor){
    for(auto& slot:slots)if(slot.editor==editor)close(slot);
}
extern "C" void mpclearn_insert_editor_inserts_changing(void* inserts){
    // The existing UI-vector hook runs before the old InsertEffect is retired.
    for(auto& slot:slots)if(slot.insert&&slot.inserts==inserts)close(slot);
}
extern "C" void mpclearn_track_editor_changed(){
    CARDINAL_GPU_EVENT(21,0,0);
    if(trace)++trace->changes;
    for(auto& slot:slots)if(slot.editor)close(slot);
}
extern "C" void mpclearn_track_editor_destroy(void* editor){
    CARDINAL_GPU_EVENT(24,editor,0);
    if(trace)++trace->destroys;
    for(auto& slot:slots)if(slot.editor==editor){close(slot);slot=Slot{};}
}
extern "C" void mpclearn_track_editor_refresh(){
    Slot* visible=nullptr;
    for(auto& slot:slots)if(slot.editor&&slot.active){
        void* component=nullptr;void* inserts=nullptr;
        void* effect=resolve(slot,component,inserts);
        if(slot.effect!=effect||slot.component!=component){close(slot);slot.effect=effect;slot.component=component;slot.inserts=inserts;}
        if(!effect)continue;
        if(!slot.host)attach(slot);
        else {
            if(!mpc_ui_private_app_host_drain(slot.host)){failure(5);continue;}
            if(slot.backRequested&&!mpc_ui_app_is_open(slot.host)){
                void* editor=slot.editor;bool insert=slot.insert;
                close(slot);
                if(insert){
                    // Same close request as the stock insert overlay's Back.
                    ((void(*)(void*))(uintptr_t)(imageBias+0x0364bde8u))(editor);
                }else if(!mpclearn_track_editor_main())failure(9);
                return; // Native navigation can synchronously deactivate the page.
            }
        }
        if(slot.host&&mpc_ui_app_is_open(slot.host)&&(!visible||slot.insert))visible=&slot;
    }
    screenRefresh(visible);
    if(visible&&!mpc_ui_app_is_closing(visible->host)&&
       !mpc_ui_private_app_host_is_suspended(visible->host)&&visible->api->idle)
        visible->api->idle(visible->view);
}
extern "C" void mpclearn_track_editor_root_retiring(){CARDINAL_GPU_EVENT(25,0,0);screenRetiring();}

extern "C" void mpclearn_track_qlink_publication(unsigned kind,uint32_t owner,unsigned index,uint32_t stack,uint32_t link){
    screenPublication(kind,owner,index,stack,link);
}
