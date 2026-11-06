#pragma once

namespace bedrockvisuals::hooks {

// Install GameMode attack + ClientInstance update (jump edge detect).
// Returns true if at least the attack hook installed.
bool install();
void uninstall();
bool attackInstalled();
bool jumpInstalled();

} // namespace
