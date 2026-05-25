#pragma once

#include "devtools/autosim/AutoSimulationTypes.hpp"
#include "game/GameTestProbe.hpp"

#include <optional>

namespace majo::autosim {

class AutoSimulationCombatModel {
public:
    std::optional<AutoSimulationPlan> makePlan(const GameTestSnapshot& snapshot) const;
};

} // namespace majo::autosim
