#pragma once

#include "customskies/Module.hpp"
#include "customskies/Config.hpp"
#include "customskies/VideoDecoder.hpp"
#include "customskies/SkyRenderer.hpp"

#include <atomic>
#include <memory>
#include <string>

namespace customskies {

class CustomSkiesModule : public Module {
public:
    CustomSkiesModule();
    ~CustomSkiesModule() override;

    void onInit() override;
    void onEnable() override;
    void onDisable() override;
    void onFrame() override;

    // Called from glClear detour on the GL thread
    void onSkyDraw();

    void setConfig(const ModConfig& cfg);
    const ModConfig& config() const { return mConfig; }

    void setVideoPath(std::string path);
    const std::string& videoPath() const { return mVideoPath; }

private:
    ModConfig mConfig;
    std::string mVideoPath;

    std::unique_ptr<VideoDecoder> mDecoder;
    std::unique_ptr<SkyRenderer> mRenderer;
    std::atomic<bool> mWantsDraw{false};
};

} // namespace customskies
