#pragma once

#include "devtools/autosim/AutoSimulationBaseTasks.hpp"
#include "devtools/autosim/AutoSimulationConsumablePlanner.hpp"
#include "devtools/autosim/AutoSimulationGearPlanner.hpp"
#include "devtools/autosim/AutoSimulationIntentFormatter.hpp"
#include "devtools/autosim/AutoSimulationLevelUpPlanner.hpp"
#include "devtools/autosim/AutoSimulationLoadoutPlanner.hpp"
#include "devtools/autosim/AutoSimulationLogger.hpp"
#include "devtools/autosim/AutoSimulationNavigator.hpp"
#include "devtools/autosim/AutoSimulationPlanner.hpp"
#include "devtools/autosim/AutoSimulationRingPlanner.hpp"
#include "devtools/autosim/AutoSimulationTypes.hpp"
#include "engine/Input.hpp"
#include "game/GameTestAction.hpp"
#include "game/GameTestProbe.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace majo::autosim {

class AutoSimulationController {
public:
    bool executeCommand(std::string_view normalizedCommand, const GameTestSnapshot& snapshot);
    void update(const GameTestSnapshot& snapshot, float dt);

    GameTestSnapshotOptions snapshotOptionsForNextStep() const;
    const InputAutomationFrame& inputFrame() const { return inputFrame_; }
    std::optional<GameTestAction> consumeAction();
    void recordActionResult(
        const GameTestAction& action,
        const GameTestActionResult& result,
        const GameTestSnapshot& snapshot);
    bool active() const { return state_ == AutoSimulationState::Running; }
    AutoSimulationState state() const { return state_; }
    int speedMultiplier() const { return settings_.speedMultiplier; }
    bool adjustSpeedMultiplier(int delta);
    void setSimulationStepsLastFrame(int steps);
    const std::vector<AutoSimulationIntent>& intentHistory() const { return intentHistory_; }
    const AutoSimulationDebugSnapshot& debugSnapshot() const { return debugSnapshot_; }
    std::optional<std::filesystem::path> consumePendingReportPath();

private:
    struct MissionState {
        AutoSimulationGoal goal = AutoSimulationGoal::None;
        Vec2 targetWorld{};
        int targetIndex = -1;
        std::string reason;
        float startedAt = 0.0f;
        float bestDistance = 0.0f;
        float progressAnchorDistance = 0.0f;
        int progressGeneration = 0;
        int progressMarker = 0;
        bool adaptive = false;

        bool active() const { return goal != AutoSimulationGoal::None; }
    };

    struct TaskState {
        AutoSimulationGoal goal = AutoSimulationGoal::None;
        Vec2 targetWorld{};
        std::string targetId;
        std::string reason;
        float startedAt = 0.0f;
        float bestDistance = 0.0f;
        float noProgressSeconds = 0.0f;

        bool active() const { return goal != AutoSimulationGoal::None; }
    };

    struct TraversalMemory {
        std::string stageId;
        std::uint32_t seed = 0;
        int furthestMainPathIndex = -1;
        int lastOpportunityProgressIndex = -1;
        Vec2 furthestWorld{};
        std::vector<Vec2> breadcrumbs;

        bool matches(const GameTestSnapshot& snapshot) const
        {
            return stageId == snapshot.stageId && seed == snapshot.dungeon.seed;
        }
    };

    void start(const GameTestSnapshot& snapshot);
    void startCheckpointMeasurement(const GameTestSnapshot& snapshot, std::string stageId);
    void finish(const GameTestSnapshot& snapshot, AutoSimulationResult result);
    void finishCheckpointMeasurement(const GameTestSnapshot& snapshot, AutoSimulationResult result);
    void report() const;
    void queueAction(GameTestAction action);
    void recordIntent(AutoSimulationIntent intent);
    AutoSimulationPlan stabilizePlan(const GameTestSnapshot& snapshot, AutoSimulationPlan candidate, float dt);
    bool canReuseCachedPlan(const GameTestSnapshot& snapshot) const;
    void cachePlanForFastForward(const AutoSimulationPlan& plan);
    void clearCachedPlan();
    void clearPlanLock();
    void resetObjectiveState();
    void beginDungeonExcursion(int knownWarpCountAtDeparture);
    void updateDungeonExcursionState(const GameTestSnapshot& snapshot);
    void updateTraversalMemory(const GameTestSnapshot& snapshot);
    void updateMissionState(const GameTestSnapshot& snapshot, float dt);
    void updateTaskState(const GameTestSnapshot& snapshot, float dt);
    void beginMission(
        AutoSimulationGoal goal,
        Vec2 targetWorld,
        std::string reason,
        int targetIndex = -1);
    void beginAdaptiveMission(
        AutoSimulationGoal goal,
        Vec2 progressTargetWorld,
        std::string reason,
        int progressMarker = 0);
    void completeMission();
    void beginTask(const AutoSimulationPlan& plan, const GameTestSnapshot& snapshot);
    void completeTask(bool succeeded, std::string_view reason);
    AutoSimulationPlan makeHierarchicalPlan(const GameTestSnapshot& snapshot, bool escapeStuck);
    AutoSimulationPlan makeMissionPlan(const GameTestSnapshot& snapshot) const;
    AutoSimulationPlan makeTaskPlan(const GameTestSnapshot& snapshot) const;
    AutoSimulationPlan applyMiningContactCorrection(AutoSimulationPlan plan) const;
    void noteObjectiveTransition(AutoSimulationGoal goal, Vec2 targetWorld);
    bool setSpeedMultiplier(int value);
    void applyNavigationPlan(const GameTestSnapshot& snapshot, const AutoSimulationPlan& plan, float dt);
    void populateCommonDebugSnapshot(
        AutoSimulationDebugSnapshot& debug,
        const GameTestSnapshot& snapshot) const;
    void updateDecisionDebugSnapshot(
        const GameTestSnapshot& snapshot,
        std::string phase,
        std::string detail);
    void updateDebugSnapshot(const GameTestSnapshot& snapshot, const AutoSimulationPlan& plan);
    void updateStuckDetection(const GameTestSnapshot& snapshot, const AutoSimulationPlan& plan, float dt);
    AutoSimulationRunRecord makeRecord(const GameTestSnapshot& snapshot, AutoSimulationResult result) const;
    static bool canStartFromSnapshot(const GameTestSnapshot& snapshot);
    static bool backpackFull(const GameTestSnapshot& snapshot);
    static bool nearReturnPoint(const GameTestSnapshot& snapshot);
    bool shouldReturnForCheckpointPrep(const GameTestSnapshot& snapshot) const;

    AutoSimulationSettings settings_;
    AutoSimulationState state_ = AutoSimulationState::Idle;
    AutoSimulationPlanner planner_;
    AutoSimulationNavigator navigator_;
    AutoSimulationBaseTasks baseTasks_;
    AutoSimulationLevelUpPlanner levelUpPlanner_;
    AutoSimulationGearPlanner gearPlanner_;
    AutoSimulationLoadoutPlanner loadoutPlanner_;
    AutoSimulationRingPlanner ringPlanner_;
    AutoSimulationConsumablePlanner consumablePlanner_;
    AutoSimulationIntentFormatter intentFormatter_;
    AutoSimulationLogger logger_;
    InputAutomationFrame inputFrame_;
    std::optional<GameTestAction> pendingAction_;
    std::optional<std::filesystem::path> pendingReportPath_;
    std::optional<AutoSimulationPlan> lockedPlan_;
    std::optional<AutoSimulationPlan> cachedPlan_;
    std::vector<AutoSimulationIntent> intentHistory_;
    AutoSimulationDebugSnapshot debugSnapshot_;
    std::string lastActionResult_;
    MissionState mission_;
    TaskState task_;
    TraversalMemory traversalMemory_;
    int runIndex_ = 0;
    float runElapsedSeconds_ = 0.0f;
    float actionCooldownSeconds_ = 0.0f;
    float baseIdleSeconds_ = 0.0f;
    float stillSeconds_ = 0.0f;
    float miningNoProgressSeconds_ = 0.0f;
    float planLockSeconds_ = 0.0f;
    float cachedPlanSeconds_ = 0.0f;
    float escapeStuckSeconds_ = 0.0f;
    float missionNoProgressSeconds_ = 0.0f;
    float opportunitySuspendSeconds_ = 0.0f;
    float objectiveSwitchWindowSeconds_ = 0.0f;
    Vec2 lastPlayerPosition_{};
    Vec2 lastMiningTarget_{};
    int lastMiningTargetHp_ = -1;
    int lastMiningDugTiles_ = 0;
    int lastBaseVisitKnownWarpCount_ = 0;
    int backpackReturnRearmMainPathIndex_ = 0;
    bool hasLastPlayerPosition_ = false;
    bool hasLastMiningTarget_ = false;
    bool backpackReturnArmed_ = true;
    bool backpackReturnRearmProgressPending_ = false;
    int stuckCount_ = 0;
    int opportunityBudget_ = 1;
    int objectiveSwitchCount_ = 0;
    int resumeBreadcrumbIndex_ = -1;
    int observedMissionProgressGeneration_ = 0;
    AutoSimulationGoal lastGoal_ = AutoSimulationGoal::None;
    AutoSimulationGoal lastObjectiveGoal_ = AutoSimulationGoal::None;
    Vec2 lastObjectiveTarget_{};
    GameTestScreenMode previousScreenMode_ = GameTestScreenMode::Title;
    bool hasPreviousScreenMode_ = false;
    bool resumeFrontierRequested_ = false;
    bool checkpointMeasurementMode_ = false;
    bool checkpointDungeonLogged_ = false;
    int simulationStepsLastFrame_ = 0;
};

} // namespace majo::autosim
