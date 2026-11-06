#include "bedrockvisuals/Config.hpp"
#include "bedrockvisuals/Diag.hpp"
#include "bedrockvisuals/Hooks.hpp"
#include "bedrockvisuals/Modules.hpp"
#include "bedrockvisuals/Render.hpp"

#include <pl/Mod.hpp>
#include <pl/ModMenu.hpp>

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace {

bedrockvisuals::ModConfig gCfg;

bool asBool(const nlohmann::json& j, bool def = false) {
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_number()) return j.get<double>() != 0;
    if (j.is_string()) {
        auto s = j.get<std::string>();
        return s == "true" || s == "1" || s == "yes";
    }
    return def;
}
int asInt(const nlohmann::json& j, int def = 0) {
    if (j.is_number_integer()) return j.get<int>();
    if (j.is_number()) return static_cast<int>(j.get<double>());
    if (j.is_string()) try { return std::stoi(j.get<std::string>()); } catch (...) {}
    return def;
}
float asFloat(const nlohmann::json& j, float def = 0.f) {
    if (j.is_number()) return static_cast<float>(j.get<double>());
    if (j.is_string()) try { return std::stof(j.get<std::string>()); } catch (...) {}
    return def;
}

void loadConfig(const std::string& path) {
    std::ifstream in(path);
    if (!in) return;
    try {
        auto j = nlohmann::json::parse(in, nullptr, false);
        if (!j.is_object()) return;
        auto& c = gCfg;
        if (j.contains("jumpCircleEnabled")) c.jumpCircleEnabled = asBool(j["jumpCircleEnabled"], true);
        if (j.contains("jumpDuration")) c.jumpDuration = asFloat(j["jumpDuration"], 1.2f);
        if (j.contains("jumpSize")) c.jumpSize = asFloat(j["jumpSize"], 1.f);
        if (j.contains("jumpColorR")) c.jumpColorR = asInt(j["jumpColorR"], 255);
        if (j.contains("jumpColorG")) c.jumpColorG = asInt(j["jumpColorG"], 140);
        if (j.contains("jumpColorB")) c.jumpColorB = asInt(j["jumpColorB"], 0);
        if (j.contains("damageCircleEnabled")) c.damageCircleEnabled = asBool(j["damageCircleEnabled"], true);
        if (j.contains("damageDuration")) c.damageDuration = asFloat(j["damageDuration"], 0.9f);
        if (j.contains("damageSize")) c.damageSize = asFloat(j["damageSize"], 1.f);
        if (j.contains("damageColorR")) c.damageColorR = asInt(j["damageColorR"], 255);
        if (j.contains("damageColorG")) c.damageColorG = asInt(j["damageColorG"], 60);
        if (j.contains("damageColorB")) c.damageColorB = asInt(j["damageColorB"], 60);
        if (j.contains("hitboxEnabled")) c.hitboxEnabled = asBool(j["hitboxEnabled"], true);
        if (j.contains("hitboxLockMs")) c.hitboxLockMs = asFloat(j["hitboxLockMs"], 3000.f);
        if (j.contains("crosshairEnabled")) c.crosshairEnabled = asBool(j["crosshairEnabled"], true);
        if (j.contains("crosshairStyle")) c.crosshairStyle = asInt(j["crosshairStyle"], 0);
        if (j.contains("crosshairSize")) c.crosshairSize = asFloat(j["crosshairSize"], 10.f);
        if (j.contains("crosshairRgb")) c.crosshairRgb = asBool(j["crosshairRgb"], false);
        if (j.contains("enableNativeHooks")) c.enableNativeHooks = asBool(j["enableNativeHooks"], true);
    } catch (...) {}
}

void applyAll() {
    bedrockvisuals::modules::applyConfig(gCfg);
}

pl::modmenu::ModuleInfo makeToggleModule(
    const char* id, const char* name, const char* desc,
    bool defaultOn,
    std::vector<pl::modmenu::ConfigEntry> configs,
    std::function<void(bool)> onToggle)
{
    pl::modmenu::ModuleInfo m;
    m.moduleId = id;
    m.displayName = name;
    m.description = desc;
    m.defaultEnabled = defaultOn;
    m.configs = std::move(configs);
    m.onToggle = [onToggle](std::string_view, bool enabled) {
        if (onToggle) onToggle(enabled);
    };
    m.onConfigChanged = [](std::string_view, std::string_view key, std::string_view value) {
        // simple parsers
        auto asB = [](std::string_view v) {
            return v == "true" || v == "1" || v == "yes";
        };
        auto asF = [](std::string_view v) {
            try { return std::stof(std::string(v)); } catch (...) { return 0.f; }
        };
        auto asI = [](std::string_view v) {
            try { return std::stoi(std::string(v)); } catch (...) { return 0; }
        };
        if (key == "jumpDuration") gCfg.jumpDuration = asF(value);
        else if (key == "jumpSize") gCfg.jumpSize = asF(value);
        else if (key == "damageDuration") gCfg.damageDuration = asF(value);
        else if (key == "damageSize") gCfg.damageSize = asF(value);
        else if (key == "hitboxLockMs") gCfg.hitboxLockMs = asF(value);
        else if (key == "crosshairStyle") gCfg.crosshairStyle = asI(value);
        else if (key == "crosshairSize") gCfg.crosshairSize = asF(value);
        else if (key == "crosshairRgb") gCfg.crosshairRgb = asB(value);
        applyAll();
    };
    return m;
}

void registerMenu() {
    using pl::modmenu::ConfigEntry;
    using pl::modmenu::ConfigType;

    {
        std::vector<ConfigEntry> cfgs = {
            {"jumpDuration", "Duration (s)", ConfigType::SliderFloat, std::to_string(gCfg.jumpDuration), "0.3", "3.0"},
            {"jumpSize", "Size", ConfigType::SliderFloat, std::to_string(gCfg.jumpSize), "0.4", "2.5"},
        };
        auto m = makeToggleModule("jump_circle", "Jump Circle",
            "Expanding glow ring when you jump or land",
            gCfg.jumpCircleEnabled, std::move(cfgs),
            [](bool on) { gCfg.jumpCircleEnabled = on; applyAll(); });
        pl::modmenu::registerModule(m);
    }
    {
        std::vector<ConfigEntry> cfgs = {
            {"damageDuration", "Duration (s)", ConfigType::SliderFloat, std::to_string(gCfg.damageDuration), "0.2", "2.5"},
            {"damageSize", "Size", ConfigType::SliderFloat, std::to_string(gCfg.damageSize), "0.4", "2.5"},
        };
        auto m = makeToggleModule("damage_circle", "Damage Circle",
            "Ring on entities you hit",
            gCfg.damageCircleEnabled, std::move(cfgs),
            [](bool on) { gCfg.damageCircleEnabled = on; applyAll(); });
        pl::modmenu::registerModule(m);
    }
    {
        std::vector<ConfigEntry> cfgs = {
            {"hitboxLockMs", "Lock (ms)", ConfigType::SliderFloat, std::to_string(gCfg.hitboxLockMs), "500", "10000"},
        };
        auto m = makeToggleModule("hitbox", "Hitbox",
            "Bracket box on last hit entity",
            gCfg.hitboxEnabled, std::move(cfgs),
            [](bool on) { gCfg.hitboxEnabled = on; applyAll(); });
        pl::modmenu::registerModule(m);
    }
    {
        std::vector<ConfigEntry> cfgs = {
            {"crosshairStyle", "Style (0-4)", ConfigType::SliderInt, std::to_string(gCfg.crosshairStyle), "0", "4"},
            {"crosshairSize", "Size", ConfigType::SliderFloat, std::to_string(gCfg.crosshairSize), "4", "24"},
            {"crosshairRgb", "RGB Cycle", ConfigType::Toggle, gCfg.crosshairRgb ? "true" : "false"},
        };
        auto m = makeToggleModule("crosshair", "Crosshair",
            "Custom crosshair: Cross / Circle / CircleDot / Brackets / Dot",
            gCfg.crosshairEnabled, std::move(cfgs),
            [](bool on) { gCfg.crosshairEnabled = on; applyAll(); });
        pl::modmenu::registerModule(m);
    }
}

class BedrockVisualsMod {
public:
    bool load(pl::mod::ModContext& ctx) {
        auto dir = ctx.configDir().string();
        bedrockvisuals::diag::init(dir);
        bedrockvisuals::diag::stage("load", "diag file logger ready");
        loadConfig(dir + "/config.json");
        applyAll();
        bedrockvisuals::diag::stage("load", "config applied");
        bedrockvisuals::diag::info("v1.0.1 BTP-style modules loaded");
        bedrockvisuals::diag::info("load complete");
        return true;
    }

    bool enable(pl::mod::ModContext&) {
        if (gCfg.enableNativeHooks) {
            bedrockvisuals::hooks::install();
        }
        applyAll();
        registerMenu();
        bedrockvisuals::diag::stage("enable", "modules registered");
        bedrockvisuals::diag::dumpStatus();
        return true;
    }

    bool disable(pl::mod::ModContext&) {
        bedrockvisuals::hooks::uninstall();
        bedrockvisuals::render::uninstall();
        return true;
    }
};

} // namespace

PL_REGISTER_MOD(BedrockVisualsMod, BedrockVisualsMod{})
