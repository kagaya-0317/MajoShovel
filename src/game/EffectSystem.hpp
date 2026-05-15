#pragma once

#include "data/GameBalance.hpp"
#include "engine/Math.hpp"
#include "engine/ObjectPool.hpp"
#include "engine/Renderer.hpp"
#include "game/TileMap.hpp"
#include <string_view>

namespace majo {

enum class EffectType {
    Ring,
    Particle
};

enum class ParticleVisual {
    Circle,
    RockShard
};

enum class EffectLayer {
    World,
    Foreground
};

enum class DamagePopupStyle {
    Enemy,
    Player
};

enum class ParticleEffectId {
    DigDust,
    DirtBreak,
    RockBreak,
    OreBreak,
    RingTrail,
    EnemyHit,
    EnemyPoisonHit,
    EnemySlowHit,
    EnemyBleedHit,
    EnemyDeathSoul,
    CaptureSuccess,
    DropPickup,
    TorchFlicker,
    MagicFire,
    MagicIce,
    MagicThunder,
    MagicWind,
    MagicEarth,
    MagicDefault,
    PoisonAura,
    SlowAura,
    BleedAura,
    SpecialItemGlimmer,
    WarpCircle,
    BossCircle,
};

struct Effect {
    bool active = false;
    EffectType type = EffectType::Particle;
    EffectLayer layer = EffectLayer::World;
    Vec2 position{};
    Vec2 velocity{};
    Vec2 acceleration{};
    Color color{};
    ParticleVisual visual = ParticleVisual::Circle;
    float age = 0.0f;
    float duration = 0.35f;
    float startRadius = 2.0f;
    float endRadius = 10.0f;
    float drag = 3.5f;
    float rotation = 0.0f;
    float angularVelocity = 0.0f;
    float shardAspect = 1.0f;
    int shardVariant = 0;
};

struct DamagePopup {
    bool active = false;
    Vec2 position{};
    Vec2 velocity{};
    float age = 0.0f;
    float duration = 0.72f;
    int amount = 0;
    DamagePopupStyle style = DamagePopupStyle::Enemy;
};

struct SmokeBurstOptions {
    int count = 10;
    float size = 22.0f;
    float sizeJitter = 0.35f;
    float spreadRadius = 11.0f;
    float speed = 28.0f;
    float riseSpeed = 18.0f;
    float duration = 0.64f;
    float durationJitter = 0.10f;
    Color colorA{226, 226, 224, 178};
    Color colorB{142, 146, 150, 172};
    EffectLayer layer = EffectLayer::Foreground;
};

struct SmokePuff {
    bool active = false;
    EffectLayer layer = EffectLayer::Foreground;
    Vec2 position{};
    Vec2 velocity{};
    Color color{};
    float age = 0.0f;
    float duration = 0.64f;
    float radius = 18.0f;
    float growEnd = 0.22f;
    float shrinkStart = 0.58f;
    float peakScale = 1.20f;
    float lobeSpread = 0.35f;
    float phase = 0.0f;
};

class EffectSystem {
public:
    void update(float dt);
    void render(Renderer& renderer);
    void renderForeground(Renderer& renderer);
    void renderDamagePopups(Renderer& renderer);

    void spawn(
        ParticleEffectId id,
        Vec2 position,
        Vec2 direction = {1.0f, 0.0f},
        float scale = 1.0f,
        EffectLayer layer = EffectLayer::World,
        Color colorOverride = {0, 0, 0, 0});
    void spawnDamagePopup(Vec2 position, int amount, DamagePopupStyle style = DamagePopupStyle::Enemy);
    void spawnDigHit(Vec2 position, Vec2 direction = {1.0f, 0.0f}, Color colorOverride = {0, 0, 0, 0});
    void spawnTileBreak(Vec2 position, TileType tileType = TileType::Dirt, Color colorOverride = {0, 0, 0, 0});
    void spawnSmokeBurst(Vec2 position, SmokeBurstOptions options = {});
    void spawnEnemyHit(Vec2 position, std::string_view effect = {});
    void spawnEnemyDeath(Vec2 position);
    void spawnThrowStart(Vec2 position, Vec2 direction);
    void spawnReturn(Vec2 position);
    void spawnRingTrail(Vec2 position, Vec2 direction);
    void spawnForegroundRingTrail(Vec2 position, Vec2 direction);
    void spawnCaptureSuccess(Vec2 position, Vec2 direction);
    void spawnDropPickup(Vec2 position, Vec2 direction);
    void spawnMaterialFloat(Vec2 position, Color color);
    void spawnTorchFlicker(Vec2 position);
    void spawnForegroundTorchFlicker(Vec2 position);
    void spawnStatusAura(Vec2 position, std::string_view stateId);
    void spawnSpecialItemGlimmer(Vec2 position);
    void spawnForegroundSpecialItemGlimmer(Vec2 position);
    void spawnWarpCircle(Vec2 position, bool boss);
    void spawnAreaPulse(Vec2 position, float radius, Color color);
    void spawnMagicCast(Vec2 origin, Vec2 direction, std::string_view element, float power);

private:
    void renderLayer(Renderer& renderer, EffectLayer layer);
    void renderSmokeLayer(Renderer& renderer, EffectLayer layer);
    void spawnRing(Vec2 position, float startRadius, float endRadius, Color color, float duration, EffectLayer layer = EffectLayer::World);
    void spawnParticle(
        Vec2 position,
        Vec2 velocity,
        float radius,
        Color color,
        float duration,
        Vec2 acceleration = {},
        float drag = 3.5f,
        EffectLayer layer = EffectLayer::World,
        ParticleVisual visual = ParticleVisual::Circle,
        int shardVariant = 0,
        float rotation = 0.0f,
        float angularVelocity = 0.0f,
        float shardAspect = 1.0f);
    void spawnBurst(Vec2 position, int count, Color color, float speed, float radius, float duration, EffectLayer layer = EffectLayer::World);

    ObjectPool<Effect, balance::MaxEffects> effects_;
    ObjectPool<SmokePuff, 192> smokePuffs_;
    ObjectPool<DamagePopup, 128> damagePopups_;
};

}
