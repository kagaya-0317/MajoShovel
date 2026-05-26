#pragma once

#include "game/GameTestAction.hpp"
#include "game/GameTestProbe.hpp"

#include <optional>

namespace majo::autosim {

class AutoSimulationConsumablePlanner {
public:
    std::optional<GameTestAction> chooseAction(const GameTestSnapshot& snapshot) const;
};

} // namespace majo::autosim
