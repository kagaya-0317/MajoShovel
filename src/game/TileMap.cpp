#include "game/TileMap.hpp"

#include "data/GameBalance.hpp"
#include "game/Collision.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace majo {

namespace {

constexpr float GoalCavernRadius = 9.0f;

std::uint32_t hashTile(int x, int y, std::uint32_t seed)
{
    std::uint32_t h = seed ^ 0x9E3779B9u;
    h ^= static_cast<std::uint32_t>(x) + 0x85EBCA6Bu + (h << 6) + (h >> 2);
    h ^= static_cast<std::uint32_t>(y) + 0xC2B2AE35u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

float noise01(int x, int y, std::uint32_t seed)
{
    return static_cast<float>(hashTile(x, y, seed) & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

float smoothNoise(int x, int y, std::uint32_t seed)
{
    const float center = noise01(x, y, seed) * 4.0f;
    const float cardinal =
        noise01(x + 1, y, seed) +
        noise01(x - 1, y, seed) +
        noise01(x, y + 1, seed) +
        noise01(x, y - 1, seed);
    const float diagonal =
        noise01(x + 1, y + 1, seed) +
        noise01(x - 1, y + 1, seed) +
        noise01(x + 1, y - 1, seed) +
        noise01(x - 1, y - 1, seed);
    return (center + cardinal * 2.0f + diagonal) / 16.0f;
}

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

float distanceToPath(Vec2 point, const std::vector<Vec2>& points)
{
    if (points.empty()) {
        return 1.0e30f;
    }
    if (points.size() == 1) {
        return length(point - points.front());
    }

    float best = 1.0e30f;
    for (std::size_t i = 1; i < points.size(); ++i) {
        const Vec2 a = points[i - 1];
        const Vec2 b = points[i];
        const Vec2 ab = b - a;
        const float lenSq = lengthSquared(ab);
        float t = 0.0f;
        if (lenSq > 0.0001f) {
            t = clamp(dot(point - a, ab) / lenSq, 0.0f, 1.0f);
        }
        best = std::min(best, length(point - (a + ab * t)));
    }
    return best;
}

int scaledHp(int baseHp, float stageHardnessMultiplier, float localHardnessMultiplier)
{
    return std::max(1, static_cast<int>(std::round(
        static_cast<float>(std::max(1, baseHp)) *
        std::max(0.25f, stageHardnessMultiplier) *
        std::max(0.25f, localHardnessMultiplier))));
}

int clampTileHp(int hp)
{
    return std::clamp(hp, 0, 255);
}

int baseHpFor(TileType type, const RuntimeBalance& config)
{
    switch (type) {
    case TileType::Dirt:
        return config.dirtHp;
    case TileType::Rock:
        return config.rockHp;
    case TileType::Ore:
        return config.oreHp;
    case TileType::HardRock:
        return std::max(config.rockHp + 1, config.rockHp * 2);
    case TileType::Empty:
        return 0;
    }
    return config.dirtHp;
}

}

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
    (void)config;
    for (int y = 0; y < balance::ChunkTiles; ++y) {
        for (int x = 0; x < balance::ChunkTiles; ++x) {
            const int wx = chunk.cx * balance::ChunkTiles + x;
            const int wy = chunk.cy * balance::ChunkTiles + y;
            Tile& tile = chunk.at(x, y);
            const TerrainDebugInfo info = terrainInfoForTile(wx, wy, nullptr);
            tile.type = info.type;
            tile.hp = static_cast<unsigned char>(clampTileHp(info.effectiveHp));
            if (tile.type == TileType::Empty) {
                tile.hp = 0;
            }
        }
    }
}

void TileMap::updateAround(Vec2 worldCenter, float, const RuntimeBalance& config, const DungeonLayout& dungeonLayout)
{
    balanceSnapshot_ = config;
    dungeonLayoutSnapshot_ = dungeonLayout;
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

const Tile* TileMap::tileAtWorldIfGenerated(int tx, int ty) const
{
    const int cx = floorDiv(tx, balance::ChunkTiles);
    const int cy = floorDiv(ty, balance::ChunkTiles);
    const int lx = floorMod(tx, balance::ChunkTiles);
    const int ly = floorMod(ty, balance::ChunkTiles);
    const auto it = chunks_.find(key(cx, cy));
    if (it == chunks_.end()) {
        return nullptr;
    }
    return &it->second.at(lx, ly);
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

bool TileMap::damageTile(int tx, int ty, int damage, Vec2& openedTileCenter, TileType* openedTileType)
{
    Tile* tile = tileAtWorld(tx, ty);
    if (!tile || tile->type == TileType::Empty) {
        return false;
    }

    const TileType destroyedType = tile->type;
    tile->hp = static_cast<unsigned char>(std::max(0, static_cast<int>(tile->hp) - damage));
    tile->flash = 8;
    if (tile->hp == 0) {
        tile->type = TileType::Empty;
        openedTileCenter = tileCenter(tx, ty);
        if (openedTileType) {
            *openedTileType = destroyedType;
        }
        return true;
    }
    return false;
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

bool TileMap::isLit(Vec2 world, Vec2 playerLight, const std::vector<LightSource>& extraLights) const
{
    const float radiusSq = balanceSnapshot_.playerLightRadius * balanceSnapshot_.playerLightRadius;
    if (distanceSquared(world, playerLight) <= radiusSq) {
        return true;
    }
    for (const LightSource& light : extraLights) {
        const float lightRadius = light.radius > 0.0f ? light.radius : balanceSnapshot_.lightRadius;
        if (distanceSquared(world, light.position) <= lightRadius * lightRadius) {
            return true;
        }
    }
    return false;
}

TerrainDebugInfo TileMap::terrainInfoForTile(int tx, int ty, const Tile* tile) const
{
    TerrainDebugInfo info;
    const DungeonLayoutMetrics metrics = calculateDungeonLayoutMetrics(
        dungeonLayoutSnapshot_,
        {static_cast<float>(tx), static_cast<float>(ty)});
    info.distanceFromMainPath = metrics.distanceFromMainPath;

    const Vec2 tilePosition{static_cast<float>(tx), static_cast<float>(ty)};
    float carvedDistance = metrics.distanceFromMainPath;
    for (const DungeonPath& branch : dungeonLayoutSnapshot_.branchPathPoints) {
        carvedDistance = std::min(carvedDistance, distanceToPath(tilePosition, branch.points));
    }

    const float broadNoise = smoothNoise(tx / 3, ty / 3, dungeonLayoutSnapshot_.seed);
    const float fineNoise = smoothNoise(tx, ty, dungeonLayoutSnapshot_.seed ^ 0xA511E9B3u);
    const float widthNoise = smoothNoise(tx / 8, ty / 8, dungeonLayoutSnapshot_.seed ^ 0x63D83595u);
    const float caveWidth = 3.9f + (widthNoise - 0.5f) * 2.2f;
    const float edgeJitter = (broadNoise - 0.5f) * 2.8f + (fineNoise - 0.5f) * 0.9f;
    const float shapedDistance = carvedDistance + edgeJitter;
    const Vec2 goal = {
        static_cast<float>(dungeonLayoutSnapshot_.goalTile.x),
        static_cast<float>(dungeonLayoutSnapshot_.goalTile.y),
    };
    const float distanceFromGoal = length(tilePosition - goal);
    float distanceFromWarp = 1.0e30f;
    for (Vec2 warpAnchor : dungeonLayoutSnapshot_.warpPointAnchors) {
        distanceFromWarp = std::min(distanceFromWarp, length(tilePosition - warpAnchor));
    }
    SpecialRoomType currentRoomType = SpecialRoomType::None;
    float distanceFromSpecialRoomCenter = 1.0e30f;
    float specialRoomRadius = 0.0f;
    for (const SpecialRoomAnchor& room : dungeonLayoutSnapshot_.specialRoomAnchors) {
        const float distanceToRoom = length(tilePosition - room.center);
        if (distanceToRoom <= room.radius) {
            currentRoomType = room.type;
            distanceFromSpecialRoomCenter = distanceToRoom;
            specialRoomRadius = room.radius;
            break;
        }
        if (room.type == SpecialRoomType::TreasureRoom && distanceToRoom <= room.radius + 2.0f) {
            currentRoomType = room.type;
            distanceFromSpecialRoomCenter = distanceToRoom;
            specialRoomRadius = room.radius;
        } else if (room.type == SpecialRoomType::OreRoom && distanceToRoom <= room.radius + 1.5f) {
            currentRoomType = room.type;
            distanceFromSpecialRoomCenter = distanceToRoom;
            specialRoomRadius = room.radius;
        }
    }

    TileType generatedType = TileType::Rock;
    if (currentRoomType == SpecialRoomType::SafeCavern && distanceFromSpecialRoomCenter <= specialRoomRadius) {
        generatedType = TileType::Empty;
    } else if (currentRoomType == SpecialRoomType::CoinRoom && distanceFromSpecialRoomCenter <= specialRoomRadius) {
        generatedType = distanceFromSpecialRoomCenter <= specialRoomRadius * 0.88f ? TileType::Empty : TileType::Dirt;
    } else if (currentRoomType == SpecialRoomType::EnemyRoom && distanceFromSpecialRoomCenter <= specialRoomRadius) {
        generatedType = distanceFromSpecialRoomCenter <= specialRoomRadius * 0.78f ? TileType::Empty : TileType::Rock;
    } else if (currentRoomType == SpecialRoomType::OreRoom && distanceFromSpecialRoomCenter <= specialRoomRadius + 1.5f) {
        if (distanceFromSpecialRoomCenter <= specialRoomRadius * 0.50f) {
            generatedType = TileType::Empty;
        } else if (distanceFromSpecialRoomCenter <= specialRoomRadius) {
            generatedType = fineNoise > 0.28f ? TileType::Ore : TileType::Rock;
        } else {
            generatedType = fineNoise > 0.45f ? TileType::Ore : TileType::Rock;
        }
    } else if (currentRoomType == SpecialRoomType::TreasureRoom && distanceFromSpecialRoomCenter <= specialRoomRadius + 2.0f) {
        if (distanceFromSpecialRoomCenter <= specialRoomRadius * 0.58f) {
            generatedType = TileType::Empty;
        } else {
            generatedType = TileType::HardRock;
        }
    } else if (distanceFromWarp <= 2.35f) {
        generatedType = TileType::Empty;
    } else if (distanceFromGoal <= GoalCavernRadius + (widthNoise - 0.5f) * 2.0f) {
        generatedType = TileType::Empty;
    } else if (shapedDistance <= caveWidth) {
        generatedType = TileType::Empty;
    } else if (shapedDistance <= caveWidth + 3.4f) {
        generatedType = TileType::Dirt;
    } else if (shapedDistance <= caveWidth + 10.0f) {
        generatedType = fineNoise > 0.82f ? TileType::Ore : TileType::Rock;
    } else {
        generatedType = broadNoise > 0.62f ? TileType::HardRock : TileType::Rock;
        if (fineNoise > 0.92f) {
            generatedType = TileType::Ore;
        }
    }

    const float distanceHardness = clamp((carvedDistance - caveWidth) / 22.0f, 0.0f, 1.0f);
    info.localHardnessMultiplier = 0.85f + distanceHardness * 0.75f + (broadNoise - 0.5f) * 0.28f;
    if (generatedType == TileType::HardRock) {
        info.localHardnessMultiplier += 0.35f;
    }
    if (currentRoomType == SpecialRoomType::TreasureRoom && generatedType == TileType::HardRock) {
        info.localHardnessMultiplier += 0.25f;
    } else if (currentRoomType == SpecialRoomType::SafeCavern) {
        info.localHardnessMultiplier *= 0.8f;
    }
    info.localHardnessMultiplier = std::max(0.35f, info.localHardnessMultiplier);

    info.type = tile != nullptr ? tile->type : generatedType;
    info.hp = tile != nullptr ? static_cast<int>(tile->hp) : 0;
    const TileType hpType = tile != nullptr ? tile->type : generatedType;
    info.effectiveHp = hpType == TileType::Empty
        ? 0
        : scaledHp(
            baseHpFor(hpType, balanceSnapshot_),
            dungeonLayoutSnapshot_.stageHardnessMultiplier,
            info.localHardnessMultiplier);
    return info;
}

TerrainDebugInfo TileMap::terrainDebugAtWorld(Vec2 world) const
{
    const int tx = worldToTile(world.x);
    const int ty = worldToTile(world.y);
    const Tile* tile = tileAtWorldIfGenerated(tx, ty);
    return terrainInfoForTile(tx, ty, tile);
}

Color TileMap::tileColor(const Tile& tile) const
{
    Color base{9, 9, 13, 255};
    switch (tile.type) {
    case TileType::Empty: base = {14, 14, 20, 255}; break;
    case TileType::Dirt: base = {105, 68, 37, 255}; break;
    case TileType::Rock: base = {64, 66, 72, 255}; break;
    case TileType::Ore: base = {70, 76, 130, 255}; break;
    case TileType::HardRock: base = {38, 40, 48, 255}; break;
    }
    if (tile.flash > 0) {
        base = {static_cast<unsigned char>(std::min(255, base.r + 70)), static_cast<unsigned char>(std::min(255, base.g + 55)), base.b, 255};
    }
    return base;
}

void TileMap::drawTileLitByCircles(Renderer& renderer, Vec2 pos, Color color, Vec2 playerLight, const std::vector<LightSource>& extraLights) const
{
    const float tileSize = static_cast<float>(balance::TileSize);
    std::vector<LightSource> lights;
    lights.reserve(extraLights.size() + 1);
    lights.push_back({playerLight, balanceSnapshot_.playerLightRadius});
    lights.insert(lights.end(), extraLights.begin(), extraLights.end());

    for (const LightSource& light : lights) {
        const float radius = light.radius > 0.0f ? light.radius : balanceSnapshot_.lightRadius;
        const float radiusSq = radius * radius;
        const Vec2 corners[] = {
            pos,
            pos + Vec2{tileSize, 0.0f},
            pos + Vec2{0.0f, tileSize},
            pos + Vec2{tileSize, tileSize},
        };
        bool fullyInside = true;
        for (Vec2 corner : corners) {
            if (distanceSquared(corner, light.position) > radiusSq) {
                fullyInside = false;
                break;
            }
        }
        if (fullyInside) {
            renderer.fillRect(pos, {tileSize, tileSize}, color);
            return;
        }
    }

    for (int row = 0; row < balance::TileSize; ++row) {
        const float y = pos.y + static_cast<float>(row) + 0.5f;
        std::vector<Vec2> spans;
        spans.reserve(lights.size());
        for (const LightSource& light : lights) {
            const float radius = light.radius > 0.0f ? light.radius : balanceSnapshot_.lightRadius;
            const float radiusSq = radius * radius;
            const float dy = y - light.position.y;
            const float remaining = radiusSq - dy * dy;
            if (remaining <= 0.0f) {
                continue;
            }
            const float dx = std::sqrt(remaining);
            const float x0 = std::max(pos.x, light.position.x - dx);
            const float x1 = std::min(pos.x + tileSize, light.position.x + dx);
            if (x1 > x0) {
                spans.push_back({x0, x1});
            }
        }
        if (spans.empty()) {
            continue;
        }
        std::sort(spans.begin(), spans.end(), [](Vec2 a, Vec2 b) {
            return a.x < b.x;
        });
        float start = spans[0].x;
        float end = spans[0].y;
        for (std::size_t i = 1; i < spans.size(); ++i) {
            if (spans[i].x <= end) {
                end = std::max(end, spans[i].y);
                continue;
            }
            renderer.fillRect({start, pos.y + static_cast<float>(row)}, {end - start, 1.0f}, color);
            start = spans[i].x;
            end = spans[i].y;
        }
        renderer.fillRect({start, pos.y + static_cast<float>(row)}, {end - start, 1.0f}, color);
    }
}

void TileMap::render(Renderer& renderer, const Camera&, Vec2 lightCenter, const std::vector<LightSource>& extraLights)
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
                    drawTileLitByCircles(renderer, pos, tileColor(chunk.at(x, y)), lightCenter, extraLights);
                }
            }
        }
    }
}

}
