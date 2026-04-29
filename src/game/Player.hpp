#pragma once

#include "engine/Input.hpp"
#include "engine/Camera.hpp"
#include "engine/Math.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/EntityStatus.hpp"

namespace majo {

class TileMap;

struct Player {
    Vec2 position{0.0f, 0.0f};
    Vec2 velocity{};
    Vec2 facing{1.0f, 0.0f};
    int hp = 10;
    int level = 1;
    int xp = 0;
    int xpToNext = 12;
    float spellRingShift = 0.0f;
    float throwCooldownRemaining = 0.0f;
    EntityStatus status;

    void update(const Input& input, const Camera& camera, TileMap& map, float dt, bool paused, const RuntimeBalance& balance);
};

}
