#pragma once

#include "engine/Input.hpp"
#include "engine/Math.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/CharacterSprite.hpp"
#include "game/Collision.hpp"
#include "game/EntityStatus.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

class TileMap;

enum class DamageSource {
    Unknown,
    Poison,
    Hot,
    Bleed,
    SlimeAttack,
    SlimeContact,
    Projectile,
    Explosion,
    Trap
};

struct DamageCause {
    DamageSource source = DamageSource::Unknown;
    std::string actorName;
    std::string objectName;
};

std::string deathCauseText(const DamageCause& cause);
std::string_view fallbackDeathCauseText(DamageSource source);
int playerSpriteFrameIndex(float animationTime, CharacterSpriteMotion motion);
int playerSpriteFrameIndex(float animationTime, bool walking);
inline constexpr float WitchSelfLightRadiusMultiplier = 2.0f;
inline constexpr float WitchSelfLightCenterYOffset = -26.0f;

inline Vec2 witchSelfLightCenter(Vec2 footAnchor)
{
    return footAnchor + Vec2{0.0f, WitchSelfLightCenterYOffset};
}

inline float witchSelfLightRadius(float baseRadius)
{
    return baseRadius * WitchSelfLightRadiusMultiplier;
}

struct PlayerDamageEvent {
    int amount = 0;
    Vec2 position{};
};

struct PlayerHealEvent {
    int amount = 0;
    Vec2 position{};
};

struct Player {
    Vec2 position{0.0f, 0.0f};
    Vec2 velocity{};
    Vec2 facing{1.0f, 0.0f};
    Vec2 spellRingShiftDirection{1.0f, 0.0f};
    Vec2 spellRingShiftDragAnchorScreen{};
    int hp = 10;
    int maxHp = 10;
    int minimumHpAfterDamage = 0;
    int level = 1;
    int xp = 0;
    int xpToNext = balance::XpBase + balance::XpPerLevel;
    float spellRingShift = 0.0f;
    float spellRingShiftDistanceBonus = 0.0f;
    float spellRingShiftDistanceMultiplier = 1.0f;
    float spriteAnimationTime = 0.0f;
    bool spriteWalking = false;
    bool spriteFlipHorizontal = true;
    bool spellRingShiftDragActive = false;
    float damageFlash = 0.0f;
    float stunWakeTimer = 0.0f;
    Vec2 knockbackVelocity{};
    float knockbackTimer = 0.0f;
    double poisonDamageAccumulator = 0.0;
    double hotDamageAccumulator = 0.0;
    double bleedDamageAccumulator = 0.0;
    DamageCause lastDamageCause{};
    std::vector<PlayerDamageEvent> damageEvents;
    std::vector<PlayerHealEvent> healEvents;
    EntityStatus status;

    void applyDamage(int amount, const DamageCause& cause);
    void applyDamage(int amount, DamageSource source);
    void applyKnockback(Vec2 direction, float speed, float durationSeconds = 0.16f);
    int heal(int amount);
    [[nodiscard]] float effectiveRadius(float baseRadius) const;
    void update(
        const Input& input,
        TileMap& map,
        float dt,
        bool paused,
        const RuntimeBalance& balance,
        std::span<const CollisionRect> objectBlockers = {});
    void updateSpriteAnimation(float dt, bool walking);
    void updateSpriteFlipFromFacing();
    int spriteFrameIndex(CharacterSpriteMotion motion) const;
    int spriteFrameIndex() const;
};

}
