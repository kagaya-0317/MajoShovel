#include "engine/FrameProfiler.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace majo {

namespace {

struct FrameProfileLogSample {
    const char* name = "";
    double totalMilliseconds = 0.0;
    double maxMilliseconds = 0.0;
    int calls = 0;
};

struct FrameProfileLogState {
    std::array<FrameProfileLogSample, FrameProfileSnapshot::MaxSamples + 1> samples{};
    std::size_t count = 0;
    int frames = 0;
    int reportIndex = 0;
    int reportFrameCount = 180;
    std::filesystem::path path;
    bool initialized = false;
    bool enabled = false;
};

std::uint64_t performanceCounter()
{
    return static_cast<std::uint64_t>(SDL_GetPerformanceCounter());
}

double elapsedMilliseconds(std::uint64_t startCounter, std::uint64_t endCounter)
{
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
    if (frequency <= 0.0 || endCounter < startCounter) {
        return 0.0;
    }
    return (static_cast<double>(endCounter - startCounter) * 1000.0) / frequency;
}

int parsePositiveInt(const char* value, int fallback)
{
    if (value == nullptr || value[0] == '\0') {
        return fallback;
    }
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (end == value || parsed <= 0 || parsed > 1000000) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

FrameProfileLogState& profileLogState()
{
    static FrameProfileLogState state;
    if (state.initialized) {
        return state;
    }

    state.initialized = true;
    const char* path = std::getenv("MAJOSHOVEL_PROFILE_LOG");
    if (path == nullptr || path[0] == '\0') {
        return state;
    }

    state.path = std::filesystem::path(path);
    state.enabled = true;
    state.reportFrameCount = parsePositiveInt(std::getenv("MAJOSHOVEL_PROFILE_REPORT_FRAMES"), state.reportFrameCount);

    std::error_code ec;
    if (state.path.has_parent_path()) {
        std::filesystem::create_directories(state.path.parent_path(), ec);
    }
    std::ofstream file(state.path, std::ios::trunc);
    file << "report\tframes\tsample\tavg_ms\tmax_ms\tcalls_per_frame\n";
    return state;
}

void resetLogInterval(FrameProfileLogState& state)
{
    state.samples = {};
    state.count = 0;
    state.frames = 0;
}

void addLogSample(FrameProfileLogState& state, const char* name, double milliseconds, int calls)
{
    if (name == nullptr || name[0] == '\0') {
        return;
    }
    for (std::size_t i = 0; i < state.count; ++i) {
        FrameProfileLogSample& sample = state.samples[i];
        if (std::strcmp(sample.name, name) == 0) {
            sample.totalMilliseconds += milliseconds;
            sample.maxMilliseconds = std::max(sample.maxMilliseconds, milliseconds);
            sample.calls += calls;
            return;
        }
    }
    if (state.count >= state.samples.size()) {
        return;
    }
    state.samples[state.count++] = FrameProfileLogSample{
        name,
        milliseconds,
        milliseconds,
        calls,
    };
}

void writeLogInterval(FrameProfileLogState& state)
{
    if (!state.enabled || state.frames <= 0) {
        resetLogInterval(state);
        return;
    }

    std::ofstream file(state.path, std::ios::app);
    if (!file) {
        resetLogInterval(state);
        return;
    }

    ++state.reportIndex;
    for (std::size_t i = 0; i < state.count; ++i) {
        const FrameProfileLogSample& sample = state.samples[i];
        file
            << state.reportIndex << '\t'
            << state.frames << '\t'
            << sample.name << '\t'
            << (sample.totalMilliseconds / static_cast<double>(state.frames)) << '\t'
            << sample.maxMilliseconds << '\t'
            << (static_cast<double>(sample.calls) / static_cast<double>(state.frames)) << '\n';
    }
    resetLogInterval(state);
}

void recordProfileLog(const FrameProfileSnapshot& snapshot)
{
    FrameProfileLogState& state = profileLogState();
    if (!state.enabled) {
        return;
    }

    ++state.frames;
    addLogSample(state, "Frame.total", snapshot.frameMilliseconds, 1);
    for (std::size_t i = 0; i < snapshot.count; ++i) {
        const FrameProfileSample& sample = snapshot.samples[i];
        addLogSample(state, sample.name, sample.milliseconds, sample.calls);
    }
    if (state.frames >= state.reportFrameCount) {
        writeLogInterval(state);
    }
}

} // namespace

void FrameProfiler::beginFrame()
{
    current_ = {};
    frameStartCounter_ = performanceCounter();
}

void FrameProfiler::endFrame()
{
    current_.frameMilliseconds = elapsedMilliseconds(frameStartCounter_, performanceCounter());
    snapshot_ = current_;
    recordProfileLog(snapshot_);
}

void FrameProfiler::addSample(const char* name, double milliseconds)
{
    if (name == nullptr || name[0] == '\0') {
        return;
    }
    for (std::size_t i = 0; i < current_.count; ++i) {
        FrameProfileSample& sample = current_.samples[i];
        if (std::strcmp(sample.name, name) == 0) {
            sample.milliseconds += milliseconds;
            ++sample.calls;
            return;
        }
    }
    if (current_.count >= FrameProfileSnapshot::MaxSamples) {
        return;
    }
    current_.samples[current_.count++] = FrameProfileSample{
        name,
        milliseconds,
        1,
    };
}

FrameProfileScope::FrameProfileScope(const char* name)
    : name_(name)
    , startCounter_(performanceCounter())
{
}

FrameProfileScope::~FrameProfileScope()
{
    frameProfiler().addSample(name_, elapsedMilliseconds(startCounter_, performanceCounter()));
}

FrameProfileFrame::FrameProfileFrame()
{
    frameProfiler().beginFrame();
}

FrameProfileFrame::~FrameProfileFrame()
{
    frameProfiler().endFrame();
}

FrameProfiler& frameProfiler()
{
    static FrameProfiler profiler;
    return profiler;
}

} // namespace majo
