#pragma once
#include "bedrockvisuals/Config.hpp"
#include <atomic>
#include <mutex>
#include <vector>

namespace bedrockvisuals::modules {

struct CircleFx {
    float x = 0, y = 0;      // screen space (set at spawn or each frame)
    float worldX = 0, worldY = 0, worldZ = 0;
    bool hasWorld = false;
    float age = 0, maxAge = 1.f;
    float size = 1.f;
    float r = 1, g = 0.5f, b = 0;
    float a = 1;
};

struct HitboxLock {
    float worldX = 0, worldY = 0, worldZ = 0;
    bool hasWorld = false;
    float age = 0, maxAge = 3.f;
    float bodyH = 1.8f;
};

struct Shared {
    std::atomic<bool> jumpOn{true};
    std::atomic<bool> damageOn{true};
    std::atomic<bool> hitboxOn{true};
    std::atomic<bool> crosshairOn{true};

    std::atomic<float> jumpDur{1.2f};
    std::atomic<float> jumpSize{1.f};
    std::atomic<int> jumpR{255}, jumpG{140}, jumpB{0}, jumpA{220};

    std::atomic<float> dmgDur{0.9f};
    std::atomic<float> dmgSize{1.f};
    std::atomic<int> dmgR{255}, dmgG{60}, dmgB{60}, dmgA{230};

    std::atomic<int> hbR{255}, hbG{255}, hbB{255}, hbA{200};
    std::atomic<float> hbThick{2.f};
    std::atomic<float> hbLock{3000.f};

    std::atomic<int> chStyle{0};
    std::atomic<float> chSize{10.f};
    std::atomic<float> chThick{2.f};
    std::atomic<int> chR{255}, chG{255}, chB{255}, chA{220};
    std::atomic<bool> chRgb{false};

    std::mutex mu;
    std::vector<CircleFx> jumps;
    std::vector<CircleFx> damages;
    HitboxLock hitbox;
};

Shared& shared();
void applyConfig(const ModConfig& c);

// Events
void onJump();
void onLand();
void onHitWorld(float x, float y, float z, bool hasPos);
void onTick(float dtMs, void* localPlayer);

// Camera / project (same as before)
bool project(float wx, float wy, float wz, float sw, float sh, float& sx, float& sy, float& sc);
bool camValid();

} // namespace
