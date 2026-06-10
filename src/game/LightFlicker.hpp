#pragma once

#include "engine/Math.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

[[nodiscard]] inline float flickeredLightRadius(float radius, float totalSeconds, float phase)
{
    if (radius <= 0.0f) {
        return radius;
    }
    const float waveA = std::sin(totalSeconds * 13.4f + phase);
    const float waveB = std::sin(totalSeconds * 34.7f + phase * 2.3f + 0.6f);
    const float offsetPx = std::clamp(waveA * 1.35f + waveB * 0.95f, -2.0f, 2.0f);
    return std::max(0.0f, radius + offsetPx);
}

[[nodiscard]] inline Vec2 flickeredLightPosition(Vec2 position, float totalSeconds, float phase)
{
    const float x =
        std::sin(totalSeconds * 7.1f + phase * 1.37f) * 1.15f +
        std::sin(totalSeconds * 17.3f + phase * 2.11f + 1.2f) * 0.55f;
    const float y =
        std::sin(totalSeconds * 6.4f + phase * 1.83f + 2.0f) * 1.10f +
        std::sin(totalSeconds * 19.1f + phase * 2.47f + 0.4f) * 0.60f;
    return position + Vec2{
        std::clamp(x, -1.7f, 1.7f),
        std::clamp(y, -1.7f, 1.7f),
    };
}

}
