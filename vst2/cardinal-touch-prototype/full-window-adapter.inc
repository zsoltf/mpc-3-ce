/* Included inside rack::window, alongside the pinned Window::Internal. */
void* mpcWindowEnter(Window* window) {
    if (!window || !window->vg) return nullptr;
    auto* scope = new TouchEglScope(window->internal->gpu, window->internal->restoreFailed);
    try { window->internal->gpu.enter(); }
    catch (...) { delete scope; throw; }
    return scope;
}
void mpcWindowLeave(void* scope) { delete static_cast<TouchEglScope*>(scope); }

void mpcWindowSetViewportMoving(Window* window, bool moving) {
    auto* state = window->internal;
    state->touchViewportMoving = moving;
    if (!moving && state->touchViewportDirty) {
        state->touchViewportDirty = false;
        widget::EventContext context;
        widget::Widget::DirtyEvent event;
        event.context = &context;
        APP->scene->rackScroll->zoomWidget->onDirty(event);
    }
}
bool mpcWindowViewportMoving(Window* window) {
    return window && window->internal->touchViewportMoving;
}
bool mpcWindowDeferZoomDirty(Window* window, widget::Widget* zoom) {
    if (!mpcWindowViewportMoving(window) || !APP->scene ||
        zoom != APP->scene->rackScroll->zoomWidget) return false;
    window->internal->touchViewportDirty = true;
    return true;
}

bool mpcWindowOperation(Window* window, bool (*operation)(void*), void* argument) {
    if (!window || !window->vg) return false;
    auto* state = window->internal;
    TouchEglScope scope(state->gpu, state->restoreFailed);
    state->gpu.enter();
    const bool okay = operation(argument);
    return scope.restore() && okay;
}

// Called inside mpcWindowOperation with this window's EGL context current.
// The canonical scene and its caches still belong to the live synth owner.
void mpcWindowReleaseView(Window* window) {
    auto* state = window->internal;
    state->overlayGpu.release(state->gpu.display);
    state->readbackRevision = 0;
    if (state->touchFramebuffer) {
        nvgluDeleteFramebuffer(state->touchFramebuffer);
        state->touchFramebuffer = nullptr;
    }
    std::vector<unsigned char>().swap(state->touchReadback);
    state->touchWidth = state->touchHeight = 0;
    state->touchViewportMoving = false;
    state->touchViewportDirty = false;
}

// Called inside mpcWindowOperation. Layout stays synchronous with input so a
// subsequent contact sees the actual Rack geometry even before the next paint.
bool mpcWindowUpdate(Window* window, uint32_t width, uint32_t height) {
    if (!window || !window->vg || width < 640 || width > 1280 || height < 400 || height > 800)
        return false;
    auto* state = window->internal;
    state->size = math::Vec(width, height);
    window->pixelRatio = window->windowRatio = 1;
    const double now = system::getTime();
    state->lastFrameDuration = std::isfinite(state->frameTime) ? now-state->frameTime : 0;
    state->frameTime = now;
    state->fbCount = 0;
    nvgReset(window->vg);
    APP->scene->box.size = state->size;
    APP->scene->step();
    // SequentialLayout publishes its height during its child step. Browser
    // reads that height before stepping children, so a freshly shown browser
    // needs one bounded layout settlement before this input-driven frame.
    // This advances no engine/audio clock and installs no repaint timer.
    if (APP->scene->browser->visible)
        APP->scene->step();
    return true;
}

bool mpcWindowRenderFrame(Window* window, uint32_t width, uint32_t height, bool download) {
    if (!window || !window->vg || width < 640 || width > 1280 || height < 400 || height > 800)
        return false;
    auto* state = window->internal;
    TouchEglScope scope(state->gpu, state->restoreFailed);
    state->gpu.enter();
    if (!state->touchFramebuffer || state->touchWidth != width || state->touchHeight != height) {
        if (state->touchFramebuffer)
            nvgluDeleteFramebuffer(state->touchFramebuffer);
        state->touchFramebuffer = nvgluCreateFramebuffer(window->vg, width, height, 0);
        if (!state->touchFramebuffer)
            return false;
        state->touchReadback.resize(width * height * 4);
        state->touchWidth = width;
        state->touchHeight = height;
    }
    nvgluBindFramebuffer(state->touchFramebuffer);
    nvgBeginFrame(window->vg, width, height, 1);
    if (window->uiFont)
        bndSetFont(window->uiFont->handle);
    widget::Widget::DrawArgs draw;
    draw.vg = window->vg;
    // This is the scene's presentation target, not a nested widget cache.
    // Setting DrawArgs.fb would bypass every Rack FramebufferWidget cache.
    draw.clipBox = APP->scene->box.zeroPos();
    APP->scene->draw(draw);
    mpcTouchDrawOverlay(window, window->vg);
    // Widget caches bind their own targets and return to framebuffer zero.
    // Submit the root NanoVG commands to our target, at the root viewport.
    nvgluBindFramebuffer(state->touchFramebuffer);
    glViewport(0, 0, width, height);
    glClearColor(0.05f, 0.05f, 0.06f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    nvgEndFrame(window->vg);
    if (download) glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, state->touchReadback.data());
    const bool okay = glGetError() == GL_NO_ERROR;
    nvgluBindFramebuffer(nullptr);
    if (!okay)
        return false;
    if (++state->touchRevision == 0)
        ++state->touchRevision;
    if (download) state->readbackRevision = state->touchRevision;
    ++state->frame;
    return scope.restore();
}

bool mpcWindowRender(Window* window, uint32_t width, uint32_t height) {
    return mpcWindowRenderFrame(window,width,height,true);
}
bool mpcWindowOverlay(Window* window,uint32_t width,uint32_t height,bool render,const MpcCardinalOverlayTarget* target,uint32_t* revision) {
    if(!target||!revision||!window||!window->vg)return false;
    if(render&&!mpcWindowRenderFrame(window,width,height,false))return false;
    auto* state=window->internal;if(!state->touchFramebuffer||state->touchWidth!=width||state->touchHeight!=height)return false;
    TouchEglScope scope(state->gpu,state->restoreFailed);state->gpu.enter();
    bool okay=state->overlayGpu.copy(state->gpu.display,state->touchFramebuffer->texture,width,height,*target);
    nvgluBindFramebuffer(nullptr);if(!okay)return false;*revision=state->touchRevision;return scope.restore();
}
void mpcWindowOverlayRelease(Window* window){auto* state=window->internal;state->overlayGpu.release(state->gpu.display);}

bool mpcWindowPixels(Window* window, unsigned char* output, uint32_t bytes, uint32_t* revision) {
    auto* state = window->internal;
    if (!output || !revision || bytes < state->touchReadback.size() || state->touchReadback.empty())
        return false;
    // A prior GPU-only frame must never masquerade as a fresh CPU export.
    if(state->readbackRevision!=state->touchRevision){
        TouchEglScope scope(state->gpu,state->restoreFailed);state->gpu.enter();
        nvgluBindFramebuffer(state->touchFramebuffer);
        glReadPixels(0,0,state->touchWidth,state->touchHeight,GL_RGBA,GL_UNSIGNED_BYTE,state->touchReadback.data());
        const bool okay=glGetError()==GL_NO_ERROR;nvgluBindFramebuffer(nullptr);
        if(!scope.restore()||!okay)return false;
        state->readbackRevision=state->touchRevision;
    }
    // Keep GL's bottom-up readback; orient directly into the caller's buffer.
    // Repeated exports use the same completed frame and revision.
    const uint32_t rowBytes = state->touchWidth * 4;
    for (uint32_t row = 0; row < state->touchHeight; ++row)
        std::memcpy(output + row * rowBytes,
                    state->touchReadback.data() + (state->touchHeight - row - 1) * rowBytes, rowBytes);
    *revision = state->touchRevision;
    return true;
}
