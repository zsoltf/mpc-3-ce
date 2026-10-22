#!/usr/bin/env python3
"""Retain Global Learn through its original owner on the one admitted app host."""
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

def replace(before, after):
    global text
    assert text.count(before) == 1, before
    text = text.replace(before, after)

replace("MpclearnToolkitExampleApp example_app;uint32_t example_open_counted;",
        "MpclearnToolkitExampleApp example_app;uint32_t example_open_counted;\n uint32_t patch_entry_global,patch_entry_next,patch_entry_open_requested;")
replace(" }else if(intent==MPCLEARN_TOOLKIT_EXAMPLE_BACK){", """ }else if(intent==MPCLEARN_TOOLKIT_EXAMPLE_GLOBAL_LEARN){
  if(instance.patch_entry_global||instance.patch_entry_next||!mpc_ui_app_request_close(instance.app_host)){
   failure(NP_TOOLKIT_NAVIGATION);return;
  }
  instance.patch_entry_next=1;
 }else if(intent==MPCLEARN_TOOLKIT_EXAMPLE_BACK){""")
replace("#endif\nstatic bool register_launcher_app", """// Only the active presentation receives lifecycle callbacks. Global Learn's
// original cancellation, persistence and forced-close rules remain authoritative.
static int patch_entry_open(void *owner,MpcUiAppHost *host,MpcUiContext *context,MpcUiParent *parent,MpcUiRect bounds) {
 auto &instance=*(LauncherInstance*)owner;
 return instance.patch_entry_global?global_app_callbacks.open(owner,host,context,parent,bounds):
  mpclearn_toolkit_example_app_callbacks()->open(&instance.example_app,host,context,parent,bounds);
}
static int patch_entry_prepare(void *owner,int forced) {
 auto &instance=*(LauncherInstance*)owner;
 return instance.patch_entry_global?global_app_callbacks.prepare_close(owner,forced):
  mpclearn_toolkit_example_app_callbacks()->prepare_close(&instance.example_app,forced);
}
static int patch_entry_close(void *owner) {
 auto &instance=*(LauncherInstance*)owner;
 return instance.patch_entry_global?global_app_callbacks.close(owner):
  mpclearn_toolkit_example_app_callbacks()->close(&instance.example_app);
}
static const MpcUiAppCallbacks patch_entry_callbacks={sizeof(MpcUiAppCallbacks),patch_entry_open,patch_entry_prepare,patch_entry_close};
#endif
static bool register_launcher_app""")
replace("mpc_ui_app_register(host,mpclearn_toolkit_example_app_callbacks(),&instance.example_app);",
        "mpc_ui_app_register(host,&patch_entry_callbacks,&instance);")
replace("#if MPCLEARN_NATIVE_UI_EXAMPLE\n return true;\n#else\n return instance.file_action", """#if MPCLEARN_NATIVE_UI_EXAMPLE
 if(!instance.patch_entry_global)return true;
#endif
 return instance.file_action""")
replace("(!state||!__atomic_load_n(&state->global_learn_pending,__ATOMIC_ACQUIRE));\n#endif\n}",
        "(!state||!__atomic_load_n(&state->global_learn_pending,__ATOMIC_ACQUIRE));\n}")
replace("""  if(!mpc_ui_app_open(instance->app_host)){failure(NP_LAUNCHER_CONSTRUCTION);return;}
#if MPCLEARN_NATIVE_UI_EXAMPLE
  instance->example_open_counted=1;
  if(state){increment(&state->example_opens);increment(&state->live_examples);increment(&state->shells_created);increment(&state->live_shells);}
#endif""", """#if MPCLEARN_NATIVE_UI_EXAMPLE
  // The launcher is itself a widget callback. Construct only at the existing
  // UI drain so a partially failed editor can legally release its screens.
  instance->patch_entry_open_requested=1;
#else
  if(!mpc_ui_app_open(instance->app_host)){failure(NP_LAUNCHER_CONSTRUCTION);return;}
#endif""")
replace(""" for(auto &instance:launcher_instances)if(instance.overlay&&instance.app_host){
  if(instance.resume_requested){""", """ for(auto &instance:launcher_instances)if(instance.overlay&&instance.app_host){
#if MPCLEARN_NATIVE_UI_EXAMPLE
  if(instance.patch_entry_open_requested){
   instance.patch_entry_open_requested=0;
   if(!mpc_ui_app_open(instance.app_host))failure(NP_LAUNCHER_CONSTRUCTION);
   else {
    instance.example_open_counted=1;
    if(state){increment(&state->example_opens);increment(&state->live_examples);increment(&state->shells_created);increment(&state->live_shells);}
   }
  }
#endif
  if(instance.resume_requested){""")
replace("else if(!mpc_ui_app_is_open(instance.app_host))example_mark_closed(instance);", """else if(!mpc_ui_app_is_open(instance.app_host)){
    example_mark_closed(instance);
    // This existing drain runs after widget callbacks unwind. Close every
    // editor presentation before opening the original Global Learn screen.
    instance.patch_entry_global=instance.patch_entry_next;
    instance.patch_entry_next=0;
    if(instance.patch_entry_global&&!mpc_ui_app_open(instance.app_host)){
     if(!mpc_ui_app_is_open(instance.app_host))instance.patch_entry_global=0;
     failure(NP_LAUNCHER_CONSTRUCTION);
    }
   }""")
path.write_text(text)
print("Retained Global Learn callbacks, cancellation and suspension on existing app host")
