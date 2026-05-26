#include "devtools/autosim/AutoSimulationExplorationModel.hpp"

#include "data/GameBalance.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace majo::autosim {

namespace {

constexpr float TileSize = static_cast<float>(balance::TileSize);
constexpr float UnreachableScore = std::numeric_limits<float>::max();
constexpr float MaxClueAttractionDistance = TileSize * 18.0f;
constexpr int NoveltyRadiusTiles = 2;

struct ExplorationCandidate {
    Vec2 target{};
    std::string reason;
    GameTestTerrainKind terrainKind = GameTestTerrainKind::Empty;
    GameTestTerrainAttribute terrainAttribute = GameTestTerrainAttribute::None;
    float score = UnreachableScore;
};

bool reachedWithoutDigging(const AutoSimulationPathCell& cell)
{
    return std::isfinite(cell.cost) && cell.digCount == 0 && !cell.tile.solid;
}

const AutoSimulationPathCell* cellAt(const AutoSimulationPathField& field, int tileX, int tileY)
{
    const int index = field.indexForTile(tileX, tileY);
    if (index < 0) {
        return nullptr;
    }
    return &field.cells[static_cast<std::size_t>(index)];
}

float terrainBaseScore(GameTestTerrainKind kind)
{
    switch (kind) {
    case GameTestTerrainKind::Dirt: return -190.0f;
    case GameTestTerrainKind::Ore: return -28.0f;
    case GameTestTerrainKind::Rock: return 95.0f;
    case GameTestTerrainKind::HardRock: return 470.0f;
    case GameTestTerrainKind::Empty: return 0.0f;
    }
    return 0.0f;
}

float terrainAttributeScore(GameTestTerrainAttribute attribute)
{
    switch (attribute) {
    case GameTestTerrainAttribute::Soft: return -72.0f;
    case GameTestTerrainAttribute::Ore: return -18.0f;
    case GameTestTerrainAttribute::Hard: return 82.0f;
    case GameTestTerrainAttribute::None: return 0.0f;
    }
    return 0.0f;
}

float expectedBreakHits(const GameTestSnapshot& snapshot, const GameTestPathTileSnapshot& tile)
{
    const int hp = std::max(1, tile.hp > 0 ? tile.hp : tile.effectiveHp);
    const int digPower = std::max(1, snapshot.ring.bestDigPower);
    return std::ceil(static_cast<float>(hp) / static_cast<float>(digPower));
}

float clueKindValue(GameTestMapClueKind kind)
{
    switch (kind) {
    case GameTestMapClueKind::WarpGlow:
        return 220.0f;
    case GameTestMapClueKind::UnknownLight:
        break;
    }
    return 140.0f;
}

float mapClueBenefit(const GameTestSnapshot& snapshot, Vec2 target)
{
    float best = 0.0f;
    for (const GameTestMapClueSnapshot& clue : snapshot.dungeon.mapClues) {
        if (!clue.visibleOnMinimap || clue.alreadyVisited) {
            continue;
        }
        const float distanceToClue = length(clue.position - target);
        if (distanceToClue > MaxClueAttractionDistance) {
            continue;
        }

        const float confidence = std::clamp(clue.confidence, 0.15f, 1.0f);
        const float proximity = 1.0f - distanceToClue / MaxClueAttractionDistance;
        best = std::max(best, clueKindValue(clue.kind) * confidence * proximity);
    }
    return best;
}

float terrainPotentialValue(const GameTestPathTileSnapshot& tile)
{
    float value = 0.0f;
    switch (tile.terrainKind) {
    case GameTestTerrainKind::Dirt:
        value = 34.0f;
        break;
    case GameTestTerrainKind::Ore:
        value = 48.0f;
        break;
    case GameTestTerrainKind::Rock:
        value = 18.0f;
        break;
    case GameTestTerrainKind::HardRock:
        value = -8.0f;
        break;
    case GameTestTerrainKind::Empty:
        value = 24.0f;
        break;
    }

    switch (tile.terrainAttribute) {
    case GameTestTerrainAttribute::Soft:
        value += 16.0f;
        break;
    case GameTestTerrainAttribute::Ore:
        value += 12.0f;
        break;
    case GameTestTerrainAttribute::Hard:
        value -= 12.0f;
        break;
    case GameTestTerrainAttribute::None:
        break;
    }
    return value;
}

float frontierNoveltyBenefit(const AutoSimulationPathField& field, const GameTestPathTileSnapshot& target)
{
    float benefit = 0.0f;
    int unreachedCount = 0;
    int edgeCount = 0;
    for (int dy = -NoveltyRadiusTiles; dy <= NoveltyRadiusTiles; ++dy) {
        for (int dx = -NoveltyRadiusTiles; dx <= NoveltyRadiusTiles; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            const int manhattan = std::abs(dx) + std::abs(dy);
            if (manhattan > NoveltyRadiusTiles + 1) {
                continue;
            }

            const AutoSimulationPathCell* cell = cellAt(field, target.tileX + dx, target.tileY + dy);
            const float weight = 1.0f / static_cast<float>(std::max(1, manhattan));
            if (cell == nullptr) {
                benefit += 8.0f * weight;
                ++edgeCount;
                continue;
            }
            if (!std::isfinite(cell->cost)) {
                ++unreachedCount;
                benefit += terrainPotentialValue(cell->tile) * weight;
                continue;
            }
            if (cell->tile.solid && cell->digCount > 0) {
                benefit += terrainPotentialValue(cell->tile) * 0.35f * weight;
            }
        }
    }

    benefit += static_cast<float>(unreachedCount) * 7.0f;
    benefit += static_cast<float>(edgeCount) * 3.0f;
    return std::max(0.0f, benefit);
}

std::string terrainReason(const GameTestPathTileSnapshot& tile)
{
    if (tile.terrainKind == GameTestTerrainKind::Dirt &&
        tile.terrainAttribute == GameTestTerrainAttribute::Soft) {
        return "explore_soft_dirt";
    }
    switch (tile.terrainKind) {
    case GameTestTerrainKind::Dirt:
        return "explore_dirt";
    case GameTestTerrainKind::Ore:
        return "explore_ore";
    case GameTestTerrainKind::Rock:
        return "explore_rock";
    case GameTestTerrainKind::HardRock:
        return "explore_hard_rock";
    case GameTestTerrainKind::Empty:
        break;
    }
    return "explore_frontier";
}

float neighborhoodScore(const AutoSimulationPathField& field, const GameTestPathTileSnapshot& wall)
{
    float score = 0.0f;
    constexpr int Neighbors[4][2] = {
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
    };

    for (const auto& offset : Neighbors) {
        const AutoSimulationPathCell* neighbor = cellAt(field, wall.tileX + offset[0], wall.tileY + offset[1]);
        if (neighbor == nullptr) {
            continue;
        }
        const GameTestPathTileSnapshot& tile = neighbor->tile;
        if (!tile.solid) {
            score -= reachedWithoutDigging(*neighbor) ? 8.0f : 2.0f;
            continue;
        }

        if (tile.terrainKind == GameTestTerrainKind::Dirt) {
            score -= tile.terrainAttribute == GameTestTerrainAttribute::Soft ? 22.0f : 14.0f;
        } else if (tile.terrainKind == GameTestTerrainKind::Ore) {
            score -= 8.0f;
        } else if (tile.terrainKind == GameTestTerrainKind::HardRock) {
            score += 46.0f;
        } else if (tile.terrainKind == GameTestTerrainKind::Rock) {
            score += 16.0f;
        }
    }
    return score;
}

float outwardProgressScore(const GameTestSnapshot& snapshot, Vec2 target)
{
    const float distanceFromStart = length(target - snapshot.dungeon.startWorld);
    const float playerDistanceFromStart = length(snapshot.player.position - snapshot.dungeon.startWorld);
    return (playerDistanceFromStart - distanceFromStart) * 0.025f;
}

std::optional<ExplorationCandidate> wallCandidateForNeighbor(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathField& field,
    const AutoSimulationPathCell& access,
    const AutoSimulationPathCell& wall)
{
    if (!wall.tile.solid || !wall.tile.diggable) {
        return std::nullopt;
    }

    const int dx = wall.tile.tileX - access.tile.tileX;
    const int dy = wall.tile.tileY - access.tile.tileY;
    const AutoSimulationPathCell* behind = cellAt(field, wall.tile.tileX + dx, wall.tile.tileY + dy);
    const float hits = expectedBreakHits(snapshot, wall.tile);
    const float expectedValue =
        mapClueBenefit(snapshot, wall.tile.center) +
        frontierNoveltyBenefit(field, wall.tile);

    float score =
        access.cost +
        terrainBaseScore(wall.tile.terrainKind) +
        terrainAttributeScore(wall.tile.terrainAttribute) +
        hits * 18.0f +
        std::max(0.0f, wall.tile.localHardnessMultiplier - 1.0f) * 58.0f +
        std::clamp(wall.tile.distanceFromMainPath, 0.0f, 12.0f) * 9.0f +
        length(wall.tile.center - snapshot.player.position) * 0.025f +
        outwardProgressScore(snapshot, wall.tile.center) +
        neighborhoodScore(field, wall.tile) -
        expectedValue / std::max(1.0f, hits) * 0.85f;

    if (behind == nullptr) {
        score -= 20.0f;
    } else if (!behind->tile.solid) {
        score -= 42.0f;
    } else if (behind->tile.terrainKind == GameTestTerrainKind::Dirt) {
        score -= behind->tile.terrainAttribute == GameTestTerrainAttribute::Soft ? 34.0f : 20.0f;
    } else if (behind->tile.terrainKind == GameTestTerrainKind::HardRock) {
        score += 72.0f;
    }

    return ExplorationCandidate{
        .target = wall.tile.center,
        .reason = terrainReason(wall.tile),
        .terrainKind = wall.tile.terrainKind,
        .terrainAttribute = wall.tile.terrainAttribute,
        .score = score,
    };
}

std::optional<ExplorationCandidate> openFrontierCandidate(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathField& field,
    const AutoSimulationPathCell& cell)
{
    if (!reachedWithoutDigging(cell)) {
        return std::nullopt;
    }

    const bool edge =
        cell.tile.tileX == field.minTileX ||
        cell.tile.tileY == field.minTileY ||
        cell.tile.tileX == field.minTileX + field.width - 1 ||
        cell.tile.tileY == field.minTileY + field.height - 1;
    const float distanceFromPlayer = length(cell.tile.center - snapshot.player.position);
    if (!edge && distanceFromPlayer < TileSize * 5.0f) {
        return std::nullopt;
    }

    const float score =
        cell.cost +
        std::clamp(cell.tile.distanceFromMainPath, 0.0f, 12.0f) * 7.0f -
        (edge ? 80.0f : 0.0f) -
        std::min(distanceFromPlayer, TileSize * 16.0f) * 0.05f +
        outwardProgressScore(snapshot, cell.tile.center) -
        mapClueBenefit(snapshot, cell.tile.center) * 0.55f -
        frontierNoveltyBenefit(field, cell.tile) * 0.35f;

    return ExplorationCandidate{
        .target = cell.tile.center,
        .reason = edge ? "explore_open_edge" : "explore_open_frontier",
        .terrainKind = cell.tile.terrainKind,
        .terrainAttribute = cell.tile.terrainAttribute,
        .score = score,
    };
}

void keepBetter(std::optional<ExplorationCandidate>& best, std::optional<ExplorationCandidate> candidate)
{
    if (!candidate) {
        return;
    }
    if (!best || candidate->score < best->score) {
        best = std::move(candidate);
    }
}

bool softWallCandidate(const ExplorationCandidate& candidate)
{
    return candidate.terrainKind == GameTestTerrainKind::Dirt ||
        candidate.terrainAttribute == GameTestTerrainAttribute::Soft;
}

} // namespace

std::optional<AutoSimulationExplorationTarget> AutoSimulationExplorationModel::chooseTarget(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathField& pathField) const
{
    if (!pathField.valid()) {
        return std::nullopt;
    }

    std::optional<ExplorationCandidate> bestWall;
    std::optional<ExplorationCandidate> bestSoftWall;
    std::optional<ExplorationCandidate> bestOpen;
    constexpr int Neighbors[4][2] = {
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
    };

    for (const AutoSimulationPathCell& cell : pathField.cells) {
        if (!reachedWithoutDigging(cell)) {
            continue;
        }

        keepBetter(bestOpen, openFrontierCandidate(snapshot, pathField, cell));
        for (const auto& offset : Neighbors) {
            const AutoSimulationPathCell* neighbor = cellAt(
                pathField,
                cell.tile.tileX + offset[0],
                cell.tile.tileY + offset[1]);
            if (neighbor == nullptr) {
                continue;
            }
            std::optional<ExplorationCandidate> candidate =
                wallCandidateForNeighbor(snapshot, pathField, cell, *neighbor);
            if (candidate && softWallCandidate(*candidate)) {
                keepBetter(bestSoftWall, candidate);
            }
            keepBetter(bestWall, std::move(candidate));
        }
    }

    std::optional<ExplorationCandidate> best = bestWall;
    if (bestSoftWall && (!best || bestSoftWall->score <= best->score + 260.0f)) {
        best = bestSoftWall;
    }
    if (bestOpen && (!best || bestOpen->score < best->score - 30.0f)) {
        best = bestOpen;
    }
    if (!best) {
        return std::nullopt;
    }

    return AutoSimulationExplorationTarget{
        .world = best->target,
        .reason = best->reason,
    };
}

} // namespace majo::autosim
