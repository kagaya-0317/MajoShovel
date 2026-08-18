#include "devtools/autosim/AutoSimulationPlanner.hpp"

#include "devtools/autosim/AutoSimulationConsumablePlanner.hpp"
#include "devtools/autosim/AutoSimulationItemEvaluator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace majo::autosim {

namespace {

constexpr float DropAcquireRadius = 900.0f;
constexpr float EmergencyDropAcquireRadius = 900.0f;
constexpr float ChestAcquireRadius = 420.0f;
constexpr float UnlimitedTargetRadius = 100000.0f;
constexpr float RouteActionDistance = 150.0f;
constexpr float RouteWaypointArriveDistance = 14.0f;
constexpr float RouteStartWaypointArriveDistance = 6.0f;
constexpr float PreciseWarpEntryDistance = 96.0f;
constexpr float PreciseWarpEntryArriveDistance = 8.0f;
constexpr int MainPathLookAheadPoints = 3;
constexpr float OpportunisticPickupRouteCost = 18.0f;
constexpr float OpportunisticPickupBonus = 92.0f;
constexpr float OpportunisticPickupEnemyClearance = 180.0f;
constexpr float CheapDigMaxExpectedHits = 8.0f;
constexpr float CheapRockMaxExpectedHits = 3.0f;
constexpr float CheapDigMaxCost = 92.0f;

AutoSimulationGoal objectiveGoal(const AutoSimulationPlan& plan)
{
    return plan.objectiveGoal != AutoSimulationGoal::None
        ? plan.objectiveGoal
        : plan.goal;
}

AutoSimulationDigPolicy digPolicyForGoal(AutoSimulationGoal goal)
{
    switch (goal) {
    case AutoSimulationGoal::CollectDrop:
    case AutoSimulationGoal::OpenChest:
    case AutoSimulationGoal::DiscoverWarp:
        return AutoSimulationDigPolicy::CheapOnly;
    case AutoSimulationGoal::ReturnToBase:
    case AutoSimulationGoal::ApproachBoss:
    case AutoSimulationGoal::FollowMainPath:
        return AutoSimulationDigPolicy::Required;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
    case AutoSimulationGoal::UseItem:
    case AutoSimulationGoal::MineWall:
    case AutoSimulationGoal::Combat:
    case AutoSimulationGoal::EscapeStuck:
    case AutoSimulationGoal::ResumeFrontier:
        break;
    }
    return AutoSimulationDigPolicy::Avoid;
}

bool routeAllowedByDigPolicy(const AutoSimulationRoute& route, AutoSimulationDigPolicy policy)
{
    if (route.digTileCount <= 0) {
        return true;
    }
    if (policy == AutoSimulationDigPolicy::Required) {
        return true;
    }
    if (policy == AutoSimulationDigPolicy::Avoid) {
        return false;
    }
    if (route.expectedDigHits > CheapDigMaxExpectedHits || route.digCost > CheapDigMaxCost) {
        return false;
    }
    if (route.hasFirstDigTerrainKind) {
        if (route.firstDigTerrainKind == GameTestTerrainKind::HardRock) {
            return false;
        }
        if (route.firstDigTerrainKind == GameTestTerrainKind::Rock &&
            route.expectedDigHits > CheapRockMaxExpectedHits) {
            return false;
        }
    }
    return true;
}

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

bool knownWarpDiscovered(const GameTestSnapshot& snapshot, const GameTestWarpPointSnapshot& point)
{
    return point.discovered || point.unlocked || point.index < knownWarpCount(snapshot);
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

bool preciseWarpEntryGoal(AutoSimulationGoal goal, const std::string& reason)
{
    return (goal == AutoSimulationGoal::ReturnToBase || goal == AutoSimulationGoal::DiscoverWarp) &&
        reason.find("warp") != std::string::npos;
}

void applyRouteMetadata(AutoSimulationPlan& plan, const AutoSimulationRoute& route)
{
    plan.routePathTileCount = route.pathTileCount;
    plan.routeWaypointPathIndex = route.waypointPathIndex;
    plan.routeFirstDigPathIndex = route.firstDigPathIndex;
    plan.routeDigTileCount = route.digTileCount;
    plan.routeHardTileCount = route.hardTileCount;
    plan.routeAvoidingHardWall = route.avoidingHardWall;
    plan.routeTotalCost = route.totalCost;
    plan.routeDigCost = route.digCost;
    plan.routeExpectedDigHits = route.expectedDigHits;
    if (route.hasFirstDigTerrainKind) {
        plan.targetTerrainKind = route.firstDigTerrainKind;
        plan.hasTargetTerrainKind = true;
    }
}

struct ScoredPlan {
    AutoSimulationPlan plan;
    float score = -std::numeric_limits<float>::max();
};

float hpRatio(const GameTestSnapshot& snapshot)
{
    if (snapshot.player.maxHp <= 0) {
        return 1.0f;
    }
    return std::clamp(
        static_cast<float>(snapshot.player.hp) / static_cast<float>(snapshot.player.maxHp),
        0.0f,
        1.0f);
}

bool backpackPressure(const GameTestSnapshot& snapshot)
{
    if (snapshot.inventory.backpackCapacity <= 0) {
        return false;
    }
    return snapshot.inventory.backpackUsedSlots >= snapshot.inventory.backpackCapacity - 2 ||
        static_cast<float>(snapshot.inventory.backpackUsedSlots) /
            static_cast<float>(snapshot.inventory.backpackCapacity) >= 0.78f;
}

float nearestEnemyDistance(const GameTestSnapshot& snapshot)
{
    float best = std::numeric_limits<float>::max();
    for (const GameTestEnemySnapshot& enemy : snapshot.enemies) {
        best = std::min(best, length(enemy.position - snapshot.player.position));
    }
    return best;
}

bool bossPresent(const GameTestSnapshot& snapshot)
{
    if (snapshot.dungeon.bossSpawned) {
        return true;
    }
    return std::any_of(snapshot.enemies.begin(), snapshot.enemies.end(), [](const GameTestEnemySnapshot& enemy) {
        return enemy.boss;
    });
}

bool dropRestoresMissingDig(const GameTestSnapshot& snapshot, const GameTestDropSnapshot& drop)
{
    return drop.kind == GameTestDropKind::Object &&
        drop.digPower > 0 &&
        (!snapshot.ring.hasDigTool || snapshot.ring.bestDigPower <= 0);
}

bool dropRestoresMissingCombat(const GameTestSnapshot& snapshot, const GameTestDropSnapshot& drop)
{
    return drop.kind == GameTestDropKind::Object &&
        drop.attackPower > 0 &&
        !snapshot.ring.hasCombatTool;
}

bool dropRestoresMissingLight(const GameTestSnapshot& snapshot, const GameTestDropSnapshot& drop)
{
    return drop.kind == GameTestDropKind::Object &&
        drop.lightRadius > 0.0f &&
        (!snapshot.ring.hasLightTool || snapshot.ring.bestLightRadius < 120.0f);
}

GameTestObjectEntrySnapshot objectEntryForDrop(const GameTestDropSnapshot& drop)
{
    GameTestObjectEntrySnapshot item;
    item.location = GameTestInventoryLocation::Backpack;
    item.kind = GameTestObjectEntryKind::Stack;
    item.objectId = drop.id;
    item.name = drop.displayName;
    item.category = drop.category;
    item.damageType = drop.damageType;
    item.tags = drop.tags;
    item.useEffects = drop.useEffects;
    item.count = 1;
    item.rarity = drop.rarity;
    item.price = drop.price;
    item.attackPower = drop.attackPower;
    item.digPower = drop.digPower;
    item.lightRadius = drop.lightRadius;
    item.durability = drop.durability;
    item.weightKg = drop.weightKg;
    item.currentDurability = drop.currentDurability;
    item.maxDurability = drop.maxDurability;
    item.broken = drop.broken;
    item.codexStage = GameTestCodexStage::Obtained;
    return item;
}

float backpackHealingReserve(const GameTestSnapshot& snapshot)
{
    double reserve = 0.0;
    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        const AutoSimulationConsumableProfile profile = autoSimulationConsumableProfile(item);
        if (!profile.unsafeSelfEffect && profile.heal > 0.0) {
            reserve += profile.heal * static_cast<double>(std::max(1, item.count));
        }
    }
    return static_cast<float>(reserve);
}

bool hasBackpackDigReserve(const GameTestSnapshot& snapshot)
{
    return std::any_of(
        snapshot.inventory.backpackItems.begin(),
        snapshot.inventory.backpackItems.end(),
        [](const GameTestObjectEntrySnapshot& item) {
            return !item.broken && item.digPower > 0;
        });
}

bool activeDigToolNeedsReserve(const GameTestSnapshot& snapshot)
{
    bool foundActiveDigTool = false;
    for (const GameTestRingItemSnapshot& item : snapshot.ring.items) {
        if (item.ringIndex != snapshot.ring.activeRingIndex || item.broken || item.digPower <= 0) {
            continue;
        }
        foundActiveDigTool = true;
        if (item.maxDurability > 0 && item.durability >= 0 &&
            static_cast<float>(item.durability) / static_cast<float>(item.maxDurability) <= 0.35f) {
            return true;
        }
    }
    return !foundActiveDigTool;
}

float objectDropAcquireScore(
    const GameTestSnapshot& snapshot,
    const GameTestDropSnapshot& drop,
    const AutoSimulationItemEvaluator& evaluator,
    const AutoSimulationItemEvaluationContext& itemContext)
{
    if (!drop.canAcquire) {
        return -std::numeric_limits<float>::max();
    }

    const GameTestObjectEntrySnapshot entry = objectEntryForDrop(drop);
    const AutoSimulationItemScore itemScore = evaluator.evaluate(entry, itemContext);
    float score = 48.0f;
    score += std::min(78.0f, itemScore.keep * 0.48f);
    score += std::min(92.0f, itemScore.loadout * 0.66f);
    score += std::min(42.0f, itemScore.investment * 0.20f);

    const AutoSimulationConsumableProfile consumable = autoSimulationConsumableProfile(entry);
    if (!consumable.unsafeSelfEffect && consumable.heal > 0.0) {
        const float desiredReserve = static_cast<float>(std::max(1, snapshot.player.maxHp)) * 1.5f;
        const float reserveShortage = std::max(0.0f, desiredReserve - backpackHealingReserve(snapshot));
        score += std::min(72.0f, static_cast<float>(consumable.heal) * 0.65f);
        score += std::min(54.0f, reserveShortage * 0.45f);
    }

    if (drop.digPower > 0 &&
        (!hasBackpackDigReserve(snapshot) || activeDigToolNeedsReserve(snapshot))) {
        score += 96.0f + static_cast<float>(std::max(0, drop.digPower)) * 6.0f;
    }

    if (dropRestoresMissingDig(snapshot, drop)) {
        score += 210.0f + static_cast<float>(std::max(0, drop.digPower)) * 18.0f;
    }
    if (dropRestoresMissingCombat(snapshot, drop)) {
        score += 130.0f + static_cast<float>(std::max(0, drop.attackPower)) * 9.0f;
    }
    if (dropRestoresMissingLight(snapshot, drop)) {
        score += 118.0f + std::min(52.0f, drop.lightRadius * 0.20f);
    }
    if (drop.digPower > snapshot.ring.bestDigPower && snapshot.ring.bestDigPower > 0) {
        score += static_cast<float>(drop.digPower - snapshot.ring.bestDigPower) * 18.0f;
    }
    if (drop.attackPower > snapshot.ring.bestDamage && snapshot.ring.bestDamage > 0) {
        score += static_cast<float>(drop.attackPower - snapshot.ring.bestDamage) * 10.0f;
    }
    return score;
}

float dropAcquireScore(
    const GameTestSnapshot& snapshot,
    const GameTestDropSnapshot& drop,
    const AutoSimulationItemEvaluator& evaluator,
    const AutoSimulationItemEvaluationContext& itemContext)
{
    switch (drop.kind) {
    case GameTestDropKind::Object:
        return objectDropAcquireScore(snapshot, drop, evaluator, itemContext);
    case GameTestDropKind::Material:
        return 50.0f + std::min(26.0f, static_cast<float>(std::max(1, drop.quantity)) * 2.0f);
    case GameTestDropKind::Money:
        return 42.0f + std::min(34.0f, static_cast<float>(std::max(1, drop.quantity)) * 0.12f);
    }
    return 0.0f;
}

float dropAcquireRadius(const GameTestSnapshot& snapshot, const GameTestDropSnapshot& drop)
{
    if (dropRestoresMissingDig(snapshot, drop) ||
        dropRestoresMissingCombat(snapshot, drop) ||
        dropRestoresMissingLight(snapshot, drop)) {
        return EmergencyDropAcquireRadius;
    }
    return DropAcquireRadius;
}

std::string dropAcquireReason(const GameTestSnapshot& snapshot, const GameTestDropSnapshot& drop)
{
    std::string reason = "drop";
    if (drop.kind == GameTestDropKind::Object) {
        reason += "_object";
        if (dropRestoresMissingDig(snapshot, drop)) {
            reason += "_need_dig";
        } else if (dropRestoresMissingCombat(snapshot, drop)) {
            reason += "_need_combat";
        } else if (dropRestoresMissingLight(snapshot, drop)) {
            reason += "_need_light";
        }
        if (!drop.id.empty()) {
            reason += "_";
            reason += drop.id;
        }
    } else if (drop.kind == GameTestDropKind::Material) {
        reason += "_material";
    } else if (drop.kind == GameTestDropKind::Money) {
        reason += "_money";
    }
    return reason;
}

bool softDigPreferenceReason(const std::string& reason)
{
    return reason.find("explore") != std::string::npos ||
        reason.find("main_path") != std::string::npos ||
        reason.find("map_clue") != std::string::npos;
}

float softDigPreferencePenalty(const AutoSimulationPlan& plan)
{
    if (plan.goal != AutoSimulationGoal::MineWall ||
        !plan.hasTargetTerrainKind ||
        !softDigPreferenceReason(plan.reason)) {
        return 0.0f;
    }

    switch (plan.targetTerrainKind) {
    case GameTestTerrainKind::Dirt:
        return -55.0f;
    case GameTestTerrainKind::Ore:
        return -8.0f;
    case GameTestTerrainKind::Rock:
        return 260.0f;
    case GameTestTerrainKind::HardRock:
        return 900.0f;
    case GameTestTerrainKind::Empty:
        break;
    }
    return 0.0f;
}

float routePenalty(const AutoSimulationPlan& plan)
{
    if (plan.routeTotalCost > 0.0f) {
        return plan.routeTotalCost * 0.72f;
    }

    float penalty = 0.0f;
    penalty += static_cast<float>(std::max(0, plan.routePathTileCount)) * 0.85f;
    penalty += static_cast<float>(std::max(0, plan.routeDigTileCount)) * 15.0f;
    penalty += static_cast<float>(std::max(0, plan.routeHardTileCount)) * 58.0f;
    if (plan.routeAvoidingHardWall) {
        penalty -= 10.0f;
    }
    if (plan.goal == AutoSimulationGoal::MineWall &&
        plan.hasTargetTerrainKind &&
        plan.targetTerrainKind == GameTestTerrainKind::HardRock) {
        penalty += 24.0f;
    }
    penalty += softDigPreferencePenalty(plan);
    return penalty;
}

float distancePenalty(const GameTestSnapshot& snapshot, const AutoSimulationPlan& plan)
{
    if (plan.routeTotalCost > 0.0f) {
        return 0.0f;
    }
    if (!plan.hasTarget) {
        return 0.0f;
    }
    return length(plan.targetWorld - snapshot.player.position) * 0.025f;
}

float contextualGoalBonus(const GameTestSnapshot& snapshot, const AutoSimulationPlan& plan)
{
    const float hp = hpRatio(snapshot);
    const float enemyDistance = nearestEnemyDistance(snapshot);
    float bonus = 0.0f;
    const bool explicitBackpackFull = plan.reason.find("backpack_full") != std::string::npos;
    if (explicitBackpackFull) {
        bonus += 110.0f;
    } else if (plan.reason.find("low_hp") != std::string::npos) {
        bonus += hp <= 0.32f ? 58.0f : (hp <= 0.46f ? 24.0f : 0.0f);
    }
    if (hasUnknownWarpPoint(snapshot) &&
        (plan.reason.find("visible_warp") != std::string::npos ||
            plan.reason.find("map_clue") != std::string::npos ||
            plan.reason.find("explore_") != std::string::npos)) {
        bonus += 24.0f;
    }
    switch (objectiveGoal(plan)) {
    case AutoSimulationGoal::ReturnToBase:
        bonus += (!explicitBackpackFull && backpackFull(snapshot) ? 110.0f : 0.0f) +
            (hp <= 0.32f ? 58.0f : (hp <= 0.46f ? 24.0f : 0.0f));
        return bonus;
    case AutoSimulationGoal::Combat:
        bonus += (enemyDistance <= 90.0f ? 62.0f : (enemyDistance <= 180.0f ? 32.0f : 0.0f)) +
            (bossPresent(snapshot) ? 48.0f : 0.0f) +
            (hp <= 0.35f ? -26.0f : 0.0f);
        return bonus;
    case AutoSimulationGoal::CollectDrop:
        return bonus + (backpackPressure(snapshot) ? -24.0f : 0.0f);
    case AutoSimulationGoal::OpenChest:
        return bonus + (backpackPressure(snapshot) ? -18.0f : 0.0f);
    case AutoSimulationGoal::DiscoverWarp:
        bonus += (hasUnknownWarpPoint(snapshot) ? 26.0f : 0.0f) +
            (plan.reason.find("visible_warp") != std::string::npos ? 24.0f : 0.0f) +
            (plan.reason.find("map_clue") != std::string::npos ? 12.0f : 0.0f);
        return bonus;
    case AutoSimulationGoal::ApproachBoss:
        bonus += (snapshot.ring.hasCombatTool && hp >= 0.58f ? 28.0f : -18.0f) +
            (hasUnknownWarpPoint(snapshot) ? -16.0f : 12.0f);
        return bonus;
    case AutoSimulationGoal::FollowMainPath:
        return bonus + (hasUnknownWarpPoint(snapshot) ? 6.0f : 14.0f);
    case AutoSimulationGoal::ResumeFrontier:
        return bonus + 18.0f;
    case AutoSimulationGoal::MineWall:
        return bonus + (snapshot.ring.hasDigTool ? 8.0f : -18.0f);
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
    case AutoSimulationGoal::UseItem:
    case AutoSimulationGoal::EscapeStuck:
        break;
    }
    return bonus;
}

void keepBetterPlan(
    std::optional<ScoredPlan>& best,
    const GameTestSnapshot& snapshot,
    AutoSimulationPlan plan,
    float baseScore)
{
    if (plan.goal == AutoSimulationGoal::None) {
        return;
    }

    const float score =
        baseScore +
        contextualGoalBonus(snapshot, plan) -
        routePenalty(plan) -
        distancePenalty(snapshot, plan);
    const AutoSimulationGoal objective = objectiveGoal(plan);
    const bool requiredObjective =
        objective == AutoSimulationGoal::ReturnToBase ||
        objective == AutoSimulationGoal::Combat ||
        objective == AutoSimulationGoal::ApproachBoss ||
        objective == AutoSimulationGoal::FollowMainPath;
    if (!requiredObjective && score < 0.0f) {
        return;
    }
    if (!best || score > best->score) {
        best = ScoredPlan{std::move(plan), score};
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
    bool moveAwayFromTarget,
    AutoSimulationRingRole preferredRingRole)
{
    AutoSimulationPlan plan;
    plan.goal = goal;
    plan.objectiveGoal = goal;
    plan.targetWorld = target;
    plan.objectiveTargetWorld = target;
    plan.moveTargetWorld = target;
    plan.aimTargetWorld = target;
    plan.hasTarget = true;
    plan.hasObjectiveTarget = true;
    plan.hasMoveTarget = true;
    plan.hasAimTarget = true;
    plan.reason = std::move(reason);
    plan.throwRing = throwRing;
    plan.ringOffset = ringOffset;
    plan.moveAwayFromTarget = moveAwayFromTarget;
    plan.preferredRingRole = preferredRingRole;
    plan.digPolicy = digPolicyForGoal(goal);
    return plan;
}

AutoSimulationPlan AutoSimulationPlanner::makeTravelPlan(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathField& pathField,
    AutoSimulationGoal goal,
    Vec2 target,
    std::string reason,
    bool throwRing,
    bool ringOffset,
    AutoSimulationRingRole preferredRingRole,
    std::optional<AutoSimulationDigPolicy> digPolicyOverride) const
{
    (void)ringOffset;
    const bool preciseWarpEntry = preciseWarpEntryGoal(goal, reason);
    const AutoSimulationDigPolicy digPolicy = digPolicyOverride.value_or(digPolicyForGoal(goal));
    if (pathField.valid()) {
        if (std::optional<AutoSimulationRoute> route = pathfinder_.findRoute(pathField, target)) {
            if (!routeAllowedByDigPolicy(*route, digPolicy)) {
                return {};
            }
            const std::string routeReason = pathReason(reason, *route);
            if (route->nextDigTile) {
                if (std::optional<AutoSimulationPlan> miningPlan = miningModel_.makePlanForTile(
                    snapshot,
                    *route->nextDigTile,
                    route->nextWaypointWorld,
                    routeReason)) {
                    applyRouteMetadata(*miningPlan, *route);
                    miningPlan->objectiveGoal = goal;
                    miningPlan->objectiveTargetWorld = target;
                    miningPlan->hasObjectiveTarget = true;
                    miningPlan->digPolicy = digPolicy;
                    miningPlan->targetTerrainKind = route->nextDigTile->terrainKind;
                    miningPlan->hasTargetTerrainKind = true;
                    return *miningPlan;
                }
            }

            AutoSimulationPlan plan = makeTargetPlan(goal, target, routeReason, false, false, false, preferredRingRole);
            plan.moveTargetWorld = route->nextWaypointWorld;
            plan.hasMoveTarget = true;
            plan.aimTargetWorld = target;
            plan.hasAimTarget = true;
            if (route->pathTileCount > 1) {
                plan.moveTargetArriveDistance = route->waypointPathIndex == 0
                    ? RouteStartWaypointArriveDistance
                    : RouteWaypointArriveDistance;
            }
            if (preciseWarpEntry &&
                distanceSquared(snapshot.player.position, target) <= PreciseWarpEntryDistance * PreciseWarpEntryDistance &&
                pathfinder_.hasClearLine(pathField, snapshot.player.position, target)) {
                plan.moveTargetWorld = target;
                plan.moveTargetArriveDistance = PreciseWarpEntryArriveDistance;
            }
            plan.throwRing = throwRing &&
                distanceSquared(snapshot.player.position, target) <= RouteActionDistance * RouteActionDistance;
            plan.ringOffset = false;
            applyRouteMetadata(plan, *route);
            return plan;
        }
        if (digPolicy == AutoSimulationDigPolicy::Avoid) {
            return {};
        }
    }

    if (std::optional<AutoSimulationPlan> miningPlan = miningModel_.makePlan(snapshot, target, reason)) {
        if (digPolicy != AutoSimulationDigPolicy::Required &&
            miningPlan->hasTargetTerrainKind &&
            (miningPlan->targetTerrainKind == GameTestTerrainKind::Rock ||
                miningPlan->targetTerrainKind == GameTestTerrainKind::HardRock)) {
            return {};
        }
        miningPlan->objectiveGoal = goal;
        miningPlan->objectiveTargetWorld = target;
        miningPlan->hasObjectiveTarget = true;
        miningPlan->digPolicy = digPolicy;
        return *miningPlan;
    }
    return makeTargetPlan(goal, target, std::move(reason), throwRing, false, false, preferredRingRole);
}

AutoSimulationPlan AutoSimulationPlanner::makeDirectedPlan(
    const GameTestSnapshot& snapshot,
    AutoSimulationGoal goal,
    Vec2 target,
    std::string reason,
    AutoSimulationDigPolicy digPolicy,
    AutoSimulationRingRole preferredRingRole) const
{
    if (snapshot.worldLoading || snapshot.transitionActive || snapshot.screenMode != GameTestScreenMode::Playing) {
        return {};
    }
    const AutoSimulationPathField pathField = pathfinder_.buildField(snapshot, {digPolicy});
    return makeTravelPlan(
        snapshot,
        pathField,
        goal,
        target,
        std::move(reason),
        false,
        false,
        preferredRingRole,
        digPolicy);
}

AutoSimulationPlan AutoSimulationPlanner::makePlan(
    const GameTestSnapshot& snapshot,
    bool escapeStuck,
    AutoSimulationPlanScope scope) const
{
    if (snapshot.worldLoading || snapshot.transitionActive) {
        return {};
    }
    if (scope == AutoSimulationPlanScope::All && needsConfirmInput(snapshot)) {
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

    std::optional<ScoredPlan> bestPlan;
    const bool shouldConsiderReturn = scope == AutoSimulationPlanScope::All &&
        (backpackFull(snapshot) || autoSimulationNeedsLowHpReturn(snapshot));
    if (shouldConsiderReturn) {
        const GameTestWarpPointSnapshot* discoveredWarp = bestAcceptedByRoute(
            snapshot.dungeon.warpPoints,
            pathfinder_,
            pathField,
            snapshot.player.position,
            UnlimitedTargetRadius,
            [](const GameTestWarpPointSnapshot& value) { return value.position; },
            [&snapshot](const GameTestWarpPointSnapshot& value) { return knownWarpDiscovered(snapshot, value); });
        if (discoveredWarp != nullptr) {
            keepBetterPlan(
                bestPlan,
                snapshot,
                makeTravelPlan(
                    snapshot,
                    pathField,
                    AutoSimulationGoal::ReturnToBase,
                    discoveredWarp->position,
                    backpackFull(snapshot) ? "backpack_full_warp" : "low_hp_no_recovery_warp",
                    false,
                    true,
                    AutoSimulationRingRole::None),
                78.0f);
        } else {
            keepBetterPlan(
                bestPlan,
                snapshot,
                makeTravelPlan(
                    snapshot,
                    pathField,
                    AutoSimulationGoal::ReturnToBase,
                    snapshot.dungeon.startWorld,
                    backpackFull(snapshot) ? "backpack_full_entrance" : "low_hp_no_recovery_entrance",
                    false,
                    true,
                    AutoSimulationRingRole::None),
                70.0f);
        }
    }

    if (scope == AutoSimulationPlanScope::All || scope == AutoSimulationPlanScope::CombatOnly) {
        if (std::optional<AutoSimulationPlan> combatPlan = combatModel_.makePlan(snapshot, pathfinder_, pathField)) {
            keepBetterPlan(bestPlan, snapshot, *combatPlan, 72.0f);
        }
    }

    if (scope == AutoSimulationPlanScope::All || scope == AutoSimulationPlanScope::OpportunityOnly) {
        AutoSimulationItemEvaluator itemEvaluator;
        const AutoSimulationItemEvaluationContext itemContext =
            autoSimulationItemEvaluationContextForSnapshot(snapshot);
        const bool opportunisticPickupSafe =
            nearestEnemyDistance(snapshot) > OpportunisticPickupEnemyClearance;
        for (const GameTestDropSnapshot& drop : snapshot.drops) {
        const float acquireRadius = dropAcquireRadius(snapshot, drop);
        if (distanceSquared(snapshot.player.position, drop.position) > acquireRadius * acquireRadius) {
            continue;
        }
        const float baseScore = dropAcquireScore(snapshot, drop, itemEvaluator, itemContext);
        if (baseScore <= -std::numeric_limits<float>::max() * 0.5f) {
            continue;
        }
        AutoSimulationPlan dropPlan = makeTravelPlan(
            snapshot,
            pathField,
            AutoSimulationGoal::CollectDrop,
            drop.position,
            dropAcquireReason(snapshot, drop),
            false,
            false,
            AutoSimulationRingRole::Utility);
        if (dropPlan.goal == AutoSimulationGoal::None) {
            continue;
        }
        float adjustedScore = baseScore;
        if (dropPlan.routePathTileCount > 0 &&
            dropPlan.routeDigTileCount == 0 &&
            dropPlan.routeTotalCost <= OpportunisticPickupRouteCost &&
            opportunisticPickupSafe) {
            adjustedScore += OpportunisticPickupBonus;
        }
        keepBetterPlan(
            bestPlan,
            snapshot,
            std::move(dropPlan),
            adjustedScore);
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
        keepBetterPlan(
            bestPlan,
            snapshot,
            makeTravelPlan(
                snapshot,
                pathField,
                AutoSimulationGoal::OpenChest,
                chest->position,
                "chest",
                true,
                false,
                AutoSimulationRingRole::Utility),
            48.0f);
        }
    }

    if (scope == AutoSimulationPlanScope::All || scope == AutoSimulationPlanScope::ProgressOnly) {
    const GameTestWarpPointSnapshot* visibleWarp = bestAcceptedByRoute(
        snapshot.dungeon.warpPoints,
        pathfinder_,
        pathField,
        snapshot.player.position,
        UnlimitedTargetRadius,
        [](const GameTestWarpPointSnapshot& value) { return value.position; },
        [&snapshot](const GameTestWarpPointSnapshot& value) {
            return value.visible && !knownWarpDiscovered(snapshot, value);
        });
    if (visibleWarp != nullptr) {
        keepBetterPlan(
            bestPlan,
            snapshot,
            makeTravelPlan(
                snapshot,
                pathField,
                AutoSimulationGoal::DiscoverWarp,
                visibleWarp->position,
                "visible_warp",
                false,
                true,
                AutoSimulationRingRole::Light),
            62.0f);
    }

    if (hasUnknownWarpPoint(snapshot)) {
        if (std::optional<AutoSimulationMapClueTarget> target =
                mapClueModel_.chooseTarget(snapshot, pathfinder_, pathField)) {
            keepBetterPlan(
                bestPlan,
                snapshot,
                makeTravelPlan(
                    snapshot,
                    pathField,
                    AutoSimulationGoal::DiscoverWarp,
                    target->world,
                    target->reason,
                    false,
                    true,
                    AutoSimulationRingRole::Light),
                54.0f);
        }
    }

    if (hasUnknownWarpPoint(snapshot)) {
        if (std::optional<AutoSimulationExplorationTarget> target =
                explorationModel_.chooseTarget(snapshot, pathField)) {
            keepBetterPlan(
                bestPlan,
                snapshot,
                makeTravelPlan(
                    snapshot,
                    pathField,
                    AutoSimulationGoal::DiscoverWarp,
                    target->world,
                    target->reason,
                    false,
                    true,
                    AutoSimulationRingRole::Light),
                42.0f + target->utilityAdjustment);
        }
    }

    if (snapshot.dungeon.hasBossSpawnPoint && !snapshot.dungeon.bossSpawned) {
        keepBetterPlan(
            bestPlan,
            snapshot,
            makeTravelPlan(
                snapshot,
                pathField,
                AutoSimulationGoal::ApproachBoss,
                snapshot.dungeon.bossSpawnPoint,
                "boss_spawn",
                false,
                true,
                AutoSimulationRingRole::Light),
            42.0f);
    }

    keepBetterPlan(
        bestPlan,
        snapshot,
        makeTravelPlan(
            snapshot,
            pathField,
            AutoSimulationGoal::FollowMainPath,
            mainPathTarget(snapshot),
            "main_path",
            false,
            true,
            AutoSimulationRingRole::Light),
        16.0f);
    }

    if (bestPlan) {
        return bestPlan->plan;
    }
    return {};
}

} // namespace majo::autosim
