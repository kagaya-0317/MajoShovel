#pragma once

#include "data/GameBalance.hpp"
#include "engine/Math.hpp"
#include "engine/ObjectPool.hpp"
#include "engine/Renderer.hpp"

namespace majo {

enum class EffectType {
    Ring,
    Particle
};

struct Effect {
    bool active = false;
    EffectType type = EffectType::Particle;
    Vec2 position{};
    Vec2 velocity{};
    Color color{};
    float age = 0.0f;
    float duration = 0.35f;
    float startRadius = 2.0f;
    float endRadius = 10.0f;
};

class EffectSystem {
public:
    void update(float dt);
    void render(Renderer& renderer);

    void spawnDigHit(Vec2 position);
    void spawnTileBreak(Vec2 position);
    void spawnEnemyHit(Vec2 position);
    void spawnEnemyDeath(Vec2 position);
    void spawnThrowStart(Vec2 position, Vec2 direction);
    void spawnReturn(Vec2 position);

private:
    void spawnRing(Vec2 position, float startRadius, float endRadius, Color color, float duration);
    void spawnParticle(Vec2 position, Vec2 velocity, float radius, Color color, float duration);
    void spawnBurst(Vec2 position, int count, Color color, float speed, float radius, float duration);

    ObjectPool<Effect, balance::MaxEffects> effects_;
};

}
