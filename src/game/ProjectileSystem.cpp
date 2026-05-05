#include "game/ProjectileSystem.hpp"

#include "game/Collision.hpp"
#include "game/EnemySystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace majo {

namespace {

struct ProjectilePrototype {
    std::string_view id;
    float speed = 180.0f;
    float radius = 4.0f;
    float lifetime = 2.0f;
    int damage = 1;
    std::string_view damageType = "physical";
    std::initializer_list<std::string_view> tags;
};

constexpr std::array<ProjectilePrototype, 8> Prototypes{{
    {"stone_bullet", 190.0f, 4.5f, 2.4f, 1, "physical", {"small", "stone"}},
    {"poison_spit", 155.0f, 5.0f, 2.8f, 1, "water", {"small", "poison"}},
    {"cactus_needle", 260.0f, 2.5f, 1.8f, 1, "physical", {"small", "needle"}},
    {"water_shot", 210.0f, 4.0f, 2.2f, 1, "water", {"small", "water"}},
    {"fire_breath", 145.0f, 7.0f, 1.2f, 2, "fire", {"fire", "short_range"}},
    {"web_thread", 135.0f, 3.5f, 2.4f, 0, "none", {"small", "web"}},
    {"wind_wave", 235.0f, 6.0f, 1.6f, 1, "wind", {"wind"}},
    {"explosion_small", 80.0f, 10.0f, 0.55f, 2, "fire", {"explosion"}},
}};

constexpr float CapturedMagnetProjectileRadius = 170.0f;
constexpr float CapturedMagnetProjectileAcceleration = 230.0f;
constexpr int CapturedMagnetProjectileLimit = 6;
constexpr float CapturedWindDeflectRadius = 150.0f;
constexpr float CapturedWindDeflectImpulse = 72.0f;
constexpr int CapturedWindDeflectLimit = 5;

const ProjectilePrototype& prototypeFor(std::string_view id)
{
    const auto it = std::find_if(Prototypes.begin(), Prototypes.end(), [id](const ProjectilePrototype& prototype) {
        return prototype.id == id;
    });
    if (it != Prototypes.end()) {
        return *it;
    }
    return Prototypes.front();
}

Color colorFor(std::string_view damageType)
{
    if (damageType == "fire") {
        return {255, 112, 48, 245};
    }
    if (damageType == "water") {
        return {78, 166, 255, 245};
    }
    if (damageType == "wind") {
        return {160, 235, 200, 235};
    }
    if (damageType == "magic") {
        return {190, 118, 255, 245};
    }
    if (damageType == "none") {
        return {184, 188, 198, 230};
    }
    return {220, 205, 160, 245};
}

int projectileDamage(const Projectile& projectile)
{
    if (projectile.damageType == "fire" || projectile.damageType == "magic") {
        return std::max(0, projectile.damage + 1);
    }
    return std::max(0, projectile.damage);
}

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

bool hasProjectileTag(const Projectile& projectile, std::string_view tag)
{
    return std::any_of(projectile.tags.begin(), projectile.tags.end(), [tag](const std::string& projectileTag) {
        return projectileTag == tag;
    });
}

bool isSmallProjectile(const Projectile& projectile)
{
    return projectile.radius <= 5.0f || hasProjectileTag(projectile, "small");
}

bool blocksProjectile(const SpellRingItem& item, const SpellRingSystem& spellRing, const Projectile& projectile)
{
    if (item.broken()) {
        return false;
    }

    if (item.hasCapturedBehavior("magic_guard") && projectile.damageType == "magic" &&
        circlesOverlap(projectile.position, projectile.radius, item.worldPosition, item.hitRadius + 12.0f)) {
        return true;
    }

    if (item.hasCapturedBehavior("heavy_guard") &&
        (projectile.damageType == "physical" || isSmallProjectile(projectile)) &&
        circlesOverlap(projectile.position, projectile.radius, item.worldPosition, item.hitRadius + 10.0f)) {
        return true;
    }

    if (item.hasCapturedBehavior("outward_guard") &&
        circlesOverlap(projectile.position, projectile.radius, item.worldPosition, item.hitRadius + 8.0f)) {
        const Vec2 outward = normalize(item.worldPosition - spellRing.center());
        const Vec2 incomingFrom = normalize(projectile.velocity * -1.0f);
        if (dot(outward, incomingFrom) > 0.25f) {
            return true;
        }
    }

    return isSmallProjectile(projectile) &&
        circlesOverlap(projectile.position, projectile.radius, item.worldPosition, item.hitRadius);
}

void pushPlayer(Player& player, TileMap& map, Vec2 direction, float distance)
{
    const Vec2 delta = normalize(direction) * distance;
    const Vec2 next = player.position + delta;
    if (!map.isCircleBlocked(next, balance::PlayerRadius)) {
        player.position = next;
    }
}

}

bool ProjectileSystem::spawn(std::string_view projectileId, Vec2 position, Vec2 direction, ProjectileOwnerType ownerType)
{
    static const std::vector<EffectSpec> NoEffects;
    return spawn(projectileId, position, direction, ownerType, NoEffects);
}

bool ProjectileSystem::spawn(std::string_view projectileId, Vec2 position, Vec2 direction, ProjectileOwnerType ownerType, const std::vector<EffectSpec>& effects)
{
    Projectile* projectile = projectiles_.acquire();
    if (projectile == nullptr) {
        return false;
    }

    const ProjectilePrototype& prototype = prototypeFor(projectileId);
    *projectile = Projectile{};
    projectile->active = true;
    projectile->position = position;
    projectile->velocity = normalize(direction) * prototype.speed;
    projectile->radius = prototype.radius;
    projectile->lifetime = prototype.lifetime;
    projectile->ownerType = ownerType;
    projectile->projectileId = std::string(prototype.id);
    projectile->damage = prototype.damage;
    projectile->damageType = std::string(prototype.damageType);
    for (std::string_view tag : prototype.tags) {
        projectile->tags.emplace_back(tag);
    }
    projectile->effects = effects;
    return true;
}

void ProjectileSystem::update(
    Player& player,
    SpellRingSystem& spellRing,
    EnemySystem& enemies,
    TileMap& map,
    float dt,
    const EffectDispatcher& effectDispatcher,
    const ObjectCatalog& objectCatalog,
    std::vector<EffectDiscoveryEvent>* discoveryEvents)
{
    for (Projectile& projectile : projectiles_.items()) {
        if (!projectile.active) {
            continue;
        }

        projectile.lifetime -= dt;
        if (projectile.lifetime <= 0.0f) {
            projectile.active = false;
            continue;
        }

        projectile.position += projectile.velocity * dt;
        if (map.isCircleBlocked(projectile.position, projectile.radius)) {
            projectile.active = false;
            continue;
        }

        if (projectile.ownerType == ProjectileOwnerType::Enemy) {
            bool consumedByRing = false;
            const SpellRingItem* blockingItem = nullptr;
            for (const SpellRingItem& item : spellRing.items()) {
                if (blocksProjectile(item, spellRing, projectile)) {
                    consumedByRing = true;
                    blockingItem = &item;
                    break;
                }
            }
            if (consumedByRing) {
                if (discoveryEvents != nullptr && blockingItem != nullptr && !blockingItem->objectId.empty()) {
                    const ObjectDefinition* object = objectCatalog.registry.findById(blockingItem->objectId);
                    if (object != nullptr) {
                        discoveryEvents->push_back(EffectDiscoveryEvent{
                            .objectId = object->id,
                            .objectName = object->name,
                            .effectKey = "projectile_guard",
                            .description = "飛んできた弾を防ぐ",
                            .position = blockingItem->worldPosition,
                        });
                    }
                }
                projectile.active = false;
                continue;
            }
        }

        if (projectile.ownerType == ProjectileOwnerType::Enemy &&
            circlesOverlap(projectile.position, projectile.radius, player.position, balance::PlayerRadius)) {
            player.applyDamage(projectileDamage(projectile), DamageSource::Projectile);
            if (projectile.projectileId == "wind_wave") {
                pushPlayer(player, map, projectile.velocity, 18.0f);
            }
            if (!projectile.effects.empty()) {
                EffectContext context;
                context.owner = &player;
                context.position = projectile.position;
                context.triggerType = EffectTriggerType::Hit;
                context.logUnimplementedEffects = false;
                effectDispatcher.dispatch(projectile.effects, context);
            }
            projectile.active = false;
            continue;
        }

        if (projectile.ownerType == ProjectileOwnerType::PlayerOrbit &&
            enemies.hitByPlayerProjectile(projectile, player, spellRing, projectileDamage(projectile), effectDispatcher, discoveryEvents)) {
            projectile.active = false;
        }
    }
}

int ProjectileSystem::activeCount(ProjectileOwnerType ownerType) const
{
    int count = 0;
    for (const Projectile& projectile : projectiles_.items()) {
        if (projectile.active && projectile.ownerType == ownerType) {
            ++count;
        }
    }
    return count;
}

int ProjectileSystem::pullMetalProjectiles(Vec2 center, float dt)
{
    if (dt <= 0.0f) {
        return 0;
    }

    int pulled = 0;
    const float radiusSq = CapturedMagnetProjectileRadius * CapturedMagnetProjectileRadius;
    for (Projectile& projectile : projectiles_.items()) {
        if (!projectile.active || !hasProjectileTag(projectile, "metal")) {
            continue;
        }
        const Vec2 toCenter = center - projectile.position;
        const float distanceSq = lengthSquared(toCenter);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }
        const float distance = std::sqrt(distanceSq);
        const float falloff = 1.0f - clamp(distance / CapturedMagnetProjectileRadius, 0.0f, 1.0f);
        projectile.velocity += normalize(toCenter) * (CapturedMagnetProjectileAcceleration * falloff * dt);
        ++pulled;
        if (pulled >= CapturedMagnetProjectileLimit) {
            break;
        }
    }
    return pulled;
}

int ProjectileSystem::deflectEnemyProjectiles(Vec2 center, float dt)
{
    if (dt <= 0.0f) {
        return 0;
    }

    int deflected = 0;
    const float radiusSq = CapturedWindDeflectRadius * CapturedWindDeflectRadius;
    for (Projectile& projectile : projectiles_.items()) {
        if (!projectile.active || projectile.ownerType != ProjectileOwnerType::Enemy) {
            continue;
        }
        const Vec2 fromCenter = projectile.position - center;
        const float distanceSq = lengthSquared(fromCenter);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }
        const Vec2 side{-fromCenter.y, fromCenter.x};
        projectile.velocity += normalize(side) * (CapturedWindDeflectImpulse * dt);
        ++deflected;
        if (deflected >= CapturedWindDeflectLimit) {
            break;
        }
    }
    return deflected;
}

void ProjectileSystem::render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights) const
{
    for (const Projectile& projectile : projectiles_.items()) {
        if (!projectile.active || !map.isLit(projectile.position, playerLight, extraLights)) {
            continue;
        }
        const Color color = colorFor(projectile.damageType);
        renderer.fillCircle(projectile.position, projectile.radius, color);
        renderer.drawCircle(projectile.position, projectile.radius + 2.0f, {30, 30, 38, 210});
    }
}

void ProjectileSystem::clear()
{
    projectiles_ = {};
}

}
