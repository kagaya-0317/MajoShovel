#pragma once

#include "devtools/autosim/AutoSimulationItemEvaluator.hpp"
#include "game/GameTestAction.hpp"
#include "game/GameTestProbe.hpp"

#include <optional>

namespace majo::autosim {

class AutoSimulationBaseTasks {
public:
    std::optional<GameTestAction> chooseAction(const GameTestSnapshot& snapshot) const;

private:
    AutoSimulationItemEvaluator itemEvaluator_;
};

} // namespace majo::autosim
