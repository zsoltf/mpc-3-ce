#pragma once
#include <stdint.h>
// Resident observer owns these records. No scene, Slot, API, or native object pointer.
namespace cardinal_presenter {
constexpr unsigned BUFFER_COUNT=3;
constexpr int NONE=-1;
enum class Use : uint8_t { Free, Writing, Ready, Pending, Scanning };
struct Buffer {
    int allocationFd=-1, dmaFd=-1;
    uint32_t handle=0, nativeHandle=0, framebuffer=0, pitch=0, width=0, height=0;
    uint64_t bytes=0, producerGeneration=0;uint32_t revision=0;
    Use use=Use::Free;
};
struct State {
    Buffer buffers[BUFFER_COUNT];
    uint64_t producerGeneration=0, submissionGeneration=0, pendingGeneration=0;
    int scanning=NONE, pending=NONE;
    bool pendingCommit=false, producerLive=false, visible=false, ownerLost=false;
    // Start only after the old producer has released its GL imports. In-flight
    // physical outputs remain in this resident pool until actual replacement.
    uint64_t open() {if(ownerLost||producerLive)return 0;producerLive=true;visible=false;return ++producerGeneration;}
    void suppress() {visible=false;for(auto& b:buffers)if(b.use==Use::Ready)b.use=Use::Free;}
    void close() {suppress();producerLive=false;}
    int acquire(uint64_t generation) {
        if(ownerLost||!producerLive||generation!=producerGeneration)return NONE;
        for(unsigned i=0;i<BUFFER_COUNT;++i)if(buffers[i].use==Use::Free){buffers[i].use=Use::Writing;buffers[i].producerGeneration=generation;return int(i);}
        return NONE;
    }
    bool ready(int i,uint64_t generation) {
        if(i<0||i>=int(BUFFER_COUNT)||buffers[i].use!=Use::Writing)return false;
        auto& b=buffers[i];if(ownerLost||!producerLive||generation!=producerGeneration||b.producerGeneration!=generation){b.use=Use::Free;return false;}
        // A newer completed frame supersedes only an unsubmitted frame.
        for(auto& old:buffers)if(old.use==Use::Ready)old.use=Use::Free;
        b.use=Use::Ready;return true;
    }
    void abandon(int i){if(i>=0&&i<int(BUFFER_COUNT)&&buffers[i].use==Use::Writing)buffers[i].use=Use::Free;}
    // Caller supplies current fully admitted visibility. Absence of admission
    // requests a disable, never permission to revive previous pixels.
    int desired() const {
        if(ownerLost||pendingCommit)return NONE;
        if(producerLive&&visible)for(unsigned i=0;i<BUFFER_COUNT;++i)if(buffers[i].use==Use::Ready&&buffers[i].producerGeneration==producerGeneration)return int(i);
        return NONE;
    }
    bool needsCommit(int target) const {return !ownerLost&&!pendingCommit&&(target!=NONE||((!producerLive||!visible)&&scanning!=NONE));}
    bool submitted(int target) {
        if(!needsCommit(target)||(target!=NONE&&(target<0||target>=int(BUFFER_COUNT)||buffers[target].use!=Use::Ready)))return false;
        pending=target;pendingCommit=true;pendingGeneration=++submissionGeneration;
        if(target!=NONE)buffers[target].use=Use::Pending;
        return true;
    }
    bool completed(uint64_t generation) {
        if(ownerLost||!pendingCommit||generation!=pendingGeneration)return false;
        // The newly scanned buffer is NOT retired by its own first flip.
        if(scanning!=NONE)buffers[scanning].use=Use::Free;
        scanning=pending;if(scanning!=NONE)buffers[scanning].use=Use::Scanning;
        pending=NONE;pendingCommit=false;pendingGeneration=0;return true;
    }
    void loseOwner(){ownerLost=true;producerLive=false;visible=false;/* Hold physical resources; no fabricated retirement. */}
    bool detached()const{return !pendingCommit&&scanning==NONE;}
    bool releasable(unsigned i)const{return i<BUFFER_COUNT&&buffers[i].use==Use::Free;}
};
}
