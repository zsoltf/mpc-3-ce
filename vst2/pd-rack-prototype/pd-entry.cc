#include "pd-consumer.cc"
#define ENTRY_APP MpclearnPdEditorApp
#define ENTRY_INIT mpclearn_pd_editor_app_init
#define ENTRY_CALLBACKS mpclearn_pd_editor_app_callbacks
#define ENTRY_TITLE mpclearn_pd_editor_set_title
#define ENTRY_EXPORT mpclearn_pd_editor_entry
#define ENTRY_BIND mpclearn_pd_editor_bind
#include "entry-wrapper.inc"
