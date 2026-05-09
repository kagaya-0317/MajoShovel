#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace majo {

enum class LogLevel {
    Info,
    Warning,
    Error,
    Command,
};

using LogSink = std::function<void(LogLevel, std::string_view)>;

void setLogSink(LogSink sink);
void logMessage(LogLevel level, std::string_view message);
void logInfo(std::string_view message);
void logWarning(std::string_view message);
void logError(std::string_view message);

}
