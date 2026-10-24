/* Native Type popup admission and its ordinary human-selected history result.
 * Menu rows/selection remain entirely native; no controller menu navigation. */
static Track *type_selected(const CommandRequest *r){
 ChannelCell *cell=channel_cell(project+channel_offset(CF_SELECTION));if(!cell||!atomic_load(&cell->live))return NULL;
 unsigned rev=atomic_load(&cell->revision),address=atomic_load(&cell->bits);if(!rev||(rev&1)||rev!=atomic_load(&cell->revision))return NULL;
 for(unsigned i=0;i<count;i++){Track *t=tracks+i;if(t->serial!=r->field_incarnation||t->track!=address)continue;
  if(t->kind>6||t->transition||channel_owner_id(t->track)!=t->track_owner||!typed(t->track,MODEL_TRACK_VPTR)||!vector(pool,MODEL_POOL_BEGIN,t->track)||word(t->track+0x648)!=t->program||word(t->track+0x64c)!=t->kind)return NULL;
  return t;
 }return NULL;
}
static unsigned type_open_selected(const CommandRequest *r){return type_selected(r)!=NULL;}
typedef struct {uint32_t tmi,launcher,controller,button,menu;} TypeOpenNative;
static int type_open_roots(TypeOpenNative *n,const CommandRequest *r){
 memset(n,0,sizeof(*n));Track *t=type_selected(r);if(!t||!root||root>UINT32_MAX-(MODEL_ROOT_TMI+4u))return 0;
 n->tmi=word(root+MODEL_ROOT_TMI);if(!typed(n->tmi,MODEL_TMI_VPTR)||word(n->tmi+0x124)!=root)return 0;
 n->launcher=word(n->tmi+MODEL_TMI_LAUNCHER);if(!typed(n->launcher,MODEL_CALLOUT_LAUNCHER_VPTR))return 0;
 uint32_t x=word(n->tmi+MODEL_TMI_MAIN_PAGE);if(!typed(x,MODEL_MAIN_PAGE_VPTR))return 0;x=word(x+0xe8);if(!typed(x,MODEL_MAIN_CONTENT_VPTR))return 0;x=word(x+0x9c);if(!typed(x,MODEL_MAIN_TRACK_PANEL_VPTR))return 0;
 n->controller=word(x+0xfc);if(!typed(n->controller,MODEL_TRACK_TAB_CONTROLLER_VPTR)||word(n->controller+0x38)!=n->launcher)return 0;
 uint32_t current=word(n->controller+0x2c);if(!pointer(current)||word(current+0xa0)!=t->track)return 0;
 n->button=word(n->controller+0x48);n->menu=word(n->controller+0x4c);
 return typed(n->button,MODEL_DRAWABLE_BUTTON_VPTR)&&word(word(n->button)+0xb4)==observer_image_bias+MODEL_TYPE_BUTTON_CLICK&&word(n->button+0x150)==observer_image_bias+MODEL_TYPE_ICON_INVOKER&&pointer(n->menu);
}
static int type_menu(uint32_t menu,uint32_t *begin,unsigned expected){
 if(!pointer(menu))return 0;
 uint32_t b=word(menu),e=word(menu+4),cap=word(menu+8);
 if(!pointer(b)||e<b||cap<e||e-b!=expected*12||(cap-b)%12||cap-b>256*12)return 0;
 *begin=b;return 1;
}
static unsigned type_open_native(Context *c,const CommandRequest *r,uint32_t editor){
 TypeOpenNative n;uint32_t tmi=word(root+MODEL_ROOT_TMI);
 if(!typed(tmi,MODEL_TMI_VPTR)||word(tmi+0x124)!=root||!typed(word(tmi+MODEL_TMI_LAUNCHER),MODEL_CALLOUT_LAUNCHER_VPTR))return C_NATIVE_MEMBERSHIP;
 if(word(word(tmi+MODEL_TMI_LAUNCHER)+0x30))return C_BUSY;
 unsigned handled=((unsigned (*)(void*,unsigned))(uintptr_t)(observer_image_bias+MODEL_EDITOR_ACTION))((void*)(uintptr_t)editor,MODEL_PAGE_ACTION_MAIN);
 if(handled>1||!type_open_roots(&n,r)||word(n.launcher+0x30))return C_SOURCE_CHANGED;
 ((void (*)(void*))(uintptr_t)(observer_image_bias+MODEL_TYPE_BUTTON_CLICK))((void*)(uintptr_t)n.button);
 uint32_t target=0,function=0;if(global_resolve(c,r,&target,&function)||target!=editor||!type_open_roots(&n,r))return C_SOURCE_CHANGED;
 Track *t=type_selected(r);if(!t)return C_IDENTITY;unsigned count=t->kind==2?6:5;uint32_t original,copy;
 uint32_t box=word(n.launcher+0x30);if(!typed(box,MODEL_CALLOUT_BOX_VPTR)||!typed(word(box+0x7c),MODEL_CALLOUT_COMPONENT_VPTR)||!type_menu(n.menu,&original,count)||!type_menu(word(n.launcher+0x2c),&copy,count))return C_SOURCE_CHANGED;
 static const uint32_t ids[]={0,1,3,4,5,6},invokers[]={MODEL_TYPE_ROW_INVOKER_0,MODEL_TYPE_ROW_INVOKER_1,MODEL_TYPE_ROW_INVOKER_2,MODEL_TYPE_ROW_INVOKER_3,MODEL_TYPE_ROW_INVOKER_4,MODEL_TYPE_ROW_INVOKER_5};unsigned j=0;
 for(unsigned i=0;i<6;i++)if(ids[i]!=t->kind){uint32_t a=original+j*12,b=copy+j*12,obj=word(a);
  if(!pointer(obj)||!pointer(word(a+4))||*(unsigned char*)(uintptr_t)(a+8)!=4||*(unsigned char*)(uintptr_t)(b+8)!=4||word(b)!=obj||word(b+4)!=word(a+4)||word(obj+0x24)!=observer_image_bias+invokers[i])return C_SOURCE_CHANGED;
  j++;
 }
 return C_OK;
}
static __thread struct {uint32_t command,sp,lr,epoch,project_owner,track,serial,owner,old_program,chosen,phase,binding;} type_do;
static int command_type_inventory(unsigned site,const Track *t){
 return site==293&&type_do.phase==2&&token()==command_owner&&!depth&&epoch==type_do.epoch&&channel_owner_id(project)==type_do.project_owner&&t->track==type_do.track&&t->serial==type_do.serial&&t->track_owner==type_do.owner&&channel_owner_id(t->track)==type_do.owner&&t->program==type_do.old_program&&t->transition==3&&t->replace_token==command_owner&&t->kind==type_do.chosen&&typed(t->track,MODEL_TRACK_VPTR)&&word(t->track+0x648)==t->replacement&&word(t->track+0x64c)==type_do.chosen;
}
static void type_do_fail(void){memset(&type_do,0,sizeof(type_do));mirror_stop(MV_LIFETIME);}
static void type_do_point(Context *c,unsigned site){
 if(type_do.phase&&(type_do.epoch!=epoch||type_do.project_owner!=channel_owner_id(project))){type_do_fail();return;}
 if(site==292){
  if(type_do.phase){type_do_fail();return;}
  uint32_t cmd=c->r[0];if(!active()||token()!=command_owner||atomic_load(&command_in_hook)||!ready||loading||reset_pending||depth||c->lr!=observer_image_bias+MODEL_TYPE_DO_CALLER_LR||!typed(cmd,MODEL_TYPE_COMMAND_VPTR)||word(cmd+0x2c)!=project)return;
  unsigned index=word(cmd+0x34),chosen=word(cmd+0x38);if(index>=count||chosen>6||chosen==2)return;Track *t=tracks+index;
  uint32_t begin=word(pool+MODEL_POOL_BEGIN),end=word(pool+MODEL_POOL_END);if(!pointer(begin)||end<begin||(end-begin)/4<=index||word(begin+index*4)!=t->track)return;
  if(t->transition||word(cmd+0x44)!=t->program||channel_owner_id(t->track)!=t->track_owner||!vector(pool,MODEL_POOL_BEGIN,t->track)||word(t->track+0x648)!=t->program)return;
  type_do=(__typeof__(type_do)){cmd,source_sp(c),c->lr,epoch,channel_owner_id(project),t->track,t->serial,t->track_owner,t->program,chosen,1,t->binding};return;
 }
 if(!type_do.phase)return;
 if(type_do.phase!=1||!active()||token()!=command_owner||!ready||loading||reset_pending||depth||c->r[4]!=type_do.command||source_sp(c)+0x70!=type_do.sp||word(source_sp(c)+0x6c)!=type_do.lr||(c->r[0]!=0&&c->r[0]!=3)){type_do_fail();return;}
 if(c->r[0]==0){
  type_do.phase=2;Track *t=NULL;for(unsigned i=0;i<count;i++)if(tracks[i].serial==type_do.serial)t=tracks+i;
  if(!t||t->track!=type_do.track||t->track_owner!=type_do.owner||channel_owner_id(t->track)!=type_do.owner||!typed(t->track,MODEL_TRACK_VPTR)||!vector(pool,MODEL_POOL_BEGIN,t->track)||word(t->track+0x64c)!=type_do.chosen){type_do_fail();return;}
  if(t->transition){
   if(!command_type_inventory(site,t)){type_do_fail();return;}
   bump(&generation);inventory(site);export_topology();
  }else{
   /* An optional native inventory notification may already have consumed the
    * replacement. Its newer binding is required before accepting a new Program. */
   if(word(t->track+0x648)!=t->program||(t->program&&channel_owner_id(t->program)!=t->program_owner)||(t->program!=type_do.old_program&&t->binding<=type_do.binding)){type_do_fail();return;}
   if(t->kind!=type_do.chosen){t->kind=type_do.chosen;t->binding=bump(&binding);bump(&generation);export_topology();}
  }
 }
 memset(&type_do,0,sizeof(type_do));
}
