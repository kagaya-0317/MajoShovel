#pragma once

#include "engine/ObjectPool.hpp"
#include "engine/Renderer.hpp"
#include "data/GameBalance.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/Enemy.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/Player.hpp"
#include "game/TileMap.hpp"
#include <vector>

namespace majo {

enum class EnemyEventType {
    Hit,
    Death
};

struct EnemyEvent {
    EnemyEventType type = EnemyEventType::Hit;
    Vec2 position{};
};

class EnemySystem {
public:
    void spawnFromDugTiles(const std::vector<Vec2>& dugTiles, TileMap& map, Vec2 playerPosition, const RuntimeBalance& balance);
    void update(Player& player, SpellRingSystem& spellRing, TileMap& map, float dt, float totalTime, bool paused, const RuntimeBalance& balance);
    void render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<Vec2>& extraLights);
    int activeCount() const { return enemies_.activeCount(); }
    const std::vector<EnemyEvent>& events() const { return events_; }
    int consumePendingXp();

private:
    void spawnAt(Vec2 position, const RuntimeBalance& balance);
    bool findSpawnPosition(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, Vec2& outPosition) const;
    void rebuildFlowField(TileMap& map, Vec2 playerPosition);
    Vec2 flowDirectionFor(TileMap& map, Vec2 enemyPosition, Vec2 playerPosition) const;
    Vec2 separationFor(const Enemy& enemy) const;
    void moveWithCollision(Enemy& enemy, TileMap& map, Vec2 desiredVelocity, float dt);
    bool resolvePlayerOverlap(Player& player, Enemy& enemy, TileMap& map, const RuntimeBalance& balance);

    ObjectPool<Enemy, balance::MaxEnemies> enemies_;
    std::vector<EnemyEvent> events_;
    int pendingXp_ = 0;
    int dugSpawnCounter_ = 0;
    int nextEnemyId_ = 1;
    int flowMinX_ = 0;
    int flowMinY_ = 0;
    int flowWidth_ = 0;
    int flowHeight_ = 0;
    float flowTimer_ = 0.0f;
    std::vector<int> flowDistance_;
};

}
