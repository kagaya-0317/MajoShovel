#include "game/EffectSystem.hpp"

#include <algorithm>

namespace majo {

namespace {

unsigned char fadeAlpha(Color color, float t)
{
    return static_cast<unsigned char>(static_cast<float>(color.a) * (1.0f - clamp(t, 0.0f, 1.0f)));
}

float seedAngle(Vec2 position)
{
    return std::sin(position.x * 0.073f + position.y * 0.117f) * Pi;
}

}

void EffectSystem::update(float dt)
{
    for (Effect& effect : effects_.items()) {
        if (!effect.active) {
            continue;
        }
        effect.age += dt;
        if (effect.age >= effect.duration) {
            effect.active = false;
            continue;
        }
        effect.position += effect.velocity * dt;
        effect.velocity = effect.velocity * std::max(0.0f, 1.0f - 3.5f * dt);
    }
}

void EffectSystem::render(Renderer& renderer)
{
    for (const Effect& effect : effects_.items()) {
        if (!effect.active) {
            continue;
        }
        const float t = effect.duration > 0.0f ? effect.age / effect.duration : 1.0f;
        Color color = effect.color;
        color.a = fadeAlpha(color, t);
        const float radius = lerp(effect.startRadius, effect.endRadius, t);
        if (effect.type == EffectType::Ring) {
            renderer.drawCircle(effect.position, radius, color);
        } else {
            renderer.fillCircle(effect.position, std::max(0.8f, radius), color);
        }
    }
}

void EffectSystem::spawnRing(Vec2 position, float startRadius, float endRadius, Color color, float duration)
{
    Effect* effect = effects_.acquire();
    if (!effect) {
        return;
    }
    effect->type = EffectType::Ring;
    effect->position = position;
    effect->color = color;
    effect->duration = duration;
    effect->startRadius = startRadius;
    effect->endRadius = endRadius;
}

void EffectSystem::spawnParticle(Vec2 position, Vec2 velocity, float radius, Color color, float duration)
{
    Effect* effect = effects_.acquire();
    if (!effect) {
        return;
    }
    effect->type = EffectType::Particle;
    effect->position = position;
    effect->velocity = velocity;
    effect->color = color;
    effect->duration = duration;
    effect->startRadius = radius;
    effect->endRadius = radius * 0.35f;
}

void EffectSystem::spawnBurst(Vec2 position, int count, Color color, float speed, float radius, float duration)
{
    const float offset = seedAngle(position);
    for (int i = 0; i < count; ++i) {
        const float angle = offset + (static_cast<float>(i) / static_cast<float>(count)) * Pi * 2.0f;
        const float speedScale = 0.65f + 0.35f * std::sin(angle * 2.7f + position.x * 0.01f);
        spawnParticle(position, fromAngle(angle) * (speed * speedScale), radius, color, duration);
    }
}

void EffectSystem::spawnDigHit(Vec2 position)
{
    spawnRing(position, 3.0f, 12.0f, {180, 132, 72, 150}, 0.18f);
    spawnBurst(position, 3, {126, 92, 58, 180}, 42.0f, 2.2f, 0.30f);
}

void EffectSystem::spawnTileBreak(Vec2 position)
{
    spawnRing(position, 8.0f, 27.0f, {218, 164, 88, 210}, 0.28f);
    spawnBurst(position, 9, {150, 108, 66, 220}, 86.0f, 3.0f, 0.45f);
}

void EffectSystem::spawnEnemyHit(Vec2 position)
{
    spawnRing(position, 5.0f, 18.0f, {255, 170, 170, 205}, 0.20f);
    spawnBurst(position, 4, {220, 54, 70, 210}, 70.0f, 2.5f, 0.32f);
}

void EffectSystem::spawnEnemyDeath(Vec2 position)
{
    spawnRing(position, 9.0f, 36.0f, {255, 92, 116, 230}, 0.36f);
    spawnBurst(position, 14, {178, 38, 54, 230}, 112.0f, 3.4f, 0.52f);
}

void EffectSystem::spawnThrowStart(Vec2 position, Vec2 direction)
{
    const Vec2 forward = normalize(direction);
    const Vec2 side{-forward.y, forward.x};
    spawnRing(position, 10.0f, 30.0f, {225, 184, 84, 210}, 0.25f);
    for (int i = -2; i <= 2; ++i) {
        spawnParticle(position + side * static_cast<float>(i * 5), forward * 120.0f + side * static_cast<float>(i * 16), 2.6f, {246, 214, 128, 200}, 0.28f);
    }
}

void EffectSystem::spawnReturn(Vec2 position)
{
    spawnRing(position, 5.0f, 22.0f, {170, 142, 240, 190}, 0.22f);
    spawnBurst(position, 6, {130, 125, 210, 180}, 58.0f, 2.2f, 0.28f);
}

void EffectSystem::spawnAreaPulse(Vec2 position, float radius, Color color)
{
    spawnRing(position, std::max(4.0f, radius * 0.15f), std::max(10.0f, radius), color, 0.32f);
}

void EffectSystem::spawnMagicCast(Vec2 origin, Vec2 direction, std::string_view element, float power)
{
    Color color{235, 235, 255, 220};
    if (element == "fire") {
        color = {255, 102, 58, 225};
    } else if (element == "ice") {
        color = {120, 210, 255, 220};
    } else if (element == "thunder") {
        color = {255, 230, 90, 230};
    } else if (element == "wind") {
        color = {150, 245, 190, 210};
    } else if (element == "earth") {
        color = {196, 142, 78, 220};
    }

    const Vec2 forward = normalize(direction);
    const Vec2 impact = origin + forward * (44.0f + power * 1.5f);
    spawnRing(impact, 7.0f, 20.0f + power * 0.4f, color, 0.26f);
    spawnBurst(impact, 7, color, 80.0f + power * 2.0f, 2.8f, 0.38f);
    for (int i = 0; i < 4; ++i) {
        spawnParticle(origin + forward * static_cast<float>(i * 8), forward * (120.0f + power * 2.0f), 2.3f, color, 0.22f);
    }
}

}
