#pragma once

#include "game/OrbitSystem.hpp"
#include "game/TileMap.hpp"
#include <vector>

namespace majo {

class DiggingSystem {
public:
    void update(TileMap& map, OrbitSystem& orbit, float totalTime);
    const std::vector<Vec2>& openedTiles() const { return openedTiles_; }

private:
    std::vector<Vec2> openedTiles_;
};

}
