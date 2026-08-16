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
    UseItem,
    MineWall,
    Combat,
    CollectDrop,
    OpenChest,
    DiscoverWarp,
    ReturnToBase,
    ResumeFrontier,
    ApproachBoss,
    FollowMainPath,
    EscapeStuck,
};

enum class AutoSimulationRingRole {
    None,
    Combat,
    Dig,
    Light,
    Utility,
};

enum class AutoSimulationDigPolicy {
    Avoid,
    CheapOnly,
    Required,
};

inline constexpr float DefaultAutoSimulationTimeoutSeconds = 2.0f * 60.0f * 60.0f;

struct AutoSimulationSettings {
    int requestedRuns = 1;
    float timeoutSeconds = DefaultAutoSimulationTimeoutSeconds;
    int speedMultiplier = 1;
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
    AutoSimulationGoal objectiveGoal = AutoSimulationGoal::None;
    Vec2 targetWorld{};
    Vec2 objectiveTargetWorld{};
    Vec2 moveTargetWorld{};
    Vec2 aimTargetWorld{};
    bool hasTarget = false;
    bool hasObjectiveTarget = false;
    bool hasMoveTarget = false;
    bool hasAimTarget = false;
    bool confirm = false;
    bool throwRing = false;
    bool ringOffset = false;
    bool ringOffsetAwayFromAim = false;
    bool ringOffsetRequiresMoveTarget = false;
    float ringOffsetMoveTargetDistance = 18.0f;
    bool moveAwayFromTarget = false;
    bool rangeControl = false;
    bool alignMoveTargetInRange = false;
    float moveTargetArriveDistance = 22.0f;
    float desiredRangeMin = 0.0f;
    float desiredRangeMax = 0.0f;
    bool strafe = false;
    AutoSimulationRingRole preferredRingRole = AutoSimulationRingRole::None;
    AutoSimulationDigPolicy digPolicy = AutoSimulationDigPolicy::Avoid;
    GameTestTerrainKind targetTerrainKind = GameTestTerrainKind::Empty;
    bool hasTargetTerrainKind = false;
    int routePathTileCount = 0;
    int routeWaypointPathIndex = -1;
    int routeFirstDigPathIndex = -1;
    int routeDigTileCount = 0;
    int routeHardTileCount = 0;
    bool routeAvoidingHardWall = false;
    float routeTotalCost = 0.0f;
    float routeDigCost = 0.0f;
    float routeExpectedDigHits = 0.0f;
    std::string reason;
};

struct AutoSimulationDebugSnapshot {
    bool active = false;
    AutoSimulationState state = AutoSimulationState::Idle;
    int speedMultiplier = 1;
    int simulationStepsLastFrame = 0;
    bool hasPlan = false;
    AutoSimulationGoal goal = AutoSimulationGoal::None;
    AutoSimulationGoal missionGoal = AutoSimulationGoal::None;
    AutoSimulationGoal taskGoal = AutoSimulationGoal::None;
    std::string missionReason;
    std::string taskReason;
    std::string reason;
    std::string decisionPhase;
    std::string decisionDetail;
    std::string lastActionResult;
    Vec2 playerWorld{};
    Vec2 targetWorld{};
    Vec2 moveTargetWorld{};
    Vec2 aimTargetWorld{};
    bool hasTarget = false;
    bool hasMoveTarget = false;
    bool hasAimTarget = false;
    float distanceToTarget = 0.0f;
    float distanceToMoveTarget = 0.0f;
    float moveTargetArriveDistance = 0.0f;
    Vec2 inputMoveAxis{};
    float stuckMoveDistance = 0.0f;
    float activeLightRadius = 0.0f;
    float bestBackpackLightRadius = 0.0f;
    bool missingLight = false;
    int routePathTileCount = 0;
    int routeWaypointPathIndex = -1;
    int routeFirstDigPathIndex = -1;
    int routeDigTileCount = 0;
    int routeHardTileCount = 0;
    bool routeAvoidingHardWall = false;
    bool lockedPlanActive = false;
    float planLockSeconds = 0.0f;
    int stuckCount = 0;
    float stillSeconds = 0.0f;
    float miningNoProgressSeconds = 0.0f;
    float escapeStuckSeconds = 0.0f;
    float missionNoProgressSeconds = 0.0f;
    float opportunitySuspendSeconds = 0.0f;
    int opportunityBudget = 0;
    bool baseScreen = false;
    int backpackUsedSlots = 0;
    int backpackCapacity = 0;
    int backpackFreeSlots = 0;
    int desiredBackpackFreeSlots = 0;
    bool backpackReadyForDeparture = false;
    bool backpackCanDepart = false;
    int warehouseUsedSlots = 0;
    int warehouseCapacity = 0;
    int optionalSpendBudgetLimit = 0;
    int optionalSpendBudgetSpent = 0;
    int optionalSpendBudgetRemaining = 0;
    float actionCooldownSeconds = 0.0f;
    float baseIdleSeconds = 0.0f;
    bool pendingAction = false;
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
