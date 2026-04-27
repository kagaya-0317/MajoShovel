#pragma once

#include "engine/Math.hpp"

namespace majo {

enum class OrbitItemType {
    Shovel,
    Torch
};

struct OrbitItem {
    OrbitItemType type = OrbitItemType::Shovel;
    float localAngle = 0.0f;
    float hitRadius = 10.0f;
    int damage = 1;
    int digPower = 0;
    float weight = 1.0f;
    float hitInterval = 0.22f;
    float lastTerrainHitTime = -100.0f;
    float lastEnemyHitTime = -100.0f;
    Vec2 worldPosition{};
};

OrbitItem makeShovel();
OrbitItem makeTorch();

}
