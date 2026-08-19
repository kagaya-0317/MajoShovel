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
#include "game/EnemyPlacement.hpp"
#include "game/EnemyShadow.hpp"
#include "game/Hitbox.hpp"
#include "game/InventorySystem.hpp"
#include "game/ItemModel.hpp"
#include "game/RingImpactSound.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/Player.hpp"
#include "game/ProjectileSystem.hpp"
#include "game/TileMap.hpp"
#include <array>
#include <functional>
#include <random>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace majo {

class EffectSystem;
class MagicSystem;
class WorldDropSystem;
class EncyclopediaSystem;
struct CollisionRect;
struct WetGroundEmitter;

enum class EnemyEventType {
    Hit,
    AttackHit,
    Spawn,
    Alert,
    Attack,
    Shoot,
    HealCast,
    Heal,
    ExplosionWarningTick,
    Explode,
    BossTelegraph,
    BossImpact,
    TerrainHit,
    TerrainBreak,
    Death,
    BossDeath,
    BossResolved,
    Steal,
    RewardDrop,
    ObjectDrop,
    MoneyDrop,
    MaterialDrop,
    CapturedExplosion,
    Inspected
};

struct EnemyEvent {
    EnemyEventType type = EnemyEventType::Hit;
    Vec2 position{};
    Vec2 effectDirection{};
    int enemyRuntimeId = 0;
    std::string dungeonEventId;
    std::string enemyId;
    std::string enemyName;
    std::string effectId;
    int damageAmount = -1;
    float effectRadius = 0.0f;
    float terrainRadius = 0.0f;
    int terrainDamage = -1;
    TileType terrainTileType = TileType::Dirt;
    Color terrainColor{0, 0, 0, 0};
    int healAmount = 0;
    bool critical = false;
    bool frontGuarded = false;
    bool weakPointHit = false;
    bool ringItemImpact = false;
    bool playerDealtDamage = false;
    bool suppressRewards = false;
    int moneyDrop = 0;
    std::string objectDropId;
    std::string objectDropProfile;
    int objectDropCount = 0;
    std::optional<ItemInstance> objectDropInstance;
    std::optional<ItemData> objectDropRuntimeItem;
    MaterialType materialDropType = MaterialType::Count;
    int materialDropCount = 0;
};

enum class CaptureResultType {
    NoTarget,
    Success,
    Failed,
    OutOfRange,
    InventoryFull,
    BossLocked,
    BossAlreadyOwned,
    KnowledgeLocked,
};

struct EventEnemySpawnOptions {
    std::string enemyId;
    std::string dungeonEventId;
    std::string stageId;
    int depthRank = 1;
    bool allowNearPlayer = true;
    bool detectedOnSpawn = true;
    bool fixedPosition = false;
    bool sleeping = false;
    bool activationLocked = false;
    bool bossVariant = false;
    float hpMultiplier = 1.0f;
    float contactDamageMultiplier = 1.0f;
    float radiusMultiplier = 1.0f;
    float xpMultiplier = 1.0f;
};

struct CaptureResult {
    CaptureResultType type = CaptureResultType::NoTarget;
    std::string enemyName;
    std::string objectId;
    std::string instanceId;
    ItemData capturedItem;
    Enemy capturedEnemy;
    float chance = 0.0f;
    Vec2 position{};
    bool protectable = false;
};

[[nodiscard]] ObjectDefinition makeCapturedObjectDefinition(
    const EnemyDefinition& enemy,
    EnemyVariantTier variantTier = EnemyVariantTier::Normal);

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

struct DugEnemySpawnRequest {
    Vec2 position{};
    int depthRank = 1;
};

struct EnemyMagicHitSpec {
    Vec2 position{};
    float radius = 0.0f;
    int damage = 0;
    double ringItemDamageMultiplier = 1.0;
    std::string damageType;
    std::string effectId;
    std::string statusEffect;
    double statusValue = 1.0;
    double statusDuration = 0.0;
    double statusChance = 100.0;
    Vec2 knockbackDirection{};
    float knockbackStrength = 0.0f;
    int maxHits = 0;
    int excludedRuntimeId = 0;
};

struct EnemyMinimapMarker {
    Vec2 position{};
    float radius = 10.0f;
    float jumpLandingRadius = 0.0f;
    float countdownExplodeRadius = 0.0f;
    int contactAttackPower = 0;
    float contactDamageMultiplier = 1.0f;
    bool ranged = false;
    bool boss = false;
};

struct EnemyWindPulse {
    Vec2 center{};
    Vec2 direction{1.0f, 0.0f};
    float radius = 0.0f;
    float strength = 1.0f;
    float remainingSeconds = 0.0f;
    float initialSeconds = 0.0f;
    int sourceRuntimeId = 0;
};

struct EnemySoundEvent {
    std::string cueId;
    Vec2 position{};
    float volumeScale = 1.0f;
    float pitchScale = 1.0f;
};

class EnemySystem {
public:
    void setHitboxCatalog(const HitboxCatalog* catalog) { hitboxCatalog_ = catalog; }
    void setPlacementCatalog(const EnemyPlacementCatalog* catalog) { placementCatalog_ = catalog; }
    void setShadowCatalog(const EnemyShadowCatalog* catalog) { shadowCatalog_ = catalog; }
    std::vector<DugEnemySpawnRequest> collectDugSpawnRequests(
        const std::vector<DugEnemySpawnPoint>& dugTiles,
        TileMap& map,
        Vec2 playerPosition,
        const RuntimeBalance& balance,
        int reservedAmbientSpawns = 0);
    bool spawnNodeEnemy(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool allowNearPlayer, bool detectedOnSpawn = false, std::string_view lootStageId = {}, int lootDepthRank = 1, float spawnWarmupOverride = -1.0f);
    bool spawnFixedNodeEnemy(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn = false, int* outRuntimeId = nullptr, std::string_view lootStageId = {}, int lootDepthRank = 1);
    bool spawnSpecificEnemy(TileMap& map, std::string_view enemyId, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool allowNearPlayer, bool detectedOnSpawn = false, float spawnWarmupOverride = -1.0f, int* outRuntimeId = nullptr, std::string_view lootStageId = {}, int lootDepthRank = 1);
    bool spawnSpecificEnemyAtPosition(TileMap& map, std::string_view enemyId, Vec2 position, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn = false, float spawnWarmupOverride = -1.0f, int* outRuntimeId = nullptr, std::string_view lootStageId = {}, int lootDepthRank = 1);
    bool spawnEventEnemy(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, const EventEnemySpawnOptions& options, int* outRuntimeId = nullptr);
    bool spawnBoss(
        TileMap& map,
        Vec2 playerPosition,
        const RuntimeBalance& balance,
        const EnemyCatalog& enemyCatalog,
        std::string_view bossEnemyId = {},
        EnemyVariantTier variantTier = EnemyVariantTier::Normal,
        int effectiveBaseLevel = 0,
        EnemySpawnVisualKind spawnVisualKind = EnemySpawnVisualKind::Default);
    bool spawnBossNear(
        TileMap& map,
        Vec2 desiredPosition,
        Vec2 playerPosition,
        const RuntimeBalance& balance,
        const EnemyCatalog& enemyCatalog,
        std::string_view bossEnemyId = {},
        EnemyVariantTier variantTier = EnemyVariantTier::Normal,
        int effectiveBaseLevel = 0,
        EnemySpawnVisualKind spawnVisualKind = EnemySpawnVisualKind::Default);
    bool spawnBossPreviewAt(
        Vec2 position,
        Vec2 playerPosition,
        const RuntimeBalance& balance,
        const EnemyCatalog& enemyCatalog,
        std::string_view bossEnemyId = {});
    int removeBosses(std::string_view bossEnemyId = {});
    bool activateBossPreview(std::string_view bossEnemyId = {});
    bool advanceBossSpawnPresentation(float dt);
    bool configureActiveBossWalkInPresentation(Vec2 startPosition, float durationSeconds);
    void updateBossStoryVisuals(float dt);
    void update(
        Player& player,
        SpellRingSystem& spellRing,
        InventorySystem& inventory,
        TileMap& map,
        float dt,
        float totalTime,
        bool paused,
        const RuntimeBalance& balance,
        const EnemyCatalog& enemyCatalog,
        const ObjectCatalog& objectCatalog,
        WorldDropSystem& worldDrops,
        const std::function<bool(int, Vec2)>& grantMoney,
        const std::function<int(int, Vec2)>& takeMoney,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights,
        const EffectDispatcher& effectDispatcher,
        ProjectileSystem& projectiles,
        MagicSystem& magic,
        const CollisionRect& stealViewBounds,
        bool allowBossCapture = true,
        std::string_view bossCaptureObjectId = {},
        const std::unordered_set<std::string>* allowedCaptureEnemyIds = nullptr,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    void render(
        Renderer& renderer,
        const TileMap& map,
        const ObjectCatalog& objectCatalog,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights,
        int highlightedEnemyId = 0,
        const EncyclopediaSystem* encyclopedia = nullptr);
    void renderShadows(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights) const;
    void appendRenderEntries(
        std::vector<DepthRenderEntry>& entries,
        Renderer& renderer,
        const TileMap& map,
        const ObjectCatalog& objectCatalog,
        Vec2 playerLight,
        const std::vector<LightSource>& extraLights,
        int highlightedEnemyId = 0,
        const EncyclopediaSystem* encyclopedia = nullptr) const;
    void emitStatusParticles(EffectSystem& effects) const;
    void appendWetGroundEmitters(std::vector<WetGroundEmitter>& emitters) const;
    void appendHittableHitboxCircles(std::vector<WorldHitCircle>& circles) const;
    int activeCount() const { return enemies_.activeCount(); }
    int ambientActiveCount() const;
    int syncScreenDormantEnemies(const CollisionRect& activeBounds, SpellRingSystem& spellRing);
    int eventActiveCount() const;
    int bossSourceActiveCount() const;
    bool bossActive() const;
    void appendMinimapMarkers(std::vector<EnemyMinimapMarker>& markers) const;
    const std::vector<EnemyEvent>& events() const { return events_; }
    const std::vector<RingImpactSoundEvent>& impactSoundEvents() const { return impactSoundEvents_; }
    std::vector<EnemySoundEvent> consumeSoundEvents();
    std::vector<CaptureResult> consumeCaptureResults();
    std::vector<StatusPopupEvent> consumeStatusPopupEvents();
    std::string debugEnemySummary(Vec2 playerPosition) const;
    CaptureTargetPreview previewCaptureAt(
        Vec2 targetWorld,
        const Player& player,
        bool allowBossCapture = true,
        std::string_view bossCaptureObjectId = {}) const;
    CaptureTargetPreview previewCaptureInDirection(
        Vec2 origin,
        Vec2 direction,
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
    CaptureResult tryCaptureInDirection(
        Vec2 origin,
        Vec2 direction,
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
        SpellRingSystem& spellRing,
        double ringItemDamageMultiplier = 1.0);
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
        int* outHotCount = nullptr);
    int applyMagicArea(const EnemyMagicHitSpec& spec, SpellRingSystem& spellRing);
    bool applyMagicNearest(Vec2 origin, float range, EnemyMagicHitSpec spec, SpellRingSystem& spellRing, Vec2* outTargetPosition = nullptr);
    void applyExplosionDamage(Vec2 position, float radius, SpellRingSystem& spellRing, int damage, int excludedEnemyRuntimeId = 0);
    void addMudZone(
        Vec2 position,
        float radius,
        float duration,
        float speedMultiplier,
        float damagePerSecond,
        std::string damageType,
        DamageCause damageCause = DamageCause{.source = DamageSource::Poison, .objectName = "毒の泥"});
    int pullMetalEnemies(Vec2 center, TileMap& map, float dt, float radius = 160.0f);
    int pullLightEnemies(Vec2 center, TileMap& map, float dt, float radius, float strength = 1.0f);
    int pushLightEnemies(Vec2 center, TileMap& map, float dt, float radius, float strength = 1.0f);
    void clearSpawnBiases();
    void applySpawnBias(std::string_view group, double multiplier);
    void activateDungeonEventEnemies(std::string_view eventId, bool wakeSleepingEnemies);
    void wakeDungeonEventEnemies(std::string_view eventId);
    bool setManualDetectionOnlyForRuntimeEnemy(int runtimeId, bool manualOnly);
    bool setManualDetectionOnlyNear(Vec2 position, float radius, bool manualOnly);
    bool forceDetectRuntimeEnemy(int runtimeId, Vec2 playerPosition, bool showIcon = true);
    bool forceDetectEnemyNear(Vec2 position, float radius, Vec2 playerPosition, bool showIcon = true);
    bool setRuntimeEnemyMovementLeash(int runtimeId, Vec2 center, float radius);
    bool tryStealHeldDrop(
        Enemy& enemy,
        WorldDropSystem& worldDrops,
        const ObjectCatalog& objectCatalog,
        Vec2 targetPosition,
        float spawnedAtSeconds,
        float chance,
        std::string_view targetFilter = {});
    int activeDungeonEventEnemyCount(std::string_view eventId) const;
    int activeRuntimeEnemyCount(const std::vector<int>& runtimeIds) const;
    bool runtimeEnemyActive(int runtimeId) const;
    bool runtimeEnemyPosition(int runtimeId, Vec2& outPosition) const;
    bool setRuntimeEnemyHp(int runtimeId, int hp);
    int consumePendingXp();
    void clearTemporaryState();
    int activeSleepingDungeonEventEnemyCount(std::string_view eventId) const;

private:
    static constexpr std::size_t MudZoneOutlinePointCount = 16;
    static constexpr std::size_t MudZoneBubbleCount = 5;

    struct MudZoneBubble {
        Vec2 offset{};
        float radius = 0.0f;
        float phase = 0.0f;
    };

    struct MudZone {
        Vec2 position{};
        float radius = 0.0f;
        float initialSeconds = 0.0f;
        float remainingSeconds = 0.0f;
        float speedMultiplier = 1.0f;
        float damagePerSecond = 0.0f;
        std::string damageType = "poison";
        DamageCause damageCause{.source = DamageSource::Poison, .objectName = "毒の泥"};
        std::array<Vec2, MudZoneOutlinePointCount> outlineOffsets{};
        std::array<MudZoneBubble, MudZoneBubbleCount> bubbles{};
    };
    struct CaptureAttemptOptions {
        bool requirePlayerReach = true;
        float chanceMultiplier = 1.0f;
        const std::unordered_set<std::string>* allowedEnemyIds = nullptr;
    };
    struct EnemySpawnSelection {
        const EnemyDefinition* definition = nullptr;
        EnemyVariantTier variantTier = EnemyVariantTier::Normal;
        int effectiveBaseLevel = 1;
    };
    struct RoguelikeEnemyPoolEntry {
        std::string enemyId;
        EnemyVariantTier variantTier = EnemyVariantTier::Normal;
        int effectiveBaseLevel = 1;
    };
    struct SwarmSpawnRequest {
        const EnemyDefinition* definition = nullptr;
        Vec2 origin{};
        EnemySpawnSource spawnSource = EnemySpawnSource::Ambient;
        std::string lootStageId;
        int lootDepthRank = 1;
        int parentRuntimeId = 0;
        int count = 0;
        float radius = 0.0f;
        float childPassageRadius = 0.0f;
        bool detectedOnSpawn = false;
        Vec2 detectedTarget{};
        bool screenSleepAllowed = false;
    };

    void queueEnemyObjectDrops(Enemy& enemy);
    void queueEnemyHeldDrops(Enemy& enemy);
    void queueEnemyMaterialDrops(Enemy& enemy);
    void ensureEnemyHeldDropsInitialized(Enemy& enemy, const ObjectCatalog& objectCatalog);
    Enemy* findCaptureTarget(Vec2 targetWorld);
    const Enemy* findCaptureTarget(Vec2 targetWorld) const;
    Enemy* findCaptureTargetInDirection(Vec2 origin, Vec2 direction);
    const Enemy* findCaptureTargetInDirection(Vec2 origin, Vec2 direction) const;
    CaptureTargetPreview previewCaptureTarget(
        const Enemy* target,
        const Player& player,
        bool allowBossCapture,
        std::string_view bossCaptureObjectId) const;
    CaptureResult tryCaptureTarget(
        Enemy* target,
        Player& player,
        SpellRingSystem& spellRing,
        InventorySystem& inventory,
        bool allowBossCapture,
        std::string_view bossCaptureObjectId,
        const CaptureAttemptOptions& options = {});
    Enemy* findActiveEnemyNear(Vec2 position, float radius);
    Enemy* findRuntimeEnemy(int runtimeId);
    const Enemy* findRuntimeEnemy(int runtimeId) const;

    void setAwareness(Enemy& enemy, EnemyAwarenessState nextState, bool showIcon);
    void forceDetectInSight(Enemy& enemy, Vec2 playerPosition, bool showIcon);
    void wakeDungeonEventEnemy(Enemy& enemy, Vec2 playerPosition, bool showIcon);
    const EnemyDefinition* chooseEnemyDefinition(const EnemyCatalog& enemyCatalog);
    const EnemyDefinition* chooseNormalRandomEnemyDefinition(const EnemyCatalog& enemyCatalog);
    EnemySpawnSelection chooseDugSpawnEnemy(const EnemyCatalog& enemyCatalog, std::string_view stageId, int depthRank);
    const EnemyDefinition* chooseDugSpawnEnemyDefinition(const EnemyCatalog& enemyCatalog, std::string_view stageId, int depthRank);
    double spawnBiasMultiplierFor(const EnemyDefinition& definition) const;
    void logSpawnWeightFallbackOnce(std::string key, std::string message);
    void applyDefinition(Enemy& enemy, const EnemyDefinition* definition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog);
    bool queueSwarmSpawn(
        Enemy& enemy,
        Vec2 detectedTarget,
        std::vector<SwarmSpawnRequest>& outRequests);
    int processSwarmSpawnRequest(
        const SwarmSpawnRequest& request,
        TileMap& map,
        const RuntimeBalance& balance,
        const EnemyCatalog& enemyCatalog);
    bool spawnDefinitionAt(
        Vec2 position,
        const EnemyDefinition* definition,
        const RuntimeBalance& balance,
        const EnemyCatalog& enemyCatalog,
        bool detectedOnSpawn = false,
        Vec2 detectedTarget = {},
        float spawnWarmupOverride = -1.0f,
        int* outRuntimeId = nullptr,
        std::string_view lootStageId = {},
        int lootDepthRank = 1,
        EnemyVariantTier variantTier = EnemyVariantTier::Normal,
        int effectiveBaseLevel = 0,
        EnemySpawnSource spawnSource = EnemySpawnSource::Ambient,
        bool screenSleepAllowed = true);
    void spawnAt(Vec2 position, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn = false, Vec2 detectedTarget = {});
    bool spawnBossAt(
        Vec2 position,
        const RuntimeBalance& balance,
        const EnemyCatalog& enemyCatalog,
        std::string_view bossEnemyId = {},
        bool detectedOnSpawn = false,
        Vec2 detectedTarget = {},
        EnemyVariantTier variantTier = EnemyVariantTier::Normal,
        int effectiveBaseLevel = 0,
        EnemySpawnVisualKind spawnVisualKind = EnemySpawnVisualKind::Default);
    bool findSpawnPosition(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, Vec2& outPosition) const;
    bool findSpawnPosition(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, float radius, float minPlayerDistance, Vec2& outPosition) const;
    bool findBossSpawnPosition(TileMap& map, Vec2 playerPosition, const RuntimeBalance& balance, Vec2& outPosition) const;
    bool updateBossActionSequence(Enemy& enemy, Player& player, TileMap& map, ProjectileSystem& projectiles, float dt);
    void rebuildFlowField(TileMap& map, Vec2 playerPosition);
    Vec2 flowDirectionFor(TileMap& map, Vec2 enemyPosition, Vec2 playerPosition) const;
    Vec2 updateFleeDirection(TileMap& map, Enemy& enemy, Vec2 playerPosition, float dt);
    bool planFleeWaypoint(TileMap& map, const Enemy& enemy, Vec2 playerPosition, Vec2& outWaypoint) const;
    void updateFleeProgress(Enemy& enemy, Vec2 actualMovement, float expectedDistance, float dt);
    Vec2 separationFor(const Enemy& enemy) const;
    void moveWithCollision(Enemy& enemy, TileMap& map, Vec2 desiredVelocity, float dt);
    bool resolvePlayerOverlap(Player& player, Enemy& enemy, TileMap& map, const RuntimeBalance& balance);
    void beginEnemyDeath(
        Enemy& enemy,
        SpellRingSystem& spellRing,
        std::optional<Vec2> hitOrigin = std::nullopt,
        bool suppressRewards = false);
    void updateEnemyDeath(Enemy& enemy, TileMap& map, SpellRingSystem& spellRing, float dt);
    void finishEnemyDeath(Enemy& enemy, SpellRingSystem& spellRing);
    int applyConductiveShock(Vec2 position, float radius, double value, double duration, int excludedEnemyId, std::string_view source);

    ObjectPool<Enemy, balance::MaxEnemies> enemies_;
    const HitboxCatalog* hitboxCatalog_ = nullptr;
    const EnemyPlacementCatalog* placementCatalog_ = nullptr;
    const EnemyShadowCatalog* shadowCatalog_ = nullptr;
    std::vector<EnemyEvent> events_;
    std::vector<RingImpactSoundEvent> impactSoundEvents_;
    std::vector<EnemySoundEvent> soundEvents_;
    std::vector<CaptureResult> captureResults_;
    std::vector<StatusPopupEvent> statusPopupEvents_;
    int pendingXp_ = 0;
    int dugSpawnCounter_ = 0;
    int nextEnemyId_ = 1;
    std::mt19937 rng_{std::random_device{}()};
    std::unordered_set<std::string> loggedUnknownAi_;
    std::unordered_set<std::string> loggedUnknownUnawareAi_;
    std::unordered_set<std::string> loggedUnsupportedBehavior_;
    std::unordered_set<std::string> loggedSpawnWeightFallbacks_;
    std::unordered_map<std::string, double> spawnBiasMultipliers_;
    std::unordered_map<std::string, std::vector<RoguelikeEnemyPoolEntry>> roguelikeEnemyPools_;
    std::vector<Enemy> dormantEnemies_;
    int flowMinX_ = 0;
    int flowMinY_ = 0;
    int flowWidth_ = 0;
    int flowHeight_ = 0;
    float flowTimer_ = 0.0f;
    std::vector<int> flowDistance_;
    std::vector<MudZone> mudZones_;
    std::vector<EnemyWindPulse> windPulses_;
    double mudDamageAccumulator_ = 0.0;
};

}
