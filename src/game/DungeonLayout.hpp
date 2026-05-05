#pragma once

#include "engine/Math.hpp"

#include <cstdint>
#include <vector>

namespace majo {

struct DungeonTile {
    int x = 0;
    int y = 0;
};

struct DungeonPath {
    std::vector<Vec2> points;
};

struct DungeonGenerationContext {
    int stageId = 1;
    std::uint32_t seed = 0;
    float stageHardnessMultiplier = 1.0f;
    bool roguelike = false;
};

struct DungeonLayout {
    int stageId = 1;
    std::uint32_t seed = 0;
    float stageHardnessMultiplier = 1.0f;
    DungeonTile startTile{};
    DungeonTile goalTile{};
    std::vector<Vec2> mainPathPoints;
    std::vector<DungeonPath> branchPathPoints;
    std::vector<Vec2> warpPointAnchors;
    std::vector<Vec2> specialRoomAnchors;
};

struct DungeonLayoutMetrics {
    float distanceFromStart = 0.0f;
    float pathProgress = 0.0f;
    float distanceFromMainPath = 0.0f;
};

DungeonLayout generateDungeonLayout(const DungeonGenerationContext& context);
DungeonLayoutMetrics calculateDungeonLayoutMetrics(const DungeonLayout& layout, Vec2 tilePosition);

}
