#pragma once

#include "engine/ObjectPool.hpp"
#include "engine/Renderer.hpp"
#include "data/EnemyCatalog.hpp"
#include "data/GameBalance.hpp"
#include "data/ObjectCatalog.hpp"
#include "data/RuntimeBalance.hpp"
#include "game/DepthRender.hpp"
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
class EncyclopediaSystem;

enum class EnemyEventType {
    Hit,
    AttackHit,
    Death,
    BossDeath,
    RewardDrop,
    ObjectDrop,
    CapturedExplosion
};

struct EnemyEvent {
    EnemyEventType type = EnemyEventType::Hit;
    Vec2 position{};
    std::string enemyId;
    std::string enemyName;
    std::string effectId;
    int damageAmount = -1;
    int moneyDrop = 0;
    std::string objectDropId;
    int objectDropCount = 0;
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
    bool spawnSpecificEnemy(TileMap& map, std::string_view enemyId, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool allowNearPlayer, bool detectedOnSpawn = false);
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
        WorldDropSystem& worldDrops,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights,
        const EffectDispatcher& effectDispatcher,
        ProjectileSystem& projectiles,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    void render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights);
    void renderShadows(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights) const;
    void appendRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        const TileMap& map,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights) const;
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
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    void applyCapturedExplosion(Vec2 position, SpellRingSystem& spellRing, int damage);
    void addMudZone(Vec2 position, float radius, float duration, float speedMultiplier, float damagePerSecond, std::string damageType);
    int pullMetalEnemies(Vec2 center, TileMap& map, float dt, float radius = 160.0f);
    int consumePendingXp();
    void clearTemporaryState();

private:
    struct MudZone {
        Vec2 position{};
        float radius = 0.0f;
        float remainingSeconds = 0.0f;
        float speedMultiplier = 1.0f;
        float damagePerSecond = 0.0f;
        std::string damageType = "poison";
    };

    void queueEnemyObjectDrops(Enemy& enemy);

    void setAwareness(Enemy& enemy, EnemyAwarenessState nextState, bool showIcon);
    void forceDetectInSight(Enemy& enemy, Vec2 playerPosition, bool showIcon);
    const EnemyDefinition* chooseEnemyDefinition(const EnemyCatalog& enemyCatalog);
    void applyDefinition(Enemy& enemy, const EnemyDefinition* definition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog);
    bool spawnDefinitionAt(Vec2 position, const EnemyDefinition* definition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn = false, Vec2 detectedTarget = {});
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
    std::vector<MudZone> mudZones_;
    double mudDamageAccumulator_ = 0.0;
};

}
