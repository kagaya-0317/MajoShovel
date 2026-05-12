#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
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

struct WorldDropItem {
    WorldDropKind kind = WorldDropKind::Object;
    std::string id;
    int quantity = 1;
    Vec2 position{};
    Vec2 velocity{};
    float spawnedAtSeconds = 0.0f;
    float ageSeconds = 0.0f;
};

class WorldDropSystem {
public:
    void clear();
    void setDropLimit(int limit);
    void spawnFromDugTiles(const std::vector<DugTile>& dugTiles, const ObjectCatalog& catalog, float spawnedAtSeconds = 0.0f);
    bool spawnObjectDrop(const ObjectCatalog& catalog, std::string_view objectId, Vec2 position, float spawnedAtSeconds = 0.0f);
    bool spawnDigItemDrop(const ObjectCatalog& catalog, Vec2 position, float spawnedAtSeconds = 0.0f);
    bool spawnMoneyDrop(int amount, Vec2 position, float spawnedAtSeconds = 0.0f);
    bool spawnMaterialDrop(MaterialType type, int count, Vec2 position, float spawnedAtSeconds = 0.0f);
    bool spawnRewardDrop(const ObjectCatalog& catalog, Vec2 position, float spawnedAtSeconds = 0.0f);
    bool stealNearestDrop(const ObjectCatalog& catalog, Vec2 center, float radius, std::string_view targetFilter, WorldDropItem& outDrop);
    int pullMetalDrops(const ObjectCatalog& catalog, Vec2 center, float dt, float radius = 170.0f);
    int update(float dt, const Player& player, InventorySystem& inventory, int& money, const ObjectCatalog& catalog, EffectSystem* effects = nullptr);
    void render(
        Renderer& renderer,
        const TileMap& tileMap,
        const ObjectCatalog& catalog,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights) const;

    [[nodiscard]] std::size_t size() const { return drops_.size(); }
    [[nodiscard]] const std::vector<WorldDropItem>& drops() const { return drops_; }

private:
    const ObjectDefinition* chooseDigDrop(const ObjectCatalog& catalog, std::string_view warningContext) const;
    const ObjectDefinition* chooseDropForTile(TileType tileType, const ObjectCatalog& catalog) const;
    bool canSpawnDrop(std::string_view label);
    bool pruneOneDropForLimit();
    Vec2 randomDropVelocity() const;
    void spawnDrop(const ObjectDefinition& object, Vec2 position, float spawnedAtSeconds);

    std::vector<WorldDropItem> drops_;
    int dropLimit_ = 300;
};

}
