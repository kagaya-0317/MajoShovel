#include "engine/Log.hpp"

#include "engine/CrashReporter.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace majo {

namespace {

std::mutex SinkMutex;
LogSink Sink;
std::mutex FileLogMutex;
bool FileLoggingEnabled = true;
bool FileLogInitialized = false;
std::ofstream FileLog;

#ifdef _WIN32
std::wstring utf8ToWide(std::string_view text)
{
    if (text.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}
#endif

void emitToSink(LogLevel level, std::string_view message)
{
    LogSink sink;
    {
        std::lock_guard<std::mutex> lock(SinkMutex);
        sink = Sink;
    }
    if (sink) {
        sink(level, message);
    }
}

const char* logLevelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Info:
        return "info";
    case LogLevel::Warning:
        return "warn";
    case LogLevel::Error:
        return "error";
    case LogLevel::Command:
        return "cmd";
    }
    return "log";
}

std::string currentLogTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif
    std::ostringstream out;
    out << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

void ensureFileLogOpen()
{
    if (FileLogInitialized || !FileLoggingEnabled) {
        return;
    }
    FileLogInitialized = true;

    const std::filesystem::path path = latestLogPath();
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    FileLog.open(path, std::ios::binary | std::ios::trunc);
    if (FileLog) {
        FileLog << "\xEF\xBB\xBF";
        FileLog << currentLogTimestamp() << " [info] log file started: " << path.string() << "\n";
        FileLog.flush();
    }
}

void emitToFile(LogLevel level, std::string_view message)
{
    std::lock_guard<std::mutex> lock(FileLogMutex);
    ensureFileLogOpen();
    if (!FileLoggingEnabled || !FileLog) {
        return;
    }
    FileLog << currentLogTimestamp() << " [" << logLevelName(level) << "] ";
    FileLog.write(message.data(), static_cast<std::streamsize>(message.size()));
    FileLog << "\n";
    FileLog.flush();
}

}

void setLogSink(LogSink sink)
{
    std::lock_guard<std::mutex> lock(SinkMutex);
    Sink = std::move(sink);
}

void setFileLoggingEnabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(FileLogMutex);
    FileLoggingEnabled = enabled;
    if (!enabled && FileLog.is_open()) {
        FileLog.close();
    }
    if (enabled) {
        FileLogInitialized = false;
    }
}

std::filesystem::path currentLogFilePath()
{
    return latestLogPath();
}

void logMessage(LogLevel level, std::string_view message)
{
    emitToSink(level, message);
    emitToFile(level, message);

#ifdef _WIN32
    HANDLE handle = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (handle != INVALID_HANDLE_VALUE && handle != nullptr && GetConsoleMode(handle, &mode)) {
        std::wstring wide = utf8ToWide(message);
        wide.push_back(L'\n');
        DWORD written = 0;
        WriteConsoleW(handle, wide.data(), static_cast<DWORD>(wide.size()), &written, nullptr);
        return;
    }
#endif

    std::fwrite(message.data(), 1, message.size(), stderr);
    std::fputc('\n', stderr);
}

void logInfo(std::string_view message)
{
    logMessage(LogLevel::Info, message);
}

void logWarning(std::string_view message)
{
    logMessage(LogLevel::Warning, message);
}

void logError(std::string_view message)
{
    logMessage(LogLevel::Error, message);
}

}
