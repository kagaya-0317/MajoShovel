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
    Vec2 position{};
    Vec2 velocity{};
    Vec2 acceleration{};
    Color color{};
    float age = 0.0f;
    float duration = 0.35f;
    float startRadius = 2.0f;
    float endRadius = 10.0f;
    float drag = 3.5f;
};

class EffectSystem {
public:
    void update(float dt);
    void render(Renderer& renderer);

    void spawn(ParticleEffectId id, Vec2 position, Vec2 direction = {1.0f, 0.0f}, float scale = 1.0f);
    void spawnDigHit(Vec2 position, Vec2 direction = {1.0f, 0.0f});
    void spawnTileBreak(Vec2 position, TileType tileType = TileType::Dirt);
    void spawnEnemyHit(Vec2 position, std::string_view effect = {});
    void spawnEnemyDeath(Vec2 position);
    void spawnThrowStart(Vec2 position, Vec2 direction);
    void spawnReturn(Vec2 position);
    void spawnRingTrail(Vec2 position, Vec2 direction);
    void spawnCaptureSuccess(Vec2 position, Vec2 direction);
    void spawnDropPickup(Vec2 position, Vec2 direction);
    void spawnTorchFlicker(Vec2 position);
    void spawnStatusAura(Vec2 position, std::string_view stateId);
    void spawnSpecialItemGlimmer(Vec2 position);
    void spawnWarpCircle(Vec2 position, bool boss);
    void spawnAreaPulse(Vec2 position, float radius, Color color);
    void spawnMagicCast(Vec2 origin, Vec2 direction, std::string_view element, float power);

private:
    void spawnRing(Vec2 position, float startRadius, float endRadius, Color color, float duration);
    void spawnParticle(Vec2 position, Vec2 velocity, float radius, Color color, float duration, Vec2 acceleration = {}, float drag = 3.5f);
    void spawnBurst(Vec2 position, int count, Color color, float speed, float radius, float duration);

    ObjectPool<Effect, balance::MaxEffects> effects_;
};

}
