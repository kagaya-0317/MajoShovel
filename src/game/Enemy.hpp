#pragma once

#include "engine/Math.hpp"
#include "data/EnemyCatalog.hpp"
#include "game/EntityStatus.hpp"
#include "game/ItemModel.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace majo {

enum class EnemyAwarenessState {
    Unaware,
    Detected,
};

enum class EnemyAwarenessIcon {
    None,
    Exclamation,
    Question,
};

enum class BossActionPhase {
    None,
    Submerge,
    Telegraph,
    Jump,
    LandingDelay,
    Charge,
    Stun,
    Recover,
};

enum class JunkCrabPhase {
    None,
    RingGuard,
    ClawWindup,
    ClawStrike,
    ThrowWindup,
    ThrowBurst,
    Toppled,
    Recover,
};

enum class AstragnaPhase {
    None,
    Sealed,
    Downed,
    Rescued,
};

inline constexpr int JunkCrabMaxDebris = 6;
inline constexpr int AstragnaSealPartCount = 5;
inline constexpr int AstragnaMaxShellBlocks = 36;

struct JunkCrabDebrisRuntime {
    bool active = false;
    float baseAngle = 0.0f;
    float radius = 0.0f;
    int hp = 0;
};

struct JunkCrabBossRuntime {
    JunkCrabPhase phase = JunkCrabPhase::None;
    float timer = 0.0f;
    float orbitAngle = 0.0f;
    int toppleMeter = 0;
    int throwBurstIndex = 0;
    bool actionFired = false;
    std::array<JunkCrabDebrisRuntime, JunkCrabMaxDebris> debris{};
};

struct AstragnaSealPartRuntime {
    bool active = false;
    float localAngle = 0.0f;
    float orbitRadius = 0.0f;
    float radius = 0.0f;
    int hp = 0;
    int maxHp = 0;
};

struct AstragnaShellBlockRuntime {
    bool active = false;
    bool repairing = false;
    float localAngle = 0.0f;
    float orbitRadius = 0.0f;
    float radius = 0.0f;
    int hp = 0;
    int maxHp = 0;
};

struct AstragnaBossRuntime {
    AstragnaPhase phase = AstragnaPhase::None;
    float timer = 0.0f;
    float rotationAngle = 0.0f;
    float repairTimer = 0.0f;
    int reviveCount = 0;
    int repairCursor = 0;
    bool initialized = false;
    std::array<AstragnaSealPartRuntime, AstragnaSealPartCount> sealParts{};
    std::array<AstragnaShellBlockRuntime, AstragnaMaxShellBlocks> shellBlocks{};
};

struct BossActionRuntime {
    bool enabled = false;
    std::string pattern;
    BossActionPhase phase = BossActionPhase::None;
    float timer = 0.0f;
    float phaseDuration = 0.0f;
    Vec2 targetPosition{};
    Vec2 chargeDirection{1.0f, 0.0f};
    bool hidden = false;
    bool invulnerable = false;
    JunkCrabBossRuntime junkCrab;
    AstragnaBossRuntime astragna;
};

struct EnemyActionRuntime {
    bool active = false;
    std::string behaviorId;
    std::string animationId;
    float elapsedSeconds = 0.0f;
    float durationSeconds = 0.0f;
    float fireAtSeconds = 0.0f;
    bool fired = false;
    bool lockMovement = false;
    bool lockFacing = true;
};

inline constexpr int EnemyHeldDropCapacity = 3;

enum class EnemyHeldDropKind {
    Object,
    Money,
};

enum class EnemyHeldDropOrigin {
    Initial,
    PickedUp,
};

struct EnemyHeldDrop {
    EnemyHeldDropKind kind = EnemyHeldDropKind::Object;
    EnemyHeldDropOrigin origin = EnemyHeldDropOrigin::Initial;
    std::string objectId;
    int quantity = 1;
    float deathDropChance = 0.0f;
    std::optional<ItemInstance> instance;
};

struct EnemyDeathRuntime {
    bool active = false;
    float elapsedSeconds = 0.0f;
    float durationSeconds = 0.0f;
    Vec2 knockbackVelocity{};
    float knockbackTimer = 0.0f;
    unsigned int shakeSeed = 0;
    bool suppressRewards = false;
};

struct Enemy {
    bool active = false;
    bool isBoss = false;
    bool dungeonEventBoss = false;
    bool dungeonEventSleeping = false;
    int id = 0;
    std::string dungeonEventId;
    std::string enemyId;
    std::string enemyName;
    const EnemyDefinition* definition = nullptr;
    std::string behaviorId;
    std::vector<std::string> behaviorIds;
    std::string projectileId;
    std::string rangedBehaviorId;
    float projectileInterval = 0.0f;
    float projectileSpeedMultiplier = 1.0f;
    int projectileDamageOverride = -1;
    float projectileRadiusScale = 1.0f;
    int projectileBurstCount = 1;
    int projectileBurstRemaining = 0;
    float projectileBurstInterval = 0.12f;
    int fireVolleyCount = 1;
    float fireSpreadDegrees = 8.0f;
    std::vector<EffectSpec> projectileEffects;
    std::string aiId;
    std::string unawareAiId;
    float behaviorTimer = 0.0f;
    float projectileTimer = 0.0f;
    std::vector<std::string> enemyTags;
    Vec2 position{};
    Vec2 velocity{};
    float radius = 10.0f;
    int hp = 5;
    int maxHp = 5;
    int xp = 5;
    int moneyDrop = 0;
    std::vector<EnemyHeldDrop> heldDrops;
    bool heldDropsInitialized = false;
    std::string lootStageId;
    int lootDepthRank = 1;
    int contactAttackPower = 1;
    std::string contactDamageType = "blunt";
    float contactTimer = 0.0f;
    float contactDamageMultiplier = 1.0f;
    EnemyActionRuntime action;
    BossActionRuntime bossAction;
    float frontGuardArcDegrees = 140.0f;
    float frontGuardDamageMultiplier = 0.35f;
    float physicalDamageMultiplier = 0.55f;
    float magicBodyPhysicalMultiplier = 0.35f;
    float magicBodyMagicMultiplier = 1.0f;
    float ringSlowMultiplier = -1.0f;
    float ringSlowDurationSeconds = -1.0f;
    int digMovePower = 1;
    float digMoveIntervalSeconds = 0.11f;
    float enemyHealRadius = 0.0f;
    float enemyHealAmount = 0.0f;
    float enemyHealIntervalSeconds = 0.0f;
    float enemyHealTimer = 0.0f;
    float countdownExplodeRadius = 0.0f;
    float countdownExplodeDelay = 0.0f;
    float countdownExplodeInitialDelay = 0.0f;
    float countdownExplodeArmDistance = 0.0f;
    int countdownExplodeDamage = 0;
    int countdownExplodeTerrainDamage = 0;
    int countdownExplodeWarningTickIndex = -1;
    bool countdownExplodeArmed = false;
    bool countdownExplodeOnce = false;
    bool countdownExploded = false;
    float jumpAttackDistance = 0.0f;
    float jumpLandingRadius = 0.0f;
    float jumpLandingDamageMultiplier = 1.0f;
    float jumpAttackIntervalSeconds = 0.0f;
    float jumpAttackTimer = 0.0f;
    float jumpAttackDurationSeconds = 0.28f;
    float jumpAttackArcHeight = 24.0f;
    float jumpLandingBuffTimer = 0.0f;
    bool jumpActive = false;
    Vec2 jumpStartPosition{};
    Vec2 jumpTargetPosition{};
    float jumpElapsedSeconds = 0.0f;
    float jumpDurationSeconds = 0.0f;
    float jumpArcHeight = 0.0f;
    bool externalBounceActive = false;
    float externalBounceFallDamage = 0.0f;
    float externalBounceFallDamageMultiplier = 1.0f;
    float altitude = 0.0f;
    float hoverAltitude = 0.0f;
    float hoverBobAmplitude = 0.0f;
    float hoverBobSpeed = 0.0f;
    float lightSpeedMultiplier = 1.0f;
    float magnetRadius = 0.0f;
    float magnetStrength = 0.0f;
    std::string magnetTargetTag;
    float rustDefenseMultiplier = 1.0f;
    float rustDurationSeconds = 0.0f;
    std::string rustTargetTag;
    float chestBiteKnockback = 0.0f;
    bool swarmSpawnEnabled = false;
    bool swarmSpawnExecuted = false;
    int swarmSpawnCount = 0;
    float swarmSpawnRadius = 0.0f;
    bool dropItemEnabled = false;
    std::string dropItemProfile;
    float dropItemChance = 0.0f;
    int dropItemCount = 0;
    float dropItemScatterRadius = 0.0f;
    bool dropItemConsumed = false;
    bool dropMaterialEnabled = false;
    MaterialType dropMaterialType = MaterialType::Count;
    float dropMaterialChance = 0.0f;
    int dropMaterialMin = 0;
    int dropMaterialMax = 0;
    float dropMaterialScatterRadius = 0.0f;
    bool dropMaterialConsumed = false;
    bool stealItemEnabled = false;
    std::string stealTarget;
    float stealRadius = 0.0f;
    float stealEscapeDistance = 0.0f;
    int stealMaxCarry = 0;
    EnemyDeathRuntime death;
    float hitFlash = 0.0f;
    float hpBarTimer = 0.0f;
    float facingAngle = 0.0f;
    EnemyAwarenessState awareness = EnemyAwarenessState::Unaware;
    bool manualDetectionOnly = false;
    float loseSightTimer = 0.0f;
    float visionDistance = 120.0f;
    float visionAngle = 100.0f;
    float loseSightSeconds = 1.5f;
    EnemyAwarenessIcon awarenessIcon = EnemyAwarenessIcon::None;
    float awarenessIconTimer = 0.0f;
    Vec2 aiMoveDirection{1.0f, 0.0f};
    Vec2 patrolAnchor{};
    bool patrolAnchorInitialized = false;
    bool movementLeashEnabled = false;
    Vec2 movementLeashCenter{};
    float movementLeashRadius = 0.0f;
    float aiDecisionTimer = 0.0f;
    float aiDigTimer = 0.0f;
    float repathTimer = 0.0f;
    float spawnTimer = 0.0f;
    float spawnDuration = 0.0f;
    Vec2 knockbackVelocity{};
    float knockbackTimer = 0.0f;
    float stunWakeTimer = 0.0f;
    double poisonDamageAccumulator = 0.0;
    double hotDamageAccumulator = 0.0;
    double bleedDamageAccumulator = 0.0;
    float coldExposure = 0.0f;
    bool coldExposureTouched = false;
    EntityStatus status;
};

}
