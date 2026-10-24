/* Expected addresses are the accepted 3.9.1 resolver at 6a470e1 and the
 * separately traced 3.10 receipt, never the selected production macros. */
static void native_resolver_checks(){
 const uint32_t bias=0x10000u;
 configure_host(bias);
 require(image_bias==bias,"resolver retains actual nonzero load bias");
#ifdef MODEL_FIRMWARE_310
#include "native-preferences-resolver-310.inc"
#else
 require((uintptr_t)core.allocate==bias+0x037fa0e0u,"native resolver core.allocate");
 require((uintptr_t)core.sized_delete==bias+0x037fa32cu,"native resolver core.sized_delete");
 require((uintptr_t)core.set_visible==bias+0x00bbf2fcu,"native resolver core.set_visible");
 require((uintptr_t)core.repaint==bias+0x00b8f258u,"native resolver core.repaint");
 require((uintptr_t)preferences.std_string_ctor==bias+0x03665c60u,"native resolver preferences.std_string_ctor");
 require((uintptr_t)preferences.tab_ctor==bias+0x0369d174u,"native resolver preferences.tab_ctor");
 require((uintptr_t)preferences.tab_dtor==bias+0x0369cdecu,"native resolver preferences.tab_dtor");
 require((uintptr_t)preferences.tab_remove==bias+0x036a047cu,"native resolver preferences.tab_remove");
 require((uintptr_t)preferences.tab_vtable==bias+0x069cd0b4u,"native resolver preferences.tab_vtable");
 require((uintptr_t)launcher.overlay_add==bias+0x03664150u,"native resolver launcher.overlay_add");
 require((uintptr_t)launcher.remove_child==bias+0x00bc5c00u,"native resolver launcher.remove_child");
 require((uintptr_t)launcher.overlay_complete==bias+0x03663334u,"native resolver launcher.overlay_complete");
 require((uintptr_t)launcher.overlay_deleting==bias+0x03663404u,"native resolver launcher.overlay_deleting");
 require((uintptr_t)launcher.overlay_vtable==bias+0x069cabacu,"native resolver launcher.overlay_vtable");
 require((uintptr_t)global_host.file_handler==bias+0x018b49f8u,"native resolver global_host.file_handler");
 require((uintptr_t)global_host.target_fixed==bias+0x018bdd90u,"native resolver global_host.target_fixed");
 require((uintptr_t)global_host.target_dtor==bias+0x018bdc20u,"native resolver global_host.target_dtor");
 require((uintptr_t)global_host.lookup==bias+0x018b5840u,"native resolver global_host.lookup");
 require((uintptr_t)global_host.format_mapping==bias+0x018bed00u,"native resolver global_host.format_mapping");
 require((uintptr_t)global_host.target_label==bias+0x02fa6e84u,"native resolver global_host.target_label");
 require((uintptr_t)global_host.string_dtor==bias+0x00911f58u,"native resolver global_host.string_dtor");
 require((uintptr_t)global_host.select_target==bias+0x018737dcu,"native resolver global_host.select_target");
 require((uintptr_t)global_host.set_learn_intent==bias+0x02ad5e0cu,"native resolver global_host.set_learn_intent");
 require((uintptr_t)global_host.file_name==bias+0x018a7da8u,"native resolver global_host.file_name");
 require((uintptr_t)global_host.duplicate_file==bias+0x02ad5f88u,"native resolver global_host.duplicate_file");
 require((uintptr_t)global_host.create_file==bias+0x02ad7adcu,"native resolver global_host.create_file");
 require((uintptr_t)global_host.export_file==bias+0x018b2068u,"native resolver global_host.export_file");
 require((uintptr_t)global_host.select_file==bias+0x02ad71e0u,"native resolver global_host.select_file");
 require((uintptr_t)global_host.clear_target==bias+0x02ad60f0u,"native resolver global_host.clear_target");
 require((uintptr_t)global_host.note_message==bias+0x007d6d8cu,"native resolver global_host.note_message");
 require((uintptr_t)global_host.cc_message==bias+0x007d6d4cu,"native resolver global_host.cc_message");
 require((uintptr_t)global_host.pitch_message==bias+0x007d6c90u,"native resolver global_host.pitch_message");
 require((uintptr_t)global_host.message_dtor==bias+0x007d594cu,"native resolver global_host.message_dtor");
 require((uintptr_t)global_host.mapping_ctor==bias+0x018ba570u,"native resolver global_host.mapping_ctor");
 require((uintptr_t)global_host.commit_mapping==bias+0x018b7558u,"native resolver global_host.commit_mapping");
 require((uintptr_t)global_host.set_control==bias+0x018b6220u,"native resolver global_host.set_control");
 require((uintptr_t)global_host.set_reverse==bias+0x018b6360u,"native resolver global_host.set_reverse");
 require((uintptr_t)global_host.signal_dtor==bias+0x01efc91cu,"native resolver global_host.signal_dtor");
 require((uintptr_t)global_host.free_block==bias+0x004a68f4u,"native resolver global_host.free_block");
 require((uintptr_t)global_host.string_ctor==bias+0x00961120u,"native resolver global_host.string_ctor");
 require((uintptr_t)global_host.file_ctor==bias+0x00986680u,"native resolver global_host.file_ctor");
 require((uintptr_t)global_host.user_midi_directory==bias+0x015a7cf0u,"native resolver global_host.user_midi_directory");
 require((uintptr_t)global_host.chooser_initial_directory==bias+0x015a8034u,"native resolver global_host.chooser_initial_directory");
 require((uintptr_t)global_host.filename==bias+0x00956d44u,"native resolver global_host.filename");
 require((uintptr_t)global_host.string_utf8==bias+0x0093352cu,"native resolver global_host.string_utf8");
 require((uintptr_t)global_host.child_file==bias+0x009866f8u,"native resolver global_host.child_file");
 require((uintptr_t)global_host.file_exists==bias+0x0094da2cu,"native resolver global_host.file_exists");
 require((uintptr_t)global_host.file_exists_as_file==bias+0x0094da68u,"native resolver global_host.file_exists_as_file");
 require((uintptr_t)global_host.copy_file==bias+0x0096b708u,"native resolver global_host.copy_file");
 require((uintptr_t)global_host.open_mapping==bias+0x018bed5cu,"native resolver global_host.open_mapping");
 require((uintptr_t)global_host.register_mapping==bias+0x018ad794u,"native resolver global_host.register_mapping");
 require((uintptr_t)global_host.select_handler_file==bias+0x018b3420u,"native resolver global_host.select_handler_file");
 require((uintptr_t)global_host.release_control==bias+0x00c96a68u,"native resolver global_host.release_control");
 require(NP_VPTR_GLOBAL_FILE==0x0689c878u,"selected native XmmFile type guard");
 require(NP_VPTR_GLOBAL_CHOOSER==0x0689c134u&&NP_RVA_GLOBAL_CHOOSER_OPEN==0x01801cd4u,"selected native chooser type and open slot guard");
#endif
 require(!global_host.chooser_open,"chooser entry remains dynamically typed before dispatch");
 image_bias=0; /* Subsequent host substitutions deliberately use zero-bias objects. */
 printf("PASS actual native Preferences resolver at nonzero bias matches independent selected targets/tables (native bodies not called)\n");
}
