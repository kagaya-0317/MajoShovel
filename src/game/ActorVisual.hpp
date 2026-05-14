#pragma once

#include "engine/Math.hpp"

#include <algorithm>

namespace majo {

struct ActorAltitudeShadowTuning {
    float scaleDistance = 128.0f;
    float minScale = 0.55f;
};

[[nodiscard]] inline Vec2 elevatedDrawPosition(Vec2 groundPosition, float altitude)
{
    groundPosition.y -= std::max(0.0f, altitude);
    return groundPosition;
}

[[nodiscard]] inline float actorShadowScaleForAltitude(
    float altitude,
    ActorAltitudeShadowTuning tuning = {})
{
    const float scaleDistance = std::max(0.001f, tuning.scaleDistance);
    const float minScale = std::clamp(tuning.minScale, 0.0f, 1.0f);
    return std::clamp(1.0f - std::max(0.0f, altitude) / scaleDistance, minScale, 1.0f);
}

[[nodiscard]] inline float actorShadowVisualSizeForAltitude(
    float baseVisualSize,
    float altitude,
    ActorAltitudeShadowTuning tuning = {})
{
    return std::max(1.0f, baseVisualSize) * actorShadowScaleForAltitude(altitude, tuning);
}

}
