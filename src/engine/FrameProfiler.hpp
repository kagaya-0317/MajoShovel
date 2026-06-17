#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace majo {

struct FrameProfileSample {
    const char* name = "";
    double milliseconds = 0.0;
    int calls = 0;
};

struct FrameProfileSnapshot {
    static constexpr std::size_t MaxSamples = 96;

    std::array<FrameProfileSample, MaxSamples> samples{};
    std::size_t count = 0;
    double frameMilliseconds = 0.0;
};

class FrameProfiler {
public:
    void beginFrame();
    void endFrame();
    void addSample(const char* name, double milliseconds);

    const FrameProfileSnapshot& snapshot() const { return snapshot_; }

private:
    FrameProfileSnapshot current_{};
    FrameProfileSnapshot snapshot_{};
    std::uint64_t frameStartCounter_ = 0;
};

class FrameProfileScope {
public:
    explicit FrameProfileScope(const char* name);
    ~FrameProfileScope();

    FrameProfileScope(const FrameProfileScope&) = delete;
    FrameProfileScope& operator=(const FrameProfileScope&) = delete;

private:
    const char* name_ = "";
    std::uint64_t startCounter_ = 0;
};

class FrameProfileFrame {
public:
    FrameProfileFrame();
    ~FrameProfileFrame();

    FrameProfileFrame(const FrameProfileFrame&) = delete;
    FrameProfileFrame& operator=(const FrameProfileFrame&) = delete;
};

FrameProfiler& frameProfiler();

} // namespace majo
