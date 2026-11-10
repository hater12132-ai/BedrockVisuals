#include "customskies/FrameHook.hpp"
#include "customskies/Diag.hpp"

#include <pl/memory/Hook.hpp>

#include <atomic>
#include <dlfcn.h>

namespace customskies {
namespace {

using EglSwapBuffersFn = unsigned (*)(void* display, void* surface);

std::atomic<bool> gActive{false};
std::atomic<FrameCallback> gCallback{nullptr};
std::atomic<void*> gSwapOrig{nullptr};
void* gSwapTarget = nullptr;

unsigned eglSwapBuffersDetour(void* display, void* surface) {
    // Run our frame logic before the real swap
    if (gActive.load(std::memory_order_relaxed)) {
        if (auto cb = gCallback.load(std::memory_order_relaxed)) {
            cb();
        }
    }

    auto original = reinterpret_cast<EglSwapBuffersFn>(gSwapOrig.load(std::memory_order_acquire));
    if (original) {
        return original(display, surface);
    }
    return 0;
}

} // namespace

void setFrameCallback(FrameCallback cb) {
    gCallback.store(cb, std::memory_order_relaxed);
}

bool installFrameHook() {
    if (gActive.load()) return true;

    // Resolve eglSwapBuffers from the process (libEGL)
    void* swap = ::dlsym(RTLD_DEFAULT, "eglSwapBuffers");
    if (!swap) {
        // Some loaders only expose it via eglGetProcAddress
        using EglGetProcAddressFn = void* (*)(const char*);
        auto getProc = reinterpret_cast<EglGetProcAddressFn>(::dlsym(RTLD_DEFAULT, "eglGetProcAddress"));
        if (getProc) {
            swap = getProc("eglSwapBuffers");
        }
    }

    if (!swap) {
        diag::error("FrameHook: eglSwapBuffers not found");
        diag::setLastError("eglSwapBuffers symbol missing");
        return false;
    }

    void* original = nullptr;
    // preloader inline hook
    int rc = pl::memory::hook(swap, reinterpret_cast<void*>(&eglSwapBuffersDetour), &original);
    if (rc != 0 || !original) {
        diag::error("FrameHook: pl::memory::hook(eglSwapBuffers) failed");
        diag::setLastError("hook eglSwapBuffers failed");
        return false;
    }

    gSwapOrig.store(original, std::memory_order_release);
    gSwapTarget = swap;
    gActive.store(true, std::memory_order_release);
    diag::setFrameHookStatus(true);

    diag::info("FrameHook: eglSwapBuffers hooked OK");
    return true;
}

void removeFrameHook() {
    if (!gActive.exchange(false)) return;

    if (gSwapTarget) {
        (void)pl::memory::unhook(gSwapTarget, reinterpret_cast<void*>(&eglSwapBuffersDetour));
        gSwapTarget = nullptr;
    }
    gSwapOrig.store(nullptr);
    diag::setFrameHookStatus(false);
    diag::info("FrameHook: removed");
}

bool frameHookActive() {
    return gActive.load(std::memory_order_relaxed);
}

} // namespace customskies
