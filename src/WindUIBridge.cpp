#include "customskies/WindUIBridge.hpp"
#include "customskies/Diag.hpp"
#include "customskies/CustomSkiesModule.hpp"

#include <dlfcn.h>
#include <android/log.h>

#include <atomic>
#include <cstring>
#include <string>

// Forward-declared by main.cpp
namespace customskies {
CustomSkiesModule* getActiveModule();
}

namespace customskies {
namespace {

using WindUI_IsReadyFn = int (*)();
using WindUI_AddTabFn = void (*)(const char*, const char*);
using WindUI_AddToggleFn = int (*)(const char*, const char*, const char*, int);
using WindUI_AddSliderFn = int (*)(const char*, const char*, const char*, int, int, int, int);
using WindUI_AddButtonFn = int (*)(const char*, const char*, const char*);
using WindUI_OnFn = void (*)(int, void (*)(int, const char*, void*), void*);
using WindUI_OffFn = void (*)(int);
using WindUI_NotifyFn = void (*)(const char*, const char*);
using WindUI_RegisterCallbackFn = void (*)(void (*)(int, const char*, void*), void*);

WindUI_IsReadyFn        pIsReady = nullptr;
WindUI_AddTabFn         pAddTab = nullptr;
WindUI_AddToggleFn      pAddToggle = nullptr;
WindUI_AddSliderFn      pAddSlider = nullptr;
WindUI_AddButtonFn      pAddButton = nullptr;
WindUI_OnFn             pOn = nullptr;
WindUI_OffFn            pOff = nullptr;
WindUI_NotifyFn         pNotify = nullptr;

std::atomic<bool> gAttached{false};
int gToggleId = -1;
int gSliderId = -1;

void* resolve(const char* name) {
    void* p = dlsym(RTLD_DEFAULT, name);
    if (!p) {
        // Some loaders only export from the WindUI .so handle
        void* lib = dlopen("libWindUI.so", RTLD_NOLOAD);
        if (!lib) lib = dlopen("libWindUIBE.so", RTLD_NOLOAD);
        if (lib) p = dlsym(lib, name);
    }
    return p;
}

bool resolveAll() {
    pIsReady   = reinterpret_cast<WindUI_IsReadyFn>(resolve("WindUI_IsReady"));
    pAddTab    = reinterpret_cast<WindUI_AddTabFn>(resolve("WindUI_AddTab"));
    pAddToggle = reinterpret_cast<WindUI_AddToggleFn>(resolve("WindUI_AddToggle"));
    pAddSlider = reinterpret_cast<WindUI_AddSliderFn>(resolve("WindUI_AddSlider"));
    pAddButton = reinterpret_cast<WindUI_AddButtonFn>(resolve("WindUI_AddButton"));
    pOn        = reinterpret_cast<WindUI_OnFn>(resolve("WindUI_On"));
    pOff       = reinterpret_cast<WindUI_OffFn>(resolve("WindUI_Off"));
    pNotify    = reinterpret_cast<WindUI_NotifyFn>(resolve("WindUI_Notify"));
    return pAddTab && pAddToggle && pAddSlider && pOn;
}

// WindUI callbacks run on the Android UI thread — only touch atomics / module API.
void onToggle(int /*id*/, const char* value, void* /*ud*/) {
    auto* mod = getActiveModule();
    if (!mod) return;

    bool on = false;
    if (value) {
        // value may be "true"/"false", "1"/"0", or JSON-ish
        if (std::strcmp(value, "true") == 0 || std::strcmp(value, "1") == 0 ||
            std::strcmp(value, "True") == 0 || std::strcmp(value, "on") == 0) {
            on = true;
        } else if (std::strcmp(value, "false") == 0 || std::strcmp(value, "0") == 0) {
            on = false;
        } else {
            // try parse as int
            on = (std::atoi(value) != 0);
        }
    }

    auto cfg = mod->config();
    cfg.enabled = on;
    mod->setConfig(cfg);

    if (on) {
        if (!mod->isEnabled()) mod->onEnable();
        diag::info("WindUI: CustomSkies enabled");
        if (pNotify) pNotify("CustomSkies", "Video sky ON");
    } else {
        if (mod->isEnabled()) mod->onDisable();
        diag::info("WindUI: CustomSkies disabled");
        if (pNotify) pNotify("CustomSkies", "Video sky OFF");
    }
}

void onOpacity(int /*id*/, const char* value, void* /*ud*/) {
    auto* mod = getActiveModule();
    if (!mod || !value) return;

    int pct = std::atoi(value);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    auto cfg = mod->config();
    cfg.opacity = pct / 100.f;
    mod->setConfig(cfg);
    diag::info(std::string("WindUI: opacity=") + std::to_string(pct) + "%");
}

} // namespace

bool tryAttachWindUI() {
    if (gAttached.load()) return true;

    if (!resolveAll()) {
        diag::info("WindUI: symbols not found (is WindUIBE installed?)");
        return false;
    }

    if (pIsReady && !pIsReady()) {
        diag::info("WindUI: not ready yet");
        return false;
    }

    // Create tab + controls
    pAddTab("CustomSkies", "cloud");

    auto* mod = getActiveModule();
    int enabled = 1;
    int opacityPct = 100;
    if (mod) {
        enabled = mod->config().enabled ? 1 : 0;
        opacityPct = (int)(mod->config().opacity * 100.f + 0.5f);
        if (opacityPct < 0) opacityPct = 0;
        if (opacityPct > 100) opacityPct = 100;
    }

    gToggleId = pAddToggle(
        "CustomSkies",
        "Enabled",
        "Replace the Minecraft sky with a looping video",
        enabled);

    gSliderId = pAddSlider(
        "CustomSkies",
        "Opacity",
        "How strong the video sky is (0–100%)",
        0, 100, opacityPct, 1);

    if (gToggleId >= 0) pOn(gToggleId, &onToggle, nullptr);
    if (gSliderId >= 0) pOn(gSliderId, &onOpacity, nullptr);

    if (pAddButton) {
        // Informational only
        pAddButton("CustomSkies", "Status", "Decoder + glClear sky hook active when Enabled");
    }

    gAttached.store(true);
    diag::info("WindUI: CustomSkies tab attached");
    if (pNotify) pNotify("CustomSkies", "Tab ready — open WindUI");
    return true;
}

void detachWindUI() {
    if (!gAttached.exchange(false)) return;
    if (pOff) {
        if (gToggleId >= 0) pOff(gToggleId);
        if (gSliderId >= 0) pOff(gSliderId);
    }
    gToggleId = gSliderId = -1;
    diag::info("WindUI: detached");
}

bool windUIAttached() {
    return gAttached.load();
}

} // namespace customskies
