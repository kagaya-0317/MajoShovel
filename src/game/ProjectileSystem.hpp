#pragma once

#include "data/GameBalance.hpp"
#include "data/ObjectCatalog.hpp"
#include "engine/ObjectPool.hpp"
#include "engine/Renderer.hpp"
#include "game/DepthRender.hpp"
#include "game/EffectDispatcher.hpp"
#include "game/Player.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/TileMap.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace majo {

class EnemySystem;
class EncyclopediaSystem;

enum class ProjectileOwnerType {
    Enemy,
    PlayerOrbit,
};

struct Projectile {
    bool active = false;
    Vec2 position{};
    Vec2 velocity{};
    float radius = 4.0f;
    float lifetime = 1.0f;
    ProjectileOwnerType ownerType = ProjectileOwnerType::Enemy;
    std::string projectileId;
    int damage = 1;
    std::string damageType = "blunt";
    std::vector<EffectSpec> effects;
    std::vector<std::string> tags;
};

struct ProjectileSpawnTuning {
    float speedMultiplier = 1.0f;
    int damageOverride = -1;
    double damageMultiplier = 1.0;
    float radiusScale = 1.0f;
};

class ProjectileSystem {
public:
    bool spawn(std::string_view projectileId, Vec2 position, Vec2 direction, ProjectileOwnerType ownerType);
    bool spawn(std::string_view projectileId, Vec2 position, Vec2 direction, ProjectileOwnerType ownerType, const std::vector<EffectSpec>& effects);
    bool spawn(
        std::string_view projectileId,
        Vec2 position,
        Vec2 direction,
        ProjectileOwnerType ownerType,
        const std::vector<EffectSpec>& effects,
        const ProjectileSpawnTuning& tuning);
    void update(
        Player& player,
        SpellRingSystem& spellRing,
        EnemySystem& enemies,
        TileMap& map,
        float dt,
        const EffectDispatcher& effectDispatcher,
        const ObjectCatalog& objectCatalog,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    void render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights) const;
    void appendRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        const TileMap& map,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights) const;
    void clear();
    int activeCount() const { return projectiles_.activeCount(); }
    int activeCount(ProjectileOwnerType ownerType) const;
    int pullMetalProjectiles(Vec2 center, float dt, float radius = 170.0f);
    int deflectEnemyProjectiles(Vec2 center, float dt, float radius = 150.0f);

private:
    ObjectPool<Projectile, balance::MaxProjectiles> projectiles_;
};

}
