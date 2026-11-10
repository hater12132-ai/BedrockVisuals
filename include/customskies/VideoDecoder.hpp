#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace customskies {

// Decodes sky.mp4 on a background thread into double-buffered RGBA frames.
// Main/render thread only calls currentFrame() — never blocks on decode.
class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    // Open file and start looping decode thread. Returns false on failure.
    bool start(const std::string& path);

    void stop();

    bool ready() const { return mReady.load(std::memory_order_acquire); }

    // Latest decoded frame (RGBA8). Valid until next call that succeeds.
    // Returns nullptr if no frame yet.
    const uint8_t* currentFrame(int& outW, int& outH, uint64_t& outSerial);

private:
    void threadMain(std::string path);
    bool decodeLoop(const std::string& path);

    std::atomic<bool> mStop{false};
    std::atomic<bool> mReady{false};
    std::thread mThread;

    std::mutex mMu;
    std::vector<uint8_t> mBuf[2];
    int mW = 0, mH = 0;
    int mFront = 0; // index of buffer readers should use
    uint64_t mSerial = 0;
};

} // namespace customskies
