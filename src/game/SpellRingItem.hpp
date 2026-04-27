#pragma once

#include "data/GameBalance.hpp"
#include "engine/Math.hpp"
#include <array>

namespace majo {

enum class SpellRingItemType {
    Shovel,
    Torch,
    Stone,
    Ore
};

struct SpellRingItem {
    SpellRingItemType type = SpellRingItemType::Shovel;
    float localAngle = 0.0f;
    float hitRadius = 10.0f;
    int damage = 1;
    int digPower = 0;
    float weight = 1.0f;
    float hitInterval = 0.22f;
    float lastTerrainHitTime = -100.0f;
    float lastEnemyHitTime = -100.0f;
    int lastDigTileX = 2147483647;
    int lastDigTileY = 2147483647;
    Vec2 worldPosition{};
    std::array<int, balance::MaxEnemies> latchedEnemyIds{};

    bool isEnemyLatched(int enemyId) const;
    void latchEnemy(int enemyId);
    void unlatchEnemy(int enemyId);
};

SpellRingItem makeShovel();
SpellRingItem makeTorch();
SpellRingItem makeStone();
SpellRingItem makeOre();

}
