#pragma once

#include "engine/Math.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

struct ExplosionWarningVisual {
    bool active = false;
    float elapsed = 0.0f;
    float progress = 0.0f;
    float urgency = 0.0f;
    float pulse = 0.0f;
    float intensity = 0.0f;
};

inline float explosionWarningEase(float t)
{
    t = clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float explosionWarningPhaseCycles(float initialDelay, float progress)
{
    constexpr float StartCyclesPerSecond = 1.45f;
    constexpr float EndCyclesPerSecond = 10.60f;

    progress = clamp(progress, 0.0f, 1.0f);
    const float progressSq = progress * progress;
    return std::max(0.001f, initialDelay) * (
        StartCyclesPerSecond * progress +
        (EndCyclesPerSecond - StartCyclesPerSecond) * progressSq * progress / 3.0f);
}

inline int explosionWarningTickIndex(float initialDelay, float remainingDelay)
{
    if (initialDelay <= 0.0f) {
        return -1;
    }

    const float initial = std::max(0.001f, initialDelay);
    const float remaining = clamp(remainingDelay, 0.0f, initial);
    const float progress = clamp((initial - remaining) / initial, 0.0f, 1.0f);
    const float phaseCycles = explosionWarningPhaseCycles(initial, progress);
    if (phaseCycles < 0.5f) {
        return -1;
    }
    return static_cast<int>(std::floor(phaseCycles - 0.5f));
}

inline ExplosionWarningVisual explosionWarningVisual(float initialDelay, float remainingDelay)
{
    ExplosionWarningVisual visual;
    if (initialDelay <= 0.0f) {
        return visual;
    }

    const float initial = std::max(0.001f, initialDelay);
    const float remaining = clamp(remainingDelay, 0.0f, initial);
    visual.active = true;
    visual.elapsed = std::max(0.0f, initial - remaining);
    visual.progress = clamp(visual.elapsed / initial, 0.0f, 1.0f);
    visual.urgency = explosionWarningEase(visual.progress);

    const float phaseCycles = explosionWarningPhaseCycles(initial, visual.progress);
    const float phase = phaseCycles * Pi * 2.0f;
    visual.pulse = 0.5f - 0.5f * std::cos(phase);
    visual.intensity = clamp(
        0.20f + visual.urgency * 0.30f + visual.pulse * (0.40f + visual.urgency * 0.38f),
        0.0f,
        1.0f);
    return visual;
}

}
