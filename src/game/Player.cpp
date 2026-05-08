#include "game/Player.hpp"

#include "game/TileMap.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace majo {

int playerSpriteFrameIndex(float animationTime, bool walking)
{
    constexpr float TargetFps = 60.0f;
    constexpr std::array<int, 4> IdleFrames{0, 1, 2, 1};
    constexpr int WalkFrameStart = 3;
    constexpr int WalkFrameCount = 6;
    constexpr float IdleFrameDuration = 12.0f / TargetFps;
    constexpr float WalkFrameDuration = 8.0f / TargetFps;

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
    hp = std::max(0, hp - amount);
}

void Player::update(const Input& input, const Camera& camera, TileMap& map, float dt, bool paused, const RuntimeBalance& balance)
{
    if (paused) {
        return;
    }

    status.update(dt);
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
    const bool walkingNow = lengthSquared(moveAxis) > 0.0001f;
    if (walkingNow != spriteWalking) {
        spriteWalking = walkingNow;
        spriteAnimationTime = 0.0f;
    } else {
        spriteAnimationTime += dt;
    }
    const Vec2 delta = velocity * dt;
    Vec2 next = position + Vec2{delta.x, 0.0f};
    if (!map.isCircleBlocked(next, balance.playerRadius)) {
        position = next;
    }
    next = position + Vec2{0.0f, delta.y};
    if (!map.isCircleBlocked(next, balance.playerRadius)) {
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
