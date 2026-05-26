#include "engine/Time.hpp"

#include <SDL3/SDL.h>
#include <algorithm>

namespace majo {

namespace {

constexpr float MaxDeltaSeconds = 0.25f;
constexpr float FpsSmoothing = 0.90f;

}

void Time::reset()
{
    lastCounter_ = static_cast<double>(SDL_GetPerformanceCounter());
    deltaSeconds_ = 1.0f / 60.0f;
    totalSeconds_ = 0.0f;
    fps_ = 60.0f;
}

void Time::tick()
{
    const double now = static_cast<double>(SDL_GetPerformanceCounter());
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    const float rawDeltaSeconds = static_cast<float>((now - lastCounter_) / freq);
    lastCounter_ = now;
    const float nonNegativeDeltaSeconds = std::max(0.0f, rawDeltaSeconds);
    deltaSeconds_ = std::min(nonNegativeDeltaSeconds, MaxDeltaSeconds);
    totalSeconds_ += deltaSeconds_;
    if (nonNegativeDeltaSeconds > 0.0f) {
        fps_ = FpsSmoothing * fps_ + (1.0f - FpsSmoothing) * (1.0f / nonNegativeDeltaSeconds);
    }
}

void Time::advanceSimulation(float dt)
{
    deltaSeconds_ = std::min(std::max(0.0f, dt), MaxDeltaSeconds);
    totalSeconds_ += deltaSeconds_;
}

}
