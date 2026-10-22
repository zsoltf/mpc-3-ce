/* Version-pinned adapter: Pd owns objects, position, ports, edges and writer. */
#include "pd-touch.h"
#include "z_libpd.h"
#include "g_canvas.h"
#include "m_imp.h"
#include "g_all_guis.h"
#include "pd-control-layout.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static const char directory[]="/tmp/mpclearn-pd-touch/state";
static const char filename[]="prototype.pd";
static const char receiver[]="pd-prototype.pd";
struct PdTouch { t_pdinstance* instance; t_canvas *canvas,*root; uint32_t revision; char error[256],warning[256],path[512],origin[512],save_path[512]; int copy,readonly,sample_rate,inputs,active; };
static const char* copy_path(PdTouch* o){return o->save_path[0]?o->save_path:"/tmp/mpclearn-pd-touch/state/prototype.pd";}
static void capture(const char* s) {PdTouch* o=libpd_get_instancedata();if(o&&s){size_t used=strlen(o->warning);snprintf(o->warning+used,sizeof(o->warning)-used,"%s",s);}}
static t_pdinstance* enter(PdTouch* o) {t_pdinstance* p=libpd_this_instance();libpd_set_instance(o->instance);return p;}
static void leave(t_pdinstance* p) {libpd_set_instance(p);}
static int fail(PdTouch* o,const char* s) {snprintf(o->error,sizeof(o->error),"%s",s);return 0;}
static int contains_canvas(t_canvas* root,t_canvas* target,unsigned depth,unsigned* budget){
    if(root==target)return 1;if(depth>64)return 0;
    for(t_gobj* g=root->gl_list;g&&*budget;g=g->g_next){--*budget;if(pd_class(&g->g_pd)==canvas_class&&contains_canvas((t_canvas*)g,target,depth+1,budget))return 1;}return 0;
}
static int live_canvas(PdTouch* o){
    int found=0;for(t_canvas* c=pd_getcanvaslist();c;c=c->gl_next)if(c==o->root){found=1;break;}
    if(!found){o->root=o->canvas=NULL;return fail(o,"Patch closed by Pd; load a patch to continue");}
    unsigned budget=4096;if(!contains_canvas(o->root,o->canvas,0,&budget)){o->canvas=o->root;o->readonly=0;}
    return 1;
}
static t_object* member(PdTouch* o,int index) {
    if(!o->canvas||index<0||index>=PD_TOUCH_NODES)return NULL;
    t_gobj* g=o->canvas->gl_list;
    while(g&&index--)g=g->g_next;
    return g?pd_checkobject(&g->g_pd):NULL;
}
static int index_of(PdTouch* o,t_object* object) {
    int n=0;for(t_gobj* g=o->canvas->gl_list;g;g=g->g_next,++n)if((void*)g==(void*)object)return n;
    return -1;
}
static int unresolved_tree(t_canvas* c,unsigned depth,unsigned* budget){
    if(depth>64)return 1;
    for(t_gobj* g=c->gl_list;g;g=g->g_next){if(!*budget)return 1;--*budget;t_object* obj=pd_checkobject(&g->g_pd);
        if(obj&&obj->te_type==T_OBJECT&&!strcmp(class_getname(pd_class(&g->g_pd)),"text"))return 1;
        if(pd_class(&g->g_pd)==canvas_class&&unresolved_tree((t_canvas*)g,depth+1,budget))return 1;}
    return 0;
}
static int gop_visible(const PdTouchNode* n){return (n->control&&n->control!=PD_CONTROL_MESSAGE)||n->kind==T_TEXT||n->gop||n->array;}
static int snapshot_locked(PdTouch* o,PdTouchSnapshot* out,unsigned gop_depth) {
    if(!gop_depth&&!live_canvas(o))return 0;
    if(gop_depth>16)return fail(o,"GOP nesting exceeds 16; owner preserved");
    /* Public snapshots retain zeroed unused storage. Private GOP scratch is
       consumed only through its counted node/edge/sample prefixes: clearing
       its unused 65536-sample capacity at every nested graph serves no reader. */
    memset(out,0,gop_depth?offsetof(PdTouchSnapshot,node):sizeof(*out));
    out->revision=o->revision;out->canvas_identity=(uintptr_t)o->canvas;
    for(t_gobj* g=o->canvas->gl_list;g;g=g->g_next) {
        if(out->count>=PD_TOUCH_NODES)return fail(o,"View exceeds 256 objects; engine preserved");
        t_object* object=pd_checkobject(&g->g_pd);
        const char* name=class_getname(pd_class(&g->g_pd));
        PdTouchNode* n=&out->node[out->count++];if(gop_depth)memset(n,0,sizeof(*n));n->identity=(uintptr_t)g;n->owner_parent=-1;n->owner_index=out->count-1;snprintf(n->class_name,sizeof(n->class_name),"%s",name);
        if(pd_class(&g->g_pd)==garray_class){
            PdGarrayLayout* a=(PdGarrayLayout*)g;t_glist* graph=garray_getglist((t_garray*)g);int count=0;t_word* words=NULL;
            n->kind=4;n->array=1;n->array_offset=-1;n->array_edit=a->x_edit;n->array_save=a->x_saveit;n->hide_text=1;
            n->pixel_width=graph->gl_pixwidth;n->pixel_height=graph->gl_pixheight;
            n->array_x1=graph->gl_x1;n->array_x2=graph->gl_x2;n->array_y1=graph->gl_y1;n->array_y2=graph->gl_y2;
            snprintf(n->text,sizeof(n->text),"%s",a->x_name->s_name);
            if(garray_getfloatwords((t_garray*)g,&count,&words)){n->array_count=count;
                if(count>0&&count<=PD_TOUCH_SAMPLES-out->samples){n->array_offset=out->samples;
                    for(int i=0;i<count;++i)out->sample[out->samples++]=words[i].w_float;}}
            continue;
        }
        if(!object){n->kind=4;n->y=out->count*24;snprintf(n->text,sizeof(n->text),"[%s: noneditable presentation]",name);continue;}
        n->x=object->te_xpix;n->y=object->te_ypix;n->width=object->te_width;n->kind=object->te_type;
        n->unresolved=object->te_type==T_OBJECT&&!strcmp(name,"text");out->unresolved+=n->unresolved;
        n->child=pd_class(&g->g_pd)==canvas_class;n->editable=!o->readonly&&!n->unresolved;
        n->inputs=obj_ninlets(object);n->outputs=obj_noutlets(object);
        if(n->inputs>32||n->outputs>32)return fail(o,"View exceeds 32 ports; engine preserved");
        for(int i=0;i<n->inputs;++i)if(obj_issignalinlet(object,i))n->signal_inputs|=1u<<i;
        for(int i=0;i<n->outputs;++i)if(obj_issignaloutlet(object,i))n->signal_outputs|=1u<<i;
        if(object->te_type==T_MESSAGE)n->control=PD_CONTROL_MESSAGE;
        else if(object->te_type==T_ATOM&&!strcmp(name,"gatom")){
            PdGatomPrefix* a=(PdGatomPrefix*)object;
            n->control=a->a_flavor==A_FLOAT?PD_CONTROL_NUMBER:a->a_flavor==A_SYMBOL?PD_CONTROL_SYMBOL:PD_CONTROL_LIST;
            n->minimum=a->a_draglo;n->maximum=a->a_draghi;
            if(binbuf_getnatom(object->te_binbuf)&&binbuf_getvec(object->te_binbuf)[0].a_type==A_FLOAT)n->value=atom_getfloat(binbuf_getvec(object->te_binbuf));
            if(a->a_label)snprintf(n->label,sizeof(n->label),"%s",a->a_label->s_name);
        }else if(!strcmp(name,"bng")||!strcmp(name,"tgl")||!strcmp(name,"nbx")||!strcmp(name,"hsl")||!strcmp(name,"hradio")){
            t_iemgui* gui=(t_iemgui*)object;int zoom=o->canvas->gl_zoom>0?o->canvas->gl_zoom:1;
            n->pixel_width=gui->x_w/zoom;n->pixel_height=gui->x_h/zoom;
            n->label_positioned=1;n->label_x=gui->x_ldx;n->label_y=gui->x_ldy;n->label_size=gui->x_fontsize;
            if(gui->x_lab&&strcmp(gui->x_lab->s_name,"empty"))snprintf(n->label,sizeof(n->label),"%s",gui->x_lab->s_name);
            if(!strcmp(name,"bng"))n->control=PD_CONTROL_BANG;
            else if(!strcmp(name,"tgl")){n->control=PD_CONTROL_TOGGLE;n->value=((t_toggle*)object)->x_on;}
            else if(!strcmp(name,"hsl")){t_slider* slider=(t_slider*)object;n->control=PD_CONTROL_SLIDER;n->value=slider->x_fval;n->minimum=slider->x_min;n->maximum=slider->x_max;n->logarithmic=slider->x_lin0_log1;n->horizontal=slider->x_orientation==horizontal;int length=n->horizontal?n->pixel_width:n->pixel_height;n->position=length>1?slider->x_val/(100.f*(length-1)):0;}
            else if(!strcmp(name,"hradio")){t_radio* radio=(t_radio*)object;n->control=PD_CONTROL_RADIO;n->value=radio->x_fval;n->position=radio->x_on;n->steps=radio->x_number;n->horizontal=radio->x_orientation==horizontal;if(n->horizontal)n->pixel_width*=n->steps;else n->pixel_height*=n->steps;}
            else {t_my_numbox* box=(t_my_numbox*)object;n->control=PD_CONTROL_NUMBER;n->value=box->x_val;n->minimum=box->x_min;n->maximum=box->x_max;}
        }
        if(n->child){t_canvas* child=(t_canvas*)object;if(child->gl_isgraph&&child->gl_goprect&&child->gl_zoom==o->canvas->gl_zoom){n->gop=1;n->pixel_width=child->gl_pixwidth;n->pixel_height=child->gl_pixheight;n->hide_text=child->gl_hidetext;}}
        if(n->child){t_canvas* child=(t_canvas*)object;if(child->gl_isgraph)for(t_gobj* a=child->gl_list;a;a=a->g_next)if(pd_class(&a->g_pd)==garray_class){n->gop=2;n->pixel_width=child->gl_pixwidth;n->pixel_height=child->gl_pixheight;n->hide_text=1;break;}}
        /* Private nodes rejected by the same parent filter never export text.
           Retain all live validation/limits even for these hidden objects. */
        if(!gop_depth||gop_visible(n)){
            char* text=NULL;int size=0;binbuf_gettext(object->te_binbuf,&text,&size);
            n->text_editable=!o->readonly&&size<=511&&(object->te_type==T_OBJECT||object->te_type==T_MESSAGE||object->te_type==T_TEXT)&&glist_getcanvas(o->canvas)==o->canvas;
            if(text){int copy=size<511?size:511;memcpy(n->text,text,copy);if(size>511)memcpy(n->text+487,"... [display truncated]",23);freebytes(text,size);}
            if(!n->text[0])snprintf(n->text,sizeof(n->text),"[%s]",name);
        }
    }
    out->visible=out->count;
    t_linetraverser t;linetraverser_start(&t,o->canvas);
    while(linetraverser_next(&t)) {
        if(out->edges>=PD_TOUCH_EDGES)return fail(o,"Cable limit exceeded");
        PdTouchEdge* e=&out->edge[out->edges++];e->source=index_of(o,t.tr_ob);e->target=index_of(o,t.tr_ob2);
        e->outlet=t.tr_outno;e->inlet=t.tr_inno;
        if(e->source<0||e->target<0)return fail(o,"Invalid live cable");
    }
    out->readonly=o->readonly;snprintf(out->name,sizeof(out->name),"%s",o->canvas->gl_name->s_name);
    snprintf(out->path,sizeof(out->path),"%s",o->path);snprintf(out->warning,sizeof(out->warning),"%s",o->warning);
    for(t_canvas* c=o->canvas;c&&c!=o->root;c=c->gl_owner)++out->depth;
    for(int parent=0;parent<out->count;++parent)if(out->node[parent].gop){
        t_canvas* child=(t_canvas*)member(o,parent);PdTouch view=*o;view.canvas=child;view.readonly=1;
        PdTouchSnapshot* nested=malloc(sizeof(*nested));if(!nested)return fail(o,"GOP snapshot allocation failed");
        /* The child was just resolved from this live parent under the same
           lock. Do not run stale-view recovery on an internal subtree read. */
        if(!snapshot_locked(&view,nested,gop_depth+1)){free(nested);return fail(o,view.error);}
        int mapping[PD_TOUCH_NODES];for(int i=0;i<PD_TOUCH_NODES;++i)mapping[i]=-1;
        PdTouchNode* panel=&out->node[parent];
        for(int i=0;i<nested->visible;++i){PdTouchNode node=nested->node[i];
            if(!gop_visible(&node))continue;
            if(panel->gop==2&&!node.array)continue;
            if(node.owner_parent>=0&&mapping[node.owner_parent]<0)continue;
            int direct_array=node.array&&node.owner_parent<0;
            node.x+=panel->x-(direct_array?0:child->gl_xmargin);node.y+=panel->y-(direct_array?0:child->gl_ymargin);
            /* Modern GOP uses Pd's owned margin/size, with no fake window map.
               Native text metrics are reinterpreted; clip to the real panel. */
            if(node.x<panel->x||node.y<panel->y||node.x>=panel->x+panel->pixel_width||node.y>=panel->y+panel->pixel_height)continue;
            if(out->visible>=PD_TOUCH_NODES){free(nested);return fail(o,"GOP view exceeds 256 visible items; owner preserved");}
            node.clip_x=panel->x;node.clip_y=panel->y;node.clip_w=panel->pixel_width;node.clip_h=panel->pixel_height;
            if(nested->node[i].clip_w){int x=nested->node[i].clip_x+panel->x-child->gl_xmargin,y=nested->node[i].clip_y+panel->y-child->gl_ymargin;
                int right=x+nested->node[i].clip_w,bottom=y+nested->node[i].clip_h;
                if(right>node.clip_x+node.clip_w)right=node.clip_x+node.clip_w;if(bottom>node.clip_y+node.clip_h)bottom=node.clip_y+node.clip_h;
                if(x>node.clip_x)node.clip_x=x;if(y>node.clip_y)node.clip_y=y;node.clip_w=right-node.clip_x;node.clip_h=bottom-node.clip_y;
                if(node.clip_w<=0||node.clip_h<=0)continue;}
            node.editable=node.text_editable=0;node.owner_parent=node.owner_parent<0?parent:mapping[node.owner_parent];
            if(node.array&&node.array_offset>=0){if(node.array_count<=PD_TOUCH_SAMPLES-out->samples){memcpy(out->sample+out->samples,nested->sample+node.array_offset,node.array_count*sizeof(float));node.array_offset=out->samples;out->samples+=node.array_count;}else node.array_offset=-1;}
            mapping[i]=out->visible;out->node[out->visible++]=node;
        }
        free(nested);
    }
    return 1;
}
static t_gobj* visible_gobj(PdTouch* o,const PdTouchSnapshot* s,int index){
    if(index<0||index>=s->visible||s->canvas_identity!=(uintptr_t)o->canvas)return NULL;int route[17],count=0;uint64_t identities[17];
    while(index>=0){if(count>=17)return NULL;identities[count]=s->node[index].identity;route[count++]=s->node[index].owner_index;index=s->node[index].owner_parent;}
    t_canvas* canvas=o->canvas;t_gobj* object=NULL;
    for(int i=count-1;i>=0;--i){t_gobj* g=canvas->gl_list;int n=route[i];while(g&&n--)g=g->g_next;if(!g||identities[i]!=(uintptr_t)g)return NULL;object=g;
        if(i){if(pd_class(&g->g_pd)!=canvas_class)return NULL;canvas=(t_canvas*)g;}}
    return object;
}
static t_object* visible_member(PdTouch* o,const PdTouchSnapshot* s,int index){t_gobj* g=visible_gobj(o,s,index);return g?pd_checkobject(&g->g_pd):NULL;}
/* Caller holds the existing instance lock; never retain this object afterward. */
static t_object* control_locked(PdTouch* o,const PdTouchSnapshot* snapshot,int index,PdTouchNode* output){
    if(!snapshot||index<0||index>=snapshot->visible)return NULL;
    t_object* object=live_canvas(o)?visible_member(o,snapshot,index):NULL;
    int ok=0;
    if(object){
        const char* name=class_getname(pd_class(&object->ob_pd));
        *output=snapshot->node[index];
        if(!strcmp(name,output->class_name)){
            if(output->control==PD_CONTROL_TOGGLE&&!strcmp(name,"tgl")){
                output->value=((t_toggle*)object)->x_on;ok=1;
            }else if(output->control==PD_CONTROL_SLIDER&&!strcmp(name,"hsl")){
                t_slider* slider=(t_slider*)object;
                int zoom=o->canvas->gl_zoom>0?o->canvas->gl_zoom:1;
                int length=(slider->x_orientation==horizontal?slider->x_gui.x_w:slider->x_gui.x_h)/zoom;
                output->value=slider->x_fval;output->minimum=slider->x_min;output->maximum=slider->x_max;
                output->logarithmic=slider->x_lin0_log1;output->position=length>1?slider->x_val/(100.f*(length-1)):0;ok=1;
            }else if(output->control==PD_CONTROL_RADIO&&!strcmp(name,"hradio")){
                t_radio* radio=(t_radio*)object;output->value=radio->x_fval;output->position=radio->x_on;output->steps=radio->x_number;ok=1;
            }else if(output->control==PD_CONTROL_NUMBER&&!strcmp(name,"nbx")){
                t_my_numbox* number=(t_my_numbox*)object;output->value=number->x_val;output->minimum=number->x_min;output->maximum=number->x_max;ok=1;
            }else if(output->control==PD_CONTROL_NUMBER&&!strcmp(name,"gatom")&&object->te_type==T_ATOM){
                PdGatomPrefix* atom=(PdGatomPrefix*)object;
                if(atom->a_flavor==A_FLOAT&&binbuf_getnatom(object->te_binbuf)&&binbuf_getvec(object->te_binbuf)[0].a_type==A_FLOAT){
                    output->value=atom_getfloat(binbuf_getvec(object->te_binbuf));output->minimum=atom->a_draglo;output->maximum=atom->a_draghi;ok=1;}
            }
        }
    }
    return ok?object:NULL;
}
int pd_touch_control_read(PdTouch* o,const PdTouchSnapshot* snapshot,int index,PdTouchNode* output){
    if(!o||!output)return 0;
    t_pdinstance* previous=enter(o);sys_lock();
    int ok=control_locked(o,snapshot,index,output)!=NULL;
    sys_unlock();leave(previous);return ok;
}
int pd_touch_number_from_snapshot(PdTouch* o,const PdTouchSnapshot* snapshot,int index,float value){
    if(!o||!isfinite(value))return 0;
    t_pdinstance* previous=enter(o);sys_lock();PdTouchNode node;
    t_object* object=control_locked(o,snapshot,index,&node);
    int ok=object&&node.control==PD_CONTROL_NUMBER&&isfinite(node.minimum)&&isfinite(node.maximum);
    if(ok){
        if(node.maximum>node.minimum){if(value<node.minimum)value=node.minimum;if(value>node.maximum)value=node.maximum;}
        pd_float(&object->ob_pd,value);++o->revision;
    }
    sys_unlock();leave(previous);return ok;
}
int pd_touch_array(PdTouch* o,int index,uint32_t revision,int first,int count,const float* values){
    if(!o||!values||first<0||count<1||count>PD_TOUCH_SAMPLES)return 0;
    for(int i=0;i<count;++i)if(!isfinite(values[i]))return fail(o,"Array values must be finite");
    PdTouchSnapshot* s=malloc(sizeof(*s));t_atom* atoms=malloc((count+1)*sizeof(*atoms));if(!s||!atoms){free(s);free(atoms);return fail(o,"Array intent allocation failed");}
    t_pdinstance* p=enter(o);sys_lock();int ok=revision==o->revision&&snapshot_locked(o,s,0)&&index>=0&&index<s->visible;
    t_gobj* g=ok?visible_gobj(o,s,index):NULL;int size=0;t_word* words=NULL;
    ok=g&&pd_class(&g->g_pd)==garray_class&&((PdGarrayLayout*)g)->x_edit&&garray_getfloatwords((t_garray*)g,&size,&words)&&first<=size&&count<=size-first;
    if(ok){SETFLOAT(atoms,first);for(int i=0;i<count;++i)SETFLOAT(atoms+i+1,values[i]);pd_list(&g->g_pd,&s_list,count+1,atoms);
        ok=garray_getfloatwords((t_garray*)g,&size,&words)&&first<=size&&count<=size-first;
        if(ok)for(int i=0;i<count;++i)if(words[first+i].w_float!=values[i])ok=0;
        canvas_dirty(garray_getglist((t_garray*)g),1);++o->revision;}
    sys_unlock();leave(p);free(s);free(atoms);return ok?1:fail(o,"Array target changed, read-only, or outside bounds");
}
int pd_touch_snapshot(PdTouch* o,PdTouchSnapshot* out) {
    if(!o||!out)return 0;t_pdinstance* p=enter(o);sys_lock();int ok=snapshot_locked(o,out,0);sys_unlock();leave(p);return ok;
}
static int create(PdTouch* o,int load) {
    t_pdinstance* previous=libpd_this_instance();o->instance=libpd_new_instance();
    if(!o->instance){leave(previous);return fail(o,"Pd instance failed");}
    libpd_set_instancedata(o,NULL);libpd_set_printhook(capture);
    if(o->sample_rate&&libpd_init_audio(o->inputs,2,o->sample_rate)){leave(previous);return fail(o,"Pd audio configuration failed");}
    libpd_add_to_search_path("/tmp/mpclearn-pd-touch/library/extra");
    if(!o->path[0])snprintf(o->path,sizeof(o->path),"%s",copy_path(o));
    if(load){char dir[512];strcpy(dir,o->origin[0]?o->origin:o->path);char* base=strrchr(dir,'/');if(base){*base++=0;
        libpd_add_to_search_path(dir);
        if(o->copy){/* Load the engine-written copy with the original canvas directory.
                       Pd still resolves relative abstractions/resources itself. */
            char relative[1024]="";for(const char* q=dir;*q;++q)if(*q=='/')strcat(relative,"../");
            strcat(relative,o->path+1);o->canvas=libpd_openfile(relative,dir);
            if(o->canvas){sys_lock();canvas_rename(o->canvas,gensym(base),gensym(dir));sys_unlock();}
        }else o->canvas=libpd_openfile(base,dir);
    }}
    else {
        char folder[512];snprintf(folder,sizeof(folder),"%s",copy_path(o));char* slash=strrchr(folder,'/');if(slash)*slash=0;
        t_atom a[2];libpd_set_symbol(a,filename);libpd_set_symbol(a+1,folder);
        if(!libpd_message("pd","menunew",2,a)) {
            sys_lock();o->canvas=(t_canvas*)pd_findbyclass(gensym(receiver),canvas_class);sys_unlock();
        }
    }
    if(o->sample_rate){libpd_start_message(1);libpd_add_float(o->active!=0);libpd_finish_message("pd","dsp");}
    leave(previous);
    if(!o->canvas)return fail(o,"Pd canvas failed");
    o->root=o->canvas;++o->revision;return 1;
}
PdTouch* pd_touch_open(void) {
    (void)libpd_init();
    if((mkdir("/tmp/mpclearn-pd-touch",0700)&&errno!=EEXIST)||(mkdir(directory,0700)&&errno!=EEXIST))return NULL;
    PdTouch* o=calloc(1,sizeof(*o));if(!o)return NULL;
    struct stat st;int load=!stat("/tmp/mpclearn-pd-touch/state/prototype.pd",&st);
    PdTouchSnapshot admitted;
    if((load&&(!S_ISREG(st.st_mode)||st.st_size>65536))||!create(o,load)||!pd_touch_snapshot(o,&admitted)){pd_touch_close(&o);return NULL;}
    return o;
}
PdTouch* pd_touch_open_document(const char* source,int saved_copy){
    if(!source||strlen(source)>=512)return NULL;(void)libpd_init();
    if((mkdir("/tmp/mpclearn-pd-touch",0700)&&errno!=EEXIST)||(mkdir(directory,0700)&&errno!=EEXIST))return NULL;
    PdTouch* o=calloc(1,sizeof(*o));if(!o)return NULL;strcpy(o->origin,source);o->copy=saved_copy&&source[0];
    if(o->copy)snprintf(o->path,sizeof(o->path),"%s/%s",directory,filename);else strcpy(o->path,source);
    struct stat st;PdTouchSnapshot admitted;
    if((source[0]&&(stat(o->path,&st)||!S_ISREG(st.st_mode)||st.st_size>4194304))||!create(o,source[0]!=0)||!pd_touch_snapshot(o,&admitted)){pd_touch_close(&o);return NULL;}return o;
}
void pd_touch_close(PdTouch** pointer) {
    if(!pointer||!*pointer)return;PdTouch* o=*pointer;t_pdinstance* p=libpd_this_instance();
    if(o->instance){libpd_set_instance(o->instance);if(o->root)libpd_closefile(o->root);libpd_free_instance(o->instance);}
    if(p&&p!=o->instance)leave(p);else leave(libpd_main_instance());free(o);*pointer=NULL;
}
PdTouch* pd_touch_open_hosted(uint64_t owner,int inputs,int sample_rate){
    if(!owner||(inputs!=0&&inputs!=2)||sample_rate<8000||sample_rate>192000)return NULL;
    (void)libpd_init();
    if((mkdir("/tmp/mpclearn-pd-touch",0700)&&errno!=EEXIST)||(mkdir(directory,0700)&&errno!=EEXIST))return NULL;
    char folder[256];snprintf(folder,sizeof(folder),"%s/instance.%llu",directory,(unsigned long long)owner);
    if(mkdir(folder,0700)&&errno!=EEXIST)return NULL;
    PdTouch* o=calloc(1,sizeof(*o));if(!o)return NULL;
    snprintf(o->save_path,sizeof(o->save_path),"%s/%s",folder,filename);
    o->sample_rate=sample_rate;o->inputs=inputs;
    if(!create(o,0)){pd_touch_close(&o);return NULL;}return o;
}
int pd_touch_audio(PdTouch* o,int sample_rate,int active){
    if(!o||!o->sample_rate||sample_rate<8000||sample_rate>192000)return 0;
    t_pdinstance* previous=enter(o);int okay=1;
    if(sample_rate!=o->sample_rate)okay=!libpd_init_audio(o->inputs,2,sample_rate);
    if(okay){o->sample_rate=sample_rate;o->active=active!=0;libpd_start_message(1);libpd_add_float(o->active);okay=!libpd_finish_message("pd","dsp");}
    leave(previous);return okay;
}
int pd_touch_tick(PdTouch* o,const float* input,float* stereo){
    if(!o||!o->active||!stereo||(o->inputs&&!input))return 0;
    t_pdinstance* previous=enter(o);int okay=!libpd_process_float(1,input,stereo);leave(previous);return okay;
}
int pd_touch_midi(PdTouch* o,unsigned status,unsigned first,unsigned second){
    if(!o||status<0x80||status>0xef||first>127||second>127)return 0;
    t_pdinstance* previous=enter(o);int result=-1;int channel=status&15;
    switch(status&0xf0){
    case 0x80:result=libpd_noteon(channel,first,0);break;
    case 0x90:result=libpd_noteon(channel,first,second);break;
    case 0xa0:result=libpd_polyaftertouch(channel,first,second);break;
    case 0xb0:result=libpd_controlchange(channel,first,second);break;
    case 0xc0:result=libpd_programchange(channel,first);break;
    case 0xd0:result=libpd_aftertouch(channel,first);break;
    case 0xe0:result=libpd_pitchbend(channel,(int)(first|(second<<7))-8192);break;
    }
    leave(previous);return !result;
}
int pd_touch_add(PdTouch* o,int kind,int x,int y) {
    if(!o||o->readonly||kind<0||kind>1||x<0||y<0||x>30000||y>30000)return 0;
    PdTouchSnapshot s;if(!pd_touch_snapshot(o,&s)||s.count>=PD_TOUCH_NODES)return fail(o,"View object limit");
    t_pdinstance* p=enter(o);t_atom a[4];libpd_set_float(a,x);libpd_set_float(a+1,y);
    libpd_set_symbol(a+2,kind?"*~":"osc~");libpd_set_float(a+3,kind?0.1f:220);
    sys_lock();pd_typedmess(&o->canvas->gl_pd,gensym("obj"),4,a);++o->revision;int ok=1;t_object* added=member(o,s.count);
    ok=ok&&added&&!strcmp(class_getname(pd_class(&added->ob_pd)),kind?"*~":"osc~");
    sys_unlock();leave(p);
    PdTouchSnapshot after;return ok&&pd_touch_snapshot(o,&after)&&after.count==s.count+1;
}
int pd_touch_add_text(PdTouch* o,const char* text,int x,int y){
    return pd_touch_create_text(o,PD_CREATE_OBJECT,text,x,y);
}
int pd_touch_create_text(PdTouch* o,int kind,const char* text,int x,int y){
    if(!o||o->readonly||kind<PD_CREATE_OBJECT||kind>PD_CREATE_COMMENT||!text||!text[0]||strlen(text)>255||strpbrk(text,"\n\r")||(kind==PD_CREATE_OBJECT&&strchr(text,';'))||x<0||y<0||x>30000||y>30000)return 0;
    PdTouchSnapshot before;if(!pd_touch_snapshot(o,&before)||before.count>=PD_TOUCH_NODES)return fail(o,"View object limit");
    t_pdinstance* p=enter(o);sys_lock();t_binbuf* b=binbuf_new();binbuf_text(b,text,strlen(text));int count=binbuf_getnatom(b);
    int ok=count>0&&count<=64; t_atom args[66];
    if(ok){const char* selectors[]={"obj","msg","text"};SETFLOAT(args,x);SETFLOAT(args+1,y);memcpy(args+2,binbuf_getvec(b),count*sizeof(t_atom));pd_typedmess(&o->canvas->gl_pd,gensym(selectors[kind]),count+2,args);canvas_dirty(o->canvas,1);++o->revision;}
    binbuf_free(b);sys_unlock();leave(p);PdTouchSnapshot after;
    if(!ok||!pd_touch_snapshot(o,&after)||after.count!=before.count+1)return fail(o,"Pd could not add object");
    return after.node[before.count].unresolved?fail(o,"Missing Pd class: visible unresolved object; save disabled"):1;
}
int pd_touch_move(PdTouch* o,int index,int x,int y) {
    if(!o||o->readonly||x<0||y<0||x>30000||y>30000)return 0;t_pdinstance* p=enter(o);sys_lock();
    t_object* object=member(o,index);if(object){gobj_displace((t_gobj*)object,o->canvas,x-object->te_xpix,y-object->te_ypix);canvas_dirty(o->canvas,1);++o->revision;}
    sys_unlock();leave(p);return object!=NULL;
}
int pd_touch_edit_text(PdTouch* o,int index,const char* text,int* resulting_node){
    PdTouchSnapshot before;if(!o||!text||strlen(text)>511||!pd_touch_snapshot(o,&before)||index<0||index>=before.count||!before.node[index].text_editable)return o?fail(o,"Text unavailable: read-only, truncated, atom or embedded graph-on-parent"):0;
    t_pdinstance* previous=enter(o);sys_lock();t_canvas* canvas=o->canvas;t_object* object=member(o,index);
    if(!object){sys_unlock();leave(previous);return 0;}
    /* Use Pd's editor-owned connection/selection buffers. There is no Tk peer:
       sign off its newly created connection now, while the owner is live, so
       canonical editor destruction is immediate rather than clock-delayed. */
    if(!canvas->gl_editor){canvas_create_editor(canvas);pd_typedmess((t_pd*)canvas->gl_editor->e_guiconnect,gensym("signoff"),0,NULL);}
    glist_noselect(canvas);
    if(object->te_type==T_OBJECT){glist_select(canvas,&object->te_g);canvas_stowconnections(canvas);glist_noselect(canvas);}
    int oldkind=object->te_type;
    text_setto(object,canvas,text,(int)strlen(text));
    int ok=live_canvas(o)&&o->canvas==canvas;
    if(ok){canvas_dirty(canvas,1);++o->revision;glist_noselect(canvas);
        /* Pd moves recreated objects to the end; message/comment identity stays.
           Resolve only after the canonical operation, never retain old indices. */
        int selected=-1,n=0;for(t_gobj* g=canvas->gl_list;g;g=g->g_next,++n)if(oldkind==T_OBJECT||(void*)g==(void*)object)selected=n;
        if(resulting_node)*resulting_node=selected;}
    sys_unlock();leave(previous);return ok;
}
int pd_touch_delete(PdTouch* o,int index) {
    if(!o||o->readonly)return 0;t_pdinstance* p=enter(o);sys_lock();t_object* object=member(o,index);
    if(object){glist_delete(o->canvas,(t_gobj*)object);canvas_dirty(o->canvas,1);++o->revision;}
    sys_unlock();leave(p);return object!=NULL;
}
static int edge_message(PdTouch* o,const char* selector,int source,int outlet,int target,int inlet) {
    t_atom a[4];libpd_set_float(a,source);libpd_set_float(a+1,outlet);libpd_set_float(a+2,target);libpd_set_float(a+3,inlet);
    sys_lock();pd_typedmess(&o->canvas->gl_pd,gensym(selector),4,a);sys_unlock();return 1;
}
int pd_touch_connect(PdTouch* o,int source,int outlet,int target,int inlet) {
    PdTouchSnapshot s;if(!o||o->readonly||!pd_touch_snapshot(o,&s)||source<0||target<0||source>=s.count||target>=s.count||
       outlet<0||outlet>=s.node[source].outputs||inlet<0||inlet>=s.node[target].inputs||s.edges>=PD_TOUCH_EDGES)return 0;
    if((s.node[source].signal_outputs&(1u<<outlet))&&!(s.node[target].signal_inputs&(1u<<inlet)))return fail(o,"Signal needs a signal inlet");
    for(int i=0;i<s.edges;++i){PdTouchEdge* e=&s.edge[i];if(e->source==source&&e->target==target&&e->outlet==outlet&&e->inlet==inlet)return 1;}
    t_pdinstance* p=enter(o);int ok=edge_message(o,"connect",source,outlet,target,inlet);++o->revision;leave(p);
    PdTouchSnapshot after;if(!ok||!pd_touch_snapshot(o,&after))return 0;
    for(int i=0;i<after.edges;++i){PdTouchEdge* e=&after.edge[i];if(e->source==source&&e->target==target&&e->outlet==outlet&&e->inlet==inlet)return 1;}
    return fail(o,"Pd refused cable");
}
int pd_touch_unwire(PdTouch* o,int index) {
    PdTouchSnapshot s;if(!o||o->readonly||!pd_touch_snapshot(o,&s)||index<0||index>=s.count)return 0;
    t_pdinstance* p=enter(o);int ok=1;
    for(int i=0;i<s.edges;++i){PdTouchEdge* e=&s.edge[i];if(e->source==index||e->target==index)ok=edge_message(o,"disconnect",e->source,e->outlet,e->target,e->inlet)&&ok;}
    ++o->revision;leave(p);
    PdTouchSnapshot after;if(!ok||!pd_touch_snapshot(o,&after))return 0;
    for(int i=0;i<after.edges;++i)if(after.edge[i].source==index||after.edge[i].target==index)return fail(o,"Pd cable still connected");
    return 1;
}
int pd_touch_save(PdTouch* o) {
    if(!o)return 0;t_pdinstance* p=enter(o);sys_lock();if(!live_canvas(o)){sys_unlock();leave(p);return 0;}unsigned budget=4096;
    if(unresolved_tree(o->root,0,&budget)){sys_unlock();leave(p);return fail(o,"Unresolved objects or inspection limit: original preserved; save disabled");}
    /* Invoke the same Pd serializer used by savetofile, without writing.
       This bounds the one output file before the actual owner writes it. */
    t_binbuf* serialized=binbuf_new();mess1(&o->root->gl_pd,gensym("saveto"),serialized);
    char* bytes=NULL;int size=0;binbuf_gettext(serialized,&bytes,&size);if(bytes)freebytes(bytes,size);binbuf_free(serialized);
    if(size<1||size>4194304){sys_unlock();leave(p);return fail(o,"Pd serialized copy exceeds 4 MiB; previous copy retained");}
    t_symbol* oldname=o->root->gl_name; t_symbol* olddir=canvas_getdir(o->root);canvas_dirty(o->root,1);
    char folder[512];snprintf(folder,sizeof(folder),"%s",copy_path(o));char* slash=strrchr(folder,'/');if(slash)*slash=0;
    t_atom a[3];libpd_set_symbol(a,filename);libpd_set_symbol(a+1,folder);libpd_set_float(a+2,0);
    pd_typedmess(&o->root->gl_pd,gensym("savetofile"),3,a);int ok=!o->root->gl_dirty;
    canvas_rename(o->root,oldname,olddir);sys_unlock();leave(p);
    if(ok){snprintf(o->path,sizeof(o->path),"%s",copy_path(o));o->copy=o->origin[0]!=0;}
    return ok?1:fail(o,"Pd save failed");
}
int pd_touch_serialize(PdTouch* o,unsigned char** output,uint32_t* length,char origin[512]){
    if(!o||!output||!length||!origin)return 0;*output=NULL;*length=0;
    t_pdinstance* previous=enter(o);sys_lock();unsigned budget=4096;
    if(!live_canvas(o)||unresolved_tree(o->root,0,&budget)){sys_unlock();leave(previous);return fail(o,"Unresolved patch cannot be serialized");}
    t_binbuf* serialized=binbuf_new();mess1(&o->root->gl_pd,gensym("saveto"),serialized);
    char* text=NULL;int size=0;binbuf_gettext(serialized,&text,&size);binbuf_free(serialized);
    unsigned char* bytes=size>0&&size<=4194304?malloc(size):NULL;
    if(bytes){memcpy(bytes,text,size);snprintf(origin,512,"%s",o->origin[0]?o->origin:copy_path(o));*output=bytes;*length=size;}
    if(text)freebytes(text,size);sys_unlock();leave(previous);
    return bytes?1:fail(o,"Canonical Pd host state exceeds 4 MiB or allocation failed");
}
static int replace_context(PdTouch* o,const char* path,int reload,const char* origin) {
    if(!o||!path||strlen(path)>=512)return 0;struct stat st;if(stat(path,&st)||!S_ISREG(st.st_mode)||st.st_size>4194304)return fail(o,"Patch unavailable or over 4 MiB");
    PdTouch* next=calloc(1,sizeof(*next));if(!next)return 0;
    strcpy(next->path,path);strcpy(next->origin,origin?origin:reload?o->origin:path);next->copy=origin?1:reload?o->copy:0;
    strcpy(next->save_path,o->save_path);next->sample_rate=o->sample_rate;next->inputs=o->inputs;next->active=o->active;
    PdTouchSnapshot admitted;
    if(!create(next,1)||!pd_touch_snapshot(next,&admitted)){pd_touch_close(&next);return fail(o,"Fresh Pd load failed; previous patch retained");}
    t_pdinstance* p=enter(o);if(o->root)libpd_closefile(o->root);libpd_free_instance(o->instance);leave(p==o->instance?libpd_main_instance():p);
    uint32_t revision=o->revision+1;*o=*next;o->revision=revision;free(next);
    p=enter(o);libpd_set_instancedata(o,NULL);leave(p);return 1;
}
int pd_touch_restore(PdTouch* o,const char* origin,const void* bytes,uint32_t length){
    if(!o||!origin||origin[0]!='/'||strlen(origin)>=512||!bytes||!length||length>4194304)return 0;
    char path[512];snprintf(path,sizeof(path),"%s",copy_path(o));char* slash=strrchr(path,'/');if(!slash)return 0;strcpy(slash+1,"recall.pd");
    FILE* file=fopen(path,"wb");if(!file)return fail(o,"Host recall staging unavailable");
    int okay=fwrite(bytes,1,length,file)==length;if(fclose(file))okay=0;
    return okay?replace_context(o,path,1,origin):fail(o,"Host recall staging failed");
}
int pd_touch_reload(PdTouch* o){return o?replace_context(o,o->path,1,NULL):0;}
int pd_touch_recall_copy(PdTouch* o){if(!o)return 0;int previous=o->copy;o->copy=o->origin[0]!=0;int ok=replace_context(o,copy_path(o),1,NULL);if(!ok)o->copy=previous;return ok;}
int pd_touch_load(PdTouch* o,const char* path){return replace_context(o,path,0,NULL);}
int pd_touch_enter(PdTouch* o,int node){if(!o)return 0;t_pdinstance* p=enter(o);sys_lock();t_object* obj=member(o,node);int ok=obj&&pd_class(&obj->ob_pd)==canvas_class;
    if(ok){o->canvas=(t_canvas*)obj;o->readonly=o->readonly||canvas_isabstraction(o->canvas);++o->revision;}sys_unlock();leave(p);return ok;}
int pd_touch_parent(PdTouch* o){if(!o||o->canvas==o->root)return 0;t_pdinstance* p=enter(o);sys_lock();o->canvas=o->canvas->gl_owner;o->readonly=0;
    for(t_canvas* c=o->canvas;c&&c!=o->root;c=c->gl_owner)if(canvas_isabstraction(c))o->readonly=1;
    ++o->revision;sys_unlock();leave(p);return 1;}
const char* pd_touch_error(PdTouch* o){return o&&o->error[0]?o->error:"Pd operation unavailable";}
int pd_touch_activate(PdTouch* o,int index){
    PdTouchSnapshot s;if(!o||!pd_touch_snapshot(o,&s)||index<0||index>=s.visible)return 0;int kind=s.node[index].control;
    if(kind!=PD_CONTROL_MESSAGE&&kind!=PD_CONTROL_BANG&&kind!=PD_CONTROL_TOGGLE)return fail(o,"This object has no supported click control");
    t_pdinstance* p=enter(o);sys_lock();t_object* object=visible_member(o,&s,index);int ok=object!=NULL;
    if(object){/* Exact registered click methods, as invoked by Pd's widgets. */
        t_atom args[5];for(int i=0;i<5;++i)SETFLOAT(args+i,0);pd_typedmess(&object->ob_pd,gensym("click"),5,args);++o->revision;}
    sys_unlock();leave(p);return ok;
}
int pd_touch_number(PdTouch* o,int index,float value){
    PdTouchSnapshot s;if(!o||!isfinite(value)||!pd_touch_snapshot(o,&s))return 0;
    return pd_touch_number_from_snapshot(o,&s,index,value);
}
int pd_touch_control_position(PdTouch* o,int index,float normalized){
    PdTouchSnapshot s;if(!o||!isfinite(normalized)||!pd_touch_snapshot(o,&s)||index<0||index>=s.visible)return 0;
    if(normalized<0)normalized=0;if(normalized>1)normalized=1;PdTouchNode* node=&s.node[index];float value;
    if(node->control==PD_CONTROL_SLIDER)value=node->logarithmic?node->minimum*expf(normalized*logf(node->maximum/node->minimum)):node->minimum+normalized*(node->maximum-node->minimum);
    else if(node->control==PD_CONTROL_RADIO){int step=(int)(normalized*node->steps);value=step>=node->steps?node->steps-1:step;}
    else return fail(o,"Select a slider or radio control");
    if(!isfinite(value))return fail(o,"Pd control range is unavailable");t_pdinstance* p=enter(o);sys_lock();t_object* object=visible_member(o,&s,index);
    if(object){pd_float(&object->ob_pd,value);++o->revision;}sys_unlock();leave(p);return object!=NULL;
}
int pd_touch_atom_text(PdTouch* o,int index,const char* value){
    PdTouchSnapshot s;if(!o||!value||strlen(value)>255||!pd_touch_snapshot(o,&s)||index<0||index>=s.visible)return 0;int kind=s.node[index].control;
    if(kind!=PD_CONTROL_SYMBOL&&kind!=PD_CONTROL_LIST)return fail(o,"Select a symbol or list atom");
    t_pdinstance* p=enter(o);sys_lock();t_object* object=visible_member(o,&s,index);if(object){
        if(kind==PD_CONTROL_SYMBOL)pd_symbol(&object->ob_pd,gensym(value));else{t_binbuf* b=binbuf_new();binbuf_text(b,value,strlen(value));pd_list(&object->ob_pd,&s_list,binbuf_getnatom(b),binbuf_getvec(b));binbuf_free(b);}++o->revision;}
    sys_unlock();leave(p);return object!=NULL;
}
