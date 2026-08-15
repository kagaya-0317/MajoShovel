#include "devtools/autosim/AutoSimulationController.hpp"

#include "engine/Log.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace majo::autosim {

namespace {

constexpr float StuckMovementThreshold = 8.0f;
constexpr float StuckSecondsThreshold = 3.6f;
constexpr float EscapeStuckDurationSeconds = 2.4f;
constexpr float MiningNoProgressThresholdSeconds = 4.8f;
constexpr float ActionCooldownSeconds = 0.35f;
constexpr float BaseResumeDelaySeconds = 0.95f;
constexpr float FastPlanReuseSeconds = 0.16f;
constexpr float ReturnActionRadius = 64.0f;
constexpr float LowHpReturnRatio = 0.34f;
constexpr float TargetSameDistance = 28.0f;
constexpr float MineTargetSameDistance = 4.0f;
constexpr float RouteWaypointSameDistance = 18.0f;
constexpr float MeaningfulLightRadius = 140.0f;
constexpr int MinSpeedMultiplier = 1;
constexpr int MaxSpeedMultiplier = 16;
constexpr float EmergencyCombatDistance = 220.0f;
constexpr float OpportunityRouteCostBudget = 18.0f;
constexpr float TaskProgressEpsilon = 8.0f;
constexpr float TaskNoProgressTimeoutSeconds = 6.0f;
constexpr float MissionProgressEpsilon = 12.0f;
constexpr float MissionOpportunityProgressDistance = 160.0f;
constexpr float OpportunityFailureCooldownSeconds = 10.0f;
constexpr float ObjectiveSwitchWindowDurationSeconds = 6.0f;
constexpr int ObjectiveSwitchLimit = 6;
constexpr float BreadcrumbSpacing = 80.0f;
constexpr int OpportunityProgressPathPoints = 3;
constexpr float ResumeFrontierDistance = 120.0f;
constexpr float ResumeBreadcrumbArriveDistance = 34.0f;
constexpr int BackpackReturnRearmPathPoints = 4;
constexpr float MiningContactProbeSeconds = 0.8f;
constexpr float MiningContactNudgeDistance = 4.0f;
constexpr float MiningContactMaxInset = 28.0f;

std::vector<std::string> splitCommand(std::string_view command)
{
    std::istringstream stream{std::string(command)};
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

int parseIntOr(std::string_view value, int fallback)
{
    try {
        return std::stoi(std::string(value));
    } catch (...) {
        return fallback;
    }
}

float parseFloatOr(std::string_view value, float fallback)
{
    try {
        return std::stof(std::string(value));
    } catch (...) {
        return fallback;
    }
}

AutoSimulationResult resultForSnapshot(const GameTestSnapshot& snapshot)
{
    if (snapshot.screenMode == GameTestScreenMode::StageClear) {
        return AutoSimulationResult::StageClear;
    }
    if (snapshot.screenMode == GameTestScreenMode::AstralResult) {
        return AutoSimulationResult::AstralResult;
    }
    return AutoSimulationResult::None;
}

float bestBackpackLightRadius(const GameTestSnapshot& snapshot)
{
    float best = 0.0f;
    for (const GameTestObjectEntrySnapshot& item : snapshot.inventory.backpackItems) {
        if (!item.equipped && !item.broken) {
            best = std::max(best, item.lightRadius);
        }
    }
    return best;
}

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

bool shouldReturnForLowHp(const GameTestSnapshot& snapshot)
{
    return snapshot.player.hp > 0 && hpRatio(snapshot) <= LowHpReturnRatio;
}

std::optional<int> miningTargetHp(const GameTestSnapshot& snapshot, const AutoSimulationPlan& plan)
{
    const GameTestMineTileSnapshot* best = nullptr;
    float bestDistanceSq = 8.0f * 8.0f;
    for (const GameTestMineTileSnapshot& tile : snapshot.nearbyMineTiles) {
        const float distSq = distanceSquared(tile.center, plan.targetWorld);
        if (distSq < bestDistanceSq) {
            bestDistanceSq = distSq;
            best = &tile;
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    return best->hp > 0 ? best->hp : best->effectiveHp;
}

bool lockableGoal(AutoSimulationGoal goal)
{
    switch (goal) {
    case AutoSimulationGoal::MineWall:
    case AutoSimulationGoal::CollectDrop:
    case AutoSimulationGoal::OpenChest:
    case AutoSimulationGoal::DiscoverWarp:
    case AutoSimulationGoal::ReturnToBase:
    case AutoSimulationGoal::ResumeFrontier:
    case AutoSimulationGoal::ApproachBoss:
    case AutoSimulationGoal::FollowMainPath:
        return true;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
    case AutoSimulationGoal::UseItem:
    case AutoSimulationGoal::Combat:
    case AutoSimulationGoal::EscapeStuck:
        return false;
    }
    return false;
}

float lockSecondsForGoal(AutoSimulationGoal goal)
{
    switch (goal) {
    case AutoSimulationGoal::MineWall: return 1.25f;
    case AutoSimulationGoal::CollectDrop: return 0.70f;
    case AutoSimulationGoal::OpenChest: return 0.80f;
    case AutoSimulationGoal::DiscoverWarp: return 0.90f;
    case AutoSimulationGoal::ReturnToBase: return 0.90f;
    case AutoSimulationGoal::ResumeFrontier: return 0.90f;
    case AutoSimulationGoal::ApproachBoss: return 0.90f;
    case AutoSimulationGoal::FollowMainPath: return 0.45f;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
    case AutoSimulationGoal::UseItem:
    case AutoSimulationGoal::Combat:
    case AutoSimulationGoal::EscapeStuck:
        return 0.0f;
    }
    return 0.0f;
}

int goalPriority(AutoSimulationGoal goal)
{
    switch (goal) {
    case AutoSimulationGoal::DismissUi: return 100;
    case AutoSimulationGoal::ReturnToBase: return 90;
    case AutoSimulationGoal::Combat: return 82;
    case AutoSimulationGoal::MineWall: return 66;
    case AutoSimulationGoal::CollectDrop: return 54;
    case AutoSimulationGoal::OpenChest: return 50;
    case AutoSimulationGoal::DiscoverWarp: return 44;
    case AutoSimulationGoal::ResumeFrontier: return 42;
    case AutoSimulationGoal::ApproachBoss: return 40;
    case AutoSimulationGoal::FollowMainPath: return 10;
    case AutoSimulationGoal::EquipLoadout: return 8;
    case AutoSimulationGoal::UseItem: return 88;
    case AutoSimulationGoal::EscapeStuck: return 95;
    case AutoSimulationGoal::None: return 0;
    }
    return 0;
}

bool routeFollowGoal(AutoSimulationGoal goal)
{
    switch (goal) {
    case AutoSimulationGoal::CollectDrop:
    case AutoSimulationGoal::OpenChest:
    case AutoSimulationGoal::DiscoverWarp:
    case AutoSimulationGoal::ReturnToBase:
    case AutoSimulationGoal::ResumeFrontier:
    case AutoSimulationGoal::ApproachBoss:
    case AutoSimulationGoal::FollowMainPath:
        return true;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
    case AutoSimulationGoal::UseItem:
    case AutoSimulationGoal::MineWall:
    case AutoSimulationGoal::Combat:
    case AutoSimulationGoal::EscapeStuck:
        return false;
    }
    return false;
}

bool shouldKeepLockedRoutePlan(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPlan& locked,
    const AutoSimulationPlan& candidate)
{
    if (!routeFollowGoal(candidate.goal) ||
        !locked.hasMoveTarget ||
        !candidate.hasMoveTarget ||
        locked.routePathTileCount <= 1 ||
        candidate.routePathTileCount <= 1) {
        return false;
    }

    const float arriveDistance = std::max(1.0f, locked.moveTargetArriveDistance);
    if (distanceSquared(snapshot.player.position, locked.moveTargetWorld) <= arriveDistance * arriveDistance) {
        return false;
    }
    if (distanceSquared(locked.moveTargetWorld, candidate.moveTargetWorld) <=
        RouteWaypointSameDistance * RouteWaypointSameDistance) {
        return false;
    }
    if (locked.routeWaypointPathIndex >= 0 &&
        candidate.routeWaypointPathIndex > locked.routeWaypointPathIndex) {
        return false;
    }
    return true;
}

bool samePlanTarget(const AutoSimulationPlan& a, const AutoSimulationPlan& b)
{
    if (a.goal != b.goal || !a.hasTarget || !b.hasTarget) {
        return false;
    }
    const float distance = a.goal == AutoSimulationGoal::MineWall
        ? MineTargetSameDistance
        : TargetSameDistance;
    return distanceSquared(a.targetWorld, b.targetWorld) <= distance * distance;
}

bool sameIntent(const AutoSimulationIntent& a, const AutoSimulationIntent& b)
{
    if (a.visible != b.visible ||
        a.goal != b.goal ||
        a.iconKind != b.iconKind ||
        a.iconKey != b.iconKey ||
        a.prefix != b.prefix ||
        a.subject != b.subject ||
        a.suffix != b.suffix) {
        return false;
    }
    return true;
}

AutoSimulationIntent actionIntent(const GameTestAction& action)
{
    AutoSimulationIntent intent;
    intent.visible = true;
    intent.goal = AutoSimulationGoal::EquipLoadout;
    switch (action.kind) {
    case GameTestActionKind::ReturnToBaseViaWarp:
    case GameTestActionKind::ReturnToBaseAfterGameOver:
        intent.goal = AutoSimulationGoal::ReturnToBase;
        intent.iconKind = AutoSimulationIntentIconKind::Base;
        intent.subject = "拠点へ戻りたい";
        break;
    case GameTestActionKind::StartMiningFromBase:
    case GameTestActionKind::StartCheckpointMeasurement:
        intent.goal = AutoSimulationGoal::FollowMainPath;
        intent.iconKind = AutoSimulationIntentIconKind::Path;
        intent.subject = "ダンジョンへ向かいたい";
        break;
    case GameTestActionKind::EquipBackpackItemToRing:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.iconKind = action.objectId.empty() ? AutoSimulationIntentIconKind::None : AutoSimulationIntentIconKind::Object;
        intent.iconKey = action.objectId;
        intent.prefix = "リングに";
        intent.subject = "アイテム";
        intent.suffix = "を入れて準備したい";
        break;
    case GameTestActionKind::RemoveRingItemToBackpack:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.subject = "リングを整理したい";
        break;
    case GameTestActionKind::DiscardBackpackStack:
    case GameTestActionKind::DiscardBackpackInstance:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.iconKind = action.objectId.empty() ? AutoSimulationIntentIconKind::None : AutoSimulationIntentIconKind::Object;
        intent.iconKey = action.objectId;
        intent.prefix = "復旧のため";
        intent.subject = "荷物";
        intent.suffix = "を空けたい";
        break;
    case GameTestActionKind::SyncEncyclopedia:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.iconKind = AutoSimulationIntentIconKind::Base;
        intent.subject = "図鑑を確認したい";
        break;
    case GameTestActionKind::UseBackpackStackItem:
    case GameTestActionKind::UseBackpackInstanceItem:
        intent.goal = AutoSimulationGoal::UseItem;
        intent.iconKind = action.objectId.empty() ? AutoSimulationIntentIconKind::None : AutoSimulationIntentIconKind::Object;
        intent.iconKey = action.objectId;
        intent.prefix = "必要なので";
        intent.subject = "消耗アイテム";
        intent.suffix = "を使いたい";
        break;
    case GameTestActionKind::EquipBackpackStaff:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.iconKind = action.objectId.empty() ? AutoSimulationIntentIconKind::None : AutoSimulationIntentIconKind::Object;
        intent.iconKey = action.objectId;
        intent.prefix = "杖として";
        intent.subject = "アイテム";
        intent.suffix = "を装備したい";
        break;
    case GameTestActionKind::SwitchActiveRing:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.iconKind = AutoSimulationIntentIconKind::Base;
        intent.subject = "使うリングを切り替えたい";
        break;
    case GameTestActionKind::DepositBackpackStack:
    case GameTestActionKind::DepositBackpackInstance:
    case GameTestActionKind::SellBackpackStack:
    case GameTestActionKind::SellBackpackInstance:
    case GameTestActionKind::SellWarehouseStack:
    case GameTestActionKind::SellWarehouseInstance:
    case GameTestActionKind::UnprotectBackpackInstance:
    case GameTestActionKind::UnprotectWarehouseInstance:
        intent.goal = AutoSimulationGoal::ReturnToBase;
        intent.iconKind = AutoSimulationIntentIconKind::Base;
        intent.subject = "拠点で荷物を整理したい";
        break;
    case GameTestActionKind::BulkRepairAtBase:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.iconKind = AutoSimulationIntentIconKind::Base;
        intent.subject = "傷んだアイテムを一括修理したい";
        break;
    case GameTestActionKind::ProtectBackpackInstance:
    case GameTestActionKind::RepairBackpackInstance:
    case GameTestActionKind::RepairRingItem:
    case GameTestActionKind::EnhanceBackpackStackAttack:
    case GameTestActionKind::EnhanceBackpackStackDig:
    case GameTestActionKind::EnhanceBackpackInstanceAttack:
    case GameTestActionKind::EnhanceBackpackInstanceDig:
    case GameTestActionKind::EnhanceRingItemAttack:
    case GameTestActionKind::EnhanceRingItemDig:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.iconKind = action.objectId.empty() ? AutoSimulationIntentIconKind::Base : AutoSimulationIntentIconKind::Object;
        intent.iconKey = action.objectId;
        intent.subject = "強いアイテムを整えたい";
        break;
    case GameTestActionKind::BuyBaseUpgrade:
        intent.goal = AutoSimulationGoal::ReturnToBase;
        intent.iconKind = AutoSimulationIntentIconKind::Base;
        intent.subject = "拠点施設を強化したい";
        break;
    case GameTestActionKind::ChooseLevelUpUpgrade:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.iconKind = AutoSimulationIntentIconKind::Base;
        intent.subject = "レベルアップでリングを強化したい";
        break;
    case GameTestActionKind::None:
        intent.visible = false;
        break;
    }
    return intent;
}

bool actionTargetsObjectEntry(
    const GameTestAction& action,
    const GameTestObjectEntrySnapshot& item)
{
    if (!action.instanceId.empty()) {
        return item.instanceId == action.instanceId;
    }
    return !action.objectId.empty() && item.objectId == action.objectId;
}

std::string actionTargetName(
    const GameTestAction& action,
    const GameTestSnapshot& snapshot)
{
    const auto findInventoryName = [&action](const std::vector<GameTestObjectEntrySnapshot>& items) {
        const auto it = std::find_if(items.begin(), items.end(), [&action](const GameTestObjectEntrySnapshot& item) {
            return actionTargetsObjectEntry(action, item);
        });
        return it == items.end() ? std::string{} : it->name;
    };
    if (std::string name = findInventoryName(snapshot.inventory.backpackItems); !name.empty()) {
        return name;
    }
    if (std::string name = findInventoryName(snapshot.inventory.warehouseItems); !name.empty()) {
        return name;
    }
    const auto ringIt = std::find_if(
        snapshot.ring.items.begin(),
        snapshot.ring.items.end(),
        [&action](const GameTestRingItemSnapshot& item) {
            if (action.ringIndex >= 0 && action.ringItemIndex >= 0) {
                return item.ringIndex == action.ringIndex && item.itemIndex == action.ringItemIndex;
            }
            if (!action.instanceId.empty()) {
                return item.instanceId == action.instanceId;
            }
            return !action.objectId.empty() && item.objectId == action.objectId;
        });
    if (ringIt != snapshot.ring.items.end() && !ringIt->name.empty()) {
        return ringIt->name;
    }
    return "アイテム";
}

std::string actionUpgradeName(
    const GameTestAction& action,
    const GameTestSnapshot& snapshot)
{
    const auto it = std::find_if(
        snapshot.base.upgrades.begin(),
        snapshot.base.upgrades.end(),
        [&action](const GameTestUpgradeSnapshot& upgrade) {
            return upgrade.index == action.upgradeIndex;
        });
    return it == snapshot.base.upgrades.end() || it->name.empty()
        ? std::string("拠点施設")
        : it->name;
}

AutoSimulationIntent completedActionIntent(
    const GameTestAction& action,
    const GameTestSnapshot& snapshot)
{
    AutoSimulationIntent intent = actionIntent(action);
    const std::string itemName = actionTargetName(action, snapshot);
    switch (action.kind) {
    case GameTestActionKind::StartMiningFromBase:
    case GameTestActionKind::StartCheckpointMeasurement:
        intent.subject = "ダンジョンへ出発した";
        break;
    case GameTestActionKind::EquipBackpackItemToRing:
        intent.prefix = "リングに";
        intent.subject = itemName;
        intent.suffix = "を入れた";
        break;
    case GameTestActionKind::RemoveRingItemToBackpack:
        intent.prefix = "リングから";
        intent.subject = itemName;
        intent.suffix = "を外した";
        break;
    case GameTestActionKind::DiscardBackpackStack:
    case GameTestActionKind::DiscardBackpackInstance:
        intent.prefix = "リュックから";
        intent.subject = itemName;
        intent.suffix = "を捨てた";
        break;
    case GameTestActionKind::SyncEncyclopedia:
        intent.subject = "図鑑を同期した";
        break;
    case GameTestActionKind::EquipBackpackStaff:
        intent.prefix = "杖に";
        intent.subject = itemName;
        intent.suffix = "を装備した";
        break;
    case GameTestActionKind::SwitchActiveRing:
        intent.subject = "使用リングを切り替えた";
        break;
    case GameTestActionKind::DepositBackpackStack:
    case GameTestActionKind::DepositBackpackInstance:
        intent.iconKind = action.objectId.empty() ? AutoSimulationIntentIconKind::Base : AutoSimulationIntentIconKind::Object;
        intent.iconKey = action.objectId;
        intent.prefix = "収納箱に";
        intent.subject = itemName;
        intent.suffix = "をしまった";
        break;
    case GameTestActionKind::SellBackpackStack:
    case GameTestActionKind::SellBackpackInstance:
    case GameTestActionKind::SellWarehouseStack:
    case GameTestActionKind::SellWarehouseInstance:
        intent.iconKind = action.objectId.empty() ? AutoSimulationIntentIconKind::Base : AutoSimulationIntentIconKind::Object;
        intent.iconKey = action.objectId;
        intent.prefix = "商人に";
        intent.subject = itemName;
        intent.suffix = "を売った";
        break;
    case GameTestActionKind::UnprotectBackpackInstance:
    case GameTestActionKind::UnprotectWarehouseInstance:
        intent.iconKind = action.objectId.empty() ? AutoSimulationIntentIconKind::Base : AutoSimulationIntentIconKind::Object;
        intent.iconKey = action.objectId;
        intent.prefix.clear();
        intent.subject = itemName;
        intent.suffix = "の保護を解除した";
        break;
    case GameTestActionKind::ProtectBackpackInstance:
        intent.subject = itemName;
        intent.suffix = "を保護した";
        break;
    case GameTestActionKind::BulkRepairAtBase:
        intent.subject = std::to_string(std::max(0, action.count)) + "個のアイテムを一括修理した";
        break;
    case GameTestActionKind::RepairBackpackInstance:
    case GameTestActionKind::RepairRingItem:
        intent.subject = itemName;
        intent.suffix = "を修理した";
        break;
    case GameTestActionKind::EnhanceBackpackStackAttack:
    case GameTestActionKind::EnhanceBackpackInstanceAttack:
    case GameTestActionKind::EnhanceRingItemAttack:
        intent.subject = itemName;
        intent.suffix = "の攻撃力を強化した";
        break;
    case GameTestActionKind::EnhanceBackpackStackDig:
    case GameTestActionKind::EnhanceBackpackInstanceDig:
    case GameTestActionKind::EnhanceRingItemDig:
        intent.subject = itemName;
        intent.suffix = "の掘削力を強化した";
        break;
    case GameTestActionKind::BuyBaseUpgrade:
        intent.subject = actionUpgradeName(action, snapshot);
        intent.suffix = "を強化した";
        break;
    case GameTestActionKind::ReturnToBaseViaWarp:
    case GameTestActionKind::ReturnToBaseAfterGameOver:
    case GameTestActionKind::ChooseLevelUpUpgrade:
    case GameTestActionKind::None:
        break;
    case GameTestActionKind::UseBackpackStackItem:
    case GameTestActionKind::UseBackpackInstanceItem:
        intent.prefix.clear();
        intent.subject = itemName;
        intent.suffix = "を使った";
        break;
    }
    return intent;
}

bool mineTargetExists(const GameTestSnapshot& snapshot, Vec2 target)
{
    for (const GameTestMineTileSnapshot& tile : snapshot.nearbyMineTiles) {
        if (tile.diggable && distanceSquared(tile.center, target) <= MineTargetSameDistance * MineTargetSameDistance) {
            return true;
        }
    }
    return false;
}

bool dropTargetExists(const GameTestSnapshot& snapshot, Vec2 target)
{
    return std::any_of(snapshot.drops.begin(), snapshot.drops.end(), [target](const GameTestDropSnapshot& drop) {
        return distanceSquared(drop.position, target) <= TargetSameDistance * TargetSameDistance;
    });
}

int knownWarpCount(const GameTestSnapshot& snapshot)
{
    return std::max(snapshot.dungeon.discoveredWarpPoints, snapshot.dungeon.unlockedWarpPoints);
}

bool unknownWarpRemaining(const GameTestSnapshot& snapshot)
{
    return knownWarpCount(snapshot) < static_cast<int>(snapshot.dungeon.warpPoints.size());
}

bool knownWarpDiscovered(const GameTestSnapshot& snapshot, const GameTestWarpPointSnapshot& warp)
{
    return warp.discovered || warp.unlocked || warp.index < knownWarpCount(snapshot);
}

const GameTestWarpPointSnapshot* warpByIndex(const GameTestSnapshot& snapshot, int index)
{
    const auto it = std::find_if(
        snapshot.dungeon.warpPoints.begin(),
        snapshot.dungeon.warpPoints.end(),
        [index](const GameTestWarpPointSnapshot& warp) { return warp.index == index; });
    return it == snapshot.dungeon.warpPoints.end() ? nullptr : &*it;
}

const GameTestWarpPointSnapshot* visibleUndiscoveredWarp(const GameTestSnapshot& snapshot)
{
    const GameTestWarpPointSnapshot* best = nullptr;
    float bestDistanceSq = 0.0f;
    for (const GameTestWarpPointSnapshot& warp : snapshot.dungeon.warpPoints) {
        if (!warp.visible || knownWarpDiscovered(snapshot, warp)) {
            continue;
        }
        const float distanceSq = distanceSquared(snapshot.player.position, warp.position);
        if (best == nullptr || distanceSq < bestDistanceSq) {
            best = &warp;
            bestDistanceSq = distanceSq;
        }
    }
    return best;
}

const GameTestWarpPointSnapshot* nearestDiscoveredWarp(const GameTestSnapshot& snapshot)
{
    const GameTestWarpPointSnapshot* best = nullptr;
    float bestDistanceSq = 0.0f;
    for (const GameTestWarpPointSnapshot& warp : snapshot.dungeon.warpPoints) {
        if (!knownWarpDiscovered(snapshot, warp) || !warp.returnInteractionArmed) {
            continue;
        }
        const float distanceSq = distanceSquared(snapshot.player.position, warp.position);
        if (best == nullptr || distanceSq < bestDistanceSq) {
            best = &warp;
            bestDistanceSq = distanceSq;
        }
    }
    return best;
}

const GameTestDropSnapshot* matchingDrop(const GameTestSnapshot& snapshot, Vec2 target)
{
    const GameTestDropSnapshot* best = nullptr;
    float bestDistanceSq = TargetSameDistance * TargetSameDistance;
    for (const GameTestDropSnapshot& drop : snapshot.drops) {
        const float distanceSq = distanceSquared(drop.position, target);
        if (distanceSq <= bestDistanceSq) {
            best = &drop;
            bestDistanceSq = distanceSq;
        }
    }
    return best;
}

int nearestMainPathIndex(const GameTestSnapshot& snapshot)
{
    int best = -1;
    float bestDistanceSq = 0.0f;
    for (int i = 0; i < static_cast<int>(snapshot.dungeon.mainPathWorldPoints.size()); ++i) {
        const float distanceSq = distanceSquared(
            snapshot.player.position,
            snapshot.dungeon.mainPathWorldPoints[static_cast<std::size_t>(i)]);
        if (best < 0 || distanceSq < bestDistanceSq) {
            best = i;
            bestDistanceSq = distanceSq;
        }
    }
    return best;
}

float distanceBetween(Vec2 a, Vec2 b)
{
    return std::sqrt(distanceSquared(a, b));
}

const GameTestWarpPointSnapshot* nearestWarpToPlayer(const GameTestSnapshot& snapshot)
{
    const GameTestWarpPointSnapshot* best = nullptr;
    float bestDistanceSq = 0.0f;
    for (const GameTestWarpPointSnapshot& warp : snapshot.dungeon.warpPoints) {
        const float distSq = distanceSquared(snapshot.player.position, warp.position);
        if (best == nullptr || distSq < bestDistanceSq) {
            best = &warp;
            bestDistanceSq = distSq;
        }
    }
    return best;
}

const GameTestWarpPointSnapshot* nearestWarpToPoint(const GameTestSnapshot& snapshot, Vec2 point)
{
    const GameTestWarpPointSnapshot* best = nullptr;
    float bestDistanceSq = 0.0f;
    for (const GameTestWarpPointSnapshot& warp : snapshot.dungeon.warpPoints) {
        const float distSq = distanceSquared(point, warp.position);
        if (best == nullptr || distSq < bestDistanceSq) {
            best = &warp;
            bestDistanceSq = distSq;
        }
    }
    return best;
}

const GameTestWarpPointSnapshot* nextUnknownWarp(const GameTestSnapshot& snapshot)
{
    const GameTestWarpPointSnapshot* best = nullptr;
    for (const GameTestWarpPointSnapshot& warp : snapshot.dungeon.warpPoints) {
        if (knownWarpDiscovered(snapshot, warp)) {
            continue;
        }
        if (best == nullptr || warp.index < best->index) {
            best = &warp;
        }
    }
    return best;
}

bool planTargetsWarpPoint(const AutoSimulationPlan& plan)
{
    if (!plan.hasTarget) {
        return false;
    }
    if (plan.goal == AutoSimulationGoal::ReturnToBase) {
        return plan.reason.find("warp") != std::string::npos;
    }
    if (plan.goal == AutoSimulationGoal::DiscoverWarp) {
        return plan.reason.find("warp") != std::string::npos;
    }
    return false;
}

const GameTestChestSnapshot* matchingChest(const GameTestSnapshot& snapshot, Vec2 target)
{
    const auto it = std::find_if(
        snapshot.chests.begin(),
        snapshot.chests.end(),
        [target](const GameTestChestSnapshot& chest) {
            return chest.revealed &&
                distanceSquared(chest.position, target) <= TargetSameDistance * TargetSameDistance;
        });
    return it == snapshot.chests.end() ? nullptr : &*it;
}

bool chestTargetExists(const GameTestSnapshot& snapshot, Vec2 target)
{
    const GameTestChestSnapshot* chest = matchingChest(snapshot, target);
    return chest != nullptr && !chest->opened;
}

bool fastReusableGoal(AutoSimulationGoal goal)
{
    switch (goal) {
    case AutoSimulationGoal::MineWall:
    case AutoSimulationGoal::Combat:
    case AutoSimulationGoal::CollectDrop:
    case AutoSimulationGoal::OpenChest:
    case AutoSimulationGoal::DiscoverWarp:
    case AutoSimulationGoal::ReturnToBase:
    case AutoSimulationGoal::ResumeFrontier:
    case AutoSimulationGoal::ApproachBoss:
    case AutoSimulationGoal::FollowMainPath:
    case AutoSimulationGoal::EscapeStuck:
        return true;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
    case AutoSimulationGoal::UseItem:
        return false;
    }
    return false;
}

bool lockedPlanStillValid(const GameTestSnapshot& snapshot, const AutoSimulationPlan& plan)
{
    if (snapshot.screenMode != GameTestScreenMode::Playing || !plan.hasTarget) {
        return false;
    }
    switch (plan.goal) {
    case AutoSimulationGoal::MineWall:
        return mineTargetExists(snapshot, plan.targetWorld);
    case AutoSimulationGoal::CollectDrop:
        return dropTargetExists(snapshot, plan.targetWorld);
    case AutoSimulationGoal::OpenChest:
        return chestTargetExists(snapshot, plan.targetWorld);
    case AutoSimulationGoal::DiscoverWarp:
        return true;
    case AutoSimulationGoal::ReturnToBase:
    case AutoSimulationGoal::ResumeFrontier:
    case AutoSimulationGoal::FollowMainPath:
        return true;
    case AutoSimulationGoal::ApproachBoss:
        return snapshot.dungeon.hasBossSpawnPoint && !snapshot.dungeon.bossSpawned;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
    case AutoSimulationGoal::UseItem:
    case AutoSimulationGoal::Combat:
    case AutoSimulationGoal::EscapeStuck:
        return false;
    }
    return false;
}

bool cachedPlanStillValid(const GameTestSnapshot& snapshot, const AutoSimulationPlan& plan)
{
    if (snapshot.screenMode != GameTestScreenMode::Playing || !plan.hasTarget) {
        return false;
    }
    if (!fastReusableGoal(plan.goal)) {
        return false;
    }
    if (plan.goal == AutoSimulationGoal::Combat) {
        return !snapshot.enemies.empty();
    }
    if (plan.goal == AutoSimulationGoal::EscapeStuck) {
        return true;
    }
    return lockedPlanStillValid(snapshot, plan);
}

std::string actionDecisionDetail(const GameTestAction& action)
{
    return "実行待ち=" + std::string(gameTestActionKindName(action.kind)) +
        (action.estimatedMoneyCost > 0
            ? " / 費用=" + std::to_string(action.estimatedMoneyCost) + "G"
            : std::string{}) +
        (action.reason.empty() ? std::string{} : " / 理由=" + action.reason);
}

std::string pendingActionDecisionDetail(const std::optional<GameTestAction>& action)
{
    return action ? actionDecisionDetail(*action) : "行動要求をキューへ登録できなかった";
}

} // namespace

GameTestSnapshotOptions AutoSimulationController::snapshotOptionsForNextStep() const
{
    GameTestSnapshotOptions options;
    options.useLightweightAutomationUiSnapshot = state_ == AutoSimulationState::Running;
    if (state_ == AutoSimulationState::Running &&
        settings_.speedMultiplier > 1 &&
        !pendingAction_ &&
        cachedPlan_ &&
        cachedPlanSeconds_ > 0.0f &&
        fastReusableGoal(cachedPlan_->goal)) {
        options.includePathGrid = false;
    }
    return options;
}

bool AutoSimulationController::adjustSpeedMultiplier(int delta)
{
    const bool changed = setSpeedMultiplier(settings_.speedMultiplier + delta);
    if (changed) {
        logInfo("AutoSim: speed = x" + std::to_string(settings_.speedMultiplier) + ".");
    }
    return changed;
}

bool AutoSimulationController::setSpeedMultiplier(int value)
{
    const int clampedValue = std::clamp(value, MinSpeedMultiplier, MaxSpeedMultiplier);
    if (settings_.speedMultiplier == clampedValue) {
        return false;
    }
    settings_.speedMultiplier = clampedValue;
    debugSnapshot_.speedMultiplier = clampedValue;
    return true;
}

bool AutoSimulationController::executeCommand(std::string_view normalizedCommand, const GameTestSnapshot& snapshot)
{
    const std::vector<std::string> tokens = splitCommand(normalizedCommand);
    if (tokens.empty() || (tokens.front() != "autosim" && tokens.front() != "auto-sim")) {
        return false;
    }

    const std::string action = tokens.size() >= 2 ? tokens[1] : "report";
    if (action == "start") {
        if (tokens.size() >= 3) {
            settings_.requestedRuns = std::max(1, parseIntOr(tokens[2], settings_.requestedRuns));
        }
        start(snapshot);
        return true;
    }
    if (action == "checkpoint") {
        const std::string stageId = tokens.size() >= 3 ? tokens[2] : snapshot.stageId;
        startCheckpointMeasurement(snapshot, stageId);
        return true;
    }
    if (action == "stop") {
        if (state_ == AutoSimulationState::Running || state_ == AutoSimulationState::Paused) {
            if (checkpointMeasurementMode_) {
                finishCheckpointMeasurement(snapshot, AutoSimulationResult::Stopped);
            } else {
                finish(snapshot, AutoSimulationResult::Stopped);
            }
        } else {
            logInfo("AutoSim: already stopped.");
        }
        return true;
    }
    if (action == "pause") {
        if (state_ == AutoSimulationState::Running) {
            state_ = AutoSimulationState::Paused;
            debugSnapshot_.state = state_;
            inputFrame_ = {};
            logInfo("AutoSim: paused.");
        }
        return true;
    }
    if (action == "resume") {
        if (state_ == AutoSimulationState::Paused) {
            state_ = AutoSimulationState::Running;
            debugSnapshot_.state = state_;
            logInfo("AutoSim: resumed.");
        }
        return true;
    }
    if (action == "runs") {
        if (tokens.size() >= 3) {
            settings_.requestedRuns = std::max(1, parseIntOr(tokens[2], settings_.requestedRuns));
        }
        logInfo("AutoSim: requested runs = " + std::to_string(settings_.requestedRuns) + ".");
        return true;
    }
    if (action == "timeout") {
        if (tokens.size() >= 3) {
            settings_.timeoutSeconds = std::max(5.0f, parseFloatOr(tokens[2], settings_.timeoutSeconds));
        }
        logInfo("AutoSim: timeout = " + std::to_string(static_cast<int>(settings_.timeoutSeconds)) + "s.");
        return true;
    }
    if (action == "speed") {
        if (tokens.size() >= 3) {
            setSpeedMultiplier(parseIntOr(tokens[2], settings_.speedMultiplier));
        }
        logInfo("AutoSim: speed = x" + std::to_string(settings_.speedMultiplier) + ".");
        return true;
    }
    if (action == "trace") {
        if (tokens.size() >= 3) {
            settings_.trace = tokens[2] == "on" || tokens[2] == "true" || tokens[2] == "1";
        } else {
            settings_.trace = !settings_.trace;
        }
        logInfo(std::string("AutoSim: trace ") + (settings_.trace ? "on." : "off."));
        return true;
    }
    if (action == "report") {
        report();
        return true;
    }

    logWarning("AutoSim: unknown command: " + std::string(normalizedCommand));
    return true;
}

void AutoSimulationController::start(const GameTestSnapshot& snapshot)
{
    if (!canStartFromSnapshot(snapshot)) {
        logWarning("AutoSim: start requires a base or dungeon test-play screen.");
        return;
    }

    checkpointMeasurementMode_ = false;
    state_ = AutoSimulationState::Running;
    inputFrame_ = {};
    pendingAction_.reset();
    intentHistory_.clear();
    clearPlanLock();
    clearCachedPlan();
    resetObjectiveState();
    navigator_.reset();
    ringPlanner_.reset();
    ++runIndex_;
    runElapsedSeconds_ = 0.0f;
    actionCooldownSeconds_ = 0.0f;
    baseIdleSeconds_ = 0.0f;
    stillSeconds_ = 0.0f;
    miningNoProgressSeconds_ = 0.0f;
    escapeStuckSeconds_ = 0.0f;
    lastPlayerPosition_ = snapshot.player.position;
    lastMiningTarget_ = {};
    lastMiningTargetHp_ = -1;
    lastMiningDugTiles_ = snapshot.runStats.dugTiles;
    hasLastPlayerPosition_ = true;
    hasLastMiningTarget_ = false;
    stuckCount_ = 0;
    lastGoal_ = AutoSimulationGoal::None;
    lastActionResult_.clear();
    checkpointDungeonLogged_ = false;
    updateDecisionDebugSnapshot(snapshot, "start", "オートシミュ開始直後（次の判断待ち）");

    std::string startMessage = "AutoSim: started run " + std::to_string(runIndex_) +
        " stage=" + snapshot.stageId;
    if (snapshot.dungeon.active) {
        startMessage += " seed=" + std::to_string(snapshot.dungeon.seed);
    }
    logInfo(startMessage + ".");
}

void AutoSimulationController::startCheckpointMeasurement(
    const GameTestSnapshot& snapshot,
    std::string stageId)
{
    if (snapshot.screenMode != GameTestScreenMode::Base || snapshot.worldLoading || snapshot.transitionActive) {
        logWarning("AutoSim: checkpoint measurement must start from the base screen.");
        return;
    }
    if (stageId.empty() || stageId == "stage4" || stageId == "stage_04_astral_mine") {
        logWarning("AutoSim: checkpoint measurement requires a normal stage with warp points.");
        return;
    }
    start(snapshot);
    if (state_ != AutoSimulationState::Running) {
        return;
    }
    checkpointMeasurementMode_ = true;
    GameTestAction action;
    action.kind = GameTestActionKind::StartCheckpointMeasurement;
    action.stageId = std::move(stageId);
    action.reason = "checkpoint_measurement";
    queueAction(std::move(action));
    logInfo("AutoSim: checkpoint measurement requested.");
}

void AutoSimulationController::finish(const GameTestSnapshot& snapshot, AutoSimulationResult result)
{
    const AutoSimulationRunRecord record = makeRecord(snapshot, result);
    state_ = AutoSimulationState::Idle;
    inputFrame_ = {};
    pendingAction_.reset();
    intentHistory_.clear();
    clearPlanLock();
    clearCachedPlan();
    navigator_.reset();
    ringPlanner_.reset();
    resetObjectiveState();
    checkpointMeasurementMode_ = false;
    debugSnapshot_ = {};
    logger_.recordRun(record);

    logInfo("AutoSim: finished run " + std::to_string(record.runIndex) +
        " result=" + autoSimulationResultName(result) +
        " elapsed=" + std::to_string(static_cast<int>(record.elapsedSeconds)) + "s" +
        " log=" + logger_.csvPath().string());
}

void AutoSimulationController::finishCheckpointMeasurement(
    const GameTestSnapshot& snapshot,
    AutoSimulationResult result)
{
    state_ = AutoSimulationState::Idle;
    inputFrame_ = {};
    pendingAction_.reset();
    intentHistory_.clear();
    clearPlanLock();
    clearCachedPlan();
    navigator_.reset();
    ringPlanner_.reset();
    resetObjectiveState();
    debugSnapshot_ = {};
    checkpointMeasurementMode_ = false;
    pendingReportPath_ = logger_.writeCheckpointReport(snapshot.checkpointMeasurement, result);
    if (pendingReportPath_ && !pendingReportPath_->empty()) {
        logInfo("AutoSim: checkpoint report saved: " + pendingReportPath_->string());
    } else {
        pendingReportPath_.reset();
        logError("AutoSim: failed to write checkpoint report.");
    }
}

void AutoSimulationController::report() const
{
    logger_.writeSummary();
    logInfo("AutoSim: state=" + std::string(autoSimulationStateName(state_)) +
        " run=" + std::to_string(runIndex_) +
        " speed=x" + std::to_string(settings_.speedMultiplier) +
        " elapsed=" + std::to_string(static_cast<int>(runElapsedSeconds_)) + "s" +
        " csv=" + logger_.csvPath().string());
    if (logger_.records().empty()) {
        logInfo("AutoSim: no completed runs.");
        return;
    }
    const AutoSimulationRunRecord& last = logger_.records().back();
    logInfo("AutoSim: last result=" + std::string(autoSimulationResultName(last.result)) +
        " stage=" + last.stageId +
        " dug=" + std::to_string(last.dugTiles) +
        " kills=" + std::to_string(last.defeatedEnemies) +
        " items=" + std::to_string(last.acquiredItems) +
        " stuck=" + std::to_string(last.stuckCount) + ".");
}

std::optional<GameTestAction> AutoSimulationController::consumeAction()
{
    if (!pendingAction_) {
        return std::nullopt;
    }
    std::optional<GameTestAction> action = std::move(pendingAction_);
    pendingAction_.reset();
    return action;
}

std::optional<std::filesystem::path> AutoSimulationController::consumePendingReportPath()
{
    std::optional<std::filesystem::path> path = std::move(pendingReportPath_);
    pendingReportPath_.reset();
    return path;
}

void AutoSimulationController::recordActionResult(
    const GameTestAction& action,
    const GameTestActionResult& result,
    const GameTestSnapshot& snapshot)
{
    baseTasks_.recordActionResult(action, result);
    if (result.applied && action.kind == GameTestActionKind::StartMiningFromBase) {
        beginDungeonExcursion(knownWarpCount(snapshot));
    } else if (result.applied && action.kind == GameTestActionKind::StartCheckpointMeasurement) {
        beginDungeonExcursion(0);
    }
    if (action.kind == GameTestActionKind::StartCheckpointMeasurement && !result.applied) {
        state_ = AutoSimulationState::Idle;
        checkpointMeasurementMode_ = false;
        pendingAction_.reset();
        inputFrame_ = {};
        logError("AutoSim: checkpoint measurement start failed: " + result.message);
    }
    lastActionResult_ = std::string(gameTestActionKindName(action.kind)) +
        (result.applied ? " は成功" : " は未実行") +
        (action.reason.empty() ? std::string{} : " / 要求理由=" + action.reason) +
        (result.message.empty() ? std::string{} : " / 結果=" + result.message);
    debugSnapshot_.lastActionResult = lastActionResult_;
    if (result.applied && snapshot.screenMode == GameTestScreenMode::Base) {
        const AutoSimulationIntent queuedIntent = actionIntent(action);
        AutoSimulationIntent completedIntent = completedActionIntent(action, snapshot);
        if (!intentHistory_.empty() && sameIntent(intentHistory_.front(), queuedIntent)) {
            intentHistory_.front() = std::move(completedIntent);
        } else {
            recordIntent(std::move(completedIntent));
        }
    }
    if (!settings_.trace && result.applied) {
        return;
    }
    logInfo("AutoSim: action=" + std::string(gameTestActionKindName(action.kind)) +
        " result=" + (result.applied ? "applied" : "skipped") +
        (action.reason.empty() ? std::string{} : " reason=" + action.reason) +
        (result.message.empty() ? std::string{} : " message=" + result.message));
}

void AutoSimulationController::setSimulationStepsLastFrame(int steps)
{
    simulationStepsLastFrame_ = std::max(0, steps);
    debugSnapshot_.speedMultiplier = settings_.speedMultiplier;
    debugSnapshot_.simulationStepsLastFrame = simulationStepsLastFrame_;
}

void AutoSimulationController::queueAction(GameTestAction action)
{
    if (pendingAction_ || action.kind == GameTestActionKind::None) {
        return;
    }
    if (settings_.trace) {
        logInfo("AutoSim: queue action=" + std::string(gameTestActionKindName(action.kind)) +
            (action.reason.empty() ? std::string{} : " reason=" + action.reason));
    }
    pendingAction_ = std::move(action);
    recordIntent(actionIntent(*pendingAction_));
    actionCooldownSeconds_ = ActionCooldownSeconds;
    clearPlanLock();
    clearCachedPlan();
}

void AutoSimulationController::recordIntent(AutoSimulationIntent intent)
{
    if (!intent.visible) {
        return;
    }
    if (!intentHistory_.empty() && sameIntent(intentHistory_.front(), intent)) {
        intentHistory_.front() = std::move(intent);
        return;
    }

    intentHistory_.insert(intentHistory_.begin(), std::move(intent));
    constexpr std::size_t MaxIntentHistory = 3;
    if (intentHistory_.size() > MaxIntentHistory) {
        intentHistory_.resize(MaxIntentHistory);
    }
}

void AutoSimulationController::clearPlanLock()
{
    lockedPlan_.reset();
    planLockSeconds_ = 0.0f;
}

void AutoSimulationController::resetObjectiveState()
{
    mission_ = {};
    task_ = {};
    traversalMemory_ = {};
    missionNoProgressSeconds_ = 0.0f;
    opportunitySuspendSeconds_ = 0.0f;
    objectiveSwitchWindowSeconds_ = 0.0f;
    opportunityBudget_ = 1;
    objectiveSwitchCount_ = 0;
    resumeBreadcrumbIndex_ = -1;
    observedMissionProgressGeneration_ = 0;
    lastObjectiveGoal_ = AutoSimulationGoal::None;
    lastObjectiveTarget_ = {};
    previousScreenMode_ = GameTestScreenMode::Title;
    hasPreviousScreenMode_ = false;
    resumeFrontierRequested_ = false;
    lastBaseVisitKnownWarpCount_ = 0;
    backpackReturnRearmMainPathIndex_ = 0;
    backpackReturnArmed_ = true;
    backpackReturnRearmProgressPending_ = false;
}

void AutoSimulationController::beginDungeonExcursion(int knownWarpCountAtDeparture)
{
    lastBaseVisitKnownWarpCount_ = std::max(0, knownWarpCountAtDeparture);
    backpackReturnRearmMainPathIndex_ = 0;
    backpackReturnArmed_ = false;
    backpackReturnRearmProgressPending_ = true;
}

void AutoSimulationController::updateDungeonExcursionState(const GameTestSnapshot& snapshot)
{
    if (snapshot.screenMode != GameTestScreenMode::Playing) {
        return;
    }
    if (backpackReturnArmed_) {
        return;
    }

    const bool reachedNewWarp = knownWarpCount(snapshot) > lastBaseVisitKnownWarpCount_;
    if (backpackReturnRearmProgressPending_) {
        backpackReturnRearmProgressPending_ = false;
        backpackReturnRearmMainPathIndex_ = std::max(
            0,
            traversalMemory_.furthestMainPathIndex + BackpackReturnRearmPathPoints);
        if (!reachedNewWarp) {
            return;
        }
    }
    const bool advancedMainPath =
        traversalMemory_.furthestMainPathIndex >= backpackReturnRearmMainPathIndex_;
    if (!reachedNewWarp && !advancedMainPath) {
        return;
    }

    backpackReturnArmed_ = true;
    if (settings_.trace) {
        logInfo(std::string("AutoSim: backpack return rearmed reason=") +
            (reachedNewWarp ? "new_warp_progress." : "main_path_progress."));
    }
}

void AutoSimulationController::updateTraversalMemory(const GameTestSnapshot& snapshot)
{
    if (snapshot.screenMode != GameTestScreenMode::Playing) {
        return;
    }

    if (!traversalMemory_.matches(snapshot)) {
        traversalMemory_ = {};
        traversalMemory_.stageId = snapshot.stageId;
        traversalMemory_.seed = snapshot.dungeon.seed;
        traversalMemory_.breadcrumbs.push_back(snapshot.player.position);
        opportunityBudget_ = 1;
    }

    const int mainPathIndex = nearestMainPathIndex(snapshot);
    if (mainPathIndex <= traversalMemory_.furthestMainPathIndex) {
        return;
    }

    traversalMemory_.furthestMainPathIndex = mainPathIndex;
    traversalMemory_.furthestWorld = snapshot.player.position;
    if (traversalMemory_.breadcrumbs.empty() ||
        distanceSquared(traversalMemory_.breadcrumbs.back(), snapshot.player.position) >=
            BreadcrumbSpacing * BreadcrumbSpacing) {
        traversalMemory_.breadcrumbs.push_back(snapshot.player.position);
    }

    if (traversalMemory_.lastOpportunityProgressIndex < 0) {
        traversalMemory_.lastOpportunityProgressIndex = mainPathIndex;
    } else if (mainPathIndex - traversalMemory_.lastOpportunityProgressIndex >= OpportunityProgressPathPoints) {
        traversalMemory_.lastOpportunityProgressIndex = mainPathIndex;
        opportunityBudget_ = 1;
    }
}

void AutoSimulationController::beginMission(
    AutoSimulationGoal goal,
    Vec2 targetWorld,
    std::string reason,
    int targetIndex)
{
    if (mission_.goal == goal && mission_.targetIndex == targetIndex &&
        distanceSquared(mission_.targetWorld, targetWorld) <= TargetSameDistance * TargetSameDistance) {
        return;
    }

    if (goal == AutoSimulationGoal::ReturnToBase && task_.active()) {
        completeTask(false, "urgent_return");
    }

    mission_ = {};
    mission_.goal = goal;
    mission_.targetWorld = targetWorld;
    mission_.targetIndex = targetIndex;
    mission_.reason = std::move(reason);
    mission_.startedAt = runElapsedSeconds_;
    mission_.bestDistance = std::numeric_limits<float>::max();
    mission_.progressAnchorDistance = mission_.bestDistance;
    missionNoProgressSeconds_ = 0.0f;
    if (goal == AutoSimulationGoal::ReturnToBase ||
        goal == AutoSimulationGoal::DiscoverWarp ||
        goal == AutoSimulationGoal::ResumeFrontier) {
        opportunityBudget_ = 0;
    }
    clearPlanLock();
    clearCachedPlan();
    noteObjectiveTransition(goal, targetWorld);
    if (settings_.trace) {
        logInfo("AutoSim: mission=" + std::string(autoSimulationGoalName(goal)) +
            " reason=" + mission_.reason + ".");
    }
}

void AutoSimulationController::beginAdaptiveMission(
    AutoSimulationGoal goal,
    Vec2 progressTargetWorld,
    std::string reason,
    int progressMarker)
{
    beginMission(goal, progressTargetWorld, std::move(reason));
    mission_.adaptive = true;
    mission_.progressMarker = progressMarker;
    opportunityBudget_ = std::max(1, opportunityBudget_);
}

void AutoSimulationController::completeMission()
{
    if (!mission_.active()) {
        return;
    }
    if (settings_.trace) {
        logInfo("AutoSim: mission complete=" + std::string(autoSimulationGoalName(mission_.goal)) + ".");
    }
    mission_ = {};
    missionNoProgressSeconds_ = 0.0f;
    resumeBreadcrumbIndex_ = -1;
    opportunityBudget_ = 1;
    clearPlanLock();
    clearCachedPlan();
    noteObjectiveTransition(AutoSimulationGoal::None, {});
}

void AutoSimulationController::beginTask(
    const AutoSimulationPlan& plan,
    const GameTestSnapshot& snapshot)
{
    task_ = {};
    task_.goal = plan.objectiveGoal != AutoSimulationGoal::None ? plan.objectiveGoal : plan.goal;
    task_.targetWorld = plan.hasObjectiveTarget ? plan.objectiveTargetWorld : plan.targetWorld;
    task_.reason = plan.reason;
    task_.startedAt = runElapsedSeconds_;
    task_.bestDistance = distanceBetween(snapshot.player.position, task_.targetWorld);
    if (const GameTestDropSnapshot* drop = matchingDrop(snapshot, task_.targetWorld)) {
        task_.targetId = drop->id;
    }
    opportunityBudget_ = std::max(0, opportunityBudget_ - 1);
    clearPlanLock();
    clearCachedPlan();
    noteObjectiveTransition(task_.goal, task_.targetWorld);
}

void AutoSimulationController::completeTask(bool succeeded, std::string_view reason)
{
    if (!task_.active()) {
        return;
    }
    if (settings_.trace || !succeeded) {
        logInfo("AutoSim: task=" + std::string(autoSimulationGoalName(task_.goal)) +
            (succeeded ? " complete" : " abandoned") +
            (reason.empty() ? std::string{} : " reason=" + std::string(reason)) + ".");
    }
    task_ = {};
    if (!succeeded) {
        opportunitySuspendSeconds_ = OpportunityFailureCooldownSeconds;
    }
    clearPlanLock();
    clearCachedPlan();
    noteObjectiveTransition(
        mission_.active() ? mission_.goal : AutoSimulationGoal::None,
        mission_.active() ? mission_.targetWorld : Vec2{});
}

void AutoSimulationController::updateTaskState(const GameTestSnapshot& snapshot, float dt)
{
    if (!task_.active()) {
        return;
    }
    if (snapshot.screenMode == GameTestScreenMode::Base) {
        completeTask(false, "left_dungeon");
        return;
    }
    if (snapshot.screenMode != GameTestScreenMode::Playing) {
        return;
    }

    bool targetExists = true;
    if (task_.goal == AutoSimulationGoal::CollectDrop) {
        const GameTestDropSnapshot* drop = matchingDrop(snapshot, task_.targetWorld);
        targetExists = drop != nullptr && (task_.targetId.empty() || drop->id == task_.targetId);
    } else if (task_.goal == AutoSimulationGoal::OpenChest) {
        const GameTestChestSnapshot* chest = matchingChest(snapshot, task_.targetWorld);
        if (chest != nullptr && chest->opened) {
            if (!chest->contentsReleased) {
                task_.noProgressSeconds = 0.0f;
                return;
            }
            completeTask(true, "chest_contents_released");
            opportunityBudget_ = std::max(1, opportunityBudget_);
            return;
        }
        targetExists = chest != nullptr;
    }
    if (!targetExists) {
        completeTask(true, "target_completed");
        return;
    }

    const float distance = distanceBetween(snapshot.player.position, task_.targetWorld);
    if (distance + TaskProgressEpsilon < task_.bestDistance) {
        task_.bestDistance = distance;
        task_.noProgressSeconds = 0.0f;
    } else {
        task_.noProgressSeconds += std::max(0.0f, dt);
    }
    if (task_.noProgressSeconds >= TaskNoProgressTimeoutSeconds) {
        completeTask(false, "no_progress");
    }
}

void AutoSimulationController::updateMissionState(const GameTestSnapshot& snapshot, float dt)
{
    if (snapshot.screenMode == GameTestScreenMode::Base) {
        if (!snapshot.transitionActive && !snapshot.worldLoading) {
            lastBaseVisitKnownWarpCount_ = knownWarpCount(snapshot);
        }
        if (mission_.goal == AutoSimulationGoal::ReturnToBase) {
            completeMission();
            resumeFrontierRequested_ = traversalMemory_.breadcrumbs.size() > 1;
        }
        return;
    }
    if (snapshot.screenMode != GameTestScreenMode::Playing) {
        return;
    }

    updateDungeonExcursionState(snapshot);

    const bool enteredPlaying = !hasPreviousScreenMode_ || previousScreenMode_ != GameTestScreenMode::Playing;
    if (enteredPlaying && traversalMemory_.matches(snapshot) &&
        distanceSquared(snapshot.player.position, traversalMemory_.furthestWorld) >
            ResumeFrontierDistance * ResumeFrontierDistance &&
        traversalMemory_.breadcrumbs.size() > 1) {
        resumeFrontierRequested_ = true;
        resumeBreadcrumbIndex_ = 0;
        float bestDistanceSq = std::numeric_limits<float>::max();
        for (int i = 0; i < static_cast<int>(traversalMemory_.breadcrumbs.size()); ++i) {
            const float distanceSq = distanceSquared(snapshot.player.position, traversalMemory_.breadcrumbs[static_cast<std::size_t>(i)]);
            if (distanceSq < bestDistanceSq) {
                bestDistanceSq = distanceSq;
                resumeBreadcrumbIndex_ = i;
            }
        }
        resumeBreadcrumbIndex_ = std::min(
            resumeBreadcrumbIndex_ + 1,
            static_cast<int>(traversalMemory_.breadcrumbs.size()) - 1);
    }

    if (mission_.goal == AutoSimulationGoal::DiscoverWarp) {
        if (const GameTestWarpPointSnapshot* warp = warpByIndex(snapshot, mission_.targetIndex);
            warp != nullptr && knownWarpDiscovered(snapshot, *warp)) {
            completeMission();
        }
    } else if (mission_.goal == AutoSimulationGoal::ResumeFrontier &&
        distanceSquared(snapshot.player.position, mission_.targetWorld) <=
            ResumeBreadcrumbArriveDistance * ResumeBreadcrumbArriveDistance) {
        ++resumeBreadcrumbIndex_;
        if (resumeBreadcrumbIndex_ >= static_cast<int>(traversalMemory_.breadcrumbs.size())) {
            resumeFrontierRequested_ = false;
            completeMission();
        } else {
            mission_.targetWorld = traversalMemory_.breadcrumbs[static_cast<std::size_t>(resumeBreadcrumbIndex_)];
            mission_.bestDistance = std::numeric_limits<float>::max();
            ++mission_.progressGeneration;
            missionNoProgressSeconds_ = 0.0f;
            clearPlanLock();
            clearCachedPlan();
        }
    }

    if (mission_.adaptive &&
        ((mission_.goal == AutoSimulationGoal::DiscoverWarp &&
             knownWarpCount(snapshot) > mission_.progressMarker) ||
            (mission_.goal == AutoSimulationGoal::ApproachBoss && snapshot.dungeon.bossSpawned))) {
        completeMission();
    }

    const bool returnForFullBackpack = backpackReturnArmed_ && backpackFull(snapshot);
    const bool returnForLowHp = shouldReturnForLowHp(snapshot);
    const bool urgentReturn = returnForFullBackpack || returnForLowHp;
    if (urgentReturn && mission_.goal != AutoSimulationGoal::ReturnToBase) {
        const GameTestWarpPointSnapshot* warp = nearestDiscoveredWarp(snapshot);
        beginMission(
            AutoSimulationGoal::ReturnToBase,
            warp != nullptr ? warp->position : snapshot.dungeon.startWorld,
            returnForFullBackpack ? "backpack_full_mission" : "low_hp_mission",
            warp != nullptr ? warp->index : -1);
    } else if (mission_.goal == AutoSimulationGoal::ReturnToBase && mission_.targetIndex < 0) {
        if (const GameTestWarpPointSnapshot* warp = nearestDiscoveredWarp(snapshot)) {
            beginMission(
                AutoSimulationGoal::ReturnToBase,
                warp->position,
                mission_.reason,
                warp->index);
        }
    }

    if (mission_.goal != AutoSimulationGoal::ReturnToBase) {
        if (const GameTestWarpPointSnapshot* warp = visibleUndiscoveredWarp(snapshot);
            warp != nullptr && (mission_.goal != AutoSimulationGoal::DiscoverWarp || mission_.targetIndex != warp->index)) {
            beginMission(AutoSimulationGoal::DiscoverWarp, warp->position, "visible_undiscovered_warp", warp->index);
        }
    }

    if (!mission_.active() && resumeFrontierRequested_ && resumeBreadcrumbIndex_ >= 0 &&
        resumeBreadcrumbIndex_ < static_cast<int>(traversalMemory_.breadcrumbs.size())) {
        beginMission(
            AutoSimulationGoal::ResumeFrontier,
            traversalMemory_.breadcrumbs[static_cast<std::size_t>(resumeBreadcrumbIndex_)],
            "resume_previous_frontier");
    }

    if (!mission_.active()) {
        if (unknownWarpRemaining(snapshot)) {
            beginAdaptiveMission(
                AutoSimulationGoal::DiscoverWarp,
                snapshot.dungeon.goalWorld,
                "discover_next_warp_mission",
                knownWarpCount(snapshot));
        } else if (snapshot.dungeon.hasBossSpawnPoint && !snapshot.dungeon.bossSpawned) {
            beginAdaptiveMission(
                AutoSimulationGoal::ApproachBoss,
                snapshot.dungeon.bossSpawnPoint,
                "approach_boss_mission");
        } else {
            beginAdaptiveMission(
                AutoSimulationGoal::FollowMainPath,
                snapshot.dungeon.goalWorld,
                "advance_deeper_mission");
        }
    }

    if (!mission_.active()) {
        return;
    }
    const float distance = distanceBetween(snapshot.player.position, mission_.targetWorld);
    if (distance + MissionProgressEpsilon < mission_.bestDistance) {
        mission_.bestDistance = distance;
        missionNoProgressSeconds_ = 0.0f;
        ++mission_.progressGeneration;
        if (!std::isfinite(mission_.progressAnchorDistance)) {
            mission_.progressAnchorDistance = distance;
        } else if (mission_.progressAnchorDistance - distance >= MissionOpportunityProgressDistance &&
            mission_.goal != AutoSimulationGoal::ReturnToBase &&
            mission_.goal != AutoSimulationGoal::DiscoverWarp &&
            mission_.goal != AutoSimulationGoal::ResumeFrontier) {
            opportunityBudget_ = 1;
            mission_.progressAnchorDistance = distance;
        }
    } else if (!task_.active()) {
        missionNoProgressSeconds_ += std::max(0.0f, dt);
    }
}

AutoSimulationPlan AutoSimulationController::makeMissionPlan(const GameTestSnapshot& snapshot) const
{
    if (mission_.adaptive) {
        return planner_.makePlan(snapshot, false, AutoSimulationPlanScope::ProgressOnly);
    }
    AutoSimulationDigPolicy digPolicy = AutoSimulationDigPolicy::Required;
    AutoSimulationRingRole ringRole = AutoSimulationRingRole::None;
    if (mission_.goal == AutoSimulationGoal::DiscoverWarp) {
        digPolicy = missionNoProgressSeconds_ >= TaskNoProgressTimeoutSeconds
            ? AutoSimulationDigPolicy::Required
            : AutoSimulationDigPolicy::CheapOnly;
        ringRole = AutoSimulationRingRole::Light;
    } else if (mission_.goal == AutoSimulationGoal::ResumeFrontier) {
        digPolicy = AutoSimulationDigPolicy::Avoid;
        ringRole = AutoSimulationRingRole::Light;
    }
    return planner_.makeDirectedPlan(
        snapshot,
        mission_.goal,
        mission_.targetWorld,
        mission_.reason,
        digPolicy,
        ringRole);
}

AutoSimulationPlan AutoSimulationController::makeTaskPlan(const GameTestSnapshot& snapshot) const
{
    return planner_.makeDirectedPlan(
        snapshot,
        task_.goal,
        task_.targetWorld,
        task_.reason,
        AutoSimulationDigPolicy::CheapOnly,
        AutoSimulationRingRole::Utility);
}

AutoSimulationPlan AutoSimulationController::applyMiningContactCorrection(AutoSimulationPlan plan) const
{
    if (plan.goal != AutoSimulationGoal::MineWall || !plan.hasTarget ||
        miningNoProgressSeconds_ < MiningContactProbeSeconds) {
        return plan;
    }
    Vec2 direction = plan.targetWorld - plan.moveTargetWorld;
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length <= 0.001f) {
        return plan;
    }
    direction = direction / length;
    const float inset = std::min(
        MiningContactMaxInset,
        MiningContactNudgeDistance * std::floor(miningNoProgressSeconds_ / MiningContactProbeSeconds));
    plan.moveTargetWorld += direction * inset;
    plan.moveTargetArriveDistance = std::max(2.0f, plan.moveTargetArriveDistance - inset);
    return plan;
}

AutoSimulationPlan AutoSimulationController::makeHierarchicalPlan(
    const GameTestSnapshot& snapshot,
    bool escapeStuck)
{
    if (escapeStuck) {
        return planner_.makePlan(snapshot, true, AutoSimulationPlanScope::ProgressOnly);
    }

    AutoSimulationPlan combat = planner_.makePlan(snapshot, false, AutoSimulationPlanScope::CombatOnly);
    if (combat.goal == AutoSimulationGoal::Combat &&
        (!combat.hasTarget || distanceBetween(snapshot.player.position, combat.targetWorld) <= EmergencyCombatDistance)) {
        return combat;
    }
    if (task_.active()) {
        return applyMiningContactCorrection(makeTaskPlan(snapshot));
    }
    if (mission_.active() && !mission_.adaptive) {
        AutoSimulationPlan missionPlan = makeMissionPlan(snapshot);
        if (missionPlan.goal != AutoSimulationGoal::None) {
            return applyMiningContactCorrection(std::move(missionPlan));
        }
        if (mission_.goal != AutoSimulationGoal::ResumeFrontier) {
            return {};
        }
        resumeFrontierRequested_ = false;
        completeMission();
        logInfo("AutoSim: previous frontier route unavailable; resuming normal exploration.");
    }
    if (opportunityBudget_ > 0 && opportunitySuspendSeconds_ <= 0.0f) {
        AutoSimulationPlan opportunity = planner_.makePlan(snapshot, false, AutoSimulationPlanScope::OpportunityOnly);
        if (opportunity.hasTarget &&
            opportunity.routeDigTileCount == 0 &&
            opportunity.routeTotalCost <= OpportunityRouteCostBudget) {
            beginTask(opportunity, snapshot);
            return opportunity;
        }
    }
    if (mission_.active()) {
        return applyMiningContactCorrection(makeMissionPlan(snapshot));
    }
    return applyMiningContactCorrection(
        planner_.makePlan(snapshot, false, AutoSimulationPlanScope::ProgressOnly));
}

void AutoSimulationController::noteObjectiveTransition(AutoSimulationGoal goal, Vec2 targetWorld)
{
    if (goal == lastObjectiveGoal_ &&
        distanceSquared(targetWorld, lastObjectiveTarget_) <= TargetSameDistance * TargetSameDistance) {
        return;
    }

    if (objectiveSwitchWindowSeconds_ <= 0.0f) {
        objectiveSwitchWindowSeconds_ = ObjectiveSwitchWindowDurationSeconds;
        objectiveSwitchCount_ = 1;
        observedMissionProgressGeneration_ = mission_.progressGeneration;
    } else {
        ++objectiveSwitchCount_;
    }
    lastObjectiveGoal_ = goal;
    lastObjectiveTarget_ = targetWorld;

    if (objectiveSwitchCount_ >= ObjectiveSwitchLimit &&
        observedMissionProgressGeneration_ == mission_.progressGeneration) {
        opportunitySuspendSeconds_ = OpportunityFailureCooldownSeconds;
        opportunityBudget_ = 0;
        task_ = {};
        objectiveSwitchCount_ = 0;
        objectiveSwitchWindowSeconds_ = 0.0f;
        clearPlanLock();
        clearCachedPlan();
        logInfo("AutoSim: objective cycle suppressed.");
    }
}

void AutoSimulationController::clearCachedPlan()
{
    cachedPlan_.reset();
    cachedPlanSeconds_ = 0.0f;
}

bool AutoSimulationController::canReuseCachedPlan(const GameTestSnapshot& snapshot) const
{
    return settings_.speedMultiplier > 1 &&
        cachedPlan_ &&
        cachedPlanSeconds_ > 0.0f &&
        cachedPlanStillValid(snapshot, *cachedPlan_);
}

void AutoSimulationController::cachePlanForFastForward(const AutoSimulationPlan& plan)
{
    if (settings_.speedMultiplier <= 1 || !plan.hasTarget || !fastReusableGoal(plan.goal)) {
        clearCachedPlan();
        return;
    }

    cachedPlan_ = plan;
    cachedPlanSeconds_ = FastPlanReuseSeconds;
}

AutoSimulationPlan AutoSimulationController::stabilizePlan(
    const GameTestSnapshot& snapshot,
    AutoSimulationPlan candidate,
    float dt)
{
    planLockSeconds_ = std::max(0.0f, planLockSeconds_ - std::max(0.0f, dt));

    if (escapeStuckSeconds_ > 0.0f || !candidate.hasTarget || !lockableGoal(candidate.goal)) {
        clearPlanLock();
        return candidate;
    }

    if (!lockedPlan_ || !lockedPlanStillValid(snapshot, *lockedPlan_)) {
        lockedPlan_ = candidate;
        planLockSeconds_ = lockSecondsForGoal(candidate.goal);
        return candidate;
    }

    if (goalPriority(candidate.goal) > goalPriority(lockedPlan_->goal)) {
        lockedPlan_ = candidate;
        planLockSeconds_ = lockSecondsForGoal(candidate.goal);
        return candidate;
    }

    if (samePlanTarget(*lockedPlan_, candidate)) {
        planLockSeconds_ = lockSecondsForGoal(candidate.goal);
        if (routeFollowGoal(candidate.goal)) {
            if (shouldKeepLockedRoutePlan(snapshot, *lockedPlan_, candidate)) {
                return *lockedPlan_;
            }
            lockedPlan_ = candidate;
            return candidate;
        }
        return *lockedPlan_;
    }

    if (planLockSeconds_ > 0.0f &&
        goalPriority(candidate.goal) <= goalPriority(lockedPlan_->goal)) {
        return *lockedPlan_;
    }

    lockedPlan_ = candidate;
    planLockSeconds_ = lockSecondsForGoal(candidate.goal);
    return candidate;
}

void AutoSimulationController::update(const GameTestSnapshot& snapshot, float dt)
{
    inputFrame_ = {};
    if (state_ != AutoSimulationState::Running) {
        pendingAction_.reset();
        clearCachedPlan();
        return;
    }
    if (pendingAction_) {
        updateDecisionDebugSnapshot(snapshot, "pending_action", actionDecisionDetail(*pendingAction_));
        return;
    }

    if (checkpointMeasurementMode_ && !checkpointDungeonLogged_ &&
        snapshot.checkpointMeasurement.totalWarpPoints > 0) {
        checkpointDungeonLogged_ = true;
        logInfo("AutoSim: checkpoint dungeon generated stage=" +
            snapshot.checkpointMeasurement.stageId +
            " seed=" + std::to_string(snapshot.checkpointMeasurement.seed) +
            " warps=" + std::to_string(snapshot.checkpointMeasurement.totalWarpPoints) + ".");
    }

    if (checkpointMeasurementMode_ && snapshot.checkpointMeasurement.completed) {
        finishCheckpointMeasurement(snapshot, AutoSimulationResult::None);
        return;
    }
    if (checkpointMeasurementMode_ && snapshot.screenMode == GameTestScreenMode::GameOver) {
        finishCheckpointMeasurement(snapshot, AutoSimulationResult::GameOver);
        return;
    }

    if (const AutoSimulationResult result = resultForSnapshot(snapshot); result != AutoSimulationResult::None) {
        if (checkpointMeasurementMode_) {
            finishCheckpointMeasurement(snapshot, result);
        } else {
            finish(snapshot, result);
        }
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    runElapsedSeconds_ += safeDt;
    actionCooldownSeconds_ = std::max(0.0f, actionCooldownSeconds_ - safeDt);
    cachedPlanSeconds_ = std::max(0.0f, cachedPlanSeconds_ - safeDt);
    opportunitySuspendSeconds_ = std::max(0.0f, opportunitySuspendSeconds_ - safeDt);
    objectiveSwitchWindowSeconds_ = std::max(0.0f, objectiveSwitchWindowSeconds_ - safeDt);
    if (runElapsedSeconds_ >= settings_.timeoutSeconds) {
        if (checkpointMeasurementMode_) {
            finishCheckpointMeasurement(snapshot, AutoSimulationResult::Timeout);
        } else {
            finish(snapshot, AutoSimulationResult::Timeout);
        }
        return;
    }

    if (snapshot.automationUiDirective == GameTestAutomationUiDirective::Wait) {
        inputFrame_.active = true;
        inputFrame_.exclusive = true;
        baseIdleSeconds_ = 0.0f;
        clearPlanLock();
        clearCachedPlan();
        navigator_.reset();
        updateDecisionDebugSnapshot(
            snapshot,
            "ui_wait",
            snapshot.automationUiReason.empty() ? "UI側から待機を要求された（詳細なし）" : snapshot.automationUiReason);
        return;
    }

    if (snapshot.automationUiDirective == GameTestAutomationUiDirective::Confirm) {
        baseIdleSeconds_ = 0.0f;
        clearPlanLock();
        clearCachedPlan();
        AutoSimulationPlan plan;
        plan.goal = AutoSimulationGoal::DismissUi;
        plan.confirm = true;
        plan.reason = "automation_ui_confirm";
        applyNavigationPlan(snapshot, plan, safeDt);
        updateDecisionDebugSnapshot(
            snapshot,
            "ui_confirm",
            snapshot.automationUiReason.empty() ? "UIを決定入力で進行中（詳細なし）" : snapshot.automationUiReason);
        return;
    }

    updateTraversalMemory(snapshot);
    updateTaskState(snapshot, safeDt);
    updateMissionState(snapshot, safeDt);
    if (!snapshot.worldLoading && !snapshot.transitionActive) {
        previousScreenMode_ = snapshot.screenMode;
        hasPreviousScreenMode_ = true;
    }

    if (snapshot.screenMode == GameTestScreenMode::GameOver) {
        clearPlanLock();
        clearCachedPlan();
        navigator_.reset();
        if (actionCooldownSeconds_ <= 0.0f) {
            GameTestAction action;
            action.kind = GameTestActionKind::ReturnToBaseAfterGameOver;
            action.reason = "game_over_return";
            queueAction(std::move(action));
            updateDecisionDebugSnapshot(snapshot, "game_over_action", pendingActionDecisionDetail(pendingAction_));
        } else {
            updateDecisionDebugSnapshot(snapshot, "game_over_cooldown", "ゲームオーバー帰還操作の再実行待ち");
        }
        return;
    }

    if (snapshot.screenMode == GameTestScreenMode::LevelUp && snapshot.levelUp.choiceActive) {
        clearPlanLock();
        clearCachedPlan();
        navigator_.reset();
        if (actionCooldownSeconds_ <= 0.0f) {
            if (std::optional<GameTestAction> action = levelUpPlanner_.chooseAction(snapshot)) {
                queueAction(std::move(*action));
                updateDecisionDebugSnapshot(snapshot, "level_up_action", pendingActionDecisionDetail(pendingAction_));
            } else {
                updateDecisionDebugSnapshot(snapshot, "level_up_blocked", "レベルアップ候補を選べなかった");
            }
        } else {
            updateDecisionDebugSnapshot(snapshot, "level_up_cooldown", "直前の操作後の待機中");
        }
        return;
    }

    if (snapshot.screenMode == GameTestScreenMode::Playing &&
        nearReturnPoint(snapshot) &&
        actionCooldownSeconds_ <= 0.0f) {
        const bool committedReturn = mission_.goal == AutoSimulationGoal::ReturnToBase;
        const bool returnForFullBackpack = committedReturn && mission_.reason.find("backpack_full") != std::string::npos;
        const bool returnForLowHp = committedReturn && mission_.reason.find("low_hp") != std::string::npos;
        const bool returnForCheckpointPrep = shouldReturnForCheckpointPrep(snapshot);
        if (committedReturn || returnForCheckpointPrep) {
            GameTestAction action;
            action.kind = GameTestActionKind::ReturnToBaseViaWarp;
            action.reason = returnForFullBackpack
                ? "backpack_full_near_warp"
                : (returnForLowHp ? "low_hp_near_warp" : "checkpoint_base_prep");
            queueAction(std::move(action));
            inputFrame_ = {};
            return;
        }
    }

    if (actionCooldownSeconds_ <= 0.0f) {
        if (std::optional<GameTestAction> action = consumablePlanner_.chooseAction(snapshot)) {
            queueAction(std::move(*action));
            if (snapshot.screenMode == GameTestScreenMode::Base) {
                updateDecisionDebugSnapshot(snapshot, "base_consumable_action", pendingActionDecisionDetail(pendingAction_));
            }
            return;
        }
        if (std::optional<GameTestAction> action = gearPlanner_.chooseAction(snapshot)) {
            queueAction(std::move(*action));
            if (snapshot.screenMode == GameTestScreenMode::Base) {
                updateDecisionDebugSnapshot(snapshot, "base_gear_action", pendingActionDecisionDetail(pendingAction_));
            }
            return;
        }
        if (snapshot.screenMode != GameTestScreenMode::Base) {
            if (std::optional<GameTestAction> action = loadoutPlanner_.chooseAction(snapshot)) {
                queueAction(std::move(*action));
                return;
            }
        }
    }

    if (snapshot.screenMode == GameTestScreenMode::Base) {
        inputFrame_.active = true;
        inputFrame_.exclusive = true;
        clearPlanLock();
        clearCachedPlan();
        navigator_.reset();
        if (actionCooldownSeconds_ > 0.0f) {
            std::ostringstream detail;
            detail << "直前の拠点操作後の待機中（残り" << actionCooldownSeconds_ << "秒）";
            updateDecisionDebugSnapshot(snapshot, "base_action_cooldown", detail.str());
            return;
        }
        if (std::optional<GameTestAction> action = baseTasks_.choosePreparationAction(snapshot)) {
            baseIdleSeconds_ = 0.0f;
            queueAction(std::move(*action));
            updateDecisionDebugSnapshot(snapshot, "base_preparation_action", pendingActionDecisionDetail(pendingAction_));
            return;
        }
        const bool backpackReady = AutoSimulationBaseTasks::backpackReadyForDeparture(snapshot.inventory);
        if (!backpackReady) {
            if (std::optional<GameTestAction> action = baseTasks_.chooseAction(snapshot)) {
                baseIdleSeconds_ = 0.0f;
                queueAction(std::move(*action));
                updateDecisionDebugSnapshot(snapshot, "base_cleanup_action", pendingActionDecisionDetail(pendingAction_));
                return;
            }
            if (!AutoSimulationBaseTasks::backpackCanDepart(snapshot.inventory)) {
                AutoSimulationIntent blockedIntent;
                blockedIntent.visible = true;
                blockedIntent.goal = AutoSimulationGoal::ReturnToBase;
                blockedIntent.iconKind = AutoSimulationIntentIconKind::Base;
                blockedIntent.subject = "荷物を安全に整理できず空き枠もないため停止した";
                recordIntent(std::move(blockedIntent));
                state_ = AutoSimulationState::Paused;
                updateDecisionDebugSnapshot(
                    snapshot,
                    "base_blocked",
                    "リュック満杯かつ、収納・売却・破棄できる候補がないため一時停止した");
                logWarning("AutoSim: paused because the full backpack has no safe cleanup action.");
                return;
            }

            const int freeSlots = std::max(
                0,
                snapshot.inventory.backpackCapacity - snapshot.inventory.backpackUsedSlots);
            AutoSimulationIntent compromiseIntent;
            compromiseIntent.visible = true;
            compromiseIntent.goal = AutoSimulationGoal::FollowMainPath;
            compromiseIntent.iconKind = AutoSimulationIntentIconKind::Base;
            compromiseIntent.subject = "整理できる荷物がないので空き" +
                std::to_string(freeSlots) + "枠で出発したい";
            recordIntent(std::move(compromiseIntent));
        }
        if (std::optional<GameTestAction> action = loadoutPlanner_.chooseAction(snapshot)) {
            baseIdleSeconds_ = 0.0f;
            queueAction(std::move(*action));
            updateDecisionDebugSnapshot(snapshot, "base_loadout_action", pendingActionDecisionDetail(pendingAction_));
            return;
        }
        if (std::optional<GameTestAction> action = baseTasks_.chooseAction(snapshot)) {
            baseIdleSeconds_ = 0.0f;
            queueAction(std::move(*action));
            updateDecisionDebugSnapshot(snapshot, "base_task_action", pendingActionDecisionDetail(pendingAction_));
            return;
        }
        baseIdleSeconds_ += safeDt;
        if (baseIdleSeconds_ >= BaseResumeDelaySeconds) {
            GameTestAction action;
            action.kind = GameTestActionKind::StartMiningFromBase;
            action.reason = "base_tasks_complete";
            baseIdleSeconds_ = 0.0f;
            queueAction(std::move(action));
            updateDecisionDebugSnapshot(snapshot, "base_departure_action", pendingActionDecisionDetail(pendingAction_));
        } else {
            std::ostringstream detail;
            detail << "拠点作業は完了。採掘開始までの安定待ち（"
                   << baseIdleSeconds_ << "/" << BaseResumeDelaySeconds << "秒）";
            updateDecisionDebugSnapshot(snapshot, "base_departure_delay", detail.str());
        }
        return;
    }

    baseIdleSeconds_ = 0.0f;
    const bool escapingStuck = escapeStuckSeconds_ > 0.0f;
    AutoSimulationPlan plan;
    if (canReuseCachedPlan(snapshot)) {
        planLockSeconds_ = std::max(0.0f, planLockSeconds_ - safeDt);
        plan = *cachedPlan_;
    } else {
        plan = stabilizePlan(
            snapshot,
            makeHierarchicalPlan(snapshot, escapingStuck),
            safeDt);
        cachePlanForFastForward(plan);
    }

    if (actionCooldownSeconds_ <= 0.0f) {
        if (std::optional<GameTestAction> action = ringPlanner_.chooseAction(snapshot, plan, safeDt)) {
            queueAction(std::move(*action));
            updateDebugSnapshot(snapshot, plan);
            return;
        }
    }

    applyNavigationPlan(snapshot, plan, dt);
}

void AutoSimulationController::applyNavigationPlan(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPlan& plan,
    float dt)
{
    updateStuckDetection(snapshot, plan, dt);
    recordIntent(intentFormatter_.format(snapshot, plan));
    escapeStuckSeconds_ = std::max(0.0f, escapeStuckSeconds_ - std::max(0.0f, dt));

    if (plan.goal != lastGoal_) {
        if (settings_.trace || plan.goal == AutoSimulationGoal::EscapeStuck) {
            logInfo("AutoSim: goal=" + std::string(autoSimulationGoalName(plan.goal)) +
                (plan.reason.empty() ? std::string{} : " reason=" + plan.reason));
        }
        lastGoal_ = plan.goal;
    }

    inputFrame_ = navigator_.makeInput(snapshot, plan, dt);
    updateDebugSnapshot(snapshot, plan);
}

void AutoSimulationController::populateCommonDebugSnapshot(
    AutoSimulationDebugSnapshot& debug,
    const GameTestSnapshot& snapshot) const
{
    debug.active = state_ != AutoSimulationState::Idle;
    debug.state = state_;
    debug.speedMultiplier = settings_.speedMultiplier;
    debug.simulationStepsLastFrame = simulationStepsLastFrame_;
    debug.missionGoal = mission_.goal;
    debug.taskGoal = task_.goal;
    debug.missionReason = mission_.reason;
    debug.taskReason = task_.reason;
    debug.lastActionResult = lastActionResult_;
    debug.playerWorld = snapshot.player.position;
    debug.inputMoveAxis = inputFrame_.moveAxis;
    debug.stuckMoveDistance = hasLastPlayerPosition_
        ? distanceBetween(snapshot.player.position, lastPlayerPosition_)
        : 0.0f;
    debug.activeLightRadius = snapshot.ring.bestLightRadius;
    debug.bestBackpackLightRadius = bestBackpackLightRadius(snapshot);
    debug.missingLight = snapshot.ring.bestLightRadius < MeaningfulLightRadius;
    debug.lockedPlanActive = lockedPlan_.has_value();
    debug.planLockSeconds = planLockSeconds_;
    debug.stuckCount = stuckCount_;
    debug.stillSeconds = stillSeconds_;
    debug.miningNoProgressSeconds = miningNoProgressSeconds_;
    debug.escapeStuckSeconds = escapeStuckSeconds_;
    debug.missionNoProgressSeconds = missionNoProgressSeconds_;
    debug.opportunitySuspendSeconds = opportunitySuspendSeconds_;
    debug.opportunityBudget = opportunityBudget_;
    debug.baseScreen = snapshot.screenMode == GameTestScreenMode::Base;
    debug.backpackUsedSlots = snapshot.inventory.backpackUsedSlots;
    debug.backpackCapacity = snapshot.inventory.backpackCapacity;
    debug.backpackFreeSlots = std::max(
        0,
        snapshot.inventory.backpackCapacity - snapshot.inventory.backpackUsedSlots);
    debug.desiredBackpackFreeSlots = AutoSimulationBaseTasks::desiredBackpackFreeSlots(snapshot.inventory);
    debug.backpackReadyForDeparture = AutoSimulationBaseTasks::backpackReadyForDeparture(snapshot.inventory);
    debug.backpackCanDepart = AutoSimulationBaseTasks::backpackCanDepart(snapshot.inventory);
    debug.warehouseUsedSlots = snapshot.inventory.warehouseUsedSlots;
    debug.warehouseCapacity = snapshot.inventory.warehouseCapacity;
    debug.enhancementBudgetLimit = baseTasks_.enhancementBudgetLimit();
    debug.enhancementBudgetSpent = baseTasks_.enhancementBudgetSpent();
    debug.enhancementBudgetRemaining = baseTasks_.enhancementBudgetRemaining();
    debug.actionCooldownSeconds = actionCooldownSeconds_;
    debug.baseIdleSeconds = baseIdleSeconds_;
    debug.pendingAction = pendingAction_.has_value();
    debug.totalWarpPoints = static_cast<int>(snapshot.dungeon.warpPoints.size());
    debug.discoveredWarpPoints = snapshot.dungeon.discoveredWarpPoints;
    debug.unlockedWarpPoints = snapshot.dungeon.unlockedWarpPoints;
    debug.knownWarpPoints = knownWarpCount(snapshot);

    if (const GameTestWarpPointSnapshot* nearest = nearestWarpToPlayer(snapshot)) {
        debug.nearestWarpIndex = nearest->index;
        debug.nearestWarpDiscovered = nearest->discovered;
        debug.nearestWarpKnown = knownWarpDiscovered(snapshot, *nearest);
        debug.nearestWarpDistance = distanceBetween(snapshot.player.position, nearest->position);
    }

    if (const GameTestWarpPointSnapshot* nextWarp = nextUnknownWarp(snapshot)) {
        debug.nextUnknownWarpIndex = nextWarp->index;
        debug.nextUnknownWarpDiscovered = nextWarp->discovered;
        debug.nextUnknownWarpDistance = distanceBetween(snapshot.player.position, nextWarp->position);
    }
}

void AutoSimulationController::updateDecisionDebugSnapshot(
    const GameTestSnapshot& snapshot,
    std::string phase,
    std::string detail)
{
    AutoSimulationDebugSnapshot debug;
    populateCommonDebugSnapshot(debug, snapshot);
    debug.decisionPhase = std::move(phase);
    debug.decisionDetail = std::move(detail);
    debugSnapshot_ = std::move(debug);
}

void AutoSimulationController::updateDebugSnapshot(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPlan& plan)
{
    AutoSimulationDebugSnapshot debug;
    populateCommonDebugSnapshot(debug, snapshot);
    debug.hasPlan = true;
    debug.goal = plan.goal;
    debug.reason = plan.reason;
    debug.decisionPhase = "navigation";
    debug.decisionDetail = plan.reason;
    debug.targetWorld = plan.targetWorld;
    debug.moveTargetWorld = plan.moveTargetWorld;
    debug.aimTargetWorld = plan.aimTargetWorld;
    debug.hasTarget = plan.hasTarget;
    debug.hasMoveTarget = plan.hasMoveTarget;
    debug.hasAimTarget = plan.hasAimTarget;
    debug.distanceToTarget = plan.hasTarget ? distanceBetween(snapshot.player.position, plan.targetWorld) : 0.0f;
    debug.distanceToMoveTarget = plan.hasMoveTarget ? distanceBetween(snapshot.player.position, plan.moveTargetWorld) : 0.0f;
    debug.moveTargetArriveDistance = plan.moveTargetArriveDistance;
    debug.routePathTileCount = plan.routePathTileCount;
    debug.routeWaypointPathIndex = plan.routeWaypointPathIndex;
    debug.routeFirstDigPathIndex = plan.routeFirstDigPathIndex;
    debug.routeDigTileCount = plan.routeDigTileCount;
    debug.routeHardTileCount = plan.routeHardTileCount;
    debug.routeAvoidingHardWall = plan.routeAvoidingHardWall;

    if (planTargetsWarpPoint(plan)) {
        if (const GameTestWarpPointSnapshot* targetWarp = nearestWarpToPoint(snapshot, plan.targetWorld)) {
            debug.targetWarpIndex = targetWarp->index;
            debug.targetWarpDiscovered = targetWarp->discovered;
            debug.targetWarpKnown = knownWarpDiscovered(snapshot, *targetWarp);
            debug.targetWarpDistance = distanceBetween(plan.targetWorld, targetWarp->position);
        }
    }

    debugSnapshot_ = std::move(debug);
}

void AutoSimulationController::updateStuckDetection(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPlan& plan,
    float dt)
{
    if (snapshot.screenMode == GameTestScreenMode::Playing &&
        plan.goal == AutoSimulationGoal::MineWall &&
        plan.hasTarget) {
        const std::optional<int> targetHp = miningTargetHp(snapshot, plan);
        const bool targetChanged =
            !hasLastMiningTarget_ ||
            distanceSquared(plan.targetWorld, lastMiningTarget_) > 4.0f;
        const bool progress =
            snapshot.runStats.dugTiles != lastMiningDugTiles_ ||
            (targetHp && lastMiningTargetHp_ >= 0 && *targetHp < lastMiningTargetHp_);

        if (targetChanged || progress) {
            miningNoProgressSeconds_ = 0.0f;
            lastMiningTarget_ = plan.targetWorld;
            lastMiningTargetHp_ = targetHp.value_or(-1);
            lastMiningDugTiles_ = snapshot.runStats.dugTiles;
            hasLastMiningTarget_ = true;
            lastPlayerPosition_ = snapshot.player.position;
            hasLastPlayerPosition_ = true;
            return;
        }

        miningNoProgressSeconds_ += std::max(0.0f, dt);
        if (miningNoProgressSeconds_ >= MiningNoProgressThresholdSeconds) {
            ++stuckCount_;
            miningNoProgressSeconds_ = 0.0f;
            escapeStuckSeconds_ = EscapeStuckDurationSeconds;
            hasLastMiningTarget_ = false;
            clearPlanLock();
            logInfo("AutoSim: mining recovery #" + std::to_string(stuckCount_) + ".");
        }
        return;
    }

    miningNoProgressSeconds_ = 0.0f;
    hasLastMiningTarget_ = false;
    lastMiningTargetHp_ = -1;
    lastMiningDugTiles_ = snapshot.runStats.dugTiles;

    if (snapshot.screenMode != GameTestScreenMode::Playing ||
        !plan.hasTarget ||
        plan.goal == AutoSimulationGoal::Combat ||
        plan.goal == AutoSimulationGoal::DismissUi) {
        stillSeconds_ = 0.0f;
        lastPlayerPosition_ = snapshot.player.position;
        hasLastPlayerPosition_ = true;
        return;
    }

    if (!hasLastPlayerPosition_) {
        lastPlayerPosition_ = snapshot.player.position;
        hasLastPlayerPosition_ = true;
        return;
    }

    if (distanceSquared(snapshot.player.position, lastPlayerPosition_) >= StuckMovementThreshold * StuckMovementThreshold) {
        stillSeconds_ = 0.0f;
        lastPlayerPosition_ = snapshot.player.position;
        return;
    }

    stillSeconds_ += std::max(0.0f, dt);
    if (stillSeconds_ >= StuckSecondsThreshold) {
        ++stuckCount_;
        stillSeconds_ = 0.0f;
        escapeStuckSeconds_ = EscapeStuckDurationSeconds;
        clearPlanLock();
        logInfo("AutoSim: stuck recovery #" + std::to_string(stuckCount_) + ".");
    }
}

AutoSimulationRunRecord AutoSimulationController::makeRecord(
    const GameTestSnapshot& snapshot,
    AutoSimulationResult result) const
{
    AutoSimulationRunRecord record;
    record.runIndex = runIndex_;
    record.stageId = snapshot.stageId;
    record.stageName = snapshot.stageName;
    record.seed = snapshot.dungeon.seed;
    record.result = result;
    record.elapsedSeconds = runElapsedSeconds_;
    record.playerLevel = snapshot.player.level;
    record.hp = snapshot.player.hp;
    record.maxHp = snapshot.player.maxHp;
    record.dugTiles = snapshot.runStats.dugTiles;
    record.defeatedEnemies = snapshot.runStats.defeatedEnemies;
    record.acquiredItems = snapshot.runStats.acquiredItems;
    record.acquiredObjectItems = snapshot.runStats.acquiredObjectItems;
    record.money = snapshot.money;
    record.totalMaterials = snapshot.totalMaterials;
    record.discoveredWarpPoints = snapshot.dungeon.discoveredWarpPoints;
    record.totalWarpPoints = static_cast<int>(snapshot.dungeon.warpPoints.size());
    record.bossDefeated = result == AutoSimulationResult::StageClear ||
        result == AutoSimulationResult::AstralResult;
    record.timeout = result == AutoSimulationResult::Timeout;
    record.stuckCount = stuckCount_;
    return record;
}

bool AutoSimulationController::canStartFromSnapshot(const GameTestSnapshot& snapshot)
{
    return snapshot.worldLoading ||
        snapshot.screenMode == GameTestScreenMode::Base ||
        snapshot.screenMode == GameTestScreenMode::Playing ||
        snapshot.screenMode == GameTestScreenMode::LevelUp ||
        snapshot.screenMode == GameTestScreenMode::Inventory ||
        snapshot.screenMode == GameTestScreenMode::PauseMenu ||
        snapshot.screenMode == GameTestScreenMode::Ring;
}

bool AutoSimulationController::backpackFull(const GameTestSnapshot& snapshot)
{
    return snapshot.inventory.backpackCapacity > 0 &&
        snapshot.inventory.backpackUsedSlots >= snapshot.inventory.backpackCapacity;
}

bool AutoSimulationController::nearReturnPoint(const GameTestSnapshot& snapshot)
{
    if (snapshot.screenMode != GameTestScreenMode::Playing) {
        return false;
    }
    if (distanceSquared(snapshot.player.position, snapshot.dungeon.startWorld) <= ReturnActionRadius * ReturnActionRadius) {
        return true;
    }
    return std::any_of(
        snapshot.dungeon.warpPoints.begin(),
        snapshot.dungeon.warpPoints.end(),
        [&snapshot](const GameTestWarpPointSnapshot& point) {
            return point.returnInteractionArmed &&
                knownWarpDiscovered(snapshot, point) &&
                distanceSquared(snapshot.player.position, point.position) <= ReturnActionRadius * ReturnActionRadius;
        });
}

bool AutoSimulationController::shouldReturnForCheckpointPrep(const GameTestSnapshot& snapshot) const
{
    if (snapshot.screenMode != GameTestScreenMode::Playing || !nearReturnPoint(snapshot)) {
        return false;
    }
    if (knownWarpCount(snapshot) <= lastBaseVisitKnownWarpCount_) {
        return false;
    }

    GameTestSnapshot baseSnapshot = snapshot;
    baseSnapshot.screenMode = GameTestScreenMode::Base;
    baseSnapshot.base.active = true;
    baseSnapshot.worldLoading = false;
    baseSnapshot.transitionActive = false;
    baseSnapshot.dialogueActive = false;
    baseSnapshot.pendingStoryDelayActive = false;
    baseSnapshot.firstItemNoticeActive = false;
    return baseTasks_.chooseCheckpointPrepAction(baseSnapshot).has_value();
}

} // namespace majo::autosim
