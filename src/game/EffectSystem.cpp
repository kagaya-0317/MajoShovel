#include "game/EffectSystem.hpp"

#include "game/ActorVisual.hpp"
#include "game/EffectPreviewCatalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace majo {

namespace {

constexpr int LightweightMaxEffects = 512;
constexpr int LightweightMaxSmokePuffs = 96;
constexpr std::string_view AudioSeDigHit = "se.dig.hit";
constexpr std::string_view AudioSeDigBreak = "se.dig.break";
constexpr std::string_view AudioSeDigOreBreak = "se.dig.ore_break";
constexpr std::string_view AudioSeAttackHit = "se.attack.hit";
constexpr std::string_view AudioSePickup = "se.pickup";
constexpr std::string_view AudioSeEnemyDefeat = "se.enemy.defeat";
constexpr std::string_view AudioSeEnemyTransform = "se.enemy.transform";
constexpr std::string_view AudioSeCaptureSuccess = "se.capture.success";
constexpr std::string_view AudioSeCrateBreak = "se.crate.break";
constexpr std::string_view AudioSeItemBreak = "se.item.break";
constexpr std::string_view AudioSeItemBreakCeramic = "se.item.break.ceramic";
constexpr std::string_view AudioSeItemBreakGlass = "se.item.break.glass";
constexpr std::string_view AudioSeExplosion = "se.explosion.boom";

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

constexpr std::array<ParticlePreset, 37> ParticlePresets{{
    {ParticleEffectId::DigDust, 7, {142, 104, 66, 220}, {102, 78, 54, 190}, 138.0f, 66.0f, Pi * 2.0f, 5.2f, 1.5f, 0.76f, 0.08f, {0.0f, 330.0f}, 1.30f, false, false, 4.0f, 14.0f, {184, 136, 76, 140}, ParticleVisual::RockShard},
    {ParticleEffectId::DirtBreak, 18, {154, 110, 66, 235}, {214, 150, 82, 205}, 126.0f, 58.0f, Pi * 2.0f, 7.4f, 2.4f, 0.84f, 0.10f, {0.0f, 390.0f}, 1.55f, false, false, 8.0f, 34.0f, {218, 164, 88, 205}, ParticleVisual::RockShard},
    {ParticleEffectId::RockBreak, 18, {122, 126, 132, 235}, {86, 88, 96, 205}, 118.0f, 48.0f, Pi * 2.0f, 8.4f, 2.6f, 0.92f, 0.12f, {0.0f, 420.0f}, 1.45f, false, false, 8.0f, 32.0f, {170, 174, 180, 190}, ParticleVisual::RockShard},
    {ParticleEffectId::OreBreak, 22, {244, 204, 84, 238}, {126, 218, 236, 215}, 142.0f, 62.0f, Pi * 2.0f, 7.6f, 2.7f, 0.88f, 0.12f, {0.0f, 380.0f}, 1.35f, false, false, 10.0f, 38.0f, {255, 222, 110, 220}, ParticleVisual::RockShard},
    {ParticleEffectId::EnemyHit, 18, {255, 226, 74, 236}, {255, 108, 52, 204}, 156.0f, 66.0f, Pi * 2.0f, 2.1f, 0.7f, 0.34f, 0.08f, {}, 2.7f, false, false, 5.0f, 18.0f, {255, 232, 104, 232}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyPoisonHit, 14, {138, 255, 94, 222}, {178, 72, 228, 190}, 118.0f, 52.0f, Pi * 2.0f, 2.0f, 0.7f, 0.42f, 0.11f, {0.0f, -12.0f}, 2.4f, false, false, 5.0f, 20.0f, {170, 255, 104, 205}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyBleedHit, 15, {226, 32, 56, 236}, {255, 114, 74, 198}, 132.0f, 52.0f, Pi * 2.0f, 2.0f, 0.7f, 0.38f, 0.10f, {0.0f, 42.0f}, 2.8f, false, false, 4.0f, 17.0f, {238, 46, 64, 210}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemySleepHit, 14, {186, 134, 255, 226}, {112, 168, 255, 190}, 112.0f, 44.0f, Pi * 2.0f, 2.0f, 0.7f, 0.48f, 0.12f, {0.0f, -16.0f}, 2.5f, false, false, 5.0f, 20.0f, {206, 174, 255, 205}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyConfuseHit, 15, {255, 126, 230, 228}, {116, 226, 255, 194}, 122.0f, 50.0f, Pi * 2.0f, 2.0f, 0.7f, 0.42f, 0.11f, {}, 2.6f, false, false, 5.0f, 20.0f, {255, 154, 238, 210}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyBlindHit, 14, {70, 64, 118, 228}, {206, 190, 255, 188}, 110.0f, 44.0f, Pi * 2.0f, 2.1f, 0.7f, 0.44f, 0.11f, {}, 2.7f, false, false, 5.0f, 20.0f, {110, 96, 178, 205}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyFireHit, 16, {255, 86, 38, 236}, {255, 198, 74, 200}, 148.0f, 62.0f, Pi * 2.0f, 2.1f, 0.7f, 0.34f, 0.08f, {0.0f, -20.0f}, 2.8f, false, false, 5.0f, 18.0f, {255, 118, 50, 222}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyWaterHit, 15, {74, 186, 255, 228}, {174, 246, 255, 190}, 122.0f, 52.0f, Pi * 2.0f, 2.0f, 0.7f, 0.42f, 0.11f, {}, 2.5f, false, false, 5.0f, 20.0f, {108, 214, 255, 210}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyIceHit, 15, {128, 226, 255, 228}, {242, 255, 255, 190}, 112.0f, 46.0f, Pi * 2.0f, 2.1f, 0.7f, 0.46f, 0.12f, {}, 2.6f, false, false, 5.0f, 20.0f, {166, 238, 255, 210}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyThunderHit, 16, {255, 238, 82, 236}, {230, 250, 255, 196}, 150.0f, 64.0f, Pi * 2.0f, 2.0f, 0.7f, 0.30f, 0.08f, {}, 3.0f, false, false, 5.0f, 18.0f, {255, 242, 110, 224}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyWindHit, 14, {128, 246, 190, 222}, {226, 255, 224, 184}, 126.0f, 52.0f, Pi * 2.0f, 2.0f, 0.7f, 0.44f, 0.11f, {}, 2.4f, false, false, 5.0f, 20.0f, {152, 250, 198, 205}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyEarthHit, 15, {204, 142, 76, 230}, {246, 206, 132, 190}, 116.0f, 48.0f, Pi * 2.0f, 2.3f, 0.8f, 0.48f, 0.12f, {0.0f, 42.0f}, 2.6f, false, false, 5.0f, 20.0f, {214, 156, 86, 210}, ParticleVisual::ImpactSpark},
    {ParticleEffectId::EnemyDeathSoul, 18, {180, 104, 255, 220}, {255, 98, 128, 205}, 76.0f, 42.0f, Pi * 2.0f, 3.2f, 1.1f, 0.70f, 0.18f, {0.0f, -82.0f}, 1.6f, false, true, 10.0f, 38.0f, {255, 92, 116, 230}},
    {ParticleEffectId::CaptureSuccess, 28, {178, 112, 255, 225}, {255, 224, 130, 210}, 134.0f, 52.0f, Pi * 2.0f, 2.8f, 0.9f, 0.62f, 0.16f, {}, 2.0f, false, true, 20.0f, 70.0f, {222, 178, 255, 252}},
    {ParticleEffectId::DropPickup, 10, {255, 232, 132, 220}, {142, 228, 248, 180}, 92.0f, 34.0f, Pi * 2.0f, 2.0f, 0.6f, 0.36f, 0.08f, {}, 2.4f, false, false},
    {ParticleEffectId::TorchFlicker, 3, {255, 172, 58, 160}, {255, 238, 120, 135}, 24.0f, 14.0f, 1.10f, 1.7f, 0.5f, 0.42f, 0.10f, {0.0f, -36.0f}, 1.2f, true, false},
    {ParticleEffectId::MagicFire, 11, {255, 84, 42, 225}, {255, 214, 84, 185}, 106.0f, 46.0f, 1.45f, 2.8f, 0.9f, 0.42f, 0.12f, {0.0f, -16.0f}, 2.0f, true, true, 7.0f, 24.0f, {255, 112, 58, 210}},
    {ParticleEffectId::MagicIce, 11, {92, 210, 255, 215}, {230, 250, 255, 180}, 88.0f, 32.0f, 1.35f, 2.6f, 0.9f, 0.48f, 0.12f, {}, 2.3f, true, true, 7.0f, 24.0f, {120, 210, 255, 205}},
    {ParticleEffectId::MagicThunder, 13, {255, 232, 74, 230}, {255, 255, 190, 190}, 132.0f, 58.0f, 1.65f, 2.2f, 0.8f, 0.32f, 0.08f, {}, 3.2f, true, true, 6.0f, 23.0f, {255, 230, 90, 220}},
    {ParticleEffectId::MagicWind, 10, {144, 246, 194, 200}, {216, 255, 230, 150}, 112.0f, 44.0f, 1.75f, 2.1f, 0.8f, 0.46f, 0.12f, {}, 1.6f, true, true, 7.0f, 28.0f, {150, 245, 190, 185}},
    {ParticleEffectId::MagicEarth, 12, {194, 134, 72, 220}, {236, 194, 118, 170}, 86.0f, 34.0f, 1.55f, 3.1f, 1.0f, 0.54f, 0.12f, {0.0f, 96.0f}, 2.1f, true, true, 8.0f, 25.0f, {196, 142, 78, 205}},
    {ParticleEffectId::MagicDefault, 10, {220, 210, 255, 210}, {255, 255, 255, 170}, 92.0f, 38.0f, 1.45f, 2.4f, 0.8f, 0.42f, 0.10f, {}, 2.2f, true, true, 7.0f, 22.0f, {235, 235, 255, 190}},
    {ParticleEffectId::PoisonAura, 4, {94, 218, 88, 120}, {180, 72, 220, 105}, 22.0f, 14.0f, Pi * 2.0f, 1.9f, 0.6f, 0.52f, 0.12f, {0.0f, -28.0f}, 1.0f, false, false},
    {ParticleEffectId::SlowAura, 4, {98, 176, 255, 130}, {220, 248, 255, 105}, 18.0f, 12.0f, Pi * 2.0f, 1.8f, 0.6f, 0.56f, 0.12f, {0.0f, -8.0f}, 1.2f, false, false},
    {ParticleEffectId::BleedAura, 9, {190, 34, 54, 135}, {255, 76, 86, 110}, 52.0f, 22.0f, Pi * 2.0f, 1.9f, 0.6f, 0.68f, 0.12f, {0.0f, 42.0f}, 1.15f, false, false},
    {ParticleEffectId::FrozenSparkle, 6, {140, 232, 255, 210}, {255, 255, 255, 185}, 26.0f, 16.0f, Pi * 2.0f, 3.8f, 1.2f, 0.64f, 0.14f, {0.0f, -14.0f}, 0.85f, false, false, 4.0f, 9.0f, {170, 236, 255, 180}, ParticleVisual::Sparkle},
    {ParticleEffectId::SpecialItemGlimmer, 3, {180, 224, 255, 125}, {255, 224, 128, 115}, 16.0f, 10.0f, Pi * 2.0f, 1.7f, 0.5f, 0.44f, 0.10f, {0.0f, -18.0f}, 1.2f, false, false},
    {ParticleEffectId::WarpCircle, 18, {112, 208, 255, 190}, {255, 224, 112, 170}, 62.0f, 24.0f, Pi * 2.0f, 2.4f, 0.8f, 0.72f, 0.18f, {0.0f, -24.0f}, 1.5f, false, true, 18.0f, 64.0f, {126, 208, 255, 150}},
    {ParticleEffectId::BossCircle, 24, {255, 96, 120, 210}, {255, 210, 96, 190}, 74.0f, 28.0f, Pi * 2.0f, 2.7f, 0.9f, 0.82f, 0.22f, {0.0f, -18.0f}, 1.4f, false, true, 24.0f, 90.0f, {255, 176, 84, 180}},
    {ParticleEffectId::ItemBreak, 14, {226, 220, 198, 232}, {118, 128, 148, 210}, 104.0f, 42.0f, Pi * 2.0f, 4.0f, 1.2f, 0.54f, 0.10f, {0.0f, 220.0f}, 1.7f, false, true, 8.0f, 30.0f, {255, 226, 142, 185}, ParticleVisual::RockShard},
    {ParticleEffectId::WoodBreak, 16, {164, 112, 62, 232}, {92, 58, 34, 205}, 112.0f, 48.0f, Pi * 2.0f, 7.2f, 2.1f, 0.58f, 0.10f, {0.0f, 260.0f}, 1.8f, false, true, 8.0f, 30.0f, {212, 154, 84, 170}, ParticleVisual::RockShard},
    {ParticleEffectId::CeramicBreak, 18, {238, 232, 218, 236}, {154, 146, 132, 210}, 128.0f, 54.0f, Pi * 2.0f, 6.2f, 2.0f, 0.62f, 0.10f, {0.0f, 280.0f}, 1.65f, false, true, 9.0f, 34.0f, {248, 238, 212, 170}, ParticleVisual::RockShard},
    {ParticleEffectId::GlassBreak, 20, {198, 238, 255, 222}, {236, 250, 255, 190}, 148.0f, 66.0f, Pi * 2.0f, 5.2f, 1.8f, 0.50f, 0.08f, {0.0f, 220.0f}, 1.95f, false, true, 10.0f, 38.0f, {170, 228, 255, 185}, ParticleVisual::RockShard},
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
    if (effect.type == EffectType::LevelUpPulseRing) {
        constexpr float FadeStart = 60.0f / 75.0f;
        const float fade = clamp((t - FadeStart) / (1.0f - FadeStart), 0.0f, 1.0f);
        const float easedFade = fade * fade * (3.0f - 2.0f * fade);
        return static_cast<unsigned char>(static_cast<float>(color.a) * (1.0f - easedFade));
    }

    if (effect.visual == ParticleVisual::LevelUpTwinkle) {
        constexpr float FadeStart = 0.68f;
        const float fade = clamp((t - FadeStart) / (1.0f - FadeStart), 0.0f, 1.0f);
        const float easedFade = fade * fade * (3.0f - 2.0f * fade);
        return static_cast<unsigned char>(static_cast<float>(color.a) * (1.0f - easedFade));
    }

    if (effect.visual == ParticleVisual::PoisonBubble) {
        constexpr float PopStart = 0.82f;
        if (t < PopStart) {
            return color.a;
        }
        const float pop = clamp((t - PopStart) / (1.0f - PopStart), 0.0f, 1.0f);
        return static_cast<unsigned char>(static_cast<float>(color.a) * (1.0f - pop));
    }

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

std::string_view particleEffectPreviewId(ParticleEffectId id)
{
    switch (id) {
    case ParticleEffectId::DigDust: return "dig_dust";
    case ParticleEffectId::DirtBreak: return "dirt_break";
    case ParticleEffectId::RockBreak: return "rock_break";
    case ParticleEffectId::OreBreak: return "ore_break";
    case ParticleEffectId::EnemyHit: return "enemy_hit";
    case ParticleEffectId::EnemyPoisonHit: return "enemy_poison_hit";
    case ParticleEffectId::EnemyBleedHit: return "enemy_bleed_hit";
    case ParticleEffectId::EnemySleepHit: return "enemy_sleep_hit";
    case ParticleEffectId::EnemyConfuseHit: return "enemy_confuse_hit";
    case ParticleEffectId::EnemyBlindHit: return "enemy_blind_hit";
    case ParticleEffectId::EnemyFireHit: return "enemy_fire_hit";
    case ParticleEffectId::EnemyWaterHit: return "enemy_water_hit";
    case ParticleEffectId::EnemyIceHit: return "enemy_ice_hit";
    case ParticleEffectId::EnemyThunderHit: return "enemy_thunder_hit";
    case ParticleEffectId::EnemyWindHit: return "enemy_wind_hit";
    case ParticleEffectId::EnemyEarthHit: return "enemy_earth_hit";
    case ParticleEffectId::EnemyDeathSoul: return "enemy_death_soul";
    case ParticleEffectId::CaptureSuccess: return "capture_success";
    case ParticleEffectId::DropPickup: return "drop_pickup";
    case ParticleEffectId::TorchFlicker: return "torch_flicker";
    case ParticleEffectId::MagicFire: return "magic_fire";
    case ParticleEffectId::MagicIce: return "magic_ice";
    case ParticleEffectId::MagicThunder: return "magic_thunder";
    case ParticleEffectId::MagicWind: return "magic_wind";
    case ParticleEffectId::MagicEarth: return "magic_earth";
    case ParticleEffectId::MagicDefault: return "magic_default";
    case ParticleEffectId::PoisonAura: return "poison_aura";
    case ParticleEffectId::SlowAura: return "slow_aura";
    case ParticleEffectId::BleedAura: return "bleed_aura";
    case ParticleEffectId::FrozenSparkle: return "frozen_sparkle";
    case ParticleEffectId::SpecialItemGlimmer: return "special_item_glimmer";
    case ParticleEffectId::WarpCircle: return "warp_circle";
    case ParticleEffectId::BossCircle: return "boss_circle";
    case ParticleEffectId::ItemBreak: return "item_break";
    case ParticleEffectId::WoodBreak: return "wood_break";
    case ParticleEffectId::CeramicBreak: return "ceramic_break";
    case ParticleEffectId::GlassBreak: return "glass_break";
    }
    return "particle_unknown";
}

std::string_view particleEffectPreviewLabel(ParticleEffectId id)
{
    switch (id) {
    case ParticleEffectId::DigDust: return "掘削ヒット粉じん";
    case ParticleEffectId::DirtBreak: return "土壁破壊";
    case ParticleEffectId::RockBreak: return "岩壁破壊";
    case ParticleEffectId::OreBreak: return "鉱石破壊";
    case ParticleEffectId::EnemyHit: return "敵ヒット";
    case ParticleEffectId::EnemyPoisonHit: return "敵ヒット: 毒";
    case ParticleEffectId::EnemyBleedHit: return "敵ヒット: 出血";
    case ParticleEffectId::EnemySleepHit: return "敵ヒット: 睡眠";
    case ParticleEffectId::EnemyConfuseHit: return "敵ヒット: 混乱";
    case ParticleEffectId::EnemyBlindHit: return "敵ヒット: 盲目";
    case ParticleEffectId::EnemyFireHit: return "敵ヒット: 火";
    case ParticleEffectId::EnemyWaterHit: return "敵ヒット: 水";
    case ParticleEffectId::EnemyIceHit: return "敵ヒット: 氷";
    case ParticleEffectId::EnemyThunderHit: return "敵ヒット: 雷";
    case ParticleEffectId::EnemyWindHit: return "敵ヒット: 風";
    case ParticleEffectId::EnemyEarthHit: return "敵ヒット: 土";
    case ParticleEffectId::EnemyDeathSoul: return "敵死亡ソウル";
    case ParticleEffectId::CaptureSuccess: return "捕獲成功";
    case ParticleEffectId::DropPickup: return "ドロップ取得";
    case ParticleEffectId::TorchFlicker: return "たいまつ火花";
    case ParticleEffectId::MagicFire: return "魔法粒子: 火";
    case ParticleEffectId::MagicIce: return "魔法粒子: 氷";
    case ParticleEffectId::MagicThunder: return "魔法粒子: 雷";
    case ParticleEffectId::MagicWind: return "魔法粒子: 風";
    case ParticleEffectId::MagicEarth: return "魔法粒子: 土";
    case ParticleEffectId::MagicDefault: return "魔法粒子: 汎用";
    case ParticleEffectId::PoisonAura: return "毒オーラ";
    case ParticleEffectId::SlowAura: return "鈍足オーラ";
    case ParticleEffectId::BleedAura: return "出血オーラ";
    case ParticleEffectId::FrozenSparkle: return "氷結きらめき";
    case ParticleEffectId::SpecialItemGlimmer: return "特殊アイテム光";
    case ParticleEffectId::WarpCircle: return "ワープ円";
    case ParticleEffectId::BossCircle: return "ボス円";
    case ParticleEffectId::ItemBreak: return "アイテム破壊";
    case ParticleEffectId::WoodBreak: return "木製破壊";
    case ParticleEffectId::CeramicBreak: return "陶器破壊";
    case ParticleEffectId::GlassBreak: return "ガラス破壊";
    }
    return "粒子プリセット";
}

EffectPreviewTarget particleEffectPreviewTarget(ParticleEffectId id)
{
    switch (id) {
    case ParticleEffectId::DigDust:
    case ParticleEffectId::DirtBreak:
    case ParticleEffectId::RockBreak:
    case ParticleEffectId::OreBreak:
    case ParticleEffectId::ItemBreak:
    case ParticleEffectId::WoodBreak:
    case ParticleEffectId::CeramicBreak:
    case ParticleEffectId::GlassBreak:
        return EffectPreviewTarget::WallTile;
    case ParticleEffectId::EnemyHit:
    case ParticleEffectId::EnemyPoisonHit:
    case ParticleEffectId::EnemyBleedHit:
    case ParticleEffectId::EnemySleepHit:
    case ParticleEffectId::EnemyConfuseHit:
    case ParticleEffectId::EnemyBlindHit:
    case ParticleEffectId::EnemyFireHit:
    case ParticleEffectId::EnemyWaterHit:
    case ParticleEffectId::EnemyIceHit:
    case ParticleEffectId::EnemyThunderHit:
    case ParticleEffectId::EnemyWindHit:
    case ParticleEffectId::EnemyEarthHit:
    case ParticleEffectId::EnemyDeathSoul:
    case ParticleEffectId::PoisonAura:
    case ParticleEffectId::SlowAura:
    case ParticleEffectId::BleedAura:
    case ParticleEffectId::FrozenSparkle:
        return EffectPreviewTarget::EnemySlime;
    default:
        break;
    }
    return EffectPreviewTarget::Player;
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

std::string_view tileBreakSoundFor(TileType tileType)
{
    if (tileType == TileType::Ore) {
        return AudioSeDigOreBreak;
    }
    return AudioSeDigBreak;
}

void delayEffect(Effect* effect, float delay)
{
    if (effect != nullptr) {
        effect->age = -std::max(0.0f, delay);
    }
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

unsigned char scaledAlpha(unsigned char alpha, float scale)
{
    return static_cast<unsigned char>(
        std::clamp(std::lround(static_cast<float>(alpha) * clamp(scale, 0.0f, 4.0f)), 0L, 255L));
}

Color withAlphaScale(Color color, float scale)
{
    color.a = scaledAlpha(color.a, scale);
    return color;
}

Color colorTowardWhite(Color color, float amount, float alphaScale)
{
    return {
        colorTowardWhite(color.r, amount),
        colorTowardWhite(color.g, amount),
        colorTowardWhite(color.b, amount),
        scaledAlpha(color.a, alphaScale),
    };
}

bool isEnemyHitBurstEffect(ParticleEffectId id)
{
    switch (id) {
    case ParticleEffectId::EnemyHit:
    case ParticleEffectId::EnemyPoisonHit:
    case ParticleEffectId::EnemyBleedHit:
    case ParticleEffectId::EnemySleepHit:
    case ParticleEffectId::EnemyConfuseHit:
    case ParticleEffectId::EnemyBlindHit:
    case ParticleEffectId::EnemyFireHit:
    case ParticleEffectId::EnemyWaterHit:
    case ParticleEffectId::EnemyIceHit:
    case ParticleEffectId::EnemyThunderHit:
    case ParticleEffectId::EnemyWindHit:
    case ParticleEffectId::EnemyEarthHit:
        return true;
    default:
        break;
    }
    return false;
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
    if (effect.visual == ParticleVisual::PoisonBubble) {
        const float wobble = std::sin(effect.age * effect.angularVelocity + effect.rotation) * effect.shardAspect;
        return effect.position + Vec2{wobble, 0.0f};
    }
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

void renderSparkle(Renderer& renderer, const Effect& effect, Vec2 center, Color color, float radius)
{
    if (radius <= 0.0f || color.a == 0) {
        return;
    }

    const float pulse = 0.72f + 0.28f * std::sin(effect.age * 18.0f + effect.rotation);
    const float longRadius = radius * (1.15f + pulse * 0.55f);
    const float shortRadius = std::max(0.8f, radius * 0.32f);
    const float rotation = effect.rotation;
    std::array<Vec2, 8> points{};
    for (std::size_t i = 0; i < points.size(); ++i) {
        const float pointRadius = i % 2 == 0 ? longRadius : shortRadius;
        const float angle = rotation + static_cast<float>(i) * Pi * 0.25f;
        points[i] = snapPoint(center + fromAngle(angle) * pointRadius);
    }

    renderer.fillSoftCircle(center, longRadius * 0.65f, {color.r, color.g, color.b, static_cast<unsigned char>(color.a / 3)});
    renderer.fillPolygon(points.data(), points.size(), color);
    renderer.drawLine(center - fromAngle(rotation) * longRadius, center + fromAngle(rotation) * longRadius, {255, 255, 255, color.a});
    renderer.drawLine(
        center - fromAngle(rotation + Pi * 0.5f) * (longRadius * 0.72f),
        center + fromAngle(rotation + Pi * 0.5f) * (longRadius * 0.72f),
        {220, 250, 255, static_cast<unsigned char>(color.a * 3 / 4)});
}

void renderLevelUpTwinkle(Renderer& renderer, Vec2 center, Color color, float radius)
{
    if (radius <= 0.0f || color.a == 0) {
        return;
    }

    const float verticalRadius = radius * 1.35f;
    const float horizontalRadius = radius * 0.82f;
    const Color glow{color.r, color.g, color.b, static_cast<unsigned char>(color.a * 2 / 5)};
    const Color core{255, 255, 244, color.a};
    renderer.drawSoftLine(
        center - Vec2{0.0f, verticalRadius},
        center + Vec2{0.0f, verticalRadius},
        std::max(1.8f, radius * 0.48f),
        glow);
    renderer.drawSoftLine(
        center - Vec2{horizontalRadius, 0.0f},
        center + Vec2{horizontalRadius, 0.0f},
        std::max(1.8f, radius * 0.48f),
        glow);
    renderer.drawSoftLine(
        center - Vec2{0.0f, verticalRadius},
        center + Vec2{0.0f, verticalRadius},
        std::max(1.0f, radius * 0.18f),
        core);
    renderer.drawSoftLine(
        center - Vec2{horizontalRadius, 0.0f},
        center + Vec2{horizontalRadius, 0.0f},
        std::max(1.0f, radius * 0.18f),
        core);
}

void renderPoisonBubble(Renderer& renderer, const Effect& effect, Vec2 center, Color color, float radius, float t)
{
    if (radius <= 0.0f || color.a == 0) {
        return;
    }

    constexpr float PopStart = 0.82f;
    if (t < PopStart) {
        const Color fill = color;
        const Color rim = colorTowardWhite(color, 0.42f, 1.0f);
        const Color highlight{236, 255, 214, scaledAlpha(color.a, 0.82f)};
        renderer.fillSoftCircle(center, radius * 0.96f, fill);
        renderer.drawCircle(center, radius, rim);
        renderer.fillCircle(center + Vec2{-radius * 0.28f, -radius * 0.30f}, std::max(0.75f, radius * 0.22f), highlight);
        return;
    }

    const float pop = clamp((t - PopStart) / (1.0f - PopStart), 0.0f, 1.0f);
    const float popRadius = radius * lerp(1.0f, 2.35f, smooth01(pop));
    const Color popColor = colorTowardWhite(color, 0.62f, 1.0f - pop);
    renderer.drawCircle(center, popRadius, popColor);
    for (int i = 0; i < 5; ++i) {
        const float angle = effect.rotation + static_cast<float>(i) * Pi * 0.4f;
        const Vec2 direction = fromAngle(angle);
        renderer.drawLine(
            center + direction * (popRadius * 0.70f),
            center + direction * (popRadius * 1.12f),
            popColor);
    }
}

void renderWaterDrop(Renderer& renderer, const Effect& effect, Vec2 center, Color color, float radius)
{
    if (radius <= 0.0f || color.a == 0) {
        return;
    }

    const float stretch = clamp(effect.shardAspect, 0.85f, 1.75f);
    const Vec2 tip = center + Vec2{0.0f, -radius * 1.38f * stretch};
    const Vec2 lower = center + Vec2{0.0f, radius * 0.30f * stretch};
    const std::array<Vec2, 3> body{{
        tip,
        center + Vec2{-radius * 0.62f, radius * 0.16f * stretch},
        center + Vec2{radius * 0.62f, radius * 0.16f * stretch},
    }};
    renderer.fillPolygon(body.data(), body.size(), color);
    renderer.fillCircle(lower, radius * 0.72f, color);
    renderer.fillSoftCircle(lower, radius * 1.12f, withAlphaScale(color, 0.32f));
    renderer.fillCircle(
        center + Vec2{-radius * 0.22f, -radius * 0.10f * stretch},
        std::max(0.6f, radius * 0.18f),
        colorTowardWhite(color, 0.70f, 0.82f));
}

void renderThunderArc(Renderer& renderer, const Effect& effect, Vec2 center, Color color, float radius)
{
    if (radius <= 0.0f || color.a == 0) {
        return;
    }

    const Vec2 forward = lengthSquared(effect.velocity) > 0.0001f ? normalize(effect.velocity) : fromAngle(effect.rotation);
    const Vec2 side{-forward.y, forward.x};
    const int pointCount = 4 + std::abs(effect.shardVariant) % 3;
    const float visualLength = std::max(14.0f, radius * clamp(effect.shardAspect, 5.0f, 14.0f));
    const int frame = static_cast<int>(std::floor(effect.age * 18.0f + effect.rotation * 3.0f + static_cast<float>(effect.shardVariant) * 0.37f));
    std::array<Vec2, 8> points{};

    for (int i = 0; i < pointCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(pointCount - 1);
        const float centerWeight = lerp(0.52f, 1.0f, std::sin(t * Pi));
        const float seed = static_cast<float>(frame * 173 + i * 251 + effect.shardVariant * 41);
        const float alternating = i % 2 == 0 ? -1.0f : 1.0f;
        const float along =
            (t - 0.5f) * visualLength +
            (std::sin(seed * 0.019f) * visualLength * 0.10f * centerWeight);
        const float kink =
            alternating * visualLength * lerp(0.12f, 0.28f, std::sin(seed * 0.013f) * 0.5f + 0.5f) * centerWeight +
            std::cos(seed * 0.017f) * visualLength * 0.10f * centerWeight;
        points[static_cast<std::size_t>(i)] = snapPoint(center + forward * along + side * kink);
    }

    const float pulse = 0.72f + 0.28f * (static_cast<float>(std::abs(frame) % 4) / 3.0f);
    const Color glow = withAlphaScale(color, 0.34f * pulse);
    const Color line = withAlphaScale(color, 0.86f * pulse);
    const Color core = colorTowardWhite(color, 0.78f, 0.98f * pulse);
    for (int i = 1; i < pointCount; ++i) {
        renderer.drawSoftLine(points[static_cast<std::size_t>(i - 1)], points[static_cast<std::size_t>(i)], std::max(1.4f, radius * 1.08f), glow);
        renderer.drawSoftLine(points[static_cast<std::size_t>(i - 1)], points[static_cast<std::size_t>(i)], std::max(0.9f, radius * 0.44f), line);
        renderer.drawLine(points[static_cast<std::size_t>(i - 1)], points[static_cast<std::size_t>(i)], core);
    }
}

void fillJaggedStarFan(
    Renderer& renderer,
    Vec2 center,
    float rotation,
    float outerRadius,
    float innerRadius,
    int points,
    float phase,
    Color color)
{
    if (outerRadius <= 0.0f || innerRadius <= 0.0f || color.a == 0) {
        return;
    }

    constexpr int MaxSegments = 32;
    const int segments = std::clamp(points * 2, 6, MaxSegments);
    std::array<Vec2, MaxSegments + 1> vertices{};
    std::array<int, MaxSegments * 3> indices{};
    vertices[0] = snapPoint(center);
    for (int i = 0; i < segments; ++i) {
        const bool outer = i % 2 == 0;
        const float angle = rotation + static_cast<float>(i) * Pi * 2.0f / static_cast<float>(segments);
        const float wobble = 1.0f + 0.08f * std::sin(phase + static_cast<float>(i) * 1.731f);
        const float radius = outer
            ? outerRadius * wobble
            : innerRadius * (1.0f - (wobble - 1.0f) * 0.45f);
        vertices[static_cast<std::size_t>(i + 1)] = snapPoint(center + fromAngle(angle) * radius);
    }

    int cursor = 0;
    for (int i = 0; i < segments; ++i) {
        indices[static_cast<std::size_t>(cursor++)] = 0;
        indices[static_cast<std::size_t>(cursor++)] = i + 1;
        indices[static_cast<std::size_t>(cursor++)] = ((i + 1) % segments) + 1;
    }
    renderer.fillTriangleList(
        vertices.data(),
        static_cast<std::size_t>(segments + 1),
        indices.data(),
        static_cast<std::size_t>(segments * 3),
        color);
}

void renderTaperedSpike(Renderer& renderer, Vec2 center, float angle, float length, float halfWidth, float backInset, Color color)
{
    if (length <= 0.0f || halfWidth <= 0.0f || color.a == 0) {
        return;
    }

    const Vec2 direction = fromAngle(angle);
    const Vec2 normal{-direction.y, direction.x};
    const std::array<Vec2, 3> points{{
        snapPoint(center + direction * -backInset + normal * halfWidth),
        snapPoint(center + direction * length),
        snapPoint(center + direction * -backInset + normal * -halfWidth),
    }};
    renderer.fillPolygon(points.data(), points.size(), color);
}

void renderImpactSpark(Renderer& renderer, const Effect& effect, Vec2 center, Color color, float radius)
{
    if (radius <= 0.0f || color.a == 0) {
        return;
    }

    const float stretch = clamp(effect.shardAspect, 0.70f, 1.85f);
    const float pulse = 0.86f + 0.14f * std::sin(effect.age * 30.0f + effect.rotation * 1.7f);
    const float glowRadius = radius * (1.28f + stretch * 0.22f) * pulse;
    const float coreRadius = std::max(0.8f, radius * (0.52f + stretch * 0.08f));
    const Color glow = withAlphaScale(color, 0.38f);
    const Color core = colorTowardWhite(color, 0.62f, 0.95f);
    const Color hot{255, 252, 210, scaledAlpha(color.a, 0.76f)};

    renderer.fillSoftCircle(center, glowRadius, glow);
    renderer.fillCircle(center, coreRadius, core);
    renderer.fillCircle(center, std::max(0.6f, coreRadius * 0.46f), hot);
}

void renderImpactBurst(Renderer& renderer, const Effect& effect, Vec2 center, Color color, float radius, float t)
{
    if (radius <= 0.0f || color.a == 0) {
        return;
    }

    const float u = clamp(t, 0.0f, 1.0f);
    const float attack = smooth01(clamp(u / 0.22f, 0.0f, 1.0f));
    const float retract = u < 0.46f ? 1.0f : lerp(1.0f, 0.14f, smooth01((u - 0.46f) / 0.54f));
    const float fade = std::pow(clamp((1.0f - u) / 0.96f, 0.0f, 1.0f), 0.58f);
    const float bloom = lerp(0.56f, 1.06f, attack) * (1.0f + 0.05f * std::sin(effect.age * 52.0f + effect.rotation));
    const float reach = bloom * retract;
    const float coreScale = bloom * lerp(1.0f, 0.56f, smooth01(clamp((u - 0.64f) / 0.36f, 0.0f, 1.0f)));
    const float phase = static_cast<float>(effect.shardVariant) * 0.713f + effect.rotation * 0.37f;
    const float spin = effect.rotation;

    const Color outer = withAlphaScale(color, 0.24f * fade);
    const Color glow = withAlphaScale(color, 0.42f * fade);
    const Color mid = withAlphaScale(color, 0.78f * fade);
    const Color hot = colorTowardWhite(color, 0.82f, 0.94f * fade);
    const Color white{255, 255, 236, scaledAlpha(color.a, 0.88f * fade)};

    for (int i = 0; i < 16; ++i) {
        const float rayPhase = phase + static_cast<float>(i) * 1.137f;
        const float angle = spin + static_cast<float>(i) * Pi / 8.0f +
            0.12f * std::sin(rayPhase) +
            0.035f * std::sin(rayPhase * 2.37f);
        const float lengthScale = 0.76f + 0.42f * (0.5f + 0.5f * std::sin(rayPhase * 1.37f));
        const Vec2 direction = fromAngle(angle);
        renderer.drawSoftLine(
            center + direction * (radius * 0.10f * coreScale),
            center + direction * (radius * (1.05f + lengthScale) * reach),
            std::max(1.0f, radius * 0.055f),
            outer);
    }

    for (int i = 0; i < 8; ++i) {
        const bool major = i % 2 == 0;
        const float spikePhase = phase + static_cast<float>(i) * 0.91f;
        const float angle = spin + static_cast<float>(i) * Pi * 0.25f +
            0.15f * std::sin(spikePhase * 1.43f) +
            0.04f * std::sin(spikePhase * 3.11f);
        const float length = radius * (major ? 1.96f : 1.34f) * reach * (0.96f + 0.08f * std::sin(spikePhase * 1.8f));
        const float width = radius * (major ? 0.23f : 0.16f) * coreScale;
        renderTaperedSpike(renderer, center, angle, length * 1.07f, width * 1.42f, radius * 0.10f, glow);
        renderTaperedSpike(renderer, center, angle, length * 0.88f, width * 0.62f, radius * 0.04f, hot);
    }

    fillJaggedStarFan(renderer, center, spin + 0.16f, radius * 1.02f * reach, radius * 0.18f * coreScale, 12, phase, mid);
    fillJaggedStarFan(renderer, center, spin - 0.05f, radius * 0.56f * reach, radius * 0.10f * coreScale, 10, phase + 2.1f, hot);
    fillJaggedStarFan(renderer, center, spin + 0.42f, radius * 0.30f * coreScale, radius * 0.07f * coreScale, 8, phase + 4.3f, white);

    constexpr int CoreRayCount = 5;
    for (int i = 0; i < CoreRayCount; ++i) {
        const float rayPhase = phase + 0.63f + static_cast<float>(i) * 1.417f;
        const float angle = spin + static_cast<float>(i) * Pi * 2.0f / static_cast<float>(CoreRayCount) +
            0.13f * std::sin(rayPhase) +
            0.04f * std::sin(rayPhase * 2.91f);
        const float rayLength = radius * (1.20f + 0.26f * std::sin(rayPhase * 1.61f)) * reach;
        const Vec2 direction = fromAngle(angle);
        renderer.drawSoftLine(
            center + direction * (radius * 0.08f),
            center + direction * rayLength,
            std::max(1.0f, radius * 0.07f),
            white);
    }
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
    if (effect.age < 0.0f) {
        return;
    }

    const float t = effect.duration > 0.0f ? effect.age / effect.duration : 1.0f;
    Color color = effect.color;
    color.a = effectAlpha(effect, color, t);
    const float radius =
        effect.type == EffectType::LevelUpPulseRing || effect.visual == ParticleVisual::LevelUpTwinkle
        ? lerp(effect.startRadius, effect.endRadius, smooth01(t))
        : lerp(effect.startRadius, effect.endRadius, t);
    const Vec2 drawPosition = effectDrawPosition(effect);
    if (effect.type == EffectType::LevelUpPulseRing) {
        renderer.drawAntialiasedRing(drawPosition, radius, 5.0f, color);
    } else if (effect.type == EffectType::Ring) {
        const float width = std::max(2.2f, radius * 0.075f);
        renderer.drawSoftRing(drawPosition, radius, width, color);
    } else if (effect.visual == ParticleVisual::RockShard) {
        renderRockShard(renderer, effect, drawPosition, color, std::max(1.0f, radius));
    } else if (effect.visual == ParticleVisual::Sparkle) {
        renderSparkle(renderer, effect, drawPosition, color, std::max(1.0f, radius));
    } else if (effect.visual == ParticleVisual::LevelUpTwinkle) {
        renderLevelUpTwinkle(renderer, drawPosition, color, std::max(0.0f, radius));
    } else if (effect.visual == ParticleVisual::ImpactSpark) {
        renderImpactSpark(renderer, effect, drawPosition, color, std::max(1.0f, radius));
    } else if (effect.visual == ParticleVisual::ImpactBurst) {
        renderImpactBurst(renderer, effect, drawPosition, color, std::max(1.0f, radius), t);
    } else if (effect.visual == ParticleVisual::PoisonBubble) {
        renderPoisonBubble(renderer, effect, drawPosition, color, std::max(1.0f, radius), t);
    } else if (effect.visual == ParticleVisual::WaterDrop) {
        renderWaterDrop(renderer, effect, drawPosition, color, std::max(1.0f, radius));
    } else if (effect.visual == ParticleVisual::ThunderArc) {
        renderThunderArc(renderer, effect, drawPosition, color, std::max(1.0f, radius));
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
        if (effect.age < 0.0f) {
            continue;
        }
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

    for (StatusTextPopup& popup : statusTextPopups_.items()) {
        if (!popup.active) {
            continue;
        }
        popup.age += dt;
        if (popup.age >= popup.duration) {
            popup.active = false;
            continue;
        }
    }

    for (LevelUpTextPopup& popup : levelUpTextPopups_.items()) {
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
        if (!effect.active || effect.age < 0.0f || !effect.physicsShard || !isDepthSortedEffect(effect)) {
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
        if (!effect.active || effect.age < 0.0f || !isDepthSortedEffect(effect)) {
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
        if (!effect.active || effect.age < 0.0f || effect.layer != layer) {
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
        const bool emphasized = popup.style == DamagePopupStyle::Critical ||
            popup.style == DamagePopupStyle::WeakPoint;
        const bool guarded = popup.style == DamagePopupStyle::Guard;
        const int textScale = guarded
            ? 3
            : emphasized
            ? (t < 0.14f ? 5 : 4)
            : (t < 0.10f ? 4 : 3);
        const Vec2 size = renderer.measureText(buffer, textScale, TextStyle::Italic);
        float hopHeight = 0.0f;
        const float primaryHopHeight = guarded ? 20.0f : (emphasized ? 42.0f : 34.0f);
        const float secondaryHopHeight = guarded ? 7.0f : (emphasized ? 17.0f : 13.0f);
        if (t < 0.58f) {
            const float u = t / 0.58f;
            hopHeight = std::sin(u * Pi) * primaryHopHeight;
        } else {
            const float u = (t - 0.58f) / 0.42f;
            hopHeight = std::sin(u * Pi) * secondaryHopHeight;
        }
        const Vec2 center = popup.position + popup.velocity * popup.age - Vec2{0.0f, hopHeight};
        const Vec2 pos = center - Vec2{size.x * 0.5f, size.y * 0.5f};
        Color textColor{255, 255, 255, alpha};
        if (popup.style == DamagePopupStyle::Player) {
            textColor = {255, 72, 64, alpha};
        } else if (popup.style == DamagePopupStyle::Heal) {
            textColor = {72, 238, 132, alpha};
        } else if (popup.style == DamagePopupStyle::Guard) {
            textColor = {150, 202, 232, alpha};
        } else if (popup.style == DamagePopupStyle::Critical) {
            textColor = mixColor({255, 42, 36, alpha}, {255, 255, 255, alpha}, clamp(t / 0.26f, 0.0f, 1.0f));
        } else if (popup.style == DamagePopupStyle::WeakPoint) {
            textColor = mixColor({72, 210, 255, alpha}, {255, 255, 255, alpha}, clamp(t / 0.26f, 0.0f, 1.0f));
        }
        const Color shadowColor = guarded
            ? Color{24, 52, 72, static_cast<unsigned char>(std::clamp(std::lround(225.0f * fade), 0L, 255L))}
            : Color{0, 0, 0, static_cast<unsigned char>(std::clamp(std::lround(190.0f * fade), 0L, 255L))};

        if (guarded) {
            renderer.drawText(pos + Vec2{-1.0f, 0.0f}, buffer, shadowColor, textScale, TextStyle::Italic);
            renderer.drawText(pos + Vec2{1.0f, 0.0f}, buffer, shadowColor, textScale, TextStyle::Italic);
            renderer.drawText(pos + Vec2{0.0f, -1.0f}, buffer, shadowColor, textScale, TextStyle::Italic);
            renderer.drawText(pos + Vec2{0.0f, 1.0f}, buffer, shadowColor, textScale, TextStyle::Italic);
        } else {
            renderer.drawText(pos + Vec2{2.0f, 2.0f}, buffer, shadowColor, textScale, TextStyle::Italic);
        }
        renderer.drawText(pos, buffer, textColor, textScale, TextStyle::Italic);
    }

    for (const StatusTextPopup& popup : statusTextPopups_.items()) {
        if (!popup.active || popup.text.empty()) {
            continue;
        }

        const float t = popup.duration > 0.0f ? clamp(popup.age / popup.duration, 0.0f, 1.0f) : 1.0f;
        const float fade = t < 0.74f ? 1.0f : 1.0f - clamp((t - 0.74f) / 0.26f, 0.0f, 1.0f);
        const unsigned char alpha = static_cast<unsigned char>(std::clamp(std::lround(255.0f * fade), 0L, 255L));
        if (alpha == 0) {
            continue;
        }

        const bool playerTarget = popup.target == StatusPopupTarget::Player;
        const int textScale = playerTarget
            ? (t < 0.12f ? 4 : 3)
            : (t < 0.10f ? 3 : 2);
        const Vec2 size = renderer.measureText(popup.text, textScale, TextStyle::Italic);
        const float hopHeight = std::sin(clamp(t / 0.62f, 0.0f, 1.0f) * Pi) * (playerTarget ? 24.0f : 18.0f);
        const float driftY = playerTarget ? -10.0f * t : -7.0f * t;
        const Vec2 center = popup.position + popup.velocity * popup.age + Vec2{0.0f, driftY - hopHeight};
        const Vec2 pos = center - Vec2{size.x * 0.5f, size.y * 0.5f};
        const Color textColor{
            popup.color.r,
            popup.color.g,
            popup.color.b,
            alpha,
        };
        const Color outlineColor{
            24,
            10,
            12,
            static_cast<unsigned char>(std::clamp(std::lround(210.0f * fade), 0L, 255L)),
        };
        const Color glowColor{
            popup.color.r,
            popup.color.g,
            popup.color.b,
            static_cast<unsigned char>(std::clamp(std::lround((playerTarget ? 48.0f : 34.0f) * fade), 0L, 255L)),
        };

        renderer.fillSoftCircle(center, std::max(size.x, size.y) * 0.42f, glowColor);
        renderer.drawOutlinedText(pos, popup.text, textColor, outlineColor, playerTarget ? 2 : 1, textScale, TextStyle::Italic);
    }

    constexpr std::string_view LevelUpText = "Level UP!!";
    for (const LevelUpTextPopup& popup : levelUpTextPopups_.items()) {
        if (!popup.active) {
            continue;
        }

        const float t = popup.duration > 0.0f ? clamp(popup.age / popup.duration, 0.0f, 1.0f) : 1.0f;
        const float fadeIn = clamp(t / 0.12f, 0.0f, 1.0f);
        const float fadeOut = t < 0.74f ? 1.0f : 1.0f - clamp((t - 0.74f) / 0.26f, 0.0f, 1.0f);
        const float fade = fadeIn * fadeOut;
        const unsigned char alpha = static_cast<unsigned char>(std::clamp(std::lround(255.0f * fade), 0L, 255L));
        if (alpha == 0) {
            continue;
        }

        const int textScale = t < 0.16f ? 6 : 5;
        const Vec2 size = renderer.measureText(LevelUpText, textScale, TextStyle::Italic);
        const float lift = std::sin(clamp(t / 0.68f, 0.0f, 1.0f) * Pi) * 22.0f + t * 18.0f;
        const Vec2 center = popup.position + popup.velocity * popup.age - Vec2{0.0f, lift};
        const Vec2 pos = center - Vec2{size.x * 0.5f, size.y * 0.5f};
        const Color shadowColor{196, 118, 255, alpha};
        const Color textColor{255, 246, 178, alpha};
        renderer.drawText(pos + Vec2{0.0f, 3.0f}, LevelUpText, shadowColor, textScale, TextStyle::Italic);
        renderer.drawText(pos, LevelUpText, textColor, textScale, TextStyle::Italic);
    }
}

std::vector<EffectSoundEvent> EffectSystem::consumeSoundEvents()
{
    std::vector<EffectSoundEvent> events;
    events.swap(soundEvents_);
    return events;
}

void EffectSystem::queueSound(std::string_view cueId, Vec2 position, float volumeScale, float pitchScale)
{
    if (cueId.empty()) {
        return;
    }
    soundEvents_.push_back(EffectSoundEvent{
        .cueId = std::string(cueId),
        .position = position,
        .volumeScale = volumeScale,
        .pitchScale = pitchScale,
    });
}

void EffectSystem::spawnSmokeBurst(Vec2 position, SmokeBurstOptions options)
{
    if (lightweightMode_ && smokePuffs_.activeCount() >= LightweightMaxSmokePuffs) {
        return;
    }

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

    if (style == DamagePopupStyle::Critical || style == DamagePopupStyle::WeakPoint) {
        popup->position = position + Vec2{randomRange(-14.0f, 14.0f), randomRange(-42.0f, -32.0f)};
        popup->velocity = {randomRange(-12.0f, 12.0f), 0.0f};
        popup->duration = 1.05f + randomRange(-0.04f, 0.04f);
    } else if (style == DamagePopupStyle::Guard) {
        popup->position = position + Vec2{randomRange(-8.0f, 8.0f), randomRange(-29.0f, -23.0f)};
        popup->velocity = {randomRange(-4.0f, 4.0f), 0.0f};
        popup->duration = 0.78f + randomRange(-0.03f, 0.03f);
    } else {
        popup->position = position + Vec2{randomRange(-12.0f, 12.0f), randomRange(-34.0f, -26.0f)};
        popup->velocity = {randomRange(-10.0f, 10.0f), 0.0f};
        popup->duration = 0.92f + randomRange(-0.04f, 0.04f);
    }
    popup->amount = amount;
    popup->style = style;
}

void EffectSystem::spawnStatusPopup(Vec2 position, std::string_view stateId, StatusPopupTarget target)
{
    const std::string_view displayName = entityStatusDisplayName(stateId);
    if (displayName.empty()) {
        return;
    }

    StatusTextPopup* popup = statusTextPopups_.acquire();
    if (!popup) {
        return;
    }

    const bool playerTarget = target == StatusPopupTarget::Player;
    popup->position = position + Vec2{
        randomRange(playerTarget ? -8.0f : -10.0f, playerTarget ? 8.0f : 10.0f),
        randomRange(playerTarget ? -54.0f : -44.0f, playerTarget ? -46.0f : -36.0f),
    };
    popup->velocity = {randomRange(playerTarget ? -7.0f : -9.0f, playerTarget ? 7.0f : 9.0f), 0.0f};
    popup->duration = (playerTarget ? 1.02f : 0.86f) + randomRange(-0.04f, 0.04f);
    popup->text = std::string(displayName);
    popup->color = entityStatusPopupColor(stateId, target);
    popup->target = target;
}

void EffectSystem::spawnLevelUpPopup(Vec2 position)
{
    LevelUpTextPopup* popup = levelUpTextPopups_.acquire();
    if (!popup) {
        return;
    }

    popup->position = position + Vec2{randomRange(-3.0f, 3.0f), -78.0f + randomRange(-3.0f, 3.0f)};
    popup->velocity = {randomRange(-3.0f, 3.0f), 0.0f};
    popup->duration = 1.14f;
}

void EffectSystem::spawnLevelUpEffects(Vec2 position)
{
    constexpr float FramesPerSecond = 60.0f;
    constexpr float PulseDuration = 75.0f / FramesPerSecond;
    constexpr float SecondPulseDelay = 15.0f / FramesPerSecond;
    for (int pulseIndex = 0; pulseIndex < 2; ++pulseIndex) {
        Effect* pulse = effects_.acquire();
        if (pulse == nullptr) {
            break;
        }
        pulse->type = EffectType::LevelUpPulseRing;
        pulse->layer = EffectLayer::Foreground;
        pulse->position = position;
        pulse->color = {255, 232, 126, 210};
        pulse->duration = PulseDuration;
        pulse->startRadius = 8.0f;
        pulse->endRadius = 72.0f;
        pulse->age = pulseIndex == 0 ? 0.0f : -SecondPulseDelay;
    }

    constexpr int TwinkleCount = 20;
    for (int i = 0; i < TwinkleCount; ++i) {
        const float angle = randomRange(0.0f, Pi * 2.0f);
        const float distance = std::sqrt(randomRange(0.0f, 1.0f)) * 50.0f;
        const Color color = mixColor({255, 214, 76, 230}, {255, 255, 246, 255}, randomRange(0.0f, 1.0f));
        const float targetRadius = randomRange(3.4f, 7.2f);
        const float duration = randomRange(36.0f, 56.0f) / FramesPerSecond;
        Effect* twinkle = spawnParticle(
            position + fromAngle(angle) * distance,
            {},
            0.0f,
            color,
            duration,
            {},
            0.0f,
            EffectLayer::Foreground,
            ParticleVisual::LevelUpTwinkle);
        if (twinkle != nullptr) {
            twinkle->endRadius = targetRadius;
            delayEffect(twinkle, randomRange(0.0f, 45.0f) / FramesPerSecond);
        }
    }
}

void EffectSystem::spawnAttackImpactBurst(Vec2 position, SmokeBurstOptions options, bool playSound)
{
    spawnSmokeBurst(position, options);
    if (playSound) {
        queueSound(AudioSeAttackHit, position);
    }
}

void EffectSystem::spawnRing(Vec2 position, float startRadius, float endRadius, Color color, float duration, EffectLayer layer)
{
    if (lightweightMode_ && effects_.activeCount() >= LightweightMaxEffects) {
        return;
    }

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
    if (lightweightMode_ && effects_.activeCount() >= LightweightMaxEffects) {
        return nullptr;
    }

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

void EffectSystem::spawn(
    ParticleEffectId id,
    Vec2 position,
    Vec2 direction,
    float scale,
    EffectLayer layer,
    Color colorOverride,
    int countMultiplier)
{
    const float safeScale = std::max(0.1f, scale);
    const int safeCountMultiplier = std::clamp(countMultiplier, 1, 4);
    if (id == ParticleEffectId::PoisonAura) {
        for (int i = 0; i < 4; ++i) {
            Color color = mixColor({84, 232, 84, 255}, {168, 255, 112, 255}, randomRange(0.0f, 1.0f));
            color = applyColorOverride(color, colorOverride);
            Effect* bubble = spawnParticle(
                position + Vec2{randomRange(-17.0f, 17.0f), randomRange(-2.0f, 14.0f)} * safeScale,
                {randomRange(-3.0f, 3.0f) * safeScale, -randomRange(13.0f, 23.0f) * safeScale},
                randomRange(2.5f, 4.2f) * safeScale,
                color,
                randomRange(0.95f, 1.32f),
                {0.0f, -2.0f * safeScale},
                0.18f,
                layer,
                ParticleVisual::PoisonBubble,
                0,
                randomRange(0.0f, Pi * 2.0f),
                randomRange(4.3f, 6.8f),
                randomRange(2.5f, 5.6f) * safeScale);
            if (bubble != nullptr) {
                bubble->endRadius = bubble->startRadius * randomRange(1.05f, 1.24f);
            }
        }
        return;
    }
    if (id == ParticleEffectId::SlowAura) {
        for (int i = 0; i < 7; ++i) {
            Color color = mixColor({92, 176, 255, 180}, {226, 250, 255, 150}, randomRange(0.0f, 1.0f));
            color = applyColorOverride(color, colorOverride);
            Effect* drop = spawnParticle(
                position + Vec2{randomRange(-30.0f, 30.0f), randomRange(-24.0f, 8.0f)} * safeScale,
                {randomRange(-5.0f, 5.0f) * safeScale, randomRange(11.0f, 28.0f) * safeScale},
                randomRange(1.4f, 2.6f) * safeScale,
                color,
                randomRange(0.82f, 1.18f),
                {0.0f, 18.0f * safeScale},
                0.24f,
                layer);
            if (drop != nullptr) {
                drop->endRadius = drop->startRadius * randomRange(0.45f, 0.70f);
                delayEffect(drop, randomRange(0.0f, 0.36f));
            }
        }
        return;
    }

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
    if (isEnemyHitBurstEffect(id)) {
        Color burstColor = preset.ringColor.a != 0 ? preset.ringColor : preset.colorA;
        burstColor = applyColorOverride(burstColor, colorOverride);
        Effect* flash = spawnParticle(
            position,
            {},
            16.5f * std::max(0.1f, scale),
            burstColor,
            std::max(0.18f, preset.duration * 0.95f),
            {},
            0.0f,
            layer,
            ParticleVisual::ImpactBurst,
            randomInt(0, 127),
            seedAngle(position) + randomRange(-0.28f, 0.28f),
            0.0f,
            randomRange(0.92f, 1.12f));
        if (flash != nullptr) {
            flash->endRadius = flash->startRadius * 1.22f;
        }
    }

    const int particleCount = std::clamp(preset.count * safeCountMultiplier, 0, 128);
    for (int i = 0; i < particleCount; ++i) {
        const float angleBase = preset.directional ? baseAngle : seedAngle(position);
        const float angle = angleBase + randomRange(-preset.spread * 0.5f, preset.spread * 0.5f);
        const float speed = std::max(0.0f, preset.speed + randomRange(-preset.speedJitter, preset.speedJitter)) * scale;
        const float radius = std::max(0.6f, preset.radius + randomRange(-preset.radiusJitter, preset.radiusJitter)) * scale;
        const float duration = std::max(0.06f, preset.duration + randomRange(-preset.durationJitter, preset.durationJitter));
        const bool rockShard = preset.visual == ParticleVisual::RockShard;
        const bool impactSpark = preset.visual == ParticleVisual::ImpactSpark;
        const bool digHitShard = preset.id == ParticleEffectId::DigDust;
        const Vec2 shardScatterVelocity = rockShard
            ? Vec2{
                randomRange(digHitShard ? -22.0f : -42.0f, digHitShard ? 22.0f : 42.0f) * scale,
                randomRange(digHitShard ? -26.0f : -54.0f, digHitShard ? 26.0f : 54.0f) * scale}
            : Vec2{};
        const float offsetRange = rockShard ? (digHitShard ? 5.0f : 9.0f) : (impactSpark ? 2.5f : 4.0f);
        const Vec2 offset = fromAngle(angle) * randomRange(0.0f, offsetRange * scale);
        const int shardVariant = rockShard ? randomInt(0, static_cast<int>(RockShardShapes.size() - 1)) : 0;
        const float rotation = rockShard
            ? angle + randomRange(-Pi * 0.65f, Pi * 0.65f)
            : (impactSpark ? angle + randomRange(-Pi * 0.12f, Pi * 0.12f) : 0.0f);
        const float angularVelocity = rockShard ? randomRange(-8.0f, 8.0f) : 0.0f;
        const float shardAspect = rockShard ? randomRange(0.78f, 1.32f) : (impactSpark ? randomRange(0.78f, 1.62f) : 1.0f);
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

void EffectSystem::spawnDigHit(Vec2 position, Vec2 direction, Color colorOverride, bool playSound)
{
    spawn(ParticleEffectId::DigDust, position, normalize(direction) * -1.0f, 1.0f, EffectLayer::Foreground, colorOverride);
    if (playSound) {
        queueSound(AudioSeDigHit, position);
    }
}

void EffectSystem::spawnTileBreak(
    Vec2 position,
    TileType tileType,
    Color colorOverride,
    bool playSound,
    float scale,
    int debrisCountMultiplier)
{
    ParticleEffectId id = ParticleEffectId::DirtBreak;
    if (tileType == TileType::Rock || tileType == TileType::HardRock) {
        id = ParticleEffectId::RockBreak;
    } else if (tileType == TileType::Ore) {
        id = ParticleEffectId::OreBreak;
    }
    const float safeScale = std::max(0.1f, scale);
    SmokeBurstOptions smoke;
    smoke.spreadRadius *= safeScale;
    smoke.speed *= safeScale;
    spawnSmokeBurst(position, smoke);
    spawn(id, position, {1.0f, 0.0f}, safeScale, EffectLayer::Foreground, colorOverride, debrisCountMultiplier);
    if (playSound) {
        queueSound(tileBreakSoundFor(tileType), position);
    }
}

void EffectSystem::spawnCrateBreak(Vec2 position, Color colorOverride, bool playSound)
{
    spawnTileBreak(position, TileType::Dirt, colorOverride, false);
    if (playSound) {
        queueSound(AudioSeCrateBreak, position);
    }
}

void EffectSystem::spawnEnemyHit(Vec2 position, std::string_view effect, bool playSound)
{
    ParticleEffectId id = ParticleEffectId::EnemyHit;
    if (effect == "status_poison" || effect == "status_poison_chance") {
        id = ParticleEffectId::EnemyPoisonHit;
    } else if (effect == "status_bleed" || effect == "status_bleed_chance") {
        id = ParticleEffectId::EnemyBleedHit;
    } else if (effect == "status_sleep" || effect == "status_sleep_chance") {
        id = ParticleEffectId::EnemySleepHit;
    } else if (effect == "status_confuse" || effect == "status_confuse_chance") {
        id = ParticleEffectId::EnemyConfuseHit;
    } else if (effect == "status_blind") {
        id = ParticleEffectId::EnemyBlindHit;
    } else if (effect == "status_wet") {
        id = ParticleEffectId::EnemyWaterHit;
    } else if (effect == "status_hot") {
        id = ParticleEffectId::EnemyFireHit;
    } else if (effect == "status_frozen") {
        id = ParticleEffectId::EnemyIceHit;
    } else if (effect == "fire" || effect == "break_fire_burst" || effect == "flame_burst" || effect == "hot_air" || effect == "dry_wet_bonus_damage") {
        id = ParticleEffectId::EnemyFireHit;
    } else if (effect == "water" || effect == "water_spray") {
        id = ParticleEffectId::EnemyWaterHit;
    } else if (effect == "ice") {
        id = ParticleEffectId::EnemyIceHit;
    } else if (effect == "thunder" || effect == "status_paralyze" || effect == "status_paralyze_chance" || effect == "status_shocked" || effect == "shock_wet" || effect == "conduct_water_puddle") {
        id = ParticleEffectId::EnemyThunderHit;
    } else if (effect == "wind" || effect == "wind_push_light" || effect == "bounce_grounded") {
        id = ParticleEffectId::EnemyWindHit;
    } else if (effect == "earth" || effect == "fall_damage_synergy") {
        id = ParticleEffectId::EnemyEarthHit;
    }
    spawn(id, position);
    if (playSound) {
        queueSound(AudioSeAttackHit, position);
    }
}

void EffectSystem::spawnEnemyDeath(Vec2 position, bool playSound)
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
    if (playSound) {
        queueSound(AudioSeEnemyDefeat, position);
    }
}

void EffectSystem::spawnEnemyTransform(Vec2 position, bool playSound)
{
    SmokeBurstOptions options;
    options.count = 18;
    options.size = 22.0f;
    options.sizeJitter = 0.48f;
    options.spreadRadius = 13.0f;
    options.speed = 34.0f;
    options.riseSpeed = 26.0f;
    options.duration = 0.82f;
    options.durationJitter = 0.16f;
    options.colorA = {176, 38, 48, 178};
    options.colorB = {58, 16, 22, 164};
    options.layer = EffectLayer::Foreground;
    spawnSmokeBurst(position, options);
    if (playSound) {
        queueSound(AudioSeEnemyTransform, position);
    }
}

void EffectSystem::spawnThrowStart(Vec2 position, Vec2 direction)
{
    (void)position;
    (void)direction;
}

void EffectSystem::spawnReturn(Vec2 position)
{
    (void)position;
}

void EffectSystem::spawnRingTrail(Vec2 position, Vec2 direction)
{
    (void)position;
    (void)direction;
}

void EffectSystem::spawnForegroundRingTrail(Vec2 position, Vec2 direction)
{
    (void)position;
    (void)direction;
}

void EffectSystem::spawnCaptureSuccess(Vec2 position, Vec2 direction, bool playSound)
{
    spawn(ParticleEffectId::CaptureSuccess, position, direction);
    if (playSound) {
        queueSound(AudioSeCaptureSuccess, position);
    }
}

void EffectSystem::spawnDropPickup(Vec2 position, Vec2 direction, bool playSound)
{
    spawn(ParticleEffectId::DropPickup, position, direction);
    if (playSound) {
        queueSound(AudioSePickup, position);
    }
}

void EffectSystem::spawnItemBreak(Vec2 position, ItemBreakVisual visual, float scale, bool playSound)
{
    SmokeBurstOptions options;
    options.count = 7;
    options.size = 16.0f;
    options.spreadRadius = 7.0f;
    options.speed = 20.0f;
    options.riseSpeed = 12.0f;
    options.duration = 0.46f;
    options.colorA = {230, 226, 214, 150};
    options.colorB = {116, 122, 138, 135};
    options.layer = EffectLayer::Foreground;
    ParticleEffectId particleId = ParticleEffectId::ItemBreak;

    if (visual == ItemBreakVisual::Wood) {
        particleId = ParticleEffectId::WoodBreak;
        options.count = 8;
        options.colorA = {168, 118, 68, 130};
        options.colorB = {86, 58, 38, 115};
    } else if (visual == ItemBreakVisual::Ceramic) {
        particleId = ParticleEffectId::CeramicBreak;
        options.count = 8;
        options.colorA = {238, 232, 220, 140};
        options.colorB = {132, 126, 118, 120};
    } else if (visual == ItemBreakVisual::Glass) {
        particleId = ParticleEffectId::GlassBreak;
        options.count = 6;
        options.size = 13.0f;
        options.colorA = {220, 246, 255, 112};
        options.colorB = {150, 210, 238, 94};
    }

    const float visualScale = std::clamp(scale, 0.55f, 2.0f);
    options.size *= visualScale;
    options.spreadRadius *= visualScale;
    options.speed *= visualScale;
    spawnSmokeBurst(position, options);
    spawn(particleId, position, {1.0f, 0.0f}, visualScale, EffectLayer::Foreground);
    if (playSound) {
        queueSound(AudioSeItemBreak, position);
        if (visual == ItemBreakVisual::Wood) {
            queueSound(AudioSeCrateBreak, position, 0.78f);
        } else if (visual == ItemBreakVisual::Ceramic) {
            queueSound(AudioSeItemBreakCeramic, position, 0.86f);
        } else if (visual == ItemBreakVisual::Glass) {
            queueSound(AudioSeItemBreakGlass, position, 0.86f);
        }
    }
}

void EffectSystem::spawnBrokenItemSmoke(Vec2 position, float scale)
{
    const float visualScale = std::clamp(scale, 0.55f, 1.8f);
    const int count = lightweightMode_ ? 1 : 2;
    for (int i = 0; i < count; ++i) {
        Color color = mixColor(
            Color{12, 12, 16, 188},
            Color{54, 50, 60, 146},
            randomRange(0.0f, 1.0f));
        const Vec2 offset{
            randomRange(-7.0f, 7.0f) * visualScale,
            randomRange(-5.0f, 3.0f) * visualScale,
        };
        Effect* smoke = spawnParticle(
            position + offset,
            {
                randomRange(-8.0f, 8.0f) * visualScale,
                randomRange(-48.0f, -28.0f) * visualScale,
            },
            randomRange(1.8f, 3.4f) * visualScale,
            color,
            randomRange(0.62f, 0.88f),
            {0.0f, randomRange(-9.0f, -3.0f) * visualScale},
            randomRange(0.65f, 1.10f),
            EffectLayer::Foreground,
            ParticleVisual::Circle);
        if (smoke != nullptr) {
            smoke->endRadius = smoke->startRadius * randomRange(1.7f, 2.6f);
        }
    }
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
    } else if (stateId == "status_glued") {
        spawn(ParticleEffectId::SlowAura, position);
    } else if (stateId == "status_defense_down") {
        spawn(ParticleEffectId::SlowAura, position, {1.0f, 0.0f}, 1.0f, EffectLayer::World, {214, 134, 82, 255});
    } else if (stateId == "status_bleed") {
        spawn(ParticleEffectId::BleedAura, position);
    } else if (stateId == "status_paralyze") {
        for (int i = 0; i < 5; ++i) {
            const float angle = seedAngle(position) + randomRange(0.0f, Pi * 2.0f);
            const Vec2 direction = fromAngle(angle);
            const Color color = mixColor({255, 236, 92, 225}, {255, 255, 218, 196}, randomRange(0.0f, 1.0f));
            Effect* arc = spawnParticle(
                position + direction * randomRange(3.0f, 18.0f) + Vec2{randomRange(-4.0f, 4.0f), randomRange(-24.0f, 4.0f)},
                direction * randomRange(10.0f, 34.0f) + Vec2{randomRange(-9.0f, 9.0f), randomRange(-9.0f, 9.0f)},
                randomRange(1.7f, 2.8f),
                color,
                randomRange(0.24f, 0.42f),
                {},
                1.9f,
                EffectLayer::World,
                ParticleVisual::ThunderArc,
                randomInt(0, 127),
                angle,
                0.0f,
                randomRange(7.0f, 11.0f));
            if (arc != nullptr) {
                arc->endRadius = arc->startRadius * 0.42f;
                delayEffect(arc, randomRange(0.0f, 0.24f));
            }
        }
    } else if (stateId == "status_blind") {
        spawn(ParticleEffectId::SlowAura, position, {1.0f, 0.0f}, 1.0f, EffectLayer::World, {14, 14, 18, 255});
    } else if (stateId == "status_wet") {
        for (int i = 0; i < 6; ++i) {
            const Color color = mixColor({82, 184, 255, 220}, {196, 240, 255, 195}, randomRange(0.0f, 1.0f));
            Effect* drop = spawnParticle(
                position + Vec2{randomRange(-16.0f, 16.0f), randomRange(-32.0f, -8.0f)},
                {randomRange(-2.5f, 2.5f), randomRange(24.0f, 52.0f)},
                randomRange(1.2f, 2.3f),
                color,
                randomRange(0.48f, 0.78f),
                {0.0f, 42.0f},
                0.16f,
                EffectLayer::World,
                ParticleVisual::WaterDrop,
                0,
                0.0f,
                0.0f,
                randomRange(1.05f, 1.55f));
            if (drop != nullptr) {
                drop->endRadius = drop->startRadius * randomRange(0.42f, 0.62f);
                delayEffect(drop, randomRange(0.0f, 0.38f));
            }
        }
    } else if (stateId == "status_hot") {
        spawnRing(position, 7.0f, 24.0f, {255, 132, 58, 150}, 0.30f);
        for (int i = 0; i < 8; ++i) {
            const float angle = seedAngle(position) + randomRange(0.0f, Pi * 2.0f);
            const Vec2 direction = fromAngle(angle);
            const Color color = mixColor({255, 92, 36, 222}, {255, 222, 92, 184}, randomRange(0.0f, 1.0f));
            Effect* ember = spawnParticle(
                position + direction * randomRange(2.0f, 16.0f) + Vec2{randomRange(-5.0f, 5.0f), randomRange(-18.0f, 8.0f)},
                direction * randomRange(16.0f, 52.0f) + Vec2{randomRange(-8.0f, 8.0f), randomRange(-20.0f, 8.0f)},
                randomRange(1.7f, 3.2f),
                color,
                randomRange(0.42f, 0.70f),
                {0.0f, -12.0f},
                1.55f,
                EffectLayer::World);
            if (ember != nullptr) {
                ember->endRadius = ember->startRadius * randomRange(0.16f, 0.34f);
                delayEffect(ember, randomRange(0.0f, 0.30f));
            }
        }
    } else if (stateId == "status_frozen") {
        for (int i = 0; i < 9; ++i) {
            const float angle = seedAngle(position) + randomRange(0.0f, Pi * 2.0f);
            const float distance = std::sqrt(randomRange(0.0f, 1.0f)) * randomRange(10.0f, 34.0f);
            const Color color = mixColor({138, 232, 255, 216}, {255, 255, 255, 196}, randomRange(0.0f, 1.0f));
            Effect* sparkle = spawnParticle(
                position + fromAngle(angle) * distance + Vec2{randomRange(-5.0f, 5.0f), randomRange(-30.0f, 8.0f)},
                {randomRange(-10.0f, 10.0f), randomRange(-22.0f, 8.0f)},
                randomRange(2.6f, 4.8f),
                color,
                randomRange(0.74f, 1.12f),
                {0.0f, -6.0f},
                0.72f,
                EffectLayer::World,
                ParticleVisual::Sparkle,
                0,
                randomRange(0.0f, Pi * 2.0f),
                randomRange(-2.4f, 2.4f),
                1.0f);
            if (sparkle != nullptr) {
                sparkle->endRadius = sparkle->startRadius * randomRange(0.18f, 0.36f);
                delayEffect(sparkle, randomRange(0.0f, 0.46f));
            }
        }
    } else if (stateId == "status_shocked") {
        for (int i = 0; i < 4; ++i) {
            const float angle = seedAngle(position) + randomRange(0.0f, Pi * 2.0f);
            const Vec2 direction = fromAngle(angle);
            const Color color = mixColor({255, 236, 88, 232}, {255, 255, 226, 206}, randomRange(0.0f, 1.0f));
            Effect* arc = spawnParticle(
                position + direction * randomRange(3.0f, 16.0f) + Vec2{randomRange(-4.0f, 4.0f), randomRange(-22.0f, 5.0f)},
                direction * randomRange(8.0f, 26.0f) + Vec2{randomRange(-7.0f, 7.0f), randomRange(-7.0f, 7.0f)},
                randomRange(1.9f, 3.0f),
                color,
                randomRange(0.28f, 0.48f),
                {},
                1.65f,
                EffectLayer::World,
                ParticleVisual::ThunderArc,
                randomInt(0, 127),
                angle,
                0.0f,
                randomRange(8.5f, 13.0f));
            if (arc != nullptr) {
                arc->endRadius = arc->startRadius * 0.40f;
                delayEffect(arc, randomRange(0.0f, 0.22f));
            }
        }
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

void EffectSystem::spawnExplosion(Vec2 position, float radius, bool playSound)
{
    const float safeRadius = std::max(12.0f, radius) * 1.18f;
    const float scale = clamp(safeRadius / 48.0f, 0.55f, 2.0f);
    const int sparkCount = lightweightMode_ ? 16 : 30;
    const int emberCount = lightweightMode_ ? 10 : 22;
    const int shardCount = lightweightMode_ ? 8 : 16;
    if (playSound) {
        queueSound(AudioSeExplosion, position);
    }

    spawnRing(position, safeRadius * 0.08f, safeRadius * 1.08f, {255, 220, 116, 188}, 0.26f, EffectLayer::Foreground);
    spawnRing(position, safeRadius * 0.26f, safeRadius * 1.34f, {255, 104, 44, 112}, 0.42f, EffectLayer::World);
    spawnRing(position, safeRadius * 0.50f, safeRadius * 1.72f, {80, 52, 34, 86}, 0.58f, EffectLayer::World);

    Effect* flash = spawnParticle(
        position,
        {},
        safeRadius * 0.42f,
        {255, 120, 42, 235},
        0.34f,
        {},
        0.0f,
        EffectLayer::Foreground,
        ParticleVisual::ImpactBurst,
        randomInt(0, 127),
        seedAngle(position) + randomRange(-0.22f, 0.22f),
        0.0f,
        1.08f);
    if (flash != nullptr) {
        flash->endRadius = flash->startRadius * 1.18f;
    }

    Effect* core = spawnParticle(
        position + Vec2{0.0f, -2.0f * scale},
        {},
        safeRadius * 0.24f,
        {255, 246, 198, 210},
        0.18f,
        {},
        0.0f,
        EffectLayer::Foreground,
        ParticleVisual::ImpactSpark,
        0,
        seedAngle(position),
        0.0f,
        1.35f);
    if (core != nullptr) {
        core->endRadius = core->startRadius * 0.40f;
    }

    SmokeBurstOptions smoke;
    smoke.count = lightweightMode_ ? 12 : 24;
    smoke.size = 17.0f * scale;
    smoke.sizeJitter = 0.52f;
    smoke.spreadRadius = safeRadius * 0.22f;
    smoke.speed = 42.0f * scale;
    smoke.riseSpeed = 18.0f * scale;
    smoke.duration = 0.78f;
    smoke.durationJitter = 0.18f;
    smoke.colorA = {96, 66, 48, 168};
    smoke.colorB = {210, 116, 58, 126};
    smoke.layer = EffectLayer::Foreground;
    spawnSmokeBurst(position, smoke);

    for (int i = 0; i < sparkCount; ++i) {
        const float angle = seedAngle(position) + randomRange(0.0f, Pi * 2.0f);
        const Vec2 direction = fromAngle(angle);
        const float distance = randomRange(0.0f, safeRadius * 0.20f);
        Effect* spark = spawnParticle(
            position + direction * distance,
            direction * randomRange(110.0f, 245.0f) * scale + Vec2{randomRange(-18.0f, 18.0f), randomRange(-32.0f, 14.0f)} * scale,
            randomRange(1.8f, 3.8f) * scale,
            mixColor({255, 245, 188, 236}, {255, 86, 34, 212}, randomRange(0.0f, 1.0f)),
            randomRange(0.24f, 0.48f),
            {},
            2.2f,
            EffectLayer::Foreground,
            ParticleVisual::ImpactSpark,
            0,
            angle + randomRange(-0.18f, 0.18f),
            0.0f,
            randomRange(0.85f, 1.75f));
        if (spark != nullptr) {
            spark->endRadius = spark->startRadius * randomRange(0.12f, 0.30f);
            delayEffect(spark, randomRange(0.0f, 0.05f));
        }
    }

    for (int i = 0; i < emberCount; ++i) {
        const float angle = seedAngle(position) + randomRange(0.0f, Pi * 2.0f);
        const Vec2 direction = fromAngle(angle);
        Effect* ember = spawnParticle(
            position + direction * randomRange(4.0f, safeRadius * 0.42f),
            direction * randomRange(36.0f, 118.0f) * scale + Vec2{randomRange(-16.0f, 16.0f), -randomRange(8.0f, 54.0f)} * scale,
            randomRange(1.9f, 4.4f) * scale,
            mixColor({255, 118, 38, 220}, {255, 226, 102, 188}, randomRange(0.0f, 1.0f)),
            randomRange(0.48f, 0.88f),
            {0.0f, -18.0f * scale},
            1.1f,
            EffectLayer::Foreground,
            ParticleVisual::Circle,
            0,
            0.0f,
            0.0f,
            1.0f);
        if (ember != nullptr) {
            ember->endRadius = ember->startRadius * randomRange(0.08f, 0.24f);
            delayEffect(ember, randomRange(0.04f, 0.18f));
        }
    }

    for (int i = 0; i < shardCount; ++i) {
        const float angle = seedAngle(position) + randomRange(0.0f, Pi * 2.0f);
        const Vec2 direction = fromAngle(angle);
        Effect* shard = spawnParticle(
            position + direction * randomRange(3.0f, safeRadius * 0.24f),
            direction * randomRange(70.0f, 155.0f) * scale + Vec2{randomRange(-24.0f, 24.0f), randomRange(-42.0f, 18.0f)} * scale,
            randomRange(3.8f, 7.2f) * scale,
            mixColor({74, 54, 46, 230}, {184, 112, 58, 214}, randomRange(0.0f, 1.0f)),
            randomRange(0.72f, 1.06f),
            {},
            1.35f,
            EffectLayer::Foreground,
            ParticleVisual::RockShard,
            randomInt(0, static_cast<int>(RockShardShapes.size() - 1)),
            angle + randomRange(-Pi * 0.45f, Pi * 0.45f),
            randomRange(-9.0f, 9.0f),
            randomRange(0.80f, 1.36f));
        if (shard != nullptr) {
            configureShardPhysics(*shard, false, scale);
            shard->endRadius = shard->startRadius * randomRange(0.54f, 0.80f);
        }
    }
}

void EffectSystem::spawnMagicCast(Vec2 origin, Vec2 direction, std::string_view element, float power)
{
    const Vec2 forward = normalize(direction);
    const Vec2 impact = origin + forward * (44.0f + power * 1.5f);
    spawn(magicEffectFor(element), impact, forward, 1.0f + power * 0.01f);
}

std::span<const EffectPreviewEntry> effectSystemPreviewEntries()
{
    static const std::vector<EffectPreviewEntry> entries = [] {
        std::vector<EffectPreviewEntry> result;
        result.reserve(ParticlePresets.size() + 37);
        for (const ParticlePreset& preset : ParticlePresets) {
            result.push_back(EffectPreviewEntry{
                .id = particleEffectPreviewId(preset.id),
                .label = particleEffectPreviewLabel(preset.id),
                .group = "EffectSystem / 粒子プリセット",
                .source = EffectPreviewSource::EffectSystem,
                .target = particleEffectPreviewTarget(preset.id),
                .playback = EffectPreviewPlayback::BurstEvery20Frames,
                .action = EffectPreviewAction::ParticlePreset,
                .particleId = preset.id,
                .direction = {1.0f, 0.0f},
                .scale = 1.0f,
            });
        }

        static constexpr std::array<EffectPreviewEntry, 37> ActionEntries{{
            {.id = "dig_hit", .label = "掘削ヒット", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::WallTile, .action = EffectPreviewAction::DigHit, .direction = {-1.0f, 0.0f}},
            {.id = "tile_break", .label = "壁破壊", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::WallTile, .action = EffectPreviewAction::TileBreak},
            {.id = "crate_break", .label = "木箱破壊", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::WallTile, .action = EffectPreviewAction::CrateBreak},
            {.id = "enemy_hit_default", .label = "敵ヒット: 通常", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit},
            {.id = "enemy_hit_poison", .label = "敵ヒット: 毒", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "status_poison"},
            {.id = "enemy_hit_bleed", .label = "敵ヒット: 出血", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "status_bleed"},
            {.id = "enemy_hit_sleep", .label = "敵ヒット: 睡眠", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "status_sleep"},
            {.id = "enemy_hit_confuse", .label = "敵ヒット: 混乱", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "status_confuse"},
            {.id = "enemy_hit_blind", .label = "敵ヒット: 盲目", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "status_blind"},
            {.id = "enemy_hit_fire", .label = "敵ヒット: 火", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "fire"},
            {.id = "enemy_hit_water", .label = "敵ヒット: 水", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "water"},
            {.id = "enemy_hit_ice", .label = "敵ヒット: 氷", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "ice"},
            {.id = "enemy_hit_thunder", .label = "敵ヒット: 雷", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "thunder"},
            {.id = "enemy_hit_wind", .label = "敵ヒット: 風", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "wind"},
            {.id = "enemy_hit_earth", .label = "敵ヒット: 土", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyHit, .argument = "earth"},
            {.id = "enemy_death", .label = "敵死亡", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyDeath},
            {.id = "enemy_transform", .label = "敵変身煙", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::EnemyTransform},
            {.id = "api_capture_success", .label = "捕獲成功", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::CaptureSuccess, .direction = {0.0f, -1.0f}},
            {.id = "api_drop_pickup", .label = "ドロップ取得", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::Player, .action = EffectPreviewAction::DropPickup, .direction = {0.0f, -1.0f}},
            {.id = "item_break_generic", .label = "アイテム破壊: 汎用", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::WallTile, .action = EffectPreviewAction::ItemBreakGeneric},
            {.id = "item_break_wood", .label = "アイテム破壊: 木", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::WallTile, .action = EffectPreviewAction::ItemBreakWood},
            {.id = "item_break_ceramic", .label = "アイテム破壊: 陶器", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::WallTile, .action = EffectPreviewAction::ItemBreakCeramic},
            {.id = "item_break_glass", .label = "アイテム破壊: ガラス", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::WallTile, .action = EffectPreviewAction::ItemBreakGlass},
            {.id = "material_float", .label = "素材浮遊", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::Player, .action = EffectPreviewAction::MaterialFloat, .offset = {0.0f, -20.0f}},
            {.id = "torch_flicker", .label = "たいまつ火花", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::Player, .action = EffectPreviewAction::TorchFlicker, .offset = {0.0f, -38.0f}},
            {.id = "torch_flicker_foreground", .label = "たいまつ火花: 前景", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::Player, .action = EffectPreviewAction::ForegroundTorchFlicker, .offset = {0.0f, -38.0f}},
            {.id = "special_item_glimmer", .label = "特殊アイテム光", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::Player, .action = EffectPreviewAction::SpecialItemGlimmer, .offset = {0.0f, -28.0f}},
            {.id = "warp_circle", .label = "ワープ円", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::Player, .action = EffectPreviewAction::WarpCircle},
            {.id = "boss_circle", .label = "ボス円", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::BossCircle},
            {.id = "area_pulse", .label = "範囲パルス", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::Player, .action = EffectPreviewAction::AreaPulse, .radius = 70.0f},
            {.id = "explosion", .label = "爆発", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::WallTile, .action = EffectPreviewAction::Explosion, .radius = 52.0f},
            {.id = "magic_cast_fire", .label = "魔法詠唱: 火", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::MagicCast, .argument = "fire", .direction = {1.0f, 0.0f}},
            {.id = "magic_cast_ice", .label = "魔法詠唱: 氷", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::MagicCast, .argument = "ice", .direction = {1.0f, 0.0f}},
            {.id = "magic_cast_thunder", .label = "魔法詠唱: 雷", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::MagicCast, .argument = "thunder", .direction = {1.0f, 0.0f}},
            {.id = "magic_cast_wind", .label = "魔法詠唱: 風", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::MagicCast, .argument = "wind", .direction = {1.0f, 0.0f}},
            {.id = "magic_cast_earth", .label = "魔法詠唱: 土", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::MagicCast, .argument = "earth", .direction = {1.0f, 0.0f}},
            {.id = "magic_cast_default", .label = "魔法詠唱: 汎用", .group = "EffectSystem / 高水準API", .source = EffectPreviewSource::EffectSystem, .target = EffectPreviewTarget::EnemySlime, .action = EffectPreviewAction::MagicCast, .argument = "magic", .direction = {1.0f, 0.0f}},
        }};
        result.insert(result.end(), ActionEntries.begin(), ActionEntries.end());
        return result;
    }();
    return {entries.data(), entries.size()};
}

void playEffectSystemPreview(
    EffectSystem& effects,
    const EffectPreviewEntry& entry,
    Vec2 position,
    Vec2 direction,
    TileType wallTileType,
    Color wallTileColor)
{
    const Vec2 fallbackDirection = lengthSquared(direction) > 0.0001f ? normalize(direction) : Vec2{1.0f, 0.0f};
    const Vec2 entryDirection = lengthSquared(entry.direction) > 0.0001f ? normalize(entry.direction) : fallbackDirection;
    const Vec2 pos = position + entry.offset;

    switch (entry.action) {
    case EffectPreviewAction::ParticlePreset:
        effects.spawn(entry.particleId, pos, entryDirection, entry.scale);
        break;
    case EffectPreviewAction::DigHit:
        effects.spawnDigHit(pos, entryDirection, wallTileColor);
        break;
    case EffectPreviewAction::TileBreak:
        effects.spawnTileBreak(pos, wallTileType, wallTileColor);
        break;
    case EffectPreviewAction::CrateBreak:
        effects.spawnCrateBreak(pos, wallTileColor);
        break;
    case EffectPreviewAction::EnemyHit:
        effects.spawnEnemyHit(pos, entry.argument);
        break;
    case EffectPreviewAction::EnemyDeath:
        effects.spawnEnemyDeath(pos);
        break;
    case EffectPreviewAction::EnemyTransform:
        effects.spawnEnemyTransform(pos);
        break;
    case EffectPreviewAction::CaptureSuccess:
        effects.spawnCaptureSuccess(pos, entryDirection);
        break;
    case EffectPreviewAction::DropPickup:
        effects.spawnDropPickup(pos, entryDirection);
        break;
    case EffectPreviewAction::ItemBreakGeneric:
        effects.spawnItemBreak(pos, ItemBreakVisual::Generic, entry.scale);
        break;
    case EffectPreviewAction::ItemBreakWood:
        effects.spawnItemBreak(pos, ItemBreakVisual::Wood, entry.scale);
        break;
    case EffectPreviewAction::ItemBreakCeramic:
        effects.spawnItemBreak(pos, ItemBreakVisual::Ceramic, entry.scale);
        break;
    case EffectPreviewAction::ItemBreakGlass:
        effects.spawnItemBreak(pos, ItemBreakVisual::Glass, entry.scale);
        break;
    case EffectPreviewAction::MaterialFloat:
        effects.spawnMaterialFloat(pos, {150, 224, 255, 190});
        break;
    case EffectPreviewAction::TorchFlicker:
        effects.spawnTorchFlicker(pos);
        break;
    case EffectPreviewAction::ForegroundTorchFlicker:
        effects.spawnForegroundTorchFlicker(pos);
        break;
    case EffectPreviewAction::SpecialItemGlimmer:
        effects.spawnSpecialItemGlimmer(pos);
        break;
    case EffectPreviewAction::ForegroundSpecialItemGlimmer:
        effects.spawnForegroundSpecialItemGlimmer(pos);
        break;
    case EffectPreviewAction::WarpCircle:
        effects.spawnWarpCircle(pos, false);
        break;
    case EffectPreviewAction::BossCircle:
        effects.spawnWarpCircle(pos, true);
        break;
    case EffectPreviewAction::AreaPulse:
        effects.spawnAreaPulse(pos, entry.radius, {126, 208, 255, 190});
        break;
    case EffectPreviewAction::Explosion:
        effects.spawnExplosion(pos, entry.radius);
        break;
    case EffectPreviewAction::MagicCast:
        effects.spawnMagicCast(pos - entryDirection * 54.0f, entryDirection, entry.argument, 24.0f);
        break;
    }
}

}
