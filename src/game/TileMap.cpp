#include "game/TileMap.hpp"

#include "data/GameBalance.hpp"
#include "engine/Log.hpp"
#include "game/Collision.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>

namespace majo {

namespace {

constexpr float StartCavernRadius = 3.2f;
constexpr float GoalCavernRadius = 9.0f;
constexpr float WarpCavernRadius = 4.1f;
constexpr float SoftPathMargin = 3.4f;
constexpr std::string_view CrackTexturePath = "assets/crack.png";
constexpr int CrackTextureFrames = 4;
constexpr int TerrainTileSheetColumns = 5;
constexpr int TerrainTileSheetRows = 10;
constexpr int TerrainTileSourceSize = 48;
constexpr int TerrainTileQuadrantSize = TerrainTileSourceSize / 2;
constexpr float TerrainTileDrawSize = static_cast<float>(balance::TileSize);
constexpr float LightFalloffMinWidth = 10.0f;
constexpr float LightFalloffMaxWidth = 42.0f;
constexpr float LightFalloffRadiusRatio = 0.20f;
constexpr float LightFalloffSegmentWidth = 8.0f;

enum class TerrainWallCell {
    Basic,
    Top,
    Bottom,
    Left,
    Right,
    OuterTopLeft,
    OuterTopRight,
    OuterBottomLeft,
    OuterBottomRight,
    InnerTopLeft,
    InnerTopRight,
    InnerBottomLeft,
    InnerBottomRight,
    Decor1,
    Decor2,
};

enum class TerrainQuadrant {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

struct TerrainTileSheet {
    ImageHandle handle{};
    std::string path;
    int stageId = 1;
    bool available = false;
};

struct TerrainNeighbors {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool upLeft = false;
    bool upRight = false;
    bool downLeft = false;
    bool downRight = false;
};

struct TerrainWallSelection {
    bool split = false;
    TerrainWallCell full = TerrainWallCell::Basic;
    std::array<TerrainWallCell, 4> quadrants{
        TerrainWallCell::Basic,
        TerrainWallCell::Basic,
        TerrainWallCell::Basic,
        TerrainWallCell::Basic,
    };
};

float lightFalloffInnerRadius(float radius)
{
    const float falloffWidth = std::clamp(
        radius * LightFalloffRadiusRatio,
        LightFalloffMinWidth,
        LightFalloffMaxWidth);
    return std::max(0.0f, radius - falloffWidth);
}

void mergeLightSpans(std::vector<Vec2>& spans)
{
    if (spans.empty()) {
        return;
    }

    std::sort(spans.begin(), spans.end(), [](Vec2 a, Vec2 b) {
        return a.x < b.x;
    });

    std::size_t writeIndex = 0;
    for (std::size_t readIndex = 1; readIndex < spans.size(); ++readIndex) {
        if (spans[readIndex].x <= spans[writeIndex].y) {
            spans[writeIndex].y = std::max(spans[writeIndex].y, spans[readIndex].y);
            continue;
        }
        ++writeIndex;
        spans[writeIndex] = spans[readIndex];
    }
    spans.resize(writeIndex + 1);
}

void appendFalloffSpans(const std::vector<Vec2>& outerSpans, const std::vector<Vec2>& innerSpans, std::vector<Vec2>& outSpans)
{
    outSpans.clear();
    for (Vec2 outer : outerSpans) {
        float cursor = outer.x;
        for (Vec2 inner : innerSpans) {
            if (inner.y <= cursor) {
                continue;
            }
            if (inner.x >= outer.y) {
                break;
            }
            if (inner.x > cursor) {
                outSpans.push_back({cursor, std::min(inner.x, outer.y)});
            }
            cursor = std::max(cursor, inner.y);
            if (cursor >= outer.y) {
                break;
            }
        }
        if (cursor < outer.y) {
            outSpans.push_back({cursor, outer.y});
        }
    }
}

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

RectF terrainSheetCellSource(int row, int col)
{
    return RectF{
        static_cast<float>(col * TerrainTileSourceSize),
        static_cast<float>(row * TerrainTileSourceSize),
        static_cast<float>(TerrainTileSourceSize),
        static_cast<float>(TerrainTileSourceSize),
    };
}

RectF terrainSheetQuadrantSource(RectF cell, TerrainQuadrant quadrant)
{
    const float half = static_cast<float>(TerrainTileQuadrantSize);
    switch (quadrant) {
    case TerrainQuadrant::TopLeft:
        return {cell.x, cell.y, half, half};
    case TerrainQuadrant::TopRight:
        return {cell.x + half, cell.y, half, half};
    case TerrainQuadrant::BottomLeft:
        return {cell.x, cell.y + half, half, half};
    case TerrainQuadrant::BottomRight:
        return {cell.x + half, cell.y + half, half, half};
    }
    return {cell.x, cell.y, half, half};
}

Vec2 terrainQuadrantOffset(TerrainQuadrant quadrant)
{
    const float half = TerrainTileDrawSize * 0.5f;
    switch (quadrant) {
    case TerrainQuadrant::TopLeft:
        return {0.0f, 0.0f};
    case TerrainQuadrant::TopRight:
        return {half, 0.0f};
    case TerrainQuadrant::BottomLeft:
        return {0.0f, half};
    case TerrainQuadrant::BottomRight:
        return {half, half};
    }
    return {};
}

std::string terrainTileSheetPathForStage(int stageId)
{
    return "assets/tiles/tile_" + std::to_string(std::clamp(stageId, 1, 4)) + ".png";
}

TerrainTileSheet acquireTerrainTileSheet(Renderer& renderer, int stageId)
{
    struct LogState {
        std::uint32_t handleValue = 0;
        bool available = false;
        bool initialized = false;
    };
    static std::unordered_map<std::string, LogState> logStates;

    TerrainTileSheet sheet;
    sheet.stageId = std::clamp(stageId, 1, 4);
    sheet.path = terrainTileSheetPathForStage(sheet.stageId);
    sheet.handle = renderer.acquireImage(sheet.path, TextureFilter::Nearest);

    Vec2 imageSize{};
    const bool loaded = sheet.handle.valid() && renderer.getImageSize(sheet.handle, imageSize);
    const bool validSize =
        loaded &&
        imageSize.x >= static_cast<float>(TerrainTileSheetColumns * TerrainTileSourceSize) &&
        imageSize.y >= static_cast<float>(TerrainTileSheetRows * TerrainTileSourceSize);
    sheet.available = validSize;

    LogState& state = logStates[sheet.path];
    if (!state.initialized || state.handleValue != sheet.handle.value || state.available != sheet.available) {
        if (sheet.available) {
            logInfo(
                "Terrain tile sheet loaded: stage=" + std::to_string(sheet.stageId) +
                " path=" + sheet.path +
                " size=" + std::to_string(static_cast<int>(std::round(imageSize.x))) +
                "x" + std::to_string(static_cast<int>(std::round(imageSize.y))));
        } else {
            std::string reason;
            if (!loaded) {
                reason = renderer.lastAssetError();
                if (reason.empty()) {
                    reason = "image is not available";
                }
            } else {
                reason =
                    "invalid sheet size " +
                    std::to_string(static_cast<int>(std::round(imageSize.x))) +
                    "x" +
                    std::to_string(static_cast<int>(std::round(imageSize.y))) +
                    ", expected at least 240x480";
            }
            logWarning(
                "Terrain tile sheet fallback: stage=" + std::to_string(sheet.stageId) +
                " path=" + sheet.path +
                " reason=" + reason);
        }
        state = LogState{sheet.handle.value, sheet.available, true};
    }

    return sheet;
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

int terrainWallBaseRow(TileType type)
{
    switch (type) {
    case TileType::Dirt:
        return 1;
    case TileType::Rock:
    case TileType::HardRock:
        return 4;
    case TileType::Ore:
        return 7;
    case TileType::Empty:
        return 1;
    }
    return 1;
}

RectF terrainWallCellSource(TileType type, TerrainWallCell cell)
{
    const int baseRow = terrainWallBaseRow(type);
    int row = baseRow + 1;
    int col = 1;
    switch (cell) {
    case TerrainWallCell::Basic:
        row = baseRow + 1;
        col = 1;
        break;
    case TerrainWallCell::Top:
        row = baseRow + 0;
        col = 1;
        break;
    case TerrainWallCell::Bottom:
        row = baseRow + 2;
        col = 1;
        break;
    case TerrainWallCell::Left:
        row = baseRow + 1;
        col = 0;
        break;
    case TerrainWallCell::Right:
        row = baseRow + 1;
        col = 2;
        break;
    case TerrainWallCell::OuterTopLeft:
        row = baseRow + 0;
        col = 0;
        break;
    case TerrainWallCell::OuterTopRight:
        row = baseRow + 0;
        col = 2;
        break;
    case TerrainWallCell::OuterBottomLeft:
        row = baseRow + 2;
        col = 0;
        break;
    case TerrainWallCell::OuterBottomRight:
        row = baseRow + 2;
        col = 2;
        break;
    case TerrainWallCell::InnerTopLeft:
        row = baseRow + 1;
        col = 4;
        break;
    case TerrainWallCell::InnerTopRight:
        row = baseRow + 1;
        col = 3;
        break;
    case TerrainWallCell::InnerBottomLeft:
        row = baseRow + 0;
        col = 4;
        break;
    case TerrainWallCell::InnerBottomRight:
        row = baseRow + 0;
        col = 3;
        break;
    case TerrainWallCell::Decor1:
        row = baseRow + 2;
        col = 3;
        break;
    case TerrainWallCell::Decor2:
        row = baseRow + 2;
        col = 4;
        break;
    }
    return terrainSheetCellSource(row, col);
}

TerrainWallCell terrainBasicWallVariant(int tx, int ty, TileType type)
{
    const std::uint32_t materialSalt =
        type == TileType::Ore ? 0x785A2FE3u :
        type == TileType::Rock || type == TileType::HardRock ? 0x5B7D91A1u :
        0x3E4C6A17u;
    const std::uint32_t h = hashTile(tx, ty, materialSalt);
    const std::uint32_t roll = h % 1000U;
    if (roll < 35U) {
        return TerrainWallCell::Decor1;
    }
    if (roll < 70U) {
        return TerrainWallCell::Decor2;
    }
    return TerrainWallCell::Basic;
}

int terrainFloorColumn(int tx, int ty)
{
    const std::uint32_t h = hashTile(tx, ty, 0xB47C91E5u);
    if (h % 100U < 24U) {
        return 1 + static_cast<int>((h >> 8) % 4U);
    }
    return 0;
}

int trueCount(std::array<bool, 4> values)
{
    return static_cast<int>(std::count(values.begin(), values.end(), true));
}

TerrainWallCell terrainQuadrantCell(TerrainQuadrant quadrant, const TerrainNeighbors& neighbors)
{
    switch (quadrant) {
    case TerrainQuadrant::TopLeft:
        if (!neighbors.up && !neighbors.left && neighbors.upLeft) {
            return TerrainWallCell::InnerTopLeft;
        }
        if (neighbors.up && neighbors.left) {
            return TerrainWallCell::OuterTopLeft;
        }
        if (neighbors.up) {
            return TerrainWallCell::Top;
        }
        if (neighbors.left) {
            return TerrainWallCell::Left;
        }
        return TerrainWallCell::Basic;
    case TerrainQuadrant::TopRight:
        if (!neighbors.up && !neighbors.right && neighbors.upRight) {
            return TerrainWallCell::InnerTopRight;
        }
        if (neighbors.up && neighbors.right) {
            return TerrainWallCell::OuterTopRight;
        }
        if (neighbors.up) {
            return TerrainWallCell::Top;
        }
        if (neighbors.right) {
            return TerrainWallCell::Right;
        }
        return TerrainWallCell::Basic;
    case TerrainQuadrant::BottomLeft:
        if (!neighbors.down && !neighbors.left && neighbors.downLeft) {
            return TerrainWallCell::InnerBottomLeft;
        }
        if (neighbors.down && neighbors.left) {
            return TerrainWallCell::OuterBottomLeft;
        }
        if (neighbors.down) {
            return TerrainWallCell::Bottom;
        }
        if (neighbors.left) {
            return TerrainWallCell::Left;
        }
        return TerrainWallCell::Basic;
    case TerrainQuadrant::BottomRight:
        if (!neighbors.down && !neighbors.right && neighbors.downRight) {
            return TerrainWallCell::InnerBottomRight;
        }
        if (neighbors.down && neighbors.right) {
            return TerrainWallCell::OuterBottomRight;
        }
        if (neighbors.down) {
            return TerrainWallCell::Bottom;
        }
        if (neighbors.right) {
            return TerrainWallCell::Right;
        }
        return TerrainWallCell::Basic;
    }
    return TerrainWallCell::Basic;
}

TerrainWallSelection chooseTerrainWallSelection(TileType type, int tx, int ty, const TerrainNeighbors& neighbors)
{
    const std::array<bool, 4> inner{
        !neighbors.up && !neighbors.left && neighbors.upLeft,
        !neighbors.up && !neighbors.right && neighbors.upRight,
        !neighbors.down && !neighbors.left && neighbors.downLeft,
        !neighbors.down && !neighbors.right && neighbors.downRight,
    };
    const std::array<bool, 4> outer{
        neighbors.up && neighbors.left,
        neighbors.up && neighbors.right,
        neighbors.down && neighbors.left,
        neighbors.down && neighbors.right,
    };
    const std::array<bool, 4> edge{
        neighbors.up,
        neighbors.down,
        neighbors.left,
        neighbors.right,
    };

    const int innerCount = trueCount(inner);
    const int outerCount = trueCount(outer);
    const int edgeCount = trueCount(edge);

    TerrainWallSelection selection;
    if (innerCount == 0 && outerCount == 0 && edgeCount == 0) {
        selection.full = terrainBasicWallVariant(tx, ty, type);
        return selection;
    }

    selection.split = true;
    selection.quadrants = {
        terrainQuadrantCell(TerrainQuadrant::TopLeft, neighbors),
        terrainQuadrantCell(TerrainQuadrant::TopRight, neighbors),
        terrainQuadrantCell(TerrainQuadrant::BottomLeft, neighbors),
        terrainQuadrantCell(TerrainQuadrant::BottomRight, neighbors),
    };
    return selection;
}

bool drawTerrainImageRegion(Renderer& renderer, ImageHandle handle, RectF source, Vec2 pos, Vec2 size)
{
    ImageDrawOptions options;
    options.anchor = {0.0f, 0.0f};
    return renderer.drawImageRegion(handle, source, pos, size, options);
}

bool drawTerrainFloorTile(Renderer& renderer, const TerrainTileSheet& sheet, Vec2 pos, int tx, int ty)
{
    return drawTerrainImageRegion(
        renderer,
        sheet.handle,
        terrainSheetCellSource(0, terrainFloorColumn(tx, ty)),
        pos,
        {TerrainTileDrawSize, TerrainTileDrawSize});
}

bool drawTerrainWallTile(
    Renderer& renderer,
    const TerrainTileSheet& sheet,
    Vec2 pos,
    int tx,
    int ty,
    TileType type,
    const TerrainNeighbors& neighbors)
{
    const TerrainWallSelection selection = chooseTerrainWallSelection(type, tx, ty, neighbors);
    if (!selection.split) {
        return drawTerrainImageRegion(
            renderer,
            sheet.handle,
            terrainWallCellSource(type, selection.full),
            pos,
            {TerrainTileDrawSize, TerrainTileDrawSize});
    }

    bool ok = true;
    constexpr std::array<TerrainQuadrant, 4> quadrants{
        TerrainQuadrant::TopLeft,
        TerrainQuadrant::TopRight,
        TerrainQuadrant::BottomLeft,
        TerrainQuadrant::BottomRight,
    };
    const Vec2 quadrantSize{TerrainTileDrawSize * 0.5f, TerrainTileDrawSize * 0.5f};
    for (std::size_t i = 0; i < quadrants.size(); ++i) {
        const TerrainQuadrant quadrant = quadrants[i];
        const RectF sourceCell = terrainWallCellSource(type, selection.quadrants[i]);
        ok = drawTerrainImageRegion(
                 renderer,
                 sheet.handle,
                 terrainSheetQuadrantSource(sourceCell, quadrant),
                 pos + terrainQuadrantOffset(quadrant),
                 quadrantSize) && ok;
    }
    return ok;
}

bool drawTerrainTileImage(
    Renderer& renderer,
    const TerrainTileSheet& sheet,
    Vec2 pos,
    int tx,
    int ty,
    const Tile& tile,
    const TerrainNeighbors& neighbors)
{
    if (!sheet.available) {
        return false;
    }

    if (!drawTerrainFloorTile(renderer, sheet, pos, tx, ty)) {
        return false;
    }
    if (tile.type == TileType::Empty) {
        return true;
    }
    return drawTerrainWallTile(renderer, sheet, pos, tx, ty, tile.type, neighbors);
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

TileType mixedSoftPathTile(float fineNoise, float broadNoise)
{
    if (fineNoise < 0.14f) {
        return TileType::Empty;
    }
    if (fineNoise > 0.92f) {
        return TileType::Ore;
    }
    if (fineNoise > 0.72f) {
        return TileType::Rock;
    }
    if (broadNoise > 0.80f && fineNoise < 0.24f) {
        return TileType::Rock;
    }
    return TileType::Dirt;
}

TileType mixedRockBandTile(float fineNoise, float broadNoise)
{
    if (fineNoise < 0.10f) {
        return TileType::Empty;
    }
    if (fineNoise < 0.22f) {
        return TileType::Dirt;
    }
    if (fineNoise > 0.84f) {
        return TileType::Ore;
    }
    if (broadNoise > 0.66f && fineNoise > 0.38f) {
        return TileType::HardRock;
    }
    return TileType::Rock;
}

TileType mixedDeepWallTile(float fineNoise, float broadNoise)
{
    if (fineNoise < 0.07f) {
        return TileType::Empty;
    }
    if (fineNoise < 0.16f) {
        return TileType::Dirt;
    }
    if (fineNoise > 0.88f) {
        return TileType::Ore;
    }
    return broadNoise > 0.52f ? TileType::HardRock : TileType::Rock;
}

TileType mixedOreRoomWallTile(float fineNoise, float broadNoise)
{
    if (fineNoise < 0.08f) {
        return TileType::Empty;
    }
    if (fineNoise < 0.18f) {
        return TileType::Dirt;
    }
    if (fineNoise > 0.34f) {
        return TileType::Ore;
    }
    return broadNoise > 0.70f ? TileType::HardRock : TileType::Rock;
}

TileType mixedTreasureWallTile(float fineNoise, float broadNoise)
{
    if (fineNoise < 0.06f) {
        return TileType::Empty;
    }
    if (fineNoise < 0.14f) {
        return TileType::Dirt;
    }
    if (fineNoise > 0.90f) {
        return TileType::Ore;
    }
    return broadNoise > 0.36f ? TileType::HardRock : TileType::Rock;
}

TileType interleavedWallTile(TileType base, float detailNoise, float scatterNoise, float veinNoise, bool nearPathWall)
{
    if (base == TileType::Empty) {
        return base;
    }

    const bool oreFleck = detailNoise > (nearPathWall ? 0.982f : 0.962f);
    const bool oreVein = veinNoise > (nearPathWall ? 0.900f : 0.820f) && scatterNoise > 0.720f;

    switch (base) {
    case TileType::Dirt:
        if (detailNoise < 0.040f) {
            return TileType::Empty;
        }
        if (oreFleck || (veinNoise > 0.890f && scatterNoise > 0.800f)) {
            return TileType::Ore;
        }
        if (detailNoise > 0.820f || (veinNoise > 0.760f && scatterNoise > 0.660f)) {
            return TileType::Rock;
        }
        return TileType::Dirt;
    case TileType::Rock:
        if (detailNoise < 0.055f) {
            return TileType::Empty;
        }
        if (detailNoise < 0.180f) {
            return TileType::Dirt;
        }
        if (oreFleck || oreVein) {
            return TileType::Ore;
        }
        if (detailNoise > 0.905f && veinNoise > 0.560f) {
            return TileType::HardRock;
        }
        return TileType::Rock;
    case TileType::HardRock:
        if (detailNoise < 0.025f) {
            return TileType::Empty;
        }
        if (detailNoise < 0.130f) {
            return TileType::Rock;
        }
        if (oreFleck || (oreVein && scatterNoise > 0.860f)) {
            return TileType::Ore;
        }
        return TileType::HardRock;
    case TileType::Ore:
        if (detailNoise < 0.030f) {
            return TileType::Rock;
        }
        return TileType::Ore;
    case TileType::Empty:
        return TileType::Empty;
    }
    return base;
}

TileType applyTerrainProfile(
    TileType base,
    std::string_view terrainProfile,
    float detailNoise,
    float scatterNoise,
    float veinNoise,
    float broadNoise,
    bool nearPathWall)
{
    if (base == TileType::Empty) {
        return base;
    }

    if (terrainProfile == "soft_stardust") {
        if (base == TileType::HardRock && detailNoise < 0.72f) {
            return TileType::Rock;
        }
        if (base == TileType::Rock && detailNoise < (nearPathWall ? 0.42f : 0.30f)) {
            return TileType::Dirt;
        }
        if (base == TileType::Dirt && detailNoise < 0.070f) {
            return TileType::Empty;
        }
        return base;
    }

    if (terrainProfile == "junk_mixed") {
        if (base == TileType::Dirt && detailNoise > 0.760f) {
            return TileType::Rock;
        }
        if (base == TileType::Rock && veinNoise > 0.740f && scatterNoise > 0.560f) {
            return TileType::Ore;
        }
        if (base == TileType::Rock && broadNoise > 0.760f && detailNoise > 0.540f) {
            return TileType::HardRock;
        }
        return base;
    }

    if (terrainProfile == "hard_star_core") {
        if (base == TileType::Dirt && detailNoise > 0.520f) {
            return TileType::Rock;
        }
        if (base == TileType::Rock && (detailNoise > 0.610f || broadNoise > 0.720f)) {
            return TileType::HardRock;
        }
        if (base == TileType::Ore && detailNoise < 0.080f) {
            return TileType::HardRock;
        }
        return base;
    }

    if (terrainProfile == "chaos_astral") {
        if (detailNoise < 0.050f) {
            return TileType::Empty;
        }
        if (veinNoise > 0.800f && scatterNoise > 0.540f) {
            return TileType::Ore;
        }
        if (detailNoise > 0.780f && broadNoise > 0.520f) {
            return TileType::HardRock;
        }
        if (detailNoise < 0.220f) {
            return TileType::Dirt;
        }
    }

    return base;
}

}

std::string_view terrainAttributeCode(TerrainAttribute attribute)
{
    switch (attribute) {
    case TerrainAttribute::None:
        return "none";
    case TerrainAttribute::Soft:
        return "soft";
    case TerrainAttribute::Hard:
        return "hard";
    case TerrainAttribute::Ore:
        return "ore";
    }
    return "none";
}

TerrainAttribute terrainAttributeForTileType(TileType type)
{
    switch (type) {
    case TileType::Dirt:
        return TerrainAttribute::Soft;
    case TileType::Rock:
    case TileType::HardRock:
        return TerrainAttribute::Hard;
    case TileType::Ore:
        return TerrainAttribute::Ore;
    case TileType::Empty:
        return TerrainAttribute::None;
    }
    return TerrainAttribute::None;
}

int adjustedTerrainDigDamage(int baseDamage, TerrainAttribute attribute, TerrainDigModifier modifier)
{
    if (baseDamage <= 0 || attribute == TerrainAttribute::None) {
        return 0;
    }

    if (modifier == TerrainDigModifier::HardSpecialist) {
        if (attribute == TerrainAttribute::Hard || attribute == TerrainAttribute::Ore) {
            return std::max(1, static_cast<int>(std::ceil(static_cast<double>(baseDamage) * 1.25)));
        }
        return std::max(1, static_cast<int>(std::floor(static_cast<double>(baseDamage) * 0.75)));
    }

    if (attribute == TerrainAttribute::Hard || attribute == TerrainAttribute::Ore) {
        return std::max(1, static_cast<int>(std::floor(static_cast<double>(baseDamage) * 0.50)));
    }
    return baseDamage;
}

long long TileMap::key(int cx, int cy)
{
    return (static_cast<long long>(cx) << 32) ^ (static_cast<unsigned int>(cy));
}

DungeonTile TileMap::tileFromKey(long long packed)
{
    const std::uint64_t raw = static_cast<std::uint64_t>(packed);
    const auto signedFromU32 = [](std::uint32_t value) {
        if (value <= static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
            return static_cast<int>(value);
        }
        return -1 - static_cast<int>(~value);
    };
    return {
        signedFromU32(static_cast<std::uint32_t>(raw >> 32)),
        signedFromU32(static_cast<std::uint32_t>(raw & 0xFFFFFFFFull)),
    };
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

void TileMap::rememberDamagedTileMaxHp(int tx, int ty, const Tile& tile)
{
    if (tile.type == TileType::Empty || tile.hp == 0) {
        return;
    }
    damagedTileMaxHp_.try_emplace(key(tx, ty), std::max(1, static_cast<int>(tile.hp)));
}

void TileMap::clearCrackCacheForTile(int tx, int ty)
{
    damagedTileMaxHp_.erase(key(tx, ty));
}

int TileMap::crackLevelForTile(int tx, int ty, const Tile& tile) const
{
    if (tile.type == TileType::Empty || tile.hp == 0) {
        return 0;
    }

    const auto it = damagedTileMaxHp_.find(key(tx, ty));
    if (it == damagedTileMaxHp_.end()) {
        return 0;
    }

    const int maxHp = std::max(1, it->second);
    const int hp = std::clamp(static_cast<int>(tile.hp), 0, maxHp);
    const float damageRatio = 1.0f - static_cast<float>(hp) / static_cast<float>(maxHp);
    if (damageRatio < 0.10f) {
        return 0;
    }
    if (damageRatio < 0.30f) {
        return 1;
    }
    if (damageRatio < 0.50f) {
        return 2;
    }
    if (damageRatio < 0.72f) {
        return 3;
    }
    return 4;
}

void TileMap::drawTileCracks(Renderer& renderer, Vec2 pos, int tx, int ty, const Tile& tile)
{
    const int level = crackLevelForTile(tx, ty, tile);
    if (level <= 0) {
        return;
    }

    const ImageHandle crackImage = renderer.acquireImage(CrackTexturePath, TextureFilter::Nearest);
    Vec2 imageSize{};
    if (!renderer.getImageSize(crackImage, imageSize) || imageSize.x <= 0.0f || imageSize.y <= 0.0f) {
        return;
    }

    const float frameWidth = imageSize.x / static_cast<float>(CrackTextureFrames);
    const RectF source{
        frameWidth * static_cast<float>(std::clamp(level, 1, CrackTextureFrames) - 1),
        0.0f,
        frameWidth,
        imageSize.y
    };
    renderer.drawImageRegion(
        crackImage,
        source,
        pos + Vec2{static_cast<float>(balance::TileSize) * 0.5f, static_cast<float>(balance::TileSize) * 0.5f},
        {static_cast<float>(balance::TileSize), static_cast<float>(balance::TileSize)});
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

void TileMap::setTileOverride(DungeonTile tile, TileType type)
{
    tileOverrides_[key(tile.x, tile.y)] = type;

    const int cx = floorDiv(tile.x, balance::ChunkTiles);
    const int cy = floorDiv(tile.y, balance::ChunkTiles);
    auto chunkIt = chunks_.find(key(cx, cy));
    if (chunkIt == chunks_.end()) {
        return;
    }

    const auto editIt = terrainEdits_.find(key(tile.x, tile.y));
    const TileType finalType = editIt != terrainEdits_.end() ? editIt->second : type;
    Tile& target = chunkIt->second.at(floorMod(tile.x, balance::ChunkTiles), floorMod(tile.y, balance::ChunkTiles));
    target.type = finalType;
    clearCrackCacheForTile(tile.x, tile.y);
    if (finalType == TileType::Empty) {
        target.hp = 0;
        return;
    }

    const TerrainDebugInfo info = terrainInfoForTile(tile.x, tile.y, nullptr);
    target.hp = static_cast<unsigned char>(clampTileHp(info.effectiveHp));
}

void TileMap::setTerrainEdit(DungeonTile tile, TileType type)
{
    terrainEdits_[key(tile.x, tile.y)] = type;

    const int cx = floorDiv(tile.x, balance::ChunkTiles);
    const int cy = floorDiv(tile.y, balance::ChunkTiles);
    auto chunkIt = chunks_.find(key(cx, cy));
    if (chunkIt == chunks_.end()) {
        return;
    }

    const auto editIt = terrainEdits_.find(key(tile.x, tile.y));
    const TileType finalType = editIt != terrainEdits_.end() ? editIt->second : type;
    Tile& target = chunkIt->second.at(floorMod(tile.x, balance::ChunkTiles), floorMod(tile.y, balance::ChunkTiles));
    target.type = finalType;
    clearCrackCacheForTile(tile.x, tile.y);
    if (finalType == TileType::Empty) {
        target.hp = 0;
        return;
    }

    const TerrainDebugInfo info = terrainInfoForTile(tile.x, tile.y, nullptr);
    target.hp = static_cast<unsigned char>(clampTileHp(info.effectiveHp));
}

std::vector<TerrainTileEdit> TileMap::terrainEditsForSave() const
{
    std::vector<TerrainTileEdit> edits;
    edits.reserve(terrainEdits_.size());
    for (const auto& [packed, type] : terrainEdits_) {
        edits.push_back(TerrainTileEdit{tileFromKey(packed), type});
    }
    std::sort(edits.begin(), edits.end(), [](const TerrainTileEdit& lhs, const TerrainTileEdit& rhs) {
        if (lhs.tile.x != rhs.tile.x) {
            return lhs.tile.x < rhs.tile.x;
        }
        return lhs.tile.y < rhs.tile.y;
    });
    return edits;
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

std::vector<DamagedTile> TileMap::damageCircle(Vec2 center, float radius, int damage)
{
    std::vector<DamagedTile> openedTiles;
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
            const TileType destroyedType = tile->type;
            const Color destroyedColor = tileColor(*tile);
            rememberDamagedTileMaxHp(tx, ty, *tile);
            tile->hp = static_cast<unsigned char>(std::max(0, static_cast<int>(tile->hp) - damage));
            if (tile->hp == 0) {
                tile->type = TileType::Empty;
                terrainEdits_[key(tx, ty)] = TileType::Empty;
                clearCrackCacheForTile(tx, ty);
                openedTiles.push_back({tileCenter(tx, ty), destroyedType, destroyedColor});
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
    rememberDamagedTileMaxHp(tx, ty, *tile);
    tile->hp = static_cast<unsigned char>(std::max(0, static_cast<int>(tile->hp) - damage));
    if (tile->hp == 0) {
        tile->type = TileType::Empty;
        terrainEdits_[key(tx, ty)] = TileType::Empty;
        clearCrackCacheForTile(tx, ty);
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

TerrainAttribute TileMap::terrainAttributeAtTile(int tx, int ty)
{
    Tile* tile = tileAtWorld(tx, ty);
    return tile != nullptr ? terrainAttributeForTileType(tile->type) : TerrainAttribute::None;
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

bool TileMap::isRectLit(Vec2 center, Vec2 size, Vec2 playerLight, const std::vector<LightSource>& extraLights) const
{
    const Vec2 halfSize{std::max(0.0f, size.x) * 0.5f, std::max(0.0f, size.y) * 0.5f};
    const Vec2 min{center.x - halfSize.x, center.y - halfSize.y};
    const Vec2 max{center.x + halfSize.x, center.y + halfSize.y};
    const auto circleIntersectsRect = [&](const LightSource& light) {
        const float radius = light.radius > 0.0f ? light.radius : balanceSnapshot_.lightRadius;
        const float nearestX = std::clamp(light.position.x, min.x, max.x);
        const float nearestY = std::clamp(light.position.y, min.y, max.y);
        return distanceSquared({nearestX, nearestY}, light.position) <= radius * radius;
    };

    if (circleIntersectsRect({playerLight, balanceSnapshot_.playerLightRadius})) {
        return true;
    }
    for (const LightSource& light : extraLights) {
        if (circleIntersectsRect(light)) {
            return true;
        }
    }
    return false;
}

bool TileMap::isTileRectLit(Vec2 pos, Vec2 playerLight, const std::vector<LightSource>& extraLights) const
{
    const float tileSize = static_cast<float>(balance::TileSize);
    const auto circleIntersectsTile = [&](const LightSource& light) {
        const float radius = light.radius > 0.0f ? light.radius : balanceSnapshot_.lightRadius;
        const float nearestX = std::clamp(light.position.x, pos.x, pos.x + tileSize);
        const float nearestY = std::clamp(light.position.y, pos.y, pos.y + tileSize);
        return distanceSquared({nearestX, nearestY}, light.position) <= radius * radius;
    };

    if (circleIntersectsTile({playerLight, balanceSnapshot_.playerLightRadius})) {
        return true;
    }
    for (const LightSource& light : extraLights) {
        if (circleIntersectsTile(light)) {
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
    const float detailNoise = noise01(tx, ty, dungeonLayoutSnapshot_.seed ^ 0x1B56C4E9u);
    const float scatterNoise = noise01(tx * 3 + 17, ty * 5 - 11, dungeonLayoutSnapshot_.seed ^ 0x91E10DA5u);
    const float veinNoise = smoothNoise(tx / 2, ty / 2, dungeonLayoutSnapshot_.seed ^ 0x4F1BBCDCu);
    const float caveWidth = (3.9f + (widthNoise - 0.5f) * 2.2f) *
        std::max(0.35f, dungeonLayoutSnapshot_.cavernWidthMultiplier);
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
        generatedType = distanceFromSpecialRoomCenter <= specialRoomRadius * 0.78f
            ? TileType::Empty
            : mixedRockBandTile(fineNoise, broadNoise);
    } else if (currentRoomType == SpecialRoomType::OreRoom && distanceFromSpecialRoomCenter <= specialRoomRadius + 1.5f) {
        if (distanceFromSpecialRoomCenter <= specialRoomRadius * 0.50f) {
            generatedType = TileType::Empty;
        } else if (distanceFromSpecialRoomCenter <= specialRoomRadius) {
            generatedType = mixedOreRoomWallTile(fineNoise, broadNoise);
        } else {
            generatedType = mixedOreRoomWallTile(fineNoise, broadNoise);
        }
    } else if (currentRoomType == SpecialRoomType::TreasureRoom && distanceFromSpecialRoomCenter <= specialRoomRadius + 2.0f) {
        if (distanceFromSpecialRoomCenter <= specialRoomRadius * 0.58f) {
            generatedType = TileType::Empty;
        } else {
            generatedType = mixedTreasureWallTile(fineNoise, broadNoise);
        }
    } else if (metrics.distanceFromStart <= StartCavernRadius + (widthNoise - 0.5f) * 0.8f) {
        generatedType = TileType::Empty;
    } else if (distanceFromWarp <= WarpCavernRadius) {
        generatedType = TileType::Empty;
    } else if (distanceFromGoal <= GoalCavernRadius + (widthNoise - 0.5f) * 2.0f) {
        generatedType = TileType::Empty;
    } else if (shapedDistance <= caveWidth + SoftPathMargin) {
        generatedType = mixedSoftPathTile(fineNoise, broadNoise);
    } else if (shapedDistance <= caveWidth + 10.0f) {
        generatedType = mixedRockBandTile(fineNoise, broadNoise);
    } else {
        generatedType = mixedDeepWallTile(fineNoise, broadNoise);
    }

    generatedType = interleavedWallTile(
        generatedType,
        detailNoise,
        scatterNoise,
        veinNoise,
        shapedDistance <= caveWidth + SoftPathMargin + 2.0f);
    generatedType = applyTerrainProfile(
        generatedType,
        dungeonLayoutSnapshot_.terrainProfile,
        detailNoise,
        scatterNoise,
        veinNoise,
        broadNoise,
        shapedDistance <= caveWidth + SoftPathMargin + 2.0f);

    const auto overrideIt = tileOverrides_.find(key(tx, ty));
    if (overrideIt != tileOverrides_.end()) {
        generatedType = overrideIt->second;
    }
    const auto editIt = terrainEdits_.find(key(tx, ty));
    if (editIt != terrainEdits_.end()) {
        generatedType = editIt->second;
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
    info.attribute = terrainAttributeForTileType(info.type);
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

Color TileMap::tileColorAtTile(int tx, int ty) const
{
    if (const Tile* tile = tileAtWorldIfGenerated(tx, ty)) {
        return tileColor(*tile);
    }

    Tile generated{};
    generated.type = terrainInfoForTile(tx, ty, nullptr).type;
    return tileColor(generated);
}

Color TileMap::tileColorAtWorld(Vec2 world) const
{
    return tileColorAtTile(worldToTile(world.x), worldToTile(world.y));
}

Color TileMap::tileColor(const Tile& tile) const
{
    Color base{9, 9, 13, 255};
    switch (tile.type) {
    case TileType::Empty: base = {54, 55, 60, 255}; break;
    case TileType::Dirt: base = {105, 68, 37, 255}; break;
    case TileType::Rock: base = {64, 66, 72, 255}; break;
    case TileType::Ore: base = {70, 76, 130, 255}; break;
    case TileType::HardRock: base = {38, 40, 48, 255}; break;
    }
    return base;
}

void TileMap::drawTileLitByCircles(Renderer& renderer, Vec2 pos, Color color, Vec2, const std::vector<LightSource>&) const
{
    const float tileSize = static_cast<float>(balance::TileSize);
    renderer.fillRect(pos, {tileSize, tileSize}, color);
}

void TileMap::render(Renderer& renderer, const Camera& camera, Vec2 lightCenter, const std::vector<LightSource>& extraLights)
{
    constexpr int ViewTileMargin = 1;
    const Vec2 viewTopLeft = camera.screenToWorld({0.0f, 0.0f});
    const Vec2 viewBottomRight = camera.screenToWorld({
        static_cast<float>(camera.width()),
        static_cast<float>(camera.height()),
    });
    const int minTileX = worldToTile(std::min(viewTopLeft.x, viewBottomRight.x)) - ViewTileMargin;
    const int maxTileX = worldToTile(std::max(viewTopLeft.x, viewBottomRight.x)) + ViewTileMargin;
    const int minTileY = worldToTile(std::min(viewTopLeft.y, viewBottomRight.y)) - ViewTileMargin;
    const int maxTileY = worldToTile(std::max(viewTopLeft.y, viewBottomRight.y)) + ViewTileMargin;

    const TerrainTileSheet terrainSheet = acquireTerrainTileSheet(renderer, dungeonLayoutSnapshot_.stageId);
    const auto tileTypeForRender = [this](int tx, int ty) {
        if (const Tile* tile = tileAtWorldIfGenerated(tx, ty)) {
            return tile->type;
        }
        return terrainInfoForTile(tx, ty, nullptr).type;
    };
    const auto emptyForRender = [&](int tx, int ty) {
        return tileTypeForRender(tx, ty) == TileType::Empty;
    };
    const auto neighborsForRender = [&](int tx, int ty) {
        return TerrainNeighbors{
            .up = emptyForRender(tx, ty - 1),
            .down = emptyForRender(tx, ty + 1),
            .left = emptyForRender(tx - 1, ty),
            .right = emptyForRender(tx + 1, ty),
            .upLeft = emptyForRender(tx - 1, ty - 1),
            .upRight = emptyForRender(tx + 1, ty - 1),
            .downLeft = emptyForRender(tx - 1, ty + 1),
            .downRight = emptyForRender(tx + 1, ty + 1),
        };
    };

    for (int cy = centerChunkY_ - balance::ActiveChunkRadius; cy <= centerChunkY_ + balance::ActiveChunkRadius; ++cy) {
        const int chunkMinTileY = cy * balance::ChunkTiles;
        const int chunkMaxTileY = chunkMinTileY + balance::ChunkTiles - 1;
        if (chunkMaxTileY < minTileY || chunkMinTileY > maxTileY) {
            continue;
        }

        for (int cx = centerChunkX_ - balance::ActiveChunkRadius; cx <= centerChunkX_ + balance::ActiveChunkRadius; ++cx) {
            const int chunkMinTileX = cx * balance::ChunkTiles;
            const int chunkMaxTileX = chunkMinTileX + balance::ChunkTiles - 1;
            if (chunkMaxTileX < minTileX || chunkMinTileX > maxTileX) {
                continue;
            }

            Chunk& chunk = getOrCreateChunk(cx, cy, balanceSnapshot_);
            const int startY = std::max(0, minTileY - chunkMinTileY);
            const int endY = std::min(balance::ChunkTiles - 1, maxTileY - chunkMinTileY);
            const int startX = std::max(0, minTileX - chunkMinTileX);
            const int endX = std::min(balance::ChunkTiles - 1, maxTileX - chunkMinTileX);

            for (int y = startY; y <= endY; ++y) {
                for (int x = startX; x <= endX; ++x) {
                    const int tx = cx * balance::ChunkTiles + x;
                    const int ty = cy * balance::ChunkTiles + y;
                    const Vec2 pos{static_cast<float>(tx * balance::TileSize), static_cast<float>(ty * balance::TileSize)};
                    if (!isTileRectLit(pos, lightCenter, extraLights)) {
                        continue;
                    }
                    const Tile& tile = chunk.at(x, y);
                    const TerrainNeighbors neighbors = terrainSheet.available && tile.type != TileType::Empty
                        ? neighborsForRender(tx, ty)
                        : TerrainNeighbors{};
                    if (!drawTerrainTileImage(renderer, terrainSheet, pos, tx, ty, tile, neighbors)) {
                        drawTileLitByCircles(renderer, pos, tileColor(tile), lightCenter, extraLights);
                    }
                    drawTileCracks(renderer, pos, tx, ty, tile);
                }
            }
        }
    }
}

void TileMap::renderDarknessOverlay(Renderer& renderer, const Camera& camera, Vec2 lightCenter, const std::vector<LightSource>& extraLights) const
{
    const Vec2 viewTopLeft = camera.screenToWorld({0.0f, 0.0f});
    const float viewWidth = static_cast<float>(camera.width());
    const float viewHeight = static_cast<float>(camera.height());
    const float viewLeft = viewTopLeft.x;
    const float viewRight = viewTopLeft.x + viewWidth;
    const float viewTop = viewTopLeft.y;

    std::vector<LightSource> lights;
    lights.reserve(extraLights.size() + 1);
    lights.push_back({lightCenter, balanceSnapshot_.playerLightRadius});
    lights.insert(lights.end(), extraLights.begin(), extraLights.end());

    constexpr Color DarknessColor{5, 5, 8, 255};
    std::vector<Vec2> outerSpans;
    std::vector<Vec2> innerSpans;
    std::vector<Vec2> falloffSpans;
    outerSpans.reserve(lights.size());
    innerSpans.reserve(lights.size());
    falloffSpans.reserve(lights.size() * 2);

    const auto darknessAlphaAt = [&](float x, float y) {
        float alpha = static_cast<float>(DarknessColor.a);
        for (const LightSource& light : lights) {
            const float radius = light.radius > 0.0f ? light.radius : balanceSnapshot_.lightRadius;
            if (radius <= 0.0f) {
                continue;
            }
            const float dx = x - light.position.x;
            const float dy = y - light.position.y;
            const float distanceSq = dx * dx + dy * dy;
            const float radiusSq = radius * radius;
            if (distanceSq >= radiusSq) {
                continue;
            }

            const float innerRadius = lightFalloffInnerRadius(radius);
            const float innerRadiusSq = innerRadius * innerRadius;
            if (distanceSq <= innerRadiusSq) {
                return 0.0f;
            }

            const float distance = std::sqrt(distanceSq);
            const float t = std::clamp((distance - innerRadius) / std::max(1.0f, radius - innerRadius), 0.0f, 1.0f);
            const float smooth = t * t * (3.0f - 2.0f * t);
            alpha = std::min(alpha, static_cast<float>(DarknessColor.a) * smooth);
        }
        return alpha;
    };

    const auto drawFalloffSpan = [&](float x0, float x1, float y) {
        if (x1 <= x0) {
            return;
        }
        float x = x0;
        while (x < x1) {
            const float nextX = std::min(x1, x + LightFalloffSegmentWidth);
            const float midX = (x + nextX) * 0.5f;
            const float alpha0 = darknessAlphaAt(x, y);
            const float alphaMid = darknessAlphaAt(midX, y);
            const float alpha1 = darknessAlphaAt(nextX, y);
            if (std::max({alpha0, alphaMid, alpha1}) > 0.5f) {
                const auto makeColor = [](float alpha) {
                    return Color{5, 5, 8, static_cast<unsigned char>(std::clamp(std::lround(alpha), 0L, 255L))};
                };
                renderer.fillGradientRect(
                    {x, y - 0.5f},
                    {std::max(0.0f, midX - x), 1.0f},
                    makeColor(alpha0),
                    makeColor(alphaMid),
                    GradientDirection::LeftToRight);
                renderer.fillGradientRect(
                    {midX, y - 0.5f},
                    {std::max(0.0f, nextX - midX), 1.0f},
                    makeColor(alphaMid),
                    makeColor(alpha1),
                    GradientDirection::LeftToRight);
            }
            x = nextX;
        }
    };

    for (int row = 0; row < static_cast<int>(viewHeight); ++row) {
        const float y = viewTop + static_cast<float>(row) + 0.5f;
        outerSpans.clear();
        innerSpans.clear();
        for (const LightSource& light : lights) {
            const float radius = light.radius > 0.0f ? light.radius : balanceSnapshot_.lightRadius;
            const float radiusSq = radius * radius;
            const float dy = y - light.position.y;
            const float remaining = radiusSq - dy * dy;
            if (remaining > 0.0f) {
                const float dx = std::sqrt(remaining);
                const float x0 = std::max(viewLeft, light.position.x - dx);
                const float x1 = std::min(viewRight, light.position.x + dx);
                if (x1 > x0) {
                    outerSpans.push_back({x0, x1});
                }
            }

            const float innerRadius = lightFalloffInnerRadius(radius);
            const float innerRemaining = innerRadius * innerRadius - dy * dy;
            if (innerRemaining > 0.0f) {
                const float dx = std::sqrt(innerRemaining);
                const float x0 = std::max(viewLeft, light.position.x - dx);
                const float x1 = std::min(viewRight, light.position.x + dx);
                if (x1 > x0) {
                    innerSpans.push_back({x0, x1});
                }
            }
        }

        mergeLightSpans(outerSpans);
        if (outerSpans.empty()) {
            renderer.fillRect({viewLeft, viewTop + static_cast<float>(row)}, {viewWidth, 1.0f}, DarknessColor);
            continue;
        }

        float darkStart = viewLeft;
        for (Vec2 span : outerSpans) {
            const float litStart = span.x;
            const float litEnd = span.y;
            if (litStart > darkStart) {
                renderer.fillRect({darkStart, viewTop + static_cast<float>(row)}, {litStart - darkStart, 1.0f}, DarknessColor);
            }
            darkStart = litEnd;
        }
        if (darkStart < viewRight) {
            renderer.fillRect({darkStart, viewTop + static_cast<float>(row)}, {viewRight - darkStart, 1.0f}, DarknessColor);
        }

        mergeLightSpans(innerSpans);
        appendFalloffSpans(outerSpans, innerSpans, falloffSpans);
        for (Vec2 span : falloffSpans) {
            drawFalloffSpan(span.x, span.y, y);
        }
    }
}

}
