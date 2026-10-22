/* Native presentation of one borrowed MPC Pd owner. The same frame can be
 * attached by the track editor; the app entry remains a diagnostic selector. */
#include "toolkit-example.h"
#include "hosted-api.h"
#include "patch-files.h"
#include "editor-shell.h"
#include <dlfcn.h>
#include <link.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
namespace {
struct Library {void* handle;const MpcPdHostedApi* api;std::string path;};
struct Row {unsigned library;uint64_t token;std::string label;};
struct Tool {const char* name;uint32_t intent;};
const Tool tools[]={{"NEW OBJECT",21},{"NEW MESSAGE",24},{"NEW COMMENT",25},{"EDIT TEXT",23},{"OPEN SUBPATCH",7},{"PARENT PATCH",8},
    {"FOCUS SELECTION",1000},{"UNWIRE",3},{"DELETE",4},{"SAVE COPY",5},{"RELOAD ORIGINAL",6},{"LOAD COPY",14},{"INSTANCES (DEV)",30}};
struct Session {
    std::vector<Library> libraries;std::vector<Row> owners;std::vector<unsigned char> pixels;
    std::vector<MpcEditorItem> catalog;std::vector<unsigned> filtered;
    mpc_editor::Shell shell;PdFiles files{};
    int selected=-1,category=1,armed=-1;bool placingContact=false,textOpen=false;
    std::string query;
    uint64_t token=0;uint32_t revision=0,paintRevision=0;
    ~Session(){if(selected>=0)libraries[selected].api->close(token);for(auto& item:libraries)dlclose(item.handle);}
};
Session* session(MpclearnToolkitExampleApp* app){return static_cast<Session*>(app->session);}
const MpcPdHostedApi* api(MpclearnToolkitExampleApp* app){auto* s=session(app);return s&&s->selected>=0?s->libraries[s->selected].api:nullptr;}
void status(MpclearnToolkitExampleApp* app,const char* text){mpc_ui_label_text(app->status,text);}
void editText(MpclearnToolkitExampleApp* app);
void frame(void* context){auto* app=static_cast<MpclearnToolkitExampleApp*>(context);auto* s=session(app);auto* owner=api(app);if(!owner)return;
    uint32_t revision=s->revision;char text[512];
    if(!owner->pixels(s->token,s->pixels.data(),s->pixels.size(),&revision)){status(app,"Pd instance closed");return;}
    if(revision!=s->revision&&!mpc_ui_image_update(app->image,s->pixels.data(),app->width*4,s->pixels.size(),++s->paintRevision)){status(app,"Image update unavailable");return;}
    s->revision=revision;if(s->armed<0&&owner->status(s->token,text,sizeof(text)))status(app,text);}
void refresh(MpclearnToolkitExampleApp* app){auto* s=session(app);auto* owner=api(app);if(!owner)return;
    editText(app);
    MpcEditorViewport view{};if(owner->viewport(s->token,&view)&&view.mode!=s->shell.currentMode){s->shell.currentMode=view.mode;s->shell.panPad=view.mode==MPC_EDITOR_PAN;s->shell.show(s->shell.page);}
    mpc_ui_image_invalidate(app->image);}

void textDone(void* context,int accepted,const char* text){auto* app=static_cast<MpclearnToolkitExampleApp*>(context);auto* s=session(app);s->textOpen=false;
    if(api(app)){api(app)->text_finish(s->token,accepted,text);}
    refresh(app);}
void editText(MpclearnToolkitExampleApp* app){auto* s=session(app);if(s->textOpen||!api(app))return;char title[64],initial[512];
    if(!api(app)->text_begin(s->token,title,sizeof(title),initial,sizeof(initial)))return;
    s->textOpen=true;if(!mpc_ui_text_open(app->screen,title,initial,textDone,app)){s->textOpen=false;api(app)->text_finish(s->token,0,"");status(app,"Keyboard unavailable");}}
int discover(dl_phdr_info* info,size_t,void* context){auto* s=static_cast<Session*>(context);const char* path=info->dlpi_name;if(!path||!*path)return 0;
    const char* name=strrchr(path,'/');name=name?name+1:path;if(strcmp(name,"pd-sketch-synth.so")&&strcmp(name,"pd-sketch-fx.so"))return 0;
    for(auto& item:s->libraries)if(item.path==path)return 0;
    void* handle=dlopen(path,RTLD_NOW|RTLD_NOLOAD);if(!handle)return 0;
    auto get=reinterpret_cast<const MpcPdHostedApi*(*)(uint32_t)>(dlsym(handle,"mpclearn_pd_hosted_api"));const auto* owner=get?get(MPC_PD_HOSTED_ABI):nullptr;if(!owner&&get)owner=get(5);
    if(!owner||owner->size!=sizeof(MpcPdHostedApi)){dlclose(handle);return 0;}s->libraries.push_back({handle,owner,path});return 0;}
void cancelPlace(MpclearnToolkitExampleApp* app){auto* s=session(app);s->armed=-1;s->placingContact=false;s->shell.clearPreview();s->shell.placement(false);}
void instances(MpclearnToolkitExampleApp* app){if(app->effect)return;auto* s=session(app);cancelPlace(app);if(api(app))api(app)->close(s->token);s->selected=-1;s->token=0;s->revision=0;s->owners.clear();dl_iterate_phdr(discover,s);
    for(unsigned library=0;library<s->libraries.size();++library){MpcPdInstance rows[64];uint32_t count=s->libraries[library].api->instances(rows,64);
        for(uint32_t i=0;i<count&&i<64;++i)s->owners.push_back({library,rows[i].token,rows[i].label});}
    mpc_ui_list_update(app->list,s->owners.size(),UINT32_MAX);s->shell.show(mpc_editor::INSTANCE_LIST);
    status(app,s->owners.empty()?"Load Pd Sketch Synth or FX in MPC":"Select a live Pd instance (development entry)");}
void filter(MpclearnToolkitExampleApp* app){auto* s=session(app);s->filtered.clear();for(unsigned i=0;i<s->catalog.size();++i)if(s->catalog[i].control==(unsigned)s->category&&mpc_editor::matches(s->catalog[i],s->query))s->filtered.push_back(i);
    mpc_editor::searchLabel(s->shell,s->query);
    if(!s->filtered.empty())mpc_ui_list_update(s->shell.palette,s->filtered.size(),0);
    mpc_ui_list_update(s->shell.palette,s->filtered.size(),UINT32_MAX);for(int i=0;i<2;++i)mpc_ui_button_selected(s->shell.category[i],s->category==(i==0));}
void searchDone(void* owner,int accepted,const char* text){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);if(!accepted)return;
    auto* s=session(app);s->query=text;filter(app);status(app,s->filtered.empty()?"No matching items; tap Search to change":"Choose an item, then its place on the patch");}
void files(MpclearnToolkitExampleApp* app){auto* s=session(app);cancelPlace(app);if(!s->files.path[0])pd_files_open(&s->files,pd_files_root());
    mpc_ui_list_update(app->list,s->files.count+1,UINT32_MAX);s->shell.show(mpc_editor::PATCH_LIST);status(app,s->files.message[0]?s->files.message:s->files.path);}
const char* row(void* owner,uint32_t index){auto* s=session(static_cast<MpclearnToolkitExampleApp*>(owner));
    if(s->shell.page==mpc_editor::INSTANCE_LIST)return index<s->owners.size()?s->owners[index].label.c_str():"";
    if(index==0)return "UP A FOLDER";
    return index-1<(uint32_t)s->files.count?s->files.entries[index-1].name:"";}
void selected(void* owner,uint32_t index){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);
    if(s->shell.page==mpc_editor::INSTANCE_LIST){if(index>=s->owners.size())return;const Row choice=s->owners[index];s->selected=choice.library;s->token=choice.token;s->revision=0;
        if(!api(app)->open(s->token,app->width,app->height)){s->selected=-1;s->token=0;status(app,"Pd instance unavailable");return;}
        const unsigned count=api(app)->catalog(nullptr,0);if(count>4096){status(app,"Pd catalog unavailable");return;}
        s->catalog.resize(count);api(app)->catalog(s->catalog.data(),count);filter(app);
    }else{if(index==0){pd_files_parent(&s->files);files(app);return;}--index;char path[PD_PATH_MAX];if(!pd_files_child_path(&s->files,index,path,sizeof(path)))return;
        if(s->files.entries[index].directory){if(pd_files_open(&s->files,path))files(app);return;}
        if(!api(app)||!api(app)->load(s->token,path)){status(app,"Pd could not load this sketch");return;}}
    s->query.clear();s->category=1;filter(app);
    cancelPlace(app);s->shell.show(mpc_editor::ACTIONS);refresh(app);}
const char* paletteRow(void* owner,uint32_t index){auto* s=session(static_cast<MpclearnToolkitExampleApp*>(owner));return index<s->filtered.size()?s->catalog[s->filtered[index]].name:"";}
void paletteSelected(void* owner,uint32_t index){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);if(!api(app)||index>=s->filtered.size())return;
    api(app)->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);s->armed=s->filtered[index];s->placingContact=false;s->shell.placement(true);
    status(app,s->catalog[s->armed].detail);}
void place(MpclearnToolkitExampleApp* app,float x,float y){auto* s=session(app);if(s->armed<0||!api(app))return;
    unsigned id=s->catalog[s->armed].id;cancelPlace(app);
    if(api(app)->create(s->token,id,x,y)){s->shell.currentMode=MPC_EDITOR_EDIT;s->shell.panPad=false;s->shell.show(s->shell.page);refresh(app);}else status(app,"Pd could not create this object");}
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
        if(event==MPC_UI_POINTER_DOWN)s->placingContact=true;
        else if(event==MPC_UI_POINTER_UP&&s->placingContact)place(app,x,y);
        return;}
    api(app)->pointer(s->token,event,x,y);refresh(app);}
void contact(void* owner,uint32_t id,uint32_t event,float x,float y){
    auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);auto* engine=api(app);if(!engine)return;
    if(s->shell.contacts.event(id,event,x,y,app->width,app->height,
        [&](uint32_t e,float px,float py){pointer(owner,e,px,py);},
        [&](uint32_t intent,float px,float py,float amount){return engine->view(s->token,intent,px,py,amount)!=0;}))refresh(app);
}
void action(void* owner,uint32_t intent){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);auto* s=session(app);auto* engine=api(app);using namespace mpc_editor;
    if(s->shell.palette)mpc_ui_list_cancel_drag(s->shell.palette);
    if(app->image)mpc_ui_image_cancel(app->image);
    if(engine)engine->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);
    if(intent==BACK){cancelPlace(app);if(mpc_ui_app_request_close(app->host))app->callback(app->owner,MPCLEARN_TOOLKIT_EXAMPLE_BACK);return;}
    if(intent==PANEL_BACK){cancelPlace(app);if(engine)engine->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);s->shell.show(ACTIONS);return;}
    if(intent==30){instances(app);return;}if(!engine)return;
    if(intent==PATCHES){files(app);return;}if(intent==ADD){engine->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);s->shell.show(PALETTE);return;}
    if(intent==TOOLS){cancelPlace(app);engine->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);s->shell.show(TOOL_LIST);return;}
    if(intent==CATEGORY_CONTROLS||intent==CATEGORY_OBJECTS){cancelPlace(app);s->category=intent==CATEGORY_CONTROLS;filter(app);return;}
    if(intent==SEARCH){cancelPlace(app);engine->pointer(s->token,MPC_UI_POINTER_CANCEL,0,0);
        if(!mpc_ui_text_open(app->screen,"Search",s->query.c_str(),searchDone,app))status(app,"Keyboard unavailable");
        return;}
    if(intent==CANCEL_PLACE){cancelPlace(app);status(app,"Placement cancelled");return;}if(intent==PLACE_CENTRE){place(app,app->width/2.f,app->height/2.f);return;}
    if(intent>=CONTROLS&&intent<=PAN){s->shell.currentMode=intent-CONTROLS;s->shell.panPad=intent==PAN;engine->view(s->token,MPC_EDITOR_VIEW_MODE,0,0,s->shell.currentMode);s->shell.show(s->shell.page);}
    else if(intent==ZOOM_OUT||intent==ZOOM_IN)engine->view(s->token,MPC_EDITOR_VIEW_ZOOM_SELECTION,app->width/2.f,app->height/2.f,intent==ZOOM_IN?1.25f:.8f);
    else if(intent>=LEFT&&intent<=DOWN)engine->view(s->token,MPC_EDITOR_VIEW_PAN,intent==LEFT?-app->width*.4f:intent==RIGHT?app->width*.4f:0,intent==UP?-app->height*.4f:intent==DOWN?app->height*.4f:0,0);
    else if(intent==FIT)engine->view(s->token,MPC_EDITOR_VIEW_FIT,0,0,0);
    else if(intent==1000||intent==UNDO)engine->view(s->token,MPC_EDITOR_VIEW_FOCUS,0,0,0);
    else if(intent==8&&s->shell.page==PATCH_LIST){pd_files_parent(&s->files);files(app);return;}
    else {engine->action(s->token,intent);if(intent==6||intent==14){s->query.clear();s->category=1;filter(app);}}
    refresh(app);}
const char* toolRow(void*,uint32_t index){return index<sizeof(tools)/sizeof(*tools)?tools[index].name:"";}
void toolSelected(void* owner,uint32_t index){if(index<sizeof(tools)/sizeof(*tools))action(owner,tools[index].intent);}
int open(void* owner,MpcUiAppHost* host,MpcUiContext* context,MpcUiParent* parent,MpcUiRect bounds){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);
    app->host=host;app->session=new Session;auto* s=session(app);app->screen=mpc_ui_screen_create(context,parent,bounds);if(!app->screen)return 0;
    if(!s->shell.create(app->screen,bounds,"PD SKETCH",pointer,action,paletteRow,paletteSelected,row,selected,toolRow,toolSelected,app))return 0;
    app->width=s->shell.canvas.width;app->height=s->shell.canvas.height;s->pixels.resize(app->width*app->height*4);
    app->status=s->shell.status;app->image=s->shell.image;app->list=s->shell.files;
    if(!mpc_ui_image_contacts(app->image,contact)||!mpc_ui_image_on_refresh(app->image,frame))return 0;
    if(!mpc_ui_list_drag_out(s->shell.palette,paletteDrag))return 0;
    // Pd's pinned structural adapter does not yet expose canonical Undo.
    mpc_ui_button_text(s->shell.undo,"FOCUS");
    mpc_ui_list_update(s->shell.tools,sizeof(tools)/sizeof(*tools)-(app->effect?1:0),UINT32_MAX);
    if(app->effect){
        void* handle=dlopen(app->modulePath,RTLD_NOW|RTLD_NOLOAD);if(!handle)return 0;
        auto get=reinterpret_cast<const MpcPdHostedApi*(*)(uint32_t)>(dlsym(handle,"mpclearn_pd_hosted_api"));
        const auto* engine=get?get(MPC_PD_HOSTED_ABI):nullptr;if(!engine&&get)engine=get(5);
        uint64_t token=engine&&engine->size==sizeof(*engine)&&engine->bind?engine->bind(app->effect):0;
        if(!token){dlclose(handle);return 0;}
        s->libraries.push_back({handle,engine,app->modulePath});s->selected=0;s->token=token;
        if(!engine->open(token,app->width,app->height))return 0;
        unsigned count=engine->catalog(nullptr,0);if(count>4096)return 0;
        s->catalog.resize(count);engine->catalog(s->catalog.data(),count);filter(app);
        s->shell.show(mpc_editor::ACTIONS);refresh(app);
    }else instances(app);
    return mpc_ui_screen_show(app->screen);}
int prepare(void*,int){return MPC_UI_APP_CLOSE_READY;}
int close(void* owner){auto* app=static_cast<MpclearnToolkitExampleApp*>(owner);if(app->screen&&!mpc_ui_screen_hide(app->screen))return 0;
    delete session(app);app->session=nullptr;if(app->screen&&!mpc_ui_screen_close(&app->screen))return 0;app->host=nullptr;app->image=app->status=nullptr;app->list=nullptr;return 1;}
const MpcUiAppCallbacks callbacks={sizeof(MpcUiAppCallbacks),open,prepare,close};
}
extern "C" int mpclearn_toolkit_example_app_init(MpclearnToolkitExampleApp* app,double,MpcUiIntentCallback callback,MpcUiSliderCallback,void* owner){if(!app||app->host||app->screen||!callback)return 0;app->callback=callback;app->owner=owner;return 1;}
extern "C" const MpcUiAppCallbacks* mpclearn_toolkit_example_app_callbacks(){return &callbacks;}
extern "C" int mpclearn_toolkit_example_set_title(MpclearnToolkitExampleApp* app,const char* text){return mpc_ui_label_text(app->status,text);}
extern "C" int mpclearn_toolkit_example_set_value(MpclearnToolkitExampleApp*,double){return 1;}
extern "C" int mpclearn_toolkit_example_bind(MpclearnToolkitExampleApp* app,const void* effect,const char* path){
    if(!app||app->host||app->session||!effect||!path||std::strlen(path)>=sizeof(app->modulePath))return 0;
    app->effect=effect;std::snprintf(app->modulePath,sizeof(app->modulePath),"%s",path);return 1;}
