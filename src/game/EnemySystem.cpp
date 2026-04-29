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
constexpr float SpawnAvoidancePadding = 5.0f;
constexpr float PlayerPushShare = 0.35f;
constexpr float EnemyPushShare = 0.65f;

bool tryMoveCircle(TileMap& map, Vec2& position, float radius, Vec2 delta)
{
    if (lengthSquared(delta) <= 0.0001f) {
        return false;
    }

    Vec2 next = position + delta;
    if (!map.isCircleBlocked(next, radius)) {
        position = next;
        return true;
    }

    next = position + Vec2{delta.x, 0.0f};
    if (!map.isCircleBlocked(next, radius)) {
        position = next;
        return true;
    }

    next = position + Vec2{0.0f, delta.y};
    if (!map.isCircleBlocked(next, radius)) {
        position = next;
        return true;
    }

    return false;
}

}

void EnemySystem::spawnAt(Vec2 position, const RuntimeBalance& balance)
{
    Enemy* enemy = enemies_.acquire();
    if (!enemy) {
        return;
    }
    enemy->id = nextEnemyId_++;
    enemy->position = position;
    enemy->radius = balance.enemyRadius;
    enemy->hp = balance.enemyHp + std::max(0, activeCount() / 12);
    enemy->xp = balance.enemyXp;
    enemy->spawnTimer = balance.enemySpawnWarmup;
    enemy->spawnDuration = balance.enemySpawnWarmup;
}

bool EnemySystem::findSpawnPosition(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, Vec2& outPosition) const
{
    const float spacing = balance.enemyRadius * 2.4f;
    const std::array<Vec2, 13> offsets{{
        {0.0f, 0.0f},
        {spacing, 0.0f},
        {-spacing, 0.0f},
        {0.0f, spacing},
        {0.0f, -spacing},
        {spacing, spacing},
        {-spacing, spacing},
        {spacing, -spacing},
        {-spacing, -spacing},
        {spacing * 2.0f, 0.0f},
        {-spacing * 2.0f, 0.0f},
        {0.0f, spacing * 2.0f},
        {0.0f, -spacing * 2.0f},
    }};

    const float minPlayerDistanceSq = balance.enemyMinSpawnDistance * balance.enemyMinSpawnDistance;
    for (Vec2 offset : offsets) {
        const Vec2 candidate = desiredPosition + offset;
        if (distanceSquared(candidate, playerPosition) < minPlayerDistanceSq) {
            continue;
        }
        if (map.isCircleBlocked(candidate, balance.enemyRadius)) {
            continue;
        }

        bool overlapsEnemy = false;
        for (const Enemy& enemy : enemies_.items()) {
            if (!enemy.active) {
                continue;
            }
            const float minDistance = enemy.radius + balance.enemyRadius + SpawnAvoidancePadding;
            if (distanceSquared(candidate, enemy.position) < minDistance * minDistance) {
                overlapsEnemy = true;
                break;
            }
        }
        if (!overlapsEnemy) {
            outPosition = candidate;
            return true;
        }
    }

    return false;
}

void EnemySystem::spawnFromDugTiles(const std::vector<Vec2>& dugTiles, TileMap& map, Vec2 playerPosition, const RuntimeBalance& balance)
{
    if (activeCount() >= balance.enemySoftCap) {
        return;
    }
    for (Vec2 tileCenter : dugTiles) {
        ++dugSpawnCounter_;
        if (dugSpawnCounter_ % balance.enemyDugSpawnEvery != 0) {
            continue;
        }
        Vec2 spawnPosition{};
        if (!findSpawnPosition(map, tileCenter, playerPosition, balance, spawnPosition)) {
            continue;
        }
        spawnAt(spawnPosition, balance);
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

Vec2 EnemySystem::separationFor(const Enemy& enemy) const
{
    Vec2 separation{};
    for (std::size_t i = 0; i < enemies_.items().size(); ++i) {
        const Enemy& other = enemies_.items()[i];
        if (!other.active || &other == &enemy || other.spawnTimer > 0.0f) {
            continue;
        }

        Vec2 away = enemy.position - other.position;
        const float minDistance = enemy.radius + other.radius + SpawnAvoidancePadding;
        const float minDistanceSq = minDistance * minDistance;
        const float distSq = lengthSquared(away);
        if (distSq >= minDistanceSq) {
            continue;
        }
        if (distSq <= 0.0001f) {
            away = fromAngle(static_cast<float>(i) * 2.399963f);
        }
        const float dist = std::max(1.0f, std::sqrt(distSq));
        const float strength = 1.0f - clamp(dist / minDistance, 0.0f, 1.0f);
        separation += normalize(away) * strength;
    }
    return separation;
}

void EnemySystem::moveWithCollision(Enemy& enemy, TileMap& map, Vec2 desiredVelocity, float dt)
{
    const Vec2 delta = desiredVelocity * dt;
    if (lengthSquared(delta) <= 0.0001f) {
        return;
    }

    Vec2 next = enemy.position + delta;
    if (!map.isCircleBlocked(next, enemy.radius)) {
        enemy.position = next;
        return;
    }

    next = enemy.position + Vec2{delta.x, 0.0f};
    if (!map.isCircleBlocked(next, enemy.radius)) {
        enemy.position = next;
        return;
    }
    next = enemy.position + Vec2{0.0f, delta.y};
    if (!map.isCircleBlocked(next, enemy.radius)) {
        enemy.position = next;
        return;
    }

    const Vec2 direction = normalize(desiredVelocity);
    const float step = length(delta);
    const Vec2 side{-direction.y, direction.x};
    const std::array<Vec2, 4> fallbackDirections{{
        side,
        side * -1.0f,
        normalize(side + direction * 0.35f),
        normalize(side * -1.0f + direction * 0.35f),
    }};
    for (Vec2 fallback : fallbackDirections) {
        next = enemy.position + fallback * step;
        if (!map.isCircleBlocked(next, enemy.radius)) {
            enemy.position = next;
            return;
        }
    }
}

bool EnemySystem::resolvePlayerOverlap(Player& player, Enemy& enemy, TileMap& map, const RuntimeBalance& balance)
{
    Vec2 fromPlayer = enemy.position - player.position;
    const float minimumDistance = enemy.radius + balance.playerRadius;
    float distSq = lengthSquared(fromPlayer);
    if (distSq >= minimumDistance * minimumDistance) {
        return false;
    }

    if (distSq <= 0.0001f) {
        fromPlayer = lengthSquared(enemy.velocity) > 0.0001f ? normalize(enemy.velocity) : player.facing;
        distSq = 1.0f;
    }

    const float dist = std::sqrt(distSq);
    const Vec2 normal = fromPlayer / dist;
    const float overlap = minimumDistance - dist + 0.5f;

    float playerShare = PlayerPushShare;
    float enemyShare = EnemyPushShare;
    bool movedPlayer = tryMoveCircle(map, player.position, balance.playerRadius, normal * (-overlap * playerShare));
    bool movedEnemy = tryMoveCircle(map, enemy.position, enemy.radius, normal * (overlap * enemyShare));

    if (!movedPlayer && movedEnemy) {
        tryMoveCircle(map, enemy.position, enemy.radius, normal * (overlap * playerShare));
    } else if (!movedEnemy && movedPlayer) {
        tryMoveCircle(map, player.position, balance.playerRadius, normal * (-overlap * enemyShare));
    } else if (!movedEnemy && !movedPlayer) {
        tryMoveCircle(map, enemy.position, enemy.radius, normal * overlap);
    }

    return true;
}

void EnemySystem::update(Player& player, SpellRingSystem& spellRing, TileMap& map, float dt, float totalTime, bool paused, const RuntimeBalance& balance)
{
    events_.clear();
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
        enemy.status.update(dt);
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
        if (enemy.spawnTimer > 0.0f) {
            enemy.spawnTimer = std::max(0.0f, enemy.spawnTimer - dt);
            continue;
        }

        const Vec2 direction = flowDirectionFor(map, enemy.position, player.position);
        const float enemySpeed = static_cast<float>(enemy.status.applyModifiers(ModifierStat::Speed, balance.enemySpeed));
        enemy.velocity = direction * enemySpeed + separationFor(enemy) * balance.enemySeparationStrength;
        const float maxSpeed = enemySpeed * 1.75f;
        if (lengthSquared(enemy.velocity) > maxSpeed * maxSpeed) {
            enemy.velocity = normalize(enemy.velocity) * maxSpeed;
        }
        moveWithCollision(enemy, map, enemy.velocity, dt);
        enemy.contactTimer = std::max(0.0f, enemy.contactTimer - dt);
        const bool touchedPlayer = resolvePlayerOverlap(player, enemy, map, balance);
        if (touchedPlayer && enemy.contactTimer <= 0.0f) {
            player.hp = std::max(0, player.hp - 1);
            enemy.contactTimer = 0.8f;
        }

        for (auto& item : spellRing.items()) {
            const bool overlappingItem = circlesOverlap(enemy.position, enemy.radius, item.worldPosition, item.hitRadius);
            if (item.type == SpellRingItemType::Shovel && !overlappingItem) {
                item.unlatchEnemy(enemy.id);
                continue;
            }
            if (!overlappingItem) {
                continue;
            }
            if (item.type == SpellRingItemType::Shovel) {
                if (item.isEnemyLatched(enemy.id)) {
                    continue;
                }
                item.latchEnemy(enemy.id);
            } else if (totalTime - item.lastEnemyHitTime < item.hitInterval) {
                continue;
            }
            item.lastEnemyHitTime = totalTime;
            const int speedBonus = static_cast<int>(spellRing.angularSpeed() * 0.25f);
            const int modifiedDamage = static_cast<int>(player.status.applyModifiers(ModifierStat::Attack, item.damage));
            enemy.hp -= modifiedDamage + (item.type == SpellRingItemType::Shovel ? speedBonus : 0);
            enemy.hitFlash = 0.12f;
            events_.push_back({EnemyEventType::Hit, enemy.position});
            if (enemy.hp <= 0) {
                pendingXp_ += enemy.xp;
                events_.push_back({EnemyEventType::Death, enemy.position});
                for (auto& clearItem : spellRing.items()) {
                    clearItem.unlatchEnemy(enemy.id);
                }
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
        if (enemy.spawnTimer > 0.0f) {
            const float ratio = enemy.spawnDuration > 0.0f ? enemy.spawnTimer / enemy.spawnDuration : 0.0f;
            const float pulse = 1.0f + (1.0f - ratio) * 0.9f;
            renderer.drawCircle(enemy.position, enemy.radius * pulse + 4.0f, {190, 58, 76, 210});
            renderer.drawCircle(enemy.position, enemy.radius * 0.55f, {255, 160, 110, 190});
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
