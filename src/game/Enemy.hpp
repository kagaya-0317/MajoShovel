#pragma once

#include "engine/Math.hpp"

namespace majo {

struct Enemy {
    bool active = false;
    int id = 0;
    Vec2 position{};
    Vec2 velocity{};
    float radius = 10.0f;
    int hp = 5;
    int xp = 5;
    float contactTimer = 0.0f;
    float hitFlash = 0.0f;
    float repathTimer = 0.0f;
    float spawnTimer = 0.0f;
    float spawnDuration = 0.0f;
};

}
