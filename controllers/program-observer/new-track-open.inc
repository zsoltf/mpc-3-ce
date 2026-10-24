/* Open the native Add Track selector only. Human selection runs through MPC's
 * normal command/history path after this invocation returns. No menu navigation,
 * track factory call, retained popup pointer or recurring observer is added. */
typedef struct {uint32_t tmi,pages,launcher;} NewTrackOpen;
static int new_track_open_roots(NewTrackOpen *n){
 if(!root||root>UINT32_MAX-(MODEL_ROOT_TMI+4u)||!typed(root,MODEL_ROOT_VPTR))return 0;
 n->tmi=word(root+MODEL_ROOT_TMI);n->pages=word(root+MODEL_ROOT_PAGES);
 if(!typed(n->tmi,MODEL_TMI_VPTR)||n->tmi>UINT32_MAX-(MODEL_TMI_LAUNCHER+4u)||word(n->tmi+0x124)!=root||!pointer(n->pages)||word(n->tmi+0x128)!=n->pages)return 0;
 n->launcher=word(n->tmi+MODEL_TMI_LAUNCHER);
 return typed(n->launcher,MODEL_CALLOUT_LAUNCHER_VPTR)&&n->launcher<=UINT32_MAX-0x34;
}
static unsigned new_track_page_open(uint32_t pages,unsigned page){
 return ((unsigned (*)(void*,unsigned))(uintptr_t)(observer_image_bias+MODEL_PAGE_IS_OPEN))((void*)(uintptr_t)pages,page)!=0;
}
static unsigned new_track_open_native(Context *c,const CommandRequest *r,uint32_t editor){
 NewTrackOpen before,after;
 if(!new_track_open_roots(&before))return C_NATIVE_MEMBERSHIP;
 if(word(before.launcher+0x30)||new_track_page_open(before.pages,MODEL_PLUGIN_CHOOSER_PAGE)||new_track_page_open(before.pages,MODEL_ADD_TRACK_PAGE))return C_BUSY;
 uint32_t fn[4]={0,0,0,0};
 ((void (*)(void*,unsigned,uint32_t*))(uintptr_t)(observer_image_bias+MODEL_PAGE_SHOW))((void*)(uintptr_t)before.pages,MODEL_ADD_TRACK_PAGE,fn);
 if(fn[2])((void (*)(uint32_t*,uint32_t*,unsigned))(uintptr_t)fn[2])(fn,fn,3);
 uint32_t target=0,function=0;unsigned why=global_resolve(c,r,&target,&function);
 if(why)return why;
 if(target!=editor||!new_track_open_roots(&after)||before.tmi!=after.tmi||before.pages!=after.pages||before.launcher!=after.launcher||!new_track_page_open(after.pages,MODEL_ADD_TRACK_PAGE))return C_SOURCE_CHANGED;
 return C_OK;
}
