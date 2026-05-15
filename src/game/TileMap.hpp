#pragma once

#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/Chunk.hpp"
#include "game/DungeonLayout.hpp"
#include <cstddef>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

enum class TerrainAttribute {
    None,
    Soft,
    Hard,
    Ore,
};

enum class TerrainDigModifier {
    Normal,
    HardSpecialist,
};

std::string_view terrainAttributeCode(TerrainAttribute attribute);
TerrainAttribute terrainAttributeForTileType(TileType type);
int adjustedTerrainDigDamage(int baseDamage, TerrainAttribute attribute, TerrainDigModifier modifier);

struct LightSource {
    Vec2 position{};
    float radius = 0.0f;
};

struct TerrainDebugInfo {
    TileType type = TileType::Empty;
    TerrainAttribute attribute = TerrainAttribute::None;
    int hp = 0;
    int effectiveHp = 0;
    float localHardnessMultiplier = 1.0f;
    float distanceFromMainPath = 0.0f;
};

struct DamagedTile {
    Vec2 center{};
    TileType type = TileType::Dirt;
    Color color{105, 68, 37, 255};
};

class TileMap {
public:
    void updateAround(Vec2 worldCenter, float dt, const RuntimeBalance& config, const DungeonLayout& dungeonLayout);
    void render(Renderer& renderer, const Camera& camera, Vec2 lightCenter, const std::vector<LightSource>& extraLights);
    void renderDarknessOverlay(Renderer& renderer, const Camera& camera, Vec2 lightCenter, const std::vector<LightSource>& extraLights) const;
    std::vector<DamagedTile> damageCircle(Vec2 center, float radius, int damage);
    bool damageTile(int tx, int ty, int damage, Vec2& openedTileCenter, TileType* openedTileType = nullptr);
    bool isSolidAt(Vec2 world);
    bool isTileSolid(int tx, int ty);
    TerrainAttribute terrainAttributeAtTile(int tx, int ty);
    bool isCircleBlocked(Vec2 center, float radius);
    Vec2 tileCenter(int tx, int ty) const;
    int worldToTile(float value) const;
    bool isLit(Vec2 world, Vec2 playerLight, const std::vector<LightSource>& extraLights) const;
    bool isRectLit(Vec2 center, Vec2 size, Vec2 playerLight, const std::vector<LightSource>& extraLights) const;
    TerrainDebugInfo terrainDebugAtWorld(Vec2 world) const;
    Color tileColorAtTile(int tx, int ty) const;
    Color tileColorAtWorld(Vec2 world) const;
    int activeChunkCount() const { return activeChunkCount_; }
    std::size_t generatedChunkCount() const { return chunks_.size(); }

private:
    Chunk& getOrCreateChunk(int cx, int cy, const RuntimeBalance& config);
    void initializeChunk(Chunk& chunk, const RuntimeBalance& config);
    static long long key(int cx, int cy);
    static int floorDiv(int a, int b);
    static int floorMod(int a, int b);
    Tile* tileAtWorld(int tx, int ty);
    const Tile* tileAtWorldIfGenerated(int tx, int ty) const;
    void rememberDamagedTileMaxHp(int tx, int ty, const Tile& tile);
    void clearCrackCacheForTile(int tx, int ty);
    int crackLevelForTile(int tx, int ty, const Tile& tile) const;
    void drawTileCracks(Renderer& renderer, Vec2 pos, int tx, int ty, const Tile& tile);
    RuntimeBalance balanceSnapshot_;
    DungeonLayout dungeonLayoutSnapshot_;
    TerrainDebugInfo terrainInfoForTile(int tx, int ty, const Tile* tile) const;
    Color tileColor(const Tile& tile) const;
    bool isTileRectLit(Vec2 pos, Vec2 playerLight, const std::vector<LightSource>& extraLights) const;
    void drawTileLitByCircles(Renderer& renderer, Vec2 pos, Color color, Vec2 playerLight, const std::vector<LightSource>& extraLights) const;

    std::unordered_map<long long, Chunk> chunks_;
    std::unordered_map<long long, int> damagedTileMaxHp_;
    int centerChunkX_ = 0;
    int centerChunkY_ = 0;
    int activeChunkCount_ = 0;
};

}
