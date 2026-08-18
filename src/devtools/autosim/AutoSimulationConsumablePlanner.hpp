#pragma once

#include "game/GameTestAction.hpp"
#include "game/GameTestProbe.hpp"

#include <optional>

namespace majo::autosim {

struct AutoSimulationConsumableProfile {
    double heal = 0.0;
    double attackMultiplier = 1.0;
    double speedMultiplier = 1.0;
    double defenseMultiplier = 1.0;
    double giantValue = 0.0;
    bool unsafeSelfEffect = false;
};

AutoSimulationConsumableProfile autoSimulationConsumableProfile(
    const GameTestObjectEntrySnapshot& item);

[[nodiscard]] bool autoSimulationNeedsLowHpReturn(const GameTestSnapshot& snapshot);

class AutoSimulationConsumablePlanner {
public:
    std::optional<GameTestAction> chooseAction(const GameTestSnapshot& snapshot) const;
};

} // namespace majo::autosim
