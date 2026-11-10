#pragma once

#include <string>

namespace customskies {

struct ModConfig {
    int version = 1;
    bool enabled = true;
    float opacity = 1.0f;
    bool loop = true;
    std::string video = "sky.mp4";
};

} // namespace customskies
