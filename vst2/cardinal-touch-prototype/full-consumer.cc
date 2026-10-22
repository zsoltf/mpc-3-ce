/* Native presentation of the exact Cardinal instance retained by MPC. */
#include "toolkit-example.h"
#include "full-touch-api.h"
#ifdef MPCLEARN_CARDINAL_GPU
#include "../private/cardinal-gpu/presenter.inc"
#endif
#include "editor-shell.h"
#include <dlfcn.h>
#include <link.h>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>
#include <ctime>
namespace {
const char* libraryRoot="/tmp/mpclearn-cardinal-touch/library";
struct Library {void* handle;const MpcCardinalTouchApi* api;std::string path;};
struct Row {std::string label,path;unsigned library=0;uint64_t token=0;bool directory=false;};
struct Tool {const char* name;uint32_t intent;};
const Tool tools[]={{"UNDO",4},{"REDO",5},{"DELETE SELECTION",6},{"FOCUS SELECTION",1000},
    {"REMOVE CABLE",MPC_CARDINAL_REMOVE_CABLE},
    {"SAVE COPY",2},{"LOAD COPY",32},{"RACK BROWSER",1},{"TEXT KEYS",23},
    {"MODULE OPTIONS",MPC_CARDINAL_MODULE_OPTIONS},{"HOST PARAMETERS",33},{"SCROLL UP",30},{"SCROLL DOWN",31},{"INSTANCES (DEV)",20}};
struct Session {
    std::vector<Library> libraries;std::vector<Row> rows;
    std::vector<MpcEditorItem> catalog;std::vector<unsigned> filtered;
    std::vector<std::string> catalogLabels;
    std::vector<unsigned char> pixels;mpc_editor::Shell shell;
    int selected=-1,category=0,armed=-1;bool placingContact=false,keyboard=false;
    std::string query;
    MpcCardinalHostParameters hostValues{};
    MpcUiList* hostList=nullptr;MpcUiSlider* hostSlider=nullptr;MpcUiControl* hostLabel=nullptr;
    unsigned hostIndex=0;bool hostDragging=false;
    uint64_t lastAnimation=0;
    uint64_t token=0;uint32_t revision=UINT32_MAX,paintRevision=0;
#ifdef MPCLEARN_CARDINAL_GPU
    uint64_t overlayGeneration=0;const MpcCardinalOverlayApi* overlay=nullptr;bool overlayChecked=false;
#endif
    std::string directory=libraryRoot;
    ~Session(){
#ifdef MPCLEARN_CARDINAL_GPU
        if(overlay)overlay->release(token);
        if(overlayGeneration)cardinal_presenter_close(overlayGeneration);
#endif
        if(selected>=0){
            if(hostValues.binding)libraries[selected].api->host_parameters(token,MPC_CARDINAL_HOST_CLOSE,&hostValues);
            libraries[selected].api->close(token);
        }
        for(auto& item:libraries)dlclose(item.handle);
    }
};
Session* session(MpclearnToolkitExampleApp* app){return static_cast<Session*>(app->session);}
const MpcCardinalTouchApi* api(MpclearnToolkitExampleApp* app){auto* s=session(app);return s&&s->selected>=0?s->libraries[s->selected].api:nullptr;}
void status(MpclearnToolkitExampleApp* app,const char* text){mpc_ui_label_text(app->status,text);}
#include "full-host-controls.inc"
void frame(void* context){
    auto* app=static_cast<MpclearnToolkitExampleApp*>(context);auto* s=session(app);auto* owner=api(app);if(!owner)return;
    if(!owner->advance(s->token))return;
    bool omit=false;uint32_t revision=0;
#ifdef MPCLEARN_CARDINAL_GPU
    const uint64_t paint=cardinal_presenter_paint_ticket(uintptr_t(app->image));
    if(app->effect&&s->overlayGeneration){
        if(!s->overlayChecked){s->overlayChecked=true;auto get=reinterpret_cast<const MpcCardinalOverlayApi*(*)(uint32_t)>(dlsym(s->libraries[s->selected].handle,"mpclearn_cardinal_overlay_api"));s->overlay=get?get(1):nullptr;if(s->overlay&&(s->overlay->size!=sizeof(*s->overlay)||s->overlay->abi!=1))s->overlay=nullptr;}
        if(s->overlay){MpcCardinalOverlayTarget target{};if(cardinal_presenter_target(s->overlayGeneration,&target)){uint32_t gpuRevision=0;int okay=s->overlay->render(s->token,&target,&gpuRevision);cardinal_presenter_ready(s->overlayGeneration,target.slot,okay,gpuRevision);if(okay)omit=cardinal_presenter_omit(s->overlayGeneration,paint,target.slot,gpuRevision)!=0;}}
    }

#endif
    if(!omit){
        if(!owner->pixels(s->token,s->pixels.data(),s->pixels.size(),&revision)){
#ifdef MPCLEARN_CARDINAL_GPU
            cardinal_presenter_cpu(s->overlayGeneration,paint,0,0);
#endif
            status(app,"Cardinal instance closed");return;}
        if(revision!=s->revision){
            const uint32_t next=s->paintRevision+1;
            if(!mpc_ui_image_update(app->image,s->pixels.data(),app->width*4,s->pixels.size(),next)){
#ifdef MPCLEARN_CARDINAL_GPU
                cardinal_presenter_cpu(s->overlayGeneration,paint,0,revision);
#endif
                status(app,"Image update unavailable");return;}
            s->paintRevision=next;s->revision=revision;
        }

#ifdef MPCLEARN_CARDINAL_GPU
        cardinal_presenter_cpu(s->overlayGeneration,paint,1,revision);
#endif
    }
    char caption[512];
    if(s->armed<0&&owner->status(s->token,caption,sizeof(caption)))status(app,caption);
}
void refresh(MpclearnToolkitExampleApp* app){auto* s=session(app);auto* owner=api(app);if(!owner)return;
    MpcEditorViewport view{};if(owner->viewport(s->token,&view)&&view.mode!=s->shell.currentMode){s->shell.currentMode=view.mode;s->shell.panPad=view.mode==MPC_EDITOR_PAN;s->shell.show(s->shell.page);}
    mpc_ui_image_invalidate(app->image);}

// Called only by the existing visible track editor's UI drain. Never renders
// here: ordinary paint owns scene stepping and presentation, with coalescing.
void animationIdle(MpclearnToolkitExampleApp* app){
    auto* s=session(app);if(!s||!api(app)||s->keyboard)return;
#ifdef MPCLEARN_CARDINAL_GPU
    if(!cardinal_presenter_refresh_allowed(s->overlayGeneration)){s->lastAnimation=0;return;}
#endif
    timespec now{};if(clock_gettime(CLOCK_MONOTONIC,&now))return;
    const uint64_t ns=uint64_t(now.tv_sec)*1000000000u+now.tv_nsec;
    if(s->lastAnimation&&ns-s->lastAnimation<33333333u)return;
    s->lastAnimation=ns;
    if(s->shell.page==mpc_editor::HOST_PARAMETERS)hostControlsRead(app);
    mpc_ui_image_invalidate(app->image);
}

void keyboard(MpclearnToolkitExampleApp* app,bool shown){auto* s=session(app);s->keyboard=shown;mpc_ui_control_visible(app->image,!shown);for(auto* key:app->keys)if(key)mpc_ui_control_visible(key,shown);}
int discover(dl_phdr_info* info,size_t,void* context){auto* s=static_cast<Session*>(context);const char* path=info->dlpi_name;if(!path||!*path)return 0;
    const char* name=strrchr(path,'/');name=name?name+1:path;if(strcmp(name,"CardinalSynth.so")&&strcmp(name,"CardinalFX.so"))return 0;
    for(auto& item:s->libraries)if(item.path==path)return 0;
    void* handle=dlopen(path,RTLD_NOW|RTLD_NOLOAD);if(!handle)return 0;
    auto get=reinterpret_cast<const MpcCardinalTouchApi*(*)(uint32_t)>(dlsym(handle,"mpclearn_cardinal_touch_api"));auto* engine=get?get(MPC_CARDINAL_TOUCH_ABI):nullptr;
    if(!engine||engine->size!=sizeof(*engine)){dlclose(handle);return 0;}s->libraries.push_back({handle,engine,path});return 0;}
void cancelPlace(MpclearnToolkitExampleApp* app){auto* s=session(app);s->armed=-1;s->placingContact=false;s->shell.clearPreview();s->shell.placement(false);}
void filter(MpclearnToolkitExampleApp* app){auto* s=session(app);s->filtered.clear();for(unsigned i=0;i<s->catalog.size();++i)if((!s->category||s->catalog[i].control)&&mpc_editor::matches(s->catalog[i],s->query))s->filtered.push_back(i);
    mpc_editor::searchLabel(s->shell,s->query);
    if(!s->filtered.empty())mpc_ui_list_update(s->shell.palette,s->filtered.size(),0);
    mpc_ui_list_update(s->shell.palette,s->filtered.size(),UINT32_MAX);for(int i=0;i<2;++i)mpc_ui_button_selected(s->shell.category[i],s->category==(i==0));}
void catalog(MpclearnToolkitExampleApp* app){auto* s=session(app);unsigned count=api(app)->items(nullptr,0);if(count>4096){status(app,"Module catalog unavailable");return;}
    s->catalog.resize(count);api(app)->items(s->catalog.data(),count);
    s->catalogLabels.clear();for(auto& item:s->catalog){std::string name=item.name;auto split=name.find(" / ");
        s->catalogLabels.push_back(split==std::string::npos?name:name.substr(split+3)+"\n"+name.substr(0,split));}filter(app);}
void instances(MpclearnToolkitExampleApp* app){if(app->effect)return;auto* s=session(app);cancelPlace(app);keyboard(app,false);if(api(app))api(app)->close(s->token);
    s->selected=-1;s->token=0;s->revision=UINT32_MAX;s->rows.clear();dl_iterate_phdr(discover,s);
    for(unsigned library=0;library<s->libraries.size();++library){MpcCardinalInstance owners[64];unsigned count=s->libraries[library].api->instances(owners,64);
        for(unsigned i=0;i<count&&i<64;++i){Row row;row.label=owners[i].label;row.library=library;row.token=owners[i].token;s->rows.push_back(row);}}
    mpc_ui_list_update(app->list,s->rows.size(),UINT32_MAX);s->shell.show(mpc_editor::INSTANCE_LIST);status(app,"Select a live Cardinal instance (development entry)");}
void searchDone(void* owner,int accepted,const char* text){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);if(!accepted)return;
    auto* s=session(app);s->query=text;filter(app);status(app,s->filtered.empty()?"No matching items; tap Search to change":"Choose an item, then its place on the patch");}
void files(MpclearnToolkitExampleApp* app){auto* s=session(app);cancelPlace(app);keyboard(app,false);s->rows.clear();
    Row up;up.label="UP A FOLDER";up.directory=true;up.path=s->directory==libraryRoot?libraryRoot:s->directory.substr(0,s->directory.rfind('/'));s->rows.push_back(up);
    DIR* directory=opendir(s->directory.c_str());if(directory){unsigned visited=0;while(auto* entry=readdir(directory)){if(++visited>4096)break;if(entry->d_name[0]=='.')continue;
        Row row;row.path=s->directory+"/"+entry->d_name;struct stat st;if(lstat(row.path.c_str(),&st)||S_ISLNK(st.st_mode))continue;
        row.directory=S_ISDIR(st.st_mode);const char* dot=strrchr(entry->d_name,'.');if(!row.directory&&(!S_ISREG(st.st_mode)||!dot||strcmp(dot,".vcv")))continue;
        row.label=(row.directory?"[folder] ":"")+std::string(entry->d_name);s->rows.push_back(row);if(s->rows.size()==4096)break;}closedir(directory);}
    std::sort(s->rows.begin()+1,s->rows.end(),[](const Row& a,const Row& b){return a.directory!=b.directory?a.directory>b.directory:a.label<b.label;});
    mpc_ui_list_update(app->list,s->rows.size(),UINT32_MAX);s->shell.show(mpc_editor::PATCH_LIST);status(app,s->directory.c_str());}
const char* row(void* owner,uint32_t index){auto* s=session(static_cast<MpclearnToolkitExampleApp*>(owner));return index<s->rows.size()?s->rows[index].label.c_str():"";}
void selected(void* owner,uint32_t index){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);if(index>=s->rows.size())return;Row selected=s->rows[index];
    if(s->shell.page==mpc_editor::INSTANCE_LIST){s->selected=selected.library;s->token=selected.token;s->revision=UINT32_MAX;if(!api(app)->open(s->token,app->width,app->height)){s->selected=-1;s->token=0;status(app,"Cardinal instance unavailable");return;}catalog(app);}
    else if(selected.directory){s->directory=selected.path;files(app);return;}
    else if(!api(app)||!api(app)->action(s->token,MPC_CARDINAL_LOAD,selected.path.c_str())){
        char error[512]{};if(!api(app)||!api(app)->status(s->token,error,sizeof(error))||!*error)
            std::snprintf(error,sizeof(error),"Cardinal could not load this patch");
        status(app,error);return;
    }
    s->query.clear();s->category=0;filter(app);
    cancelPlace(app);s->shell.show(mpc_editor::ACTIONS);status(app,selected.label.c_str());refresh(app);}
const char* paletteRow(void* owner,uint32_t index){auto* s=session(static_cast<MpclearnToolkitExampleApp*>(owner));return index<s->filtered.size()?s->catalogLabels[s->filtered[index]].c_str():"";}
void paletteSelected(void* owner,uint32_t index){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);if(!api(app)||index>=s->filtered.size())return;
    api(app)->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);s->armed=s->filtered[index];s->placingContact=false;s->shell.placement(true);status(app,s->catalog[s->armed].detail);}
void place(MpclearnToolkitExampleApp* app,float x,float y){auto* s=session(app);if(s->armed<0||!api(app))return;unsigned id=s->catalog[s->armed].id;cancelPlace(app);
    if(api(app)->create(s->token,id,x,y)){s->shell.currentMode=MPC_EDITOR_EDIT;s->shell.panPad=false;s->shell.show(s->shell.page);status(app,"Module added");refresh(app);}else status(app,"Cardinal could not create this module");}
void paletteDrag(void* owner,uint32_t row,uint32_t event,float x,float y){
    auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);
    if(!s||!api(app))return;
    if(event==MPC_UI_LIST_DRAG_CANCEL){cancelPlace(app);status(app,"Placement cancelled");return;}
    if(event==MPC_UI_LIST_DRAG_BEGIN){
        mpc_ui_image_cancel(app->image);paletteSelected(owner,row);
        if(s->armed<0)return;
        s->shell.dragging=true;
    }
    if(!s->shell.dragging)return;
    const auto canvas=s->shell.canvas;
    if(event==MPC_UI_LIST_DRAG_DROP){
        if(x>=canvas.x+2&&y>=canvas.y+2&&x<canvas.x+canvas.width-2&&y<canvas.y+canvas.height-2)
            place(app,x-canvas.x,y-canvas.y);
        else {cancelPlace(app);status(app,"Placement cancelled");}
        return;
    }
    s->shell.preview(paletteRow(owner,row),x,y);
}
void pointer(void* owner,uint32_t event,float x,float y){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);if(!api(app))return;
    if(s->armed>=0){if(event==MPC_UI_POINTER_CANCEL||x<0||y<0||x>=app->width||y>=app->height){cancelPlace(app);return;}
        if(event==MPC_UI_POINTER_DOWN)s->placingContact=true;else if(event==MPC_UI_POINTER_UP&&s->placingContact)place(app,x,y);return;}
    api(app)->pointer(s->token,event,x,y);refresh(app);}
void contact(void* owner,uint32_t id,uint32_t event,float x,float y){
    auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);auto* engine=api(app);if(!engine)return;
    if(s->shell.contacts.event(id,event,x,y,app->width,app->height,
        [&](uint32_t e,float px,float py){pointer(owner,e,px,py);},
        [&](uint32_t intent,float px,float py,float amount){return engine->view(s->token,intent,px,py,amount)!=0;}))refresh(app);
}
void action(void* owner,uint32_t intent){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);auto* engine=api(app);using namespace mpc_editor;
    if(s->hostValues.binding)hostControlsClose(app);
    if(s->shell.palette)mpc_ui_list_cancel_drag(s->shell.palette);
    if(app->image)mpc_ui_image_cancel(app->image);
    if(engine)engine->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);
    if(intent==BACK){
#ifdef MPCLEARN_CARDINAL_GPU
        cardinal_presenter_ui(30,uintptr_t(app),0);
#endif
        cancelPlace(app);if(mpc_ui_app_request_close(app->host))app->callback(app->owner,MPCLEARN_TOOLKIT_EXAMPLE_BACK);return;}
    if(intent>=2000){if(engine)engine->text(s->token,intent-2000);refresh(app);return;}
    if(intent==PANEL_BACK){cancelPlace(app);keyboard(app,false);if(engine)engine->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);s->shell.show(ACTIONS);return;}
    if(intent==20){instances(app);return;}if(!engine)return;
    if(intent==33){cancelPlace(app);keyboard(app,false);hostControlsOpen(app);return;}
    if(intent==PATCHES){files(app);return;}if(intent==ADD){keyboard(app,false);engine->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);s->shell.show(PALETTE);return;}
    if(intent==TOOLS){cancelPlace(app);keyboard(app,false);engine->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);s->shell.show(TOOL_LIST);return;}
    if(intent==CATEGORY_CONTROLS||intent==CATEGORY_OBJECTS){cancelPlace(app);s->category=intent==CATEGORY_CONTROLS;filter(app);return;}
    if(intent==SEARCH){cancelPlace(app);engine->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);
        if(!mpc_ui_text_open(app->screen,"Search",s->query.c_str(),searchDone,app))status(app,"Keyboard unavailable");
        return;}
    if(intent==CANCEL_PLACE){cancelPlace(app);status(app,"Placement cancelled");return;}if(intent==PLACE_CENTRE){place(app,app->width/2.f,app->height/2.f);return;}
    if(intent==23){keyboard(app,!s->keyboard);return;}
    if(intent>=CONTROLS&&intent<=PAN){s->shell.currentMode=intent-CONTROLS;s->shell.panPad=intent==PAN;engine->view(s->token,MPC_EDITOR_VIEW_MODE,0,0,s->shell.currentMode);s->shell.show(s->shell.page);}
    else if(intent==ZOOM_OUT||intent==ZOOM_IN)engine->view(s->token,MPC_EDITOR_VIEW_ZOOM_SELECTION,app->width/2.f,app->height/2.f,intent==ZOOM_IN?1.25f:.8f);
    else if(intent>=LEFT&&intent<=DOWN)engine->view(s->token,MPC_EDITOR_VIEW_PAN,intent==LEFT?-app->width*.4f:intent==RIGHT?app->width*.4f:0,intent==UP?-app->height*.4f:intent==DOWN?app->height*.4f:0,0);
    else if(intent==FIT)engine->view(s->token,MPC_EDITOR_VIEW_FIT,0,0,0);
    else if(intent==1000)engine->view(s->token,MPC_EDITOR_VIEW_FOCUS,0,0,0);
    else if(intent==UNDO)engine->action(s->token,MPC_CARDINAL_UNDO,nullptr);
    else if(intent==30||intent==31)engine->scroll(s->token,app->width/2.f,app->height/2.f,0,intent==30?100:-100);
    else if(intent==32){if(engine->action(s->token,MPC_CARDINAL_LOAD,"/tmp/mpclearn-cardinal-touch/state/prototype.vcv")){s->query.clear();s->category=0;filter(app);}}
    else if(!engine->action(s->token,intent,nullptr)){
        char error[512]{};if(!engine->status(s->token,error,sizeof(error))||!*error)
            std::snprintf(error,sizeof(error),"Cardinal operation unavailable");
        status(app,error);return;
    }
    if(intent==MPC_CARDINAL_MODULE_OPTIONS)s->shell.show(ACTIONS);
    refresh(app);}
const char* toolRow(void*,uint32_t index){return index<sizeof(tools)/sizeof(*tools)?tools[index].name:"";}
void toolSelected(void* owner,uint32_t index){if(index<sizeof(tools)/sizeof(*tools))action(owner,tools[index].intent);}
int open(void* owner,MpcUiAppHost* host,MpcUiContext* context,MpcUiParent* parent,MpcUiRect bounds){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);app->host=host;app->session=new Session;
    auto* s=session(app);app->screen=mpc_ui_screen_create(context,parent,bounds);if(!app->screen)return 0;
    if(!s->shell.create(app->screen,bounds,"CARDINAL",pointer,action,paletteRow,paletteSelected,row,selected,toolRow,toolSelected,app,app->effect?32:8))return 0;
    app->width=s->shell.canvas.width;app->height=s->shell.canvas.height;s->pixels.resize(app->width*app->height*4);app->image=s->shell.image;app->status=s->shell.status;app->list=s->shell.files;
    if(!mpc_ui_image_contacts(app->image,contact)||!mpc_ui_image_on_refresh(app->image,frame))return 0;
    if(!mpc_ui_list_drag_out(s->shell.palette,paletteDrag)||!hostControlsCreate(app))return 0;
    mpc_ui_button_text(s->shell.category[1],"MODULES");mpc_ui_list_update(s->shell.tools,sizeof(tools)/sizeof(*tools)-(app->effect?1:0),UINT32_MAX);
    if(!mpc_ui_list_text_layout(s->shell.palette,18,2))return 0;
    const char* keys="1234567890qwertyuiopasdfghjkl zxcvbnm.";
    for(int i=0;i<40;++i){char label[8]={};uint32_t value;if(i==38){strcpy(label,"DEL");value=8;}else if(i==39){strcpy(label,"ENTER");value=13;}else{label[0]=keys[i];value=keys[i];}
        auto* key=mpc_ui_button_create(app->screen,label,2000+value,action,app);app->keys[i]=key;
        if(!key||!mpc_ui_button_style(key,{0xff2f2f34,0xffd31145,0xffe1e2e5})||!mpc_ui_control_place(key,{s->shell.canvas.x+(i%10)*(int)app->width/10,s->shell.canvas.y+(i/10)*90,(int)app->width/10-6,82})||!mpc_ui_control_visible(key,0))return 0;}
    if(app->effect){void* handle=dlopen(app->modulePath,RTLD_NOW|RTLD_NOLOAD);if(!handle)return 0;
        auto get=reinterpret_cast<const MpcCardinalTouchApi*(*)(uint32_t)>(dlsym(handle,"mpclearn_cardinal_touch_api"));auto* engine=get?get(MPC_CARDINAL_TOUCH_ABI):nullptr;
        uint64_t token=engine&&engine->size==sizeof(*engine)&&engine->bind?engine->bind(app->effect):0;if(!token){dlclose(handle);return 0;}
        s->libraries.push_back({handle,engine,app->modulePath});s->selected=0;s->token=token;if(!engine->open(token,app->width,app->height))return 0;catalog(app);s->shell.show(mpc_editor::ACTIONS);refresh(app);
    }else instances(app);
#ifdef MPCLEARN_CARDINAL_GPU
    if(app->effect)s->overlayGeneration=cardinal_presenter_open(uintptr_t(app->image));
#endif
    int shown=mpc_ui_screen_show(app->screen);
#ifdef MPCLEARN_CARDINAL_GPU
    cardinal_presenter_ui(31,uintptr_t(app),shown);
#endif
    return shown;}
int prepare(void*,int){return MPC_UI_APP_CLOSE_READY;}
int close(void* owner){
#ifdef MPCLEARN_CARDINAL_GPU
    cardinal_presenter_ui(34,uintptr_t(owner),0);
#endif
    auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);if(app->screen&&!mpc_ui_screen_hide(app->screen))return 0;delete session(app);app->session=nullptr;
    if(app->screen&&!mpc_ui_screen_close(&app->screen))return 0;
    app->host=nullptr;app->image=app->status=nullptr;app->list=nullptr;memset(app->keys,0,sizeof(app->keys));return 1;}
const MpcUiAppCallbacks callbacks={sizeof(MpcUiAppCallbacks),open,prepare,close};
}
extern "C" int mpclearn_toolkit_example_app_init(MpclearnToolkitExampleApp* app,double,MpcUiIntentCallback callback,MpcUiSliderCallback,void* owner){if(!app||app->host||app->screen||!callback)return 0;app->callback=callback;app->owner=owner;return 1;}
extern "C" const MpcUiAppCallbacks* mpclearn_toolkit_example_app_callbacks(){return &callbacks;}
extern "C" int mpclearn_toolkit_example_set_title(MpclearnToolkitExampleApp* app,const char* text){return mpc_ui_label_text(app->status,text);}
extern "C" int mpclearn_toolkit_example_set_value(MpclearnToolkitExampleApp*,double){return 1;}
extern "C" int mpclearn_toolkit_example_bind(MpclearnToolkitExampleApp* app,const void* effect,const char* path){if(!app||app->host||app->session||!effect||!path||strlen(path)>=sizeof(app->modulePath))return 0;app->effect=effect;snprintf(app->modulePath,sizeof(app->modulePath),"%s",path);return 1;}
