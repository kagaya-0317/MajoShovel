#pragma once

#include "devtools/autosim/AutoSimulationPathfinder.hpp"

#include <optional>
#include <string>

namespace majo::autosim {

struct AutoSimulationExplorationTarget {
    Vec2 world{};
    std::string reason;
    float utilityAdjustment = 0.0f;
};

class AutoSimulationExplorationModel {
public:
    std::optional<AutoSimulationExplorationTarget> chooseTarget(
        const GameTestSnapshot& snapshot,
        const AutoSimulationPathField& pathField) const;
};

} // namespace majo::autosim
