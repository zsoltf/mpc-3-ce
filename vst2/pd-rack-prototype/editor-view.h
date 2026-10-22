#ifndef MPCLEARN_EDITOR_VIEW_H
#define MPCLEARN_EDITOR_VIEW_H
#include <stdint.h>
/* Presentation intents shared by the two borrowed views. Coordinates are
 * local canvas pixels. The engine alone converts them to canonical geometry. */
enum MpcEditorMode { MPC_EDITOR_CONTROLS, MPC_EDITOR_EDIT, MPC_EDITOR_PAN };
enum MpcEditorViewIntent {
    MPC_EDITOR_VIEW_MODE = 1, MPC_EDITOR_VIEW_PAN, MPC_EDITOR_VIEW_ZOOM,
    MPC_EDITOR_VIEW_FIT, MPC_EDITOR_VIEW_FOCUS,
    /* Presentation-only cache reuse. Canonical geometry still follows every
     * input. END also follows cancellation, after all contacts have lifted. */
    MPC_EDITOR_VIEW_NAVIGATION_BEGIN, MPC_EDITOR_VIEW_NAVIGATION_END,
    MPC_EDITOR_VIEW_ZOOM_SELECTION
};
typedef struct {
    uint32_t id, control;
    char name[128], detail[256];
} MpcEditorItem;
typedef struct {
    float zoom, x, y;
    uint32_t mode, overview;
} MpcEditorViewport;
/* A projection of the selected engine owner, never a bank of stored values.
 * Binding changes invalidate earlier input. Values are normalized using the
 * engine control's own range; display strings come from that same control. */
#define MPC_EDITOR_PARAMETERS 16u
enum MpcEditorParameterEvent { MPC_EDITOR_PARAMETER_BEGIN=1,
    MPC_EDITOR_PARAMETER_SET, MPC_EDITOR_PARAMETER_END };
typedef struct {
    uint32_t id, steps;
    float value;
    char name[64], display[64];
} MpcEditorParameter;
typedef struct {
    uint64_t binding;
    uint32_t count;
    char owner[128];
    MpcEditorParameter parameters[MPC_EDITOR_PARAMETERS];
} MpcEditorParameters;
#endif
