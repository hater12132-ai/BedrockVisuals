#pragma once

#include <cstdint>

namespace customskies {

// GLES fullscreen sky quad. Draws at depth 1.0 so world geometry covers it.
class SkyRenderer {
public:
    SkyRenderer();
    ~SkyRenderer();

    SkyRenderer(const SkyRenderer&) = delete;
    SkyRenderer& operator=(const SkyRenderer&) = delete;

    // Resolve GLES entry points (call once with a current GL context).
    bool init();

    void shutdown();

    bool ready() const { return mReady; }

    // Upload RGBA frame if serial changed, then draw fullscreen sky.
    // Safe to call from glClear detour after a full clear on FBO 0.
    void draw(const uint8_t* rgba, int w, int h, uint64_t serial, float opacity);

private:
    bool buildProgram();
    void ensureTexture(int w, int h);

    bool mReady = false;
    unsigned mProg = 0;
    unsigned mVbo = 0;
    unsigned mTex = 0;
    int mTexW = 0, mTexH = 0;
    uint64_t mUploadedSerial = 0;
    int mLocPos = -1, mLocTex = -1, mLocOpacity = -1;
};

} // namespace customskies
