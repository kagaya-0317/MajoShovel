#include "game/GroundLineSystem.hpp"

#include "engine/Renderer.hpp"
#include "game/DepthRender.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <numeric>
#include <unordered_set>

namespace majo {

namespace {

constexpr std::size_t MaxGroundLineSegments = 900;
constexpr std::size_t MaxCandidateSegments = 220;
constexpr float MinSegmentLength = 5.0f;
constexpr float MaxContinuousSegmentLength = 92.0f;
constexpr float DefaultLineLifetimeSeconds = 26.0f;
constexpr float MinLineWidth = 2.0f;
constexpr float MaxLineWidth = 9.0f;
constexpr float SampleSpacing = 7.0f;
constexpr float MinCircleRadius = 24.0f;
constexpr float MaxCircleRadius = 190.0f;
constexpr int AngleBinCount = 24;
constexpr int MinCoveredAngleBins = 20;

bool finite(Vec2 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y);
}

float segmentLength(Vec2 a, Vec2 b)
{
    return length(b - a);
}

float distanceToSegmentSquared(Vec2 point, Vec2 a, Vec2 b)
{
    const Vec2 ab = b - a;
    const float lenSq = lengthSquared(ab);
    if (lenSq <= 0.0001f) {
        return distanceSquared(point, a);
    }
    const float t = clamp(((point.x - a.x) * ab.x + (point.y - a.y) * ab.y) / lenSq, 0.0f, 1.0f);
    return distanceSquared(point, a + ab * t);
}

Color lineColorWithAlpha(Color color, float alphaScale)
{
    color.a = static_cast<unsigned char>(std::clamp(
        static_cast<int>(std::round(static_cast<float>(color.a) * alphaScale)),
        0,
        255));
    return color;
}

}

GroundLineSystem::StrokeState& GroundLineSystem::stateForSource(std::string_view sourceKey)
{
    const auto it = std::find_if(strokeStateKeys_.begin(), strokeStateKeys_.end(), [sourceKey](const std::string& key) {
        return key == sourceKey;
    });
    if (it != strokeStateKeys_.end()) {
        return strokeStates_[static_cast<std::size_t>(std::distance(strokeStateKeys_.begin(), it))];
    }

    strokeStateKeys_.push_back(std::string(sourceKey));
    strokeStates_.push_back({});
    return strokeStates_.back();
}

bool GroundLineSystem::drawWhiteLine(
    std::string_view sourceKey,
    Vec2 position,
    float width,
    float strength,
    float lifetimeSeconds)
{
    if (sourceKey.empty() || !finite(position)) {
        return false;
    }

    StrokeState& state = stateForSource(sourceKey);
    if (!state.hasLastPosition) {
        state.lastPosition = position;
        state.hasLastPosition = true;
        return false;
    }

    const float distance = segmentLength(state.lastPosition, position);
    if (distance < MinSegmentLength) {
        return false;
    }
    if (distance > MaxContinuousSegmentLength) {
        state.lastPosition = position;
        ++state.strokeId;
        return false;
    }

    Segment segment;
    segment.id = nextSegmentId_++;
    segment.strokeId = state.strokeId;
    segment.sourceKey = std::string(sourceKey);
    segment.a = state.lastPosition;
    segment.b = position;
    segment.width = std::clamp(width, MinLineWidth, MaxLineWidth);
    segment.strength = std::max(0.1f, strength);
    segment.lifetimeSeconds = lifetimeSeconds > 0.0f ? lifetimeSeconds : DefaultLineLifetimeSeconds;
    segments_.push_back(std::move(segment));

    state.lastPosition = position;
    trimOldestSegments();
    return true;
}

std::optional<MagicCircleCandidate> GroundLineSystem::findCompletedCircleNear(
    Vec2 position,
    float searchRadius,
    std::string_view sourceKey) const
{
    if (sourceKey.empty() || !finite(position)) {
        return std::nullopt;
    }

    int latestStrokeId = 0;
    for (auto it = segments_.rbegin(); it != segments_.rend(); ++it) {
        if (it->sourceKey == sourceKey) {
            latestStrokeId = it->strokeId;
            break;
        }
    }
    if (latestStrokeId <= 0) {
        return std::nullopt;
    }

    std::vector<const Segment*> sourceSegments;
    sourceSegments.reserve(MaxCandidateSegments);
    for (auto it = segments_.rbegin(); it != segments_.rend(); ++it) {
        if (it->sourceKey != sourceKey || it->strokeId != latestStrokeId) {
            continue;
        }
        sourceSegments.push_back(&*it);
        if (sourceSegments.size() >= MaxCandidateSegments) {
            break;
        }
    }
    std::reverse(sourceSegments.begin(), sourceSegments.end());
    if (sourceSegments.size() < 10) {
        return std::nullopt;
    }

    const Segment* first = sourceSegments.front();
    const Segment* last = sourceSegments.back();
    if (distanceSquared(position, last->b) > std::max(8.0f, searchRadius) * std::max(8.0f, searchRadius)) {
        return std::nullopt;
    }

    std::vector<Vec2> samples;
    samples.reserve(sourceSegments.size() * 3);
    float totalLength = 0.0f;
    float strengthSum = 0.0f;
    for (const Segment* segment : sourceSegments) {
        const float lengthValue = segmentLength(segment->a, segment->b);
        totalLength += lengthValue;
        strengthSum += segment->strength;
        const int sampleCount = std::max(1, static_cast<int>(std::ceil(lengthValue / SampleSpacing)));
        for (int i = 0; i <= sampleCount; ++i) {
            const float t = sampleCount > 0 ? static_cast<float>(i) / static_cast<float>(sampleCount) : 1.0f;
            samples.push_back(lerp(segment->a, segment->b, t));
        }
    }
    if (samples.size() < 24 || totalLength < 80.0f) {
        return std::nullopt;
    }

    Vec2 center{};
    for (Vec2 sample : samples) {
        center += sample;
    }
    center = center / static_cast<float>(samples.size());

    float radiusSum = 0.0f;
    for (Vec2 sample : samples) {
        radiusSum += length(sample - center);
    }
    const float radius = radiusSum / static_cast<float>(samples.size());
    if (radius < MinCircleRadius || radius > MaxCircleRadius) {
        return std::nullopt;
    }

    float radiusVariance = 0.0f;
    std::array<bool, AngleBinCount> coveredBins{};
    for (Vec2 sample : samples) {
        const float sampleRadius = length(sample - center);
        const float diff = sampleRadius - radius;
        radiusVariance += diff * diff;
        float angle = std::atan2(sample.y - center.y, sample.x - center.x);
        if (angle < 0.0f) {
            angle += Pi * 2.0f;
        }
        const int bin = std::clamp(
            static_cast<int>(std::floor(angle / (Pi * 2.0f) * static_cast<float>(AngleBinCount))),
            0,
            AngleBinCount - 1);
        coveredBins[static_cast<std::size_t>(bin)] = true;
    }
    const float radiusDeviation = std::sqrt(radiusVariance / static_cast<float>(samples.size()));
    if (radiusDeviation > radius * 0.42f + 10.0f) {
        return std::nullopt;
    }

    const int coveredCount = static_cast<int>(std::count(coveredBins.begin(), coveredBins.end(), true));
    if (coveredCount < MinCoveredAngleBins) {
        return std::nullopt;
    }

    const float closeDistance = std::max({22.0f, radius * 0.30f, last->width * 4.0f});
    bool closed = distanceSquared(first->a, last->b) <= closeDistance * closeDistance;
    if (!closed) {
        const std::size_t earlySampleLimit = samples.size() / 3;
        for (std::size_t i = 0; i < earlySampleLimit; ++i) {
            if (distanceSquared(samples[i], last->b) <= closeDistance * closeDistance) {
                closed = true;
                break;
            }
        }
    }
    if (!closed) {
        for (const Segment* segment : sourceSegments) {
            if (segment == last) {
                break;
            }
            if (distanceToSegmentSquared(last->b, segment->a, segment->b) <= closeDistance * closeDistance) {
                closed = true;
                break;
            }
        }
    }
    if (!closed) {
        return std::nullopt;
    }

    const float circumference = Pi * 2.0f * radius;
    if (totalLength < circumference * 0.58f || totalLength > circumference * 1.85f) {
        return std::nullopt;
    }

    MagicCircleCandidate candidate;
    candidate.center = center;
    candidate.radius = std::clamp(radius, MinCircleRadius, MaxCircleRadius);
    const float coverageScale = static_cast<float>(coveredCount) / static_cast<float>(AngleBinCount);
    candidate.power = std::max(0.25f, strengthSum / static_cast<float>(sourceSegments.size())) *
        std::clamp(0.75f + coverageScale * 0.35f, 0.85f, 1.15f);
    candidate.segmentIds.reserve(sourceSegments.size());
    for (const Segment* segment : sourceSegments) {
        candidate.segmentIds.push_back(segment->id);
    }
    return candidate;
}

void GroundLineSystem::consumeSegments(std::span<const int> segmentIds)
{
    if (segmentIds.empty()) {
        return;
    }
    std::unordered_set<int> consumed(segmentIds.begin(), segmentIds.end());
    segments_.erase(
        std::remove_if(segments_.begin(), segments_.end(), [&consumed](const Segment& segment) {
            return consumed.find(segment.id) != consumed.end();
        }),
        segments_.end());
}

void GroundLineSystem::clearSource(std::string_view sourceKey)
{
    if (sourceKey.empty()) {
        return;
    }
    segments_.erase(
        std::remove_if(segments_.begin(), segments_.end(), [sourceKey](const Segment& segment) {
            return segment.sourceKey == sourceKey;
        }),
        segments_.end());
    const auto keyIt = std::find_if(strokeStateKeys_.begin(), strokeStateKeys_.end(), [sourceKey](const std::string& key) {
        return key == sourceKey;
    });
    if (keyIt == strokeStateKeys_.end()) {
        return;
    }
    const std::size_t index = static_cast<std::size_t>(std::distance(strokeStateKeys_.begin(), keyIt));
    strokeStateKeys_.erase(keyIt);
    strokeStates_.erase(strokeStates_.begin() + static_cast<std::ptrdiff_t>(index));
}

void GroundLineSystem::update(float dt)
{
    if (dt <= 0.0f) {
        return;
    }
    for (Segment& segment : segments_) {
        segment.ageSeconds += dt;
    }
    segments_.erase(
        std::remove_if(segments_.begin(), segments_.end(), [](const Segment& segment) {
            return segment.lifetimeSeconds > 0.0f && segment.ageSeconds >= segment.lifetimeSeconds;
        }),
        segments_.end());
}

void GroundLineSystem::appendRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer) const
{
    for (const Segment& segment : segments_) {
        const float remaining = segment.lifetimeSeconds > 0.0f
            ? std::clamp((segment.lifetimeSeconds - segment.ageSeconds) / segment.lifetimeSeconds, 0.0f, 1.0f)
            : 1.0f;
        const float fade = std::min(1.0f, remaining * 3.0f);
        const Vec2 a = segment.a;
        const Vec2 b = segment.b;
        const float width = segment.width;
        entries.push_back(DepthRenderEntry{
            (a.y + b.y) * 0.5f - 2.0f,
            [&renderer, a, b, width, fade]() {
                renderer.drawSoftLine(a, b, width + 5.0f, lineColorWithAlpha({120, 168, 255, 76}, fade));
                renderer.drawSoftLine(a, b, width + 2.0f, lineColorWithAlpha({210, 232, 255, 142}, fade));
                renderer.drawSoftLine(a, b, std::max(1.5f, width * 0.58f), lineColorWithAlpha({255, 255, 255, 230}, fade));
            },
        });
    }
}

void GroundLineSystem::clear()
{
    segments_.clear();
    strokeStates_.clear();
    strokeStateKeys_.clear();
    nextSegmentId_ = 1;
}

void GroundLineSystem::trimOldestSegments()
{
    if (segments_.size() <= MaxGroundLineSegments) {
        return;
    }
    const std::size_t removeCount = segments_.size() - MaxGroundLineSegments;
    segments_.erase(segments_.begin(), segments_.begin() + static_cast<std::ptrdiff_t>(removeCount));
}

}
