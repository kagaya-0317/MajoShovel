#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
#include "game/DepthRender.hpp"
#include "game/DiggingSystem.hpp"
#include "game/ItemModel.hpp"
#include "game/TileMap.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace majo {

class InventorySystem;
class EffectSystem;
struct Player;

enum class WorldDropKind {
    Object,
    Money,
    Material,
};

struct WorldDropSpawnMotion {
    bool jump = false;
    Vec2 startPosition{};
    float jumpDurationSeconds = 0.0f;
    float jumpArcHeight = 0.0f;
    float pickupDelaySeconds = 0.0f;
};

struct WorldDropItem {
    WorldDropKind kind = WorldDropKind::Object;
    std::string id;
    int quantity = 1;
    Vec2 position{};
    Vec2 velocity{};
    float spawnedAtSeconds = 0.0f;
    float ageSeconds = 0.0f;
    float altitude = 0.0f;
    float hoverBaseAltitude = 0.0f;
    float hoverAmplitude = 0.0f;
    float hoverSpeed = 0.0f;
    float hoverPhase = 0.0f;
    bool jumpActive = false;
    Vec2 jumpStartPosition{};
    Vec2 jumpTargetPosition{};
    float jumpElapsedSeconds = 0.0f;
    float jumpDurationSeconds = 0.0f;
    float jumpArcHeight = 0.0f;
    float pickupDelaySeconds = 0.0f;
    float materialParticleTimer = 0.0f;
};

struct WorldDropPickupEvent {
    WorldDropKind kind = WorldDropKind::Object;
    std::string id;
    std::string instanceId;
    std::string name;
    int quantity = 1;
    bool protectable = false;
};

class WorldDropSystem {
public:
    void clear();
    void setDropLimit(int limit);
    void spawnFromDugTiles(const std::vector<DugTile>& dugTiles, const ObjectCatalog& catalog, float spawnedAtSeconds = 0.0f);
    bool spawnObjectDrop(
        const ObjectCatalog& catalog,
        std::string_view objectId,
        Vec2 position,
        float spawnedAtSeconds = 0.0f,
        WorldDropSpawnMotion motion = {});
    bool spawnDigItemDrop(const ObjectCatalog& catalog, Vec2 position, float spawnedAtSeconds = 0.0f);
    bool spawnMoneyDrop(int amount, Vec2 position, float spawnedAtSeconds = 0.0f, WorldDropSpawnMotion motion = {});
    bool spawnMaterialDrop(MaterialType type, int count, Vec2 position, float spawnedAtSeconds = 0.0f, WorldDropSpawnMotion motion = {});
    bool spawnRewardDrop(const ObjectCatalog& catalog, Vec2 position, float spawnedAtSeconds = 0.0f);
    bool stealNearestDrop(const ObjectCatalog& catalog, Vec2 center, float radius, std::string_view targetFilter, WorldDropItem& outDrop);
    int pullNearbyDrops(
        Vec2 center,
        float dt,
        float radius,
        float acceleration,
        int limit = 0,
        const InventorySystem* inventory = nullptr,
        const ObjectCatalog* catalog = nullptr);
    int pullMetalDrops(
        const ObjectCatalog& catalog,
        Vec2 center,
        float dt,
        float radius = 170.0f,
        const InventorySystem* inventory = nullptr);
    int pullLightDrops(
        const ObjectCatalog& catalog,
        Vec2 center,
        float dt,
        float radius,
        float strength = 1.0f,
        const InventorySystem* inventory = nullptr);
    int pushLightDrops(
        const ObjectCatalog& catalog,
        Vec2 center,
        float dt,
        float radius,
        float strength = 1.0f);
    int update(
        float dt,
        const Player& player,
        InventorySystem& inventory,
        int& money,
        const ObjectCatalog& catalog,
        EffectSystem* effects = nullptr,
        std::vector<WorldDropPickupEvent>* pickupEvents = nullptr,
        int* blockedObjectPickupCount = nullptr);
    void render(
        Renderer& renderer,
        const TileMap& tileMap,
        const ObjectCatalog& catalog,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights) const;
    void renderShadows(
        Renderer& renderer,
        const TileMap& tileMap,
        const ObjectCatalog& catalog,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights) const;
    void appendRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        const TileMap& tileMap,
        const ObjectCatalog& catalog,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights) const;

    [[nodiscard]] std::size_t size() const { return drops_.size(); }
    [[nodiscard]] const std::vector<WorldDropItem>& drops() const { return drops_; }
    void restoreDropsForSave(std::vector<WorldDropItem> drops);

private:
    const ObjectDefinition* chooseDigDrop(const ObjectCatalog& catalog, std::string_view warningContext) const;
    const ObjectDefinition* chooseDropForTile(TileType tileType, const ObjectCatalog& catalog) const;
    bool canSpawnDrop(std::string_view label);
    bool pruneOneDropForLimit();
    Vec2 randomDropVelocity() const;
    void spawnDrop(const ObjectDefinition& object, Vec2 position, float spawnedAtSeconds, WorldDropSpawnMotion motion = {});

    std::vector<WorldDropItem> drops_;
    int dropLimit_ = 300;
};

}
