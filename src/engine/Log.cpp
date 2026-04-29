#include "engine/Log.hpp"

#include <cstdio>
#include <string>

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

}

void logError(std::string_view message)
{
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

}
