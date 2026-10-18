/* Native ListBox remains the scroll/capture owner. This listener classifies
 * deliberate leftward row drags for the right-side Add panel. It never
 * creates an object or changes the native source event. */
static void notify_list_drag(MpcUiListImpl *list, uint32_t event) noexcept {
    auto &drag = list->drag;
    if (!drag.callback || !list->model) return;
    auto *context = list->screen->context;
    context->callback_depth++;
    try { drag.callback(list->model->owner, drag.row, event, drag.x, drag.y); }
    catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    context->callback_depth--;
}

static void restore_list_scroll(MpcUiListImpl *list) noexcept {
    if (list->drag_enabled || list->screen->closing) return;
    try {
        list->screen->context->host.viewport_set_drag(list->viewport, 1, MPC_UI_LIST_DRAG_THRESHOLD);
        list->drag_enabled = 1;
    } catch (...) { set_error(list->screen->context, MPC_UI_HOST_CALL_FAILED); }
}

static void cancel_list_drag(MpcUiListImpl *list) noexcept {
    auto &drag = list->drag;
    const bool notify = drag.active != 0;
    drag.active = 0;
    drag.blocked = drag.tracking;
    restore_list_scroll(list);
    if (notify) notify_list_drag(list, MPC_UI_LIST_DRAG_CANCEL);
}

static bool list_event_id(const void *event, uint32_t &id) {
    const void *source = nullptr;
    memcpy(&source, (const unsigned char *)event + 72, sizeof(source));
    if (!source) return false;
    int32_t index = -1, type = -1;
    memcpy(&index, (const unsigned char *)source + 8, sizeof(index));
    memcpy(&type, (const unsigned char *)source + 12, sizeof(type));
    if (index < 0 || index > 65535 || type < 0 || type > 2) return false;
    id = (uint32_t)index;
    return true;
}

static void list_drag_event(void *opaque, const void *event, uint32_t kind) noexcept {
    auto *drag = (ListDrag *)opaque;
    auto *list = drag ? drag->list : nullptr;
    if (!list || list->magic != kListMagic || !drag->attached || !event ||
        !list->model || !list->model->live || list->screen->closing ||
        !on_ui_thread(list->screen->context)) return;
    uint32_t id = 0;
    if (!list_event_id(event, id)) return;
    unsigned count = 0; bool known = false;
    for (auto value : drag->contacts) { if (value) ++count; if (value == id + 1) known = true; }
    if (kind == MPC_UI_POINTER_DOWN) {
        if (known) return;
        if (count == 10) { cancel_list_drag(list); return; }
        for (auto &value : drag->contacts) if (!value) { value = id + 1; ++count; break; }
    } else if (kind == MPC_UI_POINTER_UP) {
        for (auto &value : drag->contacts) if (value == id + 1) { value = 0; --count; break; }
    }
    if (kind == MPC_UI_POINTER_DOWN && drag->tracking && id != drag->id) {
        cancel_list_drag(list); // Any second contact inside the native parent.
        return;
    }
    if (kind == MPC_UI_POINTER_DOWN && count != 1) return;
    if (kind != MPC_UI_POINTER_DOWN && (!drag->tracking || id != drag->id)) return;
    if (!list->visible || !list->screen->visible) {
        cancel_list_drag(list);
        if (kind == MPC_UI_POINTER_UP) drag->tracking = drag->blocked = drag->scrolling = 0;
        return;
    }
    try {
        auto &host = list->screen->context->host;
        alignas(8) unsigned char relative[80] = {};
        host.mouse_event_relative(relative, event, list->object);
        float xy[2]; memcpy(xy, relative, sizeof(xy));
        if (!(xy[0] >= -65536.f && xy[0] <= 65536.f && xy[1] >= -65536.f && xy[1] <= 65536.f)) return;
        const float x = xy[0] + list->bounds.x, y = xy[1] + list->bounds.y;
        if (kind == MPC_UI_POINTER_DOWN) {
            if (drag->tracking) cancel_list_drag(list);
            drag->tracking = drag->blocked = drag->scrolling = 0;
            // A scrollbar or another native control is not a palette row.
            void *original = nullptr;
            memcpy(&original, (const unsigned char *)event + 52, sizeof(original));
            if (!original || *(void **)original != host.list_row_vtable) return;
            void *owner = nullptr; int32_t row = -1;
            memcpy(&owner, (unsigned char *)original + 0x74, sizeof(owner));
            memcpy(&row, (unsigned char *)original + 0x7c, sizeof(row));
            if (owner != list->object || row < 0 || (uint32_t)row >= list->model->row_count) return;
            drag->tracking = 1; drag->id = id; drag->row = row;
            drag->start_x = drag->x = x; drag->start_y = drag->y = y;
            return;
        }
        drag->x = x; drag->y = y;
        if (kind == MPC_UI_POINTER_MOVE && !drag->blocked && !drag->scrolling) {
            const float dx = x - drag->start_x, dy = y - drag->start_y;
            const float ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
            if (!drag->active && dx < -16.f && ax > ay * 1.5f) {
                host.viewport_set_drag(list->viewport, 0, MPC_UI_LIST_DRAG_THRESHOLD);
                list->drag_enabled = 0;
                drag->active = 1;
                notify_list_drag(list, MPC_UI_LIST_DRAG_BEGIN);
            } else if (!drag->active && ay > 8.f && ay >= ax * .8f) {
                drag->scrolling = 1;
            }
            if (drag->active) notify_list_drag(list, MPC_UI_LIST_DRAG_MOVE);
        } else if (kind == MPC_UI_POINTER_UP) {
            const bool commit = drag->active && !drag->blocked;
            drag->active = drag->tracking = drag->blocked = drag->scrolling = 0;
            restore_list_scroll(list);
            if (commit) notify_list_drag(list, MPC_UI_LIST_DRAG_DROP);
        }
    } catch (...) {
        cancel_list_drag(list);
        set_error(list->screen->context, MPC_UI_HOST_CALL_FAILED);
    }
}

extern "C" void mpc_ui_list_drag_noop(void *, const void *) noexcept {}
extern "C" void mpc_ui_list_drag_destructor(void *) noexcept {}
extern "C" void mpc_ui_list_drag_wheel(void *, const void *, const void *) noexcept {}
extern "C" void mpc_ui_list_drag_magnify(void *, const void *, float) noexcept {}
extern "C" void mpc_ui_list_drag_down(void *o, const void *e) noexcept { list_drag_event(o,e,MPC_UI_POINTER_DOWN); }
extern "C" void mpc_ui_list_drag_move(void *o, const void *e) noexcept { list_drag_event(o,e,MPC_UI_POINTER_MOVE); }
extern "C" void mpc_ui_list_drag_up(void *o, const void *e) noexcept { list_drag_event(o,e,MPC_UI_POINTER_UP); }

extern "C" int mpc_ui_list_drag_out(MpcUiList *opaque, MpcUiListDragCallback callback) {
    auto *list = as_list(opaque);
    if (!list || !callback || !list->attached || list->screen->closing ||
        !on_ui_thread(list->screen->context)) return 0;
    auto &drag = list->drag;
    if (drag.attached) return 0;
    auto &host = list->screen->context->host;
    if (!host.component_add_mouse_listener || !host.component_remove_mouse_listener ||
        !host.mouse_event_relative || !host.list_row_vtable || !host.list_select_on_down) return 0;
    drag.parent = *(void **)((unsigned char *)list->object + kComponentParentOffset);
    if (!drag.parent) return 0;
    drag.list = list; drag.callback = callback;
    drag.vptr = drag.table + 2;
    for (unsigned i = 2; i < 14; ++i) drag.table[i] = (void *)(uintptr_t)&mpc_ui_list_drag_noop;
    drag.table[2] = drag.table[3] = (void *)(uintptr_t)&mpc_ui_list_drag_destructor;
    drag.table[7] = (void *)(uintptr_t)&mpc_ui_list_drag_down;
    drag.table[8] = (void *)(uintptr_t)&mpc_ui_list_drag_move;
    drag.table[9] = (void *)(uintptr_t)&mpc_ui_list_drag_up;
    drag.table[11] = (void *)(uintptr_t)&mpc_ui_list_drag_wheel;
    drag.table[12] = (void *)(uintptr_t)&mpc_ui_list_drag_magnify;
    try {
        host.list_select_on_down(list->object, 0);
        host.component_add_mouse_listener(drag.parent, &drag, 1);
        drag.attached = 1;
        return 1;
    } catch (...) { set_error(list->screen->context, MPC_UI_HOST_CALL_FAILED); return 0; }
}

extern "C" int mpc_ui_list_cancel_drag(MpcUiList *opaque) {
    auto *list = as_list(opaque);
    if (!list || !on_ui_thread(list->screen->context)) return 0;
    cancel_list_drag(list);
    return 1;
}
