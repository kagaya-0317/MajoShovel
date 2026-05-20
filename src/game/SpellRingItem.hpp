#pragma once

#include "data/GameBalance.hpp"
#include "engine/Math.hpp"
#include "game/ItemModel.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

enum class SpellRingItemType {
    Shovel = 0,
    Torch = 1,
    Object = 4
};

constexpr float SpellRingItemActionFlashSeconds = 0.10f;

struct SpellRingItem {
    SpellRingItemType type = SpellRingItemType::Shovel;
    int ringIndex = 0;
    float localAngle = 0.0f;
    float hitRadius = 10.0f;
    int damage = 1;
    std::string damageType = "blunt";
    int digPower = 0;
    int durability = -1;
    int maxDurability = -1;
    float weight = 1.0f;
    float hitInterval = 0.22f;
    float lastTerrainHitTime = -100.0f;
    float lastEnemyHitTime = -100.0f;
    float actionFlashTimer = 0.0f;
    int lastDigTileX = 2147483647;
    int lastDigTileY = 2147483647;
    std::string objectId;
    std::string instanceId;
    ItemVisualRef objectVisual;
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
    float coldAirRadius = 0.0f;
    float coldAirStrength = 0.0f;
    float coldAirFxTimer = 0.0f;
    float vacuumPullRadius = 0.0f;
    float vacuumPullStrength = 0.0f;
    float vacuumPullFxTimer = 0.0f;
    float hotAirRadius = 0.0f;
    float hotAirStrength = 0.0f;
    float hotAirFxTimer = 0.0f;
    float windPushRadius = 0.0f;
    float windPushStrength = 0.0f;
    float windPushFxTimer = 0.0f;
    int dryWetBonusDamage = 0;
    double slashDamageMultiplier = 1.0;
    float orbitDistanceOffset = 0.0f;
    Vec2 worldPosition{};
    Vec2 worldVelocity{};
    float orbitMotionSpeed = 0.0f;
    std::array<int, balance::MaxEnemies> latchedEnemyIds{};
    std::string capturedBehaviorId;
    std::vector<std::string> capturedBehaviorIds;
    std::vector<CapturedBehaviorSpec> capturedBehaviorSpecs;
    float capturedBehaviorTimer = 0.0f;
    float capturedJumpTimer = 0.0f;
    float capturedProjectileTimer = 0.0f;
    int capturedProjectileBurstRemaining = 0;
    float capturedProjectileBurstInterval = 0.12f;
    int capturedExplodeCharge = 0;
    float capturedExplodeSleepTimer = 0.0f;
    float capturedMagnetVisualTimer = 0.0f;
    float capturedWindTimer = 0.0f;
    float capturedRewardLastTime = -100.0f;
    float capturedRewardWindowStart = -100.0f;
    int capturedRewardWindowCount = 0;
    int capturedBossRewardCount = 0;
    std::string magicAuraDamageType;
    float magicAuraTimer = 0.0f;
    float magicCastCooldownTimer = 0.0f;
    std::uint32_t magicAuraFxEmitterId = 0;

    bool hasCapturedBehavior(std::string_view behaviorId) const;
    const CapturedBehaviorSpec* capturedBehaviorSpec(std::string_view behaviorId) const;
    double capturedBehaviorInterval(std::string_view behaviorId, double fallbackSeconds = 0.0) const;
    double capturedBehaviorParamDouble(std::string_view behaviorId, std::string_view key, double fallbackValue) const;
    int capturedBehaviorParamInt(std::string_view behaviorId, std::string_view key, int fallbackValue) const;
    std::string capturedBehaviorParamString(std::string_view behaviorId, std::string_view key, std::string_view fallbackValue = {}) const;
    bool isEnemyLatched(int enemyId) const;
    void latchEnemy(int enemyId);
    void unlatchEnemy(int enemyId);
    bool consumeDurability(int amount = 1);
    bool broken() const;
};

SpellRingItem makeShovel();
SpellRingItem makeTorch();
SpellRingItem makeObjectRingItem(std::string_view objectId);

}
