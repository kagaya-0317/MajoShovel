#pragma once

#include "devtools/autosim/AutoSimulationTypes.hpp"
#include "engine/Input.hpp"
#include "game/GameTestProbe.hpp"

namespace majo::autosim {

class AutoSimulationNavigator {
public:
    void reset();
    InputAutomationFrame makeInput(const GameTestSnapshot& snapshot, const AutoSimulationPlan& plan, float dt);

private:
    float throwCooldownSeconds_ = 0.0f;
    float confirmCooldownSeconds_ = 0.0f;
};

} // namespace majo::autosim
