#pragma once

#include "engine/Math.hpp"
#include "game/GameTestProbe.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace majo::autosim {

enum class AutoSimulationState {
    Idle,
    Running,
    Paused,
};

enum class AutoSimulationResult {
    None,
    StageClear,
    GameOver,
    AstralResult,
    Timeout,
    Stopped,
};

enum class AutoSimulationGoal {
    None,
    DismissUi,
    EquipLoadout,
    MineWall,
    Combat,
    CollectDrop,
    OpenChest,
    DiscoverWarp,
    ReturnToBase,
    ApproachBoss,
    FollowMainPath,
    EscapeStuck,
};

struct AutoSimulationSettings {
    int requestedRuns = 1;
    float timeoutSeconds = 600.0f;
    bool trace = false;
};

enum class AutoSimulationIntentIconKind {
    None,
    Object,
    World,
    Chest,
    Warp,
    Enemy,
    Dig,
    Path,
    Base,
};

struct AutoSimulationIntent {
    bool visible = false;
    AutoSimulationGoal goal = AutoSimulationGoal::None;
    AutoSimulationIntentIconKind iconKind = AutoSimulationIntentIconKind::None;
    std::string iconKey;
    std::string prefix;
    std::string subject;
    std::string suffix;
    Vec2 targetWorld{};
    bool hasTarget = false;
};

struct AutoSimulationPlan {
    AutoSimulationGoal goal = AutoSimulationGoal::None;
    Vec2 targetWorld{};
    Vec2 moveTargetWorld{};
    Vec2 aimTargetWorld{};
    bool hasTarget = false;
    bool hasMoveTarget = false;
    bool hasAimTarget = false;
    bool confirm = false;
    bool throwRing = false;
    bool ringOffset = false;
    bool ringOffsetRequiresMoveTarget = false;
    float ringOffsetMoveTargetDistance = 18.0f;
    bool moveAwayFromTarget = false;
    bool rangeControl = false;
    bool alignMoveTargetInRange = false;
    float moveTargetArriveDistance = 22.0f;
    float desiredRangeMin = 0.0f;
    float desiredRangeMax = 0.0f;
    bool strafe = false;
    GameTestTerrainKind targetTerrainKind = GameTestTerrainKind::Empty;
    bool hasTargetTerrainKind = false;
    int routeDigTileCount = 0;
    int routeHardTileCount = 0;
    bool routeAvoidingHardWall = false;
    std::string reason;
};

struct AutoSimulationDebugSnapshot {
    bool active = false;
    AutoSimulationState state = AutoSimulationState::Idle;
    bool hasPlan = false;
    AutoSimulationGoal goal = AutoSimulationGoal::None;
    std::string reason;
    Vec2 playerWorld{};
    Vec2 targetWorld{};
    Vec2 moveTargetWorld{};
    Vec2 aimTargetWorld{};
    bool hasTarget = false;
    bool hasMoveTarget = false;
    bool hasAimTarget = false;
    float distanceToTarget = 0.0f;
    float distanceToMoveTarget = 0.0f;
    int routeDigTileCount = 0;
    int routeHardTileCount = 0;
    bool routeAvoidingHardWall = false;
    bool lockedPlanActive = false;
    float planLockSeconds = 0.0f;
    int stuckCount = 0;
    float stillSeconds = 0.0f;
    float miningNoProgressSeconds = 0.0f;
    float escapeStuckSeconds = 0.0f;
    int totalWarpPoints = 0;
    int discoveredWarpPoints = 0;
    int unlockedWarpPoints = 0;
    int knownWarpPoints = 0;
    int nearestWarpIndex = -1;
    bool nearestWarpDiscovered = false;
    bool nearestWarpKnown = false;
    float nearestWarpDistance = 0.0f;
    int targetWarpIndex = -1;
    bool targetWarpDiscovered = false;
    bool targetWarpKnown = false;
    float targetWarpDistance = 0.0f;
    int nextUnknownWarpIndex = -1;
    bool nextUnknownWarpDiscovered = false;
    float nextUnknownWarpDistance = 0.0f;
};

struct AutoSimulationRunRecord {
    int runIndex = 0;
    std::string stageId;
    std::string stageName;
    std::uint32_t seed = 0;
    AutoSimulationResult result = AutoSimulationResult::None;
    float elapsedSeconds = 0.0f;
    int playerLevel = 1;
    int hp = 0;
    int maxHp = 0;
    int dugTiles = 0;
    int defeatedEnemies = 0;
    int acquiredItems = 0;
    int acquiredObjectItems = 0;
    int money = 0;
    int totalMaterials = 0;
    int discoveredWarpPoints = 0;
    int totalWarpPoints = 0;
    bool bossDefeated = false;
    bool timeout = false;
    int stuckCount = 0;
};

const char* autoSimulationStateName(AutoSimulationState state);
const char* autoSimulationResultName(AutoSimulationResult result);
const char* autoSimulationGoalName(AutoSimulationGoal goal);

} // namespace majo::autosim
