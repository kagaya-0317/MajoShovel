#include "engine/Time.hpp"

#include <SDL3/SDL.h>
#include <algorithm>

namespace majo {

void Time::reset()
{
    lastCounter_ = static_cast<double>(SDL_GetPerformanceCounter());
    deltaSeconds_ = 1.0f / 60.0f;
    totalSeconds_ = 0.0f;
}

void Time::tick()
{
    const double now = static_cast<double>(SDL_GetPerformanceCounter());
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    deltaSeconds_ = static_cast<float>((now - lastCounter_) / freq);
    lastCounter_ = now;
    deltaSeconds_ = std::clamp(deltaSeconds_, 0.0f, 1.0f / 20.0f);
    totalSeconds_ += deltaSeconds_;
    if (deltaSeconds_ > 0.0f) {
        fps_ = 0.9f * fps_ + 0.1f * (1.0f / deltaSeconds_);
    }
}

}
