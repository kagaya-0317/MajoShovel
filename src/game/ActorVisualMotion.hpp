#pragma once

#include "engine/Math.hpp"

#include <span>
#include <string_view>

namespace majo {

enum class ActorVisualEase {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
};

struct ActorVisualPose {
    Vec2 offset{};
    Vec2 scale{1.0f, 1.0f};
    float forwardOffset = 0.0f;
    float visualAltitude = 0.0f;
    float rotationDegrees = 0.0f;
};

struct ActorVisualKeyframe {
    float timeSeconds = 0.0f;
    ActorVisualPose pose{};
    ActorVisualEase easeToNext = ActorVisualEase::Linear;
};

struct ActorVisualMotionClip {
    std::string_view id;
    float durationSeconds = 0.0f;
    std::span<const ActorVisualKeyframe> keyframes;
};

[[nodiscard]] ActorVisualPose actorVisualPoseIdentity();
[[nodiscard]] const ActorVisualMotionClip* findActorVisualMotionClip(std::string_view id);
[[nodiscard]] float actorVisualMotionDuration(std::string_view id);
[[nodiscard]] ActorVisualPose sampleActorVisualMotion(const ActorVisualMotionClip& clip, float elapsedSeconds);
[[nodiscard]] ActorVisualPose sampleActorVisualMotion(std::string_view id, float elapsedSeconds);

}
