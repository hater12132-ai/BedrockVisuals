#include "bedrockvisuals/Hooks.hpp"
#include "bedrockvisuals/Modules.hpp"
#include "bedrockvisuals/Render.hpp"
#include "bedrockvisuals/Diag.hpp"

#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

namespace bedrockvisuals::hooks {
namespace {

constexpr std::string_view kSigGameModeAttackInternal =
    "? ? ? D1 ? ? ? A9 ? ? ? F9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 58 D0 3B D5 F3 03 00 AA F4 03 03 AA ? ? ? F9 F5 03 02 2A";

constexpr std::string_view kSigClientInstanceUpdate =
    "? ? ? FC ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 ? ? ? D1 59 D0 3B D5 ? ? ? 52 F5 03 02 2A";

constexpr std::string_view kSigClientInstanceGetLocalPlayer =
    "? ? ? D1 ? ? ? A9 ? ? ? F9 ? ? ? 91 53 D0 3B D5 E8 03 00 AA ? ? ? 91 ? ? ? F9 ? ? ? 91 ? ? ? F8 ? ? ? ? ? ? ? 91 ? ? ? ? ? ? ? 36 ? ? ? 91 ? ? ? 52 ? ? ? ? ? ? ? ? E0 03 1F AA ? ? ? F9 ? ? ? F8 1F 01 09 EB ? ? ? 54 ? ? ? A9 ? ? ? F9 ? ? ? 91 C0 03 5F D6";

constexpr std::string_view kSigUiFinish =
    "FF 43 01 D1 FE 0B 00 F9 F4 4F 02 A9 FD 7B 03 A9 FD C3 00 91 F3 03 00 AA";

constexpr std::string_view kSigFill =
    "FF 83 01 D1 F6 57 03 A9 F4 4F 04 A9 FD 7B 05 A9 FD 43 01 91";

constexpr const char* kModule = "libminecraftpe.so";

using AttackFn = bool (*)(void*, void*, bool, const void*);
using ClientUpdateFn = void* (*)(void*, bool);
using GetLocalPlayerFn = void* (*)(void*);
using FinishFn = void (*)(void*);

AttackFn oAttack = nullptr;
ClientUpdateFn oClientUpdate = nullptr;
GetLocalPlayerFn getLocalPlayer = nullptr;
FinishFn oFinish = nullptr;

void* gAttackTarget = nullptr;
void* gAttackDetour = nullptr;
void* gClientTarget = nullptr;
void* gClientDetour = nullptr;
void* gFinishTarget = nullptr;
void* gFinishDetour = nullptr;

std::atomic<bool> gAttackOk{false};
std::atomic<bool> gClientOk{false};
std::atomic<bool> gRenderOk{false};
std::atomic<bool> gEnabled{false};
std::atomic<int> gAttackCount{0};
std::atomic<int> gClientTicks{0};

void* resolveOne(const char* label, std::string_view sig) {
    auto addr = pl::memory::resolveSignature(sig, kModule);
    if (addr) {
        diag::info(std::string(label) + " FOUND @ " + std::to_string(addr));
        return reinterpret_cast<void*>(addr);
    }
    diag::warn(std::string(label) + " NOT FOUND");
    return nullptr;
}

bool installHook(void* target, void* detour, void** original, void** outTarget, void** outDetour) {
    if (!target || !detour || !original) return false;
    int rc = pl::memory::hook(target, detour, original, pl::memory::HookPriority::Normal);
    if (rc == 0) {
        *outTarget = target;
        *outDetour = detour;
        return true;
    }
    return false;
}

bool attackDetour(void* gameMode, void* target, bool playPredictiveSound, const void* hitPosition) {
    bool result = false;
    if (oAttack)
        result = oAttack(gameMode, target, playPredictiveSound, hitPosition);

    if (!gEnabled.load(std::memory_order_relaxed))
        return result;

    if (target) {
        int n = gAttackCount.fetch_add(1) + 1;
        if (n <= 8 || (n % 25) == 0)
            diag::info("ATTACK #" + std::to_string(n));

        float hx = 0, hy = 0, hz = 0;
        bool hasHitPos = false;
        if (hitPosition) {
            auto addr = reinterpret_cast<uintptr_t>(hitPosition);
            if (addr > 0x100000ULL && (addr & 3ULL) == 0) {
                const float* v = reinterpret_cast<const float*>(hitPosition);
                float a = v[0], b = v[1], c = v[2];
                if (std::isfinite(a) && std::isfinite(b) && std::isfinite(c)
                    && !(std::fabs(a) < 0.01f && std::fabs(b) < 0.01f && std::fabs(c) < 0.01f)
                    && std::fabs(a) < 3e7f && std::fabs(c) < 3e7f && b > -80.f && b < 400.f) {
                    hx = a; hy = b; hz = c;
                    hasHitPos = true;
                }
            }
        }
        modules::onHitWorld(hx, hy, hz, hasHitPos);
    }
    return result;
}

void* clientUpdateDetour(void* ci, bool v) {
    void* ret = oClientUpdate ? oClientUpdate(ci, v) : nullptr;
    if (!gEnabled.load(std::memory_order_relaxed))
        return ret;

    int t = gClientTicks.fetch_add(1) + 1;
    if (t == 1) diag::info("CLIENT UPDATE hook alive");

    void* lp = nullptr;
    if (getLocalPlayer && ci) {
        try { lp = getLocalPlayer(ci); } catch (...) { lp = nullptr; }
    }
    modules::onTick(16.f, lp);
    return ret;
}

void finishDetour(void* self) {
    render::onUiFinish(self);
    if (oFinish) oFinish(self);
}

} // namespace

bool install() {
    diag::stage("hooks.install", "begin");
    gEnabled.store(true);

    if (auto addr = resolveOne("GameModeAttackInternal", kSigGameModeAttackInternal)) {
        if (installHook(addr, reinterpret_cast<void*>(&attackDetour),
                        reinterpret_cast<void**>(&oAttack),
                        &gAttackTarget, &gAttackDetour)) {
            gAttackOk.store(true);
            diag::info("ATTACK HOOK OK");
        } else diag::error("ATTACK HOOK FAILED");
    }

    if (auto addr = resolveOne("ClientInstanceGetLocalPlayer", kSigClientInstanceGetLocalPlayer)) {
        getLocalPlayer = reinterpret_cast<GetLocalPlayerFn>(addr);
        diag::info("getLocalPlayer ready");
    }

    if (auto addr = resolveOne("ClientInstanceUpdate", kSigClientInstanceUpdate)) {
        if (installHook(addr, reinterpret_cast<void*>(&clientUpdateDetour),
                        reinterpret_cast<void**>(&oClientUpdate),
                        &gClientTarget, &gClientDetour)) {
            gClientOk.store(true);
            diag::info("CLIENT HOOK OK");
        } else diag::error("CLIENT HOOK FAILED");
    }

    if (auto addr = resolveOne("UI fillRect", kSigFill)) {
        render::setFill(addr);
    } else {
        constexpr std::string_view kSigFill2 =
            "? ? ? D1 ? ? ? A9 ? ? ? A9 ? ? ? A9 ? ? ? 91 F3 03 00 AA F4 03 01 AA";
        if (auto a2 = resolveOne("UI fillRect(alt)", kSigFill2))
            render::setFill(a2);
        else
            diag::warn("fillRect not found");
    }

    bool fin = false;
    if (auto addr = resolveOne("UI finish", kSigUiFinish)) {
        if (installHook(addr, reinterpret_cast<void*>(&finishDetour),
                        reinterpret_cast<void**>(&oFinish),
                        &gFinishTarget, &gFinishDetour)) {
            render::setFinish(reinterpret_cast<void*>(oFinish));
            fin = true;
        }
    }
    if (!fin) {
        constexpr std::string_view kAlt =
            "? ? ? D1 ? ? ? F9 ? ? ? A9 ? ? ? A9 ? ? ? 91 F3 03 00 AA";
        if (auto addr = resolveOne("UI finish(alt)", kAlt)) {
            if (installHook(addr, reinterpret_cast<void*>(&finishDetour),
                            reinterpret_cast<void**>(&oFinish),
                            &gFinishTarget, &gFinishDetour)) {
                render::setFinish(reinterpret_cast<void*>(oFinish));
                fin = true;
            }
        }
    }
    gRenderOk.store(fin);
    if (fin) diag::info("RENDER HOOK OK");
    else diag::warn("RENDER HOOK missing — visuals won't draw");

    render::install();
    diag::stage("hooks.install", "done");
    return gAttackOk.load() || gClientOk.load();
}

void uninstall() {
    gEnabled.store(false);
    if (gAttackTarget && gAttackDetour)
        pl::memory::unhook(gAttackTarget, gAttackDetour);
    if (gClientTarget && gClientDetour)
        pl::memory::unhook(gClientTarget, gClientDetour);
    if (gFinishTarget && gFinishDetour)
        pl::memory::unhook(gFinishTarget, gFinishDetour);
    gAttackTarget = gAttackDetour = nullptr;
    gClientTarget = gClientDetour = nullptr;
    gFinishTarget = gFinishDetour = nullptr;
}

bool attackInstalled() { return gAttackOk.load(); }
bool jumpInstalled() { return gClientOk.load(); }

} // namespace
