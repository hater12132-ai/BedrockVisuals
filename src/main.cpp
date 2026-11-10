#include "customskies/Diag.hpp"
#include "customskies/Config.hpp"
#include "customskies/CustomSkiesModule.hpp"
#include "customskies/FrameHook.hpp"
#include "customskies/GlSkyHook.hpp"
#include "customskies/WindUIBridge.hpp"

#include <pl/Mod.hpp>

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace {

customskies::CustomSkiesModule* gModule = nullptr;
std::atomic<bool> gDeferredDone{false};

std::string findVideo(const fs::path& configDir) {
    const fs::path candidates[] = {
        configDir / ".." / "assets" / "sky.mp4",
        configDir / "assets" / "sky.mp4",
        configDir / "sky.mp4",
    };
    for (const auto& p : candidates) {
        std::error_code ec;
        auto norm = p.lexically_normal();
        if (fs::exists(norm, ec) && fs::is_regular_file(norm, ec)) {
            return norm.string();
        }
    }
    return (configDir / ".." / "assets" / "sky.mp4").lexically_normal().string();
}

void doDeferredStart() {
    if (gDeferredDone.exchange(true)) return;

    customskies::diag::info("deferred start: hooks + decoder");

    customskies::setSkyDrawCallback([]() {
        if (gModule) gModule->onSkyDraw();
    });

    if (!customskies::installGlSkyHook()) {
        customskies::diag::error("glClear hook failed");
    } else {
        customskies::diag::info("glClear hook ok");
    }

    if (gModule) {
        gModule->onEnable(); // starts MediaCodec on background thread
    }

    // Optional — never required
    customskies::tryAttachWindUI();
}

void onFrameThunk() {
    doDeferredStart();
    if (gModule) gModule->onFrame();
}

} // namespace

namespace customskies {
CustomSkiesModule* getActiveModule() {
    return gModule;
}
} // namespace customskies

class CustomSkiesMod {
public:
    bool load(pl::mod::ModContext& ctx) {
        // Keep this FAST — Levi waits on every mod's load()
        auto dir = ctx.configDir().string();
        customskies::diag::init(dir);
        customskies::diag::info("load()");

        std::error_code ec;
        fs::create_directories(dir, ec);

        mModule = std::make_unique<customskies::CustomSkiesModule>();
        mModule->setConfig(customskies::ModConfig{});
        mModule->setVideoPath(findVideo(dir));
        mModule->onInit();
        gModule = mModule.get();

        customskies::diag::info("load() done");
        return true;
    }

    bool enable(pl::mod::ModContext&) {
        // Keep this FAST — no MediaCodec, no glClear hook here
        customskies::diag::info("enable()");
        gDeferredDone.store(false);

        customskies::setFrameCallback(&onFrameThunk);
        // Only eglSwapBuffers so we get a callback after the game is actually rendering
        if (!customskies::installFrameHook()) {
            customskies::diag::warn("eglSwapBuffers hook failed at enable");
        }

        customskies::diag::info("enable() done — sky starts on first frame");
        return true;
    }

    bool disable(pl::mod::ModContext&) {
        customskies::diag::info("disable()");
        customskies::detachWindUI();
        customskies::removeGlSkyHook();
        customskies::setSkyDrawCallback(nullptr);
        customskies::removeFrameHook();
        customskies::setFrameCallback(nullptr);
        gDeferredDone.store(false);
        if (mModule) mModule->onDisable();
        return true;
    }

    bool unload(pl::mod::ModContext&) {
        customskies::diag::info("unload()");
        customskies::detachWindUI();
        customskies::removeGlSkyHook();
        customskies::setSkyDrawCallback(nullptr);
        customskies::removeFrameHook();
        customskies::setFrameCallback(nullptr);
        gDeferredDone.store(false);
        gModule = nullptr;
        if (mModule) {
            mModule->onDisable();
            mModule.reset();
        }
        return true;
    }

private:
    std::unique_ptr<customskies::CustomSkiesModule> mModule;
};

PL_REGISTER_MOD(CustomSkiesMod, CustomSkiesMod{})
