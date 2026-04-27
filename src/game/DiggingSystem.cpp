#include "game/DiggingSystem.hpp"

namespace majo {

void DiggingSystem::update(TileMap& map, SpellRingSystem& spellRing, float totalTime)
{
    openedTiles_.clear();
    hitTiles_.clear();
    dugTiles_.clear();
    for (auto& item : spellRing.items()) {
        if (item.type != SpellRingItemType::Shovel || item.digPower <= 0) {
            continue;
        }
        const int tileX = map.worldToTile(item.worldPosition.x);
        const int tileY = map.worldToTile(item.worldPosition.y);
        if (tileX == item.lastDigTileX && tileY == item.lastDigTileY) {
            continue;
        }
        item.lastDigTileX = tileX;
        item.lastDigTileY = tileY;

        if (!map.isTileSolid(tileX, tileY)) {
            continue;
        }

        hitTiles_.push_back(map.tileCenter(tileX, tileY));
        Vec2 openedTileCenter{};
        TileType openedTileType = TileType::Dirt;
        if (map.damageTile(tileX, tileY, item.digPower, openedTileCenter, &openedTileType)) {
            openedTiles_.push_back(openedTileCenter);
            dugTiles_.push_back({openedTileCenter, openedTileType});
        }
        item.lastTerrainHitTime = totalTime;
    }
}

}
