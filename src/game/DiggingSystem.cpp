#include "game/DiggingSystem.hpp"

#include <random>

namespace majo {

namespace {

constexpr float CapturedRewardChanceWall = 0.08f;
constexpr float CapturedRewardCooldown = 0.80f;
constexpr float CapturedRewardWindowSeconds = 10.0f;
constexpr int CapturedRewardWindowLimit = 3;
constexpr int CapturedExplosionChargeLimit = 4;

bool capturedRewardAllowed(SpellRingItem& item, float totalTime)
{
    if (totalTime - item.capturedRewardLastTime < CapturedRewardCooldown) {
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

void recordCapturedReward(SpellRingItem& item, float totalTime, Vec2 position, std::vector<Vec2>& rewardDropRequests)
{
    item.capturedRewardLastTime = totalTime;
    if (totalTime - item.capturedRewardWindowStart > CapturedRewardWindowSeconds) {
        item.capturedRewardWindowStart = totalTime;
        item.capturedRewardWindowCount = 0;
    }
    ++item.capturedRewardWindowCount;
    rewardDropRequests.push_back(position);
}

bool damageDigContactTile(
    TileMap& map,
    SpellRingItem& item,
    int tileX,
    int tileY,
    std::vector<Vec2>& hitTiles,
    std::vector<Vec2>& openedTiles,
    std::vector<DugTile>& dugTiles)
{
    if (item.digPower <= 0 || !map.isTileSolid(tileX, tileY)) {
        return false;
    }

    const int damage = adjustedTerrainDigDamage(
        item.digPower,
        map.terrainAttributeAtTile(tileX, tileY),
        TerrainDigModifier::Normal);
    if (damage <= 0) {
        return false;
    }

    hitTiles.push_back(map.tileCenter(tileX, tileY));
    Vec2 openedTileCenter{};
    TileType openedTileType = TileType::Dirt;
    if (map.damageTile(tileX, tileY, damage, openedTileCenter, &openedTileType)) {
        openedTiles.push_back(openedTileCenter);
        dugTiles.push_back({openedTileCenter, openedTileType});
    }
    return true;
}

}

void DiggingSystem::update(
    TileMap& map,
    SpellRingSystem& spellRing,
    Player& player,
    float totalTime,
    const ObjectCatalog& objectCatalog,
    const EffectDispatcher& effectDispatcher,
    std::vector<EffectDiscoveryEvent>* discoveryEvents)
{
    openedTiles_.clear();
    hitTiles_.clear();
    dugTiles_.clear();
    rewardDropRequests_.clear();
    capturedExplosionRequests_.clear();
    for (auto& item : spellRing.items()) {
        if (item.broken()) {
            continue;
        }
        if (item.type != SpellRingItemType::Shovel && item.digPower <= 0 && item.objectId.empty()) {
            continue;
        }
        Vec2 digPosition = item.worldPosition;
        if (item.hasCapturedBehavior("dig_contact")) {
            const Vec2 outward = normalize(item.worldPosition - spellRing.center());
            const Vec2 probe = item.worldPosition + outward * (item.hitRadius + 4.0f);
            if (map.isTileSolid(map.worldToTile(probe.x), map.worldToTile(probe.y))) {
                digPosition = probe;
            }
        }
        const int tileX = map.worldToTile(digPosition.x);
        const int tileY = map.worldToTile(digPosition.y);
        if (tileX == item.lastDigTileX && tileY == item.lastDigTileY) {
            continue;
        }
        item.lastDigTileX = tileX;
        item.lastDigTileY = tileY;

        if (!map.isTileSolid(tileX, tileY)) {
            continue;
        }

        const std::size_t hitCountBefore = hitTiles_.size();
        if (!item.objectId.empty()) {
            const auto objectIt = objectCatalog.objectsById.find(item.objectId);
            if (objectIt != objectCatalog.objectsById.end()) {
                EffectContext context;
                context.sourceObject = &objectIt->second;
                context.owner = &player;
                context.orbit = &spellRing;
                context.orbitItem = &item;
                context.tileMap = &map;
                context.terrainHitTiles = &hitTiles_;
                context.terrainOpenedTiles = &openedTiles_;
                context.terrainDugTiles = &dugTiles_;
                context.discoveryEvents = discoveryEvents;
                context.position = digPosition;
                context.triggerType = EffectTriggerType::Hit;
                context.logUnimplementedEffects = false;
                effectDispatcher.dispatchOrbitEffects(objectIt->second, context);
            }
        }

        if (hitTiles_.size() == hitCountBefore && item.digPower > 0) {
            damageDigContactTile(map, item, tileX, tileY, hitTiles_, openedTiles_, dugTiles_);
            if (item.hasCapturedBehavior("dig_contact")) {
                const Vec2 outward = normalize(item.worldPosition - spellRing.center());
                const Vec2 extra = item.worldPosition + outward * (item.hitRadius + 12.0f);
                const int extraTileX = map.worldToTile(extra.x);
                const int extraTileY = map.worldToTile(extra.y);
                if (extraTileX != tileX || extraTileY != tileY) {
                    damageDigContactTile(map, item, extraTileX, extraTileY, hitTiles_, openedTiles_, dugTiles_);
                }
            }
        }
        if (hitTiles_.size() != hitCountBefore && discoveryEvents != nullptr && !item.objectId.empty()) {
            const auto objectIt = objectCatalog.objectsById.find(item.objectId);
            if (objectIt != objectCatalog.objectsById.end()) {
                discoveryEvents->push_back(EffectDiscoveryEvent{
                    .objectId = objectIt->second.id,
                    .objectName = objectIt->second.name,
                    .effectKey = "terrain_dig",
                    .description = "地形を掘削できる",
                    .position = digPosition,
                });
            }
        }
        if ((item.hasCapturedBehavior("reward_drop") || item.hasCapturedBehavior("steal_or_dig")) &&
            capturedRewardAllowed(item, totalTime) &&
            rollCapturedReward(CapturedRewardChanceWall)) {
            recordCapturedReward(item, totalTime, digPosition, rewardDropRequests_);
        }
        if (item.hasCapturedBehavior("charge_explode") && item.capturedExplodeSleepTimer <= 0.0f) {
            ++item.capturedExplodeCharge;
            if (item.capturedExplodeCharge >= CapturedExplosionChargeLimit) {
                item.capturedExplodeCharge = 0;
                item.capturedExplodeSleepTimer = 2.4f;
                capturedExplosionRequests_.push_back(digPosition);
            }
        }
        if (hitTiles_.size() != hitCountBefore) {
            item.consumeDurability();
        }
        item.lastTerrainHitTime = totalTime;
    }
    spellRing.removeBrokenItems();
}

}
