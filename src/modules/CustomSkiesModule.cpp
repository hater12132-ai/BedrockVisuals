#include "customskies/CustomSkiesModule.hpp"
#include "customskies/Diag.hpp"

#include <string>

namespace customskies {

CustomSkiesModule::CustomSkiesModule()
    : Module("CustomSkies", "Replace the sky with a moving video") {}

CustomSkiesModule::~CustomSkiesModule() {
    onDisable();
}

void CustomSkiesModule::setConfig(const ModConfig& cfg) {
    mConfig = cfg;
}

void CustomSkiesModule::setVideoPath(std::string path) {
    mVideoPath = std::move(path);
    diag::info(std::string("video path set: ") + mVideoPath);
}

void CustomSkiesModule::onInit() {
    diag::stage("CustomSkies::onInit");
    mDecoder = std::make_unique<VideoDecoder>();
    mRenderer = std::make_unique<SkyRenderer>();
}

void CustomSkiesModule::onEnable() {
    if (!mConfig.enabled) {
        mEnabled = false;
        mWantsDraw.store(false);
        diag::info("CustomSkies: config.enabled=false — not starting");
        return;
    }

    if (mVideoPath.empty()) {
        diag::error("CustomSkies: no video path");
        mEnabled = false;
        return;
    }

    // Restart decoder if needed
    if (mDecoder) {
        mDecoder->stop();
        if (!mDecoder->start(mVideoPath)) {
            diag::error("CustomSkies: decoder start failed");
            mEnabled = false;
            return;
        }
    }

    Module::onEnable();
    mWantsDraw.store(true);
    diag::info("CustomSkies enabled — video sky active");
}

void CustomSkiesModule::onDisable() {
    mWantsDraw.store(false);
    if (mDecoder) mDecoder->stop();
    if (mRenderer) mRenderer->shutdown();
    if (mEnabled) {
        Module::onDisable();
        diag::stage("CustomSkies::onDisable");
    }
}

void CustomSkiesModule::onFrame() {
    if (!mEnabled) return;
    static unsigned n = 0;
    if ((++n % 300) == 1) {
        diag::info(std::string("frame tick ") + std::to_string(n) +
                   (mDecoder && mDecoder->ready() ? " decoder=ready" : " decoder=wait"));
    }
}

void CustomSkiesModule::onSkyDraw() {
    if (!mWantsDraw.load(std::memory_order_relaxed) || !mEnabled) return;
    if (!mDecoder || !mRenderer) return;

    if (!mRenderer->ready()) {
        if (!mRenderer->init()) {
            static bool once = false;
            if (!once) {
                once = true;
                diag::error("SkyRenderer init failed on GL thread");
            }
            return;
        }
    }

    int w = 0, h = 0;
    uint64_t serial = 0;
    const uint8_t* rgba = mDecoder->currentFrame(w, h, serial);
    if (!rgba) return;

    mRenderer->draw(rgba, w, h, serial, mConfig.opacity);
}

} // namespace customskies
