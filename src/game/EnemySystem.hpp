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
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace majo {

class EffectSystem;
class MagicSystem;
class WorldDropSystem;
class EncyclopediaSystem;

enum class EnemyEventType {
    Hit,
    AttackHit,
    Spawn,
    Alert,
    Attack,
    Shoot,
    Heal,
    Explode,
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
    bool critical = false;
    int moneyDrop = 0;
    std::string objectDropId;
    std::string objectDropProfile;
    int objectDropCount = 0;
};

enum class CaptureResultType {
    NoTarget,
    Success,
    Failed,
    OutOfRange,
    InventoryFull,
    BossLocked,
    BossAlreadyOwned,
};

struct CaptureResult {
    CaptureResultType type = CaptureResultType::NoTarget;
    std::string enemyName;
    std::string objectId;
    std::string instanceId;
    float chance = 0.0f;
    Vec2 position{};
    bool protectable = false;
};

struct CaptureTargetPreview {
    int enemyRuntimeId = 0;
    CaptureResultType blockedReason = CaptureResultType::NoTarget;
    bool challengeable = false;
    Vec2 position{};
};

struct DugEnemySpawnPoint {
    Vec2 tileCenter{};
    int depthRank = 1;
};

struct EnemyMagicHitSpec {
    Vec2 position{};
    float radius = 0.0f;
    int damage = 0;
    std::string damageType;
    std::string effectId;
    std::string statusEffect;
    double statusValue = 1.0;
    double statusDuration = 0.0;
    double statusChance = 100.0;
    std::string consumeStateForBonus;
    int consumeStateBonusDamage = 0;
    std::string consumeStateBonusDamageType;
    std::string consumeStateBonusEffectId;
    int* outConsumedStateCount = nullptr;
    Vec2 knockbackDirection{};
    float knockbackStrength = 0.0f;
    int maxHits = 0;
    int excludedRuntimeId = 0;
};

struct EnemyMinimapMarker {
    Vec2 position{};
    bool boss = false;
};

class EnemySystem {
public:
    void spawnFromDugTiles(
        const std::vector<DugEnemySpawnPoint>& dugTiles,
        TileMap& map,
        Vec2 playerPosition,
        const RuntimeBalance& balance,
        const EnemyCatalog& enemyCatalog,
        std::string_view stageId);
    bool spawnNodeEnemy(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool allowNearPlayer, bool detectedOnSpawn = false);
    bool spawnFixedNodeEnemy(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn = false);
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
        MagicSystem& magic,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    void render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights, int highlightedEnemyId = 0);
    void renderShadows(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights) const;
    void appendRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        const TileMap& map,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights,
        int highlightedEnemyId = 0) const;
    void emitStatusParticles(EffectSystem& effects) const;
    int activeCount() const { return enemies_.activeCount(); }
    bool bossActive() const;
    void appendMinimapMarkers(std::vector<EnemyMinimapMarker>& markers) const;
    const std::vector<EnemyEvent>& events() const { return events_; }
    std::string debugEnemySummary() const;
    CaptureTargetPreview previewCaptureAt(
        Vec2 targetWorld,
        const Player& player,
        bool allowBossCapture = true,
        std::string_view bossCaptureObjectId = {}) const;
    CaptureResult tryCaptureAt(
        Vec2 targetWorld,
        Player& player,
        SpellRingSystem& spellRing,
        InventorySystem& inventory,
        bool allowBossCapture = true,
        std::string_view bossCaptureObjectId = {});
    bool hitByPlayerProjectile(
        Projectile& projectile,
        Player& player,
        SpellRingSystem& spellRing,
        int damage,
        const EffectDispatcher& effectDispatcher,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    int applyObjectBreakShardDamage(
        Vec2 position,
        float radius,
        int damage,
        std::string_view damageType,
        std::string_view effectId,
        SpellRingSystem& spellRing);
    int applyColdAirAura(
        Vec2 position,
        float radius,
        float strength,
        float dt,
        std::string_view source,
        int* outFrozenCount = nullptr);
    int applyHotAir(
        Vec2 position,
        float radius,
        float strength,
        float dt,
        std::string_view source,
        SpellRingSystem& spellRing,
        int dryWetBonusDamage = 0,
        int* outHotCount = nullptr,
        int* outDriedWetCount = nullptr);
    int applyMagicArea(const EnemyMagicHitSpec& spec, SpellRingSystem& spellRing);
    bool applyMagicNearest(Vec2 origin, float range, EnemyMagicHitSpec spec, SpellRingSystem& spellRing, Vec2* outTargetPosition = nullptr);
    void applyCapturedExplosion(Vec2 position, SpellRingSystem& spellRing, int damage);
    void addMudZone(Vec2 position, float radius, float duration, float speedMultiplier, float damagePerSecond, std::string damageType);
    int pullMetalEnemies(Vec2 center, TileMap& map, float dt, float radius = 160.0f);
    int pullLightEnemies(Vec2 center, TileMap& map, float dt, float radius, float strength = 1.0f);
    int pushLightEnemies(Vec2 center, TileMap& map, float dt, float radius, float strength = 1.0f);
    void clearSpawnBiases();
    void applySpawnBias(std::string_view group, double multiplier);
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
    Enemy* findCaptureTarget(Vec2 targetWorld);
    const Enemy* findCaptureTarget(Vec2 targetWorld) const;

    void setAwareness(Enemy& enemy, EnemyAwarenessState nextState, bool showIcon);
    void forceDetectInSight(Enemy& enemy, Vec2 playerPosition, bool showIcon);
    const EnemyDefinition* chooseEnemyDefinition(const EnemyCatalog& enemyCatalog);
    const EnemyDefinition* chooseNormalRandomEnemyDefinition(const EnemyCatalog& enemyCatalog);
    const EnemyDefinition* chooseDugSpawnEnemyDefinition(const EnemyCatalog& enemyCatalog, std::string_view stageId, int depthRank);
    double spawnBiasMultiplierFor(const EnemyDefinition& definition) const;
    void logSpawnWeightFallbackOnce(std::string key, std::string message);
    void applyDefinition(Enemy& enemy, const EnemyDefinition* definition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog);
    bool spawnDefinitionAt(Vec2 position, const EnemyDefinition* definition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn = false, Vec2 detectedTarget = {}, float spawnWarmupOverride = -1.0f);
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
    void finishEnemyDeath(Enemy& enemy, SpellRingSystem& spellRing);
    int applyConductiveShock(Vec2 position, float radius, double value, double duration, int excludedEnemyId, std::string_view source);

    ObjectPool<Enemy, balance::MaxEnemies> enemies_;
    std::vector<EnemyEvent> events_;
    int pendingXp_ = 0;
    int dugSpawnCounter_ = 0;
    int nextEnemyId_ = 1;
    std::mt19937 rng_{std::random_device{}()};
    std::unordered_set<std::string> loggedUnknownAi_;
    std::unordered_set<std::string> loggedUnknownUnawareAi_;
    std::unordered_set<std::string> loggedUnsupportedBehavior_;
    std::unordered_set<std::string> loggedSpawnWeightFallbacks_;
    std::unordered_map<std::string, double> spawnBiasMultipliers_;
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
