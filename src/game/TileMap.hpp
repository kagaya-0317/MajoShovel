#pragma once

#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/Chunk.hpp"
#include "game/DungeonLayout.hpp"
#include <array>
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

struct TerrainTileEdit {
    DungeonTile tile{};
    TileType type = TileType::Empty;
};

struct TerrainTileDamageState {
    DungeonTile tile{};
    TileType type = TileType::Dirt;
    int hp = 0;
    int maxHp = 1;
};

struct TileMapPersistentState {
    std::vector<TerrainTileEdit> tileOverrides;
    std::vector<TerrainTileEdit> terrainEdits;
    std::vector<TerrainTileDamageState> damagedTiles;
};

struct TerrainDamageProtectionArea {
    DungeonTile minTile{};
    DungeonTile maxTile{};
};

class TileMap {
public:
    void updateAround(Vec2 worldCenter, float dt, const RuntimeBalance& config, const DungeonLayout& dungeonLayout);
    void render(Renderer& renderer, const Camera& camera, Vec2 lightCenter, const std::vector<LightSource>& extraLights);
    void renderTilePreview(Renderer& renderer, Vec2 pos, int stageId, TileType type) const;
    bool renderTileQuad(
        Renderer& renderer,
        const std::array<Vec2, 4>& corners,
        int stageId,
        TileType type,
        int variantX,
        int variantY,
        Color tint = {255, 255, 255, 255}) const;
    void renderDarknessOverlay(
        Renderer& renderer,
        const Camera& camera,
        Vec2 lightCenter,
        const std::vector<LightSource>& extraLights,
        bool lightweight = false) const;
    std::vector<DamagedTile> damageCircle(Vec2 center, float radius, int damage);
    std::vector<DamagedTile> destroyCircle(Vec2 center, float radius);
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
    void setTileOverride(DungeonTile tile, TileType type);
    void setTerrainEdit(DungeonTile tile, TileType type);
    void setDamageProtectionAreas(std::vector<TerrainDamageProtectionArea> areas);
    void clearDamageProtectionAreas();
    std::vector<TerrainTileEdit> terrainEditsForSave() const;
    TileMapPersistentState capturePersistentState() const;
    void restorePersistentState(const TileMapPersistentState& state);
    int activeChunkCount() const { return activeChunkCount_; }
    std::size_t generatedChunkCount() const { return chunks_.size(); }
    std::size_t terrainEditCount() const { return terrainEdits_.size(); }
    std::size_t damagedTileStateCount() const { return damagedTileStates_.size(); }

private:
    Chunk& getOrCreateChunk(int cx, int cy, const RuntimeBalance& config);
    void initializeChunk(Chunk& chunk, const RuntimeBalance& config);
    void evictDistantChunks();
    static long long key(int cx, int cy);
    static DungeonTile tileFromKey(long long key);
    static int floorDiv(int a, int b);
    static int floorMod(int a, int b);
    Tile* tileAtWorld(int tx, int ty);
    const Tile* tileAtWorldIfGenerated(int tx, int ty) const;
    int damageStateMaxHpForTile(int tx, int ty, const Tile& tile) const;
    void recordDamagedTileState(int tx, int ty, TileType type, int hp, int maxHp);
    bool damageProtectedAt(int tx, int ty) const;
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
    std::unordered_map<long long, TileType> tileOverrides_;
    std::unordered_map<long long, TileType> terrainEdits_;
    std::unordered_map<long long, TerrainTileDamageState> damagedTileStates_;
    std::vector<TerrainDamageProtectionArea> damageProtectionAreas_;
    int centerChunkX_ = 0;
    int centerChunkY_ = 0;
    int activeChunkCount_ = 0;
};

}
