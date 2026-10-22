#include "cardinal-consumer.cc"
#define ENTRY_APP MpclearnCardinalEditorApp
#define ENTRY_INIT mpclearn_cardinal_editor_app_init
#define ENTRY_CALLBACKS mpclearn_cardinal_editor_app_callbacks
#define ENTRY_TITLE mpclearn_cardinal_editor_set_title
#define ENTRY_BIND mpclearn_cardinal_editor_bind
#define ENTRY_IDLE animationIdle
#define ENTRY_EXPORT mpclearn_cardinal_editor_entry
#include "entry-wrapper.inc"
