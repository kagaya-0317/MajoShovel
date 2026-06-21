#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

namespace majo {

struct CrashReportOptions {
    std::string applicationName = "MajoShovel";
    bool writeMiniDump = true;
    bool includeRecentLog = true;
};

using CrashContextProvider = std::function<std::string()>;

void installCrashReporter(CrashReportOptions options = {});
void setCrashPhase(std::string_view phase);
void setCrashContextProvider(CrashContextProvider provider);
std::filesystem::path crashReportDirectory();
std::filesystem::path latestLogPath();

}
