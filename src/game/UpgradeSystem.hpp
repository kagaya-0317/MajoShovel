#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "game/LevelSystem.hpp"
#include "game/SpellRingSystem.hpp"

namespace majo {

class UpgradeSystem {
public:
    void update(const Input& input, LevelSystem& level, SpellRingSystem& spellRing);
    void render(Renderer& renderer, const LevelSystem& level);
};

}
