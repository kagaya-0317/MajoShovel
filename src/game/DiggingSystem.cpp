#include "game/DiggingSystem.hpp"

namespace majo {

void DiggingSystem::update(TileMap& map, OrbitSystem& orbit, float totalTime)
{
    openedTiles_.clear();
    for (auto& item : orbit.items()) {
        if (item.type != OrbitItemType::Shovel || item.digPower <= 0) {
            continue;
        }
        if (totalTime - item.lastTerrainHitTime < item.hitInterval) {
            continue;
        }
        std::vector<Vec2> opened = map.damageCircle(item.worldPosition, item.hitRadius, item.digPower);
        openedTiles_.insert(openedTiles_.end(), opened.begin(), opened.end());
        item.lastTerrainHitTime = totalTime;
    }
}

}
