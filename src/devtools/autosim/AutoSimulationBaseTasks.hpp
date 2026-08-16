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
    static bool backpackCanDepart(const GameTestInventorySnapshot& inventory);

    std::optional<GameTestAction> chooseAction(const GameTestSnapshot& snapshot) const;
    std::optional<GameTestAction> choosePreparationAction(const GameTestSnapshot& snapshot) const;
    std::optional<GameTestAction> chooseCheckpointPrepAction(const GameTestSnapshot& snapshot) const;
    void recordActionResult(const GameTestAction& action, const GameTestActionResult& result);

    int optionalSpendBudgetLimit() const { return optionalSpendBudgetLimit_; }
    int optionalSpendBudgetSpent() const { return optionalSpendBudgetSpent_; }
    int optionalSpendBudgetRemaining() const;

private:
    void observeMoney(int money) const;

    AutoSimulationItemEvaluator itemEvaluator_;
    mutable int optionalSpendBudgetLimit_ = 0;
    int optionalSpendBudgetSpent_ = 0;
};

} // namespace majo::autosim
