#include "game/WorldDropSystem.hpp"

#include "data/GameBalance.hpp"
#include "engine/Log.hpp"
#include "game/InventorySystem.hpp"
#include "game/Player.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <sstream>
#include <string_view>

namespace majo {

namespace {
constexpr float DropPickupRadius = 23.0f;
constexpr float DropVisualRadius = 8.0f;
constexpr float CapturedMagnetDropRadius = 170.0f;
constexpr float CapturedMagnetDropAcceleration = 260.0f;
constexpr int CapturedMagnetDropLimit = 6;

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

void logDropWarning(std::string_view message)
{
    logError("[warning] WorldDropSystem: " + std::string(message));
}
}

void WorldDropSystem::clear()
{
    drops_.clear();
}

void WorldDropSystem::spawnFromDugTiles(const std::vector<DugTile>& dugTiles, const ObjectCatalog& catalog)
{
    for (const DugTile& dugTile : dugTiles) {
        if (!canDropFromTile(dugTile.type)) {
            continue;
        }

        const ObjectDefinition* object = chooseDropForTile(dugTile.type, catalog);
        if (object == nullptr) {
            continue;
        }
        spawnDrop(*object, dugTile.center);
    }
}

bool WorldDropSystem::spawnObjectDrop(const ObjectCatalog& catalog, std::string_view objectId, Vec2 position)
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

    spawnDrop(*object, position);
    return true;
}

bool WorldDropSystem::spawnRewardDrop(const ObjectCatalog& catalog, Vec2 position)
{
    const ObjectDefinition* object = chooseDropForTile(TileType::Ore, catalog);
    if (object == nullptr) {
        return false;
    }
    spawnDrop(*object, position);
    return true;
}

int WorldDropSystem::pullMetalDrops(const ObjectCatalog& catalog, Vec2 center, float dt)
{
    if (dt <= 0.0f) {
        return 0;
    }

    int pulled = 0;
    const float radiusSq = CapturedMagnetDropRadius * CapturedMagnetDropRadius;
    for (WorldDropItem& drop : drops_) {
        const ObjectDefinition* object = catalog.registry.findById(drop.objectId);
        if (object == nullptr || !hasObjectTag(*object, "metal")) {
            continue;
        }
        const Vec2 toCenter = center - drop.position;
        const float distanceSq = lengthSquared(toCenter);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }
        const float distance = std::sqrt(distanceSq);
        const float falloff = 1.0f - clamp(distance / CapturedMagnetDropRadius, 0.0f, 1.0f);
        drop.velocity += normalize(toCenter) * (CapturedMagnetDropAcceleration * falloff * dt);
        ++pulled;
        if (pulled >= CapturedMagnetDropLimit) {
            break;
        }
    }
    return pulled;
}

int WorldDropSystem::update(float dt, const Player& player, InventorySystem& inventory, const ObjectCatalog& catalog)
{
    const float pickupRadiusSq = DropPickupRadius * DropPickupRadius;
    int pickedUpCount = 0;
    for (WorldDropItem& drop : drops_) {
        drop.ageSeconds += dt;
        drop.position += drop.velocity * dt;
        const float damping = std::max(0.0f, 1.0f - dt * 5.5f);
        drop.velocity = drop.velocity * damping;
    }

    auto removeBegin = std::remove_if(drops_.begin(), drops_.end(), [&](const WorldDropItem& drop) {
        if (distanceSquared(drop.position, player.position) > pickupRadiusSq) {
            return false;
        }

        if (inventory.addObjectItem(catalog, drop.objectId)) {
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
    for (const WorldDropItem& drop : drops_) {
        if (!tileMap.isLit(drop.position, playerLight, extraLights)) {
            continue;
        }

        const ItemData* object = catalog.registry.findById(drop.objectId);
        const float bob = std::sin(drop.ageSeconds * 5.5f) * 2.5f;
        const Vec2 center = drop.position + Vec2{0.0f, bob};
        const Color color = object != nullptr ? colorForDrop(*object) : Color{255, 80, 120, 255};
        renderer.fillCircle(center, DropVisualRadius, color);
        renderer.drawCircle(center, DropVisualRadius + 3.0f, {255, 246, 190, 210});

        if (object != nullptr) {
            renderer.drawText(center + Vec2{-24.0f, -28.0f}, object->name, {245, 238, 210, 235}, 1);
        }
    }
}

const ObjectDefinition* WorldDropSystem::chooseDropForTile(TileType tileType, const ObjectCatalog& catalog) const
{
    if (!canDropFromTile(tileType)) {
        return nullptr;
    }
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
        logDropWarning("no Objects DB drop candidates for dug ore tile");
        return nullptr;
    }

    std::discrete_distribution<int> distribution(weights.begin(), weights.end());
    return candidates[static_cast<std::size_t>(distribution(dropRng()))];
}

void WorldDropSystem::spawnDrop(const ObjectDefinition& object, Vec2 position)
{
    if (object.id.empty()) {
        logDropWarning("selected drop object has empty ID");
        return;
    }

    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> speedDistribution(16.0f, 42.0f);
    const Vec2 velocity = fromAngle(angleDistribution(dropRng())) * speedDistribution(dropRng());

    drops_.push_back(WorldDropItem{
        .objectId = object.id,
        .position = position,
        .velocity = velocity,
        .ageSeconds = 0.0f,
    });

    std::ostringstream line;
    line << "WorldDropSystem: spawned object_id=\"" << object.id << "\" name=\"" << object.name << "\"";
    logError(line.str());
}

}
