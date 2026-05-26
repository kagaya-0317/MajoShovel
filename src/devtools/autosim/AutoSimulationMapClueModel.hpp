#pragma once

#include "devtools/autosim/AutoSimulationPathfinder.hpp"

#include <optional>
#include <string>

namespace majo::autosim {

struct AutoSimulationMapClueTarget {
    Vec2 world{};
    std::string reason;
};

class AutoSimulationMapClueModel {
public:
    std::optional<AutoSimulationMapClueTarget> chooseTarget(
        const GameTestSnapshot& snapshot,
        const AutoSimulationPathfinder& pathfinder,
        const AutoSimulationPathField& pathField) const;
};

} // namespace majo::autosim
