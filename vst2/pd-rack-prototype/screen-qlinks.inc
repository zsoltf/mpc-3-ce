/* Selected-control presentation on MPC's existing Screen Q-Link provider.
 * Native input stays on its audio queue. Only bounded normalized intents cross
 * to the existing UI drain; Pd/Rack retain values, smoothing and history. */
#include "track-host-profile.h"
#include "qlink-order.h"
extern "C" unsigned transport_queue_submit(uint32_t,uint32_t,void*,void(*)(void*));
extern "C" int mpclearn_track_qlink_native(unsigned,uint32_t[8]);
struct ScreenNative {
    uint32_t root=0,queue=0,mode=0,controller=0,wrapper=0,provider=0,labels=0,controls=0;
};
struct ScreenLane {
    std::atomic<uint32_t> sequence{0},epoch{0},bits{0},activity{0};
    uint32_t consumed=0;bool editing=false;uint64_t lastChange=0;
};
struct ScreenTab {
    uint32_t properties=0,program=0,first=0,second=0;
};
bool screenSameTab(const ScreenTab& a,const ScreenTab& b){
    return a.properties==b.properties&&a.program==b.program&&a.first==b.first&&a.second==b.second;
}
uint32_t screenNativePending(const ScreenNative& native){
    return __atomic_load_n(reinterpret_cast<const uint32_t*>(uintptr_t(native.mode)+0x44),__ATOMIC_ACQUIRE);
}
bool screenTabMatches(const ScreenNative& native,const ScreenTab& tab){
    void* mode=reinterpret_cast<void*>(uintptr_t(native.mode));
    if(!tab.properties)return word(mode,0)==imageBias+TH_QLINK_FX_MODE_VPTR;
    if(word(mode,0)!=imageBias+TH_QLINK_PROGRAM_MODE_VPTR||word(mode,0x29c)!=tab.properties||word(mode,0x2a8)!=tab.program||
       word(mode,0x48)!=native.labels||word(mode,0x98)!=native.labels)return false;
    void* properties=reinterpret_cast<void*>(uintptr_t(tab.properties));
    return word(properties,0x28)==tab.first&&word(properties,0x2c)==tab.second;
}
bool screenReadTab(const ScreenNative& native,const Slot& slot,ScreenTab& tab){
    tab={};if(slot.insert)return screenTabMatches(native,tab);
    void* mode=reinterpret_cast<void*>(uintptr_t(native.mode));
    if(word(mode,0)!=imageBias+TH_QLINK_PROGRAM_MODE_VPTR||word(mode,0x48)!=native.labels||word(mode,0x98)!=native.labels)return false;
    tab.properties=word(mode,0x29c);tab.program=word(mode,0x2a8);
    if(!tab.properties||!tab.program||tab.properties%4||tab.properties>UINT32_MAX-0x30||
       tab.properties!=word(slot.editor,0xe4)||tab.program!=word(slot.editor,0xe0))return false;
    void* properties=reinterpret_cast<void*>(uintptr_t(tab.properties));
    tab.first=word(properties,0x28);tab.second=word(properties,0x2c);return true;
}
struct ScreenJob {
    unsigned operation=0;uint32_t epoch=0;
    ScreenTab tab{};bool captureNative=false;uint32_t publication=0;
    uint32_t descriptorRevision[16]{},labelRevision[16]{};
    MpcEditorParameters parameters{};
    uint32_t observed[MPC_EDITOR_PARAMETERS]{};
};
struct ScreenQLinks {
    Slot* slot=nullptr;
    ScreenNative native{};
    MpcEditorParameters parameters{};
    ScreenLane lanes[MPC_EDITOR_PARAMETERS];
    ScreenJob job{};
    ScreenTab requestedTab{},savedTab{},labelTab{};
    std::atomic<uint32_t> root{0},receiver{0},desired{0},applied{0},count{0},pending{0},installed{0};
    std::atomic<uint32_t> publicationProvider{0},publicationLabels{0},publication{0},appliedPublication{0};
    std::atomic<uint32_t> descriptorRevision[16]{},labelRevision[16]{};
    uint32_t savedDescriptorRevision[16]{},savedLabelRevision[16]{},savedValueLabelRevision[16]{},ownedDescriptorRevision[16]{},ownedLabelRevision[16]{};
    uint32_t nextEpoch=0,originalTable=0,table[10]{},savedDescriptors[16][26]{};
    uint32_t labelKind[16]{},labelName[16]{},labelValue[16]{};
    bool labelsSaved=false,restoring=false;
    uint64_t nextRefresh=0;
    uint32_t inputs=0,changes=0,dropped=0; // UI-owned counters only
};
ScreenQLinks screenQLinks;
struct ScreenPublicationCall {
    uint32_t kind=0,owner=0,index=0,stack=0,link=0;
};
thread_local ScreenPublicationCall screenPublicationCall __attribute__((tls_model("initial-exec")));
static_assert(sizeof(ScreenPublicationCall)==20,"ARM publication call fields");
void screenPublication(unsigned kind,uint32_t owner,unsigned index,uint32_t stack,uint32_t link){
    if(index>=16||kind<1||kind>2)return;
    auto& q=screenQLinks;
    if(owner!=(kind==1?q.publicationProvider:q.publicationLabels).load(std::memory_order_acquire)||!owner)return;
    const auto& own=screenPublicationCall;
    if(own.kind==kind&&own.owner==owner&&own.index==index&&own.stack==stack&&own.link==link)return;
    (kind==1?q.descriptorRevision[index]:q.labelRevision[index]).fetch_add(1,std::memory_order_release);
    q.publication.fetch_add(1,std::memory_order_release);
}
// Bind the exact direct native call, not its surrounding thread: setters can
// synchronously invoke listeners, which can publish again before returning.
void screenNativePublication(unsigned kind,uint32_t owner,unsigned index,uint32_t a,uint32_t b,uint32_t c,uint32_t function){
    const auto previous=screenPublicationCall;
    screenPublicationCall={kind,owner,index,0,0};auto* own=&screenPublicationCall;
    register uint32_t r0 asm("r0")=owner,r1 asm("r1")=index,r2 asm("r2")=a,r3 asm("r3")=b;
    if(kind==1){
        asm volatile("mov r12, sp\n str r12, [%[call], #12]\n adr r12, 1f\n str r12, [%[call], #16]\n blx %[function]\n1:"
          : "+r"(r0),"+r"(r1),"+r"(r2),"+r"(r3) : [call]"r"(own),[function]"r"(function) : "r12","lr","cc","memory","d0","d1","d2","d3","d4","d5","d6","d7","d16","d17","d18","d19","d20","d21","d22","d23","d24","d25","d26","d27","d28","d29","d30","d31");
    }else{
        asm volatile("sub sp, sp, #8\n str %[last], [sp]\n mov r12, sp\n str r12, [%[call], #12]\n adr r12, 1f\n str r12, [%[call], #16]\n blx %[function]\n1: add sp, sp, #8"
          : "+r"(r0),"+r"(r1),"+r"(r2),"+r"(r3) : [call]"r"(own),[function]"r"(function),[last]"r"(c) : "r12","lr","cc","memory","d0","d1","d2","d3","d4","d5","d6","d7","d16","d17","d18","d19","d20","d21","d22","d23","d24","d25","d26","d27","d28","d29","d30","d31");
    }
    screenPublicationCall=previous;
}
struct ScreenTrace {
    uint32_t magic,bytes,desired,applied,count,pending,installed,restoring;
    uint32_t inputs,changes,dropped,mode,provider,wrapper,receiver;
    uint32_t bindingLo,bindingHi;
};
struct ScreenTraceWrite {
    ~ScreenTraceWrite(){
        if(!trace)return;
        auto& q=screenQLinks;
        auto* t=reinterpret_cast<ScreenTrace*>(reinterpret_cast<char*>(trace)+128);
        *t={0x514c4b31,sizeof(*t),q.desired.load(),q.applied.load(),q.count.load(),q.pending.load(),q.installed.load(),q.restoring,
            q.inputs,q.changes,q.dropped,q.native.mode,q.native.provider,q.native.wrapper,q.receiver.load(),
            uint32_t(q.parameters.binding),uint32_t(q.parameters.binding>>32)};
    }
};
uint64_t screenNow(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return uint64_t(t.tv_sec)*1000+t.tv_nsec/1000000;}
template<class Function> Function screenFunction(uint32_t address){return reinterpret_cast<Function>(uintptr_t(imageBias+address));}
void screenLabel(unsigned index,uint32_t kind,const uint32_t* name,const uint32_t* display){
    screenNativePublication(2,screenQLinks.native.labels,index,kind,uint32_t(uintptr_t(name)),uint32_t(uintptr_t(display)),imageBias+TH_QLINK_LABEL_SET);
}
void screenText(unsigned index,const char* name,const char* value){
    uint32_t a=0,b=0;
    auto construct=screenFunction<void(*)(uint32_t*,const char*)>(TH_STRING_CTOR);
    auto destroy=screenFunction<void(*)(uint32_t*)>(TH_STRING_DTOR);
    construct(&a,name);construct(&b,value);screenLabel(qlink_grid_index(index,2),1,&a,&b);destroy(&b);destroy(&a);
}
void screenSaveLabels(uint32_t mask=0xffff){
    auto& q=screenQLinks;void* strings=pointer(reinterpret_cast<void*>(uintptr_t(q.native.labels)),0x14);
    auto copy=screenFunction<void(*)(uint32_t*,const uint32_t*)>(TH_STRING_COPY);
    auto destroy=screenFunction<void(*)(uint32_t*)>(TH_STRING_DTOR);
    for(unsigned i=0;i<16;++i)if(mask&(1u<<i)){auto* source=static_cast<uint32_t*>(strings)+3*i;
        if(q.labelsSaved){destroy(&q.labelName[i]);destroy(&q.labelValue[i]);}
        q.labelKind[i]=source[0];copy(&q.labelName[i],source+1);copy(&q.labelValue[i],source+2);
        q.savedLabelRevision[i]=q.labelRevision[i].load(std::memory_order_acquire);}
    q.labelsSaved=true;
}
void screenFreeLabels(){
    auto& q=screenQLinks;if(!q.labelsSaved)return;
    auto destroy=screenFunction<void(*)(uint32_t*)>(TH_STRING_DTOR);
    for(unsigned i=0;i<16;++i){destroy(&q.labelName[i]);destroy(&q.labelValue[i]);}
    q.labelsSaved=false;
}
bool screenOwns(void* receiver){
    auto& q=screenQLinks;
    return q.receiver.load(std::memory_order_acquire)==uint32_t(uintptr_t(receiver));
}
void screenInput(void* receiver,unsigned index,float value,uint32_t original){
    auto& q=screenQLinks;
    if(!screenOwns(receiver)){
        screenFunction<void(*)(void*,unsigned,float)>(original)(receiver,index,value);return;
    }
    index=qlink_column_position(index);
    const uint32_t epoch=q.applied.load(std::memory_order_acquire);
    if(!q.root.load(std::memory_order_acquire)||!epoch||epoch!=q.desired.load(std::memory_order_acquire)||
       index>=q.count.load(std::memory_order_acquire)||!std::isfinite(value)||
       word(reinterpret_cast<void*>(uintptr_t(q.native.controller)),0x5c)!=0||!screenTabMatches(q.native,q.savedTab)||q.publication.load(std::memory_order_acquire)!=q.appliedPublication.load(std::memory_order_acquire))return;
    auto& lane=q.lanes[index];uint32_t bits;std::memcpy(&bits,&value,4);
    lane.sequence.fetch_add(1,std::memory_order_acq_rel);
    lane.bits.store(bits,std::memory_order_relaxed);lane.epoch.store(epoch,std::memory_order_relaxed);
    lane.sequence.fetch_add(1,std::memory_order_release);
}
void screenActivity(void* receiver,unsigned index,bool active,uint32_t original){
    if(!screenOwns(receiver)){
        screenFunction<void(*)(void*,unsigned,bool)>(original)(receiver,index,active);return;
    }
    index=qlink_column_position(index);
    if(index<16)screenQLinks.lanes[index].activity.store(active?3:1,std::memory_order_release);
}
void screenFxInput(void* receiver,unsigned index,float value){screenInput(receiver,index,value,TH_QLINK_FX_INPUT);}
void screenSynthInput(void* receiver,unsigned index,float value){screenInput(receiver,index,value,TH_QLINK_PROGRAM_INPUT);}
void screenFxActivity(void* receiver,unsigned index,bool active){screenActivity(receiver,index,active,TH_QLINK_FX_ACTIVITY);}
void screenSynthActivity(void* receiver,unsigned index,bool active){screenActivity(receiver,index,active,TH_QLINK_PROGRAM_ACTIVITY);}
void screenDescriptor(void* provider,unsigned index,const void* config){
    screenNativePublication(1,uint32_t(uintptr_t(provider)),index,uint32_t(uintptr_t(config)),0,0,imageBias+TH_QLINK_DESCRIPTOR_SET);
}
void screenValue(void* mode,unsigned index,float value){
    // Complete native mode entry: load mode+0x50, then tail-call the provider
    // setter with the descriptor-domain value unchanged. Publishing through
    // its owner also supplies the observer's real mode-refresh witness.
    screenFunction<void(*)(void*,unsigned,float)>(TH_QLINK_MODE_VALUE_SET)(mode,index,value);
}
void screenAudio(void* argument){
    auto& q=screenQLinks;auto& job=*static_cast<ScreenJob*>(argument);
    const bool live=q.root.load(std::memory_order_acquire)==q.native.root&&q.native.root;
    if(live){
        void* provider=reinterpret_cast<void*>(uintptr_t(q.native.provider));
        void* mode=reinterpret_cast<void*>(uintptr_t(q.native.mode));
        void* receiver=static_cast<char*>(mode)+0x34;
        const bool current=word(reinterpret_cast<void*>(uintptr_t(q.native.controller)),0x5c)==0&&
            *(static_cast<unsigned char*>(mode)+0x30);
        if(job.operation==1&&job.epoch==q.desired.load(std::memory_order_acquire)&&current&&screenTabMatches(q.native,job.tab)&&job.publication==q.publication.load(std::memory_order_acquire)&&screenNativePending(q.native)==1){
            if(!q.installed.load(std::memory_order_acquire)){
                const bool fx=word(mode,0)==imageBias+TH_QLINK_FX_MODE_VPTR;
                if(word(receiver,0)!=imageBias+(fx?TH_QLINK_FX_RECEIVER_VPTR:TH_QLINK_PROGRAM_RECEIVER_VPTR)){
                    q.desired.store(0,std::memory_order_release);q.pending.store(0,std::memory_order_release);return;
                }
                void* descriptors=pointer(provider,0x30);
                auto clone=screenFunction<void(*)(void*,const void*)>(TH_QLINK_DESCRIPTOR_ASSIGN);
                for(unsigned i=0;i<16;++i){std::memset(q.savedDescriptors[i],0,sizeof(q.savedDescriptors[i]));
                    clone(q.savedDescriptors[i],static_cast<char*>(descriptors)+i*0x68+4);
                    q.savedDescriptors[i][0]=word(pointer(provider,0x34),i*4);q.savedDescriptorRevision[i]=job.descriptorRevision[i];q.savedValueLabelRevision[i]=job.labelRevision[i];}
                q.originalTable=word(receiver,0);
                std::memcpy(q.table,reinterpret_cast<const void*>(uintptr_t(q.originalTable-8)),sizeof(q.table));
                q.table[4]=uint32_t(uintptr_t(fx?screenFxInput:screenSynthInput));
                q.table[6]=uint32_t(uintptr_t(fx?screenFxActivity:screenSynthActivity));
                q.receiver.store(uint32_t(uintptr_t(receiver)),std::memory_order_release);
                uint32_t table=uint32_t(uintptr_t(q.table+2));std::memcpy(receiver,&table,4);
                q.installed.store(1,std::memory_order_release);
            }else{
                // Reassign the owned snapshot; native assignment releases any
                // previous erased conversion before retaining the new bank.
                void* descriptors=pointer(provider,0x30);
                auto clone=screenFunction<void(*)(void*,const void*)>(TH_QLINK_DESCRIPTOR_ASSIGN);
                for(unsigned i=0;i<16;++i){
                    const bool descriptor=job.captureNative||job.descriptorRevision[i]!=q.savedDescriptorRevision[i];
                    if(descriptor){clone(q.savedDescriptors[i],static_cast<char*>(descriptors)+i*0x68+4);q.savedDescriptorRevision[i]=job.descriptorRevision[i];}
                    if(descriptor||job.labelRevision[i]!=q.savedValueLabelRevision[i]){
                        q.savedDescriptors[i][0]=word(pointer(provider,0x34),i*4);q.savedValueLabelRevision[i]=job.labelRevision[i];
                    }
                }
            }
            q.savedTab=job.tab;
            for(unsigned i=0;i<16;++i){
                const unsigned parameter=qlink_column_position(i);
                // Native NormalisableRange<float> in an optional, then the
                // qualified linear Screen sensitivity/default fields. No erased
                // conversion callable and no engine/parameter pointer lives here.
                alignas(8) unsigned char config[0x60]{};
                float one=1.f,two=2.f,ten=10.f;
                std::memcpy(config+4,&one,4);std::memcpy(config+12,&one,4);
                config[0x44]=parameter<job.parameters.count;
                std::memcpy(config+0x4c,&two,4);std::memcpy(config+0x54,&one,4);
                config[0x58]=1;std::memcpy(config+0x5c,&ten,4);
                if(parameter<job.parameters.count&&job.parameters.parameters[parameter].steps>1){
                    float interval=1.f/(job.parameters.parameters[parameter].steps-1);std::memcpy(config+8,&interval,4);}
                if(job.publication!=q.publication.load(std::memory_order_acquire))break;
                screenDescriptor(provider,i,config);
                if(job.publication!=q.publication.load(std::memory_order_acquire))break;
                q.ownedDescriptorRevision[i]=job.descriptorRevision[i];
                screenValue(mode,i,parameter<job.parameters.count?job.parameters.parameters[parameter].value:0.f);
                q.lanes[parameter].activity.store(0,std::memory_order_release);
            }
            if(job.publication==q.publication.load(std::memory_order_acquire)){
                q.count.store(job.parameters.count,std::memory_order_release);
                q.appliedPublication.store(job.publication,std::memory_order_release);
                q.applied.store(job.epoch,std::memory_order_release);
            }else q.desired.store(0,std::memory_order_release);
        }else if(job.operation==2){
            if(q.installed.load(std::memory_order_acquire)){
                if(current&&screenTabMatches(q.native,q.savedTab))for(unsigned i=0;i<16;++i)if(q.descriptorRevision[i].load(std::memory_order_acquire)==q.ownedDescriptorRevision[i]){screenDescriptor(provider,i,q.savedDescriptors[i]+1);
                    float value;std::memcpy(&value,q.savedDescriptors[i],4);screenValue(mode,i,value);}
                if(word(receiver,0)==uint32_t(uintptr_t(q.table+2)))std::memcpy(receiver,&q.originalTable,4);
                q.receiver.store(0,std::memory_order_release);q.installed.store(0,std::memory_order_release);
            }
            q.applied.store(0,std::memory_order_release);
        }else if(job.operation==3&&current&&job.epoch==q.applied.load(std::memory_order_acquire)&&job.epoch==q.desired.load(std::memory_order_acquire)&&screenTabMatches(q.native,job.tab)&&job.publication==q.publication.load(std::memory_order_acquire)){
            for(unsigned i=0;i<job.parameters.count;++i)
                if(q.lanes[i].sequence.load(std::memory_order_acquire)==job.observed[i])
                    screenValue(mode,qlink_grid_index(i,2),job.parameters.parameters[i].value);
        }
    }
    if(job.operation==1&&q.applied.load(std::memory_order_acquire)!=job.epoch&&q.desired.load(std::memory_order_acquire)==job.epoch)q.desired.store(0,std::memory_order_release);
    q.pending.store(0,std::memory_order_release);
}
bool screenSubmit(unsigned operation,uint32_t epoch,const MpcEditorParameters& parameters,const uint32_t* publication=nullptr){
    auto& q=screenQLinks;if(q.pending.load(std::memory_order_acquire))return false;
    q.job.operation=operation;q.job.epoch=epoch;q.job.parameters=parameters;
    q.job.tab=q.requestedTab;q.job.captureNative=operation==1&&!screenSameTab(q.savedTab,q.requestedTab);
    q.job.publication=publication?*publication:operation==3?q.appliedPublication.load(std::memory_order_acquire):q.publication.load(std::memory_order_acquire);
    if(q.job.publication!=q.publication.load(std::memory_order_acquire))return false;
    for(unsigned i=0;i<16;++i){q.job.descriptorRevision[i]=q.descriptorRevision[i].load(std::memory_order_acquire);q.job.labelRevision[i]=q.labelRevision[i].load(std::memory_order_acquire);}
    if(q.job.publication!=q.publication.load(std::memory_order_acquire))return false;
    for(unsigned i=0;i<16;++i)q.job.observed[i]=q.lanes[i].consumed;
    q.pending.store(1,std::memory_order_release);
    // This exact native mode's pending owner prevents its destruction until
    // the queue fence completes. The existing qualified callable ABI is reused.
    if(transport_queue_submit(imageBias,q.native.mode+0x40,&q.job,screenAudio))return true;
    q.pending.store(0,std::memory_order_release);failure(11);return false;
}
void screenEndGestures(){
    auto& q=screenQLinks;
    for(unsigned i=0;i<16;++i)if(q.lanes[i].editing){
        if(q.slot&&q.slot->view)q.slot->api->parameter(q.slot->view,q.parameters.binding,i,MPC_EDITOR_PARAMETER_END,0);
        q.lanes[i].editing=false;
    }
}
void screenRelease(Slot& slot){
    auto& q=screenQLinks;if(q.slot!=&slot)return;
    q.desired.store(0,std::memory_order_release);screenEndGestures();q.slot=nullptr;q.restoring=true;
}
void screenFreeDescriptors(){
    auto& q=screenQLinks;
    // The native descriptor assignment releases its own erased conversions.
    alignas(8) unsigned char empty[0x60]{};
    auto assign=screenFunction<void(*)(void*,const void*)>(TH_QLINK_DESCRIPTOR_ASSIGN);
    for(auto& descriptor:q.savedDescriptors){assign(descriptor,empty);std::memset(descriptor,0,sizeof(descriptor));}
}
void screenRetiring(){
    auto& q=screenQLinks;q.publicationProvider.store(0,std::memory_order_release);q.publicationLabels.store(0,std::memory_order_release);q.desired.store(0,std::memory_order_release);screenEndGestures();
    q.slot=nullptr;q.root.store(0,std::memory_order_release);q.restoring=true;
    // Root destruction precedes the native mode destructors, which replace
    // their own tables. Revoke the presentation hook before that lifetime ends.
    if(q.receiver.load(std::memory_order_acquire)){
        void* receiver=reinterpret_cast<void*>(uintptr_t(q.receiver.load()));
        if(word(receiver,0)==uint32_t(uintptr_t(q.table+2)))std::memcpy(receiver,&q.originalTable,4);
        q.receiver.store(0,std::memory_order_release);q.installed.store(0,std::memory_order_release);
    }
}
void screenRefresh(Slot* wanted){
    ScreenTraceWrite publish;
    auto& q=screenQLinks;ScreenNative native{};ScreenTab tab{};
    void* textOwner=mpclearn_track_text_owner();
    bool valid=wanted&&wanted->view&&mpc_ui_app_is_open(wanted->host)&&
        (!textOwner||!pointer(textOwner,0x28))&&
        mpclearn_track_qlink_native(wanted->insert,reinterpret_cast<uint32_t*>(&native))&&screenReadTab(native,*wanted,tab);
    if(q.slot&&(!valid||wanted!=q.slot||native.mode!=q.native.mode||native.root!=q.native.root))screenRelease(*q.slot);
    if(q.restoring){
        if(q.pending.load(std::memory_order_acquire))return;
        // TabIndex publication includes nested native label jobs. Let that
        // existing fence settle before removing our receiver or restoring.
        if(q.root.load(std::memory_order_acquire)&&screenNativePending(q.native))return;
        if(q.installed.load(std::memory_order_acquire)){
            if(q.root.load(std::memory_order_acquire)&&
               word(reinterpret_cast<void*>(uintptr_t(q.native.controller)),0x5c)==0&&
               *(reinterpret_cast<unsigned char*>(uintptr_t(q.native.mode))+0x30)&&q.labelsSaved&&screenTabMatches(q.native,q.labelTab))
                for(unsigned i=0;i<16;++i)if(q.labelRevision[i].load(std::memory_order_acquire)==q.ownedLabelRevision[i])screenLabel(i,q.labelKind[i],q.labelName+i,q.labelValue+i);
            screenSubmit(2,0,{});return;
        }
        q.publicationProvider.store(0,std::memory_order_release);q.publicationLabels.store(0,std::memory_order_release);
        screenFreeDescriptors();screenFreeLabels();q.native={};q.parameters={};q.requestedTab={};q.savedTab={};q.labelTab={};q.restoring=false;
        q.root.store(0,std::memory_order_release);
    }
    if(!valid)return;
    if(!q.slot){
        // Wait for the native Screen owner to finish its activation/labels.
        if(screenNativePending(native))return;
        q.native=native;q.slot=wanted;q.requestedTab=tab;q.labelTab=tab;
        q.root.store(native.root,std::memory_order_release);
        q.publicationProvider.store(native.provider,std::memory_order_release);q.publicationLabels.store(native.labels,std::memory_order_release);
        screenSaveLabels();q.nextRefresh=0;
        for(auto& lane:q.lanes){lane.consumed=lane.sequence.load(std::memory_order_acquire);lane.editing=false;}
    }
    if(!screenSameTab(q.requestedTab,tab)){
        q.desired.store(0,std::memory_order_release);screenEndGestures();
        q.requestedTab=tab;q.nextRefresh=0;
    }
    // savedTab belongs to the completed audio job. Read it only after that
    // job's release/acquire handoff, then wait for native publication too.
    if(q.pending.load(std::memory_order_acquire)&&q.job.operation==1)return;
    const bool captureNative=!screenSameTab(q.savedTab,tab);
    const bool displaced=q.publication.load(std::memory_order_acquire)!=q.appliedPublication.load(std::memory_order_acquire);
    if(displaced){q.desired.store(0,std::memory_order_release);screenEndGestures();q.nextRefresh=0;}
    if((captureNative||displaced)&&(q.pending.load(std::memory_order_acquire)||screenNativePending(native)))return;
    const uint64_t now=screenNow();bool input=false;
    for(auto& lane:q.lanes)input|=lane.sequence.load(std::memory_order_acquire)!=lane.consumed;
    // Refresh idle labels at most 30Hz. New turns bypass this cadence, and
    // the engine rechecks the selection binding before every mutation.
    if(!input&&now<q.nextRefresh)return;
    q.nextRefresh=now+33;
    MpcEditorParameters parameters{};
    if(!wanted->api->parameters(wanted->view,&parameters)||parameters.count>16){screenRelease(*wanted);return;}
    if(parameters.binding!=q.parameters.binding||parameters.count!=q.parameters.count||!q.desired.load(std::memory_order_acquire)||captureNative||displaced){
        q.desired.store(0,std::memory_order_release);screenEndGestures();
        if(q.pending.load(std::memory_order_acquire))return;
        const uint32_t publication=q.publication.load(std::memory_order_acquire);
        uint32_t labels=0;
        for(unsigned i=0;i<16;++i)if(q.labelRevision[i].load(std::memory_order_acquire)!=q.savedLabelRevision[i])labels|=1u<<i;
        if(captureNative&&!screenSameTab(q.labelTab,tab))labels=0xffff;
        if(labels){screenSaveLabels(labels);q.labelTab=tab;}
        if(publication!=q.publication.load(std::memory_order_acquire))return;
        q.parameters=parameters;const uint32_t epoch=++q.nextEpoch;
        q.desired.store(epoch,std::memory_order_release);
        for(unsigned i=0;i<16;++i){q.lanes[i].consumed=q.lanes[i].sequence.load(std::memory_order_acquire);
            screenText(i,i<parameters.count?parameters.parameters[i].name:"",i<parameters.count?parameters.parameters[i].display:"");
            if(publication!=q.publication.load(std::memory_order_acquire)){q.desired.store(0,std::memory_order_release);return;}
            const unsigned index=qlink_grid_index(i,2);q.ownedLabelRevision[index]=q.labelRevision[index].load(std::memory_order_acquire);}
        if(!screenSubmit(1,epoch,parameters,&publication))q.desired.store(0,std::memory_order_release);
        return;
    }
    const uint32_t epoch=q.desired.load(std::memory_order_acquire);bool changed=false;
    for(unsigned i=0;i<parameters.count;++i){auto& lane=q.lanes[i];
        uint32_t seq=lane.sequence.load(std::memory_order_acquire),bits=lane.bits.load(std::memory_order_relaxed),eventEpoch=lane.epoch.load(std::memory_order_relaxed);
        if(!(seq&1)&&seq!=lane.consumed&&seq==lane.sequence.load(std::memory_order_acquire)){
            lane.consumed=seq;
            if(eventEpoch==epoch){float value;std::memcpy(&value,&bits,4);++q.inputs;
                if(wanted->api->parameter(wanted->view,parameters.binding,i,MPC_EDITOR_PARAMETER_SET,value)){
                    lane.editing=true;lane.lastChange=now;changed=true;++q.changes;}
                else ++q.dropped;
            }else ++q.dropped;
        }
        const uint32_t activity=lane.activity.load(std::memory_order_acquire);
        if(lane.editing&&(activity==1||(!activity&&now-lane.lastChange>=250))){
            wanted->api->parameter(wanted->view,parameters.binding,i,MPC_EDITOR_PARAMETER_END,0);lane.editing=false;}
    }
    if(changed&&!wanted->api->parameters(wanted->view,&parameters))return;
    if(parameters.binding!=q.parameters.binding)return;
    bool feedback=false;
    for(unsigned i=0;i<parameters.count;++i){auto& old=q.parameters.parameters[i];auto& value=parameters.parameters[i];
        if(old.value!=value.value||std::strcmp(old.display,value.display)||std::strcmp(old.name,value.name)){
            const unsigned index=qlink_grid_index(i,2);uint32_t revision=q.labelRevision[index].load(std::memory_order_acquire);
            screenText(i,value.name,value.display);
            if(revision==q.labelRevision[index].load(std::memory_order_acquire))q.ownedLabelRevision[index]=revision;
            feedback=true;
        }
    }
    if((feedback||changed)&&!q.pending.load(std::memory_order_acquire)&&screenSubmit(3,epoch,parameters))q.parameters=parameters;
    if(changed||feedback)wanted->api->present(wanted->view);
}
