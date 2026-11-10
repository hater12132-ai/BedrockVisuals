#pragma once

#include <string>
#include <string_view>

namespace customskies {

class Module {
public:
    Module(std::string name, std::string description)
        : mName(std::move(name)), mDescription(std::move(description)) {}

    virtual ~Module() = default;

    const std::string& name() const { return mName; }
    const std::string& description() const { return mDescription; }

    bool isEnabled() const { return mEnabled; }

    virtual void onInit() {}
    virtual void onEnable() { mEnabled = true; }
    virtual void onDisable() { mEnabled = false; }
    virtual void onFrame() {}

protected:
    std::string mName;
    std::string mDescription;
    bool mEnabled = false;
};

} // namespace customskies
