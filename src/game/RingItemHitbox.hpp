#pragma once

#include "game/Hitbox.hpp"
#include "game/ObjectVisualPose.hpp"
#include "game/RingItemVisual.hpp"
#include "game/SpellRingSystem.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

// Object hitboxes are authored against the same 96 px preview used by the
// hitbox editor.  This value object resolves that authoring space into the
// item's current and previous world-space visual poses.
struct RingItemHitbox {
    const HitboxProfile* profile = nullptr;
    Vec2 center{};
    Vec2 previousCenter{};
    float rotationRadians = 0.0f;
    float previousRotationRadians = 0.0f;
    float profileScale = 1.0f;
    float profileRadiusPadding = 0.0f;
    float fallbackCircleRadius = 1.0f;
};

[[nodiscard]] inline float ringItemSizeModifier(const SpellRingItem& item)
{
    const float scale = static_cast<float>(item.sizeModifier);
    return std::isfinite(scale) ? std::clamp(scale, 0.05f, 8.0f) : 1.0f;
}

[[nodiscard]] inline float ringItemCollisionRadiusPadding(const SpellRingItem& item)
{
    if (item.hasCapturedBehavior("jump_outward") && item.capturedJumpTimer > 0.0f) {
        return static_cast<float>(
            std::max(2.0, item.capturedBehaviorParamDouble("jump_outward", "landingRadius", 5.0)));
    }
    return 0.0f;
}

[[nodiscard]] inline RingItemHitbox resolveRingItemHitbox(
    const SpellRingItem& item,
    const ObjectDefinition* object,
    const HitboxCatalog* catalog,
    const SpellRingSystem& spellRing,
    float totalTime,
    float dt,
    float radiusPadding = 0.0f)
{
    RingItemHitbox result;
    const float safeDt = std::max(0.0f, dt);
    const bool hasPreviousPose = item.motionHistoryValid;
    const float previousTime = hasPreviousPose ? totalTime - safeDt : totalTime;
    const float previousActionElapsed = hasPreviousPose
        ? std::max(0.0f, item.capturedProjectileActionElapsedSeconds - safeDt)
        : item.capturedProjectileActionElapsedSeconds;
    result.center = ringItemVisualCenterAt(
        item,
        item.worldPosition,
        item.orbitOutward,
        totalTime);
    result.previousCenter = ringItemVisualCenterAt(
        item,
        hasPreviousPose ? item.previousWorldPosition : item.worldPosition,
        hasPreviousPose ? item.previousOrbitOutward : item.orbitOutward,
        previousTime,
        previousActionElapsed);

    const float worldVisualScale = spellRing.worldItemVisualScale(item);
    const float itemScale = ringItemSizeModifier(item);
    const bool objectArtwork = item.objectVisual.source == ItemVisualSource::Object;
    const float fallbackArtworkScale = objectArtwork && object != nullptr
        ? objectImageScaleMultiplier(object->id)
        : 1.0f;
    const float actionScale = item.objectVisual.source == ItemVisualSource::Enemy
        ? ringItemActionUniformScale(item)
        : 1.0f;
    const float safePadding = std::max(0.0f, radiusPadding);
    result.fallbackCircleRadius = std::max(
        0.0f,
        item.hitRadius * itemScale * worldVisualScale * fallbackArtworkScale * actionScale + safePadding);
    result.profileRadiusPadding = safePadding;

    // The editor and runtime both apply the per-object artwork override.  The
    // authored circle coordinates therefore need only the preview/world-size
    // ratio here; applying the artwork override again would double-scale them.
    result.profileScale = itemScale *
        worldVisualScale *
        ringItemWorldToHitboxReferenceScale();
    result.profile = objectHitboxProfileFor(catalog, item.objectId);
    if (result.profile == nullptr || object == nullptr) {
        return result;
    }

    const auto resolveRotationRadians = [&](Vec2 outward, Vec2 velocity, float time, float actionElapsed) {
        ObjectImageDrawOptions baseImageOptions;
        baseImageOptions.rotationDegrees =
            capturedProjectileActionPoseAt(item, actionElapsed).rotationDegrees +
            ringItemRotationWobbleDegrees(item, time);
        const ObjectImageDrawOptions imageOptions = objectRingImageOptions(
            *object,
            outward,
            velocity,
            time,
            baseImageOptions);
        return imageOptions.rotationDegrees * (Pi / 180.0f);
    };
    result.rotationRadians = resolveRotationRadians(
        item.orbitOutward,
        item.worldVelocity,
        totalTime,
        item.capturedProjectileActionElapsedSeconds);
    result.previousRotationRadians = resolveRotationRadians(
        hasPreviousPose ? item.previousOrbitOutward : item.orbitOutward,
        hasPreviousPose ? item.previousWorldVelocity : item.worldVelocity,
        previousTime,
        previousActionElapsed);
    return result;
}

inline void appendRingItemHitboxCirclesAt(
    const RingItemHitbox& hitbox,
    Vec2 center,
    float rotationRadians,
    std::vector<WorldHitCircle>& outCircles)
{
    if (hitbox.profile != nullptr) {
        appendResolvedHitboxCircles(
            *hitbox.profile,
            center,
            rotationRadians,
            hitbox.profileScale,
            hitbox.profileRadiusPadding,
            outCircles);
        return;
    }

    outCircles.push_back({center, hitbox.fallbackCircleRadius});
}

[[nodiscard]] inline float ringItemHitboxShortestAngleDelta(float from, float to)
{
    return std::remainder(to - from, 2.0f * Pi);
}

[[nodiscard]] inline float ringItemHitboxMaximumPointTravel(const RingItemHitbox& hitbox)
{
    const float rotationDelta = ringItemHitboxShortestAngleDelta(
        hitbox.previousRotationRadians,
        hitbox.rotationRadians);
    const float boundsRadius = hitbox.profile != nullptr
        ? hitboxProfileBoundsRadius(
            *hitbox.profile,
            hitbox.profileScale,
            hitbox.profileRadiusPadding)
        : hitbox.fallbackCircleRadius;
    return length(hitbox.center - hitbox.previousCenter) +
        std::abs(rotationDelta) * boundsRadius;
}

[[nodiscard]] inline int ringItemHitboxSweepStepCount(const RingItemHitbox& hitbox)
{
    constexpr float MaxSweepStepPixels = 2.0f;
    constexpr int MaxSweepSteps = 32;
    return std::clamp(
        static_cast<int>(std::ceil(
            ringItemHitboxMaximumPointTravel(hitbox) / MaxSweepStepPixels)),
        1,
        MaxSweepSteps);
}

template <typename VisitPose>
void visitRingItemHitboxSweep(const RingItemHitbox& hitbox, VisitPose&& visitPose)
{
    const int steps = ringItemHitboxSweepStepCount(hitbox);
    const float rotationDelta = ringItemHitboxShortestAngleDelta(
        hitbox.previousRotationRadians,
        hitbox.rotationRadians);
    for (int step = 0; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        visitPose(
            lerp(hitbox.previousCenter, hitbox.center, t),
            hitbox.previousRotationRadians + rotationDelta * t);
    }
}

template <typename OverlapAtPose>
[[nodiscard]] bool ringItemHitboxSweepOverlaps(
    const RingItemHitbox& hitbox,
    OverlapAtPose&& overlapAtPose)
{
    if (overlapAtPose(
            hitbox.center,
            hitbox.rotationRadians)) {
        return true;
    }

    const int steps = ringItemHitboxSweepStepCount(hitbox);
    const float rotationDelta = ringItemHitboxShortestAngleDelta(
        hitbox.previousRotationRadians,
        hitbox.rotationRadians);
    for (int step = 0; step < steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        if (overlapAtPose(
                lerp(hitbox.previousCenter, hitbox.center, t),
                hitbox.previousRotationRadians + rotationDelta * t)) {
            return true;
        }
    }
    return false;
}

}
