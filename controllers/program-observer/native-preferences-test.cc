/* ARM composition for the production toolkit adapters. MPC rendering, touch,
 * stock ownership and Graphics are explicit fixtures. */
#define SAMPLER_BANNER_COMPONENT 1
#include "native-preferences.cc"

extern "C" void *malloc(size_t);
extern "C" void free(void *);
extern "C" int printf(const char *,...);
extern "C" void exit(int);
extern "C" int strcmp(const char *,const char *);
extern "C" const char *strstr(const char *,const char *);
extern "C" int memcmp(const void *,const void *,size_t) noexcept;
extern "C" void *memset(void *,int,size_t) noexcept;
extern "C" { uint32_t observer_running=1; }

static unsigned allocations,deletes,string_ctors,string_dtors,tab_ctors,tab_dtors,button_ctors,button_dtors;
static unsigned list_ctors,list_dtors,list_updates,list_selections;
static unsigned list_viewport_gets,list_drag_enables,list_drag_disables,list_background_colours;
static unsigned label_ctors,label_dtors,label_text_updates,slider_ctors,slider_dtors,slider_silent_updates;
static unsigned tab_removes,child_adds,child_removes,overlay_dtors,colour_sets,repaints,toggles,active_calls,front_keeps;
static unsigned paint_fills,paint_colours,paint_fonts,paint_texts;static uint32_t last_fill;static const char *last_paint_text;
static const char *last_primary_text;static float paint_font;static uint32_t paint_colour;
struct PaintedText {int x,y,w,h,justification,lines;float font,scale;uint32_t colour;const char *text;};
static PaintedText drawn[8];static unsigned drawn_count;
static struct {void *object;int height;} row_heights[32];static unsigned row_height_count;

static unsigned char tab_vtable_fixture[NATIVE_PREFERENCES_TAB_VTABLE_BYTES];
static unsigned char button_vtable_fixture[MPC_UI_BUTTON_VTABLE_BYTES];
static unsigned char list_model_vtable_fixture[MPC_UI_LIST_MODEL_VTABLE_BYTES];
static unsigned char overlay_vtable_fixture[NATIVE_LAUNCHER_OVERLAY_VTABLE_BYTES];
static void *relationships[256][2];static unsigned relationship_count;
static void *ordered_parent,*ordered_children[64];static unsigned ordered_count;
static void *visible_objects[8192];static int visible_values[8192];static unsigned visible_count;
struct Bounds {void *object;int x,y,w,h;};static Bounds bounds_calls[256];static unsigned bounds_count;
static void *launcher_owner_component,*expected_selected;
static void *overlay_children[256];static unsigned overlay_child_count;
static unsigned fail_remove_after;
static bool expect_retained_overlay;
static bool controller_feedback_enabled,controller_feedback_emitting;
static unsigned controller_update_feedback,controller_visibility_feedback;

static void require(bool ok,const char *why){if(!ok){printf("FAIL %s\n",why);exit(1);}}
static void *test_allocate(uint32_t bytes){allocations++;void *p=malloc(bytes);if(p)memset(p,0,bytes);return p;}
static void test_delete(void *p,uint32_t){if(p){deletes++;free(p);}}
static void test_string_ctor(void *storage,const char *text){*(const char**)storage=text;string_ctors++;}
static void test_string_dtor(void *){string_dtors++;}
static void test_set_colour(void *object,int id,uint32_t colour){
 require(object,"qualified Component colour receiver");
 if(id==MPC_UI_LIST_BACKGROUND_COLOUR_ID){list_background_colours++;return;}
 if(id==MPC_UI_LABEL_BACKGROUND_COLOUR_ID||id==MPC_UI_LABEL_TEXT_COLOUR_ID){
  require(colour==0xff17181bu||colour==0xff202126u||
   colour==0xff24252au||colour==0xff303137u||
   colour==0xffaeb0b7u||colour==0xffd5d6d9u||
   colour==0xffe1e2e5u||colour==0xffffffffu,
   "qualified Label colour");
  colour_sets++;return;
 }
 require(id>=0x1000100&&id<=0x1000103,"qualified TextButton colour");
 if(id==0x1000102||id==0x1000103)require(colour==0xffffffffu||colour==0xff73757eu||colour==0xff17181bu,"active, inactive or blank surface text");
 else require(colour==0xff17181bu||colour==0xff24252au||colour==0xff202126u||colour==0xff2f2f34u||
              colour==0xff303137u||colour==0xffd31145u,
              "bounded button fill");
 colour_sets++;
}
static void test_std_string(void *storage,const char *text){require(!strcmp(text,"CONTROLLERS"),"tab label");uint32_t *s=(uint32_t*)storage;s[0]=(uint32_t)(uintptr_t)((unsigned char*)storage+8);s[1]=11;memcpy((unsigned char*)storage+8,text,12);}
static void test_button_ctor(void *button,const void *name){require(*(const char*const*)name,"button name");*(void**)button=button_vtable_fixture+8;*(const char**)((unsigned char*)button+0xa8)=*(const char*const*)name;button_ctors++;}
static void test_button_dtor(void *button){require(*(void**)button==button_vtable_fixture+8,"base TextButton vptr restored");button_dtors++;}
static void test_label_ctor(void *label,const void *name,const void *text){require(label&&*(const char*const*)name&&*(const char*const*)text,"Label construction");label_ctors++;}
static void test_label_dtor(void *label){require(label,"Label destruction");label_dtors++;}
static void test_label_text(void *label,const void *text,int notify){require(label&&*(const char*const*)text&&notify==0,"Label silent text");label_text_updates++;}
static struct {void *object;int value;} label_alignments[64];static unsigned label_alignment_count;
static void test_label_justification(void *label,int value){require(label,"Label justification");if(label_alignment_count<64)label_alignments[label_alignment_count++]={label,value};}
static void test_mouse_intercepts(void *component,int self,int children){require(component&&!self&&!children,"Label touch pass-through");}
static void test_list_ctor(void *list,const void *name,void *model){require(list&&*(const char*const*)name&&model,"named ListBox construction");*(void**)((unsigned char*)list+0x78)=model;*(int*)((unsigned char*)list+0xa4)=-1;list_ctors++;}
static void test_list_dtor(void *list){require(list&&!*(void**)((unsigned char*)list+0x78),"ListBox model cleared before complete destructor");list_dtors++;}
static void test_list_set_model(void *list,void *model){*(void**)((unsigned char*)list+0x78)=model;}
static void test_controller_feedback(bool visibility){
 if(!controller_feedback_enabled||controller_feedback_emitting)return;
 controller_feedback_emitting=true;
 auto &instance=preferences_instances[0];MpcUiList *lists[]={instance.controllers,instance.profiles,instance.inputs,instance.outputs};
 for(auto *list:lists){void *object=mpc_ui_private_list_host_object(list);void *model=*(void**)((unsigned char*)object+0x78);require(model,"presentation feedback has a live model");((void(*)(void*,int))(*(void***)model)[8])(model,0);}
 if(visibility)controller_visibility_feedback++;else controller_update_feedback++;
 controller_feedback_emitting=false;
}
static void test_list_update(void *list){void *model=*(void**)((unsigned char*)list+0x78);require(model&&((int(*)(void*))(*(void***)model)[2])(model)>=0,"ListBox update reads model count");list_updates++;test_controller_feedback(false);}
static void test_list_row_height(void *object,int height){require(height>0&&height<=256,"bounded ListBox row height");if(row_height_count<32)row_heights[row_height_count++]={object,height};}
static void test_list_select(void *list,int row,int dont_scroll,int deselect){require((dont_scroll==0||dont_scroll==1)&&deselect==1,"qualified ListBox select flags");int *selected=(int*)((unsigned char*)list+0xa4);if(*selected==row)return;*selected=row;void *model=*(void**)((unsigned char*)list+0x78);if(model)((void(*)(void*,int))(*(void***)model)[8])(model,row);list_selections++;}
static void *test_list_get_viewport(void *list){require(list,"ListBox owns viewport");list_viewport_gets++;return list;}
static void test_viewport_set_drag(void *viewport,int enabled,float threshold){require(viewport&&threshold==MPC_UI_LIST_DRAG_THRESHOLD,"qualified viewport drag threshold");if(enabled)list_drag_enables++;else list_drag_disables++;}
static void test_slider_ctor(void *slider){require(slider,"Slider construction");slider_ctors++;}
static void test_slider_dtor(void *slider){require(slider,"Slider destruction");slider_dtors++;}
static void test_slider_style(void *slider,int style){require(slider&&(style==0||style==1),"Slider style");}
static void test_slider_range(void *slider,double minimum,double maximum,double interval){require(slider&&minimum<maximum&&interval>=0.0,"Slider range");}
static void test_slider_textbox(void *slider,int position,int readonly,int width,int height){require(slider&&position==0&&readonly==1&&width==0&&height==0,"Slider text box");}
static void test_slider_add(void *slider,void *listener){require(slider&&listener,"Slider add listener");}
static void test_slider_remove(void *slider,void *listener){require(slider&&listener,"Slider remove listener");}
static double test_slider_get(void *){return 0.0;}
static void test_slider_set(void *slider,double,int notify){require(slider&&notify==0,"Slider silent set");slider_silent_updates++;}
static bool test_always_on_top(void *child){return (*(uint32_t*)((unsigned char*)child+0x68)&0x100u)!=0;}
static void order_add(void *parent,void *child){
 if(parent!=ordered_parent)return;require(ordered_count<64,"bounded ordered children");unsigned index=ordered_count;
 if(!test_always_on_top(child))while(index&&test_always_on_top(ordered_children[index-1]))index--;
 for(unsigned i=ordered_count;i>index;i--)ordered_children[i]=ordered_children[i-1];ordered_children[index]=child;ordered_count++;
}
static void order_remove(void *parent,void *child){
 if(parent!=ordered_parent)return;for(unsigned i=0;i<ordered_count;i++)if(ordered_children[i]==child){for(unsigned j=i+1;j<ordered_count;j++)ordered_children[j-1]=ordered_children[j];ordered_count--;return;}require(false,"ordered child removal");
}
static void test_add_child(void *parent,void *child,int z){
 require(parent&&child&&z==-1&&relationship_count<256,"Component add");relationships[relationship_count][0]=parent;relationships[relationship_count++][1]=child;
 *(void**)((unsigned char*)child+0x0c)=parent;order_add(parent,child);if(parent==launcher_owner_component){require(overlay_child_count<256,"bounded overlay children");overlay_children[overlay_child_count++]=child;}child_adds++;
}
static void test_remove_child(void *parent,void *child){
 for(unsigned i=0;i<relationship_count;i++){
  if(relationships[i][0]==parent&&relationships[i][1]==child){
   if(fail_remove_after&&!--fail_remove_after)return;
   relationships[i][1]=nullptr;order_remove(parent,child);*(void**)((unsigned char*)child+0x0c)=nullptr;child_removes++;return;
  }
 }
 require(false,"Component remove");
}
static void *test_tab_remove(void *parent,void *child){test_remove_child(parent,child);tab_removes++;return parent;}
static bool attached(void *parent,void *child){for(unsigned i=0;i<relationship_count;i++)if(relationships[i][0]==parent&&relationships[i][1]==child)return true;return false;}
static void test_bounds(void *object,int x,int y,int w,int h){require(object&&w>0&&h>0&&x>=0&&y>=0&&bounds_count<256,"positive native bounds");bounds_calls[bounds_count++]={object,x,y,w,h};}
static Bounds last_bounds(void *object){for(int i=(int)bounds_count-1;i>=0;i--)if(bounds_calls[i].object==object)return bounds_calls[i];return {};}
static void test_visible(void *object,int visible){require(object&&visible_count<8192,"bounded visibility");visible_objects[visible_count]=object;visible_values[visible_count++]=visible;test_controller_feedback(true);}
static int last_visible(void *object){for(int i=(int)visible_count-1;i>=0;i--)if(visible_objects[i]==object)return visible_values[i];return -1;}
static void test_set_always_on_top(void *object,int enabled){
 require(object&&enabled==1,"launcher always-on-top");*(uint32_t*)((unsigned char*)object+0x68)|=0x100u;
 void *parent=*(void**)((unsigned char*)object+0x0c);if(parent==ordered_parent){order_remove(parent,object);order_add(parent,object);}front_keeps++;
}
static void test_toggle(void *,int selected,int notification){require((selected==0||selected==1)&&notification==0,"selection toggle");toggles++;}
static void test_repaint(void *object){require(object,"repaint object");repaints++;}
static void test_graphics_set_colour(void *graphics,uint32_t colour){require(graphics&&(colour==0xffffffffu||colour==0xffe6e6ebu||colour==0xffbfc0c8u||colour==0xff73757eu),"paint text colour");paint_colour=colour;paint_colours++;}
static void test_graphics_set_font(void *graphics,float height){require(graphics&&(height==22.0f||height==18.0f),"paint font");paint_font=height;paint_fonts++;}
static void test_graphics_fill_all(void *graphics,uint32_t colour){require(graphics&&(colour==0xff202126u||colour==0xff24252au||colour==0xff2f2f34u||colour==0xff595a62u||colour==0xffd31145u),"paint fill");last_fill=colour;paint_fills++;}
static void test_graphics_draw_fitted_text(void *graphics,const void *text,int x,int y,int width,int height,int justification,int lines,float scale){
 require(graphics&&text&&width>0&&height>0&&((x==8&&y==0&&justification==36&&lines==1&&scale==0.0f)||(x==16&&y>=0&&justification==33&&lines>=1&&lines<=2&&scale==1.0f)),"fitted label ABI and opt-in readable rows");last_paint_text=*(const char*const*)text;require(last_paint_text,"paint label String");if(paint_font==22.0f)last_primary_text=last_paint_text;if(drawn_count<8)drawn[drawn_count++]={x,y,width,height,justification,lines,paint_font,scale,paint_colour,last_paint_text};paint_texts++;
}

static void test_tab_ctor(void *tab,uint32_t index,void *scroll,void *parent,float,float,float,float,float,float,void *label,void *selected,void *icon){
 require(index==7&&scroll==(void*)0x11110000&&parent==(void*)0x22220008&&selected==expected_selected&&icon==(void*)0x44440000,"tab constructor ABI");
 require(!strcmp(*(const char**)label,"CONTROLLERS"),"inline tab string");*(void**)tab=tab_vtable_fixture+8;
 static unsigned char content[64],placeholder[16];*(void**)((unsigned char*)tab+0x4c)=content;*(void**)((unsigned char*)tab+0x70)=placeholder;
 relationships[relationship_count][0]=content;relationships[relationship_count++][1]=placeholder;*(void**)(placeholder+0x0c)=content;tab_ctors++;
}
static void test_tab_dtor(void *tab){require(*(void**)tab==tab_vtable_fixture+8,"tab vptr restored");tab_dtors++;}
static void test_tab_active(void *,int){active_calls++;}
static void *test_overlay_add(void *,void *child){test_add_child(launcher_owner_component,child,-1);return launcher_owner_component;}
static void test_overlay_destructor(void *){
 overlay_dtors++;
 if(expect_retained_overlay)require(launcher_instances[0].orphaned&&launcher_instances[0].entry_screen&&launcher_instances[0].context&&!launcher_instances[0].overlay,"failed launcher-entry detach retains inert toolkit ownership after stock owner loss");
 else require(!launcher_instances[0].launcher&&!launcher_instances[0].context,"toolkit children and context precede stock overlay teardown");
}

static unsigned global_target_dtors,global_lookups,global_formats,global_labels,global_string_dtors,global_selects,global_learn_calls;
static unsigned global_file_names,global_imports,global_exports,global_export_writes,global_creates,global_copies,global_file_selects,global_clears,global_modal_reentries;
static unsigned global_message_ctors,global_message_dtors,global_mapping_ctors,global_mapping_commits;
static unsigned global_mapping_frees,global_signal_dtors,global_control_sets,global_reverse_sets;
static unsigned char global_app_storage[0x810],global_vc_storage[0x150],global_owner_storage[0x200],global_file_handler_storage[0xc0],global_chooser_storage[8],global_files[6][0x20],global_mappings[121][0xb0];
static uint32_t global_file_entries[6][2];static uint32_t global_file_count=2;
static const char *const global_file_names_text[6]={"FACTORY MAP","USER TEST","NEW MAP","USER TEST COPY","IMPORTED MAP","SPARE"};
static bool global_import_returns_file,global_import_destination_exists,global_export_returns_file;
static unsigned global_import_copies,global_import_registers,global_import_releases;
static int32_t selected_global_target=-1;static bool requested_global_learn;
static void *test_global_current(void *property){
 require(property==global_owner_storage+0x19c,"captured owner property");return *(void**)((unsigned char*)property+0x50);
}
enum { TARGET_FIXED=1,TARGET_LABEL=2,TARGET_LOOKUP=4,TARGET_SELECT=8,TARGET_CLEAR=16,TARGET_COMMIT=32,TARGET_CONTROL=64,TARGET_REVERSE=128 };
static unsigned native_target_calls[121];
static bool fixture_native_target(int32_t id){
#ifdef MODEL_FIRMWARE_310
 return id>=0&&id<=120&&id!=118;
#else
 return id>=0&&id<120;
#endif
}
static GlobalTarget *test_global_target_fixed(GlobalTarget *out,int32_t id){
 require(out&&fixture_native_target(id),"fixed target range excludes unsupported native IDs");native_target_calls[id]|=TARGET_FIXED;out->id=id;out->word4=0;out->word8=0;return out;
}
static GlobalTarget *test_global_target_dtor(GlobalTarget *target){require(target&&target->word4==0&&target->word8==0,"fixed target lifetime");global_target_dtors++;return target;}
static GlobalLookup *test_global_lookup(GlobalLookup *out,void *property,const GlobalTarget *target){
 require(out&&property==global_owner_storage+0x19c&&target&&fixture_native_target(target->id),"live pairing lookup ABI");
 native_target_calls[target->id]|=TARGET_LOOKUP;global_lookups++;out->mapping=global_mappings[target->id];out->found=1;return out;
}
static HostString *test_global_format(HostString *out,const void *mapping){
 require(out&&mapping,"mapping formatter ABI");unsigned char data1=0;memcpy(&data1,(const unsigned char*)mapping+0xa8,1);
 out->chars=data1==1?"1":data1==13?"13":data1==119?"119":"0";global_formats++;return out;
}
static HostString *test_global_label(HostString *out,uint32_t id){
 require(out&&fixture_native_target((int32_t)id),"target label ABI excludes unsupported native IDs");native_target_calls[id]|=TARGET_LABEL;
#ifdef MODEL_FIRMWARE_310
 if(id==119){out->chars="";global_labels++;return out;}
 if(id==120){out->chars="User Level Velocity";global_labels++;return out;}
#else
 if(id==118){out->chars="";global_labels++;return out;}
 if(id==119){out->chars="User Level Velocity";global_labels++;return out;}
#endif
 out->chars=id==0?"PLAY":id==6?"STOP":id==7?"RECORD":"TARGET";global_labels++;return out;
}
static void test_global_string_dtor(HostString *text){require(text&&text->chars,"host String result lifetime");text->chars=nullptr;global_string_dtors++;}
static HostString *test_global_file_name(HostString *out,const void *file){
 require(out&&file,"selected file name ABI");
 for(unsigned index=0;index<6;index++)if(file==global_files[index]){out->chars=global_file_names_text[index];global_file_names++;return out;}
 require(false,"selected file belongs to handler collection");return out;
}
static void select_global_fixture_file(unsigned index){
 require(index<global_file_count,"bounded native file index");*(void**)(global_file_handler_storage+0x70)=global_files[index];
 global_file_handler_storage[0x9c]=index==0;
}
static void append_global_fixture_file(unsigned index){
 require(index==global_file_count&&index<6,"native file appended once");
 global_file_entries[index][0]=(uint32_t)(uintptr_t)global_files[index];global_file_entries[index][1]=0x70000000u+index;
 global_file_count++;*(uint32_t*)(global_file_handler_storage+0xa8)=(uint32_t)(uintptr_t)(global_file_entries+global_file_count);
}
static void add_global_fixture_file(unsigned index){append_global_fixture_file(index);select_global_fixture_file(index);}
static void test_global_duplicate(void *vc){require(vc==global_vc_storage,"native Copy receiver");global_copies++;add_global_fixture_file(3);}
static void test_global_create(void *vc){require(vc==global_vc_storage,"native New receiver");global_creates++;add_global_fixture_file(2);}
static bool test_global_export_file(void *handler,const void *destination){
 require(handler==global_file_handler_storage&&destination&&
  !strcmp(*(const char*const*)destination,"/sdcard/USER TEST COPY.xmm"),"native Export owner and destination File");
 global_export_writes++;return true;
}
static void test_global_select_file(void *vc,int32_t index){require(vc==global_vc_storage&&index>=0,"native file selection receiver");select_global_fixture_file((unsigned)index);global_file_selects++;}
static void test_global_clear(void *vc,const GlobalTarget *target){
 require(vc==global_vc_storage&&target&&fixture_native_target(target->id)&&!global_file_handler_storage[0x9c],"native clear receiver and User-file restriction");
 native_target_calls[target->id]|=TARGET_CLEAR;memset(global_mappings[target->id]+0x9c,0,14);global_clears++;
}
static void test_global_string_ctor(HostString *out,const char *text){require(out&&text,"host String constructor");out->chars=text;}
static void test_global_chooser_open(HostString *out,void *chooser,const HostString *title,const HostString *initial_path,
 const HostString *initial_name,const HostString *extension,const HostString *filter,const HostString *empty_arg,
 const HostString *description,bool first,bool second,int32_t mode){
 require(out&&chooser==global_chooser_storage&&!strcmp(extension->chars,".xmm")&&
  !strcmp(filter->chars,"*.xmm")&&!empty_arg->chars[0]&&!strcmp(description->chars,"MIDI Mapping File")&&mode==2,
  "qualified synchronous mapping chooser common arguments");
 if(!strcmp(title->chars,"Import MIDI Mapping")){
  require(!initial_path->chars[0]&&!initial_name->chars[0]&&!first&&second,"qualified synchronous Import chooser arguments");
  out->chars=global_import_returns_file?"/sdcard/IMPORTED MAP.xmm":"";global_imports++;
 }else{
  require(!strcmp(title->chars,"Export MIDI Mapping")&&
   !strcmp(initial_path->chars,"/sdcard/MPC Documents/Midi Learn")&&
   !strcmp(initial_name->chars,"USER TEST COPY")&&first&&!second,"qualified synchronous Export chooser arguments");
  out->chars=global_export_returns_file?"/sdcard/USER TEST COPY.xmm":"";global_exports++;
 }
 native_preferences_refresh();global_modal_reentries++;
}
static void *test_global_file_ctor(void *out,const HostString *path){require(out&&path&&path->chars[0],"source File from chooser String");*(const char**)out=path->chars;return out;}
static void test_global_user_directory(void *out){require(out,"user mapping directory result");*(const char**)out="/sdcard/MPC Documents/Midi Learn";}
static HostString *test_global_filename(HostString *out,const void *source){require(out&&source&&*(const char*const*)source,"source basename");out->chars="IMPORTED MAP.xmm";return out;}
static void test_global_utf8(void **out,const HostString *text){require(out&&text&&text->chars,"borrowed UTF-8");*out=(void*)text->chars;}
static void *test_global_child_file(void *out,const void *directory,const char *name){require(out&&directory&&*(const char*const*)directory&&!strcmp(name,"IMPORTED MAP.xmm"),"user-directory child File");*(const char**)out="/sdcard/MPC Documents/Midi Learn/IMPORTED MAP.xmm";return out;}
static bool test_global_exists(const void *file){require(file&&*(const char*const*)file,"File existence predicate");return global_import_destination_exists;}
static bool test_global_exists_as_file(const void *file){require(file&&!strcmp(*(const char*const*)file,"/sdcard/IMPORTED MAP.xmm"),"source regular File predicate");return true;}
static bool test_global_copy_file(const void *source,const void *destination){require(source&&destination,"byte-copy File operands");global_import_copies++;return true;}
static void test_global_open_mapping(GlobalSharedFile *out,const void *destination){require(out&&destination,"open copied mapping");out->object=global_files[4];out->control=malloc(8);require(out->control,"temporary shared mapping control");}
static bool test_global_register_mapping(void *handler,const GlobalSharedFile *file){require(handler==global_file_handler_storage&&file&&file->object==global_files[4]&&file->control,"handler mapping registration");append_global_fixture_file(4);global_import_registers++;return true;}
static void test_global_select_handler(void *handler,int32_t index){require(handler==global_file_handler_storage&&index>=0,"handler file selection");select_global_fixture_file((unsigned)index);global_file_selects++;}
static void test_global_release(void *control){require(control,"temporary shared-reference release");free(control);global_import_releases++;}
struct TestMidiMessage {int32_t type,channel,data;unsigned char rest[12];};
static void test_note_message(void *out,int32_t channel,int32_t note,uint32_t velocity){
 require(out&&channel>=1&&channel<=16&&note>=0&&note<=127&&velocity==127,"native note construction contract");
 *(TestMidiMessage*)out={1,channel,note,{}};global_message_ctors++;
}
static void test_cc_message(void *out,int32_t channel,int32_t controller,uint32_t value){
 require(out&&channel>=1&&channel<=16&&controller>=0&&controller<=127&&value==0,"native CC construction contract");
 *(TestMidiMessage*)out={2,channel,controller,{}};global_message_ctors++;
}
static void test_pitch_message(void *out,int32_t channel,int32_t value){
 require(out&&channel>=1&&channel<=16&&value==8192,"native pitch construction contract");
 *(TestMidiMessage*)out={3,channel,0,{}};global_message_ctors++;
}
static void test_message_dtor(void *message){require(message,"native message lifetime");global_message_dtors++;}
static void *test_mapping_ctor(void *out,const void *message){
 require(out&&message,"native Mapping construction contract");memset(out,0,172);
 const TestMidiMessage &value=*(const TestMidiMessage*)message;
 memcpy((unsigned char*)out+0x9c,&value.type,4);memcpy((unsigned char*)out+0xa4,&value.channel,4);
 memcpy((unsigned char*)out+0xa8,&value.data,1);*(void**)((unsigned char*)out+0x88)=malloc(8);
 require(*(void**)((unsigned char*)out+0x88)!=nullptr,"fixture Mapping allocation");global_mapping_ctors++;return out;
}
static void test_commit_mapping(void *collection,const GlobalTarget *target,const void *mapping){
 require(collection==global_owner_storage+0x19c&&target&&fixture_native_target(target->id)&&mapping,"native Mapping commit owner");
 native_target_calls[target->id]|=TARGET_COMMIT;memcpy(global_mappings[target->id]+0x9c,(const unsigned char*)mapping+0x9c,4);
 memcpy(global_mappings[target->id]+0xa4,(const unsigned char*)mapping+0xa4,4);
 memcpy(global_mappings[target->id]+0xa8,(const unsigned char*)mapping+0xa8,1);global_mapping_commits++;
}
static void test_set_control(void *collection,const GlobalTarget *target,int32_t control){
 require(collection==global_owner_storage+0x19c&&target&&fixture_native_target(target->id)&&control>=1&&control<=7,"native control-mode setter");
 native_target_calls[target->id]|=TARGET_CONTROL;memcpy(global_mappings[target->id]+0xa0,&control,4);global_control_sets++;
}
static void test_set_reverse(void *collection,const GlobalTarget *target,bool reverse){
 require(collection==global_owner_storage+0x19c&&target&&fixture_native_target(target->id),"native reverse setter");
 native_target_calls[target->id]|=TARGET_REVERSE;unsigned char value=reverse;memcpy(global_mappings[target->id]+0xa9,&value,1);global_reverse_sets++;
}
static void test_signal_dtor(void *signal){require(signal,"embedded Mapping signal lifetime");global_signal_dtors++;}
static void test_mapping_free(void *block){require(block,"Mapping-owned allocation");free(block);global_mapping_frees++;}
static void test_global_select(void *property,const GlobalTarget *target){
 require(property==global_owner_storage+0x38&&target&&fixture_native_target(target->id),"stock selected Target property");native_target_calls[target->id]|=TARGET_SELECT;selected_global_target=target->id;global_selects++;
}
static void test_global_set_learn(void *vc,bool enabled){
 require(vc==global_vc_storage,"captured view-controller Learn intent");requested_global_learn=enabled;
 __atomic_add_fetch((uint32_t*)(global_vc_storage+0x10),1u,__ATOMIC_RELEASE);global_learn_calls++;
}
static void __attribute__((unused)) settle_global_learn(bool enabled){
 require(requested_global_learn==enabled,"settle requested Learn direction");global_owner_storage[0x34]=enabled;global_vc_storage[0x23]=enabled;
 __atomic_store_n((uint32_t*)(global_vc_storage+0x10),0u,__ATOMIC_RELEASE);
}

static void click(void *button){void **slots=*(void***)button;auto callback=(void(*)(void*))slots[46];require(callback,"copied click slot");callback(button);}
static void paint(void *button){void **slots=*(void***)button;auto callback=(void(*)(void*,void*,int,int))slots[48];require(callback,"copied paint slot");callback(button,(void*)0x77770000,0,0);}
static void __attribute__((unused)) select_list(void *list,int row){test_list_select(list,row,0,1);}
static void __attribute__((unused)) paint_list(void *list,int row,int selected,int width,int height){drawn_count=0;void *model=*(void**)((unsigned char*)list+0x78);require(model,"live ListBox presentation model");auto callback=(void(*)(void*,int,void*,int,int,int))(*(void***)model)[3];require(callback,"copied ListBox paint slot");callback(model,row,(void*)0x77770000,width,height,selected);}

#ifdef MPCLEARN_CONTROLLERS_CAPTURE
extern "C" int snprintf(char *,size_t,const char *,...);
static void capture_text(const char *text){for(unsigned n=0;text&&text[n];n++){if(text[n]=='\n')printf("\\n");else printf("%c",text[n]);}printf("\n");}
static void capture_box(const char *kind,Bounds b,uint32_t colour){require(b.x>=0&&b.y>=0&&b.x+b.w<=948&&b.y+b.h<=628,"production Controllers bounds fit retained native content region");printf("BOX\t%s\t%d\t%d\t%d\t%d\t%08x\n",kind,b.x,b.y,b.w,b.h,colour);}
static void capture_draws(Bounds b){for(unsigned n=0;n<drawn_count;n++){auto &d=drawn[n];printf("TEXT\tpaint\t%d\t%d\t%d\t%d\t%08x\t%.0f\t%d\t%d\t%.1f\t",b.x+d.x,b.y+d.y,d.w,d.h,d.colour,d.font,d.justification,d.lines,d.scale);capture_text(d.text);}}
static void layout_scene(const char *name){
 auto &i=preferences_instances[0];printf("SCENE\t%s\t948\t628\n",name);
 MpcUiList *lists[]={i.controllers,i.profiles,i.inputs,i.outputs};
 for(auto *list:lists){void *object=mpc_ui_private_list_host_object(list);if(last_visible(object)!=1)continue;Bounds b=last_bounds(object);capture_box("list",b,0xff2f2f34u);
  void *model=*(void**)((unsigned char*)object+0x78);int count=((int(*)(void*))(*(void***)model)[2])(model);int height=0,selected=*(int*)((unsigned char*)object+0xa4);
  for(unsigned n=0;n<row_height_count;n++)if(row_heights[n].object==object)height=row_heights[n].height;require(height>0,"captured actual production row height");
  for(int row=0;row<count&&(row+1)*height<=b.h;row++){paint_list(object,row,row==selected,b.w,height);Bounds rb={object,b.x,b.y+row*height,b.w,height};capture_box("row",rb,last_fill);capture_draws(rb);}
 }
 MpcUiControl *buttons[]={i.add,i.detect,i.edit,i.toggle,i.remove,i.refresh,i.apply,i.cancel,i.policy[0],i.policy[1]};int y=-1,min=948,max=0;Bounds placed[10];unsigned used=0;
 for(auto *control:buttons){void *object=mpc_ui_private_control_host_object(control);if(last_visible(object)!=1)continue;Bounds b=last_bounds(object);if(y<0)y=b.y;require(y==b.y,"visible actions share one row");for(unsigned n=0;n<used;n++)require(b.x>=placed[n].x+placed[n].w||b.x+b.w<=placed[n].x,"visible actions do not overlap");placed[used++]=b;if(b.x<min)min=b.x;if(b.x+b.w>max)max=b.x+b.w;drawn_count=0;paint(object);capture_box("button",b,last_fill);capture_draws(b);}
 require(min==948-max,"action row has balanced native-parent margins");
 MpcUiControl *labels[]={i.status_label,i.editor_title,i.input_label,i.output_label,i.from_detail,i.to_detail,i.empty_label};const char *texts[]={i.status_text,i.editor_text,"From controller","To controller",i.connection_text[0],i.connection_text[1],"No controllers yet\nChoose Detect or Add to connect a controller."};
 for(unsigned n=0;n<7;n++){void *object=mpc_ui_private_control_host_object(labels[n]);if(last_visible(object)!=1)continue;Bounds b=last_bounds(object);capture_box("label",b,0xff202126u);int align=1;for(unsigned a=0;a<label_alignment_count;a++)if(label_alignments[a].object==object)align=label_alignments[a].value;printf("TEXT\tlabel-substitute\t%d\t%d\t%d\t%d\tffffffff\t15\t%d\t2\t1.0\t",b.x,b.y,b.w,b.h,align);capture_text(texts[n]);}
 printf("END\t%s\n",name);
}
static void capture_controller_layout(NativePreferencesState &fixture){
 NativePreferencesState saved=fixture;fixture.collection_count=4;fixture.collection_generation++;
 for(unsigned n=1;n<4;n++)fixture.controllers[n]=fixture.controllers[0];
 fixture.controllers[1].profile=NATIVE_CONTROLLER_LPP3;fixture.controllers[2].profile=NATIVE_CONTROLLER_LCXL3;fixture.controllers[3].profile=NATIVE_CONTROLLER_HUI;
 fixture.controllers[1].status=NATIVE_CONTROLLER_DISCONNECTED;fixture.controllers[2].enabled=0;fixture.controllers[2].status=NATIVE_CONTROLLER_DISABLED;
 const char *clients[]={"X-Touch Mini","Launchpad Pro Mk3","Launch Control XL 3","HUI surface"};const char *ports[]={"X-TOUCH MINI MIDI 1","Launchpad Pro Mk3 MIDI 2","Launch Control XL 3 DAW","HUI DAW"};
 for(unsigned n=0;n<4;n++){auto &row=fixture.controllers[n];row.input_client_length=row.output_client_length=snprintf((char*)row.input_client,sizeof(row.input_client),"%s",clients[n]);snprintf((char*)row.output_client,sizeof(row.output_client),"%s",clients[n]);row.input_port_length=row.output_port_length=snprintf((char*)row.input_port,sizeof(row.input_port),"%s",ports[n]);snprintf((char*)row.output_port,sizeof(row.output_port),"%s",ports[n]);}
 fixture.admission.collection_generation=fixture.collection_generation;fixture.admission.count=4;fixture.admission.status[0]=NATIVE_CONTROLLER_ACTIVE;fixture.admission.status[1]=NATIVE_CONTROLLER_DISCONNECTED;fixture.admission.status[2]=NATIVE_CONTROLLER_DISABLED;fixture.admission.status[3]=NATIVE_CONTROLLER_READY;fixture.admission.revision+=2;
 native_preferences_refresh();refresh_selection(&preferences_instances[0]);select_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),1);layout_scene("main-populated");
 fixture.collection_count=0;fixture.collection_generation++;refresh_selection(&preferences_instances[0]);layout_scene("main-empty");
 unsigned no_selection_request=fixture.request_sequence;click(mpc_ui_private_control_host_object(preferences_instances[0].edit));click(mpc_ui_private_control_host_object(preferences_instances[0].toggle));click(mpc_ui_private_control_host_object(preferences_instances[0].remove));require(!preferences_instances[0].editing&&fixture.request_sequence==no_selection_request,"dimmed no-selection actions remain inert through existing input guards");paint(mpc_ui_private_control_host_object(preferences_instances[0].edit));require(last_fill==0xff24252au&&paint_colour==0xff73757eu,"empty-state Edit is visibly dimmed at the paint boundary");
 fixture=saved;refresh_selection(&preferences_instances[0]);select_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),0);paint(mpc_ui_private_control_host_object(preferences_instances[0].edit));require(last_fill==0xff2f2f34u&&paint_colour==0xffffffffu,"selecting a controller restores ordinary action styling");
 // Retained native inventory from 2026-10-14; not a current connected claim.
 fixture.inventory_count=12;fixture.inventory_generation++;memset(fixture.endpoints,0,sizeof(fixture.endpoints));
 fixture.endpoints[0].endpoint_type=1;fixture.endpoints[0].capabilities=3;fixture.endpoints[0].client_number=24;fixture.endpoints[0].port_number=0;snprintf((char*)fixture.endpoints[0].client,sizeof(fixture.endpoints[0].client),"%s","Launchpad Pro MK3");snprintf((char*)fixture.endpoints[0].port,sizeof(fixture.endpoints[0].port),"%s","LPProMK3 MIDI");
 fixture.endpoints[1].endpoint_type=1;fixture.endpoints[1].capabilities=3;fixture.endpoints[1].client_number=24;fixture.endpoints[1].port_number=1;snprintf((char*)fixture.endpoints[1].client,sizeof(fixture.endpoints[1].client),"%s","Launchpad Pro MK3");snprintf((char*)fixture.endpoints[1].port,sizeof(fixture.endpoints[1].port),"%s","LPProMK3 DIN");
 fixture.endpoints[2].endpoint_type=1;fixture.endpoints[2].capabilities=3;fixture.endpoints[2].client_number=24;fixture.endpoints[2].port_number=2;snprintf((char*)fixture.endpoints[2].client,sizeof(fixture.endpoints[2].client),"%s","Launchpad Pro MK3");snprintf((char*)fixture.endpoints[2].port,sizeof(fixture.endpoints[2].port),"%s","LPProMK3 DAW");
 fixture.endpoints[3].endpoint_type=1;fixture.endpoints[3].capabilities=3;fixture.endpoints[3].client_number=28;fixture.endpoints[3].port_number=0;snprintf((char*)fixture.endpoints[3].client,sizeof(fixture.endpoints[3].client),"%s","SQ-64");snprintf((char*)fixture.endpoints[3].port,sizeof(fixture.endpoints[3].port),"%s","SQ-64 _ MIDI OUT 1");
 fixture.endpoints[4].endpoint_type=1;fixture.endpoints[4].capabilities=3;fixture.endpoints[4].client_number=28;fixture.endpoints[4].port_number=1;snprintf((char*)fixture.endpoints[4].client,sizeof(fixture.endpoints[4].client),"%s","SQ-64");snprintf((char*)fixture.endpoints[4].port,sizeof(fixture.endpoints[4].port),"%s","SQ-64 _ MIDI OUT 2");
 fixture.endpoints[5].endpoint_type=1;fixture.endpoints[5].capabilities=2;fixture.endpoints[5].client_number=28;fixture.endpoints[5].port_number=2;snprintf((char*)fixture.endpoints[5].client,sizeof(fixture.endpoints[5].client),"%s","SQ-64");snprintf((char*)fixture.endpoints[5].port,sizeof(fixture.endpoints[5].port),"%s","SQ-64 _ SEQ");
 fixture.endpoints[6].endpoint_type=1;fixture.endpoints[6].capabilities=3;fixture.endpoints[6].client_number=32;fixture.endpoints[6].port_number=0;snprintf((char*)fixture.endpoints[6].client,sizeof(fixture.endpoints[6].client),"%s","Launchpad X");snprintf((char*)fixture.endpoints[6].port,sizeof(fixture.endpoints[6].port),"%s","LPX DAW In");
 fixture.endpoints[7].endpoint_type=1;fixture.endpoints[7].capabilities=3;fixture.endpoints[7].client_number=32;fixture.endpoints[7].port_number=1;snprintf((char*)fixture.endpoints[7].client,sizeof(fixture.endpoints[7].client),"%s","Launchpad X");snprintf((char*)fixture.endpoints[7].port,sizeof(fixture.endpoints[7].port),"%s","LPX MIDI In");
 fixture.endpoints[8].endpoint_type=1;fixture.endpoints[8].capabilities=3;fixture.endpoints[8].client_number=36;fixture.endpoints[8].port_number=0;snprintf((char*)fixture.endpoints[8].client,sizeof(fixture.endpoints[8].client),"%s","LCXL3 1");snprintf((char*)fixture.endpoints[8].port,sizeof(fixture.endpoints[8].port),"%s","LCXL3 1 MIDI In");
 fixture.endpoints[9].endpoint_type=1;fixture.endpoints[9].capabilities=3;fixture.endpoints[9].client_number=36;fixture.endpoints[9].port_number=1;snprintf((char*)fixture.endpoints[9].client,sizeof(fixture.endpoints[9].client),"%s","LCXL3 1");snprintf((char*)fixture.endpoints[9].port,sizeof(fixture.endpoints[9].port),"%s","LCXL3 1 DAW In");
 fixture.endpoints[10].endpoint_type=1;fixture.endpoints[10].capabilities=2;fixture.endpoints[10].client_number=36;fixture.endpoints[10].port_number=2;snprintf((char*)fixture.endpoints[10].client,sizeof(fixture.endpoints[10].client),"%s","LCXL3 1");snprintf((char*)fixture.endpoints[10].port,sizeof(fixture.endpoints[10].port),"%s","LCXL3 1 To DIN Out");
 fixture.endpoints[11].endpoint_type=1;fixture.endpoints[11].capabilities=2;fixture.endpoints[11].client_number=36;fixture.endpoints[11].port_number=3;snprintf((char*)fixture.endpoints[11].client,sizeof(fixture.endpoints[11].client),"%s","LCXL3 1");snprintf((char*)fixture.endpoints[11].port,sizeof(fixture.endpoints[11].port),"%s","LCXL3 1 To DIN Out 2");
 refresh_selection(&preferences_instances[0]);click(mpc_ui_private_control_host_object(preferences_instances[0].add));select_list(mpc_ui_private_list_host_object(preferences_instances[0].profiles),NATIVE_CONTROLLER_LCXL3-1);layout_scene("editor");
 select_list(mpc_ui_private_list_host_object(preferences_instances[0].profiles),NATIVE_CONTROLLER_HUI-1);layout_scene("hui-editor");
 for(unsigned n=0;n<fixture.inventory_count;n++){snprintf((char*)fixture.endpoints[n].client,sizeof(fixture.endpoints[n].client),"Launchpad Pro Mk3 external controller");snprintf((char*)fixture.endpoints[n].port,sizeof(fixture.endpoints[n].port),"Long physical controller MIDI input/output port number %u",n+1);}
 fixture.inventory_generation++;fixture.status=NATIVE_PREFERENCES_INVALID;fixture.inventory_status=NATIVE_PREFERENCES_INVALID;refresh_selection(&preferences_instances[0]);layout_scene("long-ports");
 click(mpc_ui_private_control_host_object(preferences_instances[0].cancel));fixture=saved;refresh_selection(&preferences_instances[0]);
}
#endif

struct Frame {Context context;unsigned char stack[0x240];};
static void setup_preferences_frame(Frame &frame,unsigned char *overlay){
 memset(&frame,0,sizeof(frame));frame.context.r[11]=(uint32_t)(uintptr_t)overlay;
#ifdef MODEL_FIRMWARE_310
 /* Native constructor receipt: distinct old slots catch stale frame reads. */
 *(uint32_t*)(frame.stack+0x68)=(uint32_t)(uintptr_t)(overlay+0x164);*(uint32_t*)(frame.stack+0x50)=0x44440004;*(uint32_t*)(frame.stack+0x5c)=77;
 *(uint32_t*)(frame.stack+0x64)=(uint32_t)(uintptr_t)(overlay+0x160);*(uint32_t*)(frame.stack+0x4c)=0x44440000;*(uint32_t*)(frame.stack+0x58)=7;
#else
 *(uint32_t*)(frame.stack+0x68)=(uint32_t)(uintptr_t)(overlay+0x160);*(uint32_t*)(frame.stack+0x50)=0x44440000;*(uint32_t*)(frame.stack+0x5c)=7;
#endif
 const unsigned offsets[5]={0x198,0x194,0x1a0,0x1a4,0x19c};for(unsigned n=0;n<5;n++){float v=0.25f+n;memcpy(frame.stack+offsets[n],&v,4);}float sixth=5.25f;memcpy(overlay+0x1c8,&sixth,4);
}
static void setup_outer_layout(Frame &frame,void *page){
 memset(&frame,0,sizeof(frame));uint32_t begin=*(uint32_t*)((unsigned char*)page+0xc8),end=*(uint32_t*)((unsigned char*)page+0xcc);
 if(begin==end)frame.context.r[5]=(uint32_t)(uintptr_t)page;else frame.context.r[4]=frame.context.r[5]=end;
}

#include "native-preferences-resolver-test.inc"

#include "sampler-banner-test.inc"
int main(){
 sampler_banner_checks();
 native_resolver_checks();
 NativePreferencesState fixture={};state=&fixture;
 ((void**)tab_vtable_fixture)[2]=(void*)(uintptr_t)test_tab_dtor;((void**)tab_vtable_fixture)[4]=(void*)(uintptr_t)test_tab_active;
 ((void**)overlay_vtable_fixture)[2]=(void*)(uintptr_t)test_overlay_destructor;((void**)overlay_vtable_fixture)[3]=(void*)(uintptr_t)test_overlay_destructor;
 static const MpcUiPrivateHost host={test_allocate,test_delete,test_string_ctor,test_string_dtor,test_set_colour,test_button_ctor,test_button_dtor,
  test_label_ctor,test_label_dtor,test_label_text,test_label_justification,test_mouse_intercepts,
  test_list_ctor,test_list_dtor,test_list_set_model,test_list_update,test_list_row_height,test_list_select,
  test_list_get_viewport,test_viewport_set_drag,
  test_slider_ctor,test_slider_dtor,test_slider_style,test_slider_range,test_slider_textbox,
  test_slider_add,test_slider_remove,test_slider_get,test_slider_set,
  test_add_child,test_remove_child,test_bounds,test_visible,test_set_always_on_top,test_toggle,test_repaint,
  test_graphics_set_colour,test_graphics_set_font,test_graphics_fill_all,test_graphics_draw_fitted_text,button_vtable_fixture,list_model_vtable_fixture,
  nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,
  nullptr,nullptr,nullptr,nullptr,nullptr,
  nullptr,nullptr,nullptr,nullptr,nullptr};
 toolkit_component_host=&host;core={test_allocate,test_delete,test_visible,test_repaint};
 preferences={test_std_string,test_tab_ctor,test_tab_dtor,test_tab_remove,tab_vtable_fixture};
 launcher={test_overlay_add,test_remove_child,test_overlay_destructor,test_overlay_destructor,overlay_vtable_fixture};
 global_host={test_global_current,test_global_target_fixed,test_global_target_dtor,test_global_lookup,test_global_format,test_global_label,test_global_string_dtor,test_global_select,test_global_set_learn,
  test_global_file_name,test_global_duplicate,test_global_create,test_global_export_file,test_global_select_file,test_global_clear,
  test_note_message,test_cc_message,test_pitch_message,test_message_dtor,test_mapping_ctor,test_commit_mapping,
  test_set_control,test_set_reverse,test_signal_dtor,test_mapping_free,
  test_global_string_ctor,test_global_file_ctor,test_global_user_directory,test_global_user_directory,test_global_filename,test_global_utf8,
  test_global_child_file,test_global_exists,test_global_exists_as_file,test_global_copy_file,test_global_open_mapping,
  test_global_register_mapping,test_global_select_handler,test_global_release,test_global_chooser_open};

#ifdef MODEL_FIRMWARE_310
 for(unsigned index=0;index<6;index++)*(void**)global_files[index]=(void*)(uintptr_t)0x06ca3b54u;
#else
 for(unsigned index=0;index<6;index++)*(void**)global_files[index]=(void*)(uintptr_t)0x0689c878u;
#endif
 for(unsigned index=0;index<global_file_count;index++){global_file_entries[index][0]=(uint32_t)(uintptr_t)global_files[index];global_file_entries[index][1]=0x70000000u+index;}
 *(uint32_t*)(global_file_handler_storage+0xa4)=(uint32_t)(uintptr_t)global_file_entries;
 *(uint32_t*)(global_file_handler_storage+0xa8)=(uint32_t)(uintptr_t)(global_file_entries+global_file_count);select_global_fixture_file(1);
#ifdef MODEL_FIRMWARE_310
 *(void**)(global_app_storage+0x3c0)=global_vc_storage;
 *(void**)(global_app_storage+0x3e8)=global_owner_storage;
#else
 *(void**)(global_app_storage+0x3c0)=global_owner_storage;
#endif
 *(void**)(global_vc_storage+0x138)=global_owner_storage;*(void**)(global_vc_storage+0x13c)=global_file_handler_storage;
 *(void**)(global_vc_storage+0x130)=global_chooser_storage;
 *(void**)(global_owner_storage+0x19c+0x50)=global_file_handler_storage;
 int32_t type=2,control=5,channel=1;unsigned char data1=13;memcpy(global_mappings[0]+0x9c,&type,4);memcpy(global_mappings[0]+0xa0,&control,4);memcpy(global_mappings[0]+0xa4,&channel,4);memcpy(global_mappings[0]+0xa8,&data1,1);
 type=1;control=3;channel=16;data1=119;memcpy(global_mappings[6]+0x9c,&type,4);memcpy(global_mappings[6]+0xa0,&control,4);memcpy(global_mappings[6]+0xa4,&channel,4);memcpy(global_mappings[6]+0xa8,&data1,1);
 type=3;control=5;channel=2;data1=0;unsigned char reverse=1;memcpy(global_mappings[7]+0x9c,&type,4);memcpy(global_mappings[7]+0xa0,&control,4);memcpy(global_mappings[7]+0xa4,&channel,4);memcpy(global_mappings[7]+0xa8,&data1,1);memcpy(global_mappings[7]+0xa9,&reverse,1);

 alignas(8) unsigned char preferences_overlay[0x200]={};*(uint32_t*)(preferences_overlay+0x1a4)=0x11110000;*(uint32_t*)(preferences_overlay+0x1a8)=0x22220000;
 fixture.inventory_generation=1;fixture.inventory_count=3;fixture.inventory_status=NATIVE_PREFERENCES_ACTIVE;
 fixture.endpoints[0].endpoint_type=NATIVE_ENDPOINT_KERNEL;fixture.endpoints[0].capabilities=NATIVE_ENDPOINT_FROM_CONTROLLER;fixture.endpoints[0].client_number=24;fixture.endpoints[0].port_number=0;memcpy(fixture.endpoints[0].client,"Physical HUI",13);memcpy(fixture.endpoints[0].port,"DAW In",7);
 fixture.endpoints[1].endpoint_type=NATIVE_ENDPOINT_USER;fixture.endpoints[1].capabilities=NATIVE_ENDPOINT_TO_CONTROLLER;fixture.endpoints[1].client_number=130;fixture.endpoints[1].port_number=1;memcpy(fixture.endpoints[1].client,"Software HUI",13);memcpy(fixture.endpoints[1].port,"DAW Out",8);
 fixture.endpoints[2].endpoint_type=NATIVE_ENDPOINT_KERNEL;fixture.endpoints[2].capabilities=NATIVE_ENDPOINT_FROM_CONTROLLER|NATIVE_ENDPOINT_TO_CONTROLLER;fixture.endpoints[2].client_number=25;fixture.endpoints[2].port_number=0;memcpy(fixture.endpoints[2].client,"X-TOUCH MINI",13);memcpy(fixture.endpoints[2].port,"X-TOUCH MINI MIDI 1",20);
 fixture.collection_generation=1;fixture.collection_count=1;fixture.active_controller_index=0;
 fixture.controller_lifetime=1;fixture.admission.lifetime=1;fixture.admission.collection_generation=1;fixture.admission.count=1;fixture.admission.status[0]=NATIVE_CONTROLLER_ACTIVE;
 fixture.controllers[0].enabled=1;fixture.controllers[0].profile=NATIVE_CONTROLLER_XTOUCH_MINI;fixture.controllers[0].status=NATIVE_CONTROLLER_ACTIVE;fixture.controllers[0].input_type=fixture.controllers[0].output_type=NATIVE_ENDPOINT_KERNEL;
 fixture.controllers[0].input_client_length=fixture.controllers[0].output_client_length=12;fixture.controllers[0].input_port_length=fixture.controllers[0].output_port_length=19;
 memcpy(fixture.controllers[0].input_client,"X-TOUCH MINI",12);memcpy(fixture.controllers[0].output_client,"X-TOUCH MINI",12);memcpy(fixture.controllers[0].input_port,"X-TOUCH MINI MIDI 1",19);memcpy(fixture.controllers[0].output_port,"X-TOUCH MINI MIDI 1",19);
 uint32_t *old=(uint32_t*)test_allocate(4);old[0]=0x55550000;*(uint32_t*)(preferences_overlay+0x1b8)=(uint32_t)(uintptr_t)old;*(uint32_t*)(preferences_overlay+0x1bc)=(uint32_t)(uintptr_t)(old+1);*(uint32_t*)(preferences_overlay+0x1c0)=(uint32_t)(uintptr_t)(old+1);
 Frame frame;memset(&frame,0,sizeof(frame));frame.context.r[4]=(uint32_t)(uintptr_t)global_app_storage;frame.context.r[5]=(uint32_t)(uintptr_t)global_vc_storage;
 frame.context.r[6]=(uint32_t)(uintptr_t)global_file_handler_storage;frame.context.r[7]=(uint32_t)(uintptr_t)global_owner_storage;native_preferences_hook(&frame.context,3);
#ifdef MODEL_FIRMWARE_310
 *(void**)(global_app_storage+0x7cc)=global_owner_storage;
 *(void**)(global_app_storage+0x800)=global_vc_storage;
#else
 *(void**)(global_app_storage+0x7cc)=global_vc_storage;
#endif
 require(fixture.global_owner_captures==1&&fixture.last_global_owner==(uint32_t)(uintptr_t)global_owner_storage&&fixture.last_global_file==(uint32_t)(uintptr_t)global_file_handler_storage&&!fixture.failures,"startup hook captures qualified Global MIDI Learn owner and file handler");
 expected_selected=preferences_overlay+0x160;setup_preferences_frame(frame,preferences_overlay);native_preferences_hook(&frame.context,0);
#ifdef MODEL_FIRMWARE_310
 require(*(uint32_t*)(frame.stack+0x58)==8&&*(uint32_t*)(frame.stack+0x5c)==77,"native 3.10 index advances only traced frame slot");
#else
 require(*(uint32_t*)(frame.stack+0x5c)==8,"native 3.9.1 index advances traced frame slot");
#endif
 require(fixture.tabs_created==1&&fixture.buttons_created==10&&fixture.live_tabs==1&&!fixture.failures&&colour_sets>=38&&preferences_instances[0].endpoint_count==3&&preferences_instances[0].controller_count==1,"Controllers composes collection, split endpoint lists, policy and status through canonical toolkit");
 require(tab_removes==1,"stock placeholder detached once");
 void *tab=(void*)(uintptr_t)fixture.last_tab;void **tab_slots=*(void***)tab;fixture.saved_profile=fixture.active_profile=NATIVE_CONTROLLER_XTOUCH_MINI;fixture.status=NATIVE_PREFERENCES_ACTIVE;
 ((void(*)(void*,int))tab_slots[2])(tab,1);require(active_calls==1&&!fixture.failures,"stock-first activation refresh");
 require(strstr(preferences_instances[0].controller_text[0],"X-Touch Mini")&&strstr(preferences_instances[0].controller_detail[0],"Connected"),"collection row presents enabled state, profile and status");
#ifdef MPCLEARN_CONTROLLERS_CAPTURE
 capture_controller_layout(fixture);
#endif
 unsigned controller_paints=paint_texts;
 paint_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),0,1,900,72);
 require(paint_texts==controller_paints+2&&strstr(last_primary_text,"X-Touch Mini"),"nonempty Controllers collection paints its actual row through the production model");
 require(!strcmp(last_primary_text,"X-Touch Mini")&&!strcmp(last_paint_text,"Connected"),"main row paints controller name and state separately without a port dump");

 NativePreferencesController saved_row=fixture.controllers[0];fixture.controllers[0].profile=NATIVE_CONTROLLER_LPP3;
 const uint32_t admissions[]={NATIVE_CONTROLLER_WAIT_MODE,NATIVE_CONTROLLER_UNKNOWN_MODE,NATIVE_CONTROLLER_ACTIVE};const char *labels[]={"Waiting for controller mode","Controller mode changed","Connected"};
 for(unsigned i=0;i<3;i++){fixture.admission.status[0]=admissions[i];fixture.admission.revision+=2;native_preferences_refresh();paint_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),0,1,900,72);require(strstr(last_primary_text,"Launchpad Pro Mk3")&&strstr(last_paint_text,labels[i]),"actual Controllers row renders waiting, inactive mode reason, then delayed admission");}
 fixture.controller_lifetime++;native_preferences_refresh();paint_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),0,1,900,72);require(strstr(last_paint_text,"Bridge not running"),"same-MPC bridge exit invalidates rendered ACTIVE");
 fixture.controllers[0]=saved_row;fixture.admission.lifetime=fixture.controller_lifetime;fixture.admission.status[0]=NATIVE_CONTROLLER_ACTIVE;controller_paints=paint_texts;
 fixture.controllers[0].status=NATIVE_CONTROLLER_DISCONNECTED;fixture.collection_generation++;native_preferences_refresh();
 paint_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),0,1,900,72);
 require(paint_texts==controller_paints+2&&!strcmp(last_paint_text,"Disconnected"),"same-count disconnected collection refresh paints the new secondary state");
 fixture.controllers[0].status=NATIVE_CONTROLLER_ACTIVE;fixture.collection_generation++;native_preferences_refresh();
 NativePreferencesEndpoint short_endpoint=fixture.endpoints[0];
 memset(fixture.endpoints[0].port,'P',sizeof(fixture.endpoints[0].port));fixture.inventory_generation++;refresh_selection(&preferences_instances[0]);
 unsigned long_endpoint_paints=paint_texts;
 paint_list(mpc_ui_private_list_host_object(preferences_instances[0].inputs),0,0,296,96);
 require(paint_texts==long_endpoint_paints+2&&!strcmp(last_paint_text,"USB 24:0")&&strstr(last_primary_text,"..."),"maximum-length endpoint paints bounded primary text and separate address detail");
 require(!memcmp(preferences_instances[0].endpoint_rows[0].port,fixture.endpoints[0].port,sizeof(fixture.endpoints[0].port)),"display truncation never truncates the endpoint identity used for selection");
 fixture.endpoints[0]=short_endpoint;fixture.inventory_generation=1;refresh_selection(&preferences_instances[0]);
 click(mpc_ui_private_control_host_object(preferences_instances[0].add));require(preferences_instances[0].editing&&preferences_instances[0].editing_index==NATIVE_CONTROLLER_NEW&&last_visible(mpc_ui_private_list_host_object(preferences_instances[0].controllers))==0,"Add opens the bounded controller editor");
 select_list(mpc_ui_private_list_host_object(preferences_instances[0].profiles),3);select_list(mpc_ui_private_list_host_object(preferences_instances[0].inputs),0);select_list(mpc_ui_private_list_host_object(preferences_instances[0].outputs),0);
 require(last_visible(mpc_ui_private_control_host_object(preferences_instances[0].policy[0]))==1,"selecting HUI immediately exposes Motorized and Pickup");
 Bounds motor=last_bounds(mpc_ui_private_control_host_object(preferences_instances[0].policy[0])),pickup=last_bounds(mpc_ui_private_control_host_object(preferences_instances[0].policy[1]));
 require(motor.w>pickup.w&&motor.x+motor.w<pickup.x,"Motorized retains wider text space without overlapping Pickup");
 unsigned endpoint_paints=paint_texts;
 paint_list(mpc_ui_private_list_host_object(preferences_instances[0].inputs),0,1,296,96);
 require(paint_texts==endpoint_paints+2&&!strcmp(last_primary_text,"DAW In")&&!strcmp(last_paint_text,"USB 24:0"),"From row paints the exact endpoint type/address and port name");
 paint_list(mpc_ui_private_list_host_object(preferences_instances[0].outputs),0,1,296,96);
 require(paint_texts==endpoint_paints+4&&!strcmp(last_primary_text,"DAW Out")&&!strcmp(last_paint_text,"App 130:1"),"To row paints its independent endpoint type/address and port name");
 click(mpc_ui_private_control_host_object(preferences_instances[0].policy[0]));click(mpc_ui_private_control_host_object(preferences_instances[0].apply));
 require(fixture.request_sequence==1&&fixture.request_kind==NATIVE_REQUEST_APPLY&&fixture.request_controller_index==NATIVE_CONTROLLER_NEW&&fixture.request_profile==NATIVE_CONTROLLER_HUI&&fixture.request_input_inventory_generation==1&&fixture.request_input_endpoint_index==0&&fixture.request_input_endpoint_type==NATIVE_ENDPOINT_KERNEL&&fixture.request_output_endpoint_index==1&&fixture.request_output_endpoint_type==NATIVE_ENDPOINT_USER&&fixture.request_hui_policy==NATIVE_HUI_MOTORIZED,"Add publishes independent generation-tagged From and To ports plus explicit HUI policy");
 fixture.collection_count=2;fixture.collection_generation=2;fixture.controllers[1].enabled=1;fixture.controllers[1].profile=NATIVE_CONTROLLER_HUI;fixture.controllers[1].hui_policy=NATIVE_HUI_MOTORIZED;fixture.controllers[1].status=NATIVE_CONTROLLER_READY;fixture.controllers[1].input_type=NATIVE_ENDPOINT_KERNEL;fixture.controllers[1].output_type=NATIVE_ENDPOINT_USER;fixture.controllers[1].input_client_length=fixture.controllers[1].output_client_length=12;fixture.controllers[1].input_port_length=6;fixture.controllers[1].output_port_length=7;memcpy(fixture.controllers[1].input_client,"Physical HUI",12);memcpy(fixture.controllers[1].output_client,"Software HUI",12);memcpy(fixture.controllers[1].input_port,"DAW In",6);memcpy(fixture.controllers[1].output_port,"DAW Out",7);fixture.completed_sequence=1;fixture.status=NATIVE_PREFERENCES_ACTIVE;native_preferences_refresh();
 require(!preferences_instances[0].editing&&preferences_instances[0].controller_count==2&&last_visible(mpc_ui_private_list_host_object(preferences_instances[0].controllers))==1,"completed Add returns to the two-row collection");
 unsigned added_row_paints=paint_texts;
 paint_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),1,0,900,72);
 require(paint_texts==added_row_paints+2&&!strcmp(last_primary_text,"Generic HUI")&&!strcmp(last_paint_text,"Bridge not running"),"newly added second row paints profile and bridge availability separately");
 select_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),1);click(mpc_ui_private_control_host_object(preferences_instances[0].edit));require(preferences_instances[0].editing&&preferences_instances[0].draft_input==0&&preferences_instances[0].draft_output==1,"Edit restores independent saved ports into the role-filtered lists");click(mpc_ui_private_control_host_object(preferences_instances[0].cancel));
 click(mpc_ui_private_control_host_object(preferences_instances[0].toggle));require(fixture.request_sequence==2&&fixture.request_kind==NATIVE_REQUEST_TOGGLE&&fixture.request_controller_index==1&&!fixture.request_enabled,"Enable/Disable targets only the selected controller");
 click(mpc_ui_private_control_host_object(preferences_instances[0].remove));require(fixture.request_sequence==2&&fixture.request_kind==NATIVE_REQUEST_TOGGLE,"a rapid second mutation cannot overwrite the pending collection request");
 fixture.completed_sequence=2;fixture.status=NATIVE_PREFERENCES_ACTIVE;native_preferences_refresh();
 click(mpc_ui_private_control_host_object(preferences_instances[0].remove));require(fixture.request_sequence==3&&fixture.request_kind==NATIVE_REQUEST_REMOVE&&fixture.request_controller_index==1,"Remove targets only the selected controller");
 click(mpc_ui_private_control_host_object(preferences_instances[0].remove));require(fixture.request_sequence==3,"repeated Remove while completion is delayed cannot publish again");
 fixture.completed_sequence=3;fixture.status=NATIVE_PREFERENCES_ACTIVE;
 click(mpc_ui_private_control_host_object(preferences_instances[0].remove));require(fixture.request_sequence==3&&preferences_instances[0].selected_controller==MPC_UI_NO_SELECTION,"fast completion before UI refresh cannot admit another Remove without a fresh row selection");
 native_preferences_refresh();
 fixture.collection_count=1;fixture.collection_generation++;native_preferences_refresh();
 require(*(int*)((unsigned char*)mpc_ui_private_list_host_object(preferences_instances[0].controllers)+0xa4)==-1,"removed row does not visually select the adjacent remaining row");
 click(mpc_ui_private_control_host_object(preferences_instances[0].remove));require(fixture.request_sequence==3,"rendering the shifted collection still requires a fresh row selection before Remove");
 select_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),0);require(preferences_instances[0].selected_controller==0,"explicit remaining-row selection re-enables collection actions");
 void *refresh=mpc_ui_private_control_host_object(preferences_instances[0].refresh);click(refresh);
 require(fixture.request_sequence==4&&fixture.request_kind==NATIVE_REQUEST_REFRESH,"Refresh delegates one role-capable endpoint enumeration to the session owner");
 fixture.completed_sequence=4;fixture.status=NATIVE_PREFERENCES_REFRESHED;native_preferences_refresh();
 click(mpc_ui_private_control_host_object(preferences_instances[0].add));select_list(mpc_ui_private_list_host_object(preferences_instances[0].profiles),2);click(mpc_ui_private_control_host_object(preferences_instances[0].apply));
 require(fixture.request_sequence==4&&!fixture.failures&&strstr(preferences_instances[0].status_text,"Choose both"),"incomplete generic Add presents guidance without an internal failure or request");
 select_list(mpc_ui_private_list_host_object(preferences_instances[0].inputs),0);select_list(mpc_ui_private_list_host_object(preferences_instances[0].outputs),0);
 require(last_visible(refresh)==1,"Refresh remains reachable inside the port editor");click(refresh);
 NativePreferencesEndpoint previous=fixture.endpoints[0];fixture.endpoints[0]=fixture.endpoints[1];fixture.endpoints[1]=previous;fixture.inventory_generation++;fixture.completed_sequence=5;fixture.status=NATIVE_PREFERENCES_REFRESHED;native_preferences_refresh();
 require(preferences_instances[0].editing&&preferences_instances[0].draft_profile==NATIVE_CONTROLLER_GENERIC&&preferences_instances[0].draft_input==1&&preferences_instances[0].draft_output==0,"Refresh preserves the draft and remaps its independent logical endpoints after inventory reordering");
 click(mpc_ui_private_control_host_object(preferences_instances[0].apply));require(fixture.request_sequence==6&&fixture.request_collection_generation==fixture.collection_generation&&fixture.request_input_endpoint_index==1&&fixture.request_output_endpoint_index==0,"refreshed generic Add retains collection identity and publishes remapped ports");
 fixture.completed_sequence=fixture.request_sequence;fixture.status=NATIVE_PREFERENCES_ACTIVE;fixture.inventory_count=2;fixture.inventory_generation++;native_preferences_refresh();
 for(unsigned profile=NATIVE_CONTROLLER_GENERIC;profile<=NATIVE_CONTROLLER_PROFILE_COUNT;profile++){
  unsigned before=fixture.request_sequence;select_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),0);click(mpc_ui_private_control_host_object(preferences_instances[0].edit));
  require(preferences_instances[0].draft_input==NATIVE_ENDPOINT_UNCHANGED&&preferences_instances[0].draft_output==NATIVE_ENDPOINT_UNCHANGED,"known-profile edit starts with its absent ports retained offline");
  select_list(mpc_ui_private_list_host_object(preferences_instances[0].profiles),profile-1);if(profile==NATIVE_CONTROLLER_LPX||profile==NATIVE_CONTROLLER_LPP3)require(strstr(preferences_instances[0].status_text,"Turn off Track, Global and Control for this MIDI port."),"Programmer profile presents selected-port musical-routing prerequisite");if(profile==NATIVE_CONTROLLER_MPK249)require(strstr(preferences_instances[0].status_text,"Turn Receive MMC off in MIDI/Sync"),"MPK249 presents exclusive MMC handler prerequisite");click(mpc_ui_private_control_host_object(preferences_instances[0].apply));
  require(fixture.request_sequence==before&&!fixture.failures&&strstr(preferences_instances[0].status_text,"Choose both"),"known-to-generic edit publishes nothing without explicit port selections");
  fixture.inventory_status=NATIVE_PREFERENCES_INVALID;refresh_selection(&preferences_instances[0]);
  require(strstr(preferences_instances[0].status_text,"Too many MIDI ports, or a port name is too long."),"inventory error remains visible above every profile's setup guidance");
  fixture.inventory_status=NATIVE_PREFERENCES_ACTIVE;refresh_selection(&preferences_instances[0]);
  select_list(mpc_ui_private_list_host_object(preferences_instances[0].inputs),0);click(mpc_ui_private_control_host_object(preferences_instances[0].apply));require(fixture.request_sequence==before,"generic edit still requires its independent To Controller selection");
  select_list(mpc_ui_private_list_host_object(preferences_instances[0].outputs),0);click(mpc_ui_private_control_host_object(preferences_instances[0].apply));
  require(fixture.request_sequence==before+1&&fixture.request_profile==profile&&fixture.request_input_endpoint_index==1&&fixture.request_output_endpoint_index==0,"both explicit role-valid selections admit the generic edit");
  fixture.completed_sequence=fixture.request_sequence;fixture.status=NATIVE_PREFERENCES_INVALID;native_preferences_refresh();click(mpc_ui_private_control_host_object(preferences_instances[0].cancel));
 }
 for(unsigned profile=NATIVE_CONTROLLER_GENERIC;profile<=NATIVE_CONTROLLER_HUI;profile++){
  fixture.controllers[0].profile=profile;fixture.controllers[0].hui_policy=profile==NATIVE_CONTROLLER_HUI?NATIVE_HUI_PICKUP:0;fixture.collection_generation++;native_preferences_refresh();
  unsigned before=fixture.request_sequence;click(mpc_ui_private_control_host_object(preferences_instances[0].edit));click(mpc_ui_private_control_host_object(preferences_instances[0].apply));
  require(fixture.request_sequence==before+1&&fixture.request_profile==profile&&fixture.request_input_endpoint_index==NATIVE_ENDPOINT_UNCHANGED&&fixture.request_output_endpoint_index==NATIVE_ENDPOINT_UNCHANGED,"same-profile offline edit retains already explicit saved generic endpoints");
  fixture.completed_sequence=fixture.request_sequence;fixture.status=NATIVE_PREFERENCES_ACTIVE;native_preferences_refresh();
 }
 require(!fixture.failures,"profile-transition guidance never counts as an internal failure");
 NativePreferencesEndpoint saved_endpoints[NATIVE_PREFERENCES_ENDPOINTS];memcpy(saved_endpoints,fixture.endpoints,sizeof(saved_endpoints));
 uint32_t saved_endpoint_count=fixture.inventory_count;NativePreferencesController before_refresh_controller=fixture.controllers[0];
 fixture.inventory_count=NATIVE_PREFERENCES_ENDPOINTS;
 for(unsigned i=0;i<NATIVE_PREFERENCES_ENDPOINTS;i++){fixture.endpoints[i]=saved_endpoints[0];fixture.endpoints[i].capabilities=NATIVE_ENDPOINT_FROM_CONTROLLER|NATIVE_ENDPOINT_TO_CONTROLLER;fixture.endpoints[i].port_number=i;}
 fixture.inventory_generation++;native_preferences_refresh();click(mpc_ui_private_control_host_object(preferences_instances[0].add));
 select_list(mpc_ui_private_list_host_object(preferences_instances[0].inputs),NATIVE_PREFERENCES_ENDPOINTS-1);select_list(mpc_ui_private_list_host_object(preferences_instances[0].outputs),NATIVE_PREFERENCES_ENDPOINTS-1);
 require(preferences_instances[0].draft_input==63&&preferences_instances[0].draft_output==63&&preferences_instances[0].endpoint_rows[63].port_number==63,"both port lists can select the sixty-fourth exact inventory row");
 click(mpc_ui_private_control_host_object(preferences_instances[0].cancel));
 memset(fixture.endpoints,0,sizeof(fixture.endpoints));fixture.inventory_count=6;
 for(unsigned i=0;i<6;i++){auto &endpoint=fixture.endpoints[i];endpoint.endpoint_type=NATIVE_ENDPOINT_USER;endpoint.capabilities=NATIVE_ENDPOINT_TO_CONTROLLER;endpoint.client_number=129;endpoint.port_number=i;memcpy(endpoint.client,"MPC",4);memcpy(endpoint.port,"MPC MIDI Port",14);}
 auto &simulator=fixture.endpoints[5];simulator.capabilities|=NATIVE_ENDPOINT_FROM_CONTROLLER;simulator.client_number=132;simulator.port_number=0;memcpy(simulator.client,"MPClearn MCU Simulator",23);memcpy(simulator.port,"MCU Simulator",14);
 auto &edited=fixture.controllers[0];edited.profile=NATIVE_CONTROLLER_HUI;edited.hui_policy=NATIVE_HUI_MOTORIZED;edited.input_type=edited.output_type=NATIVE_ENDPOINT_USER;
 edited.input_client_length=edited.output_client_length=22;edited.input_port_length=edited.output_port_length=13;memcpy(edited.input_client,simulator.client,64);memcpy(edited.output_client,simulator.client,64);memcpy(edited.input_port,simulator.port,64);memcpy(edited.output_port,simulator.port,64);
 fixture.inventory_generation++;fixture.collection_generation++;native_preferences_refresh();select_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),0);
 controller_feedback_enabled=true;click(mpc_ui_private_control_host_object(preferences_instances[0].edit));
 require(preferences_instances[0].draft_profile==NATIVE_CONTROLLER_HUI&&preferences_instances[0].draft_input==5&&preferences_instances[0].draft_output==5&&!preferences_instances[0].presentation_depth,"Edit ignores cross-list native selection notifications throughout presentation");
 for(unsigned reordered=0;reordered<2;reordered++){
  click(mpc_ui_private_control_host_object(preferences_instances[0].refresh));
  if(reordered){auto first=fixture.endpoints[0];fixture.endpoints[0]=fixture.endpoints[5];fixture.endpoints[5]=first;}
  fixture.inventory_generation++;fixture.collection_generation++;fixture.status=NATIVE_PREFERENCES_REFRESHED;fixture.completed_sequence=fixture.request_sequence;native_preferences_refresh();
  void *from_list=mpc_ui_private_list_host_object(preferences_instances[0].inputs),*to_list=mpc_ui_private_list_host_object(preferences_instances[0].outputs);
  int from_selected=*(int*)((unsigned char*)from_list+0xa4),to_selected=*(int*)((unsigned char*)to_list+0xa4);
  require(from_selected==0&&to_selected==(reordered?0:5)&&preferences_instances[0].draft_input==(reordered?0u:5u)&&preferences_instances[0].draft_output==(reordered?0u:5u)&&preferences_instances[0].draft_policy==NATIVE_HUI_MOTORIZED,"same-count Refresh retains independent visible From/To selection and Motorized policy, including reorder");
  paint_list(from_list,from_selected,1,296,96);require(!strcmp(last_primary_text,"MCU Simulator")&&!strcmp(last_paint_text,"App 132:0"),"refreshed visible From selection paints simulator identity");
  paint_list(to_list,to_selected,1,296,96);require(!strcmp(last_primary_text,"MCU Simulator")&&!strcmp(last_paint_text,"App 132:0"),"refreshed visible To selection paints simulator identity, not the first unrelated port");
  paint(mpc_ui_private_control_host_object(preferences_instances[0].policy[0]));require(last_fill==0xffd31145u,"refreshed Motorized control remains visibly selected");
 }
 require(controller_update_feedback&&controller_visibility_feedback&&!preferences_instances[0].presentation_depth,"regression exercises both update and visibility feedback and releases the presentation guard");
 select_list(mpc_ui_private_list_host_object(preferences_instances[0].outputs),4);
 require(preferences_instances[0].draft_output==4&&preferences_instances[0].draft_input==0&&preferences_instances[0].draft_profile==NATIVE_CONTROLLER_HUI&&preferences_instances[0].draft_policy==NATIVE_HUI_MOTORIZED&&!preferences_instances[0].presentation_depth,"real To selection outside presentation changes only its draft while nested update notifications remain inert");
 unsigned explicit_request=fixture.request_sequence;click(mpc_ui_private_control_host_object(preferences_instances[0].apply));
 require(fixture.request_sequence==explicit_request+1&&fixture.request_input_endpoint_index==0&&fixture.request_output_endpoint_index==4&&fixture.request_hui_policy==NATIVE_HUI_MOTORIZED,"explicit user selection still publishes distinct From/To and policy after guarded refresh");
 fixture.completed_sequence=fixture.request_sequence;fixture.status=NATIVE_PREFERENCES_ACTIVE;native_preferences_refresh();controller_feedback_enabled=false;
 memcpy(fixture.endpoints,saved_endpoints,sizeof(saved_endpoints));fixture.inventory_count=saved_endpoint_count;fixture.controllers[0]=before_refresh_controller;fixture.inventory_generation++;fixture.collection_generation++;native_preferences_refresh();
 NativePreferencesController saved_controller=fixture.controllers[0];
 fixture.controllers[0].profile=NATIVE_CONTROLLER_HUI;fixture.controllers[0].input_port_length=fixture.controllers[0].output_port_length=63;
 memset(fixture.controllers[0].input_port,'F',63);memset(fixture.controllers[0].output_port,'T',63);
 memcpy(fixture.controllers[0].input_port,"From-",5);memcpy(fixture.controllers[0].output_port,"To---",5);
 memcpy(fixture.controllers[0].input_port+60,"-A1",3);memcpy(fixture.controllers[0].output_port+60,"-B2",3);
 const uint32_t row_states[]={NATIVE_CONTROLLER_ACTIVE,NATIVE_CONTROLLER_DISCONNECTED,NATIVE_CONTROLLER_DISABLED};
 const char *row_statuses[]={"Connected","Disconnected","Disabled"};
 for(unsigned i=0;i<3;i++){
  fixture.controllers[0].enabled=i!=2;fixture.controllers[0].status=row_states[i];fixture.collection_generation++;fixture.admission.collection_generation=fixture.collection_generation;fixture.admission.count=fixture.collection_count;fixture.admission.status[0]=row_states[i];fixture.admission.revision+=2;native_preferences_refresh();
  unsigned before=paint_texts;paint_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),0,1,900,72);
  require(paint_texts==before+2&&!strcmp(last_primary_text,"Generic HUI")&&strstr(last_paint_text,row_statuses[i]),"connected, disconnected and disabled rows retain readable primary name and secondary state");
  select_list(mpc_ui_private_list_host_object(preferences_instances[0].controllers),0);
  require(strstr(preferences_instances[0].connection_text[0],"From-")&&strstr(preferences_instances[0].connection_text[0],"-A1")&&strstr(preferences_instances[0].connection_text[1],"To---")&&strstr(preferences_instances[0].connection_text[1],"-B2"),"selected details preserve full distinct From/To suffixes outside uncluttered rows");
  require(!memcmp(&preferences_instances[0].controller_rows[0],&fixture.controllers[0],sizeof(fixture.controllers[0])),"row presentation preserves both full saved endpoint identities for Edit");
 }
 fixture.controllers[0]=saved_controller;fixture.collection_generation++;native_preferences_refresh();
 {
  auto &ui=preferences_instances[0];void *detect=mpc_ui_private_control_host_object(ui.detect);
  fixture.completed_sequence=fixture.request_sequence;fixture.status=NATIVE_PREFERENCES_ACTIVE;native_preferences_refresh();
  MpcUiControl *row[]={ui.add,ui.detect,ui.edit,ui.toggle,ui.remove,ui.refresh};Bounds first=last_bounds(mpc_ui_private_control_host_object(row[0])),previous=first;
  for(auto *control:row){void *object=mpc_ui_private_control_host_object(control);Bounds b=last_bounds(object);
   require(last_visible(object)==1&&b.y==first.y&&b.h==first.h&&b.w>=116&&(control==row[0]||b.x>=previous.x+previous.w+12),"six main actions share one row without overlap");previous=b;}
  require(first.x==24&&previous.x+previous.w==924,"main action row spans the 24..924 content margins");
  uint32_t base=fixture.collection_count,sequence=fixture.request_sequence;click(detect);
  require(fixture.request_sequence==sequence+1&&fixture.request_kind==NATIVE_REQUEST_DETECT&&fixture.request_controller_index==NATIVE_CONTROLLER_NEW&&fixture.request_collection_generation==fixture.collection_generation,"Detect publishes one generation-tagged request to the session owner");
  click(detect);require(fixture.request_sequence==sequence+1,"a pending Detect cannot publish again");
  fixture.status=NATIVE_PREFERENCES_APPLYING;native_preferences_refresh();require(strstr(ui.status_text,"Applying controller changes"),"pending Detect shows progress");
  fixture.controllers[base]=fixture.controllers[0];fixture.controllers[base].profile=NATIVE_CONTROLLER_XTOUCH;fixture.controllers[base+1]=fixture.controllers[0];fixture.controllers[base+1].profile=NATIVE_CONTROLLER_LPX;
  fixture.collection_count=base+2;fixture.collection_generation++;fixture.completed_sequence=fixture.request_sequence;fixture.status=NATIVE_PREFERENCES_ACTIVE;native_preferences_refresh();
  require(ui.controller_count==base+2&&strstr(ui.status_text,"Added X-Touch and Launchpad X.\n"),"completed Detect reports the appended rows in one line");
  click(mpc_ui_private_control_host_object(ui.add));require(last_visible(detect)==0,"Detect is hidden in the editor");click(mpc_ui_private_control_host_object(ui.cancel));
  require(!strstr(ui.status_text,"Added"),"the next action clears the Detect report");
  sequence=fixture.request_sequence;click(detect);fixture.completed_sequence=fixture.request_sequence;fixture.status=NATIVE_PREFERENCES_NONE_DETECTED;fixture.result_detail=0;native_preferences_refresh();
  require(fixture.request_sequence==sequence+1&&strstr(ui.status_text,"No new controllers found."),"Detect reports when nothing new is connected");
  click(detect);fixture.completed_sequence=fixture.request_sequence;fixture.status=NATIVE_PREFERENCES_STALE;native_preferences_refresh();
  require(strstr(ui.status_text,"Try Detect again"),"a stale Detect asks for a retry");
  uint32_t count=fixture.collection_count;fixture.collection_count=16;for(unsigned n=count;n<16;n++)fixture.controllers[n]=fixture.controllers[0];fixture.collection_generation++;native_preferences_refresh();
  sequence=fixture.request_sequence;click(detect);require(fixture.request_sequence==sequence&&strstr(ui.status_text,"The controller list is full."),"a full collection reports capacity without a request");
  memset(fixture.controllers+base,0,(16-base)*sizeof(fixture.controllers[0]));fixture.collection_count=base;fixture.collection_generation++;fixture.status=NATIVE_PREFERENCES_ACTIVE;native_preferences_refresh();
  require(!fixture.failures,"Detect presentation raises no toolkit failure");
 }
 ((void(*)(void*))tab_slots[1])(tab);require(fixture.tabs_created==fixture.tabs_destroyed&&fixture.buttons_created==fixture.buttons_destroyed&&!fixture.live_tabs&&!fixture.failures,"Controllers toolkit teardown balances");
 visible_count=0; /* Controllers are closed; the next phase observes launcher objects. */
 uint32_t pref_begin=*(uint32_t*)(preferences_overlay+0x1b8),pref_cap=*(uint32_t*)(preferences_overlay+0x1c0);test_delete((void*)(uintptr_t)pref_begin,pref_cap-pref_begin);

 alignas(8) unsigned char page[0x220]={},child_a[0x20]={},child_b[0x20]={},unrelated_page[0x220]={},overlay[0x220]={},owner_storage[0x90]={};launcher_owner_component=owner_storage+8;*(void**)(overlay+0x134)=owner_storage;
 alignas(8) unsigned char grid[0x70]={},viewport[0x70]={},content[0x70]={},first_page[0x70]={},second_page[0x70]={},sidebar[0x70]={},grid_extra[4][0x70]={};
 alignas(8) unsigned char filler[32][0x40]={},utility_cells[4][0x40]={},utility_buttons[4][0x70]={},second_cell[0x40]={},second_button[0x70]={};
 auto set_component=[](void *child,void *parent,int x,int y,int w,int h){*(void**)((unsigned char*)child+0x0c)=parent;int b[4]={x,y,w,h};memcpy((unsigned char*)child+0x10,b,sizeof(b));};
 auto set_children=[](void *parent,uint32_t *children,int count){*(uint32_t*)((unsigned char*)parent+0x28)=(uint32_t)(uintptr_t)children;*(int*)((unsigned char*)parent+0x2c)=count;*(int*)((unsigned char*)parent+0x30)=count;};
 uint32_t owner_children[1]={(uint32_t)(uintptr_t)(page+8)};set_children(launcher_owner_component,owner_children,1);set_component(page+8,launcher_owner_component,0,0,1280,690);
 uint32_t page_children[2]={(uint32_t)(uintptr_t)grid,(uint32_t)(uintptr_t)sidebar};set_children(page+8,page_children,2);set_component(grid,page+8,228,0,1052,690);set_component(sidebar,page+8,0,0,228,684);
 uint32_t grid_children[5]={(uint32_t)(uintptr_t)viewport,(uint32_t)(uintptr_t)grid_extra[0],(uint32_t)(uintptr_t)grid_extra[1],(uint32_t)(uintptr_t)grid_extra[2],(uint32_t)(uintptr_t)grid_extra[3]};set_children(grid,grid_children,5);set_component(viewport,grid,74,0,904,690);for(unsigned i=0;i<4;i++)set_component(grid_extra[i],grid,6+(int)i*200,311,68,68);
 uint32_t viewport_children[1]={(uint32_t)(uintptr_t)content};set_children(viewport,viewport_children,1);set_component(content,viewport,0,0,1808,690);
 uint32_t content_children[2]={(uint32_t)(uintptr_t)first_page,(uint32_t)(uintptr_t)second_page};set_children(content,content_children,2);set_component(first_page,content,0,0,904,684);set_component(second_page,content,904,0,904,684);ordered_parent=second_page;
 uint32_t first_children[40]={};for(unsigned i=0;i<32;i++){first_children[i]=(uint32_t)(uintptr_t)filler[i];set_component(filler[i],first_page,(int)(i%4)*226,(int)(i/8)*136,10,10);}
 for(unsigned i=0;i<4;i++){first_children[32+i*2]=(uint32_t)(uintptr_t)utility_cells[i];first_children[33+i*2]=(uint32_t)(uintptr_t)utility_buttons[i];set_component(utility_cells[i],first_page,(int)i*226,544,226,136);set_children(utility_cells[i],nullptr,0);set_component(utility_buttons[i],first_page,3+(int)i*226,550,220,124);uint32_t *button_children=(uint32_t*)(utility_buttons[i]+0x38);button_children[0]=1;button_children[1]=2;set_children(utility_buttons[i],button_children,2);}
 set_children(first_page,first_children,40);uint32_t second_children[41]={};second_children[14]=(uint32_t)(uintptr_t)second_cell;second_children[15]=(uint32_t)(uintptr_t)second_button;
 set_component(second_cell,second_page,678,136,226,136);set_children(second_cell,nullptr,0);set_component(second_button,second_page,681,142,220,124);uint32_t *second_button_children=(uint32_t*)(second_button+0x38);second_button_children[0]=3;second_button_children[1]=4;set_children(second_button,second_button_children,2);set_children(second_page,second_children,40);
 uint32_t launcher_children[4]={(uint32_t)(uintptr_t)child_a,0,(uint32_t)(uintptr_t)child_b,0};*(uint32_t*)(page+0xc8)=(uint32_t)(uintptr_t)launcher_children;*(uint32_t*)(page+0xcc)=(uint32_t)(uintptr_t)(launcher_children+4);*(uint32_t*)(page+0xd0)=(uint32_t)(uintptr_t)(launcher_children+4);
 int page_bounds[4]={0,0,1280,690};memcpy(unrelated_page+0x18,page_bounds,sizeof(page_bounds));
 setup_outer_layout(frame,page);native_preferences_hook(&frame.context,2);require(!fixture.launcher_overlays_created&&!fixture.failures,"layout before overlay bind ignored");
 memset(page+0x14,0,5u*sizeof(uint32_t));
 memset(&frame,0,sizeof(frame));frame.context.r[4]=(uint32_t)(uintptr_t)overlay;frame.context.r[5]=(uint32_t)(uintptr_t)page;native_preferences_hook(&frame.context,1);
 require(fixture.launcher_overlays_created==1&&fixture.live_launcher_overlays==1&&!fixture.failures&&!launcher_instances[0].app_host,
  "constructor phase binds owner lifetime but defers app registration");
 require(fixture.launcher_bind_overlay==(uint32_t)(uintptr_t)overlay&&fixture.launcher_bind_page==(uint32_t)(uintptr_t)page&&
  !fixture.launcher_bind_parent&&!fixture.launcher_bind_bounds[0]&&!fixture.launcher_bind_bounds[1]&&
  !fixture.launcher_bind_bounds[2]&&!fixture.launcher_bind_bounds[3],"constructor phase records the actual unparented and unlaid Page");
 set_component(page+8,launcher_owner_component,0,0,1280,690);
 setup_outer_layout(frame,unrelated_page);native_preferences_hook(&frame.context,2);require(!fixture.launcher_tiles_created&&!fixture.failures,"unrelated Page ignored");
 uint32_t original_records[4]={};memcpy(original_records,launcher_children,sizeof(original_records));*(uint32_t*)(page+0xcc)=(uint32_t)(uintptr_t)launcher_children;
 setup_outer_layout(frame,page);native_preferences_hook(&frame.context,2);
 require(!fixture.launcher_tiles_created&&!fixture.failures&&!launcher_instances[0].app_host&&!memcmp(original_records,launcher_children,sizeof(original_records)),"empty vector pass records layout but waits for completed population");
 require(fixture.launcher_layout_overlay==fixture.launcher_bind_overlay&&fixture.launcher_layout_page==fixture.launcher_bind_page&&
  fixture.launcher_layout_parent==(uint32_t)(uintptr_t)launcher_owner_component&&
  !fixture.launcher_layout_bounds[0]&&!fixture.launcher_layout_bounds[1]&&fixture.launcher_layout_bounds[2]==1280&&fixture.launcher_layout_bounds[3]==690,
  "layout phase records the same Page with its concrete parent and bounds");
 *(uint32_t*)(page+0xcc)=(uint32_t)(uintptr_t)(launcher_children+4);unsigned placement_bounds_before=bounds_count;
 setup_outer_layout(frame,page);native_preferences_hook(&frame.context,2);LauncherInstance &li=launcher_instances[0];void *launcher_button=mpc_ui_private_control_host_object(li.launcher);
 require(!memcmp(original_records,launcher_children,sizeof(original_records))&&li.app_host&&launcher_button&&fixture.launcher_tiles_created==1&&attached(second_page,launcher_button),"completed layout registers one app host and one toolkit launcher on second page");
 require(front_keeps==1&&test_always_on_top(launcher_button),"repair10 z-order retained through canonical toolkit");
 order_add(second_page,second_button);require(ordered_count==2&&ordered_children[0]==second_button&&ordered_children[1]==launcher_button,"later stock child remains behind launcher");
 Bounds launcher_bounds=last_bounds(launcher_button);require(launcher_bounds.x==681&&launcher_bounds.y==142&&launcher_bounds.w==220&&launcher_bounds.h==124&&bounds_count==placement_bounds_before+1,"exact page-two local placement only");
 paint(launcher_button);require(last_fill==0xff2f2f34u&&
#if MPCLEARN_NATIVE_UI_EXAMPLE
  !strcmp(last_paint_text,"UI TOOLKIT EXAMPLE"),"example launcher shared paint");
#else
  !strcmp(last_paint_text,"GLOBAL MIDI LEARN"),"launcher shared paint");
#endif
 second_children[40]=(uint32_t)(uintptr_t)launcher_button;set_children(second_page,second_children,41);
 int repeat_bounds[4]={};require(!launcher_second_page(li,repeat_bounds),"post-insertion tree no longer matches the first-admission 40-child hierarchy");
 setup_outer_layout(frame,page);native_preferences_hook(&frame.context,2);
 require(fixture.launcher_tiles_created==1&&!fixture.failures&&li.entry_screen&&li.entry_parent&&li.launcher&&li.app_host,
  "repeat completed layout recognizes the existing owned entry before revalidating the mutated stock tree");
#if MPCLEARN_NATIVE_UI_EXAMPLE
 click(launcher_button);require(fixture.launcher_clicks==1&&fixture.example_opens==1&&fixture.live_examples==1&&fixture.shells_created==1&&fixture.live_shells==1&&li.example_app.view.screen&&last_visible(page+8)==0,"launcher opens combined public example through the opaque app host");
 void *action_one=mpc_ui_private_control_host_object(li.example_app.view.action_one);
 void *action_two=mpc_ui_private_control_host_object(li.example_app.view.action_two);
 void *example_back=mpc_ui_private_control_host_object(li.example_app.view.back);
 unsigned example_slider_updates=slider_silent_updates,example_label_updates=label_text_updates;
 click(action_one);click(action_two);
 require(fixture.example_intents==2&&fixture.last_example_intent==MPCLEARN_TOOLKIT_EXAMPLE_ACTION_TWO&&
  label_text_updates==example_label_updates+2&&slider_silent_updates==example_slider_updates+2,"public example actions publish distinct intents and silently refresh Label and Slider");
 setup_outer_layout(frame,page);native_preferences_hook(&frame.context,2);
 require(mpc_ui_app_is_open(li.app_host)&&mpc_ui_private_app_host_is_suspended(li.app_host)&&
  li.example_app.view.screen&&fixture.example_opens==1&&fixture.live_examples==1&&last_visible(page+8)==1&&!fixture.failures,
  "reopened stock Menu suspends the live example and restores the app list");
 click(launcher_button);native_preferences_refresh();
 require(mpc_ui_app_is_open(li.app_host)&&!mpc_ui_private_app_host_is_suspended(li.app_host)&&
  li.example_app.view.screen&&fixture.example_opens==1&&fixture.launcher_clicks==2&&last_visible(page+8)==0,
  "existing launcher tile resumes the retained example without reconstructing its state");
 click(example_back);require(fixture.example_back_clicks==1&&fixture.example_intents==3&&fixture.live_examples==1&&mpc_ui_app_is_closing(li.app_host),"example Back defers close until the owning UI drain");
 native_preferences_refresh();
 require(fixture.example_closes==1&&!fixture.live_examples&&fixture.shells_created==fixture.shells_destroyed&&!fixture.live_shells&&!li.example_app.view.screen&&last_visible(page+8)==1,"deferred example Back closes after its callback and restores the stock page");
 click(launcher_button);require(fixture.example_opens==2&&fixture.live_examples==1&&li.example_app.view.screen,"public example reopens through the same one-consumer host");
 ((void(*)(void*))(*(void***)overlay)[1])(overlay);
 require(fixture.launcher_overlays_created==fixture.launcher_overlays_destroyed&&fixture.launcher_tiles_created==fixture.launcher_tiles_destroyed&&
  fixture.example_opens==fixture.example_closes&&!fixture.live_examples&&fixture.shells_created==fixture.shells_destroyed&&!fixture.live_shells&&
  !fixture.live_launcher_overlays&&!fixture.live_launcher_tiles&&!fixture.failures,"in-process stock owner teardown force-closes the live public example before releasing host, parents and context");
 require(fixture.overlay_destroy_entries==1&&fixture.overlay_destroy_ui_thread_matches==1&&fixture.overlay_destroy_observer_running==1&&
  fixture.overlay_destroy_consumer_open==1&&!fixture.overlay_destroy_busy&&!fixture.overlay_destroy_callback_depth&&
  fixture.overlay_destroy_cleanup_complete==1&&fixture.overlay_destroy_stock_calls==1&&!fixture.overlay_destroy_retained,
  "successful owner teardown records eligibility and complete cleanup before the stock destructor");
 require(ordered_count==1&&ordered_children[0]==second_button,"stock child order retained after example launcher removal");
 require(string_ctors==string_dtors&&button_ctors==button_dtors&&label_ctors==label_dtors&&slider_ctors==slider_dtors&&allocations==deletes&&overlay_dtors==1&&child_removes==child_adds+1,"combined example and launcher native object lifetimes balance; one extra remove is the stock placeholder");

 second_children[40]=0;set_children(second_page,second_children,40);
 memset(overlay,0,sizeof(overlay));*(void**)(overlay+0x134)=owner_storage;
 memset(&frame,0,sizeof(frame));frame.context.r[4]=(uint32_t)(uintptr_t)overlay;frame.context.r[5]=(uint32_t)(uintptr_t)page;native_preferences_hook(&frame.context,1);
 setup_outer_layout(frame,page);native_preferences_hook(&frame.context,2);
 LauncherInstance &retained=launcher_instances[0];void *retained_launcher=mpc_ui_private_control_host_object(retained.launcher);
 require(retained_launcher&&fixture.launcher_tiles_created==2&&!fixture.failures,"second admitted owner creates one fresh example launcher");
 click(retained_launcher);require(retained.example_app.view.screen&&fixture.live_examples==1,"forced-failure fixture opens public consumer");
 uint32_t launcher_clicks_before_owner_loss=fixture.launcher_clicks;
 fail_remove_after=6;expect_retained_overlay=true;
 ((void(*)(void*))(*(void***)overlay)[1])(overlay);
 require(fixture.failures==1&&fixture.last_failure==NP_LAUNCHER_TEARDOWN&&retained.orphaned&&!retained.app_host&&retained.entry_screen&&retained.launcher&&retained.context&&!retained.example_app.view.screen&&
  !fixture.live_examples&&!fixture.live_shells&&!fixture.live_launcher_overlays&&fixture.live_launcher_tiles==1,"failed launcher-entry detach is observable and retains the invalidated entry graph instead of freeing referenced state");
 require(fixture.overlay_destroy_entries==2&&fixture.overlay_destroy_consumer_open==1&&!fixture.overlay_destroy_busy&&!fixture.overlay_destroy_callback_depth&&
  !fixture.overlay_destroy_cleanup_complete&&fixture.overlay_destroy_stock_calls==2&&fixture.overlay_destroy_retained==1,
  "failed owner teardown records terminal retention before the stock destructor");
 click(retained_launcher);require(fixture.launcher_clicks==launcher_clicks_before_owner_loss&&fixture.failures==1,"terminal retained launcher entry callback remains inert after stock owner loss");
 printf("PASS production public example composition through canonical toolkit: one page-two slot, opaque app host, native Label and Slider, distinct actions, silent owner refresh, deferred Back, reopen, successful forced-owner balance and terminal inert retention on launcher-entry detach failure (host ABI, MPC rendering, native Slider touch, focus and stock ownership substituted)\n");
#else
 click(launcher_button);require(fixture.launcher_clicks==1&&fixture.shells_created==1&&fixture.live_shells==1&&li.global_screen.screen&&last_visible(page+8)==0,"launcher opens live assignment screen and focuses page away");
 require(fixture.global_screens_created==1&&fixture.global_refreshes==1&&fixture.global_target_count==120&&fixture.global_page_start==0&&fixture.global_assigned_rows==3,"first functional group read from live owner");
 require(!strcmp(li.row_text[0],"001 PLAY")&&!strcmp(li.row_text[1],"002 TARGET")&&!strcmp(li.page_text,"PADS 1-16"),"native target list keeps mapping detail out of row labels");
 require(global_labels==16&&global_lookups==16&&global_target_dtors==16&&global_formats==3&&global_file_names==1&&global_string_dtors==20,"borrowed lookup, file-name and host String lifetimes balance for first group");
 void *groups=mpc_ui_private_list_host_object(li.global_screen.groups),*targets=mpc_ui_private_list_host_object(li.global_screen.targets);
 unsigned repaint_before_same_count=repaints;require(mpc_ui_list_update(li.global_screen.targets,16,MPC_UI_NO_SELECTION)&&repaints==repaint_before_same_count+1,"same-count target data update invalidates native list paint");
 paint_list(groups,0,1,248,52);require(last_fill==0xff595a62u&&!strcmp(last_paint_text,"PADS"),"selected native group is muted before either list owns the wheel");
 select_list(groups,1);require(fixture.global_page_clicks==1&&fixture.global_page_start==16&&!strcmp(li.page_text,"PAD BANKS 17-24")&&li.selected_target==MPCLEARN_GLOBAL_LEARN_NO_SELECTION,"group selection changes only transient navigation");
 select_list(groups,0);require(fixture.global_page_clicks==2&&fixture.global_page_start==0,"group list returns to Pads without selecting a target");
 select_list(targets,1);
 require(fixture.global_row_clicks==1&&fixture.global_selected_target==1&&fixture.global_target_selections==1&&global_selects==1&&selected_global_target==1&&!strcmp(li.status_text,"SELECTED TARGET 2")&&!strcmp(li.assignment_text,"ASSIGNMENT: UNASSIGNED"),"target-list selection publishes fixed Target and separates assignment detail");
 void *file_import=mpc_ui_private_control_host_object(li.global_screen.file_load);
 void *file_new=mpc_ui_private_control_host_object(li.global_screen.file_new),*file_copy=mpc_ui_private_control_host_object(li.global_screen.file_copy);
 void *file_previous=mpc_ui_private_control_host_object(li.global_screen.mapping_previous),*file_next=mpc_ui_private_control_host_object(li.global_screen.mapping_next);
 click(file_import);
 require(global_imports==1&&global_modal_reentries==1&&fixture.global_file_actions==1&&fixture.global_file_completed==1&&fixture.global_file_import_no_additions==1&&fixture.global_file_imports==1&&fixture.global_file_no_changes==1&&
  !strcmp(li.file_status_text,"IMPORT CLOSED - NO FILE ADDED")&&global_file_count==2,"synchronous Import no-addition return reenters only the guarded UI drain and preserves the selected mapping without inferring cancel");
 click(file_new);require(global_creates==1&&global_file_count==3&&fixture.global_file_creates==1&&fixture.global_file_index==2&&!strcmp(li.mapping_text,"MAPPING: NEW MAP"),"native New appends and selects through the existing file owner");
 click(file_copy);require(global_copies==1&&global_file_count==4&&fixture.global_file_copies==1&&fixture.global_file_index==3&&!strcmp(li.mapping_text,"MAPPING: USER TEST COPY"),"native Copy appends and selects through the existing file owner");
 click(mpc_ui_private_control_host_object(li.global_screen.file_export));
 require(global_exports==1&&!global_export_writes&&global_modal_reentries==2&&fixture.global_file_exports==1&&
  fixture.global_file_no_changes==2&&fixture.global_file_action==FILE_ACTION_NONE&&
  !strcmp(li.file_status_text,"EXPORT CLOSED - NO FILE WRITTEN"),
  "Export cancel returns before File construction or native writer and settles the existing action");
 global_export_returns_file=true;click(mpc_ui_private_control_host_object(li.global_screen.file_export));
 require(global_exports==2&&global_export_writes==1&&global_modal_reentries==3&&fixture.global_file_exports==2&&
  fixture.global_file_action==FILE_ACTION_NONE&&!strcmp(li.file_status_text,"EXPORT COMPLETED"),
  "named Export retains the existing native owner writer and settles without changing selection");
 click(file_previous);require(global_file_selects==1&&fixture.global_file_index==2&&!strcmp(li.mapping_text,"MAPPING: NEW MAP"),"previous mapping uses the native collection index");
 click(file_next);require(global_file_selects==2&&fixture.global_file_index==3&&!strcmp(li.mapping_text,"MAPPING: USER TEST COPY"),"next mapping refreshes the native collection before dispatch");
 global_import_returns_file=true;global_import_destination_exists=true;click(file_import);
 require(global_imports==2&&!global_import_copies&&!global_import_registers&&!global_import_releases&&
  fixture.global_file_imports==2&&fixture.global_file_import_no_additions==2&&fixture.global_file_count==4&&fixture.global_file_index==3&&
  !strcmp(li.file_status_text,"IMPORT DESTINATION EXISTS"),
  "Import visibly refuses an existing user-directory destination before byte-copy or registration");
 global_import_destination_exists=false;click(file_import);
 require(global_imports==3&&global_import_copies==1&&global_import_registers==1&&global_import_releases==1&&
  fixture.global_file_imports==3&&fixture.global_file_count==5&&fixture.global_file_index==4&&
  !strcmp(li.mapping_text,"MAPPING: IMPORTED MAP")&&!strcmp(li.file_status_text,"IMPORTED BYTE-EXACT COPY SELECTED"),
  "Import copies to the user mapping directory, registers the exact reopened object and selects its resolved native index");
 click(file_previous);require(global_file_selects==4&&fixture.global_file_index==3&&!strcmp(li.mapping_text,"MAPPING: USER TEST COPY"),"native Previous returns from imported preset to the editable common-subset Copy");
 require(fixture.global_file_actions==10&&fixture.global_file_completed==10&&fixture.global_file_selections==3&&fixture.global_file_action==FILE_ACTION_NONE&&fixture.global_file_count==5,"file action readback distinguishes no-addition and collision Import, create, copy, canceled/successful export, byte-copy Import and native selection");
 setup_outer_layout(frame,page);native_preferences_hook(&frame.context,2);
 require(mpc_ui_private_app_host_is_suspended(li.app_host)&&li.global_screen.screen&&
  li.selected_target==1&&!strcmp(li.mapping_text,"MAPPING: USER TEST COPY")&&last_visible(page+8)==1&&!fixture.failures,
  "idle Global editor suspension exposes stock Menu without discarding transient selection");
 require(!native_preferences_wheel(1)&&li.selected_target==1,
  "suspended editor leaves the data wheel with the visible stock Menu");
 click(launcher_button);native_preferences_refresh();
 require(!mpc_ui_private_app_host_is_suspended(li.app_host)&&li.global_screen.screen,
  "launcher resumes the existing Global editor host");
 require(fixture.shells_created==1&&fixture.launcher_clicks==2,
  "resume does not construct a second Global shell");
 require(li.selected_target==1,
  "resume preserves the transient Global target selection");
 require(last_visible(page+8)==0,
  "resumed Global editor focuses the stock page away");
 require(last_visible(mpc_ui_private_control_host_object(li.global_screen.learn))==1,"qualified Learn is available only after selection; no unqualified Save control exists");
 void *learn=mpc_ui_private_control_host_object(li.global_screen.learn),*cancel=mpc_ui_private_control_host_object(li.global_screen.cancel);
 click(learn);require(global_learn_calls==1&&requested_global_learn&&fixture.global_learn_requests==1&&fixture.global_learn_state==NATIVE_GLOBAL_LEARN_ARMING&&fixture.global_learn_pending==1&&!strcmp(li.status_text,"ARMING TARGET 2"),"Learn calls only the captured view-controller queued intent and reports pending");
 require(last_visible(learn)==0&&last_visible(cancel)==1,"pending Learn exposes cancellation");
 select_list(groups,1);select_list(targets,0);
 require(fixture.global_page_clicks==2&&fixture.global_row_clicks==1&&li.page_start==0&&li.selected_target==1,"pending Learn restores group and target selection without owner intent");
 native_preferences_refresh();require(fixture.global_learn_state==NATIVE_GLOBAL_LEARN_ARMING&&fixture.global_learn_pending==1&&fixture.global_learn_settled==0,"intent return is not arm settlement");
 settle_global_learn(true);native_preferences_refresh();
 require(fixture.global_learn_state==NATIVE_GLOBAL_LEARN_ARMED&&fixture.global_learn_settled==1&&!fixture.global_learn_pending&&fixture.global_learn_owner_value==1&&fixture.global_learn_mirror_value==1&&!strcmp(li.status_text,"LEARNING TARGET 2"),"arm settles only after owner, mirror and pending drain agree");
 setup_outer_layout(frame,page);native_preferences_hook(&frame.context,2);
 require(!mpc_ui_private_app_host_is_suspended(li.app_host)&&last_visible(page+8)==0&&
  fixture.global_learn_state==NATIVE_GLOBAL_LEARN_ARMED&&!fixture.failures,
  "armed Global editor refuses Menu suspension and keeps cancellation accessible");
 unsigned polls_before=fixture.global_pairing_polls,lookups_before=global_lookups,dtors_before=global_target_dtors;
 unsigned labels_before=global_labels,formats_before=global_formats,learn_calls_before=global_learn_calls;
 for(unsigned i=0;i<4;i++)native_preferences_refresh();
 require(fixture.global_pairing_polls==polls_before+1&&global_lookups==lookups_before+1&&global_target_dtors==dtors_before+1&&
  global_labels==labels_before&&global_formats==formats_before&&!fixture.global_pairing_changes&&!fixture.global_row_repaints,
  "settled armed screen polls one selected pairing without rebuilding labels or formatting unchanged values");
 type=2;control=5;channel=1;data1=13;memcpy(global_mappings[1]+0x9c,&type,4);memcpy(global_mappings[1]+0xa0,&control,4);
 memcpy(global_mappings[1]+0xa4,&channel,4);memcpy(global_mappings[1]+0xa8,&data1,1);
 unsigned detail_updates_before=label_text_updates;
 for(unsigned i=0;i<4;i++)native_preferences_refresh();
 require(fixture.global_pairing_polls==polls_before+2&&fixture.global_pairing_changes==1&&fixture.global_row_repaints==1&&
  fixture.global_assigned_rows==4&&global_labels==labels_before&&global_formats==formats_before+1&&global_learn_calls==learn_calls_before&&
  !strcmp(li.row_text[1],"002 TARGET")&&!strcmp(li.assignment_text,"ASSIGNMENT: CH1 CC13 ABS")&&fixture.global_learn_state==NATIVE_GLOBAL_LEARN_ARMED,
  "owner pairing change repaints selected assignment detail without a new Learn intent");
 require(label_text_updates>detail_updates_before,
  "changed owner pairing refreshes the native assignment Label");
 click(cancel);require(global_learn_calls==2&&!requested_global_learn&&fixture.global_learn_cancels==1&&fixture.global_learn_state==NATIVE_GLOBAL_LEARN_CANCELING&&li.global_screen.screen,"Cancel queues false and retains the screen");
 settle_global_learn(false);native_preferences_refresh();
 require(fixture.global_learn_state==NATIVE_GLOBAL_LEARN_IDLE&&fixture.global_learn_settled==2&&!fixture.global_learn_owner_value&&!fixture.global_learn_mirror_value&&last_visible(learn)==1&&!strcmp(li.status_text,"SELECTED TARGET 2"),"cancel settles back to the selected idle target");
 click(mpc_ui_private_control_host_object(li.global_screen.clear_assignment));
 require(global_clears==1&&fixture.global_file_clears==1&&fixture.global_file_actions==11&&fixture.global_file_completed==11&&fixture.global_assigned_rows==3&&
  !strcmp(li.assignment_text,"ASSIGNMENT: UNASSIGNED")&&!strcmp(li.file_status_text,"ASSIGNMENT CLEARED"),"native Clear removes only the selected assignment and refreshes owner readback");
 void *edit_type=mpc_ui_private_control_host_object(li.global_screen.edit_type);
 click(edit_type);click(edit_type);click(edit_type);
 click(mpc_ui_private_control_host_object(li.global_screen.edit_channel_up));
 click(mpc_ui_private_control_host_object(li.global_screen.edit_data_up));
 click(mpc_ui_private_control_host_object(li.global_screen.edit_mode));
 click(mpc_ui_private_control_host_object(li.global_screen.edit_reverse));
 require(li.draft_dirty&&li.draft_mapping.type==2&&li.draft_mapping.channel==2&&li.draft_mapping.data1==1&&
  li.draft_mapping.control==6&&li.draft_mapping.reverse==1&&!strcmp(li.file_status_text,"EDIT READY - TAP APPLY"),
  "manual assignment controls modify only the transient selected-target draft");
 click(mpc_ui_private_control_host_object(li.global_screen.edit_apply));
 require(global_message_ctors==1&&global_message_dtors==1&&global_mapping_ctors==1&&global_mapping_commits==1&&
  global_mapping_frees==1&&global_signal_dtors==3&&global_control_sets==1&&global_reverse_sets==1&&
  fixture.global_edit_requests==1&&fixture.global_edit_completed==1&&!fixture.global_edit_no_changes&&
  fixture.global_edit_type==2&&fixture.global_edit_channel==2&&fixture.global_edit_data==1&&
  fixture.global_edit_control==6&&fixture.global_edit_reverse==1&&fixture.global_file_actions==12&&fixture.global_file_completed==12&&
  fixture.global_assigned_rows==4&&!strcmp(li.assignment_text,"ASSIGNMENT: CH2 CC1 REL CC OFFSET REV")&&
  !strcmp(li.file_status_text,"ASSIGNMENT APPLIED"),
  "Apply constructs a native MIDI message and Mapping, commits through the existing collection, then applies mode/reverse and balances temporaries");
 /* Existing CE row IDs stay stable while Akai's native enum adds Browser Help. */
 select_list(groups,13);
 require(li.selected_target==MPCLEARN_GLOBAL_LEARN_NO_SELECTION&&li.row_count==3&&strstr(li.row_text[2],"User Level Velocity"),"Additional preserves User Level Velocity in the existing 120-row list");
#ifdef MODEL_FIRMWARE_310
 const int32_t expected_native_ids[2]={119,120};
#else
 const int32_t expected_native_ids[2]={118,119};
#endif
 for(unsigned row=1;row<=2;row++){
  select_list(targets,row);
  require(li.selected_target==117u+row&&selected_global_target==expected_native_ids[row-1],"selected CE row reaches the exact native Target enum");
  li.draft_mapping={};li.draft_mapping.type=2;li.draft_mapping.channel=3;li.draft_mapping.control=5;li.draft_mapping.data1=73;li.draft_mapping.reverse=1;li.draft_dirty=1;
  require(apply_global_draft(li),"translated Target manual commit settles through native lookup");
  require(run_global_file_action(li,FILE_ACTION_CLEAR),"translated Target Clear settles through native lookup");
  require(native_target_calls[expected_native_ids[row-1]]==255u,"label/fixed/lookup/select/Clear/commit/mode/reverse all use independent native ID");
 }
#ifdef MODEL_FIRMWARE_310
 require(!native_target_calls[118],"new native Browser Help never aliases a CE row");
#endif
 select_list(groups,0);select_list(targets,1);
 printf("PASS fixed CE target rows preserve User Level Velocity across all five native adapter crossings; labels/lookup/select/Clear/manual commit exercised with independent native IDs (native bodies substituted)\n");
 click(learn);settle_global_learn(true);native_preferences_refresh();require(fixture.global_learn_state==NATIVE_GLOBAL_LEARN_ARMED&&fixture.global_learn_settled==3,"second arm settles before armed Back");
 void *back=mpc_ui_private_control_host_object(li.global_screen.back);click(back);
 require(fixture.back_clicks==1&&fixture.global_learn_cancels==2&&fixture.global_learn_state==NATIVE_GLOBAL_LEARN_CANCELING&&fixture.live_shells==1&&li.global_screen.screen&&!strcmp(li.status_text,"CANCELING BEFORE BACK"),"armed Back queues false and retains its binding");
 require(native_preferences_wheel(1)&&li.selected_target==1,
  "deferred Back keeps wheel input off the covered stock page until closure");
 native_preferences_refresh();require(li.global_screen.screen&&fixture.global_learn_pending==1,"Back cannot close on intent return");
 settle_global_learn(false);native_preferences_refresh();
 require(!li.global_screen.screen&&fixture.global_learn_settled==4&&fixture.shells_created==fixture.shells_destroyed&&!fixture.live_shells&&last_visible(page+8)==1,"Back closes only after false owner/mirror settlement and pending drain");
 click(launcher_button);require(fixture.shells_created==2&&fixture.live_shells==1,"launcher reopens a fresh toolkit shell");
 require(native_preferences_wheel(1)&&li.selected_target==MPCLEARN_GLOBAL_LEARN_NO_SELECTION,
  "visible editor consumes wheel before a list has focus without selecting a row");
 select_list(mpc_ui_private_list_host_object(li.global_screen.groups),1);
 require(native_preferences_wheel(1)&&li.selected_group==2&&li.page_start==24,
  "signed wheel step moves the touched group list through its native selection intent");
 require(native_preferences_wheel(-1)&&li.selected_group==1&&li.page_start==16,
  "reverse wheel step returns to the prior group");
 select_list(mpc_ui_private_list_host_object(li.global_screen.targets),0);
 require(native_preferences_wheel(2)&&li.selected_target==18&&selected_global_target==18,
  "counted wheel input selects a later target through the existing mapping owner");
 require(native_preferences_wheel(-1)&&li.selected_target==17&&selected_global_target==17,
  "reverse target input updates both list selection and owner target");
 click(mpc_ui_private_control_host_object(li.global_screen.edit_type));
 require(native_preferences_wheel(1)&&li.selected_target==17,
  "editing another control releases list focus but keeps wheel off the hidden page");
 ((void(*)(void*))(*(void***)overlay)[1])(overlay);
 require(fixture.launcher_overlays_created==fixture.launcher_overlays_destroyed&&fixture.launcher_tiles_created==fixture.launcher_tiles_destroyed&&fixture.shells_created==fixture.shells_destroyed&&fixture.global_screens_created==fixture.global_screens_destroyed&&!fixture.live_launcher_overlays&&!fixture.live_launcher_tiles&&!fixture.live_shells&&!fixture.live_global_screens&&!fixture.failures,"overlay teardown balances toolkit ownership");
 require(fixture.overlay_destroy_entries==1&&fixture.overlay_destroy_ui_thread_matches==1&&fixture.overlay_destroy_observer_running==1&&
  fixture.overlay_destroy_consumer_open==1&&!fixture.overlay_destroy_busy&&!fixture.overlay_destroy_callback_depth&&
  fixture.overlay_destroy_cleanup_complete==1&&fixture.overlay_destroy_stock_calls==1&&!fixture.overlay_destroy_retained,
  "Global owner teardown records eligible complete cleanup before the stock destructor");
 require(ordered_count==1&&ordered_children[0]==second_button,"stock child order retained after launcher removal");
 require(string_ctors==string_dtors&&button_ctors==button_dtors&&list_ctors==list_dtors&&allocations==deletes&&overlay_dtors==1&&child_removes==child_adds+1,"all toolkit adds and native-list lifetimes balance; one extra remove is the stock placeholder");
 require(list_viewport_gets==list_ctors&&list_drag_enables==list_ctors&&list_drag_disables==list_dtors&&list_background_colours>=list_ctors,"native lists configure drag and background, then disable drag before teardown");
 printf("PASS production native UI composition through canonical toolkit: Controllers, page-two launcher, opaque one-consumer app host, grouped native lists, exact owner capture, New/Copy/Export cancel/success/select/Clear, direct Import no-addition/collision/byte-copy registration, manual Mapping commit/mode/reverse cleanup, Learn/cancel settlement, changed-detail repaint, deferred armed Back, reopen and forced-owner balanced teardown (host ABI, chooser pixels, filesystem, MPC rendering, native scheduler, scrolling, touch, focus and stock ownership substituted)\n");
#endif
 return 0;
}
