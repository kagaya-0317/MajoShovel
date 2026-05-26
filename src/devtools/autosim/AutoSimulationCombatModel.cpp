#include "devtools/autosim/AutoSimulationCombatModel.hpp"

#include "data/GameBalance.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace majo::autosim {

namespace {

constexpr float CombatAcquireRadius = 520.0f;
constexpr float EnemyBodyRadiusFallback = 14.0f;
constexpr float EmergencyDistance = 42.0f;
constexpr float TileSize = static_cast<float>(balance::TileSize);
constexpr float StanceArriveDistance = 28.0f;
constexpr float StanceRangeSlack = TileSize * 0.55f;
constexpr float RingRangeSlack = TileSize * 0.38f;
constexpr float WideOpenScore = 34.0f;
constexpr int OpenAreaRadiusTiles = 3;

struct CombatRange {
    float desiredMin = 0.0f;
    float desiredMax = 0.0f;
    float ideal = 0.0f;
    float ringMin = 0.0f;
    float ringMax = 0.0f;
    float ringIdeal = 0.0f;
    float throwRange = 0.0f;
};

struct OpenArea {
    float score = 0.0f;
    int exits = 0;
    bool horizontal = false;
    bool vertical = false;
};

struct CombatStance {
    const GameTestEnemySnapshot* enemy = nullptr;
    AutoSimulationRoute route;
    Vec2 world{};
    float score = std::numeric_limits<float>::max();
    float openScore = 0.0f;
    float distanceToEnemy = 0.0f;
};

int worldToTile(float value)
{
    return static_cast<int>(std::floor(value / TileSize));
}

float enemyRadiusFor(const GameTestEnemySnapshot& enemy)
{
    const float fallback = enemy.boss ? 28.0f : EnemyBodyRadiusFallback;
    return std::max(fallback, enemy.radius);
}

Vec2 expectedRingCenterForPlayer(const GameTestSnapshot& snapshot, Vec2 playerPosition)
{
    Vec2 offset = snapshot.ring.anchorOffsetFromPlayer;
    if (lengthSquared(offset) <= 0.0001f) {
        offset = snapshot.ringCenter - snapshot.player.position;
    }
    return playerPosition + offset;
}

float ringDistanceForPlayer(const GameTestSnapshot& snapshot, Vec2 playerPosition, const GameTestEnemySnapshot& enemy)
{
    return length(expectedRingCenterForPlayer(snapshot, playerPosition) - enemy.position);
}

CombatRange combatRangeFor(const GameTestSnapshot& snapshot, const GameTestEnemySnapshot& enemy)
{
    const float ringRadius = std::max(36.0f, snapshot.ring.activeRadius);
    const float hitRadius = std::max(8.0f, snapshot.ring.bestHitRadius);
    const float enemyRadius = enemyRadiusFor(enemy);
    const float ringMin = std::max(0.0f, ringRadius - hitRadius - enemyRadius);
    const float ringMax = std::max(ringMin + 6.0f, ringRadius + hitRadius + enemyRadius);
    const float ringCenterOffset = length(expectedRingCenterForPlayer(snapshot, snapshot.player.position) - snapshot.player.position);
    const float contactPadding = enemy.boss ? 36.0f : 24.0f;
    float personalMin = snapshot.player.radius + enemyRadius + contactPadding;
    if (enemy.contactAttackPower <= 0 || enemy.contactDamageMultiplier <= 0.0f) {
        personalMin -= 10.0f;
    }
    if (enemy.jumpLandingRadius > 0.0f) {
        personalMin = std::max(personalMin, enemy.jumpLandingRadius + snapshot.player.radius + 12.0f);
    }
    if (enemy.countdownExplodeRadius > 0.0f) {
        personalMin = std::max(personalMin, enemy.countdownExplodeRadius + snapshot.player.radius + 14.0f);
    }
    if (enemy.ranged) {
        personalMin += 6.0f;
    }

    const float desiredMin = std::max({
        EmergencyDistance,
        personalMin,
        ringMin + ringCenterOffset * 0.85f,
    });
    const float desiredMax = std::max(
        desiredMin + 28.0f,
        ringMax + ringCenterOffset + 18.0f);
    return CombatRange{
        .desiredMin = desiredMin,
        .desiredMax = desiredMax,
        .ideal = std::clamp(ringRadius + ringCenterOffset * 0.45f, desiredMin + 8.0f, desiredMax - 8.0f),
        .ringMin = ringMin,
        .ringMax = ringMax,
        .ringIdeal = std::clamp(ringRadius, ringMin + 3.0f, ringMax - 3.0f),
        .throwRange = desiredMax + ringRadius * 1.7f,
    };
}

bool reachedOpenWithoutDigging(const AutoSimulationPathCell& cell)
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

OpenArea openAreaAround(const AutoSimulationPathField& field, const GameTestPathTileSnapshot& tile)
{
    OpenArea area;
    for (int dy = -OpenAreaRadiusTiles; dy <= OpenAreaRadiusTiles; ++dy) {
        for (int dx = -OpenAreaRadiusTiles; dx <= OpenAreaRadiusTiles; ++dx) {
            const float distanceTiles = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (distanceTiles > static_cast<float>(OpenAreaRadiusTiles) + 0.1f) {
                continue;
            }

            const AutoSimulationPathCell* neighbor = cellAt(field, tile.tileX + dx, tile.tileY + dy);
            const bool open = neighbor != nullptr && reachedOpenWithoutDigging(*neighbor);
            const float weight = std::max(0.25f, static_cast<float>(OpenAreaRadiusTiles + 1) - distanceTiles);
            if (open) {
                area.score += weight;
            } else if (distanceTiles <= 1.45f) {
                area.score -= 5.0f;
            } else {
                area.score -= 0.4f;
            }

            if (std::abs(dx) + std::abs(dy) == 1 && open) {
                ++area.exits;
                area.horizontal = area.horizontal || dx != 0;
                area.vertical = area.vertical || dy != 0;
            }
        }
    }

    area.score += static_cast<float>(area.exits) * 4.0f;
    if (area.exits <= 1) {
        area.score -= 42.0f;
    } else if (area.exits == 2 && !(area.horizontal && area.vertical)) {
        area.score -= 20.0f;
    } else if (area.horizontal && area.vertical) {
        area.score += 14.0f;
    }
    return area;
}

float routeNarrowPenalty(const AutoSimulationPathField& field, const AutoSimulationRoute& route)
{
    float penalty = 0.0f;
    int sampled = 0;
    for (Vec2 point : route.debugWorldPoints) {
        const int index = field.indexForTile(worldToTile(point.x), worldToTile(point.y));
        if (index < 0) {
            continue;
        }

        const AutoSimulationPathCell& cell = field.cells[static_cast<std::size_t>(index)];
        const float openScore = openAreaAround(field, cell.tile).score;
        if (openScore < WideOpenScore * 0.55f) {
            penalty += (WideOpenScore * 0.55f - openScore) * 1.15f;
        }

        ++sampled;
        if (sampled >= 8) {
            break;
        }
    }
    return penalty;
}

float nearbyEnemyPenalty(const GameTestSnapshot& snapshot, const GameTestEnemySnapshot& target, Vec2 stance)
{
    float penalty = 0.0f;
    for (const GameTestEnemySnapshot& enemy : snapshot.enemies) {
        if (&enemy == &target) {
            continue;
        }
        const float radius = enemy.boss ? 220.0f : 126.0f;
        const float distanceToEnemy = length(enemy.position - stance);
        if (distanceToEnemy >= radius) {
            continue;
        }
        const float danger = (radius - distanceToEnemy) / radius;
        penalty += danger * danger * (enemy.boss ? 95.0f : 36.0f);
    }
    return penalty;
}

bool fieldEdgeCell(const AutoSimulationPathField& field, const GameTestPathTileSnapshot& tile)
{
    return tile.tileX == field.minTileX ||
        tile.tileY == field.minTileY ||
        tile.tileX == field.minTileX + field.width - 1 ||
        tile.tileY == field.minTileY + field.height - 1;
}

std::optional<CombatStance> bestStanceForEnemy(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathfinder& pathfinder,
    const AutoSimulationPathField& pathField,
    const GameTestEnemySnapshot& enemy)
{
    const CombatRange range = combatRangeFor(snapshot, enemy);
    const float minRange = std::max(0.0f, range.desiredMin - StanceRangeSlack);
    const float maxRange = range.desiredMax + StanceRangeSlack;
    const float minRingRange = std::max(0.0f, range.ringMin - RingRangeSlack);
    const float maxRingRange = range.ringMax + RingRangeSlack;

    std::optional<CombatStance> best;
    for (const AutoSimulationPathCell& cell : pathField.cells) {
        if (!reachedOpenWithoutDigging(cell)) {
            continue;
        }

        const float distanceToEnemy = length(cell.tile.center - enemy.position);
        if (distanceToEnemy < minRange || distanceToEnemy > maxRange) {
            continue;
        }
        const float ringDistanceToEnemy = ringDistanceForPlayer(snapshot, cell.tile.center, enemy);
        if (ringDistanceToEnemy < minRingRange || ringDistanceToEnemy > maxRingRange) {
            continue;
        }
        if (!pathfinder.hasClearLine(pathField, cell.tile.center, enemy.position, true)) {
            continue;
        }

        std::optional<AutoSimulationRoute> route = pathfinder.findRoute(pathField, cell.tile.center);
        if (!route || route->digTileCount > 0 || route->nextDigTile) {
            continue;
        }

        const OpenArea openArea = openAreaAround(pathField, cell.tile);
        const float rangeError =
            std::abs(ringDistanceToEnemy - range.ringIdeal) * 0.78f +
            std::abs(distanceToEnemy - range.ideal) * 0.28f;
        const float tooClosePenalty = std::max(0.0f, range.desiredMin - distanceToEnemy) * 3.2f;
        const float tooFarPenalty = std::max(0.0f, distanceToEnemy - range.desiredMax) * 1.4f;
        const float ringTooClosePenalty = std::max(0.0f, range.ringMin - ringDistanceToEnemy) * 4.5f;
        const float ringTooFarPenalty = std::max(0.0f, ringDistanceToEnemy - range.ringMax) * 2.0f;
        const float edgePenalty = fieldEdgeCell(pathField, cell.tile) ? 90.0f : 0.0f;
        const float score =
            route->totalCost * 1.05f +
            routeNarrowPenalty(pathField, *route) +
            rangeError * 0.42f +
            tooClosePenalty +
            tooFarPenalty +
            ringTooClosePenalty +
            ringTooFarPenalty +
            nearbyEnemyPenalty(snapshot, enemy, cell.tile.center) +
            edgePenalty -
            openArea.score * 3.1f +
            (enemy.boss ? -120.0f : 0.0f);

        if (!best || score < best->score) {
            best = CombatStance{
                .enemy = &enemy,
                .route = *route,
                .world = cell.tile.center,
                .score = score,
                .openScore = openArea.score,
                .distanceToEnemy = distanceToEnemy,
            };
        }
    }
    return best;
}

std::optional<CombatStance> bestCombatStance(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathfinder& pathfinder,
    const AutoSimulationPathField& pathField)
{
    std::optional<CombatStance> best;
    for (const GameTestEnemySnapshot& enemy : snapshot.enemies) {
        if (distanceSquared(snapshot.player.position, enemy.position) > CombatAcquireRadius * CombatAcquireRadius) {
            continue;
        }

        std::optional<CombatStance> stance = bestStanceForEnemy(snapshot, pathfinder, pathField, enemy);
        if (!stance) {
            continue;
        }

        stance->score += length(enemy.position - snapshot.player.position) * 0.16f;
        if (!best || stance->score < best->score) {
            best = std::move(stance);
        }
    }
    return best;
}

const GameTestEnemySnapshot* nearestEnemy(const GameTestSnapshot& snapshot)
{
    const GameTestEnemySnapshot* best = nullptr;
    float bestDistanceSq = CombatAcquireRadius * CombatAcquireRadius;
    for (const GameTestEnemySnapshot& enemy : snapshot.enemies) {
        const float distSq = distanceSquared(snapshot.player.position, enemy.position);
        if (distSq < bestDistanceSq) {
            bestDistanceSq = distSq;
            best = &enemy;
        }
    }
    return best;
}

void applyRouteMetadata(AutoSimulationPlan& plan, const AutoSimulationRoute& route)
{
    plan.routePathTileCount = route.pathTileCount;
    plan.routeWaypointPathIndex = route.waypointPathIndex;
    plan.routeFirstDigPathIndex = route.firstDigPathIndex;
    plan.routeDigTileCount = route.digTileCount;
    plan.routeHardTileCount = route.hardTileCount;
    plan.routeAvoidingHardWall = route.avoidingHardWall;
    if (route.hasFirstDigTerrainKind) {
        plan.targetTerrainKind = route.firstDigTerrainKind;
        plan.hasTargetTerrainKind = true;
    }
}

AutoSimulationPlan makeCombatPlan(
    const GameTestSnapshot& snapshot,
    const GameTestEnemySnapshot& enemy,
    const CombatRange& range,
    Vec2 moveTarget,
    bool strafe,
    std::string reason)
{
    const float distanceToEnemy = length(enemy.position - snapshot.player.position);
    const float ringDistanceToEnemy = length(snapshot.ringCenter - enemy.position);
    float desiredMin = range.desiredMin;
    float desiredMax = range.desiredMax;
    if (ringDistanceToEnemy < range.ringMin) {
        desiredMin = std::max(desiredMin, distanceToEnemy + 14.0f);
    } else if (ringDistanceToEnemy > range.ringMax && distanceToEnemy > desiredMin + 8.0f) {
        desiredMax = std::min(desiredMax, std::max(desiredMin + 8.0f, distanceToEnemy - 14.0f));
    }
    desiredMax = std::max(desiredMax, desiredMin + 8.0f);

    AutoSimulationPlan plan;
    plan.goal = AutoSimulationGoal::Combat;
    plan.targetWorld = enemy.position;
    plan.moveTargetWorld = moveTarget;
    plan.aimTargetWorld = enemy.position;
    plan.hasTarget = true;
    plan.hasMoveTarget = true;
    plan.hasAimTarget = true;
    plan.rangeControl = true;
    plan.alignMoveTargetInRange = true;
    plan.moveTargetArriveDistance = StanceArriveDistance;
    plan.desiredRangeMin = desiredMin;
    plan.desiredRangeMax = desiredMax;
    plan.strafe = strafe;
    plan.preferredRingRole = AutoSimulationRingRole::Combat;
    plan.throwRing = snapshot.ring.hasCombatTool && distanceToEnemy <= range.throwRange;
    plan.ringOffset = false;
    plan.moveAwayFromTarget = false;
    plan.reason = std::move(reason);
    return plan;
}

std::optional<AutoSimulationPlan> makeFallbackPlan(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathfinder& pathfinder,
    const AutoSimulationPathField& pathField)
{
    const int currentIndex = pathField.indexForTile(
        worldToTile(snapshot.player.position.x),
        worldToTile(snapshot.player.position.y));
    const float currentOpenScore = currentIndex >= 0
        ? openAreaAround(pathField, pathField.cells[static_cast<std::size_t>(currentIndex)].tile).score
        : 0.0f;

    const GameTestEnemySnapshot* bestEnemy = nullptr;
    std::optional<AutoSimulationRoute> bestRoute;
    CombatRange bestRange;
    Vec2 bestWorld{};
    bool bestClearLine = false;
    float bestScore = std::numeric_limits<float>::max();

    for (const GameTestEnemySnapshot& enemy : snapshot.enemies) {
        const float distanceToEnemy = length(enemy.position - snapshot.player.position);
        if (distanceToEnemy > CombatAcquireRadius) {
            continue;
        }

        const CombatRange range = combatRangeFor(snapshot, enemy);
        const float currentRingDistance = length(snapshot.ringCenter - enemy.position);
        const bool currentlyUnsafe =
            distanceToEnemy < range.desiredMin ||
            currentRingDistance < range.ringMin ||
            distanceToEnemy < snapshot.player.radius + enemyRadiusFor(enemy) + 16.0f;
        if (!currentlyUnsafe && distanceToEnemy > range.desiredMax + StanceRangeSlack) {
            continue;
        }

        const float maxPlayerRange = range.desiredMax + StanceRangeSlack * (currentlyUnsafe ? 2.2f : 1.2f);
        const float minPlayerRange = std::max(0.0f, range.desiredMin - StanceRangeSlack * 0.45f);
        const float minRingRange = std::max(0.0f, range.ringMin - RingRangeSlack);
        const float maxRingRange = range.ringMax + RingRangeSlack * 1.4f;
        for (const AutoSimulationPathCell& cell : pathField.cells) {
            if (!reachedOpenWithoutDigging(cell)) {
                continue;
            }

            const float playerDistance = length(cell.tile.center - enemy.position);
            if (playerDistance < minPlayerRange || playerDistance > maxPlayerRange) {
                continue;
            }

            const float ringDistance = ringDistanceForPlayer(snapshot, cell.tile.center, enemy);
            if (ringDistance < minRingRange || ringDistance > maxRingRange) {
                continue;
            }

            std::optional<AutoSimulationRoute> route = pathfinder.findRoute(pathField, cell.tile.center);
            if (!route || route->digTileCount > 0 || route->nextDigTile) {
                continue;
            }

            const OpenArea openArea = openAreaAround(pathField, cell.tile);
            const bool clearLine = pathfinder.hasClearLine(pathField, cell.tile.center, enemy.position, true);
            float score =
                route->totalCost * 1.08f +
                routeNarrowPenalty(pathField, *route) +
                std::abs(ringDistance - range.ringIdeal) * 0.62f +
                std::abs(playerDistance - range.ideal) * 0.24f +
                nearbyEnemyPenalty(snapshot, enemy, cell.tile.center) +
                (clearLine ? 0.0f : 82.0f) +
                (fieldEdgeCell(pathField, cell.tile) ? 70.0f : 0.0f) -
                openArea.score * 2.2f -
                currentOpenScore * 0.8f +
                (enemy.boss ? -80.0f : 0.0f);
            if (currentlyUnsafe && playerDistance > distanceToEnemy) {
                score -= std::min(46.0f, (playerDistance - distanceToEnemy) * 0.42f);
            }

            if (score < bestScore) {
                bestScore = score;
                bestEnemy = &enemy;
                bestRoute = std::move(route);
                bestRange = range;
                bestWorld = cell.tile.center;
                bestClearLine = clearLine;
            }
        }
    }

    if (bestEnemy == nullptr || !bestRoute) {
        const GameTestEnemySnapshot* enemy = nearestEnemy(snapshot);
        if (enemy == nullptr) {
            return std::nullopt;
        }
        const CombatRange range = combatRangeFor(snapshot, *enemy);
        const float distanceToEnemy = length(enemy->position - snapshot.player.position);
        if (distanceToEnemy >= range.desiredMin) {
            return std::nullopt;
        }
        Vec2 away = snapshot.player.position - enemy->position;
        if (lengthSquared(away) <= 0.0001f) {
            away = snapshot.player.facing;
        }
        AutoSimulationPlan plan = makeCombatPlan(
            snapshot,
            *enemy,
            range,
            snapshot.player.position + normalize(away) * (TileSize * 2.0f),
            false,
            enemy->boss ? "boss_emergency_backoff" : "enemy_emergency_backoff");
        plan.alignMoveTargetInRange = false;
        return plan;
    }

    AutoSimulationPlan plan = makeCombatPlan(
        snapshot,
        *bestEnemy,
        bestRange,
        bestRoute->nextWaypointWorld,
        bestClearLine &&
            distanceSquared(snapshot.player.position, bestWorld) <= StanceArriveDistance * StanceArriveDistance &&
            pathField.valid(),
        bestEnemy->boss ? "boss_reset_range_path" : "enemy_reset_range_path");
    applyRouteMetadata(plan, *bestRoute);
    return plan;
}

AutoSimulationPlan makeDirectPlan(const GameTestSnapshot& snapshot, const GameTestEnemySnapshot& enemy)
{
    const CombatRange range = combatRangeFor(snapshot, enemy);
    const float distance = length(enemy.position - snapshot.player.position);
    AutoSimulationPlan plan = makeCombatPlan(
        snapshot,
        enemy,
        range,
        enemy.position,
        distance >= range.desiredMin && distance <= range.desiredMax,
        enemy.boss ? "boss_effective_range" : "enemy_effective_range");
    plan.alignMoveTargetInRange = false;
    return plan;
}

} // namespace

std::optional<AutoSimulationPlan> AutoSimulationCombatModel::makePlan(
    const GameTestSnapshot& snapshot,
    const AutoSimulationPathfinder& pathfinder,
    const AutoSimulationPathField& pathField) const
{
    if (pathField.valid()) {
        if (std::optional<CombatStance> stance = bestCombatStance(snapshot, pathfinder, pathField)) {
            const CombatRange range = combatRangeFor(snapshot, *stance->enemy);
            const bool arrivedAtStance =
                distanceSquared(snapshot.player.position, stance->world) <= StanceArriveDistance * StanceArriveDistance;
            AutoSimulationPlan plan = makeCombatPlan(
                snapshot,
                *stance->enemy,
                range,
                stance->route.nextWaypointWorld,
                arrivedAtStance && stance->openScore >= WideOpenScore,
                stance->enemy->boss ? "boss_open_ground_path" : "enemy_open_ground_path");
            applyRouteMetadata(plan, stance->route);
            return plan;
        }

        if (std::optional<AutoSimulationPlan> fallback = makeFallbackPlan(snapshot, pathfinder, pathField)) {
            return fallback;
        }

        return std::nullopt;
    }

    const GameTestEnemySnapshot* enemy = nearestEnemy(snapshot);
    if (enemy == nullptr) {
        return std::nullopt;
    }
    return makeDirectPlan(snapshot, *enemy);
}

} // namespace majo::autosim
