#pragma once

#include "game/ActorVisual.hpp"
#include "game/SpellRingItem.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

constexpr float RingItemBaseAltitude = 8.0f;
constexpr float RingItemBobAmplitude = 3.0f;
constexpr float RingItemBobSpeed = 4.2f;
constexpr float RingItemRotationWobbleDegrees = 4.0f;

[[nodiscard]] inline float ringItemBobPhase(const SpellRingItem& item, float totalSeconds)
{
    return totalSeconds * RingItemBobSpeed + item.localAngle * 1.7f;
}

[[nodiscard]] inline float ringItemAltitude(const SpellRingItem& item, float totalSeconds)
{
    return RingItemBaseAltitude + std::sin(ringItemBobPhase(item, totalSeconds)) * RingItemBobAmplitude;
}

[[nodiscard]] inline float ringItemRotationWobbleDegrees(const SpellRingItem& item, float totalSeconds)
{
    return std::cos(ringItemBobPhase(item, totalSeconds)) * RingItemRotationWobbleDegrees;
}

[[nodiscard]] inline Vec2 ringItemActionShakeOffset(const SpellRingItem& item, float totalSeconds)
{
    if (item.actionFlashTimer <= 0.0f) {
        return {};
    }

    const float t = std::clamp(item.actionFlashTimer / SpellRingItemActionFlashSeconds, 0.0f, 1.0f);
    const float eased = t * t * (3.0f - 2.0f * t);
    const float amplitude = 3.6f * eased;
    const float phase = item.localAngle * 17.0f + static_cast<float>(item.ringIndex) * 5.0f;
    constexpr float ShakeSpeed = 86.0f;
    return {
        std::sin(totalSeconds * ShakeSpeed + phase) * amplitude,
        std::cos(totalSeconds * ShakeSpeed * 1.27f + phase) * amplitude,
    };
}

[[nodiscard]] inline Vec2 ringItemDrawPosition(const SpellRingItem& item, float totalSeconds)
{
    return elevatedDrawPosition(item.worldPosition, ringItemAltitude(item, totalSeconds)) +
        ringItemActionShakeOffset(item, totalSeconds);
}

}
