#include "game/DungeonLayout.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string_view>
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

bool isProfile(std::string_view value, std::string_view expected)
{
    return value == expected;
}

int branchCountForContext(const DungeonGenerationContext& context)
{
    const float density = clamp(context.branchDensity, 0.0f, 1.0f);
    float branchCount = 2.0f + density * 10.0f;
    if (context.roguelike || isProfile(context.generationProfile, "astral_rogue")) {
        branchCount += 2.0f;
    } else if (isProfile(context.generationProfile, "junk_layer")) {
        branchCount += 1.0f;
    } else if (isProfile(context.generationProfile, "star_core")) {
        branchCount -= 0.5f;
    }
    return std::clamp(static_cast<int>(std::round(branchCount)), 1, 12);
}

SpecialRoomType chooseExtraRoomType(int stageId, std::string_view generationProfile, int index, float progress)
{
    const std::array<SpecialRoomType, 5> stageOne{
        SpecialRoomType::SafeCavern,
        SpecialRoomType::OreRoom,
        SpecialRoomType::CoinRoom,
        SpecialRoomType::TreasureRoom,
        SpecialRoomType::EnemyRoom,
    };
    const std::array<SpecialRoomType, 5> stageTwo{
        SpecialRoomType::CoinRoom,
        SpecialRoomType::TreasureRoom,
        SpecialRoomType::EnemyRoom,
        SpecialRoomType::OreRoom,
        SpecialRoomType::SafeCavern,
    };
    const std::array<SpecialRoomType, 5> stageThree{
        SpecialRoomType::TreasureRoom,
        SpecialRoomType::OreRoom,
        SpecialRoomType::EnemyRoom,
        SpecialRoomType::CoinRoom,
        SpecialRoomType::SafeCavern,
    };
    const std::array<SpecialRoomType, 5> stageFour{
        SpecialRoomType::OreRoom,
        SpecialRoomType::SafeCavern,
        SpecialRoomType::CoinRoom,
        SpecialRoomType::TreasureRoom,
        SpecialRoomType::EnemyRoom,
    };
    const std::array<SpecialRoomType, 5> junkLayer{
        SpecialRoomType::EnemyRoom,
        SpecialRoomType::TreasureRoom,
        SpecialRoomType::CoinRoom,
        SpecialRoomType::OreRoom,
        SpecialRoomType::SafeCavern,
    };
    const std::array<SpecialRoomType, 5> starCore{
        SpecialRoomType::TreasureRoom,
        SpecialRoomType::EnemyRoom,
        SpecialRoomType::OreRoom,
        SpecialRoomType::SafeCavern,
        SpecialRoomType::CoinRoom,
    };
    const std::array<SpecialRoomType, 5> astralRogue{
        SpecialRoomType::OreRoom,
        SpecialRoomType::EnemyRoom,
        SpecialRoomType::TreasureRoom,
        SpecialRoomType::SafeCavern,
        SpecialRoomType::CoinRoom,
    };

    if (progress > 0.78f && index % 2 == 0) {
        return index % 4 == 0 ? SpecialRoomType::TreasureRoom : SpecialRoomType::EnemyRoom;
    }
    if (isProfile(generationProfile, "junk_layer")) {
        return junkLayer[static_cast<std::size_t>(index) % junkLayer.size()];
    }
    if (isProfile(generationProfile, "star_core")) {
        return starCore[static_cast<std::size_t>(index) % starCore.size()];
    }
    if (isProfile(generationProfile, "astral_rogue")) {
        return astralRogue[static_cast<std::size_t>(index) % astralRogue.size()];
    }
    if (stageId <= 1) {
        return stageOne[static_cast<std::size_t>(index) % stageOne.size()];
    }
    if (stageId == 2) {
        return stageTwo[static_cast<std::size_t>(index) % stageTwo.size()];
    }
    if (stageId == 3) {
        return stageThree[static_cast<std::size_t>(index) % stageThree.size()];
    }
    return stageFour[static_cast<std::size_t>(index) % stageFour.size()];
}

bool overlapsExistingRoom(const std::vector<SpecialRoomAnchor>& rooms, Vec2 center, float radius)
{
    for (const SpecialRoomAnchor& room : rooms) {
        const float minDistance = room.radius + radius + 4.0f;
        if (distanceSquared(room.center, center) < minDistance * minDistance) {
            return true;
        }
    }
    return false;
}

float roomRadiusBonusForProfile(std::string_view generationProfile)
{
    if (isProfile(generationProfile, "junk_layer")) {
        return 0.4f;
    }
    if (isProfile(generationProfile, "star_core")) {
        return -0.2f;
    }
    if (isProfile(generationProfile, "astral_rogue")) {
        return 0.8f;
    }
    return 0.0f;
}

}

DungeonLayout generateDungeonLayout(const DungeonGenerationContext& context)
{
    std::mt19937 rng(context.seed);
    std::uniform_real_distribution<float> angleDist(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> unitDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> branchJitter(-0.35f, 0.35f);

    DungeonLayout layout;
    layout.stageId = std::max(1, context.stageId);
    layout.seed = context.seed;
    layout.stageHardnessMultiplier = std::max(0.25f, context.stageHardnessMultiplier);
    layout.cavernWidthMultiplier = std::max(0.35f, context.cavernWidthMultiplier);
    layout.generationProfile = context.generationProfile.empty() ? "natural_cave" : context.generationProfile;
    layout.terrainProfile = context.terrainProfile.empty() ? "soft_stardust" : context.terrainProfile;
    layout.startTile = {0, 0};

    const float goalDistance = static_cast<float>(std::clamp(context.goalDistanceTiles, 48, 1200)) *
        (0.96f + unitDist(rng) * 0.08f);
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
        std::clamp(static_cast<int>(std::round(goalDistance / 9.0f)), 32, 96),
        0.06f + clamp(context.detourRate, 0.0f, 1.0f) * 0.20f,
        0.02f + clamp(context.detourRate, 0.0f, 1.0f) * 0.06f);

    std::vector<Vec2> roomCandidates;
    const int targetBranchCount = branchCountForContext(context);
    std::uniform_int_distribution<int> branchCountDist(
        std::max(1, targetBranchCount - 1),
        std::max(1, targetBranchCount + 1));
    const int branchCount = branchCountDist(rng);
    for (int i = 0; i < branchCount; ++i) {
        const float progress = (static_cast<float>(i + 1) / static_cast<float>(branchCount + 1)) + branchJitter(rng) * 0.18f;
        const Vec2 anchor = pointAtProgress(layout.mainPathPoints, clamp(progress, 0.12f, 0.88f));
        const Vec2 tangent = tangentAtProgress(layout.mainPathPoints, progress);
        Vec2 side = perpendicular(tangent);
        if (unitDist(rng) < 0.5f) {
            side = side * -1.0f;
        }
        const float branchLength = (18.0f + unitDist(rng) * 30.0f) *
            (0.85f + clamp(context.detourRate, 0.0f, 1.0f) * 0.55f);
        const Vec2 end = anchor + normalize(side + tangent * branchJitter(rng)) * branchLength;

        DungeonPath branch;
        branch.points = makeWanderingPath(anchor, end, rng, 14, 0.18f, 0.02f);
        roomCandidates.push_back(branch.points.empty() ? end : branch.points.back());
        layout.branchPathPoints.push_back(std::move(branch));
    }

    const int warpAnchorCount = context.roguelike ? 0 : std::clamp(context.warpPointCount, 0, 8);
    layout.warpPointAnchors.reserve(static_cast<std::size_t>(warpAnchorCount));
    for (int i = 0; i < warpAnchorCount; ++i) {
        layout.warpPointAnchors.push_back(pointAtProgress(
            layout.mainPathPoints,
            static_cast<float>(i + 1) / static_cast<float>(warpAnchorCount + 1)));
    }

    std::vector<std::pair<Vec2, float>> typedCandidates;
    typedCandidates.reserve(roomCandidates.size() + 8);
    for (std::size_t i = 0; i < roomCandidates.size(); ++i) {
        const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(layout, roomCandidates[i]);
        typedCandidates.push_back({roomCandidates[i], metrics.pathProgress});
    }
    for (float progress : {0.18f, 0.33f, 0.48f, 0.63f, 0.78f, 0.88f}) {
        const Vec2 anchor = pointAtProgress(layout.mainPathPoints, progress);
        const Vec2 tangent = tangentAtProgress(layout.mainPathPoints, progress);
        const Vec2 side = perpendicular(tangent) * (unitDist(rng) < 0.5f ? -1.0f : 1.0f);
        typedCandidates.push_back({anchor + side * (8.0f + unitDist(rng) * 8.0f), progress});
    }

    const std::array<SpecialRoomType, 5> roomSequence{
        SpecialRoomType::OreRoom,
        SpecialRoomType::SafeCavern,
        SpecialRoomType::CoinRoom,
        SpecialRoomType::TreasureRoom,
        SpecialRoomType::EnemyRoom,
    };
    int specialRoomCount = std::clamp(context.specialRoomCount, 0, 24);
    if (context.roguelike || isProfile(layout.generationProfile, "astral_rogue")) {
        specialRoomCount = std::max(5, specialRoomCount);
    }
    const float roomRadiusBonus = roomRadiusBonusForProfile(layout.generationProfile);
    for (int roomIndex = 0; roomIndex < specialRoomCount && !typedCandidates.empty(); ++roomIndex) {
        const std::size_t candidateIndex = static_cast<std::size_t>(roomIndex) % typedCandidates.size();
        const std::size_t roomTypeIndex = static_cast<std::size_t>(roomIndex) % roomSequence.size();
        const Vec2 center = typedCandidates[candidateIndex].first;
        const float progress = typedCandidates[candidateIndex].second;
        const float radius = 5.2f + unitDist(rng) * 2.3f + roomRadiusBonus + (progress > 0.75f ? 1.0f : 0.0f);
        if (roomIndex >= static_cast<int>(roomSequence.size()) && overlapsExistingRoom(layout.specialRoomAnchors, center, radius)) {
            continue;
        }
        SpecialRoomAnchor room;
        room.type = roomIndex < static_cast<int>(roomSequence.size())
            ? roomSequence[roomTypeIndex]
            : chooseExtraRoomType(layout.stageId, layout.generationProfile, roomIndex, progress);
        room.center = center;
        room.radius = std::max(3.2f, radius);
        layout.specialRoomAnchors.push_back(room);
    }
    for (int retry = 0; static_cast<int>(layout.specialRoomAnchors.size()) < specialRoomCount && retry < specialRoomCount * 2; ++retry) {
        const float progress = clamp(
            static_cast<float>(layout.specialRoomAnchors.size() + 1) / static_cast<float>(specialRoomCount + 1),
            0.12f,
            0.90f);
        const Vec2 anchor = pointAtProgress(layout.mainPathPoints, progress);
        const Vec2 tangent = tangentAtProgress(layout.mainPathPoints, progress);
        const Vec2 side = perpendicular(tangent) * (unitDist(rng) < 0.5f ? -1.0f : 1.0f);
        const Vec2 center = anchor + side * (9.0f + unitDist(rng) * 12.0f);
        const float radius = 4.8f + unitDist(rng) * 2.6f + roomRadiusBonus;
        if (overlapsExistingRoom(layout.specialRoomAnchors, center, radius)) {
            continue;
        }
        layout.specialRoomAnchors.push_back(SpecialRoomAnchor{
            .type = chooseExtraRoomType(layout.stageId, layout.generationProfile, retry, progress),
            .center = center,
            .radius = std::max(3.2f, radius),
        });
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

SpecialRoomMetrics calculateSpecialRoomMetrics(const DungeonLayout& layout, Vec2 tilePosition)
{
    SpecialRoomMetrics metrics;
    metrics.distanceToNearestRoom = 1.0e30f;
    for (const SpecialRoomAnchor& room : layout.specialRoomAnchors) {
        const float distanceToCenter = length(tilePosition - room.center);
        const float edgeDistance = std::max(0.0f, distanceToCenter - room.radius);
        if (distanceToCenter <= room.radius) {
            metrics.currentRoomType = room.type;
        }
        if (edgeDistance < metrics.distanceToNearestRoom) {
            metrics.distanceToNearestRoom = edgeDistance;
            metrics.nearestRoomType = room.type;
        }
    }
    if (layout.specialRoomAnchors.empty()) {
        metrics.distanceToNearestRoom = 0.0f;
    }
    return metrics;
}

const char* specialRoomTypeName(SpecialRoomType type)
{
    switch (type) {
    case SpecialRoomType::None:
        return "None";
    case SpecialRoomType::OreRoom:
        return "OreRoom";
    case SpecialRoomType::SafeCavern:
        return "SafeCavern";
    case SpecialRoomType::CoinRoom:
        return "CoinRoom";
    case SpecialRoomType::TreasureRoom:
        return "TreasureRoom";
    case SpecialRoomType::EnemyRoom:
        return "EnemyRoom";
    }
    return "Unknown";
}

}
