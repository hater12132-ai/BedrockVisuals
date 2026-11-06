#pragma once
#include <string>

namespace bedrockvisuals {

struct ModConfig {
    // Jump Circle — expanding glow on jump / land
    bool jumpCircleEnabled = true;
    float jumpDuration = 1.2f;   // seconds
    float jumpSize = 1.0f;
    int jumpColorR = 255, jumpColorG = 140, jumpColorB = 0, jumpColorA = 220;
    bool jumpOnLand = true;

    // Damage Circle — ring when YOU hit something (and optional self)
    bool damageCircleEnabled = true;
    float damageDuration = 0.9f;
    float damageSize = 1.0f;
    int damageColorR = 255, damageColorG = 60, damageColorB = 60, damageColorA = 230;
    bool damageOnTargets = true;

    // Hitbox — simple screen-space box on last hit entity
    bool hitboxEnabled = true;
    int hitboxColorR = 255, hitboxColorG = 255, hitboxColorB = 255, hitboxColorA = 200;
    float hitboxThickness = 2.f;
    float hitboxLockMs = 3000.f;

    // Crosshair — custom center reticle
    bool crosshairEnabled = true;
    int crosshairStyle = 0; // 0 cross, 1 circle, 2 circle-dot, 3 brackets, 4 dot
    float crosshairSize = 10.f;
    float crosshairThickness = 2.f;
    int crosshairColorR = 255, crosshairColorG = 255, crosshairColorB = 255, crosshairColorA = 220;
    bool crosshairRgb = false;

    // Global
    bool enableNativeHooks = true;
};

} // namespace
