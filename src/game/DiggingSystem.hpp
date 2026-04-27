#pragma once

#include "game/SpellRingSystem.hpp"
#include "game/TileMap.hpp"
#include <vector>

namespace majo {

struct DugTile {
    Vec2 center{};
    TileType type = TileType::Dirt;
};

class DiggingSystem {
public:
    void update(TileMap& map, SpellRingSystem& spellRing, float totalTime);
    const std::vector<Vec2>& openedTiles() const { return openedTiles_; }
    const std::vector<Vec2>& hitTiles() const { return hitTiles_; }
    const std::vector<DugTile>& dugTiles() const { return dugTiles_; }

private:
    std::vector<Vec2> openedTiles_;
    std::vector<Vec2> hitTiles_;
    std::vector<DugTile> dugTiles_;
};

}
