#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Ui.hpp"
#include "game/LevelSystem.hpp"
#include "game/SpellRingSystem.hpp"

#include <optional>

namespace majo {

class UpgradeSystem {
public:
    std::optional<UpgradeChoice> update(
        const Input& input,
        UiContext& ui,
        LevelSystem& level,
        SpellRingSystem& spellRing,
        int& levelRingRadiusPoints,
        int& levelRingSpeedPoints,
        int& levelRingWeightLimitPoints);
    void render(
        Renderer& renderer,
        const LevelSystem& level,
        const SpellRingSystem& spellRing,
        int levelRingRadiusPoints,
        int levelRingSpeedPoints,
        int levelRingWeightLimitPoints);

private:
    int selectedOption_ = 0;
};

}
