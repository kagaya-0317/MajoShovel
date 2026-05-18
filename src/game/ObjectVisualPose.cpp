#include "game/ObjectVisualPose.hpp"

#include <cmath>

namespace majo {

namespace {

constexpr float RadiansToDegrees = 180.0f / Pi;

float directionDegrees(Vec2 direction)
{
    const Vec2 normalized = normalize(direction);
    return std::atan2(normalized.y, normalized.x) * RadiansToDegrees;
}

float rotationDegreesForSpriteUpToDirection(Vec2 direction)
{
    return directionDegrees(direction) + 90.0f;
}

Vec2 fallbackForwardFromOutward(Vec2 outward)
{
    const Vec2 normalized = normalize(outward);
    return {-normalized.y, normalized.x};
}

}

ObjectImageDrawOptions objectGroundImageOptions(
    const ObjectDefinition& object,
    const ObjectImageDrawOptions& base)
{
    ObjectImageDrawOptions options = base;
    options.rotationDegrees = base.rotationDegrees + object.visualRotation.groundDegrees;
    return options;
}

ObjectImageDrawOptions objectRingImageOptions(
    const ObjectDefinition& object,
    Vec2 outward,
    Vec2 forward,
    float totalSeconds,
    const ObjectImageDrawOptions& base)
{
    ObjectImageDrawOptions options = base;
    const ObjectRingRotation& rotation = object.visualRotation.ring;
    switch (rotation.mode) {
    case ObjectRingRotationMode::Fixed:
        options.rotationDegrees = base.rotationDegrees + rotation.offsetDegrees;
        break;
    case ObjectRingRotationMode::Outward:
        options.rotationDegrees = base.rotationDegrees +
            rotationDegreesForSpriteUpToDirection(outward) +
            rotation.offsetDegrees;
        break;
    case ObjectRingRotationMode::Forward: {
        const Vec2 direction = lengthSquared(forward) > 0.0001f
            ? forward
            : fallbackForwardFromOutward(outward);
        options.rotationDegrees = base.rotationDegrees +
            rotationDegreesForSpriteUpToDirection(direction) +
            rotation.offsetDegrees;
        break;
    }
    case ObjectRingRotationMode::Spin:
        options.rotationDegrees = base.rotationDegrees +
            rotation.offsetDegrees +
            totalSeconds * rotation.spinDegreesPerSecond;
        break;
    }
    return options;
}

}
