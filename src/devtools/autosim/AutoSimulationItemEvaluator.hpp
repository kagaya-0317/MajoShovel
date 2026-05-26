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
    float combat = 0.0f;
    float dig = 0.0f;
    float light = 0.0f;
    float utility = 0.0f;
    float loadout = 0.0f;
    float investment = 0.0f;
    bool preferAttackEnhance = false;
    bool preferDigEnhance = false;
    std::string reason;
};

struct AutoSimulationItemEvaluationContext {
    float combatWeight = 1.0f;
    float digWeight = 1.0f;
    float lightWeight = 1.0f;
    float utilityWeight = 1.0f;
    bool backpackPressure = false;
};

AutoSimulationItemEvaluationContext autoSimulationItemEvaluationContextForSnapshot(
    const GameTestSnapshot& snapshot);

class AutoSimulationItemEvaluator {
public:
    AutoSimulationItemScore evaluate(const GameTestObjectEntrySnapshot& item) const;
    AutoSimulationItemScore evaluate(
        const GameTestObjectEntrySnapshot& item,
        const AutoSimulationItemEvaluationContext& context) const;
    AutoSimulationItemScore evaluate(const GameTestRingItemSnapshot& item) const;
    AutoSimulationItemScore evaluate(
        const GameTestRingItemSnapshot& item,
        const AutoSimulationItemEvaluationContext& context) const;
};

} // namespace majo::autosim
