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

#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace majo {

class EnemySystem;
class EncyclopediaSystem;

enum class ProjectileOwnerType {
    Enemy,
    PlayerOrbit,
};

struct ProjectileSoundEvent {
    std::string cueId;
    float volumeScale = 1.0f;
    float pitchScale = 1.0f;
};

struct Projectile {
    bool active = false;
    Vec2 position{};
    Vec2 velocity{};
    float radius = 4.0f;
    float age = 0.0f;
    float lifetime = 1.0f;
    float initialLifetime = 1.0f;
    float altitude = 0.0f;
    float arcHeight = 0.0f;
    float trailTimer = 0.0f;
    bool ballistic = false;
    bool piercesTargets = false;
    bool previewTargetHit = false;
    int visualVariant = 0;
    ProjectileOwnerType ownerType = ProjectileOwnerType::Enemy;
    std::string projectileId;
    int damage = 1;
    std::string damageType = "blunt";
    std::string sourceActorName;
    std::string displayName;
    std::vector<EffectSpec> effects;
    std::vector<std::string> tags;
    std::string launchSeId;
    std::string destroySeId;
};

enum class ProjectileFxVisual {
    SoftCircle,
    Ring,
    SparkLine,
    Shard,
    Needle,
    Thread,
    WindArc,
    Droplet,
    LightningBolt,
    Flame,
    StickySplat,
};

struct ProjectileFxParticle {
    bool active = false;
    ProjectileFxVisual visual = ProjectileFxVisual::SoftCircle;
    Vec2 position{};
    Vec2 velocity{};
    Color startColor{255, 255, 255, 220};
    Color endColor{255, 255, 255, 0};
    float age = 0.0f;
    float lifetime = 0.3f;
    float startRadius = 2.0f;
    float endRadius = 0.0f;
    float rotation = 0.0f;
    float angularVelocity = 0.0f;
    float drag = 2.8f;
    float stretch = 1.0f;
};

struct ProjectileSpawnTuning {
    float speedMultiplier = 1.0f;
    int damageOverride = -1;
    double damageMultiplier = 1.0;
    float radiusScale = 1.0f;
};

struct ProjectileSpawnMetadata {
    std::string sourceActorName;
};

struct ProjectilePreviewTarget {
    Vec2 position{};
    float radius = 0.0f;
    bool enabled = false;
};

struct ProjectileDefinition {
    std::string id;
    std::string displayName;
    float speed = 180.0f;
    float radius = 4.0f;
    float lifetime = 2.0f;
    int damage = 1;
    std::string damageType = "blunt";
    bool piercesTargets = false;
    std::vector<std::string> tags;
    std::string launchSeId;
    std::string destroySeId;
};

[[nodiscard]] std::span<const ProjectileDefinition> projectileDefinitions();

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
    bool spawn(
        std::string_view projectileId,
        Vec2 position,
        Vec2 direction,
        ProjectileOwnerType ownerType,
        const std::vector<EffectSpec>& effects,
        const ProjectileSpawnTuning& tuning,
        const ProjectileSpawnMetadata& metadata);
    void updatePreview(float dt);
    void updatePreview(float dt, std::optional<ProjectilePreviewTarget> target);
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
    void appendPreviewRenderEntries(std::vector<DepthRenderEntry>& entries, Renderer& renderer) const;
    void clear();
    int activeCount() const { return projectiles_.activeCount(); }
    int activeCount(ProjectileOwnerType ownerType) const;
    int pullMetalProjectiles(Vec2 center, float dt, float radius = 170.0f);
    int deflectEnemyProjectiles(Vec2 center, float dt, float radius = 150.0f);
    int pushProjectilesInDirection(Vec2 center, Vec2 direction, float dt, float radius, float strength = 1.0f);
    std::vector<ProjectileSoundEvent> consumeSoundEvents();
    std::vector<StatusPopupEvent> consumeStatusPopupEvents();

private:
    ObjectPool<Projectile, balance::MaxProjectiles> projectiles_;
    std::vector<ProjectileFxParticle> projectileFx_;
    std::vector<ProjectileSoundEvent> soundEvents_;
    std::vector<StatusPopupEvent> statusPopupEvents_;
};

}
