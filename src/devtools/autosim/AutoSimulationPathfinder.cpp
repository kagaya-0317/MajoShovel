#include "devtools/autosim/AutoSimulationPathfinder.hpp"

#include "data/GameBalance.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace majo::autosim {

namespace {

constexpr float TileSize = static_cast<float>(balance::TileSize);
constexpr float TileHalfSize = TileSize * 0.5f;
constexpr float TargetApproachRadius = TileSize * 1.35f;
constexpr float OutsideTargetDistanceWeight = 0.045f;
constexpr float InsideTargetDistanceWeight = 0.020f;
constexpr float ImmediateDigDistance = TileSize * 3.65f;
constexpr int WaypointLookAheadTiles = 4;
constexpr float UnreachableCost = std::numeric_limits<float>::infinity();
constexpr int InvalidTile = std::numeric_limits<int>::min();

int worldToTile(float value)
{
    return static_cast<int>(std::floor(value / TileSize));
}

bool reached(const AutoSimulationPathCell& cell)
{
    return std::isfinite(cell.cost);
}

bool hardTerrain(GameTestTerrainKind kind)
{
    return kind == GameTestTerrainKind::Rock || kind == GameTestTerrainKind::HardRock;
}

bool sameTile(const GameTestPathTileSnapshot& left, const GameTestPathTileSnapshot& right)
{
    return left.tileX == right.tileX && left.tileY == right.tileY;
}

bool circleIntersectsRect(Vec2 center, float radius, const GameTestCollisionRectSnapshot& rect)
{
    const float closestX = std::clamp(center.x, rect.pos.x, rect.pos.x + rect.size.x);
    const float closestY = std::clamp(center.y, rect.pos.y, rect.pos.y + rect.size.y);
    return distanceSquared(center, {closestX, closestY}) <= radius * radius;
}

bool objectBlocksPlayer(const AutoSimulationPathField& field, Vec2 center)
{
    const float radius = std::max(0.0f, field.playerRadius);
    for (const GameTestCollisionRectSnapshot& rect : field.objectBlockers) {
        if (circleIntersectsRect(center, radius, rect)) {
            return true;
        }
    }
    return false;
}

bool terrainSampleBlocked(
    const AutoSimulationPathField& field,
    Vec2 sample,
    bool allowBlockedDestination,
    int destinationTileX,
    int destinationTileY)
{
    const int tileX = worldToTile(sample.x);
    const int tileY = worldToTile(sample.y);
    if (allowBlockedDestination && tileX == destinationTileX && tileY == destinationTileY) {
        return false;
    }

    const int index = field.indexForTile(tileX, tileY);
    if (index < 0) {
        return true;
    }
    return field.cells[static_cast<std::size_t>(index)].tile.terrainKind != GameTestTerrainKind::Empty;
}

bool terrainBlocksPlayer(
    const AutoSimulationPathField& field,
    Vec2 center,
    bool allowBlockedDestination,
    int destinationTileX,
    int destinationTileY)
{
    const float sample = std::max(0.0f, field.playerRadius) * 0.55f;
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
        if (terrainSampleBlocked(field, point, allowBlockedDestination, destinationTileX, destinationTileY)) {
            return true;
        }
    }
    return false;
}

bool positionBlocked(
    const AutoSimulationPathField& field,
    Vec2 center,
    bool allowBlockedDestination = false,
    int destinationTileX = InvalidTile,
    int destinationTileY = InvalidTile)
{
    if (terrainBlocksPlayer(field, center, allowBlockedDestination, destinationTileX, destinationTileY)) {
        return true;
    }
    if (allowBlockedDestination &&
        worldToTile(center.x) == destinationTileX &&
        worldToTile(center.y) == destinationTileY) {
        return false;
    }
    return objectBlocksPlayer(field, center);
}

bool lineWalkable(const AutoSimulationPathField& field, Vec2 from, Vec2 to, bool allowBlockedDestination = false)
{
    const Vec2 delta = to - from;
    const float distance = length(delta);
    if (distance <= 0.0001f) {
        return true;
    }

    const int steps = std::max(1, static_cast<int>(std::ceil(distance / (TileSize * 0.25f))));
    const int destinationTileX = worldToTile(to.x);
    const int destinationTileY = worldToTile(to.y);
    for (int step = 1; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        const Vec2 sample = from + delta * t;
        if (positionBlocked(field, sample, allowBlockedDestination, destinationTileX, destinationTileY)) {
            return false;
        }
    }
    return true;
}

float expectedBreakHits(const GameTestSnapshot& snapshot, const GameTestPathTileSnapshot& tile)
{
    if (!tile.solid) {
        return 0.0f;
    }
    const int remainingHp = std::max(1, tile.hp > 0 ? tile.hp : tile.effectiveHp);
    const int digPower = std::max(1, snapshot.ring.bestDigPower);
    return std::ceil(static_cast<float>(remainingHp) / static_cast<float>(digPower));
}

float terrainDigCost(const GameTestSnapshot& snapshot, const GameTestPathTileSnapshot& tile)
{
    if (!tile.solid) {
        return 0.0f;
    }
    if (!tile.diggable) {
        return UnreachableCost;
    }

    const float hits = expectedBreakHits(snapshot, tile);
    float cost = 0.0f;
    switch (tile.terrainKind) {
    case GameTestTerrainKind::Dirt:
        cost = 4.0f + hits * 3.4f;
        break;
    case GameTestTerrainKind::Ore:
        cost = 8.0f + hits * 5.2f;
        break;
    case GameTestTerrainKind::Rock:
        cost = 15.0f + hits * 8.5f;
        break;
    case GameTestTerrainKind::HardRock:
        cost = 42.0f + hits * 17.0f;
        break;
    case GameTestTerrainKind::Empty:
        break;
    }

    switch (tile.terrainAttribute) {
    case GameTestTerrainAttribute::Soft:
        cost -= 2.0f;
        break;
    case GameTestTerrainAttribute::Ore:
        cost -= 1.0f;
        break;
    case GameTestTerrainAttribute::Hard:
        cost += 5.0f;
        break;
    case GameTestTerrainAttribute::None:
        break;
    }

    cost += std::max(0.0f, tile.localHardnessMultiplier - 1.0f) * 12.0f;
    if (!snapshot.ring.hasDigTool) {
        cost += 80.0f;
    }
    return std::max(1.0f, cost);
}

float enemyDangerCost(const GameTestSnapshot& snapshot, Vec2 position)
{
    float cost = 0.0f;
    const bool canFight = snapshot.ring.hasCombatTool && snapshot.ring.bestDamage > 0;
    const float hpRatio = snapshot.player.maxHp > 0
        ? static_cast<float>(snapshot.player.hp) / static_cast<float>(snapshot.player.maxHp)
        : 1.0f;
    const float survivalScale = canFight && hpRatio >= 0.45f ? 0.45f : 1.0f;

    for (const GameTestEnemySnapshot& enemy : snapshot.enemies) {
        const float radius = enemy.boss ? 180.0f : 104.0f;
        const float distanceToEnemy = length(enemy.position - position);
        if (distanceToEnemy >= radius) {
            continue;
        }
        const float danger = (radius - distanceToEnemy) / radius;
        cost += danger * danger * (enemy.boss ? 80.0f : 30.0f) * survivalScale;
    }
    return cost;
}

float enterCost(const GameTestSnapshot& snapshot, const GameTestPathTileSnapshot& tile)
{
    const float digCost = terrainDigCost(snapshot, tile);
    if (!std::isfinite(digCost)) {
        return UnreachableCost;
    }

    const float mainPathCost = std::clamp(tile.distanceFromMainPath, 0.0f, 12.0f) * 0.08f;
    return 1.0f + digCost + mainPathCost + enemyDangerCost(snapshot, tile.center);
}

GameTestMineTileSnapshot makeMineTile(const AutoSimulationPathField& field, const GameTestPathTileSnapshot& tile)
{
    Vec2 normal = normalize(field.playerPosition - tile.center);
    if (lengthSquared(normal) <= 0.0001f) {
        normal = normalize(field.playerFacing);
    }

    return GameTestMineTileSnapshot{
        .center = tile.center,
        .surfacePoint = tile.center + normal * TileHalfSize,
        .outwardNormal = normal,
        .tileX = tile.tileX,
        .tileY = tile.tileY,
        .hp = tile.hp,
        .effectiveHp = tile.effectiveHp,
        .terrainKind = tile.terrainKind,
        .terrainAttribute = tile.terrainAttribute,
        .localHardnessMultiplier = tile.localHardnessMultiplier,
        .distanceFromMainPath = tile.distanceFromMainPath,
        .solid = tile.solid,
        .diggable = tile.diggable,
    };
}

int nearestCellToWorld(const AutoSimulationPathField& field, Vec2 world)
{
    int bestIndex = -1;
    float bestDistanceSq = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(field.cells.size()); ++i) {
        const float distSq = distanceSquared(field.cells[static_cast<std::size_t>(i)].tile.center, world);
        if (distSq < bestDistanceSq) {
            bestDistanceSq = distSq;
            bestIndex = i;
        }
    }
    return bestIndex;
}

std::optional<int> destinationIndexForTarget(const AutoSimulationPathField& field, Vec2 targetWorld)
{
    const int targetTileX = worldToTile(targetWorld.x);
    const int targetTileY = worldToTile(targetWorld.y);
    const int targetIndex = field.indexForTile(targetTileX, targetTileY);
    const bool targetInside = targetIndex >= 0;
    if (targetInside && reached(field.cells[static_cast<std::size_t>(targetIndex)])) {
        return targetIndex;
    }
    const bool targetIsUndiggableBlock =
        targetInside &&
        field.cells[static_cast<std::size_t>(targetIndex)].tile.solid &&
        !field.cells[static_cast<std::size_t>(targetIndex)].tile.diggable;

    int bestIndex = -1;
    float bestScore = std::numeric_limits<float>::max();
    for (int i = 0; i < static_cast<int>(field.cells.size()); ++i) {
        const AutoSimulationPathCell& cell = field.cells[static_cast<std::size_t>(i)];
        if (!reached(cell)) {
            continue;
        }

        const float targetDistance = length(cell.tile.center - targetWorld);
        if (targetInside && targetDistance > TargetApproachRadius) {
            continue;
        }

        const bool edge =
            cell.tile.tileX == field.minTileX ||
            cell.tile.tileY == field.minTileY ||
            cell.tile.tileX == field.minTileX + field.width - 1 ||
            cell.tile.tileY == field.minTileY + field.height - 1;
        if (!targetInside && !edge) {
            continue;
        }
        if (targetInside && !lineWalkable(field, cell.tile.center, targetWorld, targetIsUndiggableBlock)) {
            continue;
        }

        const float score = cell.cost + targetDistance * (targetInside ? InsideTargetDistanceWeight : OutsideTargetDistanceWeight);
        if (score < bestScore) {
            bestScore = score;
            bestIndex = i;
        }
    }

    if (bestIndex >= 0) {
        return bestIndex;
    }

    const int nearest = nearestCellToWorld(field, targetWorld);
    if (nearest >= 0 && reached(field.cells[static_cast<std::size_t>(nearest)])) {
        if (targetInside &&
            !lineWalkable(
                field,
                field.cells[static_cast<std::size_t>(nearest)].tile.center,
                targetWorld,
                targetIsUndiggableBlock)) {
            return std::nullopt;
        }
        return nearest;
    }
    return std::nullopt;
}

int safeWaypointPathIndex(const AutoSimulationPathField& field, const std::vector<int>& path, int firstDigPathIndex)
{
    if (path.size() <= 1) {
        return 0;
    }

    int maxPathIndex = std::min(static_cast<int>(path.size()) - 1, WaypointLookAheadTiles);
    if (firstDigPathIndex > 0) {
        maxPathIndex = std::min(maxPathIndex, std::max(1, firstDigPathIndex - 1));
    }

    int bestIndex = 0;
    for (int pathIndex = 1; pathIndex <= maxPathIndex; ++pathIndex) {
        const GameTestPathTileSnapshot& tile =
            field.cells[static_cast<std::size_t>(path[static_cast<std::size_t>(pathIndex)])].tile;
        if (tile.solid || !lineWalkable(field, field.playerPosition, tile.center)) {
            break;
        }
        bestIndex = pathIndex;
    }
    return bestIndex;
}

std::vector<int> reconstructPath(const AutoSimulationPathField& field, int destinationIndex)
{
    std::vector<int> reversed;
    int index = destinationIndex;
    while (index >= 0 && index < static_cast<int>(field.cells.size())) {
        reversed.push_back(index);
        if (index == field.startIndex) {
            break;
        }
        index = field.cells[static_cast<std::size_t>(index)].previous;
    }
    std::reverse(reversed.begin(), reversed.end());
    if (reversed.empty() || reversed.front() != field.startIndex) {
        reversed.clear();
    }
    return reversed;
}

std::optional<GameTestPathTileSnapshot> firstDirectSolidTile(
    const AutoSimulationPathField& field,
    Vec2 targetWorld)
{
    const Vec2 toTarget = targetWorld - field.playerPosition;
    const float targetDistance = length(toTarget);
    if (targetDistance <= TileSize * 0.5f) {
        return std::nullopt;
    }

    const Vec2 direction = normalize(toTarget);
    const int startTileX = worldToTile(field.playerPosition.x);
    const int startTileY = worldToTile(field.playerPosition.y);
    const float maxDistance = std::min(targetDistance, TileSize * 8.0f);
    const float step = TileSize * 0.35f;

    for (float distance = step; distance <= maxDistance; distance += step) {
        const Vec2 sample = field.playerPosition + direction * distance;
        const int index = field.indexForTile(worldToTile(sample.x), worldToTile(sample.y));
        if (index < 0) {
            continue;
        }
        const GameTestPathTileSnapshot& tile = field.cells[static_cast<std::size_t>(index)].tile;
        if (tile.tileX == startTileX && tile.tileY == startTileY) {
            continue;
        }
        if (tile.solid) {
            return tile;
        }
    }
    return std::nullopt;
}

} // namespace

bool AutoSimulationPathField::valid() const
{
    return width > 0 &&
        height > 0 &&
        startIndex >= 0 &&
        startIndex < static_cast<int>(cells.size()) &&
        cells.size() == static_cast<std::size_t>(width * height);
}

int AutoSimulationPathField::indexForTile(int tileX, int tileY) const
{
    const int localX = tileX - minTileX;
    const int localY = tileY - minTileY;
    if (localX < 0 || localY < 0 || localX >= width || localY >= height) {
        return -1;
    }
    return localY * width + localX;
}

AutoSimulationPathField AutoSimulationPathfinder::buildField(const GameTestSnapshot& snapshot) const
{
    AutoSimulationPathField field;
    field.minTileX = snapshot.pathGrid.minTileX;
    field.minTileY = snapshot.pathGrid.minTileY;
    field.width = snapshot.pathGrid.width;
    field.height = snapshot.pathGrid.height;
    field.playerPosition = snapshot.player.position;
    field.playerFacing = snapshot.player.facing;
    field.playerRadius = snapshot.player.radius;
    field.objectBlockers = snapshot.pathGrid.objectBlockers;

    const std::size_t expectedSize = static_cast<std::size_t>(std::max(0, field.width) * std::max(0, field.height));
    if (expectedSize == 0 || snapshot.pathGrid.tiles.size() != expectedSize) {
        return field;
    }

    field.cells.reserve(snapshot.pathGrid.tiles.size());
    for (const GameTestPathTileSnapshot& tile : snapshot.pathGrid.tiles) {
        field.cells.push_back(AutoSimulationPathCell{
            .tile = tile,
            .cost = UnreachableCost,
            .previous = -1,
            .digCount = 0,
        });
    }

    field.startIndex = field.indexForTile(
        worldToTile(snapshot.player.position.x),
        worldToTile(snapshot.player.position.y));
    if (field.startIndex < 0) {
        field.startIndex = nearestCellToWorld(field, snapshot.player.position);
    }
    if (!field.valid()) {
        return field;
    }

    using QueueEntry = std::pair<float, int>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> open;
    field.cells[static_cast<std::size_t>(field.startIndex)].cost = 0.0f;
    open.push({0.0f, field.startIndex});

    constexpr int Neighbors[4][2] = {
        {1, 0},
        {-1, 0},
        {0, 1},
        {0, -1},
    };

    while (!open.empty()) {
        const auto [currentCost, currentIndex] = open.top();
        open.pop();
        AutoSimulationPathCell& current = field.cells[static_cast<std::size_t>(currentIndex)];
        if (currentCost > current.cost) {
            continue;
        }

        for (const auto& offset : Neighbors) {
            const int neighborIndex = field.indexForTile(
                current.tile.tileX + offset[0],
                current.tile.tileY + offset[1]);
            if (neighborIndex < 0) {
                continue;
            }

            AutoSimulationPathCell& neighbor = field.cells[static_cast<std::size_t>(neighborIndex)];
            const float stepCost = enterCost(snapshot, neighbor.tile);
            if (!std::isfinite(stepCost)) {
                continue;
            }
            if (!current.tile.solid &&
                !neighbor.tile.solid &&
                !lineWalkable(field, current.tile.center, neighbor.tile.center)) {
                continue;
            }
            const float nextCost = current.cost + stepCost;
            if (nextCost >= neighbor.cost) {
                continue;
            }

            neighbor.cost = nextCost;
            neighbor.previous = currentIndex;
            neighbor.digCount = current.digCount + (neighbor.tile.solid ? 1 : 0);
            open.push({nextCost, neighborIndex});
        }
    }

    return field;
}

std::optional<AutoSimulationRoute> AutoSimulationPathfinder::findRoute(
    const AutoSimulationPathField& field,
    Vec2 targetWorld) const
{
    if (!field.valid()) {
        return std::nullopt;
    }

    const std::optional<int> destinationIndex = destinationIndexForTarget(field, targetWorld);
    if (!destinationIndex) {
        return std::nullopt;
    }

    std::vector<int> path = reconstructPath(field, *destinationIndex);
    if (path.empty()) {
        return std::nullopt;
    }

    AutoSimulationRoute route;
    route.found = true;
    route.totalCost = field.cells[static_cast<std::size_t>(*destinationIndex)].cost;
    route.debugWorldPoints.reserve(path.size());

    int firstDigPathIndex = -1;
    for (int i = 0; i < static_cast<int>(path.size()); ++i) {
        const GameTestPathTileSnapshot& tile = field.cells[static_cast<std::size_t>(path[static_cast<std::size_t>(i)])].tile;
        route.debugWorldPoints.push_back(tile.center);
        if (i > 0 && tile.solid) {
            ++route.digTileCount;
            if (hardTerrain(tile.terrainKind)) {
                ++route.hardTileCount;
            }
            if (firstDigPathIndex < 0) {
                firstDigPathIndex = i;
            }
        }
    }

    if (firstDigPathIndex > 0) {
        const GameTestPathTileSnapshot& firstDigTile =
            field.cells[static_cast<std::size_t>(path[static_cast<std::size_t>(firstDigPathIndex)])].tile;
        route.firstDigTerrainKind = firstDigTile.terrainKind;
        route.hasFirstDigTerrainKind = true;
    }

    if (std::optional<GameTestPathTileSnapshot> directBlock = firstDirectSolidTile(field, targetWorld);
        directBlock && hardTerrain(directBlock->terrainKind)) {
        const bool directBlockIsFirstDig =
            firstDigPathIndex > 0 &&
            sameTile(
                *directBlock,
                field.cells[static_cast<std::size_t>(path[static_cast<std::size_t>(firstDigPathIndex)])].tile);
        route.avoidingHardWall = !directBlockIsFirstDig;
    }

    if (firstDigPathIndex > 0) {
        const int firstDigIndex = path[static_cast<std::size_t>(firstDigPathIndex)];
        const GameTestPathTileSnapshot& firstDigTile = field.cells[static_cast<std::size_t>(firstDigIndex)].tile;
        if (length(firstDigTile.center - field.playerPosition) <= ImmediateDigDistance) {
            route.nextDigTile = makeMineTile(field, firstDigTile);
            const int afterDigPathIndex = std::min(
                static_cast<int>(path.size()) - 1,
                firstDigPathIndex + 1);
            route.nextWaypointWorld = field.cells[static_cast<std::size_t>(path[static_cast<std::size_t>(afterDigPathIndex)])].tile.center;
            return route;
        }
    }

    const int waypointPathIndex = safeWaypointPathIndex(field, path, firstDigPathIndex);
    route.nextWaypointWorld = path.size() == 1
        ? targetWorld
        : field.cells[static_cast<std::size_t>(path[static_cast<std::size_t>(waypointPathIndex)])].tile.center;
    return route;
}

} // namespace majo::autosim
