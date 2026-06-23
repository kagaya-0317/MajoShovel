#include "engine/CrashReporter.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <DbgHelp.h>
#endif

namespace majo {

namespace {

std::mutex CrashStateMutex;
CrashReportOptions CrashOptions;
CrashContextProvider ContextProvider;
std::string CurrentPhase = "startup";
std::terminate_handler PreviousTerminateHandler = nullptr;
std::atomic_bool ReportInProgress{false};

#ifdef _WIN32
LPTOP_LEVEL_EXCEPTION_FILTER PreviousUnhandledFilter = nullptr;
#endif

std::filesystem::path localAppDataRoot()
{
#ifdef _WIN32
    char* rawValue = nullptr;
    std::size_t valueSize = 0;
    if (_dupenv_s(&rawValue, &valueSize, "LOCALAPPDATA") == 0 && rawValue != nullptr && rawValue[0] != '\0') {
        std::filesystem::path path(rawValue);
        std::free(rawValue);
        return path / "MajoShovel";
    }
    std::free(rawValue);
#endif
    if (const char* localAppData = std::getenv("LOCALAPPDATA")) {
        if (localAppData[0] != '\0') {
            return std::filesystem::path(localAppData) / "MajoShovel";
        }
    }
    return std::filesystem::path(".local") / "MajoShovel";
}

std::string pathForReport(const std::filesystem::path& path)
{
    return path.string();
}

std::string currentTimestampForFile()
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
    out << std::put_time(&localTime, "%Y%m%d-%H%M%S");
    return out.str();
}

std::string currentTimestampForText()
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

std::string snapshotCurrentPhase()
{
    std::lock_guard<std::mutex> lock(CrashStateMutex);
    return CurrentPhase;
}

CrashReportOptions snapshotOptions()
{
    std::lock_guard<std::mutex> lock(CrashStateMutex);
    return CrashOptions;
}

std::string snapshotContext()
{
    CrashContextProvider provider;
    {
        std::lock_guard<std::mutex> lock(CrashStateMutex);
        provider = ContextProvider;
    }
    if (!provider) {
        return {};
    }
    try {
        return provider();
    } catch (const std::exception& ex) {
        return std::string("crash_context_error=") + ex.what() + "\n";
    } catch (...) {
        return "crash_context_error=unknown\n";
    }
}

std::vector<std::string> readRecentLogLines(std::size_t maxLines)
{
    std::ifstream file(latestLogPath(), std::ios::binary);
    if (!file) {
        return {};
    }

    std::deque<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (lines.size() >= maxLines) {
            lines.pop_front();
        }
        lines.push_back(std::move(line));
    }
    return {lines.begin(), lines.end()};
}

std::string exceptionCodeName(unsigned long code)
{
#ifdef _WIN32
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_BREAKPOINT:
        return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:
        return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_STACK_OVERFLOW:
        return "EXCEPTION_STACK_OVERFLOW";
    default:
        break;
    }
#endif
    return "UNKNOWN_EXCEPTION";
}

std::string formatHex(std::uintptr_t value)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase << value;
    return out.str();
}

#ifdef _WIN32
std::string moduleNameForAddress(DWORD64 address)
{
    const DWORD64 moduleBase = SymGetModuleBase64(GetCurrentProcess(), address);
    if (moduleBase == 0) {
        return {};
    }

    char modulePath[MAX_PATH]{};
    if (GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleBase), modulePath, MAX_PATH) == 0) {
        return {};
    }
    return std::filesystem::path(modulePath).filename().string();
}

std::vector<std::string> captureStackTrace(CONTEXT* sourceContext)
{
    if (sourceContext == nullptr) {
        return {};
    }

    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    const bool initialized = SymInitialize(process, nullptr, TRUE) != FALSE;
    if (!initialized) {
        return {};
    }

    CONTEXT context = *sourceContext;
    STACKFRAME64 frame{};
    DWORD machineType = 0;
#if defined(_M_X64)
    machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrStack.Offset = context.Rsp;
#elif defined(_M_IX86)
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip;
    frame.AddrFrame.Offset = context.Ebp;
    frame.AddrStack.Offset = context.Esp;
#else
    SymCleanup(process);
    return {};
#endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    std::vector<std::string> lines;
    for (int index = 0; index < 64; ++index) {
        if (!StackWalk64(
                machineType,
                process,
                thread,
                &frame,
                &context,
                nullptr,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                nullptr)) {
            break;
        }
        if (frame.AddrPC.Offset == 0) {
            break;
        }

        std::ostringstream line;
        line << "#" << index << " " << formatHex(static_cast<std::uintptr_t>(frame.AddrPC.Offset));
        const std::string moduleName = moduleNameForAddress(frame.AddrPC.Offset);
        if (!moduleName.empty()) {
            line << " " << moduleName;
        }

        alignas(SYMBOL_INFO) unsigned char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) {
            line << "!" << symbol->Name;
            if (displacement != 0) {
                line << "+" << formatHex(static_cast<std::uintptr_t>(displacement));
            }
        }

        IMAGEHLP_LINE64 sourceLine{};
        sourceLine.SizeOfStruct = sizeof(sourceLine);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &sourceLine)) {
            line << " (" << sourceLine.FileName << ":" << sourceLine.LineNumber << ")";
        }
        lines.push_back(line.str());
    }

    SymCleanup(process);
    return lines;
}

bool writeMiniDump(const std::filesystem::path& dumpPath, EXCEPTION_POINTERS* exceptionPointers)
{
    HANDLE file = CreateFileW(
        dumpPath.wstring().c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = exceptionPointers;
    exceptionInfo.ClientPointers = FALSE;
    const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithThreadInfo);
    const BOOL ok = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        file,
        dumpType,
        exceptionPointers != nullptr ? &exceptionInfo : nullptr,
        nullptr,
        nullptr);
    CloseHandle(file);
    return ok != FALSE;
}
#endif

std::filesystem::path writeCrashReport(
    std::string_view reason,
    std::string_view detail,
#ifdef _WIN32
    EXCEPTION_POINTERS* exceptionPointers
#else
    void* exceptionPointers
#endif
)
{
    if (ReportInProgress.exchange(true)) {
        return {};
    }

    const CrashReportOptions options = snapshotOptions();
    const std::filesystem::path directory = crashReportDirectory();
    std::error_code error;
    std::filesystem::create_directories(directory, error);

    const std::string timestamp = currentTimestampForFile();
    const std::filesystem::path reportPath = directory / ("crash-" + timestamp + ".txt");
    const std::filesystem::path dumpPath = directory / ("crash-" + timestamp + ".dmp");

    bool dumpWritten = false;
#ifdef _WIN32
    if (options.writeMiniDump) {
        dumpWritten = writeMiniDump(dumpPath, exceptionPointers);
    }
#else
    (void)exceptionPointers;
#endif

    std::ofstream report(reportPath, std::ios::binary | std::ios::trunc);
    if (report) {
        report << "\xEF\xBB\xBF";
        report << "MajoShovel Crash Report\n";
        report << "time=" << currentTimestampForText() << "\n";
        report << "application=" << options.applicationName << "\n";
        report << "reason=" << reason << "\n";
        if (!detail.empty()) {
            report << "detail=" << detail << "\n";
        }
        report << "phase=" << snapshotCurrentPhase() << "\n";
        report << "report=" << pathForReport(reportPath) << "\n";
        if (options.writeMiniDump) {
            report << "dump=" << pathForReport(dumpPath) << "\n";
            report << "dump_written=" << (dumpWritten ? "true" : "false") << "\n";
        }

#ifdef _WIN32
        if (exceptionPointers != nullptr && exceptionPointers->ExceptionRecord != nullptr) {
            const EXCEPTION_RECORD* record = exceptionPointers->ExceptionRecord;
            report << "exception_code=" << formatHex(record->ExceptionCode) << "\n";
            report << "exception_name=" << exceptionCodeName(record->ExceptionCode) << "\n";
            report << "exception_address=" << formatHex(reinterpret_cast<std::uintptr_t>(record->ExceptionAddress)) << "\n";
            if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
                const bool writing = record->ExceptionInformation[0] != 0;
                report << "access_violation_operation=" << (writing ? "write" : "read") << "\n";
                report << "access_violation_address=" << formatHex(static_cast<std::uintptr_t>(record->ExceptionInformation[1])) << "\n";
            }
        }
#endif

        const std::string context = snapshotContext();
        if (!context.empty()) {
            report << "\n[Context]\n" << context;
            if (context.back() != '\n') {
                report << "\n";
            }
        }

#ifdef _WIN32
        if (exceptionPointers != nullptr) {
            const std::vector<std::string> stack = captureStackTrace(exceptionPointers->ContextRecord);
            if (!stack.empty()) {
                report << "\n[Stack]\n";
                for (const std::string& line : stack) {
                    report << line << "\n";
                }
            }
        }
#endif

        if (options.includeRecentLog) {
            const std::vector<std::string> recentLog = readRecentLogLines(200);
            if (!recentLog.empty()) {
                report << "\n[Recent Log]\n";
                for (const std::string& line : recentLog) {
                    report << line << "\n";
                }
            }
        }
    }

    ReportInProgress = false;
    return reportPath;
}

void terminateHandler()
{
    std::string detail;
    if (std::exception_ptr exception = std::current_exception()) {
        try {
            std::rethrow_exception(exception);
        } catch (const std::exception& ex) {
            detail = ex.what();
        } catch (...) {
            detail = "unknown C++ exception";
        }
    }

    writeCrashReport("std::terminate", detail, nullptr);
    if (PreviousTerminateHandler) {
        PreviousTerminateHandler();
    }
    std::abort();
}

#ifdef _WIN32
LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
{
    writeCrashReport("unhandled SEH exception", {}, exceptionPointers);
    if (PreviousUnhandledFilter) {
        return PreviousUnhandledFilter(exceptionPointers);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

}

void installCrashReporter(CrashReportOptions options)
{
    {
        std::lock_guard<std::mutex> lock(CrashStateMutex);
        CrashOptions = std::move(options);
    }
    PreviousTerminateHandler = std::set_terminate(terminateHandler);
#ifdef _WIN32
    PreviousUnhandledFilter = SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif
}

void setCrashPhase(std::string_view phase)
{
    std::lock_guard<std::mutex> lock(CrashStateMutex);
    CurrentPhase.assign(phase.begin(), phase.end());
}

void setCrashContextProvider(CrashContextProvider provider)
{
    std::lock_guard<std::mutex> lock(CrashStateMutex);
    ContextProvider = std::move(provider);
}

std::filesystem::path crashReportDirectory()
{
    return localAppDataRoot() / "crashes";
}

std::filesystem::path latestLogPath()
{
    return localAppDataRoot() / "logs" / "latest.log";
}

}
