#pragma once

#include "devtools/autosim/AutoSimulationTypes.hpp"
#include "game/GameTestProbe.hpp"

#include <optional>
#include <string>

namespace majo::autosim {

class AutoSimulationMiningModel {
public:
    std::optional<AutoSimulationPlan> makePlan(
        const GameTestSnapshot& snapshot,
        Vec2 travelTarget,
        std::string reason) const;
    std::optional<AutoSimulationPlan> makePlanForTile(
        const GameTestSnapshot& snapshot,
        const GameTestMineTileSnapshot& tile,
        Vec2 travelTarget,
        std::string reason) const;
};

} // namespace majo::autosim
