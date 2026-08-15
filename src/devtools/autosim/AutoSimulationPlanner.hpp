#pragma once

#include "devtools/autosim/AutoSimulationCombatModel.hpp"
#include "devtools/autosim/AutoSimulationExplorationModel.hpp"
#include "devtools/autosim/AutoSimulationMapClueModel.hpp"
#include "devtools/autosim/AutoSimulationMiningModel.hpp"
#include "devtools/autosim/AutoSimulationPathfinder.hpp"
#include "devtools/autosim/AutoSimulationTypes.hpp"
#include "game/GameTestProbe.hpp"

#include <optional>

namespace majo::autosim {

enum class AutoSimulationPlanScope {
    All,
    CombatOnly,
    OpportunityOnly,
    ProgressOnly,
};

class AutoSimulationPlanner {
public:
    AutoSimulationPlan makePlan(
        const GameTestSnapshot& snapshot,
        bool escapeStuck,
        AutoSimulationPlanScope scope = AutoSimulationPlanScope::All) const;
    AutoSimulationPlan makeDirectedPlan(
        const GameTestSnapshot& snapshot,
        AutoSimulationGoal goal,
        Vec2 target,
        std::string reason,
        AutoSimulationDigPolicy digPolicy,
        AutoSimulationRingRole preferredRingRole = AutoSimulationRingRole::Utility) const;

private:
    static bool needsConfirmInput(const GameTestSnapshot& snapshot);
    static AutoSimulationPlan makeTargetPlan(
        AutoSimulationGoal goal,
        Vec2 target,
        std::string reason,
        bool throwRing = false,
        bool ringOffset = false,
        bool moveAwayFromTarget = false,
        AutoSimulationRingRole preferredRingRole = AutoSimulationRingRole::None);
    AutoSimulationPlan makeTravelPlan(
        const GameTestSnapshot& snapshot,
        const AutoSimulationPathField& pathField,
        AutoSimulationGoal goal,
        Vec2 target,
        std::string reason,
        bool throwRing = false,
        bool ringOffset = false,
        AutoSimulationRingRole preferredRingRole = AutoSimulationRingRole::Utility,
        std::optional<AutoSimulationDigPolicy> digPolicyOverride = std::nullopt) const;

    AutoSimulationCombatModel combatModel_;
    AutoSimulationExplorationModel explorationModel_;
    AutoSimulationMapClueModel mapClueModel_;
    AutoSimulationMiningModel miningModel_;
    AutoSimulationPathfinder pathfinder_;
};

} // namespace majo::autosim
