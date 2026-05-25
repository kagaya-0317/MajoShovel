#include "devtools/autosim/AutoSimulationController.hpp"

#include "engine/Log.hpp"

#include <algorithm>
#include <cmath>
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
constexpr float ReturnActionRadius = 64.0f;
constexpr float TargetSameDistance = 28.0f;
constexpr float MineTargetSameDistance = 4.0f;

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
    case AutoSimulationGoal::ApproachBoss:
    case AutoSimulationGoal::FollowMainPath:
        return true;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
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
    case AutoSimulationGoal::ApproachBoss: return 0.90f;
    case AutoSimulationGoal::FollowMainPath: return 0.45f;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
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
    case AutoSimulationGoal::ApproachBoss: return 40;
    case AutoSimulationGoal::FollowMainPath: return 10;
    case AutoSimulationGoal::EquipLoadout: return 8;
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
    case AutoSimulationGoal::ApproachBoss:
    case AutoSimulationGoal::FollowMainPath:
        return true;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
    case AutoSimulationGoal::MineWall:
    case AutoSimulationGoal::Combat:
    case AutoSimulationGoal::EscapeStuck:
        return false;
    }
    return false;
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
    case GameTestActionKind::SyncEncyclopedia:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.iconKind = AutoSimulationIntentIconKind::Base;
        intent.subject = "図鑑を確認したい";
        break;
    case GameTestActionKind::EquipBackpackStaff:
        intent.goal = AutoSimulationGoal::EquipLoadout;
        intent.iconKind = action.objectId.empty() ? AutoSimulationIntentIconKind::None : AutoSimulationIntentIconKind::Object;
        intent.iconKey = action.objectId;
        intent.prefix = "杖として";
        intent.subject = "アイテム";
        intent.suffix = "を装備したい";
        break;
    case GameTestActionKind::DepositBackpackStack:
    case GameTestActionKind::DepositBackpackInstance:
    case GameTestActionKind::SellBackpackStack:
    case GameTestActionKind::SellBackpackInstance:
        intent.goal = AutoSimulationGoal::ReturnToBase;
        intent.iconKind = AutoSimulationIntentIconKind::Base;
        intent.subject = "拠点で荷物を整理したい";
        break;
    case GameTestActionKind::ProtectBackpackInstance:
    case GameTestActionKind::RepairBackpackInstance:
    case GameTestActionKind::EnhanceBackpackStackAttack:
    case GameTestActionKind::EnhanceBackpackStackDig:
    case GameTestActionKind::EnhanceBackpackInstanceAttack:
    case GameTestActionKind::EnhanceBackpackInstanceDig:
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
    case GameTestActionKind::None:
        intent.visible = false;
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

bool knownWarpDiscovered(const GameTestSnapshot& snapshot, const GameTestWarpPointSnapshot& warp)
{
    return warp.discovered || warp.index < knownWarpCount(snapshot);
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
        return plan.reason.rfind("warp", 0) == 0;
    }
    return false;
}

bool chestTargetExists(const GameTestSnapshot& snapshot, Vec2 target)
{
    return std::any_of(snapshot.chests.begin(), snapshot.chests.end(), [target](const GameTestChestSnapshot& chest) {
        return chest.revealed &&
            !chest.opened &&
            distanceSquared(chest.position, target) <= TargetSameDistance * TargetSameDistance;
    });
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
    case AutoSimulationGoal::FollowMainPath:
        return true;
    case AutoSimulationGoal::ApproachBoss:
        return snapshot.dungeon.hasBossSpawnPoint && !snapshot.dungeon.bossSpawned;
    case AutoSimulationGoal::None:
    case AutoSimulationGoal::DismissUi:
    case AutoSimulationGoal::EquipLoadout:
    case AutoSimulationGoal::Combat:
    case AutoSimulationGoal::EscapeStuck:
        return false;
    }
    return false;
}

} // namespace

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
    if (action == "stop") {
        if (state_ == AutoSimulationState::Running || state_ == AutoSimulationState::Paused) {
            finish(snapshot, AutoSimulationResult::Stopped);
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

    state_ = AutoSimulationState::Running;
    inputFrame_ = {};
    pendingAction_.reset();
    intentHistory_.clear();
    clearPlanLock();
    navigator_.reset();
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
    debugSnapshot_ = {};
    debugSnapshot_.active = true;
    debugSnapshot_.state = state_;
    debugSnapshot_.playerWorld = snapshot.player.position;
    debugSnapshot_.totalWarpPoints = static_cast<int>(snapshot.dungeon.warpPoints.size());
    debugSnapshot_.discoveredWarpPoints = snapshot.dungeon.discoveredWarpPoints;
    debugSnapshot_.unlockedWarpPoints = snapshot.dungeon.unlockedWarpPoints;
    debugSnapshot_.knownWarpPoints = knownWarpCount(snapshot);

    logInfo("AutoSim: started run " + std::to_string(runIndex_) +
        " stage=" + snapshot.stageId +
        " seed=" + std::to_string(snapshot.dungeon.seed) + ".");
}

void AutoSimulationController::finish(const GameTestSnapshot& snapshot, AutoSimulationResult result)
{
    const AutoSimulationRunRecord record = makeRecord(snapshot, result);
    state_ = AutoSimulationState::Idle;
    inputFrame_ = {};
    pendingAction_.reset();
    intentHistory_.clear();
    clearPlanLock();
    navigator_.reset();
    debugSnapshot_ = {};
    logger_.recordRun(record);

    logInfo("AutoSim: finished run " + std::to_string(record.runIndex) +
        " result=" + autoSimulationResultName(result) +
        " elapsed=" + std::to_string(static_cast<int>(record.elapsedSeconds)) + "s" +
        " log=" + logger_.csvPath().string());
}

void AutoSimulationController::report() const
{
    logger_.writeSummary();
    logInfo("AutoSim: state=" + std::string(autoSimulationStateName(state_)) +
        " run=" + std::to_string(runIndex_) +
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

void AutoSimulationController::recordActionResult(const GameTestAction& action, const GameTestActionResult& result)
{
    if (!settings_.trace && result.applied) {
        return;
    }
    logInfo("AutoSim: action=" + std::string(gameTestActionKindName(action.kind)) +
        " result=" + (result.applied ? "applied" : "skipped") +
        (action.reason.empty() ? std::string{} : " reason=" + action.reason) +
        (result.message.empty() ? std::string{} : " message=" + result.message));
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
        return;
    }
    if (pendingAction_) {
        return;
    }

    if (const AutoSimulationResult result = resultForSnapshot(snapshot); result != AutoSimulationResult::None) {
        finish(snapshot, result);
        return;
    }

    const float safeDt = std::max(0.0f, dt);
    runElapsedSeconds_ += safeDt;
    actionCooldownSeconds_ = std::max(0.0f, actionCooldownSeconds_ - safeDt);
    if (runElapsedSeconds_ >= settings_.timeoutSeconds) {
        finish(snapshot, AutoSimulationResult::Timeout);
        return;
    }

    if (snapshot.screenMode == GameTestScreenMode::GameOver) {
        clearPlanLock();
        navigator_.reset();
        if (actionCooldownSeconds_ <= 0.0f) {
            GameTestAction action;
            action.kind = GameTestActionKind::ReturnToBaseAfterGameOver;
            action.reason = "game_over_return";
            queueAction(std::move(action));
        }
        return;
    }

    if (actionCooldownSeconds_ <= 0.0f) {
        if (std::optional<GameTestAction> action = loadoutPlanner_.chooseAction(snapshot)) {
            queueAction(std::move(*action));
            return;
        }
    }

    if (snapshot.screenMode == GameTestScreenMode::Base) {
        clearPlanLock();
        navigator_.reset();
        if (actionCooldownSeconds_ > 0.0f) {
            return;
        }
        if (std::optional<GameTestAction> action = baseTasks_.chooseAction(snapshot)) {
            baseIdleSeconds_ = 0.0f;
            queueAction(std::move(*action));
            return;
        }
        if (!backpackFull(snapshot)) {
            baseIdleSeconds_ += safeDt;
            if (baseIdleSeconds_ >= BaseResumeDelaySeconds) {
                GameTestAction action;
                action.kind = GameTestActionKind::StartMiningFromBase;
                action.reason = "base_tasks_complete";
                baseIdleSeconds_ = 0.0f;
                queueAction(std::move(action));
            }
        }
        return;
    }

    baseIdleSeconds_ = 0.0f;
    if (snapshot.screenMode == GameTestScreenMode::Playing &&
        nearReturnPoint(snapshot) &&
        actionCooldownSeconds_ <= 0.0f &&
        (backpackFull(snapshot) || shouldReturnForCheckpointPrep(snapshot))) {
        GameTestAction action;
        action.kind = GameTestActionKind::ReturnToBaseViaWarp;
        action.reason = backpackFull(snapshot) ? "backpack_full_near_warp" : "checkpoint_base_prep";
        queueAction(std::move(action));
        inputFrame_ = {};
        return;
    }

    const bool escapingStuck = escapeStuckSeconds_ > 0.0f;
    AutoSimulationPlan plan = stabilizePlan(
        snapshot,
        planner_.makePlan(snapshot, escapingStuck),
        safeDt);
    updateStuckDetection(snapshot, plan, dt);
    recordIntent(intentFormatter_.format(snapshot, plan));
    escapeStuckSeconds_ = std::max(0.0f, escapeStuckSeconds_ - std::max(0.0f, dt));
    updateDebugSnapshot(snapshot, plan);

    if (plan.goal != lastGoal_) {
        if (settings_.trace || plan.goal == AutoSimulationGoal::EscapeStuck) {
            logInfo("AutoSim: goal=" + std::string(autoSimulationGoalName(plan.goal)) +
                (plan.reason.empty() ? std::string{} : " reason=" + plan.reason));
        }
        lastGoal_ = plan.goal;
    }

    inputFrame_ = navigator_.makeInput(snapshot, plan, dt);
}

void AutoSimulationController::updateDebugSnapshot(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPlan& plan)
{
    AutoSimulationDebugSnapshot debug;
    debug.active = state_ != AutoSimulationState::Idle;
    debug.state = state_;
    debug.hasPlan = true;
    debug.goal = plan.goal;
    debug.reason = plan.reason;
    debug.playerWorld = snapshot.player.position;
    debug.targetWorld = plan.targetWorld;
    debug.moveTargetWorld = plan.moveTargetWorld;
    debug.aimTargetWorld = plan.aimTargetWorld;
    debug.hasTarget = plan.hasTarget;
    debug.hasMoveTarget = plan.hasMoveTarget;
    debug.hasAimTarget = plan.hasAimTarget;
    debug.distanceToTarget = plan.hasTarget ? distanceBetween(snapshot.player.position, plan.targetWorld) : 0.0f;
    debug.distanceToMoveTarget = plan.hasMoveTarget ? distanceBetween(snapshot.player.position, plan.moveTargetWorld) : 0.0f;
    debug.routeDigTileCount = plan.routeDigTileCount;
    debug.routeHardTileCount = plan.routeHardTileCount;
    debug.routeAvoidingHardWall = plan.routeAvoidingHardWall;
    debug.lockedPlanActive = lockedPlan_.has_value();
    debug.planLockSeconds = planLockSeconds_;
    debug.stuckCount = stuckCount_;
    debug.stillSeconds = stillSeconds_;
    debug.miningNoProgressSeconds = miningNoProgressSeconds_;
    debug.escapeStuckSeconds = escapeStuckSeconds_;
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

    if (planTargetsWarpPoint(plan)) {
        if (const GameTestWarpPointSnapshot* targetWarp = nearestWarpToPoint(snapshot, plan.targetWorld)) {
            debug.targetWarpIndex = targetWarp->index;
            debug.targetWarpDiscovered = targetWarp->discovered;
            debug.targetWarpKnown = knownWarpDiscovered(snapshot, *targetWarp);
            debug.targetWarpDistance = distanceBetween(plan.targetWorld, targetWarp->position);
        }
    }

    if (const GameTestWarpPointSnapshot* nextWarp = nextUnknownWarp(snapshot)) {
        debug.nextUnknownWarpIndex = nextWarp->index;
        debug.nextUnknownWarpDiscovered = nextWarp->discovered;
        debug.nextUnknownWarpDistance = distanceBetween(snapshot.player.position, nextWarp->position);
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
            return knownWarpDiscovered(snapshot, point) &&
                distanceSquared(snapshot.player.position, point.position) <= ReturnActionRadius * ReturnActionRadius;
        });
}

bool AutoSimulationController::shouldReturnForCheckpointPrep(const GameTestSnapshot& snapshot) const
{
    if (snapshot.screenMode != GameTestScreenMode::Playing || !nearReturnPoint(snapshot)) {
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
    return baseTasks_.chooseAction(baseSnapshot).has_value();
}

} // namespace majo::autosim
