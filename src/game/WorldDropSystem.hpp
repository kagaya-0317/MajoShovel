#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
#include "game/DiggingSystem.hpp"
#include "game/TileMap.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace majo {

class InventorySystem;
class EffectSystem;
struct Player;

struct WorldDropItem {
    std::string objectId;
    Vec2 position{};
    Vec2 velocity{};
    float ageSeconds = 0.0f;
};

class WorldDropSystem {
public:
    void clear();
    void spawnFromDugTiles(const std::vector<DugTile>& dugTiles, const ObjectCatalog& catalog);
    bool spawnObjectDrop(const ObjectCatalog& catalog, std::string_view objectId, Vec2 position);
    bool spawnRewardDrop(const ObjectCatalog& catalog, Vec2 position);
    int pullMetalDrops(const ObjectCatalog& catalog, Vec2 center, float dt);
    int update(float dt, const Player& player, InventorySystem& inventory, const ObjectCatalog& catalog, EffectSystem* effects = nullptr);
    void render(
        Renderer& renderer,
        const TileMap& tileMap,
        const ObjectCatalog& catalog,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights) const;

    [[nodiscard]] std::size_t size() const { return drops_.size(); }

private:
    const ObjectDefinition* chooseDropForTile(TileType tileType, const ObjectCatalog& catalog) const;
    void spawnDrop(const ObjectDefinition& object, Vec2 position);

    std::vector<WorldDropItem> drops_;
};

}
