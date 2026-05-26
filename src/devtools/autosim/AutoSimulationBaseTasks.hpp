#pragma once

#include "devtools/autosim/AutoSimulationItemEvaluator.hpp"
#include "game/GameTestAction.hpp"
#include "game/GameTestProbe.hpp"

#include <optional>

namespace majo::autosim {

class AutoSimulationBaseTasks {
public:
    static int desiredBackpackFreeSlots(const GameTestInventorySnapshot& inventory);
    static bool backpackReadyForDeparture(const GameTestInventorySnapshot& inventory);

    std::optional<GameTestAction> chooseAction(const GameTestSnapshot& snapshot) const;
    std::optional<GameTestAction> choosePreparationAction(const GameTestSnapshot& snapshot) const;
    std::optional<GameTestAction> chooseCheckpointPrepAction(const GameTestSnapshot& snapshot) const;

private:
    AutoSimulationItemEvaluator itemEvaluator_;
};

} // namespace majo::autosim
