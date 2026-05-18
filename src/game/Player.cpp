#include "game/Player.hpp"

#include "game/TileMap.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace majo {

int playerSpriteFrameIndex(float animationTime, bool walking)
{
    constexpr float TargetFps = 60.0f;
    constexpr std::array<int, 3> IdleFrames{0, 1, 2};
    constexpr int WalkFrameStart = 3;
    constexpr int WalkFrameCount = 6;
    constexpr float IdleFrameDuration = 12.0f / TargetFps;
    constexpr float WalkFrameDuration = 6.0f / TargetFps;

    const float frameDuration = walking ? WalkFrameDuration : IdleFrameDuration;
    const int step = static_cast<int>(std::floor(std::max(0.0f, animationTime) / frameDuration));
    if (walking) {
        return WalkFrameStart + step % WalkFrameCount;
    }
    return IdleFrames[static_cast<std::size_t>(step % static_cast<int>(IdleFrames.size()))];
}

std::string_view deathCauseText(DamageSource source)
{
    switch (source) {
    case DamageSource::Poison:
        return "毒で死亡";
    case DamageSource::Bleed:
        return "出血で死亡";
    case DamageSource::SlimeAttack:
        return "スライムの攻撃で死亡";
    case DamageSource::SlimeContact:
        return "スライムの接触で死亡";
    case DamageSource::Projectile:
        return "発射物で死亡";
    case DamageSource::Trap:
        return "罠で死亡";
    case DamageSource::Unknown:
        break;
    }
    return "不明なダメージで死亡";
}

void Player::applyDamage(int amount, DamageSource source)
{
    if (amount <= 0 || hp <= 0) {
        return;
    }

    lastDamageSource = source;
    const int beforeHp = hp;
    hp = std::max(0, hp - amount);
    const int damageTaken = beforeHp - hp;
    if (damageTaken > 0) {
        damageFlash = 0.16f;
        status.removeState("status_sleep");
        damageEvents.push_back({damageTaken, position});
    }
}

int Player::heal(int amount)
{
    if (amount <= 0 || hp <= 0) {
        return 0;
    }

    const int beforeHp = hp;
    hp = std::min(maxHp, hp + amount);
    const int healed = hp - beforeHp;
    if (healed > 0) {
        healEvents.push_back({healed, position});
    }
    return healed;
}

float Player::effectiveRadius(float baseRadius) const
{
    return std::max(0.0f, baseRadius * static_cast<float>(status.sizeMultiplierFromStates()));
}

void Player::update(
    const Input& input,
    const Camera& camera,
    TileMap& map,
    float dt,
    bool paused,
    const RuntimeBalance& balance,
    std::span<const CollisionRect> objectBlockers)
{
    damageFlash = std::max(0.0f, damageFlash - dt);
    if (paused) {
        return;
    }

    stunWakeTimer = std::max(0.0f, stunWakeTimer - dt);
    const bool wasStunned = status.hasState("status_stun");
    status.update(dt);
    if (wasStunned && !status.hasState("status_stun")) {
        stunWakeTimer = 0.18f;
    }
    const double poisonDps = status.poisonDamagePerSecond();
    if (poisonDps > 0.0) {
        poisonDamageAccumulator += poisonDps * static_cast<double>(dt);
        const int poisonDamage = static_cast<int>(std::floor(poisonDamageAccumulator));
        if (poisonDamage > 0) {
            applyDamage(poisonDamage, DamageSource::Poison);
            poisonDamageAccumulator -= static_cast<double>(poisonDamage);
        }
    } else {
        poisonDamageAccumulator = 0.0;
    }

    const float speed = static_cast<float>(
        status.applyModifiers(ModifierStat::Speed, balance.playerSpeed) *
        status.movementMultiplierFromStates());
    const Vec2 moveAxis = input.moveAxis();
    velocity = moveAxis * speed;
    const double bleedDps = status.bleedDamagePerSecond();
    if (bleedDps > 0.0) {
        const double movementScale = lengthSquared(velocity) > 1.0f ? 1.5 : 0.5;
        bleedDamageAccumulator += bleedDps * movementScale * static_cast<double>(dt);
        const int bleedDamage = static_cast<int>(std::floor(bleedDamageAccumulator));
        if (bleedDamage > 0) {
            applyDamage(bleedDamage, DamageSource::Bleed);
            bleedDamageAccumulator -= static_cast<double>(bleedDamage);
        }
    } else {
        bleedDamageAccumulator = 0.0;
    }
    const bool walkingNow = lengthSquared(moveAxis) > 0.0001f;
    if (walkingNow != spriteWalking) {
        spriteWalking = walkingNow;
        spriteAnimationTime = 0.0f;
    } else {
        spriteAnimationTime += dt;
    }
    const Vec2 delta = velocity * dt;
    const float playerRadius = effectiveRadius(balance.playerRadius);
    const auto blocked = [&](Vec2 center) {
        return map.isCircleBlocked(center, playerRadius) ||
            circleIntersectsAnyRect(center, playerRadius, objectBlockers);
    };
    Vec2 next = position + Vec2{delta.x, 0.0f};
    if (!blocked(next)) {
        position = next;
    }
    next = position + Vec2{0.0f, delta.y};
    if (!blocked(next)) {
        position = next;
    }

    const Vec2 aim = camera.screenToWorld(input.mouseScreen()) - position;
    if (lengthSquared(aim) > 16.0f) {
        facing = normalize(aim);
    } else if (lengthSquared(velocity) > 1.0f) {
        facing = normalize(velocity);
    }

    const float targetShift = input.ringOffsetHeld()
        ? balance.spellRingShiftDistance + spellRingShiftDistanceBonus
        : 0.0f;
    spellRingShift = lerp(spellRingShift, targetShift, 1.0f - std::exp(-14.0f * dt));
    throwCooldownRemaining = std::max(0.0f, throwCooldownRemaining - dt);
}

int Player::spriteFrameIndex() const
{
    return playerSpriteFrameIndex(spriteAnimationTime, spriteWalking);
}

}
