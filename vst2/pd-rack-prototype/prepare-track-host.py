#!/usr/bin/env python3
"""Add standalone Instrument Edit lifecycle admission to the native host."""
from pathlib import Path
import shutil
import sys

kit = Path(__file__).resolve().parent
stage = Path(sys.argv[1])
host = stage / "controllers/program-observer"
shutil.copyfile(kit / "track-host.cc", stage / "ui/examples/track-host.cc")
shutil.copyfile(kit / "screen-qlinks.inc", stage / "ui/examples/screen-qlinks.inc")
shutil.copyfile(kit / "track-host-profile.h", stage / "ui/examples/track-host-profile.h")
shutil.copyfile(host / "qlink-order.h", stage / "ui/examples/qlink-order.h")
# MPC 3.10 admission reads the same owners from traced coordinates; the
# exact 3.10 Preferences preparation merges them with its own.
shutil.copyfile(kit / "track-host-310-coordinates.json", host / "track-host-310-coordinates.json")


def replace(text, before, after):
    if text.count(before) != 1:
        raise SystemExit(f"track preparation anchor changed: {before}")
    return text.replace(before, after)


path = host / "native-preferences-prepare.py"
text = path.read_text()
ranges = [
    (0x0120d6e8, 0x6c, "Program Screen TabIndex listener registration"),
    (0x0120cb3c, 0x24, "Program Screen properties and selected Program correlation"),
    (0x012098ac, 0xe8, "Program Screen TabIndex pair and bank rebuild"),
    (0x03916ae0, 0x14, "Instrument Edit same TabIndex properties"),
    (0x011cf3d8, 0x94, "Screen descriptor job pending owner"),
    (0x011d21fc, 0x8c, "Screen descriptor publication before pending release"),
    (0x011cfec8, 0xa8, "Screen nested label job pending owner"),
    (0x011ceda4, 0xf4, "Screen label publication before pending release"),
    (0x01207ab8, 0x108, "Program Screen common construction and type"),
    (0x01207dc4, 4, "Program Screen type literal"),
    (0x011d379c, 0xe0, "Screen common same label owner passed to base and offset 98"),
    (0x011b7220, 0x1a8, "Screen base label owner at offset 48"),
    (0x011cdb14, 0x50, "FX Screen Q-Link input listener"),
    (0x011cdc1c, 0x50, "FX Screen Q-Link activity listener"),
    (0x011fc4a0, 0x50, "Program Screen Q-Link input listener"),
    (0x011fc29c, 0x50, "Program Screen Q-Link activity listener"),
    (0x014e0634, 0x50, "Q-Link provider descriptor assignment"),
    (0x014dd398, 0x5c4, "Q-Link descriptor complete owned callable assignment and release"),
    # The command observer already admits and detours the first two words.
    # This adapter admits the remaining live bytes, as for Mapping retirement.
    (0x014e0414, 0x48, "Q-Link provider actual value feedback after command hook"),
    (0x014e6768, 0x50, "Q-Link label publication"),
    (0x00961120, 0x50, "native String construction"),
    (0x00925c88, 0x40, "native String retained copy"),
    (0x00911f58, 0x40, "native String destruction"),
    (0x00bc2a6c, 0x284, "MouseEvent source construction for image contacts"),
    (0x00bcf610, 0x248, "MouseInputSource index and type for image contacts"),
    (0x01aef8e4, 0xb8, "stock text session constructor"),
    (0x01aefc2c, 0x360, "stock text session copy and open"),
    (0x01aeff94, 0x1f0, "stock text active-session change and close"),
    (0x035d8078, 0xa8, "stock terminal text callbacks"),
    (0x00925d58, 0xb0, "stock String assignment for keyboard session"),
    (0x0093352c, 0x0c, "stock String UTF-8 borrow for keyboard result"),
    (0x03917070, 0x5b8, "TouchProgramEditPage activation and selected Program"),
    (0x03914f68, 0x90, "TouchProgramEditPage selected Program field"),
    (0x03915548, 0x1e4, "TouchProgramEditPage destruction before children"),
    (0x035cede8, 0xa0, "Instrument Edit paint surrounds only the native Q-Link outline"),
    (0x031bb884, 8, "Instrument Edit before/after paint hooks are no-ops"),
    (0x039a0e10, 0x628, "Plugin tabset owner transition before retirement"),
    (0x008ca97c, 8, "VST platform data returns exact AEffect"),
    (0x036518e0, 0x698, "Insert overlay activation and current session"),
    (0x03651f78, 0x718, "Insert overlay destruction before children"),
    (0x0365151c, 0x3c4, "Insert overlay session transition before retirement"),
    (0x03663da0, 0x38c, "Insert overlay content parent and header layout"),
    (0x0364bde8, 8, "Insert overlay stock close request"),
    (0x0364c09c, 0x21c, "Insert overlay Q-Link paint over children"),
    (0x037fa614, 12, "Insert overlay close signal veneer"),
    (0x00e977bc, 0x130, "Insert overlay close signal dispatch"),
    (0x01a47a70, 0x68, "EditEffectSession owner and selected insert"),
    (0x0235d10c, 0x60, "Insert UI and audio vectors"),
]
text = replace(text, "\nranges=[\n", "\nranges=[\n" + "".join(f" {r!r},\n" for r in ranges))
# No PC-relative instruction is displaced. The destructor entry's initial
# literal load and stack save stay in their original location.
text = replace(text, "site_bytes=[]", """sites += [
 (0x03917078,'f0426de1f860cde1','Instrument Edit activation'),
 (0x0391554c,'f0416de1385190e5','Instrument Edit before destruction'),
 (0x039a0e10,'f4426de1f860cde1','Plugin tabset before owner transition'),
 (0x036518e8,'f0426de1f860cde1','Insert editor activation'),
 (0x03651f7c,'f4426de10050a0e1','Insert editor before destruction'),
 (0x0365151c,'f4426de1003091e5','Insert editor before session transition'),
]
site_bytes=[]""")
text = replace(text, "pointers=[", """pointers=[
 (0x068e6d08,0x011cdb14,'FX Screen Q-Link input'),
 (0x068e6d10,0x011cdc1c,'FX Screen Q-Link activity'),
 (0x068e76c8,0x011fc4a0,'Program Screen Q-Link input'),
 (0x068e76d0,0x011fc29c,'Program Screen Q-Link activity'),
 (0x069eeae0,0x069eec0c,'Instrument Edit page typeinfo'),
 (0x069eeae4+0x20,0x031bb884,'Instrument Edit before-paint no-op'),
 (0x069eeae4+0x24,0x031bb888,'Instrument Edit after-paint no-op'),
 (0x069eeb30,0x069eec0c,'Instrument Edit Component typeinfo'),
 (0x069eeb34+0x68,0x035cede8,'Instrument Edit Component paint'),
 (0x069eebe8,0x069eec0c,'Instrument Edit Component table end'),
 (0x06932a44,0x06932e08,'PluginProgram typeinfo'),
 (0x06889888+0x124,0x008ca97c,'VST platform data method'),
 (0x069c9f20,0x069ca0f4,'Insert editor overlay typeinfo'),
 (0x069c9f94,0x069ca0f4,'Insert overlay Component typeinfo'),
 (0x069c9f98+0x6c,0x0364c09c,'Insert overlay paintOverChildren'),
 (0x069ca04c,0x069ca0f4,'Insert overlay Component table end'),
 (0x069cad6c,0x069cad20,'Insert editor content typeinfo'),
 (0x0689e670,0x06904458,'EditEffectSession typeinfo'),
 (0x068ac36c,0x0692e380,'Insert owner typeinfo'),
 (0x069c9f24+0x5c,0x0364bde8,'Insert overlay close request method'),""")
text = replace(text, "'#define NATIVE_PREFERENCES_SITE_COUNT 4u'", "'#define NATIVE_PREFERENCES_SITE_COUNT 10u'")
path.write_text(text)

path = host / "native-preferences-310.py"
text = path.read_text()
text = replace(text, "        raise SystemExit('native Preferences requires its four existing hook owners')\n",
"""        raise SystemExit('native Preferences requires its four existing hook owners')
    editors = json.loads((here / 'track-host-310-coordinates.json').read_text())
    if editors['stock_sha256'] != identity['input_sha256']:
        raise SystemExit('track editor coordinate source pin differs')
    # Hook order is the dispatch kind: Instrument active/destroy, tabset,
    # insert active/destroy/session transition follow the four owners.
    ranges, sites, pointers = ranges + editors['ranges'], sites + editors['sites'], pointers + editors['pointers']
""")
text = replace(text, "             '#define NATIVE_PREFERENCES_SITE_COUNT 4u',", "             f'#define NATIVE_PREFERENCES_SITE_COUNT {len(sites)}u',")
path.write_text(text)

path = host / "native-preferences.cc"
text = path.read_text()
declarations = '''extern "C" void mpclearn_track_editor_configure(uint32_t);
extern "C" void mpclearn_track_editor_active(void*,int);
extern "C" void mpclearn_track_editor_changed();
extern "C" void mpclearn_track_editor_destroy(void*);
extern "C" void mpclearn_track_editor_refresh();
extern "C" void mpclearn_insert_editor_active(void*);
extern "C" void mpclearn_insert_editor_changed(void*);
'''
text = declarations + text
text = replace(text, "configure_host(bias);}", "configure_host(bias);mpclearn_track_editor_configure(bias);}")
text = replace(text, " process_global_pending();", " mpclearn_track_editor_refresh();\n process_global_pending();")
text = replace(text, " else failure(NP_BAD_CONTEXT);", """ else if(kind==4)mpclearn_track_editor_active((void*)(uintptr_t)context->r[0],context->r[1]);
 else if(kind==5||kind==8)mpclearn_track_editor_destroy((void*)(uintptr_t)context->r[0]);
 else if(kind==6)mpclearn_track_editor_changed();
 else if(kind==7)mpclearn_insert_editor_active((void*)(uintptr_t)context->r[0]);
 else if(kind==9)mpclearn_insert_editor_changed((void*)(uintptr_t)context->r[0]);
 else failure(NP_BAD_CONTEXT);""")
# uint32_t declarations must follow the existing includes.
text = text.removeprefix(declarations)
text = replace(text, "static uint32_t image_bias;", declarations + "static uint32_t image_bias;")
text += '''
extern "C" void* mpclearn_track_text_owner() {
 if(!global_ready()||!global_capture.app)return nullptr;
 // Native stock Name qualification matches this application-owned service
 // to TextEditorLauncher::core and to the startup active-session callback.
 #ifdef MODEL_FIRMWARE_310
 return *(void**)((unsigned char*)global_capture.app+0x130);
#else
 return *(void**)((unsigned char*)global_capture.app+0x12c);
#endif
}
'''
path.write_text(text)

path = host / "command-capture.c"
text = path.read_text()
text = '#ifdef NATIVE_PREFERENCES\nextern void mpclearn_insert_editor_inserts_changing(void*);\nextern void mpclearn_track_editor_root_retiring(void);\nextern void mpclearn_track_qlink_publication(unsigned,unsigned,unsigned,unsigned,unsigned);\n#endif\n' + text
text = replace(text, '  if(site==M_ROOT_DESTROY)global_root(site);', '''  if(site==M_ROOT_DESTROY){
#ifdef NATIVE_PREFERENCES
   mpclearn_track_editor_root_retiring();
#endif
   global_root(site);
  }''')
text = replace(text, ' effects_lifecycle(c,site);', '''#ifdef NATIVE_PREFERENCES
 if(site==PX_UI_VECTOR&&c->r[0]>=0x50)
  mpclearn_insert_editor_inserts_changing((void*)(uintptr_t)(c->r[0]-0x50));
#endif
 effects_lifecycle(c,site);''')
# Relay existing completed-publication sites independently of diagnostic demand.
# Their full methods are already admitted by model-prepare.py. Native prologues
# put entry SP/saved caller LR at descriptor +f8/+f4 and labels +50/+4c;
# exact invocation provenance preserves nested listener publications.
text = replace(text, ' if(site>=QS_DESCRIPTOR&&site<=QS_LABELS&&site!=QJ_VALUE_FANOUT){', '''#ifdef NATIVE_PREFERENCES
 if(site==QS_DESCRIPTOR)mpclearn_track_qlink_publication(1,c->r[4],c->r[5],source_sp(c)+0xf8,word(source_sp(c)+0xf4));
 if(site==QS_LABELS)mpclearn_track_qlink_publication(2,c->r[5],word(source_sp(c)+4),source_sp(c)+0x50,word(source_sp(c)+0x4c));
#endif
 if(site>=QS_DESCRIPTOR&&site<=QS_LABELS&&site!=QJ_VALUE_FANOUT){''')
# Use the already qualified native page dispatcher on the same UI-drain
# thread. No second command writer or synthetic controller input is added.
text += '''
#ifdef MODEL_FIRMWARE_310
#define TRACK_QLINK_SCREEN_VPTR 0x6ca4e90u
#define TRACK_QLINK_FX_MODE 0x54u
#define TRACK_QLINK_PROGRAM_MODE 0x78u
#define TRACK_QLINK_FX_VPTR 0x6d12924u
#define TRACK_QLINK_PROGRAM_VPTR 0x6d13160u
#else
#define TRACK_QLINK_SCREEN_VPTR 0x68975b4u
#define TRACK_QLINK_FX_MODE 0x50u
#define TRACK_QLINK_PROGRAM_MODE 0x74u
#define TRACK_QLINK_FX_VPTR 0x68e6cacu
#define TRACK_QLINK_PROGRAM_VPTR 0x68e764cu
#endif
int mpclearn_track_qlink_native(unsigned insert,uint32_t output[8]) {
 if(!output||!active()||!ready||loading||reset_pending||depth||token()!=command_owner||
    !root||!project||!command_queues||!typed(root,MODEL_ROOT_VPTR)||word(root+MODEL_ROOT_PROJECT)!=project)return 0;
 uint32_t controls=word(root+MODEL_ROOT_CONTROLS),wrapper=word(root+MODEL_ROOT_QLINK_WRAPPER),queue=word(root+MODEL_ROOT_QUEUES);
 if(!qlink_span(controls,0x3c)||!qlink_span(wrapper,12)||queue!=command_queues)return 0;
 uint32_t provider=word(controls+0x30),labels=word(controls+0x38),touch=word(controls+0x2c);
 if(!qlink_span(provider,0x38)||!qlink_span(labels,0x20)||!typed(touch,MODEL_QLINK_TOUCH_VPTR)||
    word(wrapper)!=queue||word(wrapper+8)!=provider||word(provider+0x28)!=16||
    word(labels+0x10)!=16||word(labels+0x18)!=16||word(labels+0x1c)!=16||
    !qlink_span(word(provider+0x30),16*0x68)||!qlink_span(word(provider+0x34),16*4)||
    !qlink_span(word(labels+0x14),16*12))return 0;
 uint32_t owner=word(root+MODEL_ROOT_QLINK_OWNER);if(!qlink_span(owner,MODEL_QLINK_OWNER_SPAN))return 0;
 uint32_t controller=word(owner+MODEL_QLINK_OWNER_CONTROLLER);
 if(!typed(controller,MODEL_QLINK_CONTROLLER_VPTR)||word(controller+8)!=queue||word(controller+0x5c)!=0)return 0;
 uint32_t screen=word(controller+0xfc);if(!typed(screen,TRACK_QLINK_SCREEN_VPTR))return 0;
 uint32_t mode=word(screen+(insert?TRACK_QLINK_FX_MODE:TRACK_QLINK_PROGRAM_MODE));
 if(!typed(mode,insert?TRACK_QLINK_FX_VPTR:TRACK_QLINK_PROGRAM_VPTR)||
    !*(const unsigned char*)(uintptr_t)(mode+0x30)||word(mode+0x40)!=queue||
    word(mode+0x48)!=labels||word(mode+0x50)!=provider||word(mode+0x54)!=touch)return 0;
 output[0]=root;output[1]=queue;output[2]=mode;output[3]=controller;
 output[4]=wrapper;output[5]=provider;output[6]=labels;output[7]=controls;
 return 1;
}
int mpclearn_track_editor_main(void) {
 if(!active()||!ready||loading||reset_pending||depth||token()!=command_owner||!mirror_state||!root||root<0xd0||!project)return 0;
 GeneralState *s=&mirror_state->general;
 uint32_t editor=atomic_load(&s->editor),owner=atomic_load(&s->editor_owner);
 if(!owner||atomic_load(&s->editor_root)!=root||channel_owner_id(editor)!=owner||
    word(root-0xd0+MODEL_APP_EDITOR)!=editor||!typed(editor,MODEL_EDITOR_VPTR)||
    !typed(editor+0x230,MODEL_EDITOR_SECONDARY_VPTR)||word(editor+0x37c)!=root)return 0;
 return ((unsigned (*)(void*,unsigned))(uintptr_t)(observer_image_bias+MODEL_EDITOR_ACTION))((void*)(uintptr_t)editor,MODEL_PAGE_ACTION_MAIN)==1;
}
'''
path.write_text(text)

path = host / "native-preferences-build.sh"
text = path.read_text()
text = replace(text, "for editor in pd cardinal; do", "for editor in pd cardinal track-host; do")
text = replace(text, "/ui/examples/$editor-entry.cc", "/ui/examples/${editor}-entry.cc")
# Keep one compile loop; the track adapter has no consumer wrapper.
shutil.copyfile(kit / "track-host.cc", stage / "ui/examples/track-host-entry.cc")
text = replace(text, "/out/pd-entry.o /out/cardinal-entry.o /out/patch-files.o", "/out/pd-entry.o /out/cardinal-entry.o /out/track-host-entry.o /out/patch-files.o")
path.write_text(text)
print("Prepared Instrument Edit and insert-editor lifecycle; launcher remains Global MIDI Learn")
