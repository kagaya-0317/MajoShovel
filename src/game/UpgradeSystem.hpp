#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Ui.hpp"
#include "game/LevelSystem.hpp"
#include "game/RingLevelUpgrade.hpp"
#include "game/SpellRingSystem.hpp"

#include <optional>

namespace majo {

class UpgradeSystem {
public:
    std::optional<RingLevelUpgradeSelection> update(
        const Input& input,
        UiContext& ui,
        SpellRingSystem& spellRing);
    void render(
        Renderer& renderer,
        const LevelSystem& level,
        const SpellRingSystem& spellRing,
        const RingLevelUpgradePointTable& levelRingUpgradePoints);

private:
    int selectedOption_ = 0;
    int selectedRingIndex_ = 0;
    bool ringSelectionInitialized_ = false;
};

}
