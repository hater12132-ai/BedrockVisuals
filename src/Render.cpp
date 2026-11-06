#include "bedrockvisuals/Render.hpp"
#include "bedrockvisuals/Modules.hpp"
#include "bedrockvisuals/Diag.hpp"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace bedrockvisuals::render {
namespace {

struct Color { float r,g,b,a; };
struct RectangleArea { float x0, x1, y0, y1; };

using FillFn = void (*)(void*, RectangleArea const&, Color const&, Color const&, Color const&, Color const&);
using FinishFn = void (*)(void*);

FillFn oFill = nullptr;
FinishFn oFinishStored = nullptr;
std::atomic<bool> gReady{false};
std::atomic<uint64_t> gTimeMs{0};
std::atomic<int> gPaint{0};

void fill(void* ctx, float x, float y, float w, float h, Color c, float a) {
    if (!oFill || !ctx) return;
    if (!(w >= 1.f && h >= 1.f) || w > 400.f || h > 80.f) return;
    c.a = a;
    RectangleArea ra{x, x + w, y, y + h};
    oFill(ctx, ra, c, c, c, c);
}

void glowDot(void* ctx, float x, float y, float size, Color col, float alpha) {
    if (alpha < 0.02f || size < 1.f) return;
    float s0 = size * 3.0f;
    fill(ctx, x - s0 * 0.5f, y - s0 * 0.5f, s0, s0, col, alpha * 0.16f);
    float s1 = size * 1.8f;
    fill(ctx, x - s1 * 0.5f, y - s1 * 0.5f, s1, s1, col, alpha * 0.38f);
    Color core{std::min(1.f, col.r * 0.4f + 0.6f),
               std::min(1.f, col.g * 0.4f + 0.6f),
               std::min(1.f, col.b * 0.4f + 0.6f), 1.f};
    fill(ctx, x - size * 0.5f, y - size * 0.5f, size, size, core, std::min(1.f, alpha * 1.1f));
}

void glowRing(void* ctx, float cx, float cy, float rx, float ry, int n, Color col, float alpha, float dot, float phase) {
    for (int i = 0; i < n; ++i) {
        float a = phase + i * (6.2831853f / static_cast<float>(n));
        glowDot(ctx, cx + std::cos(a) * rx, cy + std::sin(a) * ry, dot, col, alpha);
    }
}

void drawCircleFx(void* ctx, float cx, float cy, const modules::CircleFx& fx, float t) {
    float expand = 0.35f + t * 1.65f;
    float alpha = fx.a * (1.f - t);
    if (alpha < 0.04f) return;
    Color col{fx.r, fx.g, fx.b, 1.f};
    float base = 28.f * fx.size;
    float rx = base * expand;
    float ry = rx * 0.38f;
    glowRing(ctx, cx, cy, rx, ry, 22, col, alpha, 2.4f, t * 2.f);
    if (t < 0.5f)
        glowRing(ctx, cx, cy, rx * 0.55f, ry * 0.55f, 14, col, alpha * 0.6f, 2.0f, -t * 3.f);
}

void drawHitbox(void* ctx, float cx, float cy, float sc, const modules::HitboxLock& hb) {
    auto& s = modules::shared();
    Color col{s.hbR.load() / 255.f, s.hbG.load() / 255.f, s.hbB.load() / 255.f, 1.f};
    float a = s.hbA.load() / 255.f * (1.f - hb.age / std::max(hb.maxAge, 1.f));
    float th = std::max(1.5f, s.hbThick.load());
    float h = std::clamp(70.f * sc, 40.f, 150.f);
    float w = h * 0.45f;
    float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
    float y0 = cy - h * 0.5f, y1 = cy + h * 0.5f;
    float arm = std::min(w, h) * 0.25f;
    auto bar = [&](float x, float y, float bw, float bh) { fill(ctx, x, y, bw, bh, col, a); };
    bar(x0, y0, arm, th); bar(x0, y0, th, arm);
    bar(x1 - arm, y0, arm, th); bar(x1 - th, y0, th, arm);
    bar(x0, y1 - th, arm, th); bar(x0, y1 - arm, th, arm);
    bar(x1 - arm, y1 - th, arm, th); bar(x1 - th, y1 - arm, th, arm);
}

void drawCrosshair(void* ctx, float cx, float cy) {
    auto& s = modules::shared();
    if (!s.crosshairOn.load()) return;
    float size = s.chSize.load();
    float th = s.chThick.load();
    float a = s.chA.load() / 255.f;
    Color col{s.chR.load() / 255.f, s.chG.load() / 255.f, s.chB.load() / 255.f, 1.f};
    if (s.chRgb.load()) {
        float t = gTimeMs.load() * 0.001f;
        col.r = 0.5f + 0.5f * std::sin(t);
        col.g = 0.5f + 0.5f * std::sin(t + 2.094f);
        col.b = 0.5f + 0.5f * std::sin(t + 4.188f);
    }
    switch (s.chStyle.load()) {
    case 1:
        glowRing(ctx, cx, cy, size, size, 20, col, a, th, 0.f);
        break;
    case 2:
        glowRing(ctx, cx, cy, size, size, 18, col, a, th, 0.f);
        fill(ctx, cx - 1.5f, cy - 1.5f, 3.f, 3.f, col, a);
        break;
    case 3:
        fill(ctx, cx - size, cy - size, size * 0.45f, th, col, a);
        fill(ctx, cx - size, cy - size, th, size * 0.45f, col, a);
        fill(ctx, cx + size - size * 0.45f, cy - size, size * 0.45f, th, col, a);
        fill(ctx, cx + size - th, cy - size, th, size * 0.45f, col, a);
        fill(ctx, cx - size, cy + size - th, size * 0.45f, th, col, a);
        fill(ctx, cx - size, cy + size - size * 0.45f, th, size * 0.45f, col, a);
        fill(ctx, cx + size - size * 0.45f, cy + size - th, size * 0.45f, th, col, a);
        fill(ctx, cx + size - th, cy + size - size * 0.45f, th, size * 0.45f, col, a);
        break;
    case 4:
        fill(ctx, cx - size * 0.25f, cy - size * 0.25f, size * 0.5f, size * 0.5f, col, a);
        break;
    default:
        fill(ctx, cx - size, cy - th * 0.5f, size * 2.f, th, col, a);
        fill(ctx, cx - th * 0.5f, cy - size, th, size * 2.f, col, a);
        break;
    }
}

void paint(void* ctx) {
    if (!ctx || !oFill) return;
    float sw = 640.f, sh = 400.f;
    int n = gPaint.fetch_add(1) + 1;
    gTimeMs.store(gTimeMs.load() + 16);

    auto& s = modules::shared();
    std::vector<modules::CircleFx> jumps, dmgs;
    modules::HitboxLock hb{};
    {
        std::lock_guard lock(s.mu);
        jumps = s.jumps;
        dmgs = s.damages;
        hb = s.hitbox;
    }

    for (auto& fx : jumps) {
        if (!fx.hasWorld) continue;
        float sx, sy, sc;
        if (!modules::project(fx.worldX, fx.worldY, fx.worldZ, sw, sh, sx, sy, sc)) continue;
        drawCircleFx(ctx, sx, sy, fx, fx.age / std::max(fx.maxAge, 1.f));
    }
    for (auto& fx : dmgs) {
        if (!fx.hasWorld) continue;
        float sx, sy, sc;
        if (!modules::project(fx.worldX, fx.worldY, fx.worldZ, sw, sh, sx, sy, sc)) continue;
        drawCircleFx(ctx, sx, sy, fx, fx.age / std::max(fx.maxAge, 1.f));
    }
    if (hb.hasWorld && s.hitboxOn.load()) {
        float sx, sy, sc;
        if (modules::project(hb.worldX, hb.worldY, hb.worldZ, sw, sh, sx, sy, sc))
            drawHitbox(ctx, sx, sy, sc, hb);
    }
    drawCrosshair(ctx, sw * 0.5f, sh * 0.5f);

    if (n <= 3 || (n % 400) == 0)
        diag::info("PAINT #" + std::to_string(n) +
                   " jumps=" + std::to_string(jumps.size()) +
                   " dmgs=" + std::to_string(dmgs.size()) +
                   " hb=" + (hb.hasWorld ? "Y" : "n"));
}

} // namespace

void setFill(void* fn) { oFill = reinterpret_cast<FillFn>(fn); }
void setFinish(void* fn) { oFinishStored = reinterpret_cast<FinishFn>(fn); }

void onUiFinish(void* ctx) { paint(ctx); }

bool ready() { return gReady.load(); }

bool install() {
    gReady.store(oFill != nullptr);
    diag::info("Render: JumpCircle + DamageCircle + Hitbox + Crosshair");
    return gReady.load();
}

void uninstall() { gReady.store(false); }

} // namespace
