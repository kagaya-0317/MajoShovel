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

namespace {

constexpr float RingShiftAimDeadzone = 0.15f;
constexpr float RingShiftPointerDeadzonePx = 2.0f;
constexpr float RingShiftDirectionResponse = 18.0f;
constexpr float RingShiftDistanceResponse = 14.0f;

std::string joinDeathCause(std::string_view actorName, std::string_view objectName, std::string_view fallbackObjectName)
{
    const std::string_view resolvedObjectName = objectName.empty() ? fallbackObjectName : objectName;
    if (!actorName.empty() && !resolvedObjectName.empty()) {
        return std::string(actorName) + "の" + std::string(resolvedObjectName) + "で死亡";
    }
    if (!resolvedObjectName.empty()) {
        return std::string(resolvedObjectName) + "で死亡";
    }
    return {};
}

Vec2 normalizedOr(Vec2 value, Vec2 fallback)
{
    if (lengthSquared(value) <= 0.0001f) {
        return lengthSquared(fallback) > 0.0001f ? normalize(fallback) : Vec2{1.0f, 0.0f};
    }
    return normalize(value);
}

Vec2 smoothDirection(Vec2 current, Vec2 target, float response, float dt)
{
    const Vec2 fallback = normalizedOr(target, {1.0f, 0.0f});
    const Vec2 from = normalizedOr(current, fallback);
    if (dt <= 0.0f) {
        return from;
    }
    const float alpha = 1.0f - std::exp(-std::max(0.0f, response) * dt);
    return normalizedOr(lerp(from, fallback, alpha), fallback);
}

}

std::string_view fallbackDeathCauseText(DamageSource source)
{
    switch (source) {
    case DamageSource::Poison:
        return "毒の継続ダメージで死亡";
    case DamageSource::Hot:
        return "熱の継続ダメージで死亡";
    case DamageSource::Bleed:
        return "出血の継続ダメージで死亡";
    case DamageSource::SlimeAttack:
        return "スライムの攻撃で死亡";
    case DamageSource::SlimeContact:
        return "スライムの接触で死亡";
    case DamageSource::Projectile:
        return "発射物で死亡";
    case DamageSource::Explosion:
        return "爆発で死亡";
    case DamageSource::Trap:
        return "罠で死亡";
    case DamageSource::Unknown:
        break;
    }
    return "不明なダメージで死亡";
}

std::string deathCauseText(const DamageCause& cause)
{
    switch (cause.source) {
    case DamageSource::SlimeAttack:
        if (!cause.actorName.empty()) {
            return cause.actorName + "の攻撃で死亡";
        }
        break;
    case DamageSource::SlimeContact:
        if (!cause.actorName.empty()) {
            return cause.actorName + "の接触で死亡";
        }
        break;
    case DamageSource::Projectile:
        if (std::string text = joinDeathCause(cause.actorName, cause.objectName, "発射物"); !text.empty()) {
            return text;
        }
        break;
    case DamageSource::Explosion:
        if (std::string text = joinDeathCause(cause.actorName, cause.objectName, "爆発"); !text.empty()) {
            return text;
        }
        break;
    case DamageSource::Trap:
        if (std::string text = joinDeathCause(cause.actorName, cause.objectName, "罠"); !text.empty()) {
            return text;
        }
        break;
    case DamageSource::Poison:
        if (!cause.actorName.empty() || !cause.objectName.empty()) {
            return joinDeathCause(cause.actorName, cause.objectName, "毒");
        }
        break;
    case DamageSource::Hot:
        if (!cause.actorName.empty() || !cause.objectName.empty()) {
            return joinDeathCause(cause.actorName, cause.objectName, "熱");
        }
        break;
    case DamageSource::Bleed:
        if (!cause.actorName.empty() || !cause.objectName.empty()) {
            return joinDeathCause(cause.actorName, cause.objectName, "出血");
        }
        break;
    case DamageSource::Unknown:
        break;
    }
    return std::string(fallbackDeathCauseText(cause.source));
}

void Player::applyDamage(int amount, const DamageCause& cause)
{
    if (amount <= 0 || hp <= 0) {
        return;
    }

    lastDamageCause = cause;
    const int beforeHp = hp;
    const int damageFloor = std::clamp(minimumHpAfterDamage, 0, hp);
    hp = std::max(damageFloor, hp - amount);
    const int damageTaken = beforeHp - hp;
    if (damageTaken > 0) {
        damageFlash = 0.16f;
        status.removeState("status_sleep");
        damageEvents.push_back({damageTaken, position});
    }
}

void Player::applyDamage(int amount, DamageSource source)
{
    applyDamage(amount, DamageCause{.source = source});
}

void Player::applyKnockback(Vec2 direction, float speed, float durationSeconds)
{
    if (speed <= 0.0f || durationSeconds <= 0.0f) {
        return;
    }

    const Vec2 fallback = lengthSquared(facing) > 0.0001f ? facing : Vec2{1.0f, 0.0f};
    const Vec2 impulse = normalize(lengthSquared(direction) > 0.0001f ? direction : fallback) * speed;
    knockbackVelocity += impulse;
    const float maxSpeed = std::max(speed, 360.0f);
    if (lengthSquared(knockbackVelocity) > maxSpeed * maxSpeed) {
        knockbackVelocity = normalize(knockbackVelocity) * maxSpeed;
    }
    knockbackTimer = std::max(knockbackTimer, durationSeconds);
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
            applyDamage(
                poisonDamage,
                DamageCause{
                    .source = DamageSource::Poison,
                    .objectName = "毒の継続ダメージ",
                });
            poisonDamageAccumulator -= static_cast<double>(poisonDamage);
        }
    } else {
        poisonDamageAccumulator = 0.0;
    }
    const double hotDps = status.hotDamagePerSecond();
    if (hotDps > 0.0) {
        hotDamageAccumulator += hotDps * static_cast<double>(dt);
        const int hotDamage = static_cast<int>(std::floor(hotDamageAccumulator));
        if (hotDamage > 0) {
            applyDamage(
                hotDamage,
                DamageCause{
                    .source = DamageSource::Hot,
                    .objectName = "熱の継続ダメージ",
                });
            hotDamageAccumulator -= static_cast<double>(hotDamage);
        }
    } else {
        hotDamageAccumulator = 0.0;
    }

    const float speed = static_cast<float>(
        status.applyModifiers(ModifierStat::Speed, balance.playerSpeed) *
        status.movementMultiplierFromStates());
    const Vec2 moveAxis = input.moveAxis();
    Vec2 knockbackMove{};
    if (knockbackTimer > 0.0f) {
        knockbackMove = knockbackVelocity;
        knockbackTimer = std::max(0.0f, knockbackTimer - dt);
        knockbackVelocity = knockbackVelocity * std::max(0.0f, 1.0f - 7.5f * dt);
        if (knockbackTimer <= 0.0f || lengthSquared(knockbackVelocity) < 1.0f) {
            knockbackTimer = 0.0f;
            knockbackVelocity = {};
        }
    }
    velocity = moveAxis * speed + knockbackMove;
    const double bleedDps = status.bleedDamagePerSecond();
    if (bleedDps > 0.0) {
        const double movementScale = lengthSquared(velocity) > 1.0f ? 1.5 : 0.5;
        bleedDamageAccumulator += bleedDps * movementScale * static_cast<double>(dt);
        const int bleedDamage = static_cast<int>(std::floor(bleedDamageAccumulator));
        if (bleedDamage > 0) {
            applyDamage(
                bleedDamage,
                DamageCause{
                    .source = DamageSource::Bleed,
                    .objectName = "出血の継続ダメージ",
                });
            bleedDamageAccumulator -= static_cast<double>(bleedDamage);
        }
    } else {
        bleedDamageAccumulator = 0.0;
    }
    updateSpriteAnimation(dt, lengthSquared(moveAxis) > 0.0001f || lengthSquared(knockbackMove) > 1.0f);
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

    if (input.hasAimAxis()) {
        facing = normalize(input.aimAxis());
    } else if (input.lastActiveDevice() == InputDeviceKind::KeyboardMouse) {
        const Vec2 aim = camera.screenToWorld(input.mouseScreen()) - position;
        if (lengthSquared(aim) > 16.0f) {
            facing = normalize(aim);
        } else if (lengthSquared(velocity) > 1.0f) {
            facing = normalize(velocity);
        }
    } else if (lengthSquared(velocity) > 1.0f) {
        facing = normalize(velocity);
    }

    const float shiftDistance =
        (balance.spellRingShiftDistance + spellRingShiftDistanceBonus) *
        clamp(spellRingShiftDistanceMultiplier, 0.25f, 3.0f);

    Vec2 targetShiftDirection = normalizedOr(spellRingShiftDirection, facing);
    float targetShift = 0.0f;
    if (input.ringOffsetPointerHeld()) {
        const Vec2 mouse = input.mouseScreen();
        if (!spellRingShiftDragActive) {
            spellRingShiftDragActive = true;
            spellRingShiftDragAnchorScreen = mouse;
        }

        Vec2 pointerOffset = mouse - spellRingShiftDragAnchorScreen;
        const float pointerDistance = length(pointerOffset);
        if (pointerDistance > shiftDistance && pointerDistance > 0.0001f) {
            pointerOffset = pointerOffset * (shiftDistance / pointerDistance);
            spellRingShiftDragAnchorScreen = mouse - pointerOffset;
        }

        targetShift = length(pointerOffset);
        if (targetShift >= RingShiftPointerDeadzonePx) {
            targetShiftDirection = normalize(pointerOffset);
        } else {
            targetShift = 0.0f;
            targetShiftDirection = normalizedOr(facing, targetShiftDirection);
        }
    } else {
        spellRingShiftDragActive = false;
        if (input.ringOffsetHeld()) {
            const Vec2 aimAxis = input.aimAxis();
            if (input.hasAimAxis() && lengthSquared(aimAxis) >= RingShiftAimDeadzone * RingShiftAimDeadzone) {
                targetShiftDirection = normalize(aimAxis);
            } else {
                targetShiftDirection = normalizedOr(facing, targetShiftDirection);
            }
            targetShift = shiftDistance;
        } else if (lengthSquared(facing) > 0.0001f) {
            targetShiftDirection = normalize(facing);
        }
    }
    const float safeDt = std::max(0.0f, dt);
    spellRingShiftDirection =
        smoothDirection(spellRingShiftDirection, targetShiftDirection, RingShiftDirectionResponse, safeDt);

    spellRingShift = lerp(spellRingShift, targetShift, 1.0f - std::exp(-RingShiftDistanceResponse * safeDt));
    throwCooldownRemaining = std::max(0.0f, throwCooldownRemaining - dt);
}

void Player::updateSpriteAnimation(float dt, bool walking)
{
    if (walking != spriteWalking) {
        spriteWalking = walking;
        spriteAnimationTime = 0.0f;
    } else {
        spriteAnimationTime += std::max(0.0f, dt);
    }
}

int Player::spriteFrameIndex() const
{
    return playerSpriteFrameIndex(spriteAnimationTime, spriteWalking);
}

}
