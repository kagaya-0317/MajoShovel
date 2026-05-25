#pragma once

#include "devtools/autosim/AutoSimulationBaseTasks.hpp"
#include "devtools/autosim/AutoSimulationIntentFormatter.hpp"
#include "devtools/autosim/AutoSimulationLoadoutPlanner.hpp"
#include "devtools/autosim/AutoSimulationLogger.hpp"
#include "devtools/autosim/AutoSimulationNavigator.hpp"
#include "devtools/autosim/AutoSimulationPlanner.hpp"
#include "devtools/autosim/AutoSimulationTypes.hpp"
#include "engine/Input.hpp"
#include "game/GameTestAction.hpp"
#include "game/GameTestProbe.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace majo::autosim {

class AutoSimulationController {
public:
    bool executeCommand(std::string_view normalizedCommand, const GameTestSnapshot& snapshot);
    void update(const GameTestSnapshot& snapshot, float dt);

    const InputAutomationFrame& inputFrame() const { return inputFrame_; }
    std::optional<GameTestAction> consumeAction();
    void recordActionResult(const GameTestAction& action, const GameTestActionResult& result);
    bool active() const { return state_ == AutoSimulationState::Running; }
    AutoSimulationState state() const { return state_; }
    const std::vector<AutoSimulationIntent>& intentHistory() const { return intentHistory_; }
    const AutoSimulationDebugSnapshot& debugSnapshot() const { return debugSnapshot_; }

private:
    void start(const GameTestSnapshot& snapshot);
    void finish(const GameTestSnapshot& snapshot, AutoSimulationResult result);
    void report() const;
    void queueAction(GameTestAction action);
    void recordIntent(AutoSimulationIntent intent);
    AutoSimulationPlan stabilizePlan(const GameTestSnapshot& snapshot, AutoSimulationPlan candidate, float dt);
    void clearPlanLock();
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
    AutoSimulationLoadoutPlanner loadoutPlanner_;
    AutoSimulationIntentFormatter intentFormatter_;
    AutoSimulationLogger logger_;
    InputAutomationFrame inputFrame_;
    std::optional<GameTestAction> pendingAction_;
    std::optional<AutoSimulationPlan> lockedPlan_;
    std::vector<AutoSimulationIntent> intentHistory_;
    AutoSimulationDebugSnapshot debugSnapshot_;
    int runIndex_ = 0;
    float runElapsedSeconds_ = 0.0f;
    float actionCooldownSeconds_ = 0.0f;
    float baseIdleSeconds_ = 0.0f;
    float stillSeconds_ = 0.0f;
    float miningNoProgressSeconds_ = 0.0f;
    float planLockSeconds_ = 0.0f;
    float escapeStuckSeconds_ = 0.0f;
    Vec2 lastPlayerPosition_{};
    Vec2 lastMiningTarget_{};
    int lastMiningTargetHp_ = -1;
    int lastMiningDugTiles_ = 0;
    bool hasLastPlayerPosition_ = false;
    bool hasLastMiningTarget_ = false;
    int stuckCount_ = 0;
    AutoSimulationGoal lastGoal_ = AutoSimulationGoal::None;
};

} // namespace majo::autosim
