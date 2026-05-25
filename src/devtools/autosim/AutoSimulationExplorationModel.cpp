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

struct ExplorationCandidate {
    Vec2 target{};
    std::string reason;
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

    float score =
        access.cost +
        terrainBaseScore(wall.tile.terrainKind) +
        terrainAttributeScore(wall.tile.terrainAttribute) +
        expectedBreakHits(snapshot, wall.tile) * 18.0f +
        std::max(0.0f, wall.tile.localHardnessMultiplier - 1.0f) * 58.0f +
        std::clamp(wall.tile.distanceFromMainPath, 0.0f, 12.0f) * 9.0f +
        length(wall.tile.center - snapshot.player.position) * 0.025f +
        outwardProgressScore(snapshot, wall.tile.center) +
        neighborhoodScore(field, wall.tile);

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
        outwardProgressScore(snapshot, cell.tile.center);

    return ExplorationCandidate{
        .target = cell.tile.center,
        .reason = edge ? "explore_open_edge" : "explore_open_frontier",
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

} // namespace

std::optional<AutoSimulationExplorationTarget> AutoSimulationExplorationModel::chooseTarget(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathField& pathField) const
{
    if (!pathField.valid()) {
        return std::nullopt;
    }

    std::optional<ExplorationCandidate> bestWall;
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
            keepBetter(bestWall, wallCandidateForNeighbor(snapshot, pathField, cell, *neighbor));
        }
    }

    const std::optional<ExplorationCandidate>& best = bestWall ? bestWall : bestOpen;
    if (!best) {
        return std::nullopt;
    }

    return AutoSimulationExplorationTarget{
        .world = best->target,
        .reason = best->reason,
    };
}

} // namespace majo::autosim
