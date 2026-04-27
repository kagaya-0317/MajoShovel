#pragma once

#include "engine/Math.hpp"
#include "engine/Renderer.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/Chunk.hpp"
#include <unordered_map>
#include <vector>

namespace majo {

class TileMap {
public:
    void updateAround(Vec2 worldCenter, float dt, const RuntimeBalance& config);
    void render(Renderer& renderer, const Camera& camera, Vec2 lightCenter, const std::vector<Vec2>& extraLights);
    std::vector<Vec2> damageCircle(Vec2 center, float radius, int damage);
    bool damageTile(int tx, int ty, int damage, Vec2& openedTileCenter, TileType* openedTileType = nullptr);
    bool isSolidAt(Vec2 world);
    bool isTileSolid(int tx, int ty);
    bool isCircleBlocked(Vec2 center, float radius);
    Vec2 tileCenter(int tx, int ty) const;
    int worldToTile(float value) const;
    bool isLit(Vec2 world, Vec2 playerLight, const std::vector<Vec2>& extraLights) const;
    int activeChunkCount() const { return activeChunkCount_; }

private:
    Chunk& getOrCreateChunk(int cx, int cy, const RuntimeBalance& config);
    void initializeChunk(Chunk& chunk, const RuntimeBalance& config);
    static long long key(int cx, int cy);
    static int floorDiv(int a, int b);
    static int floorMod(int a, int b);
    Tile* tileAtWorld(int tx, int ty);
    RuntimeBalance balanceSnapshot_;
    Color tileColor(const Tile& tile) const;
    void drawTileLitByCircles(Renderer& renderer, Vec2 pos, Color color, Vec2 playerLight, const std::vector<Vec2>& extraLights) const;

    std::unordered_map<long long, Chunk> chunks_;
    int centerChunkX_ = 0;
    int centerChunkY_ = 0;
    int activeChunkCount_ = 0;
};

}
