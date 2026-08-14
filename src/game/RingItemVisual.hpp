#pragma once

#include "game/ActorVisual.hpp"
#include "game/ActorVisualMotion.hpp"
#include "game/SpellRingItem.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

constexpr float RingItemBaseAltitude = 8.0f;
constexpr float RingItemBobAmplitude = 3.0f;
constexpr float RingItemBobSpeed = 4.2f;
constexpr float RingItemRotationWobbleDegrees = 4.0f;
constexpr float RingObjectImageMaxSize = 48.0f;
constexpr float RingItemHitboxReferenceImageSize = 96.0f;

[[nodiscard]] inline float ringItemWorldToHitboxReferenceScale()
{
    return RingObjectImageMaxSize / RingItemHitboxReferenceImageSize;
}

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

[[nodiscard]] inline Vec2 ringItemDrawPositionAt(
    const SpellRingItem& item,
    Vec2 worldPosition,
    float totalSeconds)
{
    return elevatedDrawPosition(worldPosition, ringItemAltitude(item, totalSeconds)) +
        ringItemActionShakeOffset(item, totalSeconds);
}

[[nodiscard]] inline Vec2 ringItemDrawPosition(const SpellRingItem& item, float totalSeconds)
{
    return ringItemDrawPositionAt(item, item.worldPosition, totalSeconds);
}

[[nodiscard]] inline Vec2 ringItemOutwardDirection(const SpellRingItem& item, Vec2 outward)
{
    if (lengthSquared(outward) > 0.0001f) {
        return normalize(outward);
    }
    return fromAngle(item.localAngle);
}

[[nodiscard]] inline ActorVisualPose capturedProjectileActionPoseAt(
    const SpellRingItem& item,
    float elapsedSeconds)
{
    if (item.capturedProjectileActionAnimationId.empty() ||
        item.capturedProjectileActionDurationSeconds <= 0.0f) {
        return actorVisualPoseIdentity();
    }
    return sampleActorVisualMotion(
        item.capturedProjectileActionAnimationId,
        std::max(0.0f, elapsedSeconds));
}

[[nodiscard]] inline ActorVisualPose capturedProjectileActionPose(const SpellRingItem& item)
{
    return capturedProjectileActionPoseAt(
        item,
        item.capturedProjectileActionElapsedSeconds);
}

[[nodiscard]] inline Vec2 ringItemActionDrawPositionAt(
    const SpellRingItem& item,
    Vec2 center,
    Vec2 outward,
    float actionElapsedSeconds)
{
    const ActorVisualPose pose = capturedProjectileActionPoseAt(item, actionElapsedSeconds);
    const Vec2 forward = ringItemOutwardDirection(item, outward);
    return center + pose.offset + forward * pose.forwardOffset + Vec2{0.0f, -pose.visualAltitude};
}

[[nodiscard]] inline Vec2 ringItemActionDrawPosition(
    const SpellRingItem& item,
    Vec2 center,
    Vec2 outward)
{
    return ringItemActionDrawPositionAt(
        item,
        center,
        outward,
        item.capturedProjectileActionElapsedSeconds);
}

[[nodiscard]] inline Vec2 ringItemVisualCenterAt(
    const SpellRingItem& item,
    Vec2 worldPosition,
    Vec2 outward,
    float totalSeconds,
    float actionElapsedSeconds)
{
    return ringItemActionDrawPositionAt(
        item,
        ringItemDrawPositionAt(item, worldPosition, totalSeconds),
        outward,
        actionElapsedSeconds);
}

[[nodiscard]] inline Vec2 ringItemVisualCenterAt(
    const SpellRingItem& item,
    Vec2 worldPosition,
    Vec2 outward,
    float totalSeconds)
{
    return ringItemVisualCenterAt(
        item,
        worldPosition,
        outward,
        totalSeconds,
        item.capturedProjectileActionElapsedSeconds);
}

[[nodiscard]] inline Vec2 ringItemVisualCenter(const SpellRingItem& item, float totalSeconds)
{
    return ringItemVisualCenterAt(
        item,
        item.worldPosition,
        item.orbitOutward,
        totalSeconds);
}

[[nodiscard]] inline float ringItemActionUniformScale(const SpellRingItem& item)
{
    const Vec2 scale = capturedProjectileActionPose(item).scale;
    const float x = std::isfinite(scale.x) ? std::abs(scale.x) : 1.0f;
    const float y = std::isfinite(scale.y) ? std::abs(scale.y) : 1.0f;
    return std::max({0.05f, x, y});
}

}
