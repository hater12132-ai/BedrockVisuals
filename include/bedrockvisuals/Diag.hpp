#pragma once

#include <string>
#include <string_view>

namespace bedrockvisuals::diag {

// Call once from load() with config directory path
void init(std::string_view configDir);

void info(std::string_view msg);
void warn(std::string_view msg);
void error(std::string_view msg);
void stage(std::string_view name, std::string_view detail = {});

void dumpStatus();
void setLastError(std::string_view err);
std::string lastError();

// Absolute path of the log file (empty if not inited)
std::string logPath();

} // namespace
