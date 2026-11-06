#include "bedrockvisuals/Modules.hpp"
#include "bedrockvisuals/Diag.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bedrockvisuals::modules {
namespace {

Shared g;

struct Cam {
    float x=0,y=0,z=0,pitch=0,yaw=0;
    bool valid=false;
};
Cam gCam;
std::mutex gCamMu;

constexpr std::size_t kStateVector = 0x228;
constexpr std::size_t kRotation    = 0x238;
struct Vec3 { float x,y,z; };
struct Vec2 { float x,y; };

bool validPtr(const void* p) {
    auto a = reinterpret_cast<uintptr_t>(p);
    return a > 0x100000ULL && (a & 7ULL) == 0;
}
bool sane(const Vec3& v) {
    if (!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) return false;
    if (std::fabs(v.x)<0.01f && std::fabs(v.y)<0.01f && std::fabs(v.z)<0.01f) return false;
    if (std::fabs(v.x)>3e7f||std::fabs(v.z)>3e7f) return false;
    if (v.y < -80.f || v.y > 400.f) return false;
    return true;
}
bool readVec3(const void* base, std::size_t off, Vec3& out) {
    if (!validPtr(base)) return false;
    auto* p = reinterpret_cast<const float*>(reinterpret_cast<const std::uint8_t*>(base)+off);
    if (!validPtr(p)) return false;
    out = {p[0],p[1],p[2]};
    return sane(out);
}
bool actorPos(void* actor, Vec3& out) {
    if (!validPtr(actor)) return false;
    auto* slot = reinterpret_cast<void* const*>(reinterpret_cast<const std::uint8_t*>(actor)+kStateVector);
    if (!validPtr(slot)) return false;
    void* svc = *slot;
    if (!validPtr(svc)) return false;
    return readVec3(svc, 0, out) || readVec3(svc, 8, out);
}
bool actorRot(void* actor, Vec2& out) {
    if (!validPtr(actor)) return false;
    auto* slot = reinterpret_cast<void* const*>(reinterpret_cast<const std::uint8_t*>(actor)+kRotation);
    if (!validPtr(slot)) return false;
    void* rc = *slot;
    if (!validPtr(rc)) return false;
    auto* f = reinterpret_cast<const float*>(rc);
    if (!validPtr(f)) return false;
    out = {f[0], f[1]};
    return std::isfinite(out.x)&&std::isfinite(out.y);
}

// Jump detection via vertical velocity proxy (pos delta)
float lastY = 0;
bool haveLastY = false;
bool wasAir = false;

} // namespace

Shared& shared() { return g; }

void applyConfig(const ModConfig& c) {
    g.jumpOn.store(c.jumpCircleEnabled);
    g.damageOn.store(c.damageCircleEnabled);
    g.hitboxOn.store(c.hitboxEnabled);
    g.crosshairOn.store(c.crosshairEnabled);
    g.jumpDur.store(c.jumpDuration);
    g.jumpSize.store(c.jumpSize);
    g.jumpR.store(c.jumpColorR); g.jumpG.store(c.jumpColorG); g.jumpB.store(c.jumpColorB); g.jumpA.store(c.jumpColorA);
    g.dmgDur.store(c.damageDuration);
    g.dmgSize.store(c.damageSize);
    g.dmgR.store(c.damageColorR); g.dmgG.store(c.damageColorG); g.dmgB.store(c.damageColorB); g.dmgA.store(c.damageColorA);
    g.hbR.store(c.hitboxColorR); g.hbG.store(c.hitboxColorG); g.hbB.store(c.hitboxColorB); g.hbA.store(c.hitboxColorA);
    g.hbThick.store(c.hitboxThickness);
    g.hbLock.store(c.hitboxLockMs);
    g.chStyle.store(c.crosshairStyle);
    g.chSize.store(c.crosshairSize);
    g.chThick.store(c.crosshairThickness);
    g.chR.store(c.crosshairColorR); g.chG.store(c.crosshairColorG); g.chB.store(c.crosshairColorB); g.chA.store(c.crosshairColorA);
    g.chRgb.store(c.crosshairRgb);
}

bool project(float wx, float wy, float wz, float sw, float sh, float& sx, float& sy, float& sc) {
    Cam cam;
    { std::lock_guard lock(gCamMu); cam = gCam; }
    if (!cam.valid) return false;
    float dx = wx-cam.x, dy = wy-cam.y, dz = wz-cam.z;
    constexpr float k = 0.017453292519943295f;
    float yaw = cam.yaw*k, pitch = cam.pitch*k;
    float cy = std::cos(yaw), sya = std::sin(yaw);
    float cp = std::cos(pitch), sp = std::sin(pitch);
    float fwdX = -sya*cp, fwdY = -sp, fwdZ = cy*cp;
    float rightX = cy, rightZ = sya;
    float upX = sya*sp, upY = cp, upZ = -cy*sp;
    float rx = dx*rightX + dz*rightZ;
    float ry = dx*upX + dy*upY + dz*upZ;
    float rz = dx*fwdX + dy*fwdY + dz*fwdZ;
    if (rz < 0.5f) return false;
    float inv = 1.15f / rz;
    sx = sw*0.5f + rx*inv*(sw*0.5f);
    sy = sh*0.5f - ry*inv*(sh*0.5f);
    sc = inv;
    return std::isfinite(sx)&&std::isfinite(sy);
}

bool camValid() {
    std::lock_guard lock(gCamMu);
    return gCam.valid;
}

void onJump() {
    if (!g.jumpOn.load()) return;
    CircleFx fx;
    fx.maxAge = std::max(0.3f, g.jumpDur.load()) * 1000.f;
    fx.size = g.jumpSize.load();
    fx.r = g.jumpR.load()/255.f;
    fx.g = g.jumpG.load()/255.f;
    fx.b = g.jumpB.load()/255.f;
    fx.a = g.jumpA.load()/255.f;
    // Anchor to player feet via camera XZ, y-1.62
    {
        std::lock_guard lock(gCamMu);
        if (gCam.valid) {
            fx.worldX = gCam.x;
            fx.worldY = gCam.y - 1.62f + 0.05f;
            fx.worldZ = gCam.z;
            fx.hasWorld = true;
        }
    }
    std::lock_guard lock(g.mu);
    if (g.jumps.size() > 8) g.jumps.erase(g.jumps.begin());
    g.jumps.push_back(fx);
    diag::info("JumpCircle spawn");
}

void onLand() {
    if (!g.jumpOn.load()) return;
    onJump(); // same effect for land for now
}

void onHitWorld(float x, float y, float z, bool hasPos) {
    if (!hasPos || !sane(Vec3{x,y,z})) return;

    if (g.damageOn.load()) {
        CircleFx fx;
        fx.worldX = x; fx.worldY = y; fx.worldZ = z;
        fx.hasWorld = true;
        fx.maxAge = std::max(0.25f, g.dmgDur.load()) * 1000.f;
        fx.size = g.dmgSize.load();
        fx.r = g.dmgR.load()/255.f;
        fx.g = g.dmgG.load()/255.f;
        fx.b = g.dmgB.load()/255.f;
        fx.a = g.dmgA.load()/255.f;
        std::lock_guard lock(g.mu);
        if (g.damages.size() > 12) g.damages.erase(g.damages.begin());
        g.damages.push_back(fx);
    }
    if (g.hitboxOn.load()) {
        std::lock_guard lock(g.mu);
        g.hitbox.worldX = x;
        g.hitbox.worldY = y;
        g.hitbox.worldZ = z;
        g.hitbox.hasWorld = true;
        g.hitbox.age = 0;
        g.hitbox.maxAge = g.hbLock.load();
        g.hitbox.bodyH = 1.8f;
    }
    static int n=0;
    if (++n <= 6 || (n%10)==0)
        diag::info("HitFX @" + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(z));
}

void onTick(float dtMs, void* localPlayer) {
    if (dtMs < 0.f || dtMs > 200.f) dtMs = 16.f;

    if (validPtr(localPlayer)) {
        Vec3 pos{}; Vec2 rot{};
        if (actorPos(localPlayer, pos) && actorRot(localPlayer, rot)) {
            {
                std::lock_guard lock(gCamMu);
                gCam = {pos.x, pos.y+1.62f, pos.z, rot.x, rot.y, true};
            }
            // Jump detect: upward speed
            if (haveLastY) {
                float vy = (pos.y - lastY) / (dtMs / 1000.f);
                bool air = vy > 0.5f || (wasAir && std::fabs(vy) > 0.05f);
                if (!wasAir && vy > 4.0f) onJump();
                if (wasAir && std::fabs(vy) < 0.15f && pos.y <= lastY + 0.05f) {
                    // landed
                    static float lastLand = 0;
                    // simple: fire land effect
                    if (g.jumpOn.load()) {
                        CircleFx fx;
                        fx.maxAge = std::max(0.3f, g.jumpDur.load()) * 800.f;
                        fx.size = g.jumpSize.load() * 0.85f;
                        fx.r = g.jumpR.load()/255.f;
                        fx.g = g.jumpG.load()/255.f;
                        fx.b = g.jumpB.load()/255.f;
                        fx.a = g.jumpA.load()/255.f * 0.85f;
                        fx.worldX = pos.x; fx.worldY = pos.y + 0.05f; fx.worldZ = pos.z;
                        fx.hasWorld = true;
                        std::lock_guard lock(g.mu);
                        if (g.jumps.size() > 8) g.jumps.erase(g.jumps.begin());
                        g.jumps.push_back(fx);
                    }
                }
                wasAir = air || vy > 0.3f;
            }
            lastY = pos.y;
            haveLastY = true;
        }
    }

    std::lock_guard lock(g.mu);
    auto tickVec = [&](std::vector<CircleFx>& v) {
        for (auto& c : v) c.age += dtMs;
        v.erase(std::remove_if(v.begin(), v.end(),
            [](const CircleFx& c){ return c.age >= c.maxAge; }), v.end());
    };
    tickVec(g.jumps);
    tickVec(g.damages);
    if (g.hitbox.hasWorld) {
        g.hitbox.age += dtMs;
        if (g.hitbox.age >= g.hitbox.maxAge) g.hitbox = {};
    }
}

} // namespace
