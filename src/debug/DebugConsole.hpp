#pragma once

#include "engine/Log.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace majo {

class DebugConsole {
public:
    DebugConsole();
    ~DebugConsole();

    DebugConsole(const DebugConsole&) = delete;
    DebugConsole& operator=(const DebugConsole&) = delete;

    void initialize();
    void shutdown();
    void toggleVisible();
    void appendLog(LogLevel level, std::string_view message);
    std::optional<std::string> pollCommand();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
