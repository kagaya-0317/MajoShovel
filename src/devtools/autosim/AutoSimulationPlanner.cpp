#include "devtools/autosim/AutoSimulationPlanner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

namespace majo::autosim {

namespace {

constexpr float DropAcquireRadius = 420.0f;
constexpr float ChestAcquireRadius = 420.0f;
constexpr float UnlimitedTargetRadius = 100000.0f;
constexpr float RouteActionDistance = 150.0f;
constexpr int MainPathLookAheadPoints = 3;

template <typename T, typename PositionFn, typename AcceptFn>
const T* bestAcceptedByRoute(
    const std::vector<T>& values,
    const AutoSimulationPathfinder& pathfinder,
    const AutoSimulationPathField& pathField,
    Vec2 origin,
    float maxRadius,
    PositionFn position,
    AcceptFn accept)
{
    const T* best = nullptr;
    float bestScore = std::numeric_limits<float>::max();
    for (const T& value : values) {
        if (!accept(value)) {
            continue;
        }
        const Vec2 target = position(value);
        if (maxRadius < UnlimitedTargetRadius && distanceSquared(origin, target) > maxRadius * maxRadius) {
            continue;
        }

        float score = distanceSquared(origin, target);
        if (pathField.valid()) {
            const std::optional<AutoSimulationRoute> route = pathfinder.findRoute(pathField, target);
            if (!route) {
                continue;
            }
            score = route->totalCost;
        }

        if (score < bestScore) {
            bestScore = score;
            best = &value;
        }
    }
    return best;
}

Vec2 mainPathTarget(const GameTestSnapshot& snapshot)
{
    const std::vector<Vec2>& points = snapshot.dungeon.mainPathWorldPoints;
    if (points.empty()) {
        return snapshot.dungeon.goalWorld;
    }

    int nearestIndex = 0;
    float nearestDistanceSq = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        const float distSq = distanceSquared(snapshot.player.position, points[static_cast<std::size_t>(i)]);
        if (distSq < nearestDistanceSq) {
            nearestDistanceSq = distSq;
            nearestIndex = i;
        }
    }

    const int targetIndex = std::min(
        static_cast<int>(points.size()) - 1,
        nearestIndex + MainPathLookAheadPoints);
    return points[static_cast<std::size_t>(targetIndex)];
}

bool backpackFull(const GameTestSnapshot& snapshot)
{
    return snapshot.inventory.backpackCapacity > 0 &&
        snapshot.inventory.backpackUsedSlots >= snapshot.inventory.backpackCapacity;
}

int knownWarpCount(const GameTestSnapshot& snapshot)
{
    return std::max(snapshot.dungeon.discoveredWarpPoints, snapshot.dungeon.unlockedWarpPoints);
}

bool hasUnknownWarpPoint(const GameTestSnapshot& snapshot)
{
    if (snapshot.dungeon.warpPoints.empty()) {
        return false;
    }
    return knownWarpCount(snapshot) < static_cast<int>(snapshot.dungeon.warpPoints.size());
}

Vec2 escapeTarget(const GameTestSnapshot& snapshot, const AutoSimulationPathField& pathField)
{
    const Vec2 fallback = snapshot.player.position + normalize(snapshot.player.facing) * 180.0f;
    if (!pathField.valid()) {
        return fallback;
    }

    Vec2 best = fallback;
    float bestScore = -std::numeric_limits<float>::max();
    const Vec2 facing = normalize(snapshot.player.facing);
    for (const AutoSimulationPathCell& cell : pathField.cells) {
        if (!std::isfinite(cell.cost) || cell.tile.solid) {
            continue;
        }

        const Vec2 offset = cell.tile.center - snapshot.player.position;
        const float distance = length(offset);
        if (distance < 72.0f || distance > 220.0f) {
            continue;
        }

        const Vec2 direction = distance > 0.0001f ? offset * (1.0f / distance) : Vec2{};
        const float awayFromCurrentFacing = -(direction.x * facing.x + direction.y * facing.y);
        const float score = distance - cell.cost * 5.0f + awayFromCurrentFacing * 28.0f;
        if (score > bestScore) {
            bestScore = score;
            best = cell.tile.center;
        }
    }
    return best;
}

std::string pathReason(std::string reason, const AutoSimulationRoute& route)
{
    reason += "_path";
    if (route.digTileCount > 0) {
        reason += "_dig";
        reason += std::to_string(route.digTileCount);
    }
    return reason;
}

void applyRouteMetadata(AutoSimulationPlan& plan, const AutoSimulationRoute& route)
{
    plan.routeDigTileCount = route.digTileCount;
    plan.routeHardTileCount = route.hardTileCount;
    plan.routeAvoidingHardWall = route.avoidingHardWall;
    if (route.hasFirstDigTerrainKind) {
        plan.targetTerrainKind = route.firstDigTerrainKind;
        plan.hasTargetTerrainKind = true;
    }
}

} // namespace

bool AutoSimulationPlanner::needsConfirmInput(const GameTestSnapshot& snapshot)
{
    if (snapshot.dialogueActive || snapshot.firstItemNoticeActive) {
        return true;
    }
    return snapshot.screenMode == GameTestScreenMode::LevelUp ||
        snapshot.screenMode == GameTestScreenMode::OpeningKamishibai ||
        snapshot.screenMode == GameTestScreenMode::EndingKamishibai;
}

AutoSimulationPlan AutoSimulationPlanner::makeTargetPlan(
    AutoSimulationGoal goal,
    Vec2 target,
    std::string reason,
    bool throwRing,
    bool ringOffset,
    bool moveAwayFromTarget)
{
    AutoSimulationPlan plan;
    plan.goal = goal;
    plan.targetWorld = target;
    plan.moveTargetWorld = target;
    plan.aimTargetWorld = target;
    plan.hasTarget = true;
    plan.hasMoveTarget = true;
    plan.hasAimTarget = true;
    plan.reason = std::move(reason);
    plan.throwRing = throwRing;
    plan.ringOffset = ringOffset;
    plan.moveAwayFromTarget = moveAwayFromTarget;
    return plan;
}

AutoSimulationPlan AutoSimulationPlanner::makeTravelPlan(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathField& pathField,
    AutoSimulationGoal goal,
    Vec2 target,
    std::string reason,
    bool throwRing,
    bool ringOffset) const
{
    (void)ringOffset;
    if (pathField.valid()) {
        if (std::optional<AutoSimulationRoute> route = pathfinder_.findRoute(pathField, target)) {
            const std::string routeReason = pathReason(reason, *route);
            if (route->nextDigTile) {
                if (std::optional<AutoSimulationPlan> miningPlan = miningModel_.makePlanForTile(
                    snapshot,
                    *route->nextDigTile,
                    route->nextWaypointWorld,
                    routeReason)) {
                    applyRouteMetadata(*miningPlan, *route);
                    miningPlan->targetTerrainKind = route->nextDigTile->terrainKind;
                    miningPlan->hasTargetTerrainKind = true;
                    return *miningPlan;
                }
            }

            AutoSimulationPlan plan = makeTargetPlan(goal, target, routeReason, false, false);
            plan.moveTargetWorld = route->nextWaypointWorld;
            plan.hasMoveTarget = true;
            plan.aimTargetWorld = target;
            plan.hasAimTarget = true;
            plan.throwRing = throwRing &&
                distanceSquared(snapshot.player.position, target) <= RouteActionDistance * RouteActionDistance;
            plan.ringOffset = false;
            applyRouteMetadata(plan, *route);
            return plan;
        }
    }

    if (std::optional<AutoSimulationPlan> miningPlan = miningModel_.makePlan(snapshot, target, reason)) {
        return *miningPlan;
    }
    return makeTargetPlan(goal, target, std::move(reason), throwRing, false);
}

AutoSimulationPlan AutoSimulationPlanner::makePlan(const GameTestSnapshot& snapshot, bool escapeStuck) const
{
    if (snapshot.worldLoading || snapshot.transitionActive) {
        return {};
    }
    if (needsConfirmInput(snapshot)) {
        AutoSimulationPlan plan;
        plan.goal = AutoSimulationGoal::DismissUi;
        plan.confirm = true;
        plan.reason = "ui";
        return plan;
    }
    if (snapshot.screenMode != GameTestScreenMode::Playing) {
        return {};
    }
    const AutoSimulationPathField pathField = pathfinder_.buildField(snapshot);
    if (escapeStuck) {
        const Vec2 target = escapeTarget(snapshot, pathField);
        return makeTargetPlan(AutoSimulationGoal::EscapeStuck, target, "stuck", true, false, false);
    }

    if (backpackFull(snapshot)) {
        const GameTestWarpPointSnapshot* discoveredWarp = bestAcceptedByRoute(
            snapshot.dungeon.warpPoints,
            pathfinder_,
            pathField,
            snapshot.player.position,
            UnlimitedTargetRadius,
            [](const GameTestWarpPointSnapshot& value) { return value.position; },
            [](const GameTestWarpPointSnapshot& value) { return value.discovered; });
        if (discoveredWarp != nullptr) {
            return makeTravelPlan(snapshot, pathField, AutoSimulationGoal::ReturnToBase, discoveredWarp->position, "backpack_full_warp", false, true);
        }
        return makeTravelPlan(snapshot, pathField, AutoSimulationGoal::ReturnToBase, snapshot.dungeon.startWorld, "backpack_full_entrance", false, true);
    }

    if (std::optional<AutoSimulationPlan> combatPlan = combatModel_.makePlan(snapshot)) {
        return *combatPlan;
    }

    const GameTestDropSnapshot* drop = bestAcceptedByRoute(
        snapshot.drops,
        pathfinder_,
        pathField,
        snapshot.player.position,
        DropAcquireRadius,
        [](const GameTestDropSnapshot& value) { return value.position; },
        [](const GameTestDropSnapshot&) { return true; });
    if (drop != nullptr) {
        return makeTravelPlan(snapshot, pathField, AutoSimulationGoal::CollectDrop, drop->position, "drop");
    }

    const GameTestChestSnapshot* chest = bestAcceptedByRoute(
        snapshot.chests,
        pathfinder_,
        pathField,
        snapshot.player.position,
        ChestAcquireRadius,
        [](const GameTestChestSnapshot& value) { return value.position; },
        [](const GameTestChestSnapshot& value) { return value.revealed && !value.opened; });
    if (chest != nullptr) {
        return makeTravelPlan(snapshot, pathField, AutoSimulationGoal::OpenChest, chest->position, "chest", true, false);
    }

    if (hasUnknownWarpPoint(snapshot)) {
        if (std::optional<AutoSimulationExplorationTarget> target =
                explorationModel_.chooseTarget(snapshot, pathField)) {
            return makeTravelPlan(
                snapshot,
                pathField,
                AutoSimulationGoal::DiscoverWarp,
                target->world,
                target->reason,
                false,
                true);
        }
    }

    if (snapshot.dungeon.hasBossSpawnPoint && !snapshot.dungeon.bossSpawned) {
        return makeTravelPlan(snapshot, pathField, AutoSimulationGoal::ApproachBoss, snapshot.dungeon.bossSpawnPoint, "boss_spawn", false, true);
    }

    return makeTravelPlan(snapshot, pathField, AutoSimulationGoal::FollowMainPath, mainPathTarget(snapshot), "main_path", false, true);
}

} // namespace majo::autosim
