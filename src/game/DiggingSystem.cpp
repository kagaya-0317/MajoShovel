#include "game/DiggingSystem.hpp"

#include <algorithm>
#include <random>

namespace majo {

namespace {

constexpr float CapturedRewardChanceWall = 0.08f;
constexpr float CapturedRewardCooldown = 0.80f;
constexpr float CapturedRewardWindowSeconds = 10.0f;
constexpr int CapturedRewardWindowLimit = 3;
constexpr int CapturedExplosionChargeLimit = 4;

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
    const ObjectCatalog& objectCatalog,
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
        Vec2 digPosition = item.worldPosition;
        if (item.hasCapturedBehavior("dig_contact")) {
            const Vec2 outward = item.orbitOutward;
            const float probeDistance = static_cast<float>(std::max(4.0, item.capturedBehaviorParamDouble("dig_contact", "probeDistance", 4.0)));
            const Vec2 probe = item.worldPosition + outward * (item.hitRadius + probeDistance);
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

        const TileType impactTileType = map.terrainDebugAtWorld(digPosition).type;
        const ObjectDefinition* sourceObject = nullptr;
        if (!item.objectId.empty()) {
            const auto objectIt = objectCatalog.objectsById.find(item.objectId);
            if (objectIt != objectCatalog.objectsById.end()) {
                sourceObject = &objectIt->second;
            }
        }

        const std::size_t hitCountBefore = hitTiles_.size();
        const std::size_t openedCountBefore = openedTiles_.size();
        if (sourceObject != nullptr) {
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
            context.position = digPosition;
            context.triggerType = EffectTriggerType::Hit;
            context.logUnimplementedEffects = false;
            effectDispatcher.dispatchOrbitEffects(*sourceObject, context);
        }

        const bool terrainHit = hitTiles_.size() != hitCountBefore;
        const bool terrainOpened = openedTiles_.size() != openedCountBefore;
        if (terrainHit) {
            item.actionFlashTimer = SpellRingItemActionFlashSeconds;
            impactSoundEvents_.push_back(makeTerrainRingImpactSoundEvent(
                item,
                sourceObject,
                impactTileType,
                terrainOpened ? RingImpactResult::Break : RingImpactResult::Hit,
                digPosition,
                static_cast<float>(std::max(0, item.digPower))));
        }
        if (terrainHit && discoveryEvents != nullptr && sourceObject != nullptr) {
            std::string effectKey = "dig";
            if (std::any_of(
                    sourceObject->discoveryEffectLines.begin(),
                    sourceObject->discoveryEffectLines.end(),
                    [](const DiscoveryEffectLine& line) {
                        return line.effectKey == "dig_hard";
                    })) {
                effectKey = "dig_hard";
            }
            discoveryEvents->push_back(EffectDiscoveryEvent{
                .objectId = sourceObject->id,
                .objectName = sourceObject->name,
                .effectKey = effectKey,
                .description = "",
                .note = {},
                .position = digPosition,
            });
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
                recordCapturedReward(item, totalTime, digPosition, rewardDropRequests_);
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
                capturedExplosionRequests_.push_back(makeCapturedExplosionRequest(item, digPosition));
            }
        }
        if (terrainOpened) {
            spellRing.consumeItemDurability(item);
        }
        item.lastTerrainHitTime = totalTime;
    }
    spellRing.removeBrokenItems();
}

}
