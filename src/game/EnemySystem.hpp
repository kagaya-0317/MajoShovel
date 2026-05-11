#pragma once

#include "engine/ObjectPool.hpp"
#include "engine/Renderer.hpp"
#include "data/EnemyCatalog.hpp"
#include "data/GameBalance.hpp"
#include "data/ObjectCatalog.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/EffectDispatcher.hpp"
#include "game/Enemy.hpp"
#include "game/InventorySystem.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/Player.hpp"
#include "game/ProjectileSystem.hpp"
#include "game/TileMap.hpp"
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

namespace majo {

class EffectSystem;
class WorldDropSystem;

enum class EnemyEventType {
    Hit,
    Death,
    BossDeath,
    RewardDrop,
    CapturedExplosion
};

struct EnemyEvent {
    EnemyEventType type = EnemyEventType::Hit;
    Vec2 position{};
    std::string enemyId;
    std::string enemyName;
    std::string effectId;
    int moneyDrop = 0;
};

enum class CaptureResultType {
    NoTarget,
    Success,
    Failed,
    InventoryFull,
};

struct CaptureResult {
    CaptureResultType type = CaptureResultType::NoTarget;
    std::string enemyName;
    float chance = 0.0f;
    Vec2 position{};
};

class EnemySystem {
public:
    void spawnFromDugTiles(const std::vector<Vec2>& dugTiles, TileMap& map, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog);
    bool spawnNodeEnemy(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool allowNearPlayer, bool detectedOnSpawn = false);
    bool spawnBoss(TileMap& map, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog);
    bool spawnBossNear(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog);
    void update(
        Player& player,
        SpellRingSystem& spellRing,
        TileMap& map,
        float dt,
        float totalTime,
        bool paused,
        const RuntimeBalance& balance,
        const ObjectCatalog& objectCatalog,
        const WorldDropSystem& worldDrops,
        const EffectDispatcher& effectDispatcher,
        ProjectileSystem& projectiles,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr);
    void render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights);
    void emitStatusParticles(EffectSystem& effects) const;
    int activeCount() const { return enemies_.activeCount(); }
    bool bossActive() const;
    const std::vector<EnemyEvent>& events() const { return events_; }
    std::string debugEnemySummary() const;
    CaptureResult tryCapture(Player& player, SpellRingSystem& spellRing, InventorySystem& inventory);
    bool hitByPlayerProjectile(
        Projectile& projectile,
        Player& player,
        SpellRingSystem& spellRing,
        int damage,
        const EffectDispatcher& effectDispatcher,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr);
    void applyCapturedExplosion(Vec2 position, SpellRingSystem& spellRing, int damage);
    int pullMetalEnemies(Vec2 center, TileMap& map, float dt);
    int consumePendingXp();
    void clearTemporaryState();

private:
    void setAwareness(Enemy& enemy, EnemyAwarenessState nextState, bool showIcon);
    void forceDetectInSight(Enemy& enemy, Vec2 playerPosition, bool showIcon);
    const EnemyDefinition* chooseEnemyDefinition(const EnemyCatalog& enemyCatalog);
    void applyDefinition(Enemy& enemy, const EnemyDefinition* definition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog);
    void spawnAt(Vec2 position, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn = false, Vec2 detectedTarget = {});
    bool spawnBossAt(Vec2 position, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn = false, Vec2 detectedTarget = {});
    bool findSpawnPosition(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, Vec2& outPosition) const;
    bool findSpawnPosition(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, float radius, float minPlayerDistance, Vec2& outPosition) const;
    bool findBossSpawnPosition(TileMap& map, Vec2 playerPosition, const RuntimeBalance& balance, Vec2& outPosition) const;
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
    std::mt19937 rng_{std::random_device{}()};
    std::unordered_set<std::string> loggedUnknownAi_;
    std::unordered_set<std::string> loggedUnknownUnawareAi_;
    std::unordered_set<std::string> loggedUnsupportedBehavior_;
    int flowMinX_ = 0;
    int flowMinY_ = 0;
    int flowWidth_ = 0;
    int flowHeight_ = 0;
    float flowTimer_ = 0.0f;
    std::vector<int> flowDistance_;
};

}
