#include "game/EffectSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace majo {

namespace {

struct ParticlePreset {
    ParticleEffectId id = ParticleEffectId::DigDust;
    int count = 1;
    Color colorA{};
    Color colorB{};
    float speed = 40.0f;
    float speedJitter = 12.0f;
    float spread = Pi * 2.0f;
    float radius = 2.0f;
    float radiusJitter = 0.6f;
    float duration = 0.3f;
    float durationJitter = 0.08f;
    Vec2 acceleration{};
    float drag = 3.5f;
    bool directional = false;
    bool ring = false;
    float ringStart = 4.0f;
    float ringEnd = 20.0f;
    Color ringColor{};
};

constexpr std::array<ParticlePreset, 25> ParticlePresets{{
    {ParticleEffectId::DigDust, 8, {142, 104, 66, 195}, {102, 78, 54, 150}, 82.0f, 34.0f, 1.55f, 2.8f, 1.0f, 0.38f, 0.10f, {0.0f, 72.0f}, 2.2f, true, true, 4.0f, 12.0f, {184, 136, 76, 140}},
    {ParticleEffectId::DirtBreak, 14, {154, 110, 66, 220}, {214, 150, 82, 185}, 92.0f, 42.0f, Pi * 2.0f, 3.0f, 1.1f, 0.48f, 0.12f, {0.0f, 92.0f}, 2.8f, false, true, 8.0f, 28.0f, {218, 164, 88, 205}},
    {ParticleEffectId::RockBreak, 12, {122, 126, 132, 220}, {86, 88, 96, 185}, 82.0f, 28.0f, Pi * 2.0f, 3.5f, 1.0f, 0.58f, 0.14f, {0.0f, 116.0f}, 2.4f, false, true, 7.0f, 24.0f, {170, 174, 180, 190}},
    {ParticleEffectId::OreBreak, 18, {244, 204, 84, 230}, {126, 218, 236, 205}, 104.0f, 48.0f, Pi * 2.0f, 2.6f, 1.1f, 0.56f, 0.16f, {0.0f, 72.0f}, 2.2f, false, true, 9.0f, 32.0f, {255, 222, 110, 220}},
    {ParticleEffectId::RingTrail, 4, {196, 172, 255, 135}, {244, 212, 116, 120}, 26.0f, 18.0f, 1.25f, 2.2f, 0.7f, 0.24f, 0.06f, {}, 1.5f, true, false},
    {ParticleEffectId::EnemyHit, 7, {235, 62, 72, 220}, {255, 184, 158, 190}, 88.0f, 34.0f, Pi * 2.0f, 2.6f, 0.8f, 0.34f, 0.08f, {}, 3.0f, false, true, 5.0f, 18.0f, {255, 170, 170, 205}},
    {ParticleEffectId::EnemyPoisonHit, 8, {82, 226, 92, 215}, {168, 78, 214, 180}, 68.0f, 34.0f, Pi * 2.0f, 2.4f, 0.9f, 0.46f, 0.12f, {0.0f, -12.0f}, 2.2f, false, true, 5.0f, 20.0f, {126, 240, 112, 185}},
    {ParticleEffectId::EnemySlowHit, 8, {96, 176, 255, 220}, {210, 244, 255, 180}, 62.0f, 26.0f, Pi * 2.0f, 2.3f, 0.9f, 0.48f, 0.12f, {}, 2.5f, false, true, 5.0f, 20.0f, {142, 214, 255, 180}},
    {ParticleEffectId::EnemyBleedHit, 9, {150, 24, 42, 230}, {238, 54, 78, 190}, 76.0f, 30.0f, Pi * 2.0f, 2.5f, 0.8f, 0.42f, 0.10f, {0.0f, 82.0f}, 2.6f, false, true, 4.0f, 17.0f, {180, 36, 58, 190}},
    {ParticleEffectId::EnemyDeathSoul, 18, {180, 104, 255, 220}, {255, 98, 128, 205}, 76.0f, 42.0f, Pi * 2.0f, 3.2f, 1.1f, 0.70f, 0.18f, {0.0f, -82.0f}, 1.6f, false, true, 10.0f, 38.0f, {255, 92, 116, 230}},
    {ParticleEffectId::CaptureSuccess, 24, {178, 112, 255, 225}, {255, 224, 130, 210}, 128.0f, 48.0f, 1.20f, 2.6f, 0.8f, 0.58f, 0.16f, {}, 2.0f, true, true, 12.0f, 40.0f, {202, 156, 255, 220}},
    {ParticleEffectId::DropPickup, 10, {255, 232, 132, 220}, {142, 228, 248, 180}, 92.0f, 34.0f, 1.70f, 2.0f, 0.6f, 0.36f, 0.08f, {}, 2.4f, true, false},
    {ParticleEffectId::TorchFlicker, 3, {255, 172, 58, 160}, {255, 238, 120, 135}, 24.0f, 14.0f, 1.10f, 1.7f, 0.5f, 0.42f, 0.10f, {0.0f, -36.0f}, 1.2f, true, false},
    {ParticleEffectId::MagicFire, 11, {255, 84, 42, 225}, {255, 214, 84, 185}, 106.0f, 46.0f, 1.45f, 2.8f, 0.9f, 0.42f, 0.12f, {0.0f, -16.0f}, 2.0f, true, true, 7.0f, 24.0f, {255, 112, 58, 210}},
    {ParticleEffectId::MagicIce, 11, {92, 210, 255, 215}, {230, 250, 255, 180}, 88.0f, 32.0f, 1.35f, 2.6f, 0.9f, 0.48f, 0.12f, {}, 2.3f, true, true, 7.0f, 24.0f, {120, 210, 255, 205}},
    {ParticleEffectId::MagicThunder, 13, {255, 232, 74, 230}, {255, 255, 190, 190}, 132.0f, 58.0f, 1.65f, 2.2f, 0.8f, 0.32f, 0.08f, {}, 3.2f, true, true, 6.0f, 23.0f, {255, 230, 90, 220}},
    {ParticleEffectId::MagicWind, 10, {144, 246, 194, 200}, {216, 255, 230, 150}, 112.0f, 44.0f, 1.75f, 2.1f, 0.8f, 0.46f, 0.12f, {}, 1.6f, true, true, 7.0f, 28.0f, {150, 245, 190, 185}},
    {ParticleEffectId::MagicEarth, 12, {194, 134, 72, 220}, {236, 194, 118, 170}, 86.0f, 34.0f, 1.55f, 3.1f, 1.0f, 0.54f, 0.12f, {0.0f, 96.0f}, 2.1f, true, true, 8.0f, 25.0f, {196, 142, 78, 205}},
    {ParticleEffectId::MagicDefault, 10, {220, 210, 255, 210}, {255, 255, 255, 170}, 92.0f, 38.0f, 1.45f, 2.4f, 0.8f, 0.42f, 0.10f, {}, 2.2f, true, true, 7.0f, 22.0f, {235, 235, 255, 190}},
    {ParticleEffectId::PoisonAura, 4, {94, 218, 88, 120}, {180, 72, 220, 105}, 22.0f, 14.0f, Pi * 2.0f, 1.9f, 0.6f, 0.52f, 0.12f, {0.0f, -28.0f}, 1.0f, false, false},
    {ParticleEffectId::SlowAura, 4, {98, 176, 255, 130}, {220, 248, 255, 105}, 18.0f, 12.0f, Pi * 2.0f, 1.8f, 0.6f, 0.56f, 0.12f, {0.0f, -8.0f}, 1.2f, false, false},
    {ParticleEffectId::BleedAura, 4, {190, 34, 54, 125}, {255, 76, 86, 95}, 20.0f, 12.0f, Pi * 2.0f, 1.9f, 0.6f, 0.48f, 0.10f, {0.0f, 42.0f}, 1.8f, false, false},
    {ParticleEffectId::SpecialItemGlimmer, 3, {180, 224, 255, 125}, {255, 224, 128, 115}, 16.0f, 10.0f, Pi * 2.0f, 1.7f, 0.5f, 0.44f, 0.10f, {0.0f, -18.0f}, 1.2f, false, false},
    {ParticleEffectId::WarpCircle, 18, {112, 208, 255, 190}, {255, 224, 112, 170}, 62.0f, 24.0f, Pi * 2.0f, 2.4f, 0.8f, 0.72f, 0.18f, {0.0f, -24.0f}, 1.5f, false, true, 18.0f, 64.0f, {126, 208, 255, 150}},
    {ParticleEffectId::BossCircle, 24, {255, 96, 120, 210}, {255, 210, 96, 190}, 74.0f, 28.0f, Pi * 2.0f, 2.7f, 0.9f, 0.82f, 0.22f, {0.0f, -18.0f}, 1.4f, false, true, 24.0f, 90.0f, {255, 176, 84, 180}},
}};

unsigned char fadeAlpha(Color color, float t)
{
    return static_cast<unsigned char>(static_cast<float>(color.a) * (1.0f - clamp(t, 0.0f, 1.0f)));
}

float seedAngle(Vec2 position)
{
    return std::sin(position.x * 0.073f + position.y * 0.117f) * Pi;
}

std::mt19937& particleRng()
{
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

const ParticlePreset& presetFor(ParticleEffectId id)
{
    const auto it = std::find_if(ParticlePresets.begin(), ParticlePresets.end(), [id](const ParticlePreset& preset) {
        return preset.id == id;
    });
    return it != ParticlePresets.end() ? *it : ParticlePresets.front();
}

float randomRange(float minValue, float maxValue)
{
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(particleRng());
}

Color mixColor(Color a, Color b, float t)
{
    const auto mix = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(std::round(lerp(static_cast<float>(x), static_cast<float>(y), t)));
    };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

ParticleEffectId magicEffectFor(std::string_view element)
{
    if (element == "fire") {
        return ParticleEffectId::MagicFire;
    }
    if (element == "ice") {
        return ParticleEffectId::MagicIce;
    }
    if (element == "thunder") {
        return ParticleEffectId::MagicThunder;
    }
    if (element == "wind") {
        return ParticleEffectId::MagicWind;
    }
    if (element == "earth") {
        return ParticleEffectId::MagicEarth;
    }
    return ParticleEffectId::MagicDefault;
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
        effect.velocity += effect.acceleration * dt;
        effect.position += effect.velocity * dt;
        effect.velocity = effect.velocity * std::max(0.0f, 1.0f - effect.drag * dt);
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

void EffectSystem::spawnParticle(Vec2 position, Vec2 velocity, float radius, Color color, float duration, Vec2 acceleration, float drag)
{
    Effect* effect = effects_.acquire();
    if (!effect) {
        return;
    }
    effect->type = EffectType::Particle;
    effect->position = position;
    effect->velocity = velocity;
    effect->acceleration = acceleration;
    effect->color = color;
    effect->duration = duration;
    effect->startRadius = radius;
    effect->endRadius = radius * 0.35f;
    effect->drag = drag;
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

void EffectSystem::spawn(ParticleEffectId id, Vec2 position, Vec2 direction, float scale)
{
    const ParticlePreset& preset = presetFor(id);
    const Vec2 forward = normalize(direction);
    const float baseAngle = std::atan2(forward.y, forward.x);
    if (preset.ring) {
        spawnRing(
            position,
            preset.ringStart * scale,
            preset.ringEnd * scale,
            preset.ringColor,
            std::max(0.05f, preset.duration * 0.75f));
    }

    for (int i = 0; i < preset.count; ++i) {
        const float angleBase = preset.directional ? baseAngle : seedAngle(position);
        const float angle = angleBase + randomRange(-preset.spread * 0.5f, preset.spread * 0.5f);
        const float speed = std::max(0.0f, preset.speed + randomRange(-preset.speedJitter, preset.speedJitter)) * scale;
        const float radius = std::max(0.6f, preset.radius + randomRange(-preset.radiusJitter, preset.radiusJitter)) * scale;
        const float duration = std::max(0.06f, preset.duration + randomRange(-preset.durationJitter, preset.durationJitter));
        const Vec2 offset = fromAngle(angle) * randomRange(0.0f, 4.0f * scale);
        spawnParticle(
            position + offset,
            fromAngle(angle) * speed,
            radius,
            mixColor(preset.colorA, preset.colorB, randomRange(0.0f, 1.0f)),
            duration,
            preset.acceleration * scale,
            preset.drag);
    }
}

void EffectSystem::spawnDigHit(Vec2 position, Vec2 direction)
{
    spawn(ParticleEffectId::DigDust, position, normalize(direction) * -1.0f);
}

void EffectSystem::spawnTileBreak(Vec2 position, TileType tileType)
{
    ParticleEffectId id = ParticleEffectId::DirtBreak;
    if (tileType == TileType::Rock) {
        id = ParticleEffectId::RockBreak;
    } else if (tileType == TileType::Ore) {
        id = ParticleEffectId::OreBreak;
    }
    spawn(id, position);
}

void EffectSystem::spawnEnemyHit(Vec2 position, std::string_view effect)
{
    ParticleEffectId id = ParticleEffectId::EnemyHit;
    if (effect == "status_poison" || effect == "status_poison_chance") {
        id = ParticleEffectId::EnemyPoisonHit;
    } else if (effect == "status_slow" || effect == "status_slow_chance") {
        id = ParticleEffectId::EnemySlowHit;
    } else if (effect == "status_bleed" || effect == "status_bleed_chance") {
        id = ParticleEffectId::EnemyBleedHit;
    }
    spawn(id, position);
}

void EffectSystem::spawnEnemyDeath(Vec2 position)
{
    spawn(ParticleEffectId::EnemyDeathSoul, position);
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

void EffectSystem::spawnRingTrail(Vec2 position, Vec2 direction)
{
    spawn(ParticleEffectId::RingTrail, position, normalize(direction) * -1.0f);
}

void EffectSystem::spawnCaptureSuccess(Vec2 position, Vec2 direction)
{
    spawn(ParticleEffectId::CaptureSuccess, position, direction);
}

void EffectSystem::spawnDropPickup(Vec2 position, Vec2 direction)
{
    spawn(ParticleEffectId::DropPickup, position, direction);
}

void EffectSystem::spawnTorchFlicker(Vec2 position)
{
    spawn(ParticleEffectId::TorchFlicker, position, {0.0f, -1.0f});
}

void EffectSystem::spawnStatusAura(Vec2 position, std::string_view stateId)
{
    if (stateId == "status_poison") {
        spawn(ParticleEffectId::PoisonAura, position);
    } else if (stateId == "status_slow") {
        spawn(ParticleEffectId::SlowAura, position);
    } else if (stateId == "status_bleed") {
        spawn(ParticleEffectId::BleedAura, position);
    }
}

void EffectSystem::spawnSpecialItemGlimmer(Vec2 position)
{
    spawn(ParticleEffectId::SpecialItemGlimmer, position);
}

void EffectSystem::spawnWarpCircle(Vec2 position, bool boss)
{
    spawn(boss ? ParticleEffectId::BossCircle : ParticleEffectId::WarpCircle, position);
}

void EffectSystem::spawnAreaPulse(Vec2 position, float radius, Color color)
{
    spawnRing(position, std::max(4.0f, radius * 0.15f), std::max(10.0f, radius), color, 0.32f);
}

void EffectSystem::spawnMagicCast(Vec2 origin, Vec2 direction, std::string_view element, float power)
{
    const Vec2 forward = normalize(direction);
    const Vec2 impact = origin + forward * (44.0f + power * 1.5f);
    spawn(magicEffectFor(element), impact, forward, 1.0f + power * 0.01f);
    for (int i = 0; i < 4; ++i) {
        spawn(ParticleEffectId::RingTrail, origin + forward * static_cast<float>(i * 8), forward, 0.8f);
    }
}

}
