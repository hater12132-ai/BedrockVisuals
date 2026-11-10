#include "customskies/Diag.hpp"
#include "customskies/Config.hpp"
#include "customskies/CustomSkiesModule.hpp"
#include "customskies/FrameHook.hpp"
#include "customskies/GlSkyHook.hpp"
#include "customskies/WindUIBridge.hpp"

#include <pl/Mod.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

namespace {

customskies::CustomSkiesModule* gModule = nullptr;

void onFrameThunk() {
    if (gModule) gModule->onFrame();
}

void onSkyDrawThunk() {
    if (gModule) gModule->onSkyDraw();
}

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

// Retry WindUI attach a few times — WindUIBE may load after us
void windUIAttachWorker() {
    for (int i = 0; i < 20; ++i) {
        if (customskies::tryAttachWindUI()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    customskies::diag::info("WindUI: gave up attaching (mod still works without UI)");
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
        auto dir = ctx.configDir().string();
        customskies::diag::init(dir);
        customskies::diag::info("CustomSkies loading…");

        std::error_code ec;
        fs::create_directories(dir, ec);

        mConfig = customskies::ModConfig{};

        mModule = std::make_unique<customskies::CustomSkiesModule>();
        mModule->setConfig(mConfig);
        mModule->setVideoPath(findVideo(dir));
        mModule->onInit();
        gModule = mModule.get();

        customskies::diag::stage("load", "ok");
        customskies::diag::dumpStatus();
        return true;
    }

    bool enable(pl::mod::ModContext&) {
        customskies::diag::stage("enable");

        customskies::setFrameCallback(&onFrameThunk);
        if (!customskies::installFrameHook()) {
            customskies::diag::warn("eglSwapBuffers hook failed (non-fatal)");
        }

        customskies::setSkyDrawCallback(&onSkyDrawThunk);
        if (!customskies::installGlSkyHook()) {
            customskies::diag::error("glClear hook failed — sky will not draw");
            customskies::diag::setLastError("glClear hook failed");
        } else {
            customskies::diag::info("glClear sky hook active");
        }

        // Start decoder/draw according to config (default enabled)
        if (mModule) mModule->onEnable();

        // Attach to WindUIBE menu if present (background retries)
        try {
            mUiThread = std::thread(windUIAttachWorker);
            mUiThread.detach();
        } catch (...) {
            customskies::diag::warn("WindUI attach thread failed");
        }

        return true;
    }

    bool disable(pl::mod::ModContext&) {
        customskies::diag::stage("disable");
        customskies::detachWindUI();
        customskies::removeGlSkyHook();
        customskies::setSkyDrawCallback(nullptr);
        customskies::removeFrameHook();
        customskies::setFrameCallback(nullptr);
        if (mModule) mModule->onDisable();
        return true;
    }

    bool unload(pl::mod::ModContext&) {
        customskies::diag::stage("unload");
        customskies::detachWindUI();
        customskies::removeGlSkyHook();
        customskies::setSkyDrawCallback(nullptr);
        customskies::removeFrameHook();
        customskies::setFrameCallback(nullptr);
        gModule = nullptr;
        if (mModule) {
            mModule->onDisable();
            mModule.reset();
        }
        return true;
    }

private:
    customskies::ModConfig mConfig;
    std::unique_ptr<customskies::CustomSkiesModule> mModule;
    std::thread mUiThread;
};

PL_REGISTER_MOD(CustomSkiesMod, CustomSkiesMod{})
