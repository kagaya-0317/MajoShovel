#pragma once

#include "engine/ObjectPool.hpp"
#include "engine/Renderer.hpp"
#include "data/GameBalance.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/Enemy.hpp"
#include "game/OrbitSystem.hpp"
#include "game/Player.hpp"
#include "game/TileMap.hpp"
#include <vector>

namespace majo {

class EnemySystem {
public:
    void spawnFromDugTiles(const std::vector<Vec2>& dugTiles, Vec2 playerPosition, const RuntimeBalance& balance);
    void update(Player& player, OrbitSystem& orbit, TileMap& map, float dt, float totalTime, bool paused, const RuntimeBalance& balance);
    void render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<Vec2>& extraLights);
    int activeCount() const { return enemies_.activeCount(); }
    int consumePendingXp();

private:
    void spawnAt(Vec2 position, const RuntimeBalance& balance);
    void rebuildFlowField(TileMap& map, Vec2 playerPosition);
    Vec2 flowDirectionFor(TileMap& map, Vec2 enemyPosition, Vec2 playerPosition) const;
    void moveWithCollision(Enemy& enemy, TileMap& map, Vec2 desiredVelocity, float dt);

    ObjectPool<Enemy, balance::MaxEnemies> enemies_;
    int pendingXp_ = 0;
    int dugSpawnCounter_ = 0;
    int flowMinX_ = 0;
    int flowMinY_ = 0;
    int flowWidth_ = 0;
    int flowHeight_ = 0;
    float flowTimer_ = 0.0f;
    std::vector<int> flowDistance_;
};

}
