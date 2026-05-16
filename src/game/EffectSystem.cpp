#include "game/EffectSystem.hpp"

#include "game/ActorVisual.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
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
    ParticleVisual visual = ParticleVisual::Circle;
};

constexpr std::array<ParticlePreset, 25> ParticlePresets{{
    {ParticleEffectId::DigDust, 4, {142, 104, 66, 220}, {102, 78, 54, 190}, 76.0f, 26.0f, Pi * 2.0f, 5.5f, 1.4f, 0.76f, 0.08f, {0.0f, 330.0f}, 1.45f, false, false, 4.0f, 14.0f, {184, 136, 76, 140}, ParticleVisual::RockShard},
    {ParticleEffectId::DirtBreak, 18, {154, 110, 66, 235}, {214, 150, 82, 205}, 126.0f, 58.0f, Pi * 2.0f, 7.4f, 2.4f, 0.84f, 0.10f, {0.0f, 390.0f}, 1.55f, false, false, 8.0f, 34.0f, {218, 164, 88, 205}, ParticleVisual::RockShard},
    {ParticleEffectId::RockBreak, 18, {122, 126, 132, 235}, {86, 88, 96, 205}, 118.0f, 48.0f, Pi * 2.0f, 8.4f, 2.6f, 0.92f, 0.12f, {0.0f, 420.0f}, 1.45f, false, false, 8.0f, 32.0f, {170, 174, 180, 190}, ParticleVisual::RockShard},
    {ParticleEffectId::OreBreak, 22, {244, 204, 84, 238}, {126, 218, 236, 215}, 142.0f, 62.0f, Pi * 2.0f, 7.6f, 2.7f, 0.88f, 0.12f, {0.0f, 380.0f}, 1.35f, false, false, 10.0f, 38.0f, {255, 222, 110, 220}, ParticleVisual::RockShard},
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

constexpr std::size_t MaxShardPoints = 6;

struct ShardShape {
    std::size_t count = 0;
    std::array<Vec2, MaxShardPoints> points{};
};

constexpr std::array<ShardShape, 8> RockShardShapes{{
    {4, std::array<Vec2, MaxShardPoints>{{{-0.70f, -0.42f}, {0.18f, -0.74f}, {0.78f, 0.08f}, {-0.22f, 0.66f}}}},
    {5, std::array<Vec2, MaxShardPoints>{{{-0.62f, -0.58f}, {0.38f, -0.50f}, {0.72f, 0.18f}, {0.06f, 0.72f}, {-0.72f, 0.20f}}}},
    {3, std::array<Vec2, MaxShardPoints>{{{-0.76f, -0.34f}, {0.64f, -0.64f}, {0.24f, 0.76f}}}},
    {5, std::array<Vec2, MaxShardPoints>{{{-0.48f, -0.70f}, {0.72f, -0.24f}, {0.60f, 0.44f}, {-0.12f, 0.76f}, {-0.76f, 0.02f}}}},
    {4, std::array<Vec2, MaxShardPoints>{{{-0.34f, -0.78f}, {0.76f, -0.18f}, {0.30f, 0.72f}, {-0.78f, 0.26f}}}},
    {6, std::array<Vec2, MaxShardPoints>{{{-0.66f, -0.28f}, {-0.26f, -0.72f}, {0.54f, -0.54f}, {0.80f, 0.10f}, {0.16f, 0.70f}, {-0.58f, 0.48f}}}},
    {4, std::array<Vec2, MaxShardPoints>{{{-0.80f, -0.08f}, {-0.10f, -0.64f}, {0.78f, -0.02f}, {0.12f, 0.76f}}}},
    {5, std::array<Vec2, MaxShardPoints>{{{-0.54f, -0.66f}, {0.28f, -0.72f}, {0.78f, -0.06f}, {0.34f, 0.66f}, {-0.72f, 0.34f}}}},
}};

constexpr float ShardShadowGroundOffsetY = 3.0f;
constexpr float ShardBounceStopSpeed = 34.0f;

unsigned char fadeAlpha(Color color, float t)
{
    return static_cast<unsigned char>(static_cast<float>(color.a) * (1.0f - clamp(t, 0.0f, 1.0f)));
}

unsigned char effectAlpha(const Effect& effect, Color color, float t)
{
    if (effect.visual != ParticleVisual::RockShard) {
        return fadeAlpha(color, t);
    }

    constexpr float HoldSeconds = 0.50f;
    const float hold = std::min(HoldSeconds, std::max(0.0f, effect.duration - 0.08f));
    if (effect.age <= hold) {
        return color.a;
    }

    const float fadeDuration = std::max(0.06f, effect.duration - hold);
    const float u = clamp((effect.age - hold) / fadeDuration, 0.0f, 1.0f);
    const float eased = u * u * (3.0f - 2.0f * u);
    return static_cast<unsigned char>(static_cast<float>(color.a) * (1.0f - eased));
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

int randomInt(int minValue, int maxValue)
{
    std::uniform_int_distribution<int> dist(minValue, maxValue);
    return dist(particleRng());
}

void configureShardPhysics(Effect& effect, bool digHitShard, float scale)
{
    const float safeScale = std::max(0.1f, scale);
    effect.physicsShard = true;
    effect.altitude = randomRange(digHitShard ? 1.5f : 4.0f, digHitShard ? 8.0f : 18.0f) * safeScale;
    effect.verticalVelocity = randomRange(digHitShard ? -45.0f : -95.0f, digHitShard ? 140.0f : 270.0f) * safeScale;
    effect.gravity = randomRange(digHitShard ? 560.0f : 640.0f, digHitShard ? 720.0f : 820.0f) * safeScale;
    effect.bounceRestitution = randomRange(digHitShard ? 0.28f : 0.34f, digHitShard ? 0.42f : 0.52f);
    effect.groundFriction = randomRange(digHitShard ? 9.0f : 7.5f, digHitShard ? 13.0f : 11.5f);
    effect.bouncesRemaining = digHitShard ? 1 : randomInt(1, 2);
    effect.shadowVisualSize = std::max(5.0f, effect.startRadius * randomRange(2.2f, 3.1f));
    effect.duration += randomRange(digHitShard ? 0.00f : 0.08f, digHitShard ? 0.08f : 0.22f);
}

void updateShardPhysics(Effect& effect, float dt)
{
    if (!effect.physicsShard) {
        return;
    }

    effect.verticalVelocity -= effect.gravity * dt;
    effect.altitude += effect.verticalVelocity * dt;
    if (effect.altitude > 0.0f) {
        return;
    }

    effect.altitude = 0.0f;
    if (effect.verticalVelocity < 0.0f && effect.bouncesRemaining > 0) {
        effect.verticalVelocity = -effect.verticalVelocity * effect.bounceRestitution;
        --effect.bouncesRemaining;
        effect.velocity = effect.velocity * 0.62f;
        effect.angularVelocity *= -0.58f;
        if (effect.verticalVelocity < ShardBounceStopSpeed) {
            effect.verticalVelocity = 0.0f;
            effect.bouncesRemaining = 0;
        }
    } else {
        effect.verticalVelocity = 0.0f;
        effect.bouncesRemaining = 0;
    }
}

Color mixColor(Color a, Color b, float t)
{
    const auto mix = [t](unsigned char x, unsigned char y) {
        return static_cast<unsigned char>(std::round(lerp(static_cast<float>(x), static_cast<float>(y), t)));
    };
    return {mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
}

Color applyColorOverride(Color color, Color overrideColor)
{
    if (overrideColor.a == 0) {
        return color;
    }
    return {overrideColor.r, overrideColor.g, overrideColor.b, color.a};
}

float smooth01(float value)
{
    value = clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

float smokePuffScale(const SmokePuff& smoke, float t)
{
    const float growEnd = clamp(smoke.growEnd, 0.12f, 0.36f);
    const float shrinkStart = clamp(std::max(smoke.shrinkStart, growEnd + 0.16f), growEnd + 0.16f, 0.78f);
    const float peakScale = clamp(smoke.peakScale, 1.08f, 1.30f);

    if (t < growEnd) {
        return lerp(0.30f, 1.02f, smooth01(t / growEnd));
    }
    if (t < shrinkStart) {
        return lerp(1.02f, peakScale, smooth01((t - growEnd) / (shrinkStart - growEnd)));
    }
    return lerp(peakScale, 0.0f, smooth01((t - shrinkStart) / (1.0f - shrinkStart)));
}

unsigned char colorTowardWhite(unsigned char channel, float amount)
{
    return static_cast<unsigned char>(
        std::clamp(std::lround(lerp(static_cast<float>(channel), 255.0f, amount)), 0L, 255L));
}

Color smokeHighlightColor(Color color)
{
    return {
        colorTowardWhite(color.r, 0.48f),
        colorTowardWhite(color.g, 0.48f),
        colorTowardWhite(color.b, 0.48f),
        color.a,
    };
}

void renderSmokePuff(Renderer& renderer, const SmokePuff& smoke)
{
    const float t = smoke.duration > 0.0f ? clamp(smoke.age / smoke.duration, 0.0f, 1.0f) : 1.0f;
    const float scale = smokePuffScale(smoke, t);
    const float radius = smoke.radius * scale;
    if (radius <= 0.35f) {
        return;
    }

    const float angle = smoke.phase + smoke.age * 1.2f;
    const Vec2 major = fromAngle(angle) * (radius * smoke.lobeSpread);
    const Vec2 minor{-major.y * 0.56f, major.x * 0.56f};
    const Color highlight = smokeHighlightColor(smoke.color);

    renderer.fillSoftCircle(smoke.position - major * 0.52f + minor * 0.24f, radius * 0.76f, smoke.color);
    renderer.fillSoftCircle(smoke.position + major * 0.42f, radius * 0.70f, smoke.color);
    renderer.fillSoftCircle(smoke.position - minor * 0.44f, radius * 0.58f, smoke.color);
    renderer.fillSoftCircle(smoke.position + Vec2{-radius * 0.16f, -radius * 0.18f}, radius * 0.40f, highlight);
}

Vec2 rotatePoint(Vec2 point, float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {point.x * c - point.y * s, point.x * s + point.y * c};
}

Vec2 snapPoint(Vec2 point)
{
    return {std::round(point.x), std::round(point.y)};
}

Vec2 effectDrawPosition(const Effect& effect)
{
    return effect.physicsShard ? elevatedDrawPosition(effect.position, effect.altitude) : effect.position;
}

Color shardOutlineColor(Color color)
{
    const auto darken = [](unsigned char value) {
        return static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(value) * 0.18f), 0L, 255L));
    };
    return {
        darken(color.r),
        darken(color.g),
        darken(color.b),
        static_cast<unsigned char>(std::clamp(std::lround(static_cast<float>(color.a) * 0.95f), 0L, 255L)),
    };
}

void renderRockShard(Renderer& renderer, const Effect& effect, Vec2 center, Color color, float radius)
{
    if (radius <= 0.0f || color.a == 0) {
        return;
    }

    const ShardShape& shape = RockShardShapes[static_cast<std::size_t>(std::abs(effect.shardVariant)) % RockShardShapes.size()];
    const std::size_t count = std::clamp(shape.count, std::size_t{3}, MaxShardPoints);
    const float outlineRadius = radius + std::max(1.35f, radius * 0.22f);

    std::array<Vec2, MaxShardPoints> outlinePoints{};
    std::array<Vec2, MaxShardPoints> fillPoints{};
    for (std::size_t i = 0; i < count; ++i) {
        const Vec2 base{shape.points[i].x * effect.shardAspect, shape.points[i].y};
        const Vec2 rotated = rotatePoint(base, effect.rotation);
        outlinePoints[i] = snapPoint(center + rotated * outlineRadius);
        fillPoints[i] = snapPoint(center + rotated * radius);
    }

    renderer.fillPolygon(outlinePoints.data(), count, shardOutlineColor(color));
    renderer.fillPolygon(fillPoints.data(), count, color);
}

bool isDepthSortedEffect(const Effect& effect)
{
    return effect.type == EffectType::Particle && effect.visual == ParticleVisual::RockShard;
}

bool isDepthSortedSmoke(const SmokePuff&)
{
    return true;
}

void renderEffectVisual(Renderer& renderer, const Effect& effect)
{
    const float t = effect.duration > 0.0f ? effect.age / effect.duration : 1.0f;
    Color color = effect.color;
    color.a = effectAlpha(effect, color, t);
    const float radius = lerp(effect.startRadius, effect.endRadius, t);
    const Vec2 drawPosition = effectDrawPosition(effect);
    if (effect.type == EffectType::Ring) {
        renderer.drawCircle(drawPosition, radius, color);
    } else if (effect.visual == ParticleVisual::RockShard) {
        renderRockShard(renderer, effect, drawPosition, color, std::max(1.0f, radius));
    } else {
        renderer.fillCircle(drawPosition, std::max(0.8f, radius), color);
    }
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
        updateShardPhysics(effect, dt);
        effect.velocity += effect.acceleration * dt;
        effect.position += effect.velocity * dt;
        effect.velocity = effect.velocity * std::max(0.0f, 1.0f - effect.drag * dt);
        if (effect.physicsShard && effect.altitude <= 0.0f && effect.verticalVelocity <= 0.0f) {
            effect.velocity = effect.velocity * std::max(0.0f, 1.0f - effect.groundFriction * dt);
        }
        effect.rotation += effect.angularVelocity * dt;
    }

    for (SmokePuff& smoke : smokePuffs_.items()) {
        if (!smoke.active) {
            continue;
        }
        smoke.age += dt;
        if (smoke.age >= smoke.duration) {
            smoke.active = false;
            continue;
        }
        smoke.position += smoke.velocity * dt;
        smoke.velocity = smoke.velocity * std::max(0.0f, 1.0f - 2.2f * dt);
    }

    for (DamagePopup& popup : damagePopups_.items()) {
        if (!popup.active) {
            continue;
        }
        popup.age += dt;
        if (popup.age >= popup.duration) {
            popup.active = false;
            continue;
        }
    }
}

void EffectSystem::render(Renderer& renderer)
{
    renderSmokeLayer(renderer, EffectLayer::World);
    renderLayer(renderer, EffectLayer::World);
}

void EffectSystem::renderShadows(Renderer& renderer)
{
    for (const Effect& effect : effects_.items()) {
        if (!effect.active || !effect.physicsShard || !isDepthSortedEffect(effect)) {
            continue;
        }

        const float t = effect.duration > 0.0f ? effect.age / effect.duration : 1.0f;
        const unsigned char alpha = effectAlpha(effect, {0, 0, 0, 58}, t);
        if (alpha == 0) {
            continue;
        }

        renderer.drawActorShadow(
            actorShadowAnchor(effect.position, ShardShadowGroundOffsetY),
            actorShadowVisualSizeForAltitude(effect.shadowVisualSize, effect.altitude),
            {0, 0, 0, alpha});
    }
}

void EffectSystem::appendRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer)
{
    for (const SmokePuff& smoke : smokePuffs_.items()) {
        if (!smoke.active || !isDepthSortedSmoke(smoke)) {
            continue;
        }

        entries.push_back(DepthRenderEntry{
            smoke.position.y,
            [&renderer, &smoke]() {
                renderSmokePuff(renderer, smoke);
            },
        });
    }

    for (const Effect& effect : effects_.items()) {
        if (!effect.active || !isDepthSortedEffect(effect)) {
            continue;
        }

        entries.push_back(DepthRenderEntry{
            effect.position.y,
            [&renderer, &effect]() {
                renderEffectVisual(renderer, effect);
            },
        });
    }
}

void EffectSystem::renderForeground(Renderer& renderer)
{
    renderSmokeLayer(renderer, EffectLayer::Foreground);
    renderLayer(renderer, EffectLayer::Foreground);
}

void EffectSystem::renderLayer(Renderer& renderer, EffectLayer layer)
{
    for (const Effect& effect : effects_.items()) {
        if (!effect.active || effect.layer != layer) {
            continue;
        }
        if (isDepthSortedEffect(effect)) {
            continue;
        }
        renderEffectVisual(renderer, effect);
    }
}

void EffectSystem::renderSmokeLayer(Renderer& renderer, EffectLayer layer)
{
    for (const SmokePuff& smoke : smokePuffs_.items()) {
        if (!smoke.active || smoke.layer != layer) {
            continue;
        }
        if (isDepthSortedSmoke(smoke)) {
            continue;
        }
        renderSmokePuff(renderer, smoke);
    }
}

void EffectSystem::renderDamagePopups(Renderer& renderer)
{
    char buffer[16]{};
    for (const DamagePopup& popup : damagePopups_.items()) {
        if (!popup.active || popup.amount < 0) {
            continue;
        }

        const float t = popup.duration > 0.0f ? clamp(popup.age / popup.duration, 0.0f, 1.0f) : 1.0f;
        const float fade = t < 0.78f ? 1.0f : 1.0f - clamp((t - 0.78f) / 0.22f, 0.0f, 1.0f);
        const unsigned char alpha = static_cast<unsigned char>(std::clamp(std::lround(255.0f * fade), 0L, 255L));
        if (alpha == 0) {
            continue;
        }

        std::snprintf(buffer, sizeof(buffer), "%d", popup.amount);
        const int textScale = t < 0.10f ? 4 : 3;
        const Vec2 size = renderer.measureText(buffer, textScale, TextStyle::Italic);
        float hopHeight = 0.0f;
        if (t < 0.58f) {
            const float u = t / 0.58f;
            hopHeight = std::sin(u * Pi) * 34.0f;
        } else {
            const float u = (t - 0.58f) / 0.42f;
            hopHeight = std::sin(u * Pi) * 13.0f;
        }
        const Vec2 center = popup.position + popup.velocity * popup.age - Vec2{0.0f, hopHeight};
        const Vec2 pos = center - Vec2{size.x * 0.5f, size.y * 0.5f};
        const Color textColor = popup.style == DamagePopupStyle::Player
            ? Color{255, 72, 64, alpha}
            : Color{255, 255, 255, alpha};
        const Color shadowColor{0, 0, 0, static_cast<unsigned char>(std::clamp(std::lround(190.0f * fade), 0L, 255L))};

        renderer.drawText(pos + Vec2{2.0f, 2.0f}, buffer, shadowColor, textScale, TextStyle::Italic);
        renderer.drawText(pos, buffer, textColor, textScale, TextStyle::Italic);
    }
}

void EffectSystem::spawnSmokeBurst(Vec2 position, SmokeBurstOptions options)
{
    const int count = std::clamp(options.count, 0, 96);
    const float size = std::max(0.1f, options.size);
    const float sizeJitter = clamp(options.sizeJitter, 0.0f, 0.95f);
    const float spreadRadius = std::max(0.0f, options.spreadRadius);
    const float speed = std::max(0.0f, options.speed);
    const float riseSpeed = std::max(0.0f, options.riseSpeed);
    const float baseDuration = std::max(0.06f, options.duration);
    const float durationJitter = std::max(0.0f, options.durationJitter);

    for (int i = 0; i < count; ++i) {
        SmokePuff* smoke = smokePuffs_.acquire();
        if (!smoke) {
            return;
        }

        const float angle = seedAngle(position) +
            (static_cast<float>(i) / std::max(1.0f, static_cast<float>(count))) * Pi * 2.0f +
            randomRange(-0.46f, 0.46f);
        const Vec2 direction = fromAngle(angle);
        const float distance = spreadRadius * std::sqrt(randomRange(0.0f, 1.0f));
        const float radiusScale = randomRange(1.0f - sizeJitter, 1.0f + sizeJitter);
        const float duration = std::max(0.08f, baseDuration + randomRange(-durationJitter, durationJitter));
        const float outwardSpeed = randomRange(speed * 0.35f, speed * 1.10f);

        smoke->layer = options.layer;
        smoke->position = position + direction * distance + Vec2{randomRange(-2.0f, 2.0f), randomRange(-2.0f, 2.0f)};
        smoke->velocity = direction * outwardSpeed + Vec2{randomRange(-5.0f, 5.0f), -randomRange(riseSpeed * 0.45f, riseSpeed * 1.10f)};
        smoke->color = mixColor(options.colorA, options.colorB, randomRange(0.0f, 1.0f));
        smoke->duration = duration;
        smoke->radius = size * radiusScale;
        smoke->growEnd = randomRange(0.18f, 0.28f);
        smoke->shrinkStart = randomRange(0.52f, 0.68f);
        smoke->peakScale = randomRange(1.14f, 1.26f);
        smoke->lobeSpread = randomRange(0.24f, 0.52f);
        smoke->phase = randomRange(0.0f, Pi * 2.0f);
    }
}

void EffectSystem::spawnDamagePopup(Vec2 position, int amount, DamagePopupStyle style)
{
    if (amount < 0) {
        return;
    }

    DamagePopup* popup = damagePopups_.acquire();
    if (!popup) {
        return;
    }

    popup->position = position + Vec2{randomRange(-12.0f, 12.0f), randomRange(-34.0f, -26.0f)};
    popup->velocity = {randomRange(-10.0f, 10.0f), 0.0f};
    popup->duration = 0.92f + randomRange(-0.04f, 0.04f);
    popup->amount = amount;
    popup->style = style;
}

void EffectSystem::spawnRing(Vec2 position, float startRadius, float endRadius, Color color, float duration, EffectLayer layer)
{
    Effect* effect = effects_.acquire();
    if (!effect) {
        return;
    }
    effect->type = EffectType::Ring;
    effect->layer = layer;
    effect->position = position;
    effect->color = color;
    effect->duration = duration;
    effect->startRadius = startRadius;
    effect->endRadius = endRadius;
}

Effect* EffectSystem::spawnParticle(
    Vec2 position,
    Vec2 velocity,
    float radius,
    Color color,
    float duration,
    Vec2 acceleration,
    float drag,
    EffectLayer layer,
    ParticleVisual visual,
    int shardVariant,
    float rotation,
    float angularVelocity,
    float shardAspect)
{
    Effect* effect = effects_.acquire();
    if (!effect) {
        return nullptr;
    }
    effect->type = EffectType::Particle;
    effect->layer = layer;
    effect->position = position;
    effect->velocity = velocity;
    effect->acceleration = acceleration;
    effect->color = color;
    effect->visual = visual;
    effect->duration = duration;
    effect->startRadius = radius;
    effect->endRadius = radius * 0.35f;
    effect->drag = drag;
    effect->rotation = rotation;
    effect->angularVelocity = angularVelocity;
    effect->shardAspect = shardAspect;
    effect->shardVariant = shardVariant;
    return effect;
}

void EffectSystem::spawnBurst(Vec2 position, int count, Color color, float speed, float radius, float duration, EffectLayer layer)
{
    const float offset = seedAngle(position);
    for (int i = 0; i < count; ++i) {
        const float angle = offset + (static_cast<float>(i) / static_cast<float>(count)) * Pi * 2.0f;
        const float speedScale = 0.65f + 0.35f * std::sin(angle * 2.7f + position.x * 0.01f);
        spawnParticle(position, fromAngle(angle) * (speed * speedScale), radius, color, duration, {}, 3.5f, layer);
    }
}

void EffectSystem::spawn(ParticleEffectId id, Vec2 position, Vec2 direction, float scale, EffectLayer layer, Color colorOverride)
{
    const ParticlePreset& preset = presetFor(id);
    const Vec2 forward = normalize(direction);
    const float baseAngle = std::atan2(forward.y, forward.x);
    if (preset.ring) {
        spawnRing(
            position,
            preset.ringStart * scale,
            preset.ringEnd * scale,
            applyColorOverride(preset.ringColor, colorOverride),
            std::max(0.05f, preset.duration * 0.75f),
            layer);
    }

    for (int i = 0; i < preset.count; ++i) {
        const float angleBase = preset.directional ? baseAngle : seedAngle(position);
        const float angle = angleBase + randomRange(-preset.spread * 0.5f, preset.spread * 0.5f);
        const float speed = std::max(0.0f, preset.speed + randomRange(-preset.speedJitter, preset.speedJitter)) * scale;
        const float radius = std::max(0.6f, preset.radius + randomRange(-preset.radiusJitter, preset.radiusJitter)) * scale;
        const float duration = std::max(0.06f, preset.duration + randomRange(-preset.durationJitter, preset.durationJitter));
        const bool rockShard = preset.visual == ParticleVisual::RockShard;
        const bool digHitShard = preset.id == ParticleEffectId::DigDust;
        const Vec2 shardScatterVelocity = rockShard
            ? Vec2{
                randomRange(digHitShard ? -22.0f : -42.0f, digHitShard ? 22.0f : 42.0f) * scale,
                randomRange(digHitShard ? -26.0f : -54.0f, digHitShard ? 26.0f : 54.0f) * scale}
            : Vec2{};
        const float offsetRange = rockShard ? (digHitShard ? 5.0f : 9.0f) : 4.0f;
        const Vec2 offset = fromAngle(angle) * randomRange(0.0f, offsetRange * scale);
        const int shardVariant = rockShard ? randomInt(0, static_cast<int>(RockShardShapes.size() - 1)) : 0;
        const float rotation = rockShard ? angle + randomRange(-Pi * 0.65f, Pi * 0.65f) : 0.0f;
        const float angularVelocity = rockShard ? randomRange(-8.0f, 8.0f) : 0.0f;
        const float shardAspect = rockShard ? randomRange(0.78f, 1.32f) : 1.0f;
        const Color particleColor = applyColorOverride(
            mixColor(preset.colorA, preset.colorB, randomRange(0.0f, 1.0f)),
            colorOverride);
        Effect* spawned = spawnParticle(
            position + offset,
            fromAngle(angle) * speed + shardScatterVelocity,
            radius,
            particleColor,
            duration,
            rockShard ? Vec2{} : preset.acceleration * scale,
            preset.drag,
            layer,
            preset.visual,
            shardVariant,
            rotation,
            angularVelocity,
            shardAspect);
        if (spawned != nullptr && rockShard) {
            configureShardPhysics(*spawned, digHitShard, scale);
        }
    }
}

void EffectSystem::spawnDigHit(Vec2 position, Vec2 direction, Color colorOverride)
{
    spawn(ParticleEffectId::DigDust, position, normalize(direction) * -1.0f, 1.0f, EffectLayer::Foreground, colorOverride);
}

void EffectSystem::spawnTileBreak(Vec2 position, TileType tileType, Color colorOverride)
{
    ParticleEffectId id = ParticleEffectId::DirtBreak;
    if (tileType == TileType::Rock || tileType == TileType::HardRock) {
        id = ParticleEffectId::RockBreak;
    } else if (tileType == TileType::Ore) {
        id = ParticleEffectId::OreBreak;
    }
    spawnSmokeBurst(position);
    spawn(id, position, {1.0f, 0.0f}, 1.0f, EffectLayer::Foreground, colorOverride);
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
    } else if (effect == "fire") {
        id = ParticleEffectId::MagicFire;
    } else if (effect == "ice" || effect == "water") {
        id = ParticleEffectId::MagicIce;
    } else if (effect == "thunder" || effect == "status_paralyze" || effect == "status_paralyze_chance") {
        id = ParticleEffectId::MagicThunder;
    } else if (effect == "wind") {
        id = ParticleEffectId::MagicWind;
    } else if (effect == "earth") {
        id = ParticleEffectId::MagicEarth;
    } else if (effect == "magic") {
        id = ParticleEffectId::MagicDefault;
    }
    spawn(id, position);
}

void EffectSystem::spawnEnemyDeath(Vec2 position)
{
    SmokeBurstOptions options;
    options.count = 12;
    options.size = 18.0f;
    options.sizeJitter = 0.42f;
    options.spreadRadius = 9.0f;
    options.speed = 22.0f;
    options.riseSpeed = 24.0f;
    options.duration = 0.72f;
    options.durationJitter = 0.14f;
    options.colorA = {188, 146, 232, 86};
    options.colorB = {92, 74, 132, 72};
    options.layer = EffectLayer::Foreground;
    spawnSmokeBurst(position, options);
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

void EffectSystem::spawnForegroundRingTrail(Vec2 position, Vec2 direction)
{
    spawn(ParticleEffectId::RingTrail, position, normalize(direction) * -1.0f, 1.0f, EffectLayer::Foreground);
}

void EffectSystem::spawnCaptureSuccess(Vec2 position, Vec2 direction)
{
    spawn(ParticleEffectId::CaptureSuccess, position, direction);
}

void EffectSystem::spawnDropPickup(Vec2 position, Vec2 direction)
{
    spawn(ParticleEffectId::DropPickup, position, direction);
}

void EffectSystem::spawnMaterialFloat(Vec2 position, Color color)
{
    color.a = static_cast<unsigned char>(std::clamp(static_cast<int>(color.a), 40, 220));
    spawnParticle(
        position + Vec2{randomRange(-8.0f, 8.0f), randomRange(-3.0f, 3.0f)},
        {randomRange(-9.0f, 9.0f), randomRange(-34.0f, -20.0f)},
        randomRange(1.2f, 2.3f),
        color,
        randomRange(0.55f, 0.82f),
        {0.0f, -8.0f},
        0.65f,
        EffectLayer::World);
}

void EffectSystem::spawnTorchFlicker(Vec2 position)
{
    spawn(ParticleEffectId::TorchFlicker, position, {0.0f, -1.0f});
}

void EffectSystem::spawnForegroundTorchFlicker(Vec2 position)
{
    spawn(ParticleEffectId::TorchFlicker, position, {0.0f, -1.0f}, 1.0f, EffectLayer::Foreground);
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

void EffectSystem::spawnForegroundSpecialItemGlimmer(Vec2 position)
{
    spawn(ParticleEffectId::SpecialItemGlimmer, position, {1.0f, 0.0f}, 1.0f, EffectLayer::Foreground);
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
