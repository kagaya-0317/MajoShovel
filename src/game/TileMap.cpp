#include "game/TileMap.hpp"

#include "data/GameBalance.hpp"
#include "game/Collision.hpp"
#include <cmath>

namespace majo {

long long TileMap::key(int cx, int cy)
{
    return (static_cast<long long>(cx) << 32) ^ (static_cast<unsigned int>(cy));
}

int TileMap::floorDiv(int a, int b)
{
    int q = a / b;
    int r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

int TileMap::floorMod(int a, int b)
{
    int m = a % b;
    if (m < 0) m += b;
    return m;
}

Chunk& TileMap::getOrCreateChunk(int cx, int cy, const RuntimeBalance& config)
{
    const long long k = key(cx, cy);
    auto it = chunks_.find(k);
    if (it != chunks_.end()) {
        return it->second;
    }
    Chunk chunk;
    chunk.cx = cx;
    chunk.cy = cy;
    initializeChunk(chunk, config);
    auto [inserted, _] = chunks_.emplace(k, chunk);
    return inserted->second;
}

void TileMap::initializeChunk(Chunk& chunk, const RuntimeBalance& config)
{
    for (int y = 0; y < balance::ChunkTiles; ++y) {
        for (int x = 0; x < balance::ChunkTiles; ++x) {
            const int wx = chunk.cx * balance::ChunkTiles + x;
            const int wy = chunk.cy * balance::ChunkTiles + y;
            Tile& tile = chunk.at(x, y);
            const float n = std::sin(wx * 0.19f) + std::cos(wy * 0.17f) + std::sin((wx + wy) * 0.07f);
            const float distOrigin = std::sqrt(static_cast<float>(wx * wx + wy * wy));
            if (distOrigin < 9.0f || std::abs(std::sin(wx * 0.13f) * 6.0f + wy * 0.05f) < 0.55f) {
                tile.type = TileType::Empty;
                tile.hp = 0;
            } else if (n > 1.55f) {
                tile.type = TileType::Ore;
                tile.hp = static_cast<unsigned char>(config.oreHp);
            } else if (n < -0.55f || std::abs(wx + wy) % 23 == 0) {
                tile.type = TileType::Rock;
                tile.hp = static_cast<unsigned char>(config.rockHp);
            } else {
                tile.type = TileType::Dirt;
                tile.hp = static_cast<unsigned char>(config.dirtHp);
            }
        }
    }
}

void TileMap::updateAround(Vec2 worldCenter, float, const RuntimeBalance& config)
{
    balanceSnapshot_ = config;
    const int tileX = static_cast<int>(std::floor(worldCenter.x / balance::TileSize));
    const int tileY = static_cast<int>(std::floor(worldCenter.y / balance::TileSize));
    centerChunkX_ = floorDiv(tileX, balance::ChunkTiles);
    centerChunkY_ = floorDiv(tileY, balance::ChunkTiles);
    activeChunkCount_ = 0;
    for (int cy = centerChunkY_ - balance::ActiveChunkRadius; cy <= centerChunkY_ + balance::ActiveChunkRadius; ++cy) {
        for (int cx = centerChunkX_ - balance::ActiveChunkRadius; cx <= centerChunkX_ + balance::ActiveChunkRadius; ++cx) {
            getOrCreateChunk(cx, cy, balanceSnapshot_);
            ++activeChunkCount_;
        }
    }
    for (int cy = centerChunkY_ - balance::ActiveChunkRadius; cy <= centerChunkY_ + balance::ActiveChunkRadius; ++cy) {
        for (int cx = centerChunkX_ - balance::ActiveChunkRadius; cx <= centerChunkX_ + balance::ActiveChunkRadius; ++cx) {
            Chunk& chunk = getOrCreateChunk(cx, cy, balanceSnapshot_);
            for (auto& tile : chunk.tiles) {
                if (tile.flash > 0) --tile.flash;
            }
        }
    }
}

Tile* TileMap::tileAtWorld(int tx, int ty)
{
    const int cx = floorDiv(tx, balance::ChunkTiles);
    const int cy = floorDiv(ty, balance::ChunkTiles);
    const int lx = floorMod(tx, balance::ChunkTiles);
    const int ly = floorMod(ty, balance::ChunkTiles);
    Chunk& chunk = getOrCreateChunk(cx, cy, balanceSnapshot_);
    return &chunk.at(lx, ly);
}

std::vector<Vec2> TileMap::damageCircle(Vec2 center, float radius, int damage)
{
    std::vector<Vec2> openedTiles;
    const int minX = static_cast<int>(std::floor((center.x - radius) / balance::TileSize));
    const int maxX = static_cast<int>(std::floor((center.x + radius) / balance::TileSize));
    const int minY = static_cast<int>(std::floor((center.y - radius) / balance::TileSize));
    const int maxY = static_cast<int>(std::floor((center.y + radius) / balance::TileSize));
    for (int ty = minY; ty <= maxY; ++ty) {
        for (int tx = minX; tx <= maxX; ++tx) {
            Tile* tile = tileAtWorld(tx, ty);
            if (!tile || tile->type == TileType::Empty) {
                continue;
            }
            const Vec2 rectPos{static_cast<float>(tx * balance::TileSize), static_cast<float>(ty * balance::TileSize)};
            if (!circleIntersectsAabb(center, radius, rectPos, {static_cast<float>(balance::TileSize), static_cast<float>(balance::TileSize)})) {
                continue;
            }
            tile->hp = static_cast<unsigned char>(std::max(0, static_cast<int>(tile->hp) - damage));
            tile->flash = 8;
            if (tile->hp == 0) {
                tile->type = TileType::Empty;
                openedTiles.push_back(tileCenter(tx, ty));
            }
        }
    }
    return openedTiles;
}

bool TileMap::isSolidAt(Vec2 world)
{
    return isTileSolid(worldToTile(world.x), worldToTile(world.y));
}

bool TileMap::isTileSolid(int tx, int ty)
{
    Tile* tile = tileAtWorld(tx, ty);
    return tile && tile->type != TileType::Empty;
}

bool TileMap::isCircleBlocked(Vec2 center, float radius)
{
    const float sample = radius * 0.55f;
    const Vec2 points[] = {
        center,
        center + Vec2{sample, 0.0f},
        center + Vec2{-sample, 0.0f},
        center + Vec2{0.0f, sample},
        center + Vec2{0.0f, -sample},
        center + Vec2{sample, sample},
        center + Vec2{-sample, sample},
        center + Vec2{sample, -sample},
        center + Vec2{-sample, -sample},
    };
    for (Vec2 point : points) {
        if (isSolidAt(point)) {
            return true;
        }
    }
    return false;
}

Vec2 TileMap::tileCenter(int tx, int ty) const
{
    return {
        static_cast<float>(tx * balance::TileSize) + balance::TileSize * 0.5f,
        static_cast<float>(ty * balance::TileSize) + balance::TileSize * 0.5f
    };
}

int TileMap::worldToTile(float value) const
{
    return static_cast<int>(std::floor(value / balance::TileSize));
}

bool TileMap::isLit(Vec2 world, Vec2 playerLight, const std::vector<Vec2>& extraLights) const
{
    const float radiusSq = balanceSnapshot_.lightRadius * balanceSnapshot_.lightRadius;
    if (distanceSquared(world, playerLight) <= radiusSq) {
        return true;
    }
    for (Vec2 light : extraLights) {
        if (distanceSquared(world, light) <= radiusSq) {
            return true;
        }
    }
    return false;
}

Color TileMap::tileColor(const Tile& tile) const
{
    Color base{9, 9, 13, 255};
    switch (tile.type) {
    case TileType::Empty: base = {14, 14, 20, 255}; break;
    case TileType::Dirt: base = {105, 68, 37, 255}; break;
    case TileType::Rock: base = {64, 66, 72, 255}; break;
    case TileType::Ore: base = {70, 76, 130, 255}; break;
    }
    if (tile.flash > 0) {
        base = {static_cast<unsigned char>(std::min(255, base.r + 70)), static_cast<unsigned char>(std::min(255, base.g + 55)), base.b, 255};
    }
    return base;
}

void TileMap::render(Renderer& renderer, const Camera&, Vec2 lightCenter, const std::vector<Vec2>& extraLights)
{
    for (int cy = centerChunkY_ - balance::ActiveChunkRadius; cy <= centerChunkY_ + balance::ActiveChunkRadius; ++cy) {
        for (int cx = centerChunkX_ - balance::ActiveChunkRadius; cx <= centerChunkX_ + balance::ActiveChunkRadius; ++cx) {
            Chunk& chunk = getOrCreateChunk(cx, cy, balanceSnapshot_);
            for (int y = 0; y < balance::ChunkTiles; ++y) {
                for (int x = 0; x < balance::ChunkTiles; ++x) {
                    const int tx = cx * balance::ChunkTiles + x;
                    const int ty = cy * balance::ChunkTiles + y;
                    const Vec2 pos{static_cast<float>(tx * balance::TileSize), static_cast<float>(ty * balance::TileSize)};
                    const Vec2 center = pos + Vec2{balance::TileSize * 0.5f, balance::TileSize * 0.5f};
                    if (!isLit(center, lightCenter, extraLights)) {
                        continue;
                    }
                    renderer.fillRect(pos, {static_cast<float>(balance::TileSize), static_cast<float>(balance::TileSize)}, tileColor(chunk.at(x, y)));
                }
            }
        }
    }
}

}
