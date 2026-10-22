#ifndef MPCLEARN_EDITOR_SHELL_H
#define MPCLEARN_EDITOR_SHELL_H
#include "../include/mpclearn-ui.h"
#include "editor-view.h"
#include <vector>
#include <string>
#include <cctype>
#include <cmath>
#include <algorithm>

/* One fixed canvas and one fixed side panel. Page changes only show/hide
 * native controls; they never resize or reopen an engine view. */
namespace mpc_editor {
enum Intent {
    BACK=200, PATCHES, ADD, TOOLS, PANEL_BACK, CONTROLS, EDIT, PAN,
    ZOOM_OUT, ZOOM_IN, FIT, UNDO, LEFT, RIGHT, UP, DOWN,
    CATEGORY_CONTROLS, CATEGORY_OBJECTS, PLACE_CENTRE, CANCEL_PLACE, SEARCH
};
enum Page { ACTIONS, PALETTE, PATCH_LIST, TOOL_LIST, INSTANCE_LIST, HOST_PARAMETERS };
/* Classification only: the existing engine owns every control, edit and
 * viewport. A second canvas contact cancels uncommitted single-finger intent.
 * Navigation keeps ownership until all fingers lift, including edge exit. */
struct Contacts {
    struct Point { uint32_t id=0; bool active=false; float x=0,y=0; } points[10];
    bool navigation=false,blocked=false;
    float midpointX=0,midpointY=0,distance=0;
    unsigned count() const { unsigned n=0;for(auto& p:points)if(p.active)++n;return n; }
    void measure(float& x,float& y,float& d) const {
        const Point *a=nullptr,*b=nullptr;
        for(auto& p:points)if(p.active){if(!a)a=&p;else if(!b)b=&p;}
        x=(a->x+b->x)*.5f;y=(a->y+b->y)*.5f;
        d=std::hypot(a->x-b->x,a->y-b->y);
    }
    template<class Pointer,class View>
    bool event(uint32_t id,uint32_t event,float x,float y,int width,int height,
               Pointer pointer,View view) {
        Point* point=nullptr;
        for(auto& p:points)if(p.active&&p.id==id){point=&p;break;}
        if(event==MPC_UI_POINTER_DOWN) {
            if(point)return false;
            for(auto& p:points)if(!p.active){point=&p;break;}
            if(!point)return false;
            *point={id,true,x,y};
        } else if(!point)return false;
        point->x=x;point->y=y;
        const unsigned contacts=count();
        const bool edge=x<2||y<2||x>=width-2||y>=height-2;
        bool changed=false;
        if(event==MPC_UI_POINTER_CANCEL||edge) {
            if(!blocked&&!navigation)pointer(MPC_UI_POINTER_CANCEL,x,y);
            blocked=true;
        } else if(event==MPC_UI_POINTER_DOWN&&contacts>=2) {
            if(!navigation&&!blocked)pointer(MPC_UI_POINTER_CANCEL,x,y);
            if(!navigation&&!blocked)
                changed=view(MPC_EDITOR_VIEW_NAVIGATION_BEGIN,0,0,0)||changed;
            navigation=true;
            if(contacts==2)measure(midpointX,midpointY,distance);
            else blocked=true;
        } else if(!blocked) {
            if(navigation) {
                if(contacts==2&&event==MPC_UI_POINTER_MOVE) {
                    float nx,ny,nd;measure(nx,ny,nd);
                    if(distance>=8&&nd>=8&&nd!=distance)
                        changed=view(MPC_EDITOR_VIEW_ZOOM,midpointX,midpointY,nd/distance)||changed;
                    if(nx!=midpointX||ny!=midpointY)
                        changed=view(MPC_EDITOR_VIEW_PAN,midpointX-nx,midpointY-ny,0)||changed;
                    midpointX=nx;midpointY=ny;distance=nd;
                }
            } else pointer(event,x,y);
        }
        if(event==MPC_UI_POINTER_UP||event==MPC_UI_POINTER_CANCEL)point->active=false;
        if(!count()){
            if(navigation)changed=view(MPC_EDITOR_VIEW_NAVIGATION_END,0,0,0)||changed;
            navigation=false;blocked=false;distance=0;
        }
        return changed;
    }
};
struct Button { MpcUiControl* control; unsigned pages; };
struct Shell {
    MpcUiControl *title=nullptr,*status=nullptr,*image=nullptr,*panel=nullptr;
    MpcUiControl *ghost=nullptr;
    MpcUiControl *mode[3]{},*back=nullptr,*detail=nullptr,*place=nullptr,*cancel=nullptr,*undo=nullptr,*search=nullptr;
    MpcUiControl *category[2]{};
    MpcUiList *palette=nullptr,*files=nullptr,*tools=nullptr;
    std::vector<Button> buttons;
    Contacts contacts;
    MpcUiRect canvas{},rail{},rows{},frame{};
    Page page=ACTIONS;
    unsigned currentMode=MPC_EDITOR_CONTROLS;
    bool panPad=false,placing=false,dragging=false;
    void clearPreview() {dragging=false;if(ghost)mpc_ui_control_visible(ghost,0);}
    void preview(const char* name,float x,float y) {
        if(!ghost)return;
        const int left=std::max(0,std::min(frame.width-240,int(x)-120));
        const int top=std::max(0,std::min(frame.height-52,int(y)-72));
        mpc_ui_label_text(ghost,name);
        mpc_ui_control_place(ghost,{left,top,240,52});
        mpc_ui_control_keep_in_front(ghost);
        mpc_ui_control_visible(ghost,1);
    }
    static unsigned bit(Page value){return 1u<<value;}
    void show(Page value) {
        page=value;
        for(auto& b:buttons)mpc_ui_control_visible(b.control,(b.pages&bit(page))!=0);
        mpc_ui_list_visible(palette,page==PALETTE);
        mpc_ui_list_visible(files,page==PATCH_LIST||page==INSTANCE_LIST);
        mpc_ui_list_visible(tools,page==TOOL_LIST);
        mpc_ui_control_visible(back,page!=ACTIONS);
        mpc_ui_control_visible(detail,false);
        mpc_ui_control_visible(search,page==PALETTE&&!placing);
        mpc_ui_control_visible(place,page==PALETTE&&placing);
        mpc_ui_control_visible(cancel,page==PALETTE&&placing);
        for(auto* b:category)mpc_ui_control_visible(b,page==PALETTE);
        updateMode();
    }
    void updateMode() {
        for(unsigned i=0;i<3;++i)mpc_ui_button_selected(mode[i],currentMode==i);
        // The pad replaces the three action rows inside the same reserved area.
        for(auto& b:buttons)if(b.pages==0x80000000u)mpc_ui_control_visible(b.control,page==ACTIONS&&panPad);
    }
    void placement(bool active) {placing=active;show(page);}
    bool create(MpcUiScreen* screen,MpcUiRect bounds,const char* heading,
                MpcUiPointerCallback pointer,MpcUiIntentCallback intent,
                MpcUiListRowText paletteText,MpcUiListSelectionCallback paletteSelect,
                MpcUiListRowText fileText,MpcUiListSelectionCallback fileSelect,
                MpcUiListRowText toolText,MpcUiListSelectionCallback toolSelect,void* owner,int canvasLeft=8) {
        if(bounds.width<920||bounds.height<580)return false;
        frame=bounds;
        const int width=224;
        canvas={canvasLeft,48,bounds.width-width-16-canvasLeft,bounds.height-88};
        rail={bounds.width-width-8,8,width,bounds.height-16};
        const int bottom=rail.y+rail.height-244;
        // Choosers use the whole rail; only Add reserves its category and search rows.
        rows={rail.x,rail.y+64,width,rail.height-64};
        const MpcUiRect addRows={rail.x,rail.y+128,width,rail.height-192};
        const int chooserBottom=rail.y+rail.height-56;
        const MpcUiButtonStyle style={0xff2f2f34,0xffd31145,0xffe1e2e5};
        const MpcUiLabelStyle label={0xff202126,0xffe1e2e5,33};
        // Cover the borrowed content area, including the gaps between controls.
        // The native page can retain its earlier paint beneath this view.
        auto* background=mpc_ui_label_create(screen," ");
        if(!background||!mpc_ui_label_style(background,label)||
           !mpc_ui_control_place(background,{0,0,bounds.width,bounds.height}))return false;
        title=mpc_ui_label_create(screen,heading);
        status=mpc_ui_label_create(screen,"Ready");
        panel=mpc_ui_label_create(screen," ");
        detail=mpc_ui_label_create(screen,"Tap an item, then its place on the patch");
        image=mpc_ui_image_create(screen,canvas.width,canvas.height,pointer,owner);
        if(!title||!status||!panel||!detail||!image)return false;
        if(!mpc_ui_label_style(title,label)||!mpc_ui_label_style(status,label)||
           !mpc_ui_label_style(panel,label)||!mpc_ui_label_style(detail,label)||
           !mpc_ui_control_place(title,{8,4,canvas.width,40})||
           !mpc_ui_control_place(status,{8,bounds.height-36,canvas.width,32})||
           !mpc_ui_control_place(panel,rail)||!mpc_ui_control_place(image,canvas)||
           !mpc_ui_control_place(detail,{rail.x,chooserBottom,width,52}))return false;
        auto button=[&](const char* name,unsigned value,MpcUiRect box,unsigned pages)->MpcUiControl* {
            auto* b=mpc_ui_button_create(screen,name,value,intent,owner);
            if(!b||!mpc_ui_button_style(b,style)||!mpc_ui_control_place(b,box)||
               (box.width<150&&!mpc_ui_button_text_size(b,18)))return nullptr;
            buttons.push_back({b,pages});return b;
        };
        const char* mainNames[]={"BACK","PATCHES","ADD","TOOLS"};
        for(int i=0;i<4;++i)if(!button(mainNames[i],BACK+i,{rail.x,rail.y+i*64,width,56},bit(ACTIONS)))return false;
        back=button("BACK",PANEL_BACK,{rail.x,rail.y,width,56},0);
        category[0]=button("CONTROLS",CATEGORY_CONTROLS,{rail.x,rail.y+64,120,56},0);
        category[1]=button("OBJECTS",CATEGORY_OBJECTS,{rail.x+128,rail.y+64,96,56},0);
        place=button("PLACE",PLACE_CENTRE,{rail.x,chooserBottom,108,52},0);
        cancel=button("CANCEL",CANCEL_PLACE,{rail.x+116,chooserBottom,108,52},0);
        search=button("SEARCH",SEARCH,{rail.x,chooserBottom,width,52},0);
        const unsigned all=bit(ACTIONS);
        mode[0]=button("CONTROLS",CONTROLS,{rail.x,bottom,120,56},all);
        mode[1]=button("EDIT",EDIT,{rail.x+128,bottom,96,56},all);
        mode[2]=button("PAN",PAN,{rail.x,bottom+64,68,56},all);
        undo=button("UNDO",UNDO,{rail.x+116,bottom+128,108,56},all);
        if(!button("-",ZOOM_OUT,{rail.x+76,bottom+64,68,56},all)||
           !button("+",ZOOM_IN,{rail.x+152,bottom+64,72,56},all)||
           !button("FIT",FIT,{rail.x,bottom+128,108,56},all)||!undo)return false;
        const char* arrows[]={"<",">","^","v"};
        for(int i=0;i<4;++i)if(!button(arrows[i],LEFT+i,{rail.x+i*56,bottom+192,56,52},0x80000000u))return false;
        palette=mpc_ui_list_create(screen,"ADD",0,56,paletteText,paletteSelect,owner);
        files=mpc_ui_list_create(screen,"PATCHES",0,56,fileText,fileSelect,owner);
        tools=mpc_ui_list_create(screen,"TOOLS",0,56,toolText,toolSelect,owner);
        if(!back||!place||!cancel||!search||!category[0]||!category[1]||!mode[0]||!mode[1]||!mode[2]||!palette||!files||!tools)return false;
        for(auto* list:{palette,files,tools})if(!mpc_ui_list_style(list,style)||
            !mpc_ui_list_place(list,list==palette?addRows:rows)||!mpc_ui_list_activate_on_click(list,1))return false;
        ghost=mpc_ui_label_create(screen,"Place on canvas");
        if(!ghost||!mpc_ui_label_style(ghost,{0xffd31145,0xffe1e2e5,36})||
           !mpc_ui_control_place(ghost,{canvas.x,canvas.y,240,52})||
           !mpc_ui_control_visible(ghost,0))return false;
        show(ACTIONS);return true;
    }
};
inline bool matches(const MpcEditorItem& item,const std::string& query) {
    std::string haystack=std::string(item.name)+" "+item.detail,needle=query;
    for(char& c:haystack)c=std::tolower(static_cast<unsigned char>(c));
    for(char& c:needle)c=std::tolower(static_cast<unsigned char>(c));
    return haystack.find(needle)!=std::string::npos;
}
inline void searchLabel(Shell& shell,const std::string& query) {
    std::string label=query.empty()?"SEARCH":"SEARCH: "+query.substr(0,18);
    mpc_ui_button_text(shell.search,label.c_str());
    mpc_ui_button_text_size(shell.search,18);
}
}
#endif
