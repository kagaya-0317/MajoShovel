#include "game/DiggingSystem.hpp"

#include "game/Collision.hpp"
#include "game/Hitbox.hpp"
#include "game/RingItemHitbox.hpp"
#include "game/TerrainDigRules.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace majo {

namespace {

constexpr float CapturedRewardChanceWall = 0.08f;
constexpr float CapturedRewardCooldown = 0.80f;
constexpr float CapturedRewardWindowSeconds = 10.0f;
constexpr int CapturedRewardWindowLimit = 3;
constexpr int CapturedExplosionChargeLimit = 4;
constexpr int NoLastDigTile = 2147483647;
constexpr float TerrainContactInsetPx = 8.0f;

Vec2 rotateTerrainHitboxOffset(Vec2 value, float radians)
{
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return {
        value.x * c - value.y * s,
        value.x * s + value.y * c,
    };
}

CollisionRect terrainTileRect(int tileX, int tileY)
{
    const float tileSize = static_cast<float>(balance::TileSize);
    const float inset = std::min(TerrainContactInsetPx, tileSize * 0.5f - 1.0f);
    return {
        {static_cast<float>(tileX) * tileSize + inset, static_cast<float>(tileY) * tileSize + inset},
        {tileSize - inset * 2.0f, tileSize - inset * 2.0f},
    };
}

bool terrainTileRectContainsPoint(int tileX, int tileY, Vec2 point)
{
    const CollisionRect rect = terrainTileRect(tileX, tileY);
    return point.x >= rect.pos.x &&
        point.y >= rect.pos.y &&
        point.x <= rect.pos.x + rect.size.x &&
        point.y <= rect.pos.y + rect.size.y;
}

bool containsTerrainTile(const std::vector<DungeonTile>& tiles, int tileX, int tileY)
{
    return std::any_of(
        tiles.begin(),
        tiles.end(),
        [tileX, tileY](const DungeonTile& tile) {
            return tile.x == tileX && tile.y == tileY;
        });
}

bool wasLastDigTile(const SpellRingItem& item, const DungeonTile& tile)
{
    return std::any_of(
        item.lastDigTiles.begin(),
        item.lastDigTiles.end(),
        [&tile](const std::pair<int, int>& previous) {
            return previous.first == tile.x && previous.second == tile.y;
        });
}

void addTerrainHitTile(std::vector<DungeonTile>& tiles, int tileX, int tileY)
{
    if (!containsTerrainTile(tiles, tileX, tileY)) {
        tiles.push_back({tileX, tileY});
    }
}

EffectSpec makeImplicitTerrainDigSpec(const TerrainDigProfile& profile)
{
    EffectSpec spec;
    spec.target = "terrain";
    spec.effects.push_back(std::string(terrainDigEffectForMode(profile.mode)));
    spec.values.push_back(static_cast<double>(profile.power));
    spec.duration = 0.0;
    return spec;
}

std::vector<EffectSpec> terrainContactEffectsExcludingDig(const std::vector<EffectSpec>& specs)
{
    std::vector<EffectSpec> filtered;
    for (const EffectSpec& spec : specs) {
        if (!isTerrainDigTarget(spec.target)) {
            continue;
        }

        EffectSpec copy;
        copy.target = spec.target;
        copy.duration = spec.duration;
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            const std::string& effect = spec.effects[index];
            if (isTerrainDigEffect(effect)) {
                continue;
            }
            copy.effects.push_back(effect);
            copy.values.push_back(index < spec.values.size() ? spec.values[index] : 0.0);
        }
        if (!copy.effects.empty()) {
            filtered.push_back(std::move(copy));
        }
    }
    return filtered;
}

std::string terrainDiscoveryEffectKey(TerrainDigMode mode)
{
    return mode == TerrainDigMode::HardSpecialist ? "dig_hard" : "dig";
}

void rememberDigTiles(SpellRingItem& item, const std::vector<DungeonTile>& tiles)
{
    item.lastDigTiles.clear();
    item.lastDigTiles.reserve(tiles.size());
    for (const DungeonTile& tile : tiles) {
        item.lastDigTiles.push_back({tile.x, tile.y});
    }

    if (tiles.empty()) {
        item.lastDigTileX = NoLastDigTile;
        item.lastDigTileY = NoLastDigTile;
        return;
    }

    item.lastDigTileX = tiles.front().x;
    item.lastDigTileY = tiles.front().y;
}

void collectCircleTerrainHitTiles(
    TileMap& map,
    Vec2 center,
    float radius,
    std::vector<DungeonTile>& outTiles)
{
    const float safeRadius = std::max(0.0f, radius);
    const int minTileX = map.worldToTile(center.x - safeRadius);
    const int maxTileX = map.worldToTile(center.x + safeRadius);
    const int minTileY = map.worldToTile(center.y - safeRadius);
    const int maxTileY = map.worldToTile(center.y + safeRadius);
    for (int tileY = minTileY; tileY <= maxTileY; ++tileY) {
        for (int tileX = minTileX; tileX <= maxTileX; ++tileX) {
            if (!map.isTileSolid(tileX, tileY)) {
                continue;
            }
            if (!circleIntersectsRect(center, safeRadius, terrainTileRect(tileX, tileY))) {
                continue;
            }
            addTerrainHitTile(outTiles, tileX, tileY);
        }
    }
}

void addDigContactProbeTile(TileMap& map, const SpellRingItem& item, std::vector<DungeonTile>& outTiles)
{
    if (!item.hasCapturedBehavior("dig_contact")) {
        return;
    }

    const float probeDistance =
        static_cast<float>(std::max(4.0, item.capturedBehaviorParamDouble("dig_contact", "probeDistance", 4.0)));
    const Vec2 probe = item.worldPosition + item.orbitOutward * (item.hitRadius + probeDistance);
    const int tileX = map.worldToTile(probe.x);
    const int tileY = map.worldToTile(probe.y);
    if (map.isTileSolid(tileX, tileY) && terrainTileRectContainsPoint(tileX, tileY, probe)) {
        addTerrainHitTile(outTiles, tileX, tileY);
    }
}

std::vector<DungeonTile> collectTerrainHitTiles(
    TileMap& map,
    const SpellRingItem& item,
    const ObjectDefinition* object,
    const HitboxCatalog* hitboxCatalog,
    const SpellRingSystem& spellRing,
    float totalTime,
    float dt)
{
    std::vector<DungeonTile> tiles;
    const RingItemHitbox hitbox = resolveRingItemHitbox(
        item,
        object,
        hitboxCatalog,
        spellRing,
        totalTime,
        dt);
    if (hitbox.profile != nullptr && object != nullptr) {
        visitRingItemHitboxSweep(
            hitbox,
            [&](Vec2 center, float rotationRadians) {
                for (const HitCircle& circle : hitbox.profile->circles) {
                    const Vec2 circleCenter = center + rotateTerrainHitboxOffset(
                        circle.offset * hitbox.profileScale,
                        rotationRadians);
                    collectCircleTerrainHitTiles(
                        map,
                        circleCenter,
                        circle.radius * hitbox.profileScale,
                        tiles);
                }
            });
    } else {
        visitRingItemHitboxSweep(
            hitbox,
            [&](Vec2 center, float) {
                collectCircleTerrainHitTiles(
                    map,
                    center,
                    hitbox.fallbackCircleRadius,
                    tiles);
            });
    }

    addDigContactProbeTile(map, item, tiles);
    return tiles;
}

CapturedExplosionRequest makeCapturedExplosionRequest(const SpellRingItem& item, Vec2 position)
{
    CapturedExplosionRequest request;
    request.position = position;
    request.radius = static_cast<float>(std::max(8.0, item.capturedBehaviorParamDouble("charge_explode", "radius", request.radius)));
    request.damage = std::max(0, item.capturedBehaviorParamInt("charge_explode", "damage", request.damage));
    request.terrainRadius = static_cast<float>(std::max(0.0, item.capturedBehaviorParamDouble("charge_explode", "terrainRadius", request.terrainRadius)));
    request.terrainDamage = std::max(0, item.capturedBehaviorParamInt("charge_explode", "terrainDamage", request.terrainDamage));
    return request;
}

bool capturedRewardAllowed(SpellRingItem& item, float totalTime)
{
    float interval = CapturedRewardCooldown;
    if (item.hasCapturedBehavior("reward_drop")) {
        interval = std::max(interval, static_cast<float>(item.capturedBehaviorInterval("reward_drop", CapturedRewardCooldown)));
    }
    if (item.hasCapturedBehavior("steal_or_dig")) {
        interval = std::max(interval, static_cast<float>(item.capturedBehaviorInterval("steal_or_dig", CapturedRewardCooldown)));
    }
    if (totalTime - item.capturedRewardLastTime < interval) {
        return false;
    }
    if (totalTime - item.capturedRewardWindowStart > CapturedRewardWindowSeconds) {
        item.capturedRewardWindowStart = totalTime;
        item.capturedRewardWindowCount = 0;
    }
    if (item.capturedRewardWindowCount >= CapturedRewardWindowLimit) {
        return false;
    }
    return true;
}

bool rollCapturedReward(float chance)
{
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng) <= chance;
}

std::string capturedRewardProfile(const SpellRingItem& item)
{
    if (item.hasCapturedBehavior("reward_drop")) {
        return item.capturedBehaviorParamString("reward_drop", "profile", "common");
    }
    if (item.hasCapturedBehavior("steal_or_dig")) {
        return item.capturedBehaviorParamString("steal_or_dig", "profile", "common");
    }
    return "common";
}

void recordCapturedReward(
    SpellRingItem& item,
    float totalTime,
    Vec2 position,
    std::vector<CapturedRewardDropRequest>& rewardDropRequests)
{
    item.capturedRewardLastTime = totalTime;
    if (totalTime - item.capturedRewardWindowStart > CapturedRewardWindowSeconds) {
        item.capturedRewardWindowStart = totalTime;
        item.capturedRewardWindowCount = 0;
    }
    ++item.capturedRewardWindowCount;
    rewardDropRequests.push_back({
        .position = position,
        .profile = capturedRewardProfile(item),
    });
}

}

void DiggingSystem::update(
    TileMap& map,
    SpellRingSystem& spellRing,
    Player& player,
    float totalTime,
    float dt,
    const ObjectCatalog& objectCatalog,
    const HitboxCatalog* hitboxCatalog,
    const EffectDispatcher& effectDispatcher,
    MagicSystem* magic,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    openedTiles_.clear();
    hitTiles_.clear();
    dugTiles_.clear();
    impactSoundEvents_.clear();
    rewardDropRequests_.clear();
    capturedExplosionRequests_.clear();
    std::vector<SpellRingItem*> runtimeItems = spellRing.runtimeItemsMutable();
    for (SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        SpellRingItem& item = *itemPtr;
        if (item.broken()) {
            continue;
        }
        if (item.objectId.empty()) {
            continue;
        }
        const ObjectDefinition* sourceObject = nullptr;
        if (!item.objectId.empty()) {
            const auto objectIt = objectCatalog.objectsById.find(item.objectId);
            if (objectIt != objectCatalog.objectsById.end()) {
                sourceObject = &objectIt->second;
            }
        }

        const std::vector<DungeonTile> currentTargets =
            collectTerrainHitTiles(
                map,
                item,
                sourceObject,
                hitboxCatalog,
                spellRing,
                totalTime,
                dt);
        if (currentTargets.empty()) {
            rememberDigTiles(item, currentTargets);
            continue;
        }

        std::vector<DungeonTile> newTargets;
        newTargets.reserve(currentTargets.size());
        for (const DungeonTile& target : currentTargets) {
            if (!wasLastDigTile(item, target)) {
                newTargets.push_back(target);
            }
        }
        rememberDigTiles(item, currentTargets);
        if (newTargets.empty()) {
            continue;
        }

        bool anyTerrainHit = false;
        int terrainHitCount = 0;
        bool discoveryRecorded = false;
        bool capturedBehaviorDiscoveryRecorded = false;
        const Vec2 firstHitPosition = map.tileCenter(newTargets.front().x, newTargets.front().y);
        if (sourceObject != nullptr) {
            const TerrainDigProfile digProfile = terrainDigProfileFor(sourceObject, &item);
            const std::vector<EffectSpec> implicitDigEffects = digProfile.enabled
                ? std::vector<EffectSpec>{makeImplicitTerrainDigSpec(digProfile)}
                : std::vector<EffectSpec>{};
            const std::vector<EffectSpec> nonDigTerrainContactEffects =
                terrainContactEffectsExcludingDig(sourceObject->orbitEffects);
            for (const DungeonTile& target : newTargets) {
                const Vec2 hitPosition = map.tileCenter(target.x, target.y);
                const TileType impactTileType = map.terrainDebugAtWorld(hitPosition).type;
                const std::size_t hitCountBefore = hitTiles_.size();
                const std::size_t openedCountBefore = openedTiles_.size();

                EffectContext context;
                context.sourceObject = sourceObject;
                context.owner = &player;
                context.orbit = &spellRing;
                context.orbitItem = &item;
                context.tileMap = &map;
                context.magic = magic;
                context.terrainHitTiles = &hitTiles_;
                context.terrainOpenedTiles = &openedTiles_;
                context.terrainDugTiles = &dugTiles_;
                context.discoveryEvents = discoveryEvents;
                context.encyclopedia = encyclopedia;
                context.position = hitPosition;
                context.terrainHitTile = target;
                context.triggerType = EffectTriggerType::Hit;
                context.logUnimplementedEffects = false;
                if (!implicitDigEffects.empty()) {
                    effectDispatcher.dispatch(implicitDigEffects, context);
                }
                if (!nonDigTerrainContactEffects.empty()) {
                    effectDispatcher.dispatch(nonDigTerrainContactEffects, context);
                }

                const bool terrainHit = hitTiles_.size() != hitCountBefore;
                const bool terrainOpened = openedTiles_.size() != openedCountBefore;
                terrainHitCount += static_cast<int>(hitTiles_.size() - hitCountBefore);
                anyTerrainHit = anyTerrainHit || terrainHit;
                if (terrainHit) {
                    triggerActionFlash(item.actionFlash);
                    impactSoundEvents_.push_back(makeTerrainRingImpactSoundEvent(
                        item,
                        sourceObject,
                        impactTileType,
                        terrainOpened ? RingImpactResult::Break : RingImpactResult::Hit,
                        hitPosition,
                        static_cast<float>(std::max(0, item.digPower))));
                }
                if (terrainHit && !discoveryRecorded && discoveryEvents != nullptr) {
                    const std::string effectKey = terrainDiscoveryEffectKey(digProfile.mode);
                    queueObjectEffectDiscovery(discoveryEvents, *sourceObject, effectKey, hitPosition);
                    discoveryRecorded = true;
                }
                if (terrainHit && !capturedBehaviorDiscoveryRecorded && item.hasCapturedBehavior("dig_contact")) {
                    queueObjectEffectDiscovery(discoveryEvents, *sourceObject, "dig_contact", hitPosition);
                    capturedBehaviorDiscoveryRecorded = true;
                }
            }
        }
        if (!anyTerrainHit) {
            continue;
        }
        if ((item.hasCapturedBehavior("reward_drop") || item.hasCapturedBehavior("steal_or_dig")) &&
            capturedRewardAllowed(item, totalTime)) {
            const double rewardChance = item.hasCapturedBehavior("steal_or_dig")
                ? item.capturedBehaviorParamDouble(
                      "steal_or_dig",
                      "digChance",
                      item.capturedBehaviorParamDouble("steal_or_dig", "chance", CapturedRewardChanceWall))
                : item.capturedBehaviorParamDouble("reward_drop", "chance", CapturedRewardChanceWall);
            if (rollCapturedReward(static_cast<float>(std::clamp(rewardChance, 0.0, 1.0)))) {
                recordCapturedReward(item, totalTime, firstHitPosition, rewardDropRequests_);
                if (sourceObject != nullptr) {
                    queueObjectEffectDiscovery(
                        discoveryEvents,
                        *sourceObject,
                        item.hasCapturedBehavior("steal_or_dig") ? "steal_or_dig" : "reward_drop",
                        firstHitPosition);
                }
            }
        }
        if (item.hasCapturedBehavior("charge_explode") && item.capturedExplodeSleepTimer <= 0.0f) {
            const int requiredHits = std::max(
                1,
                item.capturedBehaviorParamInt(
                    "charge_explode",
                    "count",
                    item.capturedBehaviorParamInt("charge_explode", "charges", CapturedExplosionChargeLimit)));
            const float restSeconds = static_cast<float>(std::max(0.1, item.capturedBehaviorParamDouble("charge_explode", "rest", 2.4)));
            ++item.capturedExplodeCharge;
            if (item.capturedExplodeCharge >= requiredHits) {
                item.capturedExplodeCharge = 0;
                item.capturedExplodeSleepTimer = restSeconds;
                capturedExplosionRequests_.push_back(makeCapturedExplosionRequest(item, firstHitPosition));
                if (sourceObject != nullptr) {
                    queueObjectEffectDiscovery(discoveryEvents, *sourceObject, "charge_explode", firstHitPosition);
                }
            }
        }
        if (terrainHitCount > 0) {
            spellRing.consumeItemDurability(item, terrainHitCount * TerrainHitDurabilityCostUnits);
        }
        item.lastTerrainHitTime = totalTime;
    }
    spellRing.removeBrokenItems();
}

}
