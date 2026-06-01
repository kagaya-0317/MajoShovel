#include "game/ActorVisualMotion.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace majo {

namespace {

constexpr ActorVisualPose IdentityPose{};

constexpr std::array<ActorVisualKeyframe, 4> WebShootKeys{{
    {0.00f, {{0.0f, 0.0f}, {1.00f, 1.00f}, 0.0f, 0.0f, 0.0f}, ActorVisualEase::EaseOut},
    {0.08f, {{0.0f, 1.0f}, {0.82f, 1.18f}, -1.0f, 0.0f, 0.0f}, ActorVisualEase::EaseInOut},
    {0.18f, {{0.0f, -1.0f}, {1.28f, 0.78f}, 3.0f, 0.0f, 0.0f}, ActorVisualEase::EaseOut},
    {0.32f, {{0.0f, 0.0f}, {1.00f, 1.00f}, 0.0f, 0.0f, 0.0f}, ActorVisualEase::EaseOut},
}};

constexpr std::array<ActorVisualKeyframe, 5> FireBreathHopKeys{{
    {0.00f, {{0.0f, 0.0f}, {1.00f, 1.00f}, 0.0f, 0.0f, 0.0f}, ActorVisualEase::EaseInOut},
    {0.10f, {{0.0f, 3.0f}, {1.08f, 0.86f}, -1.0f, 0.0f, 0.0f}, ActorVisualEase::EaseOut},
    {0.18f, {{0.0f, 0.0f}, {0.94f, 1.08f}, 0.0f, 15.0f, 0.0f}, ActorVisualEase::EaseIn},
    {0.26f, {{0.0f, 1.0f}, {1.22f, 0.82f}, 4.0f, 4.0f, 0.0f}, ActorVisualEase::EaseOut},
    {0.45f, {{0.0f, 0.0f}, {1.00f, 1.00f}, 0.0f, 0.0f, 0.0f}, ActorVisualEase::EaseOut},
}};

constexpr std::array<ActorVisualKeyframe, 6> RadialSpikeSquashKeys{{
    {0.00f, {{0.0f, 0.0f}, {1.00f, 1.00f}, 0.0f, 0.0f, 0.0f}, ActorVisualEase::EaseInOut},
    {0.08f, {{0.0f, -1.0f}, {0.92f, 1.08f}, 0.0f, 0.0f, -1.5f}, ActorVisualEase::EaseIn},
    {0.17f, {{0.0f, 4.0f}, {1.34f, 0.66f}, 0.0f, 0.0f, 2.0f}, ActorVisualEase::EaseOut},
    {0.24f, {{0.0f, -4.0f}, {0.84f, 1.22f}, 0.0f, 7.0f, -2.5f}, ActorVisualEase::EaseInOut},
    {0.34f, {{0.0f, 1.0f}, {1.08f, 0.92f}, 0.0f, 1.0f, 1.0f}, ActorVisualEase::EaseOut},
    {0.46f, {{0.0f, 0.0f}, {1.00f, 1.00f}, 0.0f, 0.0f, 0.0f}, ActorVisualEase::EaseOut},
}};

constexpr std::array<ActorVisualKeyframe, 7> HealSlugHopKeys{{
    {0.00f, {{0.0f, 0.0f}, {1.00f, 1.00f}, 0.0f, 0.0f, 0.0f}, ActorVisualEase::EaseOut},
    {0.10f, {{0.0f, 1.0f}, {0.78f, 1.24f}, -2.0f, 0.0f, -1.5f}, ActorVisualEase::EaseInOut},
    {0.20f, {{0.0f, 3.0f}, {1.30f, 0.72f}, -3.0f, 0.0f, 1.0f}, ActorVisualEase::EaseIn},
    {0.30f, {{0.0f, -6.0f}, {0.90f, 1.15f}, 5.5f, 18.0f, 2.0f}, ActorVisualEase::EaseOut},
    {0.38f, {{0.0f, -3.0f}, {1.04f, 0.96f}, 8.0f, 8.0f, -1.0f}, ActorVisualEase::EaseIn},
    {0.46f, {{0.0f, 2.0f}, {1.18f, 0.82f}, 2.0f, 0.0f, 0.0f}, ActorVisualEase::EaseOut},
    {0.58f, {{0.0f, 0.0f}, {1.00f, 1.00f}, 0.0f, 0.0f, 0.0f}, ActorVisualEase::EaseOut},
}};

constexpr std::array<ActorVisualMotionClip, 4> Clips{{
    {"web_shoot", 0.32f, WebShootKeys},
    {"fire_breath_hop", 0.45f, FireBreathHopKeys},
    {"radial_spike_squash", 0.46f, RadialSpikeSquashKeys},
    {"heal_slug_hop", 0.58f, HealSlugHopKeys},
}};

float easeValue(float t, ActorVisualEase ease)
{
    const float clamped = clamp(t, 0.0f, 1.0f);
    switch (ease) {
    case ActorVisualEase::EaseIn:
        return clamped * clamped;
    case ActorVisualEase::EaseOut: {
        const float inv = 1.0f - clamped;
        return 1.0f - inv * inv;
    }
    case ActorVisualEase::EaseInOut:
        return clamped * clamped * (3.0f - 2.0f * clamped);
    case ActorVisualEase::Linear:
    default:
        return clamped;
    }
}

ActorVisualPose lerpPose(const ActorVisualPose& from, const ActorVisualPose& to, float t)
{
    return {
        lerp(from.offset, to.offset, t),
        lerp(from.scale, to.scale, t),
        lerp(from.forwardOffset, to.forwardOffset, t),
        lerp(from.visualAltitude, to.visualAltitude, t),
        lerp(from.rotationDegrees, to.rotationDegrees, t),
    };
}

}

ActorVisualPose actorVisualPoseIdentity()
{
    return IdentityPose;
}

const ActorVisualMotionClip* findActorVisualMotionClip(std::string_view id)
{
    if (id.empty()) {
        return nullptr;
    }
    const auto it = std::find_if(Clips.begin(), Clips.end(), [id](const ActorVisualMotionClip& clip) {
        return clip.id == id;
    });
    return it != Clips.end() ? &*it : nullptr;
}

float actorVisualMotionDuration(std::string_view id)
{
    const ActorVisualMotionClip* clip = findActorVisualMotionClip(id);
    return clip != nullptr ? std::max(0.0f, clip->durationSeconds) : 0.0f;
}

ActorVisualPose sampleActorVisualMotion(const ActorVisualMotionClip& clip, float elapsedSeconds)
{
    if (clip.keyframes.empty()) {
        return actorVisualPoseIdentity();
    }

    const float elapsed = std::max(0.0f, elapsedSeconds);
    if (elapsed <= clip.keyframes.front().timeSeconds) {
        return clip.keyframes.front().pose;
    }

    for (std::size_t i = 0; i + 1 < clip.keyframes.size(); ++i) {
        const ActorVisualKeyframe& current = clip.keyframes[i];
        const ActorVisualKeyframe& next = clip.keyframes[i + 1];
        if (elapsed > next.timeSeconds) {
            continue;
        }

        const float span = next.timeSeconds - current.timeSeconds;
        if (span <= 0.0001f) {
            return next.pose;
        }
        const float t = easeValue((elapsed - current.timeSeconds) / span, current.easeToNext);
        return lerpPose(current.pose, next.pose, t);
    }

    return clip.keyframes.back().pose;
}

ActorVisualPose sampleActorVisualMotion(std::string_view id, float elapsedSeconds)
{
    const ActorVisualMotionClip* clip = findActorVisualMotionClip(id);
    return clip != nullptr ? sampleActorVisualMotion(*clip, elapsedSeconds) : actorVisualPoseIdentity();
}

}
