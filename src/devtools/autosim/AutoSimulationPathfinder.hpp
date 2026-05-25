#pragma once

#include "game/GameTestProbe.hpp"

#include <optional>
#include <vector>

namespace majo::autosim {

struct AutoSimulationPathCell {
    GameTestPathTileSnapshot tile;
    float cost = 0.0f;
    int previous = -1;
    int digCount = 0;
};

struct AutoSimulationPathField {
    int minTileX = 0;
    int minTileY = 0;
    int width = 0;
    int height = 0;
    int startIndex = -1;
    Vec2 playerPosition{};
    Vec2 playerFacing{1.0f, 0.0f};
    float playerRadius = 0.0f;
    std::vector<GameTestCollisionRectSnapshot> objectBlockers;
    std::vector<AutoSimulationPathCell> cells;

    bool valid() const;
    int indexForTile(int tileX, int tileY) const;
};

struct AutoSimulationRoute {
    bool found = false;
    float totalCost = 0.0f;
    int digTileCount = 0;
    int hardTileCount = 0;
    GameTestTerrainKind firstDigTerrainKind = GameTestTerrainKind::Empty;
    bool hasFirstDigTerrainKind = false;
    bool avoidingHardWall = false;
    Vec2 nextWaypointWorld{};
    std::optional<GameTestMineTileSnapshot> nextDigTile;
    std::vector<Vec2> debugWorldPoints;
};

class AutoSimulationPathfinder {
public:
    AutoSimulationPathField buildField(const GameTestSnapshot& snapshot) const;
    std::optional<AutoSimulationRoute> findRoute(const AutoSimulationPathField& field, Vec2 targetWorld) const;
};

} // namespace majo::autosim
