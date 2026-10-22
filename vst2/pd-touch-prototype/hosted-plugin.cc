/* Same hosted Pd owner for audio, canonical state and the borrowed touch view. */
#define _Static_assert static_assert
#include "../chunk-probe/abi.h"
#include "../instrument/midi.h"
#undef _Static_assert
#include "pd-touch.h"
#include "pd-canvas.h"
#include "hosted-api.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
#ifndef PD_HOSTED_INPUTS
#define PD_HOSTED_INPUTS 0
#endif
namespace {
constexpr int uid=PD_HOSTED_INPUTS?0x4d705046:0x4d705053;
const char* name=PD_HOSTED_INPUTS?"Pd Sketch FX":"Pd Sketch Synth";
struct Pending { uint64_t due; unsigned status,first,second; };
struct Plugin {
    Effect effect{};
    std::recursive_mutex mutex;
    std::recursive_mutex viewMutex;
    PdTouch* pd=nullptr;
    PdCanvas* view=nullptr;
    PdCanvasViewState viewState{};
    uint64_t token=0,frames=0;
    unsigned phase=0,eventCount=0,width=0,height=0;
    int rate=44100,active=0,failed=0,retired=0;
    float input[128]{},output[128]{};
    Pending events[256]{};
    uint64_t held[16][128]{};
    unsigned recovery=0;
    std::vector<unsigned char> chunk;
    ~Plugin(){pd_canvas_close(&view);pd_touch_close(&pd);}
    void reset(){phase=eventCount=0;frames=0;std::memset(input,0,sizeof(input));std::memset(output,0,sizeof(output));}
    void resetMidi(){recovery=0;std::memset(held,0,sizeof(held));}
};
std::mutex registryMutex;
std::map<uint64_t,std::shared_ptr<Plugin>> registry;
uint64_t nextToken=0;
std::shared_ptr<Plugin> find(uint64_t token){std::lock_guard<std::mutex> lock(registryMutex);auto at=registry.find(token);return at==registry.end()?nullptr:at->second;}
std::shared_ptr<Plugin> retain(Effect* effect){return find(static_cast<Plugin*>(effect->object)->token);}
void put32(unsigned char* p,uint32_t value){for(unsigned i=0;i<4;++i)p[i]=(value>>(8*i))&255;}
uint32_t get32(const unsigned char* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
bool save(Plugin& p){
    unsigned char* bytes=nullptr;uint32_t length=0;char origin[512];
    if(!pd_touch_serialize(p.pd,&bytes,&length,origin))return false;
    std::unique_ptr<unsigned char,decltype(&std::free)> owner(bytes,&std::free);
    const uint32_t pathLength=std::strlen(origin);
    p.chunk.resize(20+pathLength+length);
    std::memcpy(p.chunk.data(),"MPPDST1",8);put32(p.chunk.data()+8,1);put32(p.chunk.data()+12,pathLength);put32(p.chunk.data()+16,length);
    std::memcpy(p.chunk.data()+20,origin,pathLength);std::memcpy(p.chunk.data()+20+pathLength,bytes,length);return true;
}
bool restore(Plugin& p,const void* data,intptr_t count){
    if(!data||count<21||count>4194836)return false;
    const auto* bytes=static_cast<const unsigned char*>(data);
    if(std::memcmp(bytes,"MPPDST1",8)||get32(bytes+8)!=1)return false;
    uint32_t pathLength=get32(bytes+12),length=get32(bytes+16);
    if(!pathLength||pathLength>=512||!length||length>4194304||20u+pathLength+length!=uint32_t(count)||std::memchr(bytes+20,0,pathLength))return false;
    char origin[512];std::memcpy(origin,bytes+20,pathLength);origin[pathLength]=0;
    if(!pd_touch_restore(p.pd,origin,bytes+20+pathLength,length))return false;
    p.reset();p.resetMidi();p.viewState={};pd_canvas_sync(p.view);return true;
}
bool events(Plugin& p,const MidiEvents* input){
    if(!input||input->count<0||input->count>256)return false;
    for(int i=0;i<input->count;++i){const auto* event=input->events[i];
        if(!event||event->type!=1)continue;
        if(event->byte_size!=sizeof(MidiEvent)||event->delta<0||event->delta>1048576||p.eventCount==256)return false;
        const unsigned status=event->data[0],first=event->data[1],second=event->data[2];
        if(status<0x80||status>0xef||first>127||second>127)continue;
        // Pd advances in 64-frame ticks. Round an event forward to the next
        // tick; the separately advertised streaming latency is 64 frames.
        Pending item{(p.frames+event->delta+63)/64*64,status,first,second};
        unsigned at=p.eventCount++;while(at&&p.events[at-1].due>item.due){p.events[at]=p.events[at-1];--at;}p.events[at]=item;
    }return true;
}
bool midi(Plugin& p,unsigned status,unsigned first,unsigned second){
    if(!pd_touch_midi(p.pd,status,first,second))return false;
    auto& count=p.held[status&15][first];
    if((status&0xf0)==0x90&&second)++count;
    else if(((status&0xf0)==0x80||(status&0xf0)==0x90)&&count)--count;
    return true;
}
void recoverMidi(Plugin& p){
    // Lost events can include note-offs. Release through the existing MIDI
    // ingress, including repeated pitches, without resetting the patch/DSP.
    // Bound deliveries per tick; newly accepted events wait for recovery.
    unsigned budget=256;
    while(p.recovery&&budget){
        unsigned at=p.recovery-1;
        if(at<48){
            const unsigned controls[]={64,123,120};
            if(!pd_touch_midi(p.pd,0xb0|at/3,controls[at%3],0)){p.failed=1;return;}
            --budget;++p.recovery;
        }else{
            unsigned key=at-48;
            if(key==16*128){p.recovery=0;break;}
            if(!p.held[key/128][key%128]){++p.recovery;continue;}
            if(!midi(p,0x80|key/128,key%128,0)){p.failed=1;return;}
            --budget;
        }
    }
}
void process(Effect* effect,const float* const* input,float** output,int32_t frames){
    if(frames<=0)return;auto p=retain(effect);if(!p)return;std::lock_guard<std::recursive_mutex> lock(p->mutex);
    for(int32_t i=0;i<frames;++i){
        float left=input&&PD_HOSTED_INPUTS&&input[0]?input[0][i]:0;
        float right=input&&PD_HOSTED_INPUTS&&input[1]?input[1][i]:0;
        if(output){if(output[0])output[0][i]=p->active&&!p->failed?p->output[2*p->phase]:0;if(output[1])output[1][i]=p->active&&!p->failed?p->output[2*p->phase+1]:0;}
        if(!p->active||p->failed)continue;
        p->input[2*p->phase]=left;p->input[2*p->phase+1]=right;++p->phase;++p->frames;
        if(p->phase==64){
            recoverMidi(*p);
            while(!p->recovery&&!p->failed&&p->eventCount&&p->events[0].due<=p->frames-64){const Pending event=p->events[0];
                std::memmove(p->events,p->events+1,(--p->eventCount)*sizeof(Pending));
                if(!midi(*p,event.status,event.first,event.second))p->failed=1;}
            if(!pd_touch_tick(p->pd,PD_HOSTED_INPUTS?p->input:nullptr,p->output))p->failed=1;
            p->phase=0;
        }
    }
}
intptr_t dispatch(Effect* effect,int32_t op,int32_t index,intptr_t value,void* data,float option){
    auto p=retain(effect);if(!p)return 0;
    // Recall also replaces the view snapshot. Take view ownership before the
    // engine lock, as the borrowed-view entry points do. Audio never takes it.
    std::unique_lock<std::recursive_mutex> viewLock(p->viewMutex,std::defer_lock);
    if(op==24)viewLock.lock();
    std::lock_guard<std::recursive_mutex> lock(p->mutex);
    try {switch(op){
    case 0:return !p->failed;
    case 1:{p->retired=1;p->active=0;pd_touch_audio(p->pd,p->rate,0);std::lock_guard<std::mutex> registryLock(registryMutex);registry.erase(p->token);return 0;}
    case 2:return 0;case 3:return 0;
    case 5:if(data)std::snprintf(static_cast<char*>(data),24,"Pd Sketch");return 0;
    case 10:if(std::isfinite(option)&&option>=8000&&option<=192000){p->rate=int(option);p->failed=!pd_touch_audio(p->pd,p->rate,p->active);p->reset();return !p->failed;}return 0;
    case 11:return 1;
    case 12:p->active=value!=0;p->reset();p->failed=!pd_touch_audio(p->pd,p->rate,p->active);return !p->failed;
    case 23:if(data&&save(*p)){*static_cast<void**>(data)=p->chunk.data();return p->chunk.size();}return 0;
    case 24:return restore(*p,data,value);
    case 25:if(!events(*p,static_cast<const MidiEvents*>(data))){p->eventCount=0;if(!p->recovery)p->recovery=1;return 0;}return 1;
    case 29:if(!index&&data){std::snprintf(static_cast<char*>(data),24,"Pd Sketch");return 1;}return 0;
    case 35:return PD_HOSTED_INPUTS?1:2;
    case 45:case 48:if(data)std::snprintf(static_cast<char*>(data),32,"%s",name);return 1;
    case 47:if(data)std::snprintf(static_cast<char*>(data),64,"mpclearn / Pure Data");return 1;
    case 49:return 1;
    case 51:return data&&(!std::strcmp(static_cast<const char*>(data),"receiveVstEvents")||!std::strcmp(static_cast<const char*>(data),"receiveVstMidiEvent"));
    case 58:return 2400;
    default:return 0;
    }}catch(...){p->failed=1;return 0;}
}
uint32_t instances(MpcPdInstance* rows,uint32_t capacity){
    std::vector<std::shared_ptr<Plugin>> owners;{std::lock_guard<std::mutex> lock(registryMutex);for(auto& entry:registry)owners.push_back(entry.second);}
    uint32_t count=0;for(auto& p:owners){std::lock_guard<std::recursive_mutex> lock(p->mutex);if(p->retired)continue;
        if(rows&&count<capacity){auto& row=rows[count];row.token=p->token;row.frames=p->frames;row.active=p->active;row.sample_rate=p->rate;
            std::snprintf(row.label,sizeof(row.label),"%s #%llu / %s / %d Hz",name,(unsigned long long)p->token,p->active?"MPC active":"stopped",p->rate);}++count;}return count;
}
template<class Operation> int invoke(uint64_t token,Operation operation){auto p=find(token);if(!p)return 0;std::lock_guard<std::recursive_mutex> viewLock(p->viewMutex);std::lock_guard<std::recursive_mutex> lock(p->mutex);if(p->retired)return 0;try{return operation(*p);}catch(...){return 0;}}
int open(uint64_t token,uint32_t width,uint32_t height){
    auto p=find(token);if(!p)return 0;
    std::lock_guard<std::recursive_mutex> viewLock(p->viewMutex);
    {std::lock_guard<std::recursive_mutex> lock(p->mutex);
        if(p->retired)return 0;
        if(p->view&&p->width==width&&p->height==height)return 1;}
    // View-only allocation, font I/O and cleanup must not block the audio owner.
    // Recall also holds viewMutex; retirement is rechecked before publication.
    try {
        if(p->view){pd_canvas_view_state(p->view,&p->viewState,0);pd_canvas_close(&p->view);}
        auto dispose=[](PdCanvas* view){pd_canvas_close(&view);};
        std::unique_ptr<PdCanvas,decltype(dispose)> next(pd_canvas_prepare_attachment(width,height),dispose);
        if(!next)return 0;
        {std::lock_guard<std::recursive_mutex> lock(p->mutex);
            if(p->retired||!pd_canvas_bind_attachment(next.get(),p->pd))return 0;}
        // working()/recallView() use only the new copied snapshot, never Pd.
        pd_canvas_view_state(next.get(),&p->viewState,1);
        {std::lock_guard<std::recursive_mutex> lock(p->mutex);
            if(p->retired)return 0;
            p->view=next.release();p->width=width;p->height=height;}
        return 1;
    }catch(...){return 0;}
}
int pointer(uint64_t token,uint32_t event,float x,float y){return invoke(token,[=](Plugin& p){if(!p.view)return 0;pd_canvas_pointer(p.view,event,x,y);return 1;});}
int action(uint64_t token,uint32_t intent){return invoke(token,[=](Plugin& p){if(!p.view)return 0;pd_canvas_action(p.view,intent);if(intent==6||intent==14){p.viewState={};pd_canvas_sync(p.view);}return 1;});}
int load(uint64_t token,const char* path){return invoke(token,[=](Plugin& p){if(!path)return 0;int okay=p.view?pd_canvas_load(p.view,path):pd_touch_load(p.pd,path);if(okay){p.reset();p.resetMidi();p.viewState={};}return okay;});}
int exportPixels(uint64_t token,unsigned char* output,uint32_t size,uint32_t* revision,bool cached){
    auto p=find(token);if(!p)return 0;
    std::lock_guard<std::recursive_mutex> viewLock(p->viewMutex);
    {std::lock_guard<std::recursive_mutex> lock(p->mutex);
        if(p->retired||!p->view||!output||!revision||size<p->width*p->height*4)return 0;}
    // Rasterize only the view's copied snapshot. Keep its lifetime/recall lock,
    // but release the audio owner's lock before drawing or copying pixels.
    try{uint32_t current=0;const auto* input=pd_canvas_pixels(p->view,&current);if(!input)return 0;
        if(!cached||*revision!=current)std::memcpy(output,input,p->width*p->height*4);
        *revision=current;return 1;}catch(...){return 0;}}
int pixels(uint64_t token,unsigned char* output,uint32_t size,uint32_t* revision){return exportPixels(token,output,size,revision,false);}
int cachedPixels(uint64_t token,unsigned char* output,uint32_t size,uint32_t* revision){return exportPixels(token,output,size,revision,true);}
int status(uint64_t token,char* output,uint32_t size){return invoke(token,[=](Plugin& p){if(!output||!size)return 0;std::snprintf(output,size,"%s",p.view?pd_canvas_status(p.view):pd_touch_error(p.pd));return 1;});}
void close(uint64_t token){
    auto p=find(token);if(!p)return;
    std::lock_guard<std::recursive_mutex> viewLock(p->viewMutex);
    {std::lock_guard<std::recursive_mutex> lock(p->mutex);if(p->retired)return;}
    pd_canvas_view_state(p->view,&p->viewState,0);pd_canvas_close(&p->view);
}
uint64_t bind(const void* effect){std::lock_guard<std::mutex> lock(registryMutex);for(auto& item:registry)if(&item.second->effect==effect)return item.first;return 0;}
int view(uint64_t token,uint32_t intent,float x,float y,float amount){return invoke(token,[=](Plugin& p){return pd_canvas_view(p.view,intent,x,y,amount);});}
int viewport(uint64_t token,MpcEditorViewport* state){return invoke(token,[=](Plugin& p){return pd_canvas_viewport(p.view,state);});}
struct CatalogEntry { const char* text; const char* description; unsigned control; };
const CatalogEntry catalogEntries[]={
    {"hsl","Horizontal slider",1},{"vsl","Vertical slider",1},{"tgl","Toggle",1},
    {"bng","Bang button",1},{"nbx","Number box",1},{"hradio","Horizontal radio",1},{"vradio","Vertical radio",1},
    {"osc~ 440","Sine oscillator",0},{"phasor~ 1","Ramp oscillator",0},{"noise~","White noise",0},
    {"*~ 0.2","Signal gain",0},{"+~","Add signals",0},{"lop~ 1000","Low-pass filter",0},
    {"hip~ 20","High-pass filter",0},{"vcf~ 440 1","Resonant filter",0},
    {"line~","Signal ramp",0},{"vline~","Scheduled signal ramp",0},{"dac~","MPC audio output",0},
    {"adc~","MPC audio input",0},{"notein","MIDI note input",0},{"ctlin","MIDI controller input",0},
    {"mtof","MIDI pitch to frequency",0},{"poly 16 1","Polyphonic voice allocation",0},
    {"pack f f","Build a list",0},{"unpack f f","Separate a list",0},
    {"metro 500","Timed bangs",0},{"loadbang","Bang when the patch loads",0},
    {"+","Add numbers",0},{"*","Multiply numbers",0},{"select 0","Match a value",0}
};
uint32_t catalog(MpcEditorItem* output,uint32_t capacity){const unsigned count=sizeof(catalogEntries)/sizeof(*catalogEntries);
    for(unsigned i=0;output&&i<count&&i<capacity;++i){output[i]={};output[i].id=i;output[i].control=catalogEntries[i].control;std::snprintf(output[i].name,sizeof(output[i].name),"%s",catalogEntries[i].text);std::snprintf(output[i].detail,sizeof(output[i].detail),"%s",catalogEntries[i].description);}return count;}
int create(uint64_t token,uint32_t id,float x,float y){if(id>=sizeof(catalogEntries)/sizeof(*catalogEntries))return 0;return invoke(token,[=](Plugin& p){return pd_canvas_create(p.view,PD_CREATE_OBJECT,catalogEntries[id].text,x,y);});}
int textBegin(uint64_t token,char* title,uint32_t titleSize,char* initial,uint32_t initialSize){return invoke(token,[=](Plugin& p){return pd_canvas_text_begin(p.view,title,titleSize,initial,initialSize);});}
int textFinish(uint64_t token,int accepted,const char* text){return invoke(token,[=](Plugin& p){return pd_canvas_text_finish(p.view,accepted,text);});}
int parameters(uint64_t token,MpcEditorParameters* output){return invoke(token,[=](Plugin& p){return pd_canvas_parameters(p.view,output);});}
int parameter(uint64_t token,uint64_t binding,uint32_t index,uint32_t event,float value){return invoke(token,[=](Plugin& p){return pd_canvas_parameter(p.view,binding,index,event,value);});}
const MpcPdHostedApi api={MPC_PD_HOSTED_ABI,sizeof(MpcPdHostedApi),instances,open,pointer,action,load,cachedPixels,status,close,bind,view,catalog,create,viewport,textBegin,textFinish,parameters,parameter};
const MpcPdHostedApi legacy5={5,sizeof(MpcPdHostedApi),instances,open,pointer,action,load,pixels,status,close,bind,view,catalog,create,viewport,textBegin,textFinish,parameters,parameter};
const MpcPdHostedApi legacy4={4,offsetof(MpcPdHostedApi,parameters),instances,open,pointer,action,load,pixels,status,close,bind,view,catalog,create,viewport,textBegin,textFinish,nullptr,nullptr};
const MpcPdHostedApi legacy3={3,offsetof(MpcPdHostedApi,text_begin),instances,open,pointer,action,load,pixels,status,close,bind,view,catalog,create,viewport,nullptr,nullptr,nullptr,nullptr};
const MpcPdHostedApi legacy2={2,offsetof(MpcPdHostedApi,viewport),instances,open,pointer,action,load,pixels,status,close,bind,view,catalog,create,nullptr,nullptr,nullptr,nullptr,nullptr};
const MpcPdHostedApi legacy={1,offsetof(MpcPdHostedApi,bind),instances,open,pointer,action,load,pixels,status,close,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr};
}
extern "C" __attribute__((visibility("default"))) const MpcPdHostedApi* mpclearn_pd_hosted_api(uint32_t version){return version==MPC_PD_HOSTED_ABI?&api:version==5?&legacy5:version==4?&legacy4:version==3?&legacy3:version==2?&legacy2:version==1?&legacy:nullptr;}
extern "C" __attribute__((visibility("default"))) Effect* VSTPluginMain(Host host){
    try{auto p=std::make_shared<Plugin>();{std::lock_guard<std::mutex> lock(registryMutex);p->token=++nextToken;}
        if(host){intptr_t rate=host(nullptr,16,0,0,nullptr,0);if(rate>=8000&&rate<=192000)p->rate=rate;}
        // Variant-local API tokens remain distinct in the shared writer root.
        p->pd=pd_touch_open_hosted(p->token*2+(PD_HOSTED_INPUTS?1:0),PD_HOSTED_INPUTS,p->rate);if(!p->pd)return nullptr;
        p->effect.magic=0x56737450;p->effect.dispatch=dispatch;p->effect.process=p->effect.replacing=process;
        p->effect.programs=1;p->effect.inputs=PD_HOSTED_INPUTS;p->effect.outputs=2;p->effect.flags=0x30|(PD_HOSTED_INPUTS?0:0x100);
        p->effect.delay=64;p->effect.reserved_float=1;p->effect.object=p.get();p->effect.uid=uid;p->effect.version=1;
        {std::lock_guard<std::mutex> lock(registryMutex);registry[p->token]=p;}return &p->effect;
    }catch(...){return nullptr;}
}
