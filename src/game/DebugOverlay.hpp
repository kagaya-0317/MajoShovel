#pragma once

#include "engine/Renderer.hpp"
#include "engine/Time.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/EnemySystem.hpp"
#include "game/DungeonLayout.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/Player.hpp"
#include "game/TileMap.hpp"

namespace majo {

class DebugOverlay {
public:
    void toggle() { visible_ = !visible_; }
    bool visible() const { return visible_; }
    void render(
        Renderer& renderer,
        const Time& time,
        const EnemySystem& enemies,
        const TileMap& map,
        const SpellRingSystem& spellRing,
        const Player& player,
        const RuntimeBalance& balance,
        const DungeonLayout& dungeonLayout);

private:
    bool visible_ = true;
};

}
