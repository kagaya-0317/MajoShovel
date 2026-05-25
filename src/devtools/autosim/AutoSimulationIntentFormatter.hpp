#pragma once

#include "devtools/autosim/AutoSimulationTypes.hpp"
#include "game/GameTestProbe.hpp"

namespace majo::autosim {

class AutoSimulationIntentFormatter {
public:
    AutoSimulationIntent format(const GameTestSnapshot& snapshot, const AutoSimulationPlan& plan) const;
};

} // namespace majo::autosim
