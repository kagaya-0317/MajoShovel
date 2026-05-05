#pragma once

#include "data/GameBalance.hpp"
#include "engine/Math.hpp"
#include "game/ItemModel.hpp"
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

enum class SpellRingItemType {
    Shovel,
    Torch,
    Stone,
    Ore,
    Object
};

struct SpellRingItem {
    SpellRingItemType type = SpellRingItemType::Shovel;
    float localAngle = 0.0f;
    float hitRadius = 10.0f;
    int damage = 1;
    std::string damageType = "physical";
    int digPower = 0;
    int durability = -1;
    int maxDurability = -1;
    float weight = 1.0f;
    float hitInterval = 0.22f;
    float lastTerrainHitTime = -100.0f;
    float lastEnemyHitTime = -100.0f;
    int lastDigTileX = 2147483647;
    int lastDigTileY = 2147483647;
    std::string objectId;
    std::string instanceId;
    bool objectStatsApplied = false;
    int enhanceLevel = 0;
    int attackBonus = 0;
    int digBonus = 0;
    int durabilityBonus = 0;
    double weightModifier = 1.0;
    double sizeModifier = 1.0;
    bool protectionEnabled = false;
    bool isBroken = false;
    std::vector<EffectSpec> addedEffects;
    std::vector<std::string> addedTags;
    float lightRadius = 0.0f;
    float hiddenDetectionRadius = 0.0f;
    float treasureDetectionRadius = 0.0f;
    Vec2 worldPosition{};
    std::array<int, balance::MaxEnemies> latchedEnemyIds{};
    std::string capturedBehaviorId;
    std::vector<std::string> capturedBehaviorIds;
    float capturedBehaviorTimer = 0.0f;
    float capturedJumpTimer = 0.0f;
    float capturedProjectileTimer = 0.0f;
    int capturedExplodeCharge = 0;
    float capturedExplodeSleepTimer = 0.0f;
    float capturedMagnetVisualTimer = 0.0f;
    float capturedWindTimer = 0.0f;
    float capturedRewardLastTime = -100.0f;
    float capturedRewardWindowStart = -100.0f;
    int capturedRewardWindowCount = 0;
    int capturedBossRewardCount = 0;

    bool hasCapturedBehavior(std::string_view behaviorId) const;
    bool isEnemyLatched(int enemyId) const;
    void latchEnemy(int enemyId);
    void unlatchEnemy(int enemyId);
    bool consumeDurability(int amount = 1);
    bool broken() const;
};

SpellRingItem makeShovel();
SpellRingItem makeTorch();
SpellRingItem makeStone();
SpellRingItem makeOre();
SpellRingItem makeObjectRingItem(std::string_view objectId);

}
