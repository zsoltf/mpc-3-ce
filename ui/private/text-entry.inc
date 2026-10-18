/* MPC owns the keyboard and its copied session. Only submit/cancel are used;
 * the unqualified nonterminal callback remains empty. A copied callback holds
 * a small reference-counted record, never a plugin or a native UI object. */
struct TextRequest {
    uint32_t references;
    MpcUiContextImpl *context;
    MpcUiScreenImpl *screen;
    void *owner;
    MpcUiTextCallback callback;
    void (*dispose)(void *, uint32_t);
    uint32_t terminal, accepted;
    char result[1024];
};
struct TextFunction {
    TextRequest *request;
    uint32_t unused;
    int (*manage)(void *, const void *, int);
    void (*invoke)(const void *, const void *);
};
struct TextSession {
    void *title, *initial;
    uint32_t scalar0;
    void *auxiliary;
    uint32_t scalar1, flags;
    TextFunction submit, cancel, change;
};
static_assert(sizeof(TextSession) == 0x48, "pinned ARM text-session layout");

static void text_release(TextRequest *request) noexcept {
    if (request && !--request->references) {
        auto dispose = request->dispose;
        memset(request, 0, sizeof(*request));
        try { dispose(request, sizeof(*request)); } catch (...) {}
    }
}
/* The stock session copies/destroys these records using operations 2 and 3.
 * No host target-type inspection is needed by this private callback path. */
static int text_manage(void *output, const void *input, int operation) noexcept {
    auto *request = *(TextRequest *const *)input;
    if (operation == 2) {
        *(TextRequest **)output = request;
        if (request) ++request->references;
    } else if (operation == 3) text_release(request);
    else if (operation == 1) *(const void **)output = input;
    else if (operation == 0) *(const void **)output = nullptr;
    return 0;
}
static void text_submit(const void *capture, const void *native_text) noexcept {
    auto *request = *(TextRequest *const *)capture;
    if (!request || !request->context || request->terminal || !native_text) return;
    if (!on_ui_thread(request->context)) return;
    request->terminal = 1;
    try {
        const char *text = nullptr;
        request->context->host.string_utf8(&text, native_text);
        if (!text) return;
        unsigned size = 0;
        while (size < sizeof(request->result) && text[size]) ++size;
        if (size == sizeof(request->result)) return;
        memcpy(request->result, text, size + 1);
        request->accepted = 1;
    } catch (...) { set_error(request->context, MPC_UI_HOST_CALL_FAILED); }
}
static void text_cancelled(const void *capture, const void *) noexcept {
    auto *request = *(TextRequest *const *)capture;
    if (request && request->context && on_ui_thread(request->context))
        request->terminal = 1;
}
static void *text_active(const MpcUiContextImpl *context) {
    return *(void **)((unsigned char *)context->text_owner + 0x28);
}
static bool text_owns_active(const MpcUiContextImpl *context, TextRequest *request) {
    if (!context->text_owner) return false;
    auto *session = (TextSession *)text_active(context);
    return session == (void *)((unsigned char *)context->text_owner + 0x2c) &&
           session->submit.manage == text_manage && session->submit.request == request;
}
static void text_cancel_screen(MpcUiScreenImpl *screen) noexcept {
    auto *context = screen->context;
    auto *request = context->text_request;
    if (!request || request->screen != screen) return;
#ifdef MPCLEARN_CARDINAL_GPU
    cardinal_presenter_ui(43,uintptr_t(context->text_owner),0);
#endif
    const bool close = text_owns_active(context, request);
    context->text_request = nullptr;
    // Invalidate before native close; it may synchronously notify listeners.
    request->context = nullptr; request->screen = nullptr;
    request->owner = nullptr; request->callback = nullptr;
    if (close) try { context->host.text_close(context->text_owner); }
    catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    text_release(request);
}
static void text_poll(MpcUiContextImpl *context) noexcept {
    auto *request = context->text_request;
    if (!request || text_owns_active(context, request)) return;
#ifdef MPCLEARN_CARDINAL_GPU
    cardinal_presenter_ui(42,uintptr_t(context->text_owner),0);
#endif
    auto callback = request->callback;
    void *owner = request->owner;
    const bool deliver = request->screen && request->screen->visible &&
                         !request->screen->closing && callback;
    context->text_request = nullptr;
    request->context = nullptr; request->screen = nullptr;
    request->callback = nullptr; request->owner = nullptr;
    if (deliver) {
        ++context->callback_depth;
        try { callback(owner, request->accepted, request->accepted ? request->result : ""); }
        catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
        --context->callback_depth;
    }
    text_release(request);
}
static int text_open(MpcUiScreenImpl *screen, const char *title, const char *initial,
                     MpcUiTextCallback callback, void *owner) {
    if (!screen || !valid_text(title) || !initial || !callback || screen->closing ||
        !screen->visible || !on_ui_thread(screen->context)) return 0;
    unsigned length = 0;
    while (length < 1024 && initial[length]) ++length;
    if (length == 1024) return 0;
    auto *context = screen->context;
    auto &host = context->host;
    if (!context->text_owner || context->text_request || !host.text_session_ctor ||
        !host.text_open || !host.text_close || !host.string_assign ||
        !host.string_utf8 || text_active(context)) return 0;
    TextRequest *request = nullptr;
    TextSession session{};
    bool session_live = false;
    void *temporary = nullptr;
    bool temporary_live = false;
    bool opened = false;
    try {
        request = (TextRequest *)host.allocate(sizeof(*request));
        if (!request) return 0;
        memset(request, 0, sizeof(*request));
        request->references = 1; request->dispose = host.sized_delete;
        request->context = context; request->screen = screen;
        request->owner = owner; request->callback = callback;
        host.text_session_ctor(&session); session_live = true;
        host.string_ctor(&temporary, title); temporary_live = true;
        host.string_assign(&session.title, &temporary);
        host.string_dtor(&temporary); temporary_live = false;
        host.string_ctor(&temporary, initial); temporary_live = true;
        host.string_assign(&session.initial, &temporary);
        host.string_dtor(&temporary); temporary_live = false;
        session.submit = {request, 0, text_manage, text_submit};
        session.cancel = {request, 0, text_manage, text_cancelled};
        request->references += 2;
        context->text_request = request;
#ifdef MPCLEARN_CARDINAL_GPU
        cardinal_presenter_ui(40,uintptr_t(context->text_owner),0);
#endif
        host.text_open(context->text_owner, &session);
        opened = text_owns_active(context, request);
    } catch (...) { set_error(context, MPC_UI_HOST_CALL_FAILED); }
    if (temporary_live) try { host.string_dtor(&temporary); } catch (...) {}
    if (session_live) {
        TextFunction *functions[] = {&session.change, &session.cancel, &session.submit};
        for (auto *function : functions)
            if (function->manage) function->manage(function, function, 3);
        try { host.string_dtor(&session.auxiliary); } catch (...) {}
        try { host.string_dtor(&session.initial); } catch (...) {}
        try { host.string_dtor(&session.title); } catch (...) {}
    }
    if (!opened && request) {
        if (context->text_request == request) text_cancel_screen(screen);
        else text_release(request);
    }
    return opened;
}
