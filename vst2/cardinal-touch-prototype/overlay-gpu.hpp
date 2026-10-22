#pragma once
#include "overlay-api.h"
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
// Per-Window GL names only; physical storage stays in the resident observer.
// All methods execute with this Window's existing TouchGpu context current.
struct CardinalOverlayGpu {
    struct Target {EGLImageKHR image=EGL_NO_IMAGE_KHR;GLuint texture=0,fbo=0;uint64_t generation=0;int fd=-1;uint32_t width=0,height=0,pitch=0;};
    Target targets[3];GLuint program=0;
    PFNEGLCREATEIMAGEKHRPROC createImage=nullptr;
    PFNEGLDESTROYIMAGEKHRPROC destroyImage=nullptr;
    PFNGLEGLIMAGETARGETTEXTURE2DOESPROC bindImage=nullptr;
    static GLuint shader(GLenum type,const char* source){GLuint s=glCreateShader(type);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){glDeleteShader(s);return 0;}return s;}
    bool setup(){
        if(program)return true;
        createImage=(PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");destroyImage=(PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");bindImage=(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress("glEGLImageTargetTexture2DOES");if(!createImage||!destroyImage||!bindImage)return false;
        GLuint v=shader(GL_VERTEX_SHADER,"attribute highp vec2 position; varying highp vec2 uv; void main(){gl_Position=vec4(position,0.,1.);uv=(position+vec2(1.))*0.5;}");
        GLuint f=shader(GL_FRAGMENT_SHADER,"precision highp float; varying highp vec2 uv; uniform sampler2D source; void main(){gl_FragColor=texture2D(source,vec2(1.-uv.y,1.-uv.x));}");
        if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return false;}
        program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);glBindAttribLocation(program,0,"position");glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(program,GL_LINK_STATUS,&ok);if(!ok){glDeleteProgram(program);program=0;}return ok;
    }
    void drop(EGLDisplay display,Target& t){if(t.fbo)glDeleteFramebuffers(1,&t.fbo);if(t.texture)glDeleteTextures(1,&t.texture);if(t.image!=EGL_NO_IMAGE_KHR&&destroyImage)destroyImage(display,t.image);t=Target{};}
    void release(EGLDisplay display){for(auto& t:targets)drop(display,t);if(program)glDeleteProgram(program);program=0;}
    bool copy(EGLDisplay display,GLuint source,uint32_t logicalWidth,uint32_t logicalHeight,const MpcCardinalOverlayTarget& out){
        if(out.size!=sizeof(out)||out.slot>=3||out.dma_fd<0||!out.allocation_generation||out.width!=logicalHeight||out.height!=logicalWidth||out.width>800||out.height>1280||out.pitch<out.width*4||out.pitch>8192||out.fourcc!=0x34325241u||!setup())return false;
        auto& t=targets[out.slot];
        if(t.generation!=out.allocation_generation||t.fd!=out.dma_fd||t.width!=out.width||t.height!=out.height||t.pitch!=out.pitch){
            drop(display,t);
            const EGLint attributes[]={EGL_WIDTH,EGLint(out.width),EGL_HEIGHT,EGLint(out.height),EGL_LINUX_DRM_FOURCC_EXT,EGLint(out.fourcc),EGL_DMA_BUF_PLANE0_FD_EXT,out.dma_fd,EGL_DMA_BUF_PLANE0_OFFSET_EXT,0,EGL_DMA_BUF_PLANE0_PITCH_EXT,EGLint(out.pitch),EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,0,EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,0,EGL_NONE};
            t.image=createImage(display,EGL_NO_CONTEXT,EGL_LINUX_DMA_BUF_EXT,nullptr,attributes);if(t.image==EGL_NO_IMAGE_KHR)return false;
            glGenTextures(1,&t.texture);glBindTexture(GL_TEXTURE_2D,t.texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);bindImage(GL_TEXTURE_2D,t.image);
            glGenFramebuffers(1,&t.fbo);glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t.texture,0);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE||glGetError()!=GL_NO_ERROR){drop(display,t);return false;}
            t.generation=out.allocation_generation;t.fd=out.dma_fd;t.width=out.width;t.height=out.height;t.pitch=out.pitch;
        }
        const GLboolean dither=glIsEnabled(GL_DITHER);
        glBindFramebuffer(GL_FRAMEBUFFER,t.fbo);glViewport(0,0,out.width,out.height);
        glDisable(GL_DITHER);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glUseProgram(program);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,source);GLint minFilter=0,magFilter=0;glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&minFilter);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&magFilter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glUniform1i(glGetUniformLocation(program,"source"),0);
        const GLfloat vertices[]={-1,-1,1,-1,-1,1,1,1};glBindBuffer(GL_ARRAY_BUFFER,0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,vertices);glEnableVertexAttribArray(0);glDrawArrays(GL_TRIANGLE_STRIP,0,4);glDisableVertexAttribArray(0);
        // First prototype deliberately waits for target readiness. No claim of
        // implicit dma-buf synchronization or asynchronous readiness is made.
        glFinish();glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,minFilter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,magFilter);glBindTexture(GL_TEXTURE_2D,0);glUseProgram(0);if(dither)glEnable(GL_DITHER);
        return glGetError()==GL_NO_ERROR;
    }
};
