#include "game/DungeonLayout.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace majo {

namespace {

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

Vec2 tileToVec(DungeonTile tile)
{
    return {static_cast<float>(tile.x), static_cast<float>(tile.y)};
}

Vec2 perpendicular(Vec2 v)
{
    return {-v.y, v.x};
}

float distancePointSegment(Vec2 point, Vec2 a, Vec2 b, float* outSegmentT = nullptr)
{
    const Vec2 ab = b - a;
    const float abLenSq = lengthSquared(ab);
    float t = 0.0f;
    if (abLenSq > 0.0001f) {
        t = clamp(dot(point - a, ab) / abLenSq, 0.0f, 1.0f);
    }
    if (outSegmentT != nullptr) {
        *outSegmentT = t;
    }
    return length(point - (a + ab * t));
}

std::vector<Vec2> makeWanderingPath(
    Vec2 start,
    Vec2 goal,
    std::mt19937& rng,
    int sampleCount,
    float lateralScale,
    float longitudinalScale)
{
    std::uniform_real_distribution<float> phaseDist(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> jitterDist(-1.0f, 1.0f);

    const Vec2 delta = goal - start;
    const Vec2 dir = normalize(delta);
    const Vec2 side = perpendicular(dir);
    const float distance = std::max(1.0f, length(delta));
    const float lateralAmplitude = distance * lateralScale;
    const float longitudinalAmplitude = distance * longitudinalScale;
    const float phaseA = phaseDist(rng);
    const float phaseB = phaseDist(rng);

    std::vector<Vec2> points;
    points.reserve(static_cast<std::size_t>(sampleCount));
    for (int i = 0; i < sampleCount; ++i) {
        const float t = sampleCount > 1 ? static_cast<float>(i) / static_cast<float>(sampleCount - 1) : 0.0f;
        Vec2 point = lerp(start, goal, t);
        if (i != 0 && i != sampleCount - 1) {
            const float envelope = std::sin(t * Pi);
            const float lateral =
                (std::sin(t * Pi * 2.2f + phaseA) * 0.70f +
                    std::sin(t * Pi * 5.1f + phaseB) * 0.30f +
                    jitterDist(rng) * 0.22f) *
                lateralAmplitude * envelope;
            const float longitudinal =
                std::sin(t * Pi * 3.3f + phaseB) *
                longitudinalAmplitude * envelope;
            point += side * lateral + dir * longitudinal;
        }
        points.push_back(point);
    }
    return points;
}

Vec2 pointAtProgress(const std::vector<Vec2>& points, float progress)
{
    if (points.empty()) {
        return {};
    }
    if (points.size() == 1) {
        return points.front();
    }

    float totalLength = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        totalLength += length(points[i] - points[i - 1]);
    }
    if (totalLength <= 0.0001f) {
        return points.front();
    }

    const float target = totalLength * clamp(progress, 0.0f, 1.0f);
    float traveled = 0.0f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const float segmentLength = length(points[i] - points[i - 1]);
        if (traveled + segmentLength >= target) {
            const float t = segmentLength > 0.0001f ? (target - traveled) / segmentLength : 0.0f;
            return lerp(points[i - 1], points[i], t);
        }
        traveled += segmentLength;
    }
    return points.back();
}

Vec2 tangentAtProgress(const std::vector<Vec2>& points, float progress)
{
    if (points.size() < 2) {
        return {1.0f, 0.0f};
    }

    const std::size_t index = std::min(
        points.size() - 2,
        static_cast<std::size_t>(clamp(progress, 0.0f, 1.0f) * static_cast<float>(points.size() - 1)));
    return normalize(points[index + 1] - points[index]);
}

}

DungeonLayout generateDungeonLayout(const DungeonGenerationContext& context)
{
    std::mt19937 rng(context.seed);
    std::uniform_real_distribution<float> angleDist(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> unitDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> branchJitter(-0.35f, 0.35f);
    std::uniform_int_distribution<int> branchCountDist(3, context.roguelike ? 7 : 5);

    DungeonLayout layout;
    layout.stageId = std::max(1, context.stageId);
    layout.seed = context.seed;
    layout.stageHardnessMultiplier = std::max(0.25f, context.stageHardnessMultiplier);
    layout.startTile = {0, 0};

    const float hardness = layout.stageHardnessMultiplier;
    const float goalDistance = (context.roguelike ? 150.0f : 105.0f) + 18.0f * static_cast<float>(layout.stageId - 1) * hardness + unitDist(rng) * 35.0f;
    const float goalAngle = angleDist(rng);
    const Vec2 goal = fromAngle(goalAngle) * goalDistance;
    layout.goalTile = {
        static_cast<int>(std::round(goal.x)),
        static_cast<int>(std::round(goal.y)),
    };

    layout.mainPathPoints = makeWanderingPath(
        tileToVec(layout.startTile),
        tileToVec(layout.goalTile),
        rng,
        36,
        0.10f,
        0.035f);

    const int branchCount = branchCountDist(rng);
    for (int i = 0; i < branchCount; ++i) {
        const float progress = (static_cast<float>(i + 1) / static_cast<float>(branchCount + 1)) + branchJitter(rng) * 0.18f;
        const Vec2 anchor = pointAtProgress(layout.mainPathPoints, clamp(progress, 0.12f, 0.88f));
        const Vec2 tangent = tangentAtProgress(layout.mainPathPoints, progress);
        Vec2 side = perpendicular(tangent);
        if (unitDist(rng) < 0.5f) {
            side = side * -1.0f;
        }
        const float branchLength = 18.0f + unitDist(rng) * 30.0f;
        const Vec2 end = anchor + normalize(side + tangent * branchJitter(rng)) * branchLength;

        DungeonPath branch;
        branch.points = makeWanderingPath(anchor, end, rng, 14, 0.18f, 0.02f);
        layout.specialRoomAnchors.push_back(branch.points.empty() ? end : branch.points.back());
        layout.branchPathPoints.push_back(std::move(branch));
    }

    constexpr int WarpAnchorCount = 8;
    layout.warpPointAnchors.reserve(WarpAnchorCount);
    for (int i = 0; i < WarpAnchorCount; ++i) {
        layout.warpPointAnchors.push_back(pointAtProgress(
            layout.mainPathPoints,
            static_cast<float>(i + 1) / static_cast<float>(WarpAnchorCount + 1)));
    }

    return layout;
}

DungeonLayoutMetrics calculateDungeonLayoutMetrics(const DungeonLayout& layout, Vec2 tilePosition)
{
    DungeonLayoutMetrics metrics;
    const Vec2 start = tileToVec(layout.startTile);
    metrics.distanceFromStart = length(tilePosition - start);

    if (layout.mainPathPoints.size() < 2) {
        metrics.distanceFromMainPath = metrics.distanceFromStart;
        metrics.pathProgress = 0.0f;
        return metrics;
    }

    float totalLength = 0.0f;
    for (std::size_t i = 1; i < layout.mainPathPoints.size(); ++i) {
        totalLength += length(layout.mainPathPoints[i] - layout.mainPathPoints[i - 1]);
    }

    float bestDistance = 1.0e30f;
    float bestPathDistance = 0.0f;
    float traveled = 0.0f;
    for (std::size_t i = 1; i < layout.mainPathPoints.size(); ++i) {
        float segmentT = 0.0f;
        const Vec2 a = layout.mainPathPoints[i - 1];
        const Vec2 b = layout.mainPathPoints[i];
        const float distance = distancePointSegment(tilePosition, a, b, &segmentT);
        const float segmentLength = length(b - a);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestPathDistance = traveled + segmentLength * segmentT;
        }
        traveled += segmentLength;
    }

    metrics.distanceFromMainPath = bestDistance;
    metrics.pathProgress = totalLength > 0.0001f ? clamp(bestPathDistance / totalLength, 0.0f, 1.0f) : 0.0f;
    return metrics;
}

}
