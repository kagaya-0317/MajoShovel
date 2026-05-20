#include "game/MagicSystem.hpp"

#include "game/EnemySystem.hpp"
#include "game/MagicFxSystem.hpp"
#include "game/Player.hpp"
#include "game/SpellRingItem.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/TileMap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

namespace majo {

namespace {

constexpr std::size_t MaxMagicProjectiles = 96;
constexpr std::size_t MaxMagicGroundAreas = 48;
constexpr std::size_t MaxMagicTransientLights = 32;
constexpr float FireAfterglowLightSeconds = 1.22f;
constexpr float IceShatterLightSeconds = 1.05f;
constexpr float ThunderStrongLightSeconds = 1.28f;
constexpr float ThunderWeakLightSeconds = 1.04f;
constexpr float EarthDebrisLightSeconds = 0.72f;
constexpr float WindWaveFxFadeStart = 0.68f;

std::mt19937& magicSystemRng()
{
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

float sampleFloat(float minValue, float maxValue)
{
    if (maxValue < minValue) {
        std::swap(minValue, maxValue);
    }
    if (std::abs(maxValue - minValue) <= 0.0001f) {
        return minValue;
    }
    std::uniform_real_distribution<float> dist(minValue, maxValue);
    return dist(magicSystemRng());
}

int sampleInt(int minValue, int maxValue)
{
    if (maxValue < minValue) {
        std::swap(minValue, maxValue);
    }
    std::uniform_int_distribution<int> dist(minValue, maxValue);
    return dist(magicSystemRng());
}

float magicCooldownSeconds(MagicElement element)
{
    switch (element) {
    case MagicElement::Thunder:
        return 1.05f;
    case MagicElement::Earth:
        return 0.55f;
    case MagicElement::Wind:
        return 0.38f;
    case MagicElement::Fire:
    case MagicElement::Ice:
        return 0.28f;
    }
    return 0.35f;
}

float magicAuraSeconds(MagicElement element)
{
    return element == MagicElement::Thunder ? 1.2f : 1.0f;
}

float angleOf(Vec2 v)
{
    return std::atan2(v.y, v.x);
}

bool magicElementFromDamageType(std::string_view damageType, MagicElement& outElement)
{
    if (damageType == "fire") {
        outElement = MagicElement::Fire;
        return true;
    }
    if (damageType == "ice") {
        outElement = MagicElement::Ice;
        return true;
    }
    if (damageType == "thunder") {
        outElement = MagicElement::Thunder;
        return true;
    }
    if (damageType == "wind") {
        outElement = MagicElement::Wind;
        return true;
    }
    if (damageType == "earth") {
        outElement = MagicElement::Earth;
        return true;
    }
    return false;
}

Vec2 projectileLightPosition(const MagicSystem::MagicProjectile& projectile)
{
    return projectile.position + Vec2{0.0f, -projectile.height};
}

float projectileLightRadius(const MagicSystem::MagicProjectile& projectile)
{
    switch (projectile.kind) {
    case MagicSystem::ProjectileKind::Fireball:
        return 34.0f + projectile.radius * 0.9f;
    case MagicSystem::ProjectileKind::IceShard:
        return 72.0f + projectile.radius * 2.0f;
    case MagicSystem::ProjectileKind::WindWave: {
        const float progress = projectile.lifetime > 0.0f ? std::clamp(projectile.age / projectile.lifetime, 0.0f, 1.0f) : 1.0f;
        const float eased = std::pow(progress, 1.45f);
        return (43.0f + projectile.radius * 0.6f) * (1.0f - eased);
    }
    case MagicSystem::ProjectileKind::DirtClod:
        return 58.0f + projectile.radius * 2.0f;
    }
    return 80.0f;
}

float groundAreaLightRadius(const MagicSystem::MagicGroundArea& area)
{
    const float progress = area.lifetime > 0.0f ? std::clamp(area.age / area.lifetime, 0.0f, 1.0f) : 1.0f;
    const float t = 1.0f - progress;
    const float fade = 0.35f + 0.65f * t;
    switch (area.kind) {
    case MagicSystem::GroundKind::FirePatch: {
        const float baseRadius = std::max(36.0f, area.radius * 1.65f);
        if (progress < 0.18f) {
            const float expand = progress / 0.18f;
            const float eased = 1.0f - (1.0f - expand) * (1.0f - expand) * (1.0f - expand);
            return baseRadius * lerp(0.68f, 1.1f, eased);
        }
        const float shrink = (progress - 0.18f) / 0.82f;
        const float eased = shrink * shrink * (3.0f - 2.0f * shrink);
        return baseRadius * 1.1f * (1.0f - eased);
    }
    case MagicSystem::GroundKind::FireAfterglow: {
        const float baseRadius = std::max(30.0f, area.radius * 1.35f);
        if (progress < 0.16f) {
            const float expand = progress / 0.16f;
            const float eased = 1.0f - (1.0f - expand) * (1.0f - expand);
            return baseRadius * lerp(0.60f, 0.92f, eased);
        }
        const float shrink = (progress - 0.16f) / 0.84f;
        const float eased = shrink * shrink * (3.0f - 2.0f * shrink);
        return baseRadius * 0.92f * (1.0f - eased);
    }
    case MagicSystem::GroundKind::IceShatter: {
        const float baseRadius = std::max(64.0f, area.radius * 3.35f);
        if (progress < 0.24f) {
            const float expand = progress / 0.24f;
            const float eased = 1.0f - (1.0f - expand) * (1.0f - expand) * (1.0f - expand);
            return baseRadius * lerp(0.64f, 1.18f, eased);
        }
        const float shrink = (progress - 0.24f) / 0.76f;
        const float eased = shrink * shrink * (3.0f - 2.0f * shrink);
        return baseRadius * 1.18f * (1.0f - eased);
    }
    case MagicSystem::GroundKind::ThunderStrikeLight: {
        const float baseRadius = std::max(24.0f, area.radius);
        if (progress < 0.16f) {
            const float expand = progress / 0.16f;
            const float eased = 1.0f - (1.0f - expand) * (1.0f - expand);
            return baseRadius * lerp(0.72f, 1.0f, eased);
        }
        const float shrink = (progress - 0.16f) / 0.84f;
        const float eased = shrink * shrink * (3.0f - 2.0f * shrink);
        return baseRadius * (1.0f - eased);
    }
    case MagicSystem::GroundKind::EarthSpike: {
        const float baseRadius = std::max(36.0f, area.radius * 1.5f);
        if (progress < 0.76f) {
            return baseRadius;
        }
        const float shrink = (progress - 0.76f) / 0.24f;
        const float eased = shrink * shrink * (3.0f - 2.0f * shrink);
        return baseRadius * (1.0f - eased);
    }
    case MagicSystem::GroundKind::EarthDebrisBreakLight: {
        const float baseRadius = std::max(30.0f, area.radius * 2.2f);
        if (progress < 0.18f) {
            const float expand = progress / 0.18f;
            const float eased = 1.0f - (1.0f - expand) * (1.0f - expand);
            return baseRadius * lerp(0.64f, 1.0f, eased);
        }
        const float shrink = (progress - 0.18f) / 0.82f;
        const float eased = shrink * shrink * (3.0f - 2.0f * shrink);
        return baseRadius * (1.0f - eased);
    }
    }
    return 64.0f * fade;
}

} // namespace

std::string_view magicElementDamageType(MagicElement element)
{
    switch (element) {
    case MagicElement::Fire:
        return "fire";
    case MagicElement::Ice:
        return "ice";
    case MagicElement::Thunder:
        return "thunder";
    case MagicElement::Wind:
        return "wind";
    case MagicElement::Earth:
        return "earth";
    }
    return "magic";
}

bool magicElementFromCastEffect(std::string_view effect, MagicElement& outElement)
{
    if (effect == "cast_fire") {
        outElement = MagicElement::Fire;
        return true;
    }
    if (effect == "cast_ice") {
        outElement = MagicElement::Ice;
        return true;
    }
    if (effect == "cast_thunder") {
        outElement = MagicElement::Thunder;
        return true;
    }
    if (effect == "cast_wind") {
        outElement = MagicElement::Wind;
        return true;
    }
    if (effect == "cast_earth") {
        outElement = MagicElement::Earth;
        return true;
    }
    return false;
}

void MagicSystem::setFxSystem(MagicFxSystem* magicFx)
{
    magicFx_ = magicFx;
}

void MagicSystem::cast(MagicElement element, Vec2 origin, Vec2 direction, int power, SpellRingItem* sourceItem)
{
    direction = normalize(direction);
    power = std::max(1, power);
    if (sourceItem != nullptr) {
        if (sourceItem->magicCastCooldownTimer > 0.0f) {
            return;
        }
        sourceItem->magicAuraDamageType = std::string(magicElementDamageType(element));
        sourceItem->magicAuraTimer = std::max(sourceItem->magicAuraTimer, magicAuraSeconds(element));
        sourceItem->magicCastCooldownTimer = magicCooldownSeconds(element);
        if (magicFx_ != nullptr && sourceItem->magicAuraFxEmitterId == 0) {
            sourceItem->magicAuraFxEmitterId = startAuraFxEmitter(
                element,
                sourceItem->worldPosition,
                std::max(8.0f, sourceItem->hitRadius));
        }
    }

    soundEvents_.push_back(MagicSoundEvent::Cast);

    switch (element) {
    case MagicElement::Fire:
        castFire(origin, direction, power);
        return;
    case MagicElement::Ice:
        castIce(origin, direction, power);
        return;
    case MagicElement::Thunder:
        castThunder(origin, power);
        return;
    case MagicElement::Wind:
        castWind(origin, direction, power);
        return;
    case MagicElement::Earth:
        castEarth(origin, direction, power);
        return;
    }
}

void MagicSystem::update(Player&, SpellRingSystem& spellRing, EnemySystem& enemies, TileMap& map, float dt)
{
    updateItemAuras(spellRing, dt);
    updateTransientLights(dt);
    updateThunder(enemies, spellRing);
    updateProjectiles(enemies, spellRing, map, dt);
    updateGroundAreas(enemies, spellRing, dt);
}

void MagicSystem::appendLightSources(std::vector<LightSource>& outLights) const
{
    for (const MagicProjectile& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }
        outLights.push_back({
            projectileLightPosition(projectile),
            projectileLightRadius(projectile),
        });
    }

    for (const MagicGroundArea& area : groundAreas_) {
        if (!area.active) {
            continue;
        }
        outLights.push_back({
            area.position,
            groundAreaLightRadius(area),
        });
    }

    for (const MagicLight& light : transientLights_) {
        if (!light.active) {
            continue;
        }
        const float t = light.lifetime > 0.0f ? std::clamp(1.0f - light.age / light.lifetime, 0.0f, 1.0f) : 1.0f;
        if (t <= 0.0f) {
            continue;
        }
        outLights.push_back({
            light.position,
            light.radius * (0.45f + 0.55f * t),
        });
    }
}

void MagicSystem::clear()
{
    for (MagicProjectile& projectile : projectiles_) {
        stopProjectileFx(projectile);
    }
    projectiles_.clear();
    groundAreas_.clear();
    pendingThunder_.clear();
    transientLights_.clear();
    soundEvents_.clear();
}

std::vector<MagicSoundEvent> MagicSystem::consumeSoundEvents()
{
    std::vector<MagicSoundEvent> events = soundEvents_;
    soundEvents_.clear();
    return events;
}

void MagicSystem::castFire(Vec2 origin, Vec2 direction, int power)
{
    MagicProjectile projectile;
    projectile.kind = ProjectileKind::Fireball;
    projectile.element = MagicElement::Fire;
    projectile.position = origin + direction * 12.0f;
    projectile.velocity = direction * (210.0f + static_cast<float>(power) * 12.0f);
    projectile.radius = 8.0f + static_cast<float>(power);
    projectile.age = 0.0f;
    projectile.lifetime = 1.1f;
    projectile.height = 20.0f;
    projectile.verticalVelocity = 42.0f;
    projectile.gravity = 180.0f;
    projectile.damage = std::max(1, power * 2);
    projectile.piercesWalls = true;
    if (magicFx_ != nullptr) {
        projectile.fxEmitterId = magicFx_->startFireballLoop(
            projectile.position + Vec2{0.0f, -projectile.height},
            direction,
            projectile.radius).id;
    }
    addProjectile(projectile);
}

void MagicSystem::castIce(Vec2 origin, Vec2 direction, int power)
{
    const float baseAngle = angleOf(direction);
    constexpr std::array<float, 3> Spreads{-0.18f, 0.0f, 0.18f};
    for (float spread : Spreads) {
        const Vec2 shotDirection = fromAngle(baseAngle + spread);
        MagicProjectile projectile;
        projectile.kind = ProjectileKind::IceShard;
        projectile.element = MagicElement::Ice;
        projectile.position = origin + shotDirection * 12.0f;
        projectile.velocity = shotDirection * (250.0f + static_cast<float>(power) * 10.0f);
        projectile.radius = 11.0f;
        projectile.lifetime = 0.62f;
        projectile.height = 10.0f;
        projectile.damage = std::max(1, power + 1);
        projectile.fxEmitterId = startProjectileFxEmitter(projectile);
        addProjectile(projectile);
    }
}

void MagicSystem::castThunder(Vec2 origin, int power)
{
    pendingThunder_.push_back(PendingThunder{
        .origin = origin,
        .range = 126.0f + static_cast<float>(power) * 10.0f,
        .damage = std::max(1, power * 4),
        .paralyzeChance = 35.0,
        .paralyzeDuration = 1.0,
    });
}

void MagicSystem::castWind(Vec2 origin, Vec2 direction, int power)
{
    MagicProjectile projectile;
    projectile.kind = ProjectileKind::WindWave;
    projectile.element = MagicElement::Wind;
    projectile.position = origin + direction * 16.0f;
    projectile.velocity = direction * (205.0f + static_cast<float>(power) * 8.0f);
    projectile.radius = 24.0f + static_cast<float>(power) * 2.0f;
    projectile.lifetime = 0.72f;
    projectile.damage = std::max(1, power * 2);
    projectile.piercesWalls = true;
    projectile.fxEmitterId = startProjectileFxEmitter(projectile);
    addProjectile(projectile);
}

void MagicSystem::castEarth(Vec2 origin, Vec2 direction, int power)
{
    MagicGroundArea spike;
    spike.kind = GroundKind::EarthSpike;
    spike.position = origin + direction * 48.0f;
    spike.radius = 24.0f + static_cast<float>(power) * 2.0f;
    spike.lifetime = 1.10f;
    spike.damage = std::max(1, power * 3);
    if (magicFx_ != nullptr) {
        magicFx_->playEarthSpikeRise(spike.position, spike.radius);
    }
    addGroundArea(spike);

    const float baseAngle = angleOf(direction);
    const int clodCount = sampleInt(5, 7);
    const float fan = sampleFloat(0.92f, 1.28f);
    for (int i = 0; i < clodCount; ++i) {
        const float t = clodCount > 1 ? static_cast<float>(i) / static_cast<float>(clodCount - 1) : 0.5f;
        const float spread = lerp(-fan * 0.5f, fan * 0.5f, t) + sampleFloat(-0.18f, 0.18f);
        const Vec2 clodDirection = fromAngle(baseAngle + spread);
        const float speed = sampleFloat(48.0f, 96.0f) + static_cast<float>(power) * sampleFloat(2.0f, 6.0f);
        MagicProjectile projectile;
        projectile.kind = ProjectileKind::DirtClod;
        projectile.element = MagicElement::Earth;
        projectile.position = origin + clodDirection * sampleFloat(12.0f, 24.0f);
        projectile.velocity = clodDirection * speed;
        projectile.radius = sampleFloat(5.2f, 12.8f);
        projectile.lifetime = sampleFloat(0.82f, 1.36f);
        projectile.height = sampleFloat(6.0f, 24.0f);
        projectile.verticalVelocity = sampleFloat(46.0f, 104.0f);
        projectile.gravity = sampleFloat(190.0f, 340.0f);
        projectile.damage = std::max(1, power + 1);
        projectile.piercesWalls = true;
        projectile.fxEmitterId = startProjectileFxEmitter(projectile);
        addProjectile(projectile);
    }
}

void MagicSystem::updateProjectiles(EnemySystem& enemies, SpellRingSystem& spellRing, TileMap& map, float dt)
{
    for (MagicProjectile& projectile : projectiles_) {
        if (!projectile.active) {
            continue;
        }

        projectile.age += dt;
        projectile.position += projectile.velocity * dt;
        projectile.height = std::max(0.0f, projectile.height + projectile.verticalVelocity * dt);
        projectile.verticalVelocity -= projectile.gravity * dt;
        if (projectile.kind == ProjectileKind::WindWave) {
            const float progress = projectile.lifetime > 0.0f ? std::clamp(projectile.age / projectile.lifetime, 0.0f, 1.0f) : 1.0f;
            const float windDrag = lerp(0.55f, 3.2f, progress);
            projectile.velocity = projectile.velocity * std::max(0.0f, 1.0f - windDrag * dt);
        } else if (projectile.kind == ProjectileKind::DirtClod) {
            const float progress = projectile.lifetime > 0.0f ? std::clamp(projectile.age / projectile.lifetime, 0.0f, 1.0f) : 1.0f;
            const float clodDrag = lerp(0.58f, 2.25f, progress);
            projectile.velocity = projectile.velocity * std::max(0.0f, 1.0f - clodDrag * dt);
        }
        updateProjectileFx(projectile);

        if (!projectile.piercesWalls && map.isCircleBlocked(projectile.position, projectile.radius)) {
            if (projectile.kind == ProjectileKind::IceShard) {
                soundEvents_.push_back(MagicSoundEvent::Impact);
                if (magicFx_ != nullptr) {
                    magicFx_->playIceShatter(projectile.position, projectile.radius + 12.0f);
                }
                addGroundArea(MagicGroundArea{
                    .kind = GroundKind::IceShatter,
                    .position = projectile.position,
                    .radius = projectile.radius + 12.0f,
                    .age = 0.0f,
                    .lifetime = IceShatterLightSeconds,
                    .tickTimer = 0.0f,
                    .damage = 0,
                    .active = true,
                    .triggered = false,
                });
            }
            stopProjectileFx(projectile);
            projectile.active = false;
            continue;
        }

        if (projectile.kind == ProjectileKind::DirtClod && projectile.age > 0.12f && projectile.height <= 0.0f && projectile.verticalVelocity <= 0.0f) {
            playProjectileImpactFx(projectile);
            stopProjectileFx(projectile);
            projectile.active = false;
            continue;
        }

        EnemyMagicHitSpec hit;
        hit.position = projectile.position;
        hit.radius = projectile.radius;
        hit.damage = projectile.damage;
        hit.damageType = std::string(magicElementDamageType(projectile.element));
        hit.effectId = hit.damageType;
        hit.maxHits = 1;
        if (projectile.kind == ProjectileKind::WindWave && projectile.damage > 0) {
            hit.knockbackDirection = normalize(projectile.velocity);
            hit.knockbackStrength = 120.0f;
        }
        const int hitCount = enemies.applyMagicArea(hit, spellRing);
        if (hitCount > 0 && projectile.kind == ProjectileKind::WindWave) {
            playProjectileImpactFx(projectile);
            projectile.damage = 0;
            continue;
        }
        if (hitCount > 0) {
            playProjectileImpactFx(projectile);
            stopProjectileFx(projectile);
            projectile.active = false;
            if (projectile.kind == ProjectileKind::Fireball) {
                spawnFirePatch(projectile.position, 28.0f, std::max(1, projectile.damage / 2));
            }
            continue;
        }

        if (projectile.kind == ProjectileKind::Fireball && projectile.age > 0.12f && projectile.height <= 0.0f) {
            soundEvents_.push_back(MagicSoundEvent::Impact);
            stopProjectileFx(projectile);
            projectile.active = false;
            spawnFirePatch(projectile.position, 28.0f, std::max(1, projectile.damage / 2));
            continue;
        }

        if (projectile.age >= projectile.lifetime) {
            stopProjectileFx(projectile);
            projectile.active = false;
        }
    }

    projectiles_.erase(
        std::remove_if(projectiles_.begin(), projectiles_.end(), [](const MagicProjectile& projectile) {
            return !projectile.active;
        }),
        projectiles_.end());
}

void MagicSystem::updateGroundAreas(EnemySystem& enemies, SpellRingSystem& spellRing, float dt)
{
    for (MagicGroundArea& area : groundAreas_) {
        if (!area.active) {
            continue;
        }
        area.age += dt;
        if (area.age >= area.lifetime) {
            area.active = false;
            continue;
        }

        if (area.kind == GroundKind::FirePatch) {
            area.tickTimer -= dt;
            if (area.tickTimer <= 0.0f) {
                area.tickTimer = 0.25f;
                EnemyMagicHitSpec hit;
                hit.position = area.position;
                hit.radius = area.radius;
                hit.damage = area.damage;
                hit.damageType = "fire";
                hit.effectId = "fire";
                enemies.applyMagicArea(hit, spellRing);
            }
            continue;
        }

        if (area.kind == GroundKind::IceShatter || area.kind == GroundKind::EarthDebrisBreakLight) {
            continue;
        }

        if (area.kind == GroundKind::FireAfterglow || area.kind == GroundKind::ThunderStrikeLight) {
            continue;
        }

        if (area.kind == GroundKind::EarthSpike && !area.triggered) {
            area.triggered = true;
            soundEvents_.push_back(MagicSoundEvent::Impact);
            EnemyMagicHitSpec hit;
            hit.position = area.position;
            hit.radius = area.radius;
            hit.damage = area.damage;
            hit.damageType = "earth";
            hit.effectId = "earth";
            hit.statusEffect = "status_stun";
            hit.statusValue = 1.0;
            hit.statusDuration = 0.6;
            hit.statusChance = 100.0;
            enemies.applyMagicArea(hit, spellRing);
        }
    }

    groundAreas_.erase(
        std::remove_if(groundAreas_.begin(), groundAreas_.end(), [](const MagicGroundArea& area) {
            return !area.active;
        }),
        groundAreas_.end());
}

void MagicSystem::updateThunder(EnemySystem& enemies, SpellRingSystem& spellRing)
{
    for (const PendingThunder& thunder : pendingThunder_) {
        EnemyMagicHitSpec hit;
        hit.position = thunder.origin;
        hit.radius = 0.0f;
        hit.damage = thunder.damage;
        hit.damageType = "thunder";
        hit.effectId = "thunder";
        hit.statusEffect = "status_paralyze";
        hit.statusValue = 1.0;
        hit.statusDuration = thunder.paralyzeDuration;
        hit.statusChance = thunder.paralyzeChance;
        Vec2 target{};
        if (enemies.applyMagicNearest(thunder.origin, thunder.range, hit, spellRing, &target)) {
            soundEvents_.push_back(MagicSoundEvent::Impact);
            addTransientLight(thunder.origin, 108.0f, 0.18f);
            addTransientLight((thunder.origin + target) * 0.5f, 126.0f, 0.17f);
            addGroundArea(MagicGroundArea{
                .kind = GroundKind::ThunderStrikeLight,
                .position = target,
                .radius = 172.0f,
                .age = 0.0f,
                .lifetime = ThunderStrongLightSeconds,
                .tickTimer = 0.0f,
                .damage = 0,
                .active = true,
                .triggered = false,
            });
            if (magicFx_ != nullptr) {
                magicFx_->playThunderStrike(thunder.origin, target, true);
            }
        } else {
            soundEvents_.push_back(MagicSoundEvent::Impact);
            addGroundArea(MagicGroundArea{
                .kind = GroundKind::ThunderStrikeLight,
                .position = thunder.origin,
                .radius = 144.0f,
                .age = 0.0f,
                .lifetime = ThunderWeakLightSeconds,
                .tickTimer = 0.0f,
                .damage = 0,
                .active = true,
                .triggered = false,
            });
            if (magicFx_ != nullptr) {
                magicFx_->playThunderStrike(thunder.origin, thunder.origin, false);
            }
        }
    }
    pendingThunder_.clear();
}

void MagicSystem::updateTransientLights(float dt)
{
    for (MagicLight& light : transientLights_) {
        if (!light.active) {
            continue;
        }
        light.age += dt;
        if (light.age >= light.lifetime) {
            light.active = false;
        }
    }

    transientLights_.erase(
        std::remove_if(transientLights_.begin(), transientLights_.end(), [](const MagicLight& light) {
            return !light.active;
        }),
        transientLights_.end());
}

void MagicSystem::updateItemAuras(SpellRingSystem& spellRing, float dt)
{
    std::vector<SpellRingItem*> runtimeItems = spellRing.runtimeItemsMutable();
    for (SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        SpellRingItem& item = *itemPtr;
        item.magicCastCooldownTimer = std::max(0.0f, item.magicCastCooldownTimer - dt);
        MagicElement auraElement = MagicElement::Fire;
        const bool knownAura = magicElementFromDamageType(item.magicAuraDamageType, auraElement);
        if ((!knownAura || item.broken()) && item.magicAuraFxEmitterId != 0) {
            if (magicFx_ != nullptr) {
                magicFx_->stopEmitter({item.magicAuraFxEmitterId});
            }
            item.magicAuraFxEmitterId = 0;
        }
        if (item.magicAuraTimer > 0.0f) {
            item.magicAuraTimer = std::max(0.0f, item.magicAuraTimer - dt);
            if (knownAura && magicFx_ != nullptr) {
                if (item.magicAuraFxEmitterId == 0) {
                    item.magicAuraFxEmitterId = startAuraFxEmitter(auraElement, item.worldPosition, std::max(8.0f, item.hitRadius));
                } else if (!magicFx_->setEmitterPosition({item.magicAuraFxEmitterId}, item.worldPosition)) {
                    item.magicAuraFxEmitterId = startAuraFxEmitter(auraElement, item.worldPosition, std::max(8.0f, item.hitRadius));
                }
            }
            if (item.magicAuraTimer <= 0.0f) {
                if (item.magicAuraFxEmitterId != 0 && magicFx_ != nullptr) {
                    magicFx_->stopEmitter({item.magicAuraFxEmitterId});
                }
                item.magicAuraFxEmitterId = 0;
                item.magicAuraDamageType.clear();
            }
        } else if (item.magicAuraFxEmitterId != 0) {
            if (magicFx_ != nullptr) {
                magicFx_->stopEmitter({item.magicAuraFxEmitterId});
            }
            item.magicAuraFxEmitterId = 0;
        }
    }
}

std::uint32_t MagicSystem::startAuraFxEmitter(MagicElement element, Vec2 position, float radius)
{
    if (magicFx_ == nullptr) {
        return 0;
    }
    switch (element) {
    case MagicElement::Fire:
        return magicFx_->startFireAura(position, radius).id;
    case MagicElement::Ice:
        return magicFx_->startIceAura(position, radius).id;
    case MagicElement::Thunder:
        return magicFx_->startThunderAura(position, radius).id;
    case MagicElement::Wind:
        return magicFx_->startWindAura(position, radius).id;
    case MagicElement::Earth:
        return magicFx_->startEarthAura(position, radius).id;
    }
    return 0;
}

std::uint32_t MagicSystem::startProjectileFxEmitter(const MagicProjectile& projectile)
{
    if (magicFx_ == nullptr) {
        return 0;
    }
    const Vec2 drawPosition = projectile.position + Vec2{0.0f, -projectile.height};
    const Vec2 direction = lengthSquared(projectile.velocity) > 0.0001f ? normalize(projectile.velocity) : Vec2{1.0f, 0.0f};
    switch (projectile.kind) {
    case ProjectileKind::Fireball:
        return magicFx_->startFireballLoop(drawPosition, direction, projectile.radius).id;
    case ProjectileKind::IceShard:
        return magicFx_->startIceShardLoop(drawPosition, direction, projectile.radius).id;
    case ProjectileKind::WindWave:
        return magicFx_->startWindWaveLoop(drawPosition, direction, projectile.radius).id;
    case ProjectileKind::DirtClod:
        return magicFx_->startDirtClodLoop(drawPosition, direction, projectile.radius).id;
    }
    return 0;
}

void MagicSystem::updateProjectileFx(MagicProjectile& projectile)
{
    if (projectile.fxEmitterId == 0 || magicFx_ == nullptr) {
        return;
    }
    if (projectile.kind == ProjectileKind::WindWave && projectile.lifetime > 0.0f && projectile.age / projectile.lifetime >= WindWaveFxFadeStart) {
        stopProjectileFx(projectile);
        return;
    }
    const Vec2 drawPosition = projectile.position + Vec2{0.0f, -projectile.height};
    if (!magicFx_->setEmitterPosition({projectile.fxEmitterId}, drawPosition)) {
        projectile.fxEmitterId = startProjectileFxEmitter(projectile);
    }
}

void MagicSystem::playProjectileImpactFx(const MagicProjectile& projectile)
{
    const Vec2 direction = lengthSquared(projectile.velocity) > 0.0001f ? normalize(projectile.velocity) : Vec2{1.0f, 0.0f};
    soundEvents_.push_back(MagicSoundEvent::Impact);
    switch (projectile.kind) {
    case ProjectileKind::Fireball:
        return;
    case ProjectileKind::IceShard:
        if (magicFx_ != nullptr) {
            magicFx_->playIceShatter(projectile.position, projectile.radius + 12.0f);
        }
        addGroundArea(MagicGroundArea{
            .kind = GroundKind::IceShatter,
            .position = projectile.position,
            .radius = projectile.radius + 12.0f,
            .age = 0.0f,
            .lifetime = IceShatterLightSeconds,
            .tickTimer = 0.0f,
            .damage = 0,
            .active = true,
            .triggered = false,
        });
        return;
    case ProjectileKind::WindWave:
        if (magicFx_ != nullptr) {
            magicFx_->playWindImpact(projectile.position, direction, projectile.radius);
        }
        return;
    case ProjectileKind::DirtClod:
        if (magicFx_ != nullptr) {
            magicFx_->playEarthDebrisBurst(projectile.position, projectile.radius + 6.0f);
        }
        addGroundArea(MagicGroundArea{
            .kind = GroundKind::EarthDebrisBreakLight,
            .position = projectile.position,
            .radius = projectile.radius + 6.0f,
            .age = 0.0f,
            .lifetime = EarthDebrisLightSeconds,
            .tickTimer = 0.0f,
            .damage = 0,
            .active = true,
            .triggered = false,
        });
        return;
    }
}

void MagicSystem::stopProjectileFx(MagicProjectile& projectile)
{
    if (projectile.fxEmitterId == 0 || magicFx_ == nullptr) {
        projectile.fxEmitterId = 0;
        return;
    }
    magicFx_->stopEmitter({projectile.fxEmitterId});
    projectile.fxEmitterId = 0;
}

void MagicSystem::spawnFirePatch(Vec2 position, float radius, int damage)
{
    if (magicFx_ != nullptr) {
        magicFx_->playFireGroundBurn(position, radius, 1.0f);
    }
    addGroundArea(MagicGroundArea{
        .kind = GroundKind::FirePatch,
        .position = position,
        .radius = radius,
        .age = 0.0f,
        .lifetime = 1.0f,
        .tickTimer = 0.0f,
        .damage = damage,
        .active = true,
        .triggered = false,
    });
    addGroundArea(MagicGroundArea{
        .kind = GroundKind::FireAfterglow,
        .position = position,
        .radius = radius,
        .age = 0.0f,
        .lifetime = FireAfterglowLightSeconds,
        .tickTimer = 0.0f,
        .damage = 0,
        .active = true,
        .triggered = false,
    });
}

void MagicSystem::addTransientLight(Vec2 position, float radius, float lifetime)
{
    if (radius <= 0.0f || lifetime <= 0.0f) {
        return;
    }
    if (transientLights_.size() >= MaxMagicTransientLights) {
        transientLights_.erase(transientLights_.begin());
    }
    transientLights_.push_back(MagicLight{
        .position = position,
        .radius = radius,
        .age = 0.0f,
        .lifetime = lifetime,
        .active = true,
    });
}

void MagicSystem::addProjectile(MagicProjectile projectile)
{
    if (projectiles_.size() >= MaxMagicProjectiles) {
        projectiles_.erase(projectiles_.begin());
    }
    projectiles_.push_back(projectile);
}

void MagicSystem::addGroundArea(MagicGroundArea area)
{
    if (groundAreas_.size() >= MaxMagicGroundAreas) {
        groundAreas_.erase(groundAreas_.begin());
    }
    groundAreas_.push_back(area);
}

}
