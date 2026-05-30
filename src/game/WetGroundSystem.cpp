#include "game/WetGroundSystem.hpp"

#include "engine/Renderer.hpp"
#include "game/DepthRender.hpp"
#include "game/GroundLineSystem.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace majo {

namespace {

constexpr std::size_t MaxWetGroundMarks = 420;
constexpr std::size_t LightweightMaxWetGroundMarks = 72;
constexpr float MinWetGroundRadius = 8.0f;
constexpr float MaxWetGroundRadius = 42.0f;
constexpr float MinWetEmitDistance = 11.0f;
constexpr float WetEmitCooldownSeconds = 0.22f;
constexpr float SourceStateRetainSeconds = 9.0f;
constexpr float BaseLifetimeSeconds = 4.8f;
constexpr float LifetimeStrengthBonusSeconds = 0.45f;
constexpr float ErasePadding = 5.0f;
constexpr float MinEraseRadius = 4.0f;

bool finite(Vec2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

float remainingRatio(float ageSeconds, float lifetimeSeconds)
{
    if (lifetimeSeconds <= 0.0f) {
        return 1.0f;
    }
    return std::clamp((lifetimeSeconds - ageSeconds) / lifetimeSeconds, 0.0f, 1.0f);
}

float radiusScaleFor(float remaining)
{
    return remaining;
}

float alphaScaleFor(float remaining)
{
    return std::min(1.0f, remaining * 2.8f);
}

unsigned char scaledAlpha(unsigned char alpha, float scale)
{
    return static_cast<unsigned char>(std::clamp(
        static_cast<int>(std::lround(static_cast<float>(alpha) * std::max(0.0f, scale))),
        0,
        255));
}

float wetLifetimeForStrength(float strength)
{
    return BaseLifetimeSeconds + std::clamp(strength, 0.0f, 4.0f) * LifetimeStrengthBonusSeconds;
}

unsigned char wetAlphaForStrength(float strength)
{
    return static_cast<unsigned char>(std::clamp(
        static_cast<int>(std::lround(48.0f + std::max(0.0f, strength) * 10.0f)),
        42,
        82));
}

}

WetGroundSystem::SourceState& WetGroundSystem::stateForSource(std::string_view sourceKey)
{
    const auto it = std::find_if(sourceStates_.begin(), sourceStates_.end(), [sourceKey](const SourceState& state) {
        return state.key == sourceKey;
    });
    if (it != sourceStates_.end()) {
        return *it;
    }

    SourceState state;
    state.key = std::string(sourceKey);
    sourceStates_.push_back(std::move(state));
    return sourceStates_.back();
}

bool WetGroundSystem::touchSource(
    std::string_view sourceKey,
    Vec2 position,
    float radius,
    float strength)
{
    if (sourceKey.empty() || !finite(position)) {
        return false;
    }

    SourceState& state = stateForSource(sourceKey);
    state.idleSeconds = 0.0f;

    const bool movedEnough = !state.hasLastEmitPosition ||
        distanceSquared(state.lastEmitPosition, position) >= MinWetEmitDistance * MinWetEmitDistance;
    const bool cooldownElapsed = state.cooldownSeconds <= 0.0f;
    if (!movedEnough && !cooldownElapsed) {
        return false;
    }

    const bool spawned = spawn(position, radius, strength);
    if (spawned) {
        state.lastEmitPosition = position;
        state.hasLastEmitPosition = true;
        state.cooldownSeconds = WetEmitCooldownSeconds;
    }
    return spawned;
}

bool WetGroundSystem::spawn(Vec2 position, float radius, float strength)
{
    if (!finite(position) || radius <= 0.0f) {
        return false;
    }

    std::uniform_real_distribution<float> rotationDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> yScaleDistribution(0.48f, 0.68f);
    std::uniform_real_distribution<float> radialDistribution(0.78f, 1.16f);

    Mark mark;
    mark.id = nextMarkId_++;
    mark.center = position;
    mark.baseRadius = std::clamp(radius, MinWetGroundRadius, MaxWetGroundRadius);
    mark.yScale = yScaleDistribution(rng_);
    mark.rotation = rotationDistribution(rng_);
    mark.lifetimeSeconds = wetLifetimeForStrength(strength);
    mark.alpha = wetAlphaForStrength(strength);
    const float cosRotation = std::cos(mark.rotation);
    const float sinRotation = std::sin(mark.rotation);
    for (int i = 0; i < MarkPointCount; ++i) {
        const float angle = static_cast<float>(i) / static_cast<float>(MarkPointCount) * Pi * 2.0f;
        const float pointRadius = mark.baseRadius * radialDistribution(rng_);
        const Vec2 local{
            std::cos(angle) * pointRadius,
            std::sin(angle) * pointRadius * mark.yScale,
        };
        mark.baseOffsets[static_cast<std::size_t>(i)] = {
            local.x * cosRotation - local.y * sinRotation,
            local.x * sinRotation + local.y * cosRotation,
        };
    }

    marks_.push_back(std::move(mark));
    trimOldestMarks();
    return true;
}

void WetGroundSystem::update(float dt)
{
    if (dt <= 0.0f) {
        return;
    }

    for (Mark& mark : marks_) {
        mark.ageSeconds += dt;
    }
    marks_.erase(
        std::remove_if(marks_.begin(), marks_.end(), [](const Mark& mark) {
            return mark.lifetimeSeconds > 0.0f && mark.ageSeconds >= mark.lifetimeSeconds;
        }),
        marks_.end());

    for (SourceState& state : sourceStates_) {
        state.cooldownSeconds = std::max(0.0f, state.cooldownSeconds - dt);
        state.idleSeconds += dt;
    }
    trimIdleSources();
}

int WetGroundSystem::erasePendingGroundLines(GroundLineSystem& groundLines)
{
    int erased = 0;
    for (Mark& mark : marks_) {
        if (!mark.erasePending) {
            continue;
        }
        mark.erasePending = false;
        const float remaining = remainingRatio(mark.ageSeconds, mark.lifetimeSeconds);
        const float radius = mark.baseRadius * radiusScaleFor(remaining) + ErasePadding;
        if (radius <= MinEraseRadius) {
            continue;
        }
        erased += groundLines.eraseNear(mark.center, radius);
    }
    return erased;
}

void WetGroundSystem::appendRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer) const
{
    for (const Mark& mark : marks_) {
        const float remaining = remainingRatio(mark.ageSeconds, mark.lifetimeSeconds);
        const float radiusScale = radiusScaleFor(remaining);
        const unsigned char alpha = scaledAlpha(mark.alpha, alphaScaleFor(remaining));
        if (radiusScale <= 0.0f || alpha == 0) {
            continue;
        }

        const Mark drawable = mark;
        entries.push_back(DepthRenderEntry{
            mark.center.y - mark.baseRadius * 0.32f,
            [&renderer, drawable, radiusScale, alpha]() {
                std::array<Vec2, MarkPointCount> points{};
                for (int i = 0; i < MarkPointCount; ++i) {
                    points[static_cast<std::size_t>(i)] =
                        drawable.center + drawable.baseOffsets[static_cast<std::size_t>(i)] * radiusScale;
                }
                renderer.fillPolygon(points.data(), points.size(), {0, 0, 0, alpha});
            },
        });
    }
}

void WetGroundSystem::clear()
{
    marks_.clear();
    sourceStates_.clear();
    nextMarkId_ = 1;
}

void WetGroundSystem::setLightweightMode(bool enabled)
{
    if (lightweightMode_ == enabled) {
        return;
    }
    lightweightMode_ = enabled;
    trimOldestMarks();
}

std::size_t WetGroundSystem::maxMarks() const
{
    return lightweightMode_ ? LightweightMaxWetGroundMarks : MaxWetGroundMarks;
}

void WetGroundSystem::trimOldestMarks()
{
    const std::size_t limit = maxMarks();
    if (marks_.size() <= limit) {
        return;
    }
    const std::size_t removeCount = marks_.size() - limit;
    marks_.erase(marks_.begin(), marks_.begin() + static_cast<std::ptrdiff_t>(removeCount));
}

void WetGroundSystem::trimIdleSources()
{
    sourceStates_.erase(
        std::remove_if(sourceStates_.begin(), sourceStates_.end(), [](const SourceState& state) {
            return state.idleSeconds >= SourceStateRetainSeconds;
        }),
        sourceStates_.end());
}

}
