#pragma once
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <stdexcept>

// This adapter owns only a render-node GBM display/context. Never release the
// host thread, install a process handler or take display/audio ownership.
struct TouchGpu {
    int fd=-1;
    gbm_device* device=nullptr;
    EGLDisplay display=EGL_NO_DISPLAY;
    EGLContext context=EGL_NO_CONTEXT;
    bool initialized=false;
    void open(EGLDisplay hostDisplay, EGLDisplay hostEsDisplay) {
        fd=::open("/dev/dri/renderD128",O_RDWR|O_CLOEXEC|O_NOFOLLOW);
        struct stat st;
        if(fd<0 || fstat(fd,&st) || !S_ISCHR(st.st_mode) ||
           major(st.st_rdev)!=226 || minor(st.st_rdev)!=128)
            throw std::runtime_error("render node unavailable");
        device=gbm_create_device(fd);
        if(!device) throw std::runtime_error("GBM create failed");
        auto getDisplay=(PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
        if(!getDisplay) throw std::runtime_error("EGL platform unavailable");
        display=getDisplay(EGL_PLATFORM_GBM_KHR,device,nullptr);
        if(display==EGL_NO_DISPLAY || display==hostDisplay || display==hostEsDisplay) {
            display=EGL_NO_DISPLAY;
            throw std::runtime_error("EGL display is absent or borrowed");
        }
        EGLint majorVersion,minorVersion;
        if(!eglInitialize(display,&majorVersion,&minorVersion)) throw std::runtime_error("EGL initialize failed");
        initialized=true;
        const EGLint attrs[]={EGL_SURFACE_TYPE,0,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
        EGLConfig config;EGLint count=0;
        if(!eglChooseConfig(display,attrs,&config,1,&count) || count!=1)
            throw std::runtime_error("EGL config failed");
        const EGLint contextAttrs[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
        context=eglCreateContext(display,config,EGL_NO_CONTEXT,contextAttrs);
        if(context==EGL_NO_CONTEXT) throw std::runtime_error("EGL context failed");
    }
    void enter() {
        if(!eglBindAPI(EGL_OPENGL_ES_API) ||
           !eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,context))
            throw std::runtime_error("EGL entry failed");
    }
    bool closeUnbound() noexcept {
        if(context!=EGL_NO_CONTEXT) {
            if(!eglDestroyContext(display,context))return false;
            context=EGL_NO_CONTEXT;
        }
        if(initialized) {if(!eglTerminate(display))return false;initialized=false;}
        display=EGL_NO_DISPLAY;
        if(device){gbm_device_destroy(device);device=nullptr;}
        if(fd>=0){int result=::close(fd);fd=-1;if(result)return false;}
        return true;
    }
};

struct TouchEglScope {
    struct Tuple {
        EGLDisplay display;
        EGLContext context;
        EGLSurface draw,read;
        static Tuple capture(){return {eglGetCurrentDisplay(),eglGetCurrentContext(),
            eglGetCurrentSurface(EGL_DRAW),eglGetCurrentSurface(EGL_READ)};}
        bool matches()const {return display==eglGetCurrentDisplay() && context==eglGetCurrentContext() &&
            draw==eglGetCurrentSurface(EGL_DRAW) && read==eglGetCurrentSurface(EGL_READ);}
    };
    TouchGpu& gpu;
    EGLenum api;
    Tuple original,es;
    bool restored=false;
    bool& failed;
    TouchEglScope(TouchGpu& gpu,bool& failed):gpu(gpu),api(eglQueryAPI()),
        original(Tuple::capture()),es(original),failed(failed) {
        if(api!=EGL_OPENGL_ES_API && api!=EGL_OPENGL_API && api!=EGL_OPENVG_API)
            throw std::runtime_error("Unsupported host EGL API");
        if(!eglBindAPI(EGL_OPENGL_ES_API)) throw std::runtime_error("cannot bind GLES API");
        es=Tuple::capture();
    }
    bool restore() noexcept {
        if(restored)return !failed;
        bool ok=eglBindAPI(EGL_OPENGL_ES_API);
        if(es.context!=EGL_NO_CONTEXT)
            ok=eglMakeCurrent(es.display,es.draw,es.read,es.context)&&ok;
        else if(eglGetCurrentContext()!=EGL_NO_CONTEXT)
            ok=(gpu.display!=EGL_NO_DISPLAY && eglMakeCurrent(gpu.display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT))&&ok;
        ok=es.matches()&&ok;
        ok=eglBindAPI(api)&&ok;
        if(api!=EGL_OPENGL_ES_API && original.context!=EGL_NO_CONTEXT)
            ok=eglMakeCurrent(original.display,original.draw,original.read,original.context)&&ok;
        ok=original.matches() && eglQueryAPI()==api && ok;
        failed=failed||!ok;restored=true;return ok;
    }
    ~TouchEglScope(){restore();}
};
