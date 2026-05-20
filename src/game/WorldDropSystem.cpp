#include "game/WorldDropSystem.hpp"

#include "data/GameBalance.hpp"
#include "engine/Log.hpp"
#include "game/EffectSystem.hpp"
#include "game/ActorVisual.hpp"
#include "game/InventorySystem.hpp"
#include "game/ObjectImageRenderer.hpp"
#include "game/ObjectVisualPose.hpp"
#include "game/Player.hpp"
#include "game/WorldIconRenderer.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <random>
#include <sstream>
#include <string_view>
#include <utility>

namespace majo {

namespace {
constexpr float DropPickupRadius = 23.0f;
constexpr float DropVisualRadius = 8.0f;
constexpr Vec2 DropObjectImageMaxSize = {48.0f, 48.0f};
constexpr float DropFallbackShadowVisualSize = DropVisualRadius * 2.5f;
constexpr float DropHoverBaseAltitude = 4.0f;
constexpr float DropHoverAmplitude = 2.0f;
constexpr float DropHoverSpeed = 4.4f;
constexpr float MaterialHoverBaseAltitude = 8.0f;
constexpr float MaterialHoverAmplitude = 3.0f;
constexpr float MaterialHoverSpeed = 4.8f;
constexpr float MaterialParticleInterval = 0.16f;
constexpr float CapturedMagnetDropRadius = 170.0f;
constexpr float CapturedMagnetDropAcceleration = 260.0f;
constexpr int CapturedMagnetDropLimit = 6;
constexpr float VacuumLightDropAcceleration = 220.0f;
constexpr float VacuumLightObjectMaxWeightKg = 1.0f;
constexpr int VacuumLightDropLimit = 10;
constexpr float WindLightDropAcceleration = 245.0f;
constexpr int WindLightDropLimit = 10;
constexpr std::string_view MoneyDropId = "money";

constexpr std::array<std::string_view, 6> DropCategories = {
    "\xE6\x8E\x98\xE5\x89\x8A",
    "\xE6\x8E\xA2\xE7\xB4\xA2",
    "\xE5\x9B\x9E\xE5\xBE\xA9",
    "\xE6\xAD\xA6\xE5\x99\xA8",
    "\xE7\x9B\xBE",
    "\xE5\xAE\x9D",
};

constexpr std::array<std::string_view, 11> DropTags = {
    "drop",
    "dig_drop",
    "drop_dig",
    "ore_drop",
    "treasure_drop",
    "mining",
    "loot",
    "treasure",
    "\xE3\x83\x89\xE3\x83\xAD\xE3\x83\x83\xE3\x83\x97",
    "\xE6\x8E\x98\xE5\x89\x8A\xE3\x83\x89\xE3\x83\xAD\xE3\x83\x83\xE3\x83\x97",
    "\xE5\xAE\x9D",
};

constexpr std::array<std::string_view, 4> ExcludedDropTags = {
    "no_drop",
    "nodrop",
    "shop_only",
    "\xE3\x82\xB7\xE3\x83\xA7\xE3\x83\x83\xE3\x83\x97\xE5\xB0\x82\xE7\x94\xA8",
};

std::mt19937& dropRng()
{
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

float randomDropRange(float minValue, float maxValue)
{
    std::uniform_real_distribution<float> distribution(minValue, maxValue);
    return distribution(dropRng());
}

std::string trim(std::string_view text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

bool containsValue(const auto& values, std::string_view value)
{
    return std::any_of(values.begin(), values.end(), [value](std::string_view candidate) {
        return candidate == value;
    });
}

bool hasAnyTag(const ObjectDefinition& object, const auto& tags)
{
    return std::any_of(object.tags.begin(), object.tags.end(), [&tags](const std::string& objectTag) {
        return containsValue(tags, objectTag);
    });
}

bool hasObjectTag(const ObjectDefinition& object, std::string_view tag)
{
    return std::any_of(object.tags.begin(), object.tags.end(), [tag](const std::string& objectTag) {
        return objectTag == tag;
    });
}

bool filterContainsToken(std::string_view filterText, std::string_view token)
{
    std::string current;
    auto flush = [&]() {
        const std::string trimmed = trim(current);
        current.clear();
        return trimmed;
    };
    for (char ch : filterText) {
        if (ch == '|') {
            if (flush() == token) {
                return true;
            }
            continue;
        }
        current.push_back(ch);
    }
    return flush() == token;
}

bool isTreasureDrop(const WorldDropItem& drop, const ObjectCatalog& catalog)
{
    if (drop.kind != WorldDropKind::Object) {
        return false;
    }
    const ObjectDefinition* object = catalog.registry.findById(drop.id);
    if (object == nullptr) {
        return false;
    }
    return object->category == "\xE5\xAE\x9D" || hasObjectTag(*object, "treasure");
}

Color colorForDrop(const ObjectDefinition& object);
Color colorForMaterial(MaterialType type);

void drawWorldDrop(Renderer& renderer, const WorldDropItem& drop, const ObjectCatalog& catalog)
{
    const ItemData* object = drop.kind == WorldDropKind::Object ? catalog.registry.findById(drop.id) : nullptr;
    const Vec2 center = elevatedDrawPosition(drop.position, drop.altitude);
    MaterialType materialType = MaterialType::Count;
    const bool material = drop.kind == WorldDropKind::Material && materialTypeFromSaveName(drop.id, materialType);
    Color color = {255, 80, 120, 255};
    if (object != nullptr) {
        color = colorForDrop(*object);
    } else if (drop.kind == WorldDropKind::Money) {
        color = {245, 206, 76, 255};
    } else if (material) {
        color = colorForMaterial(materialType);
    }

    bool drewImage = false;
    if (object != nullptr) {
        drewImage = drawObjectImage(
            renderer,
            *object,
            center,
            DropObjectImageMaxSize,
            objectGroundImageOptions(*object));
    } else if (drop.kind == WorldDropKind::Money) {
        drewImage = drawWorldIcon(renderer, moneyWorldIconForAmount(drop.quantity), center, DropObjectImageMaxSize);
    } else if (material) {
        drewImage = drawWorldIcon(renderer, materialWorldIcon(materialType), center, DropObjectImageMaxSize);
    }

    if (!drewImage) {
        renderer.fillCircle(center, DropVisualRadius, color);
        renderer.drawCircle(center, DropVisualRadius + 3.0f, {255, 246, 190, 210});
    }

}

float dropShadowVisualSize(const WorldDropItem& drop, const ObjectCatalog& catalog)
{
    if (drop.kind == WorldDropKind::Object && catalog.registry.findById(drop.id) != nullptr) {
        return std::max(DropObjectImageMaxSize.x, DropObjectImageMaxSize.y);
    }
    if (drop.kind == WorldDropKind::Money) {
        return std::max(DropObjectImageMaxSize.x, DropObjectImageMaxSize.y);
    }
    MaterialType materialType = MaterialType::Count;
    if (drop.kind == WorldDropKind::Material && materialTypeFromSaveName(drop.id, materialType)) {
        return std::max(DropObjectImageMaxSize.x, DropObjectImageMaxSize.y);
    }
    return DropFallbackShadowVisualSize;
}

void drawWorldDropShadow(Renderer& renderer, const WorldDropItem& drop, const ObjectCatalog& catalog)
{
    renderer.drawActorShadow(
        actorShadowAnchor(drop.position, ItemShadowGroundOffsetY),
        actorShadowVisualSizeForAltitude(dropShadowVisualSize(drop, catalog), drop.altitude));
}

bool isDropStealTarget(const WorldDropItem& drop, const ObjectCatalog& catalog, std::string_view targetFilter)
{
    if (targetFilter.empty()) {
        return true;
    }

    const bool allowAny = filterContainsToken(targetFilter, "drop");
    const bool allowMoney = filterContainsToken(targetFilter, "money");
    const bool allowTreasure = filterContainsToken(targetFilter, "treasure");
    if (allowAny) {
        return true;
    }
    if (allowMoney && drop.kind == WorldDropKind::Money) {
        return true;
    }
    if (allowTreasure && isTreasureDrop(drop, catalog)) {
        return true;
    }
    return false;
}

bool objectDropCanEnterInventory(const WorldDropItem& drop, const ObjectCatalog& catalog, const InventorySystem* inventory)
{
    return drop.kind != WorldDropKind::Object || inventory == nullptr || inventory->canAddObjectItem(catalog, drop.id);
}

bool isLightObjectDrop(const ObjectDefinition& object)
{
    constexpr std::array<std::string_view, 2> LightTags = {"small", "light"};
    constexpr std::array<std::string_view, 4> HeavyTags = {"heavy", "large", "massive", "huge"};
    if (hasAnyTag(object, HeavyTags)) {
        return false;
    }
    if (hasAnyTag(object, LightTags)) {
        return true;
    }
    return object.weightKg > 0.0 && object.weightKg <= VacuumLightObjectMaxWeightKg;
}

bool isLightDrop(const WorldDropItem& drop, const ObjectCatalog& catalog)
{
    if (drop.kind == WorldDropKind::Money || drop.kind == WorldDropKind::Material) {
        return true;
    }
    if (drop.kind != WorldDropKind::Object) {
        return false;
    }
    const ObjectDefinition* object = catalog.registry.findById(drop.id);
    return object != nullptr && isLightObjectDrop(*object);
}

bool hasAllowedCategory(const ObjectDefinition& object)
{
    return containsValue(DropCategories, object.category);
}

bool canDropFromTile(TileType tileType)
{
    return tileType == TileType::Ore;
}

int rarityWeight(const ObjectDefinition& object)
{
    const int rarity = std::clamp(object.rarity, 1, 10);
    return std::max(1, 11 - rarity);
}

Color colorForDrop(const ObjectDefinition& object)
{
    if (object.category == "\xE5\x9B\x9E\xE5\xBE\xA9") {
        return {116, 220, 144, 255};
    }
    if (object.category == "\xE6\xAD\xA6\xE5\x99\xA8") {
        return {224, 96, 86, 255};
    }
    if (object.category == "\xE7\x9B\xBE") {
        return {104, 168, 226, 255};
    }
    if (object.category == "\xE5\xAE\x9D") {
        return {244, 206, 78, 255};
    }
    if (object.category == "\xE6\x8E\xA2\xE7\xB4\xA2") {
        return {136, 214, 214, 255};
    }
    return {188, 152, 236, 255};
}

Color colorForMaterial(MaterialType type)
{
    switch (type) {
    case MaterialType::OldWoodBuildingMaterial:
        return {184, 126, 70, 255};
    case MaterialType::EnhancementOre:
        return {104, 152, 224, 255};
    case MaterialType::MoonFragment:
        return {238, 224, 146, 255};
    case MaterialType::ManaDrop:
        return {112, 224, 226, 255};
    case MaterialType::Count:
        break;
    }
    return {188, 188, 196, 255};
}

Color materialParticleColor(MaterialType type)
{
    switch (type) {
    case MaterialType::OldWoodBuildingMaterial:
        return {96, 232, 122, 190};
    case MaterialType::EnhancementOre:
        return {255, 154, 64, 190};
    case MaterialType::MoonFragment:
        return {255, 230, 86, 190};
    case MaterialType::ManaDrop:
        return {188, 104, 255, 190};
    case MaterialType::Count:
        break;
    }
    return {210, 210, 220, 180};
}

float dropHoverAltitude(const WorldDropItem& drop)
{
    if (drop.hoverAmplitude <= 0.0f || drop.hoverSpeed <= 0.0f) {
        return std::max(0.0f, drop.hoverBaseAltitude);
    }
    return std::max(
        0.0f,
        drop.hoverBaseAltitude + std::sin(drop.ageSeconds * drop.hoverSpeed + drop.hoverPhase) * drop.hoverAmplitude);
}

void configureDropMotion(WorldDropItem& drop, const WorldDropSpawnMotion& motion)
{
    MaterialType materialType = MaterialType::Count;
    const bool material = drop.kind == WorldDropKind::Material && materialTypeFromSaveName(drop.id, materialType);
    drop.hoverBaseAltitude = material ? MaterialHoverBaseAltitude : DropHoverBaseAltitude;
    drop.hoverAmplitude = material ? MaterialHoverAmplitude : DropHoverAmplitude;
    drop.hoverSpeed = material ? MaterialHoverSpeed : DropHoverSpeed;
    drop.hoverPhase = randomDropRange(0.0f, Pi * 2.0f);
    drop.materialParticleTimer = material ? randomDropRange(0.02f, MaterialParticleInterval) : 0.0f;

    if (motion.jump && motion.jumpDurationSeconds > 0.0f && std::isfinite(motion.jumpDurationSeconds)) {
        const Vec2 targetPosition = drop.position;
        drop.position = motion.startPosition;
        drop.velocity = {};
        drop.jumpActive = true;
        drop.jumpStartPosition = motion.startPosition;
        drop.jumpTargetPosition = targetPosition;
        drop.jumpElapsedSeconds = 0.0f;
        drop.jumpDurationSeconds = std::max(0.05f, motion.jumpDurationSeconds);
        drop.jumpArcHeight = std::max(0.0f, motion.jumpArcHeight);
        drop.pickupDelaySeconds = std::max(0.0f, motion.pickupDelaySeconds);
    }

    drop.altitude = dropHoverAltitude(drop);
}

void logDropWarning(std::string_view message)
{
    logError("[warning] WorldDropSystem: " + std::string(message));
}
}

void WorldDropSystem::clear()
{
    drops_.clear();
}

void WorldDropSystem::setDropLimit(int limit)
{
    dropLimit_ = std::max(0, limit);
    if (dropLimit_ > 0 && static_cast<int>(drops_.size()) > dropLimit_) {
        while (static_cast<int>(drops_.size()) > dropLimit_) {
            if (!pruneOneDropForLimit()) {
                break;
            }
        }
    }
}

void WorldDropSystem::restoreDropsForSave(std::vector<WorldDropItem> drops)
{
    drops_.clear();
    drops_.reserve(drops.size());
    for (WorldDropItem& drop : drops) {
        if (drop.id.empty() || drop.quantity <= 0) {
            continue;
        }
        drop.velocity = {};
        drop.jumpActive = false;
        drop.jumpElapsedSeconds = 0.0f;
        drop.jumpDurationSeconds = 0.0f;
        drop.jumpArcHeight = 0.0f;
        drop.pickupDelaySeconds = 0.0f;
        configureDropMotion(drop, {});
        drop.altitude = dropHoverAltitude(drop);
        drops_.push_back(std::move(drop));
    }
    if (dropLimit_ > 0) {
        while (static_cast<int>(drops_.size()) > dropLimit_) {
            if (!pruneOneDropForLimit()) {
                break;
            }
        }
    }
}

void WorldDropSystem::spawnFromDugTiles(const std::vector<DugTile>& dugTiles, const ObjectCatalog& catalog, float spawnedAtSeconds)
{
    for (const DugTile& dugTile : dugTiles) {
        if (!canDropFromTile(dugTile.type)) {
            continue;
        }

        const ObjectDefinition* object = chooseDropForTile(dugTile.type, catalog);
        if (object != nullptr) {
            spawnDrop(*object, dugTile.center, spawnedAtSeconds);
        }
    }
}

bool WorldDropSystem::spawnObjectDrop(
    const ObjectCatalog& catalog,
    std::string_view objectId,
    Vec2 position,
    float spawnedAtSeconds,
    WorldDropSpawnMotion motion)
{
    if (objectId.empty()) {
        logDropWarning("reward node requested an empty object_id; no item drop spawned");
        return false;
    }

    const ObjectDefinition* object = catalog.registry.findById(objectId);
    if (object == nullptr) {
        logDropWarning("reward node object_id=\"" + std::string(objectId) + "\" is missing; no item drop spawned");
        return false;
    }

    spawnDrop(*object, position, spawnedAtSeconds, motion);
    return true;
}

bool WorldDropSystem::spawnDigItemDrop(const ObjectCatalog& catalog, Vec2 position, float spawnedAtSeconds)
{
    const ObjectDefinition* object = chooseDigDrop(catalog, "dig event");
    if (object == nullptr) {
        return false;
    }
    spawnDrop(*object, position, spawnedAtSeconds);
    return true;
}

bool WorldDropSystem::spawnMoneyDrop(int amount, Vec2 position, float spawnedAtSeconds, WorldDropSpawnMotion motion)
{
    if (amount <= 0) {
        return false;
    }
    if (!canSpawnDrop("money")) {
        return false;
    }

    WorldDropItem drop{
        .kind = WorldDropKind::Money,
        .id = std::string(MoneyDropId),
        .quantity = amount,
        .position = position,
        .velocity = randomDropVelocity(),
        .spawnedAtSeconds = spawnedAtSeconds,
        .ageSeconds = 0.0f,
    };
    configureDropMotion(drop, motion);
    drops_.push_back(std::move(drop));
    return true;
}

bool WorldDropSystem::spawnMaterialDrop(
    MaterialType type,
    int count,
    Vec2 position,
    float spawnedAtSeconds,
    WorldDropSpawnMotion motion)
{
    if (type == MaterialType::Count || count <= 0) {
        return false;
    }
    if (!canSpawnDrop("material")) {
        return false;
    }

    WorldDropItem drop{
        .kind = WorldDropKind::Material,
        .id = std::string(materialTypeSaveName(type)),
        .quantity = count,
        .position = position,
        .velocity = randomDropVelocity(),
        .spawnedAtSeconds = spawnedAtSeconds,
        .ageSeconds = 0.0f,
    };
    configureDropMotion(drop, motion);
    drops_.push_back(std::move(drop));
    return true;
}

bool WorldDropSystem::spawnRewardDrop(const ObjectCatalog& catalog, Vec2 position, float spawnedAtSeconds)
{
    const ObjectDefinition* object = chooseDropForTile(TileType::Ore, catalog);
    if (object == nullptr) {
        return false;
    }
    spawnDrop(*object, position, spawnedAtSeconds);
    return true;
}

bool WorldDropSystem::stealNearestDrop(const ObjectCatalog& catalog, Vec2 center, float radius, std::string_view targetFilter, WorldDropItem& outDrop)
{
    if (drops_.empty() || radius <= 0.0f) {
        return false;
    }

    std::size_t nearestIndex = drops_.size();
    float nearestDistanceSq = radius * radius;
    for (std::size_t i = 0; i < drops_.size(); ++i) {
        const WorldDropItem& drop = drops_[i];
        if (!isDropStealTarget(drop, catalog, targetFilter)) {
            continue;
        }
        const float distanceSq = lengthSquared(drop.position - center);
        if (distanceSq > nearestDistanceSq) {
            continue;
        }
        nearestDistanceSq = distanceSq;
        nearestIndex = i;
    }

    if (nearestIndex >= drops_.size()) {
        return false;
    }

    outDrop = drops_[nearestIndex];
    drops_.erase(drops_.begin() + static_cast<std::ptrdiff_t>(nearestIndex));
    return true;
}

int WorldDropSystem::pullNearbyDrops(
    Vec2 center,
    float dt,
    float radius,
    float acceleration,
    int limit,
    const InventorySystem* inventory,
    const ObjectCatalog* catalog)
{
    if (dt <= 0.0f || radius <= 0.0f || acceleration <= 0.0f) {
        return 0;
    }

    int pulled = 0;
    const float effectiveRadius = std::max(DropPickupRadius, radius);
    const float radiusSq = effectiveRadius * effectiveRadius;
    for (WorldDropItem& drop : drops_) {
        if (drop.jumpActive || drop.pickupDelaySeconds > 0.0f) {
            continue;
        }
        if (catalog != nullptr && !objectDropCanEnterInventory(drop, *catalog, inventory)) {
            continue;
        }
        const Vec2 toCenter = center - drop.position;
        const float distanceSq = lengthSquared(toCenter);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }
        const float distance = std::sqrt(distanceSq);
        const float falloff = 0.55f + (1.0f - clamp(distance / effectiveRadius, 0.0f, 1.0f)) * 0.45f;
        drop.velocity += normalize(toCenter) * (acceleration * falloff * dt);
        ++pulled;
        if (limit > 0 && pulled >= limit) {
            break;
        }
    }
    return pulled;
}

int WorldDropSystem::pullMetalDrops(const ObjectCatalog& catalog, Vec2 center, float dt, float radius, const InventorySystem* inventory)
{
    if (dt <= 0.0f) {
        return 0;
    }

    int pulled = 0;
    const float effectiveRadius = std::max(8.0f, radius);
    const float radiusSq = effectiveRadius * effectiveRadius;
    for (WorldDropItem& drop : drops_) {
        if (drop.kind != WorldDropKind::Object) {
            continue;
        }
        if (drop.jumpActive) {
            continue;
        }
        const ObjectDefinition* object = catalog.registry.findById(drop.id);
        if (object == nullptr || !hasObjectTag(*object, "metal")) {
            continue;
        }
        if (!objectDropCanEnterInventory(drop, catalog, inventory)) {
            continue;
        }
        const Vec2 toCenter = center - drop.position;
        const float distanceSq = lengthSquared(toCenter);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }
        const float distance = std::sqrt(distanceSq);
        const float falloff = 1.0f - clamp(distance / effectiveRadius, 0.0f, 1.0f);
        drop.velocity += normalize(toCenter) * (CapturedMagnetDropAcceleration * falloff * dt);
        ++pulled;
        if (pulled >= CapturedMagnetDropLimit) {
            break;
        }
    }
    return pulled;
}

int WorldDropSystem::pullLightDrops(
    const ObjectCatalog& catalog,
    Vec2 center,
    float dt,
    float radius,
    float strength,
    const InventorySystem* inventory)
{
    if (dt <= 0.0f || radius <= 0.0f || strength <= 0.0f) {
        return 0;
    }

    int pulled = 0;
    const float effectiveRadius = std::max(DropPickupRadius, radius);
    const float radiusSq = effectiveRadius * effectiveRadius;
    const float acceleration = VacuumLightDropAcceleration * std::clamp(strength, 0.1f, 4.0f);
    for (WorldDropItem& drop : drops_) {
        if (drop.jumpActive || drop.pickupDelaySeconds > 0.0f) {
            continue;
        }
        if (!isLightDrop(drop, catalog)) {
            continue;
        }
        if (!objectDropCanEnterInventory(drop, catalog, inventory)) {
            continue;
        }
        const Vec2 toCenter = center - drop.position;
        const float distanceSq = lengthSquared(toCenter);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }
        const float distance = std::sqrt(distanceSq);
        const float falloff = 0.45f + (1.0f - clamp(distance / effectiveRadius, 0.0f, 1.0f)) * 0.55f;
        drop.velocity += normalize(toCenter) * (acceleration * falloff * dt);
        ++pulled;
        if (pulled >= VacuumLightDropLimit) {
            break;
        }
    }
    return pulled;
}

int WorldDropSystem::pushLightDrops(
    const ObjectCatalog& catalog,
    Vec2 center,
    float dt,
    float radius,
    float strength)
{
    if (dt <= 0.0f || radius <= 0.0f || strength <= 0.0f) {
        return 0;
    }

    int pushed = 0;
    const float effectiveRadius = std::max(DropPickupRadius, radius);
    const float radiusSq = effectiveRadius * effectiveRadius;
    const float acceleration = WindLightDropAcceleration * std::clamp(strength, 0.1f, 4.0f);
    for (WorldDropItem& drop : drops_) {
        if (drop.jumpActive || drop.pickupDelaySeconds > 0.0f) {
            continue;
        }
        if (!isLightDrop(drop, catalog)) {
            continue;
        }
        const Vec2 away = drop.position - center;
        const float distanceSq = lengthSquared(away);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }
        const float distance = std::sqrt(distanceSq);
        const float falloff = 0.45f + (1.0f - clamp(distance / effectiveRadius, 0.0f, 1.0f)) * 0.55f;
        drop.velocity += normalize(away) * (acceleration * falloff * dt);
        ++pushed;
        if (pushed >= WindLightDropLimit) {
            break;
        }
    }
    return pushed;
}

int WorldDropSystem::update(
    float dt,
    const Player& player,
    InventorySystem& inventory,
    int& money,
    const ObjectCatalog& catalog,
    EffectSystem* effects,
    std::vector<WorldDropPickupEvent>* pickupEvents,
    int* blockedObjectPickupCount)
{
    const float pickupRadiusSq = DropPickupRadius * DropPickupRadius;
    int pickedUpCount = 0;
    for (WorldDropItem& drop : drops_) {
        drop.ageSeconds += dt;
        drop.pickupDelaySeconds = std::max(0.0f, drop.pickupDelaySeconds - dt);
        if (drop.jumpActive) {
            drop.jumpElapsedSeconds = std::min(drop.jumpDurationSeconds, drop.jumpElapsedSeconds + dt);
            const float t = drop.jumpDurationSeconds > 0.0f
                ? clamp(drop.jumpElapsedSeconds / drop.jumpDurationSeconds, 0.0f, 1.0f)
                : 1.0f;
            drop.position = lerp(drop.jumpStartPosition, drop.jumpTargetPosition, t);
            drop.altitude = dropHoverAltitude(drop) + std::sin(t * Pi) * std::max(0.0f, drop.jumpArcHeight);
            if (t >= 1.0f) {
                drop.position = drop.jumpTargetPosition;
                drop.jumpActive = false;
                drop.jumpElapsedSeconds = 0.0f;
                drop.jumpDurationSeconds = 0.0f;
                drop.jumpArcHeight = 0.0f;
                drop.altitude = dropHoverAltitude(drop);
            }
        } else {
            drop.position += drop.velocity * dt;
            const float damping = std::max(0.0f, 1.0f - dt * 5.5f);
            drop.velocity = drop.velocity * damping;
            drop.altitude = dropHoverAltitude(drop);
        }

        if (effects != nullptr && drop.kind == WorldDropKind::Material) {
            MaterialType materialType = MaterialType::Count;
            if (materialTypeFromSaveName(drop.id, materialType)) {
                drop.materialParticleTimer -= dt;
                if (drop.materialParticleTimer <= 0.0f) {
                    effects->spawnMaterialFloat(
                        elevatedDrawPosition(drop.position, drop.altitude),
                        materialParticleColor(materialType));
                    drop.materialParticleTimer = MaterialParticleInterval + randomDropRange(-0.035f, 0.045f);
                }
            }
        }
    }

    auto removeBegin = std::remove_if(drops_.begin(), drops_.end(), [&](const WorldDropItem& drop) {
        if (drop.pickupDelaySeconds > 0.0f) {
            return false;
        }
        if (distanceSquared(drop.position, player.position) > pickupRadiusSq) {
            return false;
        }

        bool pickedUp = false;
        WorldDropPickupEvent pickupEvent;
        bool hasPickupEvent = false;
        if (drop.kind == WorldDropKind::Object) {
            if (!inventory.canAddObjectItem(catalog, drop.id)) {
                if (blockedObjectPickupCount != nullptr && catalog.registry.findById(drop.id) != nullptr) {
                    ++*blockedObjectPickupCount;
                }
                return false;
            }
            pickedUp = inventory.addObjectItem(catalog, drop.id);
            if (pickedUp) {
                const ObjectDefinition* object = catalog.registry.findById(drop.id);
                pickupEvent.kind = drop.kind;
                pickupEvent.id = drop.id;
                pickupEvent.name = object != nullptr ? object->name : drop.id;
                pickupEvent.quantity = 1;
                hasPickupEvent = true;
            }
        } else if (drop.kind == WorldDropKind::Money) {
            money = std::max(0, money + std::max(0, drop.quantity));
            pickedUp = true;
            pickupEvent.kind = drop.kind;
            pickupEvent.id = drop.id;
            pickupEvent.quantity = std::max(0, drop.quantity);
            hasPickupEvent = true;
        } else if (drop.kind == WorldDropKind::Material) {
            MaterialType materialType = MaterialType::Count;
            if (materialTypeFromSaveName(drop.id, materialType)) {
                inventory.addMaterial(materialType, drop.quantity);
                pickedUp = true;
                pickupEvent.kind = drop.kind;
                pickupEvent.id = drop.id;
                pickupEvent.name = std::string(materialTypeDisplayName(materialType));
                pickupEvent.quantity = std::max(0, drop.quantity);
                hasPickupEvent = true;
            } else {
                logDropWarning("unknown material drop id=\"" + drop.id + "\"; removing drop");
                pickedUp = true;
            }
        }

        if (pickedUp) {
            if (effects != nullptr) {
                effects->spawnDropPickup(elevatedDrawPosition(drop.position, drop.altitude), player.position - drop.position);
            }
            if (pickupEvents != nullptr && hasPickupEvent && pickupEvent.quantity > 0) {
                pickupEvents->push_back(std::move(pickupEvent));
            }
            ++pickedUpCount;
            return true;
        }
        return false;
    });
    drops_.erase(removeBegin, drops_.end());
    return pickedUpCount;
}

void WorldDropSystem::render(
    Renderer& renderer,
    const TileMap& tileMap,
    const ObjectCatalog& catalog,
    Vec2 playerLight,
    const std::vector<LightSource>& extraLights) const
{
    std::vector<DepthRenderEntry> entries;
    renderShadows(renderer, tileMap, catalog, playerLight, extraLights);
    appendRenderEntries(entries, renderer, tileMap, catalog, playerLight, extraLights);
    std::stable_sort(entries.begin(), entries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : entries) {
        entry.draw();
    }
}

void WorldDropSystem::renderShadows(
    Renderer& renderer,
    const TileMap& tileMap,
    const ObjectCatalog& catalog,
    Vec2 playerLight,
    const std::vector<LightSource>& extraLights) const
{
    for (const WorldDropItem& drop : drops_) {
        if (!tileMap.isLit(drop.position, playerLight, extraLights)) {
            continue;
        }
        drawWorldDropShadow(renderer, drop, catalog);
    }
}

void WorldDropSystem::appendRenderEntries(
    std::vector<DepthRenderEntry>& entries,
    Renderer& renderer,
    const TileMap& tileMap,
    const ObjectCatalog& catalog,
    Vec2 playerLight,
    const std::vector<LightSource>& extraLights) const
{
    for (const WorldDropItem& drop : drops_) {
        if (!tileMap.isLit(drop.position, playerLight, extraLights)) {
            continue;
        }

        entries.push_back(DepthRenderEntry{
            drop.position.y,
            [&renderer, &catalog, &drop]() {
                drawWorldDrop(renderer, drop, catalog);
            },
        });
    }
}

const ObjectDefinition* WorldDropSystem::chooseDigDrop(const ObjectCatalog& catalog, std::string_view warningContext) const
{
    if (catalog.registry.empty()) {
        logDropWarning("Objects DB is empty; no item drop spawned");
        return nullptr;
    }

    bool hasTaggedDropCandidates = false;
    for (const ObjectDefinition& object : catalog.objects) {
        if (hasAllowedCategory(object) &&
            !hasAnyTag(object, ExcludedDropTags) &&
            hasAnyTag(object, DropTags)) {
            hasTaggedDropCandidates = true;
            break;
        }
    }

    std::vector<const ObjectDefinition*> candidates;
    std::vector<int> weights;
    for (const ObjectDefinition& object : catalog.objects) {
        if (!hasAllowedCategory(object) || hasAnyTag(object, ExcludedDropTags)) {
            continue;
        }
        if (hasTaggedDropCandidates && !hasAnyTag(object, DropTags)) {
            continue;
        }

        candidates.push_back(&object);
        weights.push_back(rarityWeight(object));
    }

    if (candidates.empty()) {
        logDropWarning("no Objects DB drop candidates for " + std::string(warningContext));
        return nullptr;
    }

    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    return candidates[static_cast<std::size_t>(distribution(dropRng()))];
}

const ObjectDefinition* WorldDropSystem::chooseDropForTile(TileType tileType, const ObjectCatalog& catalog) const
{
    if (!canDropFromTile(tileType)) {
        return nullptr;
    }
    return chooseDigDrop(catalog, "dug ore tile");
}

bool WorldDropSystem::canSpawnDrop(std::string_view label)
{
    if (dropLimit_ <= 0) {
        return false;
    }
    if (static_cast<int>(drops_.size()) >= dropLimit_) {
        if (!pruneOneDropForLimit()) {
            logDropWarning("drop limit reached; skipped " + std::string(label) + " drop");
            return false;
        }
    }
    return true;
}

bool WorldDropSystem::pruneOneDropForLimit()
{
    if (drops_.empty()) {
        return false;
    }

    auto protectedDrop = [](const WorldDropItem& drop) {
        return drop.kind == WorldDropKind::Material && drop.id == materialTypeSaveName(MaterialType::MoonFragment);
    };
    auto valueScore = [](const WorldDropItem& drop) {
        if (drop.kind == WorldDropKind::Money) {
            return std::max(1, drop.quantity);
        }
        if (drop.kind == WorldDropKind::Material) {
            MaterialType type = MaterialType::Count;
            if (materialTypeFromSaveName(drop.id, type)) {
                switch (type) {
                case MaterialType::OldWoodBuildingMaterial: return 5;
                case MaterialType::EnhancementOre: return 15;
                case MaterialType::ManaDrop: return 20;
                case MaterialType::MoonFragment: return 1000000;
                case MaterialType::Count: break;
                }
            }
            return 25;
        }
        return 100;
    };

    auto best = drops_.end();
    for (auto it = drops_.begin(); it != drops_.end(); ++it) {
        if (protectedDrop(*it)) {
            continue;
        }
        if (best == drops_.end() ||
            valueScore(*it) < valueScore(*best) ||
            (valueScore(*it) == valueScore(*best) && it->spawnedAtSeconds < best->spawnedAtSeconds)) {
            best = it;
        }
    }
    if (best == drops_.end()) {
        return false;
    }

    drops_.erase(best);
    return true;
}

Vec2 WorldDropSystem::randomDropVelocity() const
{
    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> speedDistribution(16.0f, 42.0f);
    return fromAngle(angleDistribution(dropRng())) * speedDistribution(dropRng());
}

void WorldDropSystem::spawnDrop(const ObjectDefinition& object, Vec2 position, float spawnedAtSeconds, WorldDropSpawnMotion motion)
{
    if (object.id.empty()) {
        logDropWarning("selected drop object has empty ID");
        return;
    }
    if (!canSpawnDrop("object")) {
        return;
    }

    WorldDropItem drop{
        .kind = WorldDropKind::Object,
        .id = object.id,
        .quantity = 1,
        .position = position,
        .velocity = randomDropVelocity(),
        .spawnedAtSeconds = spawnedAtSeconds,
        .ageSeconds = 0.0f,
    };
    configureDropMotion(drop, motion);
    drops_.push_back(std::move(drop));

    std::ostringstream line;
    line << "WorldDropSystem: spawned object_id=\"" << object.id << "\" name=\"" << object.name << "\"";
    logError(line.str());
}

}
