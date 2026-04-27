#pragma once

#include "engine/Renderer.hpp"
#include "engine/Time.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/EnemySystem.hpp"
#include "game/OrbitSystem.hpp"
#include "game/Player.hpp"
#include "game/TileMap.hpp"

namespace majo {

class DebugOverlay {
public:
    void toggle() { visible_ = !visible_; }
    bool visible() const { return visible_; }
    void render(Renderer& renderer, const Time& time, const EnemySystem& enemies, const TileMap& map, const OrbitSystem& orbit, const Player& player, const RuntimeBalance& balance);

private:
    bool visible_ = true;
};

}
