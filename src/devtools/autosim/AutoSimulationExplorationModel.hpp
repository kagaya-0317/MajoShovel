#pragma once

#include "devtools/autosim/AutoSimulationPathfinder.hpp"

#include <optional>
#include <string>

namespace majo::autosim {

struct AutoSimulationExplorationTarget {
    Vec2 world{};
    std::string reason;
};

class AutoSimulationExplorationModel {
public:
    std::optional<AutoSimulationExplorationTarget> chooseTarget(
        const GameTestSnapshot& snapshot,
        const AutoSimulationPathField& pathField) const;
};

} // namespace majo::autosim
