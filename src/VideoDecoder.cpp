#include "customskies/VideoDecoder.hpp"
#include "customskies/Diag.hpp"

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>

#include <android/native_window.h>

#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace customskies {
namespace {

// Convert NV12 (semi-planar YUV420) → RGBA
void nv12ToRgba(const uint8_t* y, const uint8_t* uv, int w, int h,
                int yStride, int uvStride, uint8_t* out) {
    for (int row = 0; row < h; ++row) {
        const uint8_t* yRow = y + row * yStride;
        const uint8_t* uvRow = uv + (row / 2) * uvStride;
        uint8_t* dst = out + row * w * 4;
        for (int col = 0; col < w; ++col) {
            int Y = yRow[col];
            int U = uvRow[(col & ~1)] - 128;
            int V = uvRow[(col & ~1) + 1] - 128;
            int C = Y - 16;
            if (C < 0) C = 0;
            int R = (298 * C + 409 * V + 128) >> 8;
            int G = (298 * C - 100 * U - 208 * V + 128) >> 8;
            int B = (298 * C + 516 * U + 128) >> 8;
            dst[0] = (uint8_t)(R < 0 ? 0 : R > 255 ? 255 : R);
            dst[1] = (uint8_t)(G < 0 ? 0 : G > 255 ? 255 : G);
            dst[2] = (uint8_t)(B < 0 ? 0 : B > 255 ? 255 : B);
            dst[3] = 255;
        }
    }
}

// Convert I420 (planar YUV420) → RGBA
void i420ToRgba(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                int w, int h, int yStride, int uStride, int vStride, uint8_t* out) {
    for (int row = 0; row < h; ++row) {
        const uint8_t* yRow = y + row * yStride;
        const uint8_t* uRow = u + (row / 2) * uStride;
        const uint8_t* vRow = v + (row / 2) * vStride;
        uint8_t* dst = out + row * w * 4;
        for (int col = 0; col < w; ++col) {
            int Y = yRow[col];
            int U = uRow[col / 2] - 128;
            int V = vRow[col / 2] - 128;
            int C = Y - 16;
            if (C < 0) C = 0;
            int R = (298 * C + 409 * V + 128) >> 8;
            int G = (298 * C - 100 * U - 208 * V + 128) >> 8;
            int B = (298 * C + 516 * U + 128) >> 8;
            dst[0] = (uint8_t)(R < 0 ? 0 : R > 255 ? 255 : R);
            dst[1] = (uint8_t)(G < 0 ? 0 : G > 255 ? 255 : G);
            dst[2] = (uint8_t)(B < 0 ? 0 : B > 255 ? 255 : B);
            dst[3] = 255;
        }
    }
}

} // namespace

VideoDecoder::VideoDecoder() = default;

VideoDecoder::~VideoDecoder() {
    stop();
}

bool VideoDecoder::start(const std::string& path) {
    stop();
    mStop.store(false);
    mReady.store(false);
    try {
        mThread = std::thread([this, path] { threadMain(path); });
    } catch (...) {
        diag::error("VideoDecoder: failed to start thread");
        return false;
    }
    return true;
}

void VideoDecoder::stop() {
    mStop.store(true);
    if (mThread.joinable()) mThread.join();
    mReady.store(false);
}

const uint8_t* VideoDecoder::currentFrame(int& outW, int& outH, uint64_t& outSerial) {
    std::lock_guard lock(mMu);
    if (!mReady.load(std::memory_order_relaxed) || mW <= 0 || mH <= 0) {
        outW = outH = 0;
        outSerial = 0;
        return nullptr;
    }
    outW = mW;
    outH = mH;
    outSerial = mSerial;
    return mBuf[mFront].data();
}

void VideoDecoder::threadMain(std::string path) {
    diag::info(std::string("VideoDecoder thread start: ") + path);

    // Loop the file forever until stop
    while (!mStop.load(std::memory_order_relaxed)) {
        if (!decodeLoop(path)) {
            diag::warn("VideoDecoder: decodeLoop failed, retry in 1s");
            for (int i = 0; i < 10 && !mStop.load(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        // natural loop end → restart from beginning
    }
    diag::info("VideoDecoder thread exit");
}

bool VideoDecoder::decodeLoop(const std::string& path) {
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        diag::error(std::string("VideoDecoder: cannot open ") + path);
        return false;
    }

    AMediaExtractor* extractor = AMediaExtractor_new();
    if (!extractor) {
        ::close(fd);
        return false;
    }

    media_status_t st = AMediaExtractor_setDataSourceFd(extractor, fd, 0, ::lseek(fd, 0, SEEK_END));
    ::lseek(fd, 0, SEEK_SET);
    if (st != AMEDIA_OK) {
        diag::error("VideoDecoder: setDataSourceFd failed");
        AMediaExtractor_delete(extractor);
        ::close(fd);
        return false;
    }

    int trackCount = AMediaExtractor_getTrackCount(extractor);
    int videoTrack = -1;
    AMediaFormat* format = nullptr;
    const char* mime = nullptr;

    for (int i = 0; i < trackCount; ++i) {
        AMediaFormat* f = AMediaExtractor_getTrackFormat(extractor, i);
        const char* m = nullptr;
        if (AMediaFormat_getString(f, AMEDIAFORMAT_KEY_MIME, &m) && m && std::strncmp(m, "video/", 6) == 0) {
            videoTrack = i;
            format = f;
            mime = m;
            break;
        }
        AMediaFormat_delete(f);
    }

    if (videoTrack < 0 || !format) {
        diag::error("VideoDecoder: no video track");
        AMediaExtractor_delete(extractor);
        ::close(fd);
        return false;
    }

    AMediaExtractor_selectTrack(extractor, videoTrack);

    int32_t width = 0, height = 0;
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
    AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
    diag::info(std::string("VideoDecoder: ") + mime + " " + std::to_string(width) + "x" + std::to_string(height));

    AMediaCodec* codec = AMediaCodec_createDecoderByType(mime);
    if (!codec) {
        diag::error("VideoDecoder: createDecoder failed");
        AMediaFormat_delete(format);
        AMediaExtractor_delete(extractor);
        ::close(fd);
        return false;
    }

    // Request a flexible YUV output we can read
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x7F420888); // COLOR_FormatYUV420Flexible

    st = AMediaCodec_configure(codec, format, nullptr, nullptr, 0);
    AMediaFormat_delete(format);
    format = nullptr;
    if (st != AMEDIA_OK) {
        diag::error("VideoDecoder: configure failed");
        AMediaCodec_delete(codec);
        AMediaExtractor_delete(extractor);
        ::close(fd);
        return false;
    }

    AMediaCodec_start(codec);

    bool inputEos = false;
    bool outputEos = false;
    int32_t colorFormat = 0;
    int32_t stride = width;
    int32_t sliceHeight = height;

    // Pace to ~30fps
    using clock = std::chrono::steady_clock;
    auto nextPts = clock::now();

    while (!mStop.load(std::memory_order_relaxed) && !outputEos) {
        // Feed input
        if (!inputEos) {
            ssize_t inIdx = AMediaCodec_dequeueInputBuffer(codec, 2000);
            if (inIdx >= 0) {
                size_t bufSize = 0;
                uint8_t* buf = AMediaCodec_getInputBuffer(codec, inIdx, &bufSize);
                ssize_t sample = AMediaExtractor_readSampleData(extractor, buf, bufSize);
                if (sample < 0) {
                    AMediaCodec_queueInputBuffer(codec, inIdx, 0, 0, 0, AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);
                    inputEos = true;
                } else {
                    int64_t pts = AMediaExtractor_getSampleTime(extractor);
                    AMediaCodec_queueInputBuffer(codec, inIdx, 0, sample, pts, 0);
                    AMediaExtractor_advance(extractor);
                }
            }
        }

        // Drain output
        AMediaCodecBufferInfo info;
        ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(codec, &info, 2000);
        if (outIdx >= 0) {
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
                outputEos = true;
            }

            if (info.size > 0 && width > 0 && height > 0) {
                size_t outSize = 0;
                uint8_t* outBuf = AMediaCodec_getOutputBuffer(codec, outIdx, &outSize);

                if (outBuf && outSize > 0) {
                    // Ensure RGBA buffers sized
                    size_t rgbaSize = (size_t)width * height * 4;
                    {
                        std::lock_guard lock(mMu);
                        int back = 1 - mFront;
                        if (mBuf[back].size() != rgbaSize) mBuf[back].assign(rgbaSize, 0);

                        // Prefer NV12 layout: Y plane then interleaved UV
                        // Flexible format may vary; try common layouts
                        const uint8_t* yPlane = outBuf;
                        int yStride = stride > 0 ? stride : width;
                        int ySize = yStride * (sliceHeight > 0 ? sliceHeight : height);

                        if (colorFormat == 21 || colorFormat == 0x7FA30C04 || colorFormat == 0) {
                            // NV12 / unknown → treat as NV12
                            const uint8_t* uvPlane = outBuf + ySize;
                            int uvStride = yStride;
                            nv12ToRgba(yPlane, uvPlane, width, height, yStride, uvStride, mBuf[back].data());
                        } else if (colorFormat == 19) {
                            // I420
                            const uint8_t* uPlane = outBuf + ySize;
                            const uint8_t* vPlane = uPlane + (yStride / 2) * ((sliceHeight > 0 ? sliceHeight : height) / 2);
                            i420ToRgba(yPlane, uPlane, vPlane, width, height, yStride, yStride / 2, yStride / 2, mBuf[back].data());
                        } else {
                            // Fallback NV12
                            const uint8_t* uvPlane = outBuf + ySize;
                            nv12ToRgba(yPlane, uvPlane, width, height, yStride, yStride, mBuf[back].data());
                        }

                        mW = width;
                        mH = height;
                        mFront = back;
                        ++mSerial;
                        mReady.store(true, std::memory_order_release);
                    }
                }
            }

            AMediaCodec_releaseOutputBuffer(codec, outIdx, false);

            // Pace
            nextPts += std::chrono::milliseconds(33);
            auto now = clock::now();
            if (nextPts > now)
                std::this_thread::sleep_until(nextPts);
            else
                nextPts = now;
        } else if (outIdx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat* newFmt = AMediaCodec_getOutputFormat(codec);
            if (newFmt) {
                AMediaFormat_getInt32(newFmt, AMEDIAFORMAT_KEY_WIDTH, &width);
                AMediaFormat_getInt32(newFmt, AMEDIAFORMAT_KEY_HEIGHT, &height);
                AMediaFormat_getInt32(newFmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, &colorFormat);
                AMediaFormat_getInt32(newFmt, AMEDIAFORMAT_KEY_STRIDE, &stride);
                AMediaFormat_getInt32(newFmt, "slice-height", &sliceHeight);
                if (stride <= 0) stride = width;
                if (sliceHeight <= 0) sliceHeight = height;
                diag::info(std::string("VideoDecoder: format ") + std::to_string(width) + "x" +
                           std::to_string(height) + " color=" + std::to_string(colorFormat) +
                           " stride=" + std::to_string(stride));
                AMediaFormat_delete(newFmt);
            }
        } else if (outIdx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
            // spin
        }
    }

    AMediaCodec_stop(codec);
    AMediaCodec_delete(codec);
    AMediaExtractor_delete(extractor);
    ::close(fd);
    return true;
}

} // namespace customskies
