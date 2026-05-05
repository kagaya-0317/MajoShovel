#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Ui.hpp"
#include "game/LevelSystem.hpp"
#include "game/SpellRingSystem.hpp"

namespace majo {

class UpgradeSystem {
public:
    void update(
        const Input& input,
        UiContext& ui,
        LevelSystem& level,
        SpellRingSystem& spellRing,
        int& levelRingRadiusPoints,
        int& levelRingSpeedPoints);
    void render(Renderer& renderer, const LevelSystem& level);

private:
    int selectedOption_ = 0;
};

}
