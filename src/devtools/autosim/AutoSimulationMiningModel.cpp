#include "devtools/autosim/AutoSimulationMiningModel.hpp"

#include "data/GameBalance.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace majo::autosim {

namespace {

constexpr float TileHalfSize = static_cast<float>(balance::TileSize) * 0.5f;
constexpr float ForwardProbeMin = 8.0f;
constexpr float ForwardProbeMax = 240.0f;
constexpr float WallLineWidth = 82.0f;
constexpr float MiningRangeSlack = 10.0f;
constexpr float MovementProbeDistance = 132.0f;
constexpr float MovementProbeExtraRadius = 6.0f;
constexpr float BlockedStandPenalty = 140.0f;

struct MiningCandidate {
    const GameTestMineTileSnapshot* tile = nullptr;
    Vec2 faceNormal{1.0f, 0.0f};
    Vec2 surfacePoint{};
    Vec2 standPoint{};
    float desiredFootRange = 0.0f;
    float score = 0.0f;
};

struct MovementBlock {
    bool blocking = false;
    float enter = 1.0f;
};

float cross(Vec2 a, Vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

bool sameTile(const GameTestMineTileSnapshot& a, const GameTestMineTileSnapshot& b)
{
    return a.tileX == b.tileX && a.tileY == b.tileY;
}

bool circleIntersectsTile(Vec2 center, float radius, const GameTestMineTileSnapshot& tile)
{
    const float minX = tile.center.x - TileHalfSize;
    const float maxX = tile.center.x + TileHalfSize;
    const float minY = tile.center.y - TileHalfSize;
    const float maxY = tile.center.y + TileHalfSize;
    const float nearestX = std::clamp(center.x, minX, maxX);
    const float nearestY = std::clamp(center.y, minY, maxY);
    return distanceSquared(center, {nearestX, nearestY}) <= radius * radius;
}

bool standPointBlocked(
    const GameTestSnapshot& snapshot,
    const GameTestMineTileSnapshot& target,
    Vec2 standPoint)
{
    const float playerRadius = static_cast<float>(balance::PlayerRadius) + 2.0f;
    for (const GameTestMineTileSnapshot& tile : snapshot.nearbyMineTiles) {
        if (!tile.solid || sameTile(tile, target)) {
            continue;
        }
        if (circleIntersectsTile(standPoint, playerRadius, tile)) {
            return true;
        }
    }
    return false;
}

bool segmentIntersectsExpandedTile(
    Vec2 start,
    Vec2 end,
    const GameTestMineTileSnapshot& tile,
    float expansion,
    float& outEnter)
{
    const Vec2 delta = end - start;
    const float minX = tile.center.x - TileHalfSize - expansion;
    const float maxX = tile.center.x + TileHalfSize + expansion;
    const float minY = tile.center.y - TileHalfSize - expansion;
    const float maxY = tile.center.y + TileHalfSize + expansion;
    float enter = 0.0f;
    float exit = 1.0f;

    const auto clipAxis = [&](float origin, float direction, float minValue, float maxValue) {
        if (std::abs(direction) <= 0.0001f) {
            return origin >= minValue && origin <= maxValue;
        }
        float t1 = (minValue - origin) / direction;
        float t2 = (maxValue - origin) / direction;
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        enter = std::max(enter, t1);
        exit = std::min(exit, t2);
        return enter <= exit;
    };

    if (!clipAxis(start.x, delta.x, minX, maxX) ||
        !clipAxis(start.y, delta.y, minY, maxY) ||
        exit < 0.0f ||
        enter > 1.0f) {
        return false;
    }

    outEnter = std::clamp(enter, 0.0f, 1.0f);
    return true;
}

MovementBlock movementBlockForTile(
    const GameTestSnapshot& snapshot,
    const GameTestMineTileSnapshot& tile,
    Vec2 direction)
{
    const Vec2 end = snapshot.player.position + direction * MovementProbeDistance;
    float enter = 1.0f;
    const float expansion = static_cast<float>(balance::PlayerRadius) + MovementProbeExtraRadius;
    if (!segmentIntersectsExpandedTile(snapshot.player.position, end, tile, expansion, enter)) {
        return {};
    }
    return MovementBlock{true, enter};
}

float miningSurfaceDistance(const GameTestSnapshot& snapshot)
{
    const float ringRadius = std::max(36.0f, snapshot.ring.activeRadius);
    const float hitRadius = std::max(8.0f, snapshot.ring.bestHitRadius);
    return std::clamp(ringRadius - hitRadius * 0.35f, 32.0f, 92.0f);
}

float miningOffsetDistance(const GameTestSnapshot& snapshot)
{
    if (!snapshot.ring.hasDigTool) {
        return 0.0f;
    }
    return snapshot.ring.maxOffsetDistance > 0.0f
        ? snapshot.ring.maxOffsetDistance
        : balance::SpellRingShiftDistance;
}

Vec2 travelDirection(const GameTestSnapshot& snapshot, Vec2 travelTarget)
{
    const Vec2 toTravelTarget = travelTarget - snapshot.player.position;
    if (lengthSquared(toTravelTarget) > 0.0001f) {
        return normalize(toTravelTarget);
    }
    return normalize(snapshot.player.facing);
}

Vec2 bestFaceNormal(const GameTestSnapshot& snapshot, const GameTestMineTileSnapshot& tile, Vec2 direction)
{
    const Vec2 normals[] = {
        {1.0f, 0.0f},
        {-1.0f, 0.0f},
        {0.0f, 1.0f},
        {0.0f, -1.0f},
    };
    const Vec2 toPlayer = snapshot.player.position - tile.center;
    const Vec2 playerDirection = lengthSquared(toPlayer) > 0.0001f
        ? normalize(toPlayer)
        : direction * -1.0f;

    Vec2 bestNormal = normals[0];
    float bestScore = std::numeric_limits<float>::lowest();
    for (Vec2 normal : normals) {
        const Vec2 surfacePoint = tile.center + normal * TileHalfSize;
        const float score =
            dot(normal, playerDirection) * 4.0f +
            dot(normal, direction * -1.0f) -
            distanceSquared(snapshot.player.position, surfacePoint) * 0.0002f;
        if (score > bestScore) {
            bestScore = score;
            bestNormal = normal;
        }
    }
    return bestNormal;
}

float terrainKindCost(GameTestTerrainKind kind)
{
    switch (kind) {
    case GameTestTerrainKind::Dirt: return -120.0f;
    case GameTestTerrainKind::Rock: return 28.0f;
    case GameTestTerrainKind::Ore: return 12.0f;
    case GameTestTerrainKind::HardRock: return 170.0f;
    case GameTestTerrainKind::Empty: return 0.0f;
    }
    return 0.0f;
}

float terrainAttributeCost(GameTestTerrainAttribute attribute)
{
    switch (attribute) {
    case GameTestTerrainAttribute::Soft: return -30.0f;
    case GameTestTerrainAttribute::Hard: return 34.0f;
    case GameTestTerrainAttribute::Ore: return -8.0f;
    case GameTestTerrainAttribute::None: return 0.0f;
    }
    return 0.0f;
}

bool reasonContains(std::string_view reason, std::string_view text)
{
    return reason.find(text) != std::string_view::npos;
}

bool preferSoftDigReason(std::string_view reason)
{
    return reasonContains(reason, "explore") ||
        reasonContains(reason, "main_path") ||
        reasonContains(reason, "map_clue");
}

float softDigTerrainBias(std::string_view reason, const GameTestMineTileSnapshot& tile)
{
    if (!preferSoftDigReason(reason)) {
        return 0.0f;
    }

    switch (tile.terrainKind) {
    case GameTestTerrainKind::Dirt:
        return tile.terrainAttribute == GameTestTerrainAttribute::Soft ? -120.0f : -70.0f;
    case GameTestTerrainKind::Ore:
        return -10.0f;
    case GameTestTerrainKind::Rock:
        return 180.0f;
    case GameTestTerrainKind::HardRock:
        return 620.0f;
    case GameTestTerrainKind::Empty:
        break;
    }
    return 0.0f;
}

float expectedBreakHits(const GameTestSnapshot& snapshot, const GameTestMineTileSnapshot& tile)
{
    const int remainingHp = std::max(1, tile.hp > 0 ? tile.hp : tile.effectiveHp);
    const int digPower = std::max(1, snapshot.ring.bestDigPower);
    return std::ceil(static_cast<float>(remainingHp) / static_cast<float>(digPower));
}

MiningCandidate miningCandidateForTile(
    const GameTestSnapshot& snapshot,
    const GameTestMineTileSnapshot& tile,
    Vec2 travelTarget)
{
    const Vec2 direction = travelDirection(snapshot, travelTarget);
    const float ringCenterSurfaceDistance = miningSurfaceDistance(snapshot);
    const float offsetDistance = miningOffsetDistance(snapshot);
    const float playerSurfaceDistance = ringCenterSurfaceDistance + offsetDistance;
    const Vec2 faceNormal = bestFaceNormal(snapshot, tile, direction);
    const Vec2 surfacePoint = tile.center + faceNormal * TileHalfSize;
    const Vec2 standPoint =
        surfacePoint +
        faceNormal * playerSurfaceDistance -
        snapshot.ring.anchorOffsetFromPlayer;

    MiningCandidate candidate;
    candidate.tile = &tile;
    candidate.faceNormal = faceNormal;
    candidate.surfacePoint = surfacePoint;
    candidate.standPoint = standPoint;
    candidate.desiredFootRange = length(standPoint - surfacePoint);
    return candidate;
}

std::optional<MiningCandidate> bestBlockingTile(
    const GameTestSnapshot& snapshot,
    Vec2 travelTarget,
    std::string_view reason)
{
    const Vec2 direction = travelDirection(snapshot, travelTarget);

    std::optional<MiningCandidate> best;
    for (const GameTestMineTileSnapshot& tile : snapshot.nearbyMineTiles) {
        if (!tile.diggable) {
            continue;
        }
        const MovementBlock movementBlock = movementBlockForTile(snapshot, tile, direction);
        const Vec2 toTile = tile.center - snapshot.player.position;
        const float forward = dot(toTile, direction);
        if (!movementBlock.blocking && (forward < ForwardProbeMin || forward > ForwardProbeMax)) {
            continue;
        }
        const float lateral = std::abs(cross(direction, toTile));
        if (!movementBlock.blocking && lateral > WallLineWidth) {
            continue;
        }

        MiningCandidate candidate = miningCandidateForTile(snapshot, tile, travelTarget);
        candidate.score =
            terrainKindCost(tile.terrainKind) +
            terrainAttributeCost(tile.terrainAttribute) +
            softDigTerrainBias(reason, tile) +
            expectedBreakHits(snapshot, tile) * 22.0f +
            std::max(0.0f, tile.localHardnessMultiplier - 1.0f) * 42.0f +
            std::clamp(tile.distanceFromMainPath, 0.0f, 8.0f) * 3.0f +
            (movementBlock.blocking ? -190.0f + movementBlock.enter * 75.0f : lateral * 1.35f) +
            forward * 0.18f +
            (standPointBlocked(snapshot, tile, candidate.standPoint) ? BlockedStandPenalty : 0.0f) +
            length(candidate.standPoint - snapshot.player.position) * 0.08f;

        if (!best || candidate.score < best->score) {
            best = candidate;
        }
    }
    return best;
}

AutoSimulationPlan planForCandidate(
    const GameTestSnapshot& snapshot,
    const MiningCandidate& candidate,
    std::string reason)
{
    AutoSimulationPlan plan;
    plan.goal = AutoSimulationGoal::MineWall;
    plan.targetWorld = candidate.tile->center;
    plan.moveTargetWorld = candidate.standPoint;
    plan.aimTargetWorld = candidate.surfacePoint;
    plan.hasTarget = true;
    plan.hasMoveTarget = true;
    plan.hasAimTarget = true;
    plan.rangeControl = true;
    plan.alignMoveTargetInRange = true;
    plan.moveTargetArriveDistance = 8.0f;
    plan.desiredRangeMin = std::max(18.0f, candidate.desiredFootRange - MiningRangeSlack);
    plan.desiredRangeMax = candidate.desiredFootRange + MiningRangeSlack;
    plan.strafe = false;
    plan.preferredRingRole = AutoSimulationRingRole::Dig;
    plan.throwRing = false;
    plan.ringOffset = snapshot.ring.hasDigTool;
    plan.ringOffsetRequiresMoveTarget = true;
    plan.ringOffsetMoveTargetDistance = 18.0f;
    plan.targetTerrainKind = candidate.tile->terrainKind;
    plan.hasTargetTerrainKind = true;
    plan.reason = std::move(reason) + "_mine_wall";
    return plan;
}

} // namespace

std::optional<AutoSimulationPlan> AutoSimulationMiningModel::makePlan(
    const GameTestSnapshot& snapshot,
    Vec2 travelTarget,
    std::string reason) const
{
    const std::optional<MiningCandidate> candidate = bestBlockingTile(snapshot, travelTarget, reason);
    if (!candidate) {
        return std::nullopt;
    }

    return planForCandidate(snapshot, *candidate, std::move(reason));
}

std::optional<AutoSimulationPlan> AutoSimulationMiningModel::makePlanForTile(
    const GameTestSnapshot& snapshot,
    const GameTestMineTileSnapshot& tile,
    Vec2 travelTarget,
    std::string reason) const
{
    if (!tile.diggable) {
        return std::nullopt;
    }

    MiningCandidate candidate = miningCandidateForTile(snapshot, tile, travelTarget);
    return planForCandidate(snapshot, candidate, std::move(reason));
}

} // namespace majo::autosim
