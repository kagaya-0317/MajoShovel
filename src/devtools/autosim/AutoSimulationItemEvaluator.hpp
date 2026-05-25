#pragma once

#include "game/GameTestProbe.hpp"

#include <string>

namespace majo::autosim {

struct AutoSimulationItemScore {
    float keep = 0.0f;
    float sell = 0.0f;
    float store = 0.0f;
    float protect = 0.0f;
    float enhance = 0.0f;
    bool preferAttackEnhance = false;
    bool preferDigEnhance = false;
    std::string reason;
};

class AutoSimulationItemEvaluator {
public:
    AutoSimulationItemScore evaluate(const GameTestObjectEntrySnapshot& item) const;
};

} // namespace majo::autosim
