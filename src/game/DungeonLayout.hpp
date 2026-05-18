#pragma once

#include "engine/Math.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace majo {

struct DungeonTile {
    int x = 0;
    int y = 0;
};

struct DungeonPath {
    std::vector<Vec2> points;
};

enum class SpecialRoomType {
    None,
    OreRoom,
    SafeCavern,
    CoinRoom,
    TreasureRoom,
    EnemyRoom,
};

struct SpecialRoomAnchor {
    SpecialRoomType type = SpecialRoomType::None;
    Vec2 center{};
    float radius = 5.0f;
};

struct DungeonGenerationContext {
    int stageId = 1;
    std::uint32_t seed = 0;
    float stageHardnessMultiplier = 1.0f;
    int goalDistanceTiles = 320;
    float detourRate = 0.30f;
    float branchDensity = 0.25f;
    float cavernWidthMultiplier = 1.0f;
    int warpPointCount = 8;
    int specialRoomCount = 6;
    std::string generationProfile = "natural_cave";
    std::string terrainProfile = "soft_stardust";
    bool roguelike = false;
};

struct DungeonLayout {
    int stageId = 1;
    std::uint32_t seed = 0;
    float stageHardnessMultiplier = 1.0f;
    float cavernWidthMultiplier = 1.0f;
    std::string generationProfile = "natural_cave";
    std::string terrainProfile = "soft_stardust";
    DungeonTile startTile{};
    DungeonTile goalTile{};
    std::vector<Vec2> mainPathPoints;
    std::vector<DungeonPath> branchPathPoints;
    std::vector<Vec2> warpPointAnchors;
    std::vector<SpecialRoomAnchor> specialRoomAnchors;
};

struct DungeonLayoutMetrics {
    float distanceFromStart = 0.0f;
    float pathProgress = 0.0f;
    float distanceFromMainPath = 0.0f;
};

struct SpecialRoomMetrics {
    SpecialRoomType currentRoomType = SpecialRoomType::None;
    SpecialRoomType nearestRoomType = SpecialRoomType::None;
    float distanceToNearestRoom = 0.0f;
};

DungeonLayout generateDungeonLayout(const DungeonGenerationContext& context);
DungeonLayoutMetrics calculateDungeonLayoutMetrics(const DungeonLayout& layout, Vec2 tilePosition);
SpecialRoomMetrics calculateSpecialRoomMetrics(const DungeonLayout& layout, Vec2 tilePosition);
const char* specialRoomTypeName(SpecialRoomType type);

}
