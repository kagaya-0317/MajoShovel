#pragma once

#include "engine/Input.hpp"
#include "engine/Camera.hpp"
#include "engine/Math.hpp"
#include "data/RuntimeBalance.hpp"
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
    SlimeAttack,
    SlimeContact,
    Projectile,
    Trap
};

std::string_view deathCauseText(DamageSource source);
int playerSpriteFrameIndex(float animationTime, bool walking);

struct PlayerDamageEvent {
    int amount = 0;
    Vec2 position{};
};

struct Player {
    Vec2 position{0.0f, 0.0f};
    Vec2 velocity{};
    Vec2 facing{1.0f, 0.0f};
    int hp = 10;
    int maxHp = 10;
    int level = 1;
    int xp = 0;
    int xpToNext = 12;
    float spellRingShift = 0.0f;
    float spellRingShiftDistanceBonus = 0.0f;
    float throwCooldownRemaining = 0.0f;
    float spriteAnimationTime = 0.0f;
    bool spriteWalking = false;
    float damageFlash = 0.0f;
    double poisonDamageAccumulator = 0.0;
    DamageSource lastDamageSource = DamageSource::Unknown;
    std::string lastDamageEnemyName;
    std::vector<PlayerDamageEvent> damageEvents;
    EntityStatus status;

    void applyDamage(int amount, DamageSource source);
    void update(
        const Input& input,
        const Camera& camera,
        TileMap& map,
        float dt,
        bool paused,
        const RuntimeBalance& balance,
        std::span<const CollisionRect> objectBlockers = {});
    int spriteFrameIndex() const;
};

}
