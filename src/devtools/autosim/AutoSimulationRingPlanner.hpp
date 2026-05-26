#pragma once

#include "devtools/autosim/AutoSimulationItemEvaluator.hpp"
#include "devtools/autosim/AutoSimulationTypes.hpp"
#include "game/GameTestAction.hpp"
#include "game/GameTestProbe.hpp"

#include <optional>

namespace majo::autosim {

class AutoSimulationRingPlanner {
public:
    void reset();
    std::optional<GameTestAction> chooseAction(
        const GameTestSnapshot& snapshot,
        const AutoSimulationPlan& plan,
        float dt);

private:
    AutoSimulationItemEvaluator itemEvaluator_;
    float switchCooldownSeconds_ = 0.0f;
};

} // namespace majo::autosim
