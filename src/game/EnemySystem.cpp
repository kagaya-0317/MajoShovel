#include "game/EnemySystem.hpp"

#include "data/GameBalance.hpp"
#include "game/Collision.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <queue>

namespace majo {

namespace {

constexpr int FlowRadiusTiles = 80;

}

void EnemySystem::spawnAt(Vec2 position, const RuntimeBalance& balance)
{
    Enemy* enemy = enemies_.acquire();
    if (!enemy) {
        return;
    }
    enemy->position = position;
    enemy->radius = balance.enemyRadius;
    enemy->hp = balance.enemyHp + std::max(0, activeCount() / 12);
    enemy->xp = balance.enemyXp;
}

void EnemySystem::spawnFromDugTiles(const std::vector<Vec2>& dugTiles, Vec2 playerPosition, const RuntimeBalance& balance)
{
    if (activeCount() >= balance.enemySoftCap) {
        return;
    }
    for (Vec2 tileCenter : dugTiles) {
        if (distanceSquared(tileCenter, playerPosition) < 48.0f * 48.0f) {
            continue;
        }
        ++dugSpawnCounter_;
        if (dugSpawnCounter_ % balance.enemyDugSpawnEvery != 0) {
            continue;
        }
        spawnAt(tileCenter, balance);
        if (activeCount() >= balance.enemySoftCap) {
            return;
        }
    }
}

void EnemySystem::rebuildFlowField(TileMap& map, Vec2 playerPosition)
{
    const int playerTileX = map.worldToTile(playerPosition.x);
    const int playerTileY = map.worldToTile(playerPosition.y);
    flowMinX_ = playerTileX - FlowRadiusTiles;
    flowMinY_ = playerTileY - FlowRadiusTiles;
    flowWidth_ = FlowRadiusTiles * 2 + 1;
    flowHeight_ = FlowRadiusTiles * 2 + 1;
    flowDistance_.assign(static_cast<std::size_t>(flowWidth_ * flowHeight_), -1);

    auto inBounds = [&](int tx, int ty) {
        return tx >= flowMinX_ && ty >= flowMinY_ && tx < flowMinX_ + flowWidth_ && ty < flowMinY_ + flowHeight_;
    };
    auto index = [&](int tx, int ty) {
        return (ty - flowMinY_) * flowWidth_ + (tx - flowMinX_);
    };

    if (!inBounds(playerTileX, playerTileY) || map.isTileSolid(playerTileX, playerTileY)) {
        return;
    }

    std::queue<std::pair<int, int>> open;
    flowDistance_[static_cast<std::size_t>(index(playerTileX, playerTileY))] = 0;
    open.emplace(playerTileX, playerTileY);

    constexpr std::array<std::pair<int, int>, 4> Directions{{
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    }};

    while (!open.empty()) {
        const auto [tx, ty] = open.front();
        open.pop();
        const int current = flowDistance_[static_cast<std::size_t>(index(tx, ty))];
        for (const auto [dx, dy] : Directions) {
            const int nx = tx + dx;
            const int ny = ty + dy;
            if (!inBounds(nx, ny) || map.isTileSolid(nx, ny)) {
                continue;
            }
            int& next = flowDistance_[static_cast<std::size_t>(index(nx, ny))];
            if (next >= 0) {
                continue;
            }
            next = current + 1;
            open.emplace(nx, ny);
        }
    }
}

Vec2 EnemySystem::flowDirectionFor(TileMap& map, Vec2 enemyPosition, Vec2 playerPosition) const
{
    const int enemyTileX = map.worldToTile(enemyPosition.x);
    const int enemyTileY = map.worldToTile(enemyPosition.y);
    auto inBounds = [&](int tx, int ty) {
        return tx >= flowMinX_ && ty >= flowMinY_ && tx < flowMinX_ + flowWidth_ && ty < flowMinY_ + flowHeight_;
    };
    auto index = [&](int tx, int ty) {
        return (ty - flowMinY_) * flowWidth_ + (tx - flowMinX_);
    };

    if (!inBounds(enemyTileX, enemyTileY) || flowDistance_.empty()) {
        return normalize(playerPosition - enemyPosition);
    }

    const int current = flowDistance_[static_cast<std::size_t>(index(enemyTileX, enemyTileY))];
    if (current < 0) {
        return {0.0f, 0.0f};
    }

    Vec2 bestTarget = map.tileCenter(enemyTileX, enemyTileY);
    int bestDistance = current;
    constexpr std::array<std::pair<int, int>, 4> Directions{{
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    }};
    for (const auto [dx, dy] : Directions) {
        const int nx = enemyTileX + dx;
        const int ny = enemyTileY + dy;
        if (!inBounds(nx, ny)) {
            continue;
        }
        const int candidate = flowDistance_[static_cast<std::size_t>(index(nx, ny))];
        if (candidate >= 0 && candidate < bestDistance) {
            bestDistance = candidate;
            bestTarget = map.tileCenter(nx, ny);
        }
    }

    if (bestDistance == current) {
        bestTarget = playerPosition;
    }
    return normalize(bestTarget - enemyPosition);
}

void EnemySystem::moveWithCollision(Enemy& enemy, TileMap& map, Vec2 desiredVelocity, float dt)
{
    const Vec2 delta = desiredVelocity * dt;
    Vec2 next = enemy.position + delta;
    if (!map.isCircleBlocked(next, enemy.radius)) {
        enemy.position = next;
        return;
    }

    next = enemy.position + Vec2{delta.x, 0.0f};
    if (!map.isCircleBlocked(next, enemy.radius)) {
        enemy.position = next;
    }
    next = enemy.position + Vec2{0.0f, delta.y};
    if (!map.isCircleBlocked(next, enemy.radius)) {
        enemy.position = next;
    }
}

void EnemySystem::update(Player& player, OrbitSystem& orbit, TileMap& map, float dt, float totalTime, bool paused, const RuntimeBalance& balance)
{
    if (paused) {
        return;
    }

    flowTimer_ -= dt;
    if (flowTimer_ <= 0.0f || flowDistance_.empty()) {
        rebuildFlowField(map, player.position);
        flowTimer_ = 0.20f;
    }

    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active) {
            continue;
        }
        const Vec2 direction = flowDirectionFor(map, enemy.position, player.position);
        enemy.velocity = direction * balance.enemySpeed;
        moveWithCollision(enemy, map, enemy.velocity, dt);
        enemy.contactTimer = std::max(0.0f, enemy.contactTimer - dt);
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
        if (circlesOverlap(enemy.position, enemy.radius, player.position, balance.playerRadius) && enemy.contactTimer <= 0.0f) {
            player.hp = std::max(0, player.hp - 1);
            enemy.contactTimer = 0.8f;
        }

        for (auto& item : orbit.items()) {
            if (!circlesOverlap(enemy.position, enemy.radius, item.worldPosition, item.hitRadius)) {
                continue;
            }
            if (totalTime - item.lastEnemyHitTime < item.hitInterval) {
                continue;
            }
            item.lastEnemyHitTime = totalTime;
            const int speedBonus = static_cast<int>(orbit.angularSpeed() * 0.25f);
            enemy.hp -= item.damage + (item.type == OrbitItemType::Shovel ? speedBonus : 0);
            enemy.hitFlash = 0.12f;
            if (enemy.hp <= 0) {
                pendingXp_ += enemy.xp;
                enemy.active = false;
                break;
            }
        }
    }
}

void EnemySystem::render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<Vec2>& extraLights)
{
    for (const Enemy& enemy : enemies_.items()) {
        if (!enemy.active) {
            continue;
        }
        if (!map.isLit(enemy.position, playerLight, extraLights)) {
            continue;
        }
        const Color color = enemy.hitFlash > 0.0f ? Color{255, 220, 220, 255} : Color{178, 38, 54, 255};
        renderer.fillCircle(enemy.position, enemy.radius, color);
        renderer.drawCircle(enemy.position, enemy.radius + 3.0f, {80, 18, 28, 255});
    }
}

int EnemySystem::consumePendingXp()
{
    const int xp = pendingXp_;
    pendingXp_ = 0;
    return xp;
}

}
