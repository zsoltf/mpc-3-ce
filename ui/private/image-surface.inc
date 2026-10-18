/* Toolkit-owned image and contact lifetime. Included in the private
 * implementation namespace; no native object escapes through the public API. */
struct ImageBitmap {
    unsigned char *data;
    int format, line_stride, pixel_stride, width, height;
    void *releaser;
};
static_assert(sizeof(ImageBitmap) == 28 && alignof(ImageBitmap) == 4,
              "pinned ARM BitmapData layout");

// Four pixels per NEON operation. memcpy permits unaligned client buffers;
// the compiler emits native vector loads/stores without an alignment promise.
#if defined(__ARM_NEON) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
using ImagePixelWords = uint32_t __attribute__((vector_size(16)));
#endif
static bool image_pixels_opaque(const unsigned char *rgba, uint32_t width,
                                uint32_t height, uint32_t stride) {
#if defined(__ARM_NEON) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    ImagePixelWords alpha = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
#endif
    for (uint32_t y = 0; y < height; ++y) {
        const unsigned char *src = rgba + y * stride;
        uint32_t x = 0;
#if defined(__ARM_NEON) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        for (; x + 4 <= width; x += 4, src += 16) {
            ImagePixelWords pixels;
            memcpy(&pixels, src, sizeof(pixels));
            alpha &= pixels;
        }
#endif
        for (; x < width; ++x, src += 4)
            if (src[3] != 255) return false;
    }
#if defined(__ARM_NEON) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (((alpha[0] & alpha[1] & alpha[2] & alpha[3]) & 0xff000000u) != 0xff000000u)
        return false;
#endif
    return true;
}

static void image_pixels_bgra(unsigned char *dst, const unsigned char *src,
                               uint32_t width) {
    uint32_t x = 0;
#if defined(__ARM_NEON) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    for (; x + 4 <= width; x += 4, src += 16, dst += 16) {
        ImagePixelWords pixels;
        memcpy(&pixels, src, sizeof(pixels));
        pixels = (pixels & 0xff00ff00u) | ((pixels & 0x00ff0000u) >> 16) |
                 ((pixels & 0x000000ffu) << 16);
        memcpy(dst, &pixels, sizeof(pixels));
    }
#endif
    for (; x < width; ++x, src += 4, dst += 4) {
        dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; dst[3] = 255;
    }
}

static void image_contact_notify(Binding *binding, ImageContact *contact,
                                  uint32_t event) noexcept {
    if (!binding->contact_callback) return;
    ++binding->context->callback_depth;
    ++binding->pointer_depth;
    try { binding->contact_callback(binding->owner, contact->id, event,
                                    contact->x, contact->y); }
    catch (...) { set_error(binding->context, MPC_UI_HOST_CALL_FAILED); }
    --binding->pointer_depth;
    if (!binding->pointer_depth) {
        for (auto &pending : binding->contacts) if (pending.cancel_pending) {
            pending.cancel_pending = 0;
            image_contact_notify(binding, &pending, MPC_UI_POINTER_CANCEL);
        }
    }
    --binding->context->callback_depth;
}

static void image_pointer_notify(Binding *binding, uint32_t event) noexcept {
    if (!binding->pointer_callback) return;
    binding->context->callback_depth++;
    binding->pointer_depth++;
    try { binding->pointer_callback(binding->owner, event,
                                    binding->pointer_x, binding->pointer_y); }
    catch (...) { set_error(binding->context, MPC_UI_HOST_CALL_FAILED); }
    binding->pointer_depth--;
    if (!binding->pointer_depth && binding->cancel_pending) {
        binding->cancel_pending = 0;
        image_pointer_notify(binding, MPC_UI_POINTER_CANCEL);
    }
    binding->context->callback_depth--;
}

static void cancel_image_contact(Binding *binding) noexcept {
    if (!binding->image) return;
    // Invalidate the complete contact set before the first callback. Hiding
    // again from that callback cannot cancel or revive another contact.
    for (auto &contact : binding->contacts) if (contact.active) {
        contact.active = 0;
        contact.cancel_pending = 1;
    }
    if (!binding->pointer_depth) {
        for (auto &contact : binding->contacts) if (contact.cancel_pending) {
            contact.cancel_pending = 0;
            image_contact_notify(binding, &contact, MPC_UI_POINTER_CANCEL);
        }
    }
    if (!binding->contact) return;
    binding->contact = 0; // before callback: reentrant hide cannot cancel twice
    if (binding->pointer_depth) binding->cancel_pending = 1;
    else image_pointer_notify(binding, MPC_UI_POINTER_CANCEL);
}

static void image_pointer(void *object, const void *event, uint32_t kind) noexcept {
    Binding *b = binding_from_object(object);
    if (!b || !b->live || !b->image || !event || !b->control ||
        b->screen->closing || !b->screen->visible || !b->control->visible ||
        !on_ui_thread(b->context) || b->pointer_depth) return;
    float xy[2];
    memcpy(xy, event, sizeof(xy));
    // Preserve out-of-bounds drag coordinates, reject NaN/infinity/corruption.
    if (!(xy[0] >= -65536.0f && xy[0] <= 65536.0f &&
          xy[1] >= -65536.0f && xy[1] <= 65536.0f)) return;
    if (b->contact_callback) {
        // Pinned MouseEvent::source contains its pimpl at +72. Native physical
        // two-finger DOWN/MOVE/UP qualifies pimpl's index +8 and source type +12;
        // keep both offsets private, and retain only the value, never the event.
        const void *source = nullptr;
        memcpy(&source, (const unsigned char *)event + 72, sizeof(source));
        if (!source) return;
        int32_t id = -1, type = -1;
        memcpy(&id, (const unsigned char *)source + 8, sizeof(id));
        memcpy(&type, (const unsigned char *)source + 12, sizeof(type));
        if (id < 0 || id > 65535 || type < 0 || type > 2) return;
        ImageContact *contact = nullptr;
        for (auto &item : b->contacts)
            if (item.active && item.id == uint32_t(id)) { contact = &item; break; }
        if (kind == MPC_UI_POINTER_DOWN) {
            if (contact) return;
            for (auto &item : b->contacts) if (!item.active && !item.cancel_pending) {
                contact = &item; break;
            }
            if (!contact) { cancel_image_contact(b); return; }
            *contact = {uint32_t(id), 1, 0, xy[0], xy[1]};
        } else if (!contact) return;
        contact->x = xy[0]; contact->y = xy[1];
        if (kind == MPC_UI_POINTER_UP) contact->active = 0;
        image_contact_notify(b, contact, kind);
        return;
    }
    if (kind == MPC_UI_POINTER_DOWN) {
        if (b->contact) return;
        b->contact = 1;
    } else if (!b->contact) return;
    b->pointer_x = xy[0]; b->pointer_y = xy[1];
    if (kind == MPC_UI_POINTER_UP) b->contact = 0;
    image_pointer_notify(b, kind);
}

extern "C" int mpc_ui_image_contacts(MpcUiControl *image, MpcUiContactCallback callback) {
    auto *control = as_control(image);
    if (!control || !control->binding || !control->binding->image || !callback ||
        !on_ui_thread(control->screen->context)) return 0;
    auto *binding = control->binding;
    if (binding->pointer_depth || binding->contact) return 0;
    for (auto &contact : binding->contacts)
        if (contact.active || contact.cancel_pending) return 0;
    try { control->screen->context->host.component_set_intercepts_mouse(control->object, 1, 0); }
    catch (...) { set_error(control->screen->context, MPC_UI_HOST_CALL_FAILED); return 0; }
    binding->contact_callback = callback;
    return 1;
}
extern "C" int mpc_ui_image_cancel(MpcUiControl *image) {
    auto *control = as_control(image);
    if (!control || !control->binding || !control->binding->image ||
        !on_ui_thread(control->screen->context)) return 0;
    cancel_image_contact(control->binding);
    return 1;
}
extern "C" int mpc_ui_image_invalidate(MpcUiControl *image) {
    auto *control = as_control(image);
    if (!control || !control->binding || !control->binding->image ||
        control->screen->closing || !on_ui_thread(control->screen->context)) return 0;
    auto *binding = control->binding;
    if (binding->image_pending) return 1;
    binding->image_pending = 1;
    try { if (!binding->image_painting) control->screen->context->host.repaint(control->object); }
    catch (...) { binding->image_pending = 0; set_error(control->screen->context, MPC_UI_HOST_CALL_FAILED); return 0; }
    return 1;
}
extern "C" int mpc_ui_image_on_refresh(MpcUiControl *image, MpcUiImageRefreshCallback callback) {
    auto *control = as_control(image);
    if (!control || !control->binding || !control->binding->image || !callback ||
        control->screen->closing || !on_ui_thread(control->screen->context) ||
        control->binding->image_painting) return 0;
    control->binding->image_refresh_callback = callback;
    return mpc_ui_image_invalidate(image);
}

extern "C" void mpc_ui_image_down(void *object, const void *event) noexcept {
    image_pointer(object, event, MPC_UI_POINTER_DOWN);
}
extern "C" void mpc_ui_image_drag(void *object, const void *event) noexcept {
    image_pointer(object, event, MPC_UI_POINTER_MOVE);
}
extern "C" void mpc_ui_image_up(void *object, const void *event) noexcept {
    image_pointer(object, event, MPC_UI_POINTER_UP);
}
extern "C" void mpc_ui_image_paint(void *object, void *graphics, int, int) noexcept {
    Binding *b = binding_from_object(object);
    if (!b || !b->live || !b->image || !graphics || !b->control ||
        b->screen->closing || !b->screen->visible || !b->control->visible ||
        !on_ui_thread(b->context)) return;

#ifdef MPCLEARN_CARDINAL_GPU
    uint64_t paintTicket=0;
    if(cardinal_presenter_paint(uintptr_t(b->control),uintptr_t(graphics),b->image_width,b->image_height,&paintTicket))b->image_pending=1;

#endif
    b->context->callback_depth++;
    ++b->image_painting;

#ifdef MPCLEARN_CARDINAL_GPU
    bool drew=false,okay=false;
#endif
    try {
        if (b->image_pending) {
            b->image_pending = 0;
            if (b->image_refresh_callback) b->image_refresh_callback(b->owner);
        }
#ifdef MPCLEARN_CARDINAL_GPU
        if(!cardinal_presenter_skip_draw(uintptr_t(b->control),paintTicket)){
#endif
            b->context->host.graphics_draw_image(graphics, &b->image, 0, 0, 0);
#ifdef MPCLEARN_CARDINAL_GPU
            drew=true;
        }
        okay=true;
#endif
    }
    catch (...) { set_error(b->context, MPC_UI_HOST_CALL_FAILED); }

#ifdef MPCLEARN_CARDINAL_GPU
    cardinal_presenter_paint_done(uintptr_t(b->control),paintTicket,drew,okay);
#endif
    --b->image_painting;
    if (b->image_pending) try { b->context->host.repaint(object); }
    catch (...) { set_error(b->context, MPC_UI_HOST_CALL_FAILED); }
    b->context->callback_depth--;
}
