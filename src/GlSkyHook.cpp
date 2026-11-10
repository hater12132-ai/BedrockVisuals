#include "customskies/GlSkyHook.hpp"
#include "customskies/Diag.hpp"

#include <pl/memory/Hook.hpp>

#include <atomic>
#include <dlfcn.h>

namespace customskies {
namespace {

using GLbitfield = unsigned int;
using GLenum = unsigned int;
using GlClearFn = void (*)(GLbitfield);
using GlGetIntegervFn = void (*)(GLenum, int*);

constexpr GLbitfield GL_COLOR_BUFFER_BIT = 0x00004000;
constexpr GLbitfield GL_DEPTH_BUFFER_BIT = 0x00000100;
constexpr GLenum GL_FRAMEBUFFER_BINDING = 0x8CA6;

std::atomic<bool> gActive{false};
std::atomic<SkyDrawFn> gDraw{nullptr};
std::atomic<void*> gClearOrig{nullptr};
void* gClearTarget = nullptr;
GlGetIntegervFn gGetIntegerv = nullptr;

void glClearDetour(GLbitfield mask) {
    auto original = reinterpret_cast<GlClearFn>(gClearOrig.load(std::memory_order_acquire));
    if (original) original(mask);

    if (!gActive.load(std::memory_order_relaxed)) return;

    // Only after a full color+depth clear on the default framebuffer
    if ((mask & GL_COLOR_BUFFER_BIT) == 0) return;
    if ((mask & GL_DEPTH_BUFFER_BIT) == 0) return;

    if (gGetIntegerv) {
        int fbo = -1;
        gGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        if (fbo != 0) return;
    }

    if (auto fn = gDraw.load(std::memory_order_relaxed)) {
        fn();
    }
}

} // namespace

void setSkyDrawCallback(SkyDrawFn fn) {
    gDraw.store(fn, std::memory_order_relaxed);
}

bool installGlSkyHook() {
    if (gActive.load()) return true;

    void* clear = dlsym(RTLD_DEFAULT, "glClear");
    if (!clear) {
        void* gles = dlopen("libGLESv2.so", RTLD_NOW);
        if (!gles) gles = dlopen("libGLESv3.so", RTLD_NOW);
        if (gles) clear = dlsym(gles, "glClear");
    }
    if (!clear) {
        diag::error("GlSkyHook: glClear not found");
        return false;
    }

    gGetIntegerv = reinterpret_cast<GlGetIntegervFn>(dlsym(RTLD_DEFAULT, "glGetIntegerv"));
    if (!gGetIntegerv) {
        void* gles = dlopen("libGLESv2.so", RTLD_NOW);
        if (gles) gGetIntegerv = reinterpret_cast<GlGetIntegervFn>(dlsym(gles, "glGetIntegerv"));
    }

    void* original = nullptr;
    int rc = pl::memory::hook(clear, reinterpret_cast<void*>(&glClearDetour), &original);
    if (rc != 0 || !original) {
        diag::error("GlSkyHook: hook(glClear) failed");
        return false;
    }

    gClearOrig.store(original, std::memory_order_release);
    gClearTarget = clear;
    gActive.store(true, std::memory_order_release);
    diag::info("GlSkyHook: glClear hooked OK");
    return true;
}

void removeGlSkyHook() {
    if (!gActive.exchange(false)) return;
    if (gClearTarget) {
        (void)pl::memory::unhook(gClearTarget, reinterpret_cast<void*>(&glClearDetour));
        gClearTarget = nullptr;
    }
    gClearOrig.store(nullptr);
    diag::info("GlSkyHook: removed");
}

bool glSkyHookActive() {
    return gActive.load(std::memory_order_relaxed);
}

} // namespace customskies
