#include "customskies/Diag.hpp"

#include <android/log.h>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>

#define TAG "CustomSkies"

namespace customskies::diag {
namespace {

std::mutex gMu;
std::string gLastError;
std::string gLogPath;
std::string gConfigDir;
bool gReady = false;
bool gFrameHookActive = false;

std::string timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    char buf[64];
    std::tm tm{};
    localtime_r(&t, &tm);
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
                  tm.tm_hour, tm.tm_min, tm.tm_sec, (int)ms.count());
    return buf;
}

void writeLine(const char* level, std::string_view msg) {
    int prio = ANDROID_LOG_INFO;
    if (level[0] == 'W') prio = ANDROID_LOG_WARN;
    if (level[0] == 'E') prio = ANDROID_LOG_ERROR;
    __android_log_print(prio, TAG, "%.*s", (int)msg.size(), msg.data());

    std::lock_guard lock(gMu);
    if (!gLogPath.empty()) {
        std::ofstream f(gLogPath, std::ios::app);
        if (f) {
            f << timestamp() << " [" << level << "] " << msg << '\n';
            f.flush();
        }
    }
}

} // namespace

void init(std::string_view configDir) {
    std::lock_guard lock(gMu);
    gConfigDir.assign(configDir.begin(), configDir.end());
    gLogPath = gConfigDir + "/customskies.log";
    gReady = true;

    {
        std::ofstream f(gLogPath, std::ios::trunc);
        if (f) {
            f << "==== CustomSkies log ====\n";
            f << "configDir=" << gConfigDir << "\n";
            f << "If logcat shows nothing, open THIS file.\n";
            f << "=========================\n";
        }
    }

    info("diag init ok");
}

void info(std::string_view msg)  { writeLine("I", msg); }
void warn(std::string_view msg)  { writeLine("W", msg); }
void error(std::string_view msg) { writeLine("E", msg); }

void stage(std::string_view name, std::string_view detail) {
    if (detail.empty())
        info(std::string("[stage] ") + std::string(name));
    else
        info(std::string("[stage] ") + std::string(name) + " — " + std::string(detail));
}

void setLastError(std::string_view err) {
    std::lock_guard lock(gMu);
    gLastError.assign(err.begin(), err.end());
    error(std::string("LAST_ERROR: ") + gLastError);
}

std::string lastError() {
    std::lock_guard lock(gMu);
    return gLastError;
}

std::string logPath() {
    std::lock_guard lock(gMu);
    return gLogPath;
}

void setFrameHookStatus(bool active) {
    std::lock_guard lock(gMu);
    gFrameHookActive = active;
}

void dumpStatus() {
    info("======== CustomSkies status ========");
    info(std::string("  logPath     = ") + logPath());
    info(std::string("  lastError   = ") + (lastError().empty() ? "(none)" : lastError()));
    info(std::string("  frameHook   = ") + (gFrameHookActive ? "active" : "inactive"));
    info("  modules     = CustomSkies");
    info("====================================");
}

} // namespace customskies::diag
