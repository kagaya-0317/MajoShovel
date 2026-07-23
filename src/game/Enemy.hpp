#pragma once

#include "engine/Math.hpp"
#include "data/EnemyCatalog.hpp"
#include "game/EntityStatus.hpp"
#include "game/ItemModel.hpp"
#include "game/Chunk.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

enum class EnemyVariantTier {
    Normal,
    Deep,
    Abyss,
};

enum class EnemySpawnSource {
    Ambient,
    Event,
    Boss,
};

enum class EnemySpawnVisualKind {
    Default,
    GroundEmerge,
    WalkIn,
};

inline constexpr int EnemyDeepVariantLevelBonus = 35;
inline constexpr int EnemyAbyssVariantLevelBonus = 70;

inline int enemyVariantLevelBonus(EnemyVariantTier tier)
{
    switch (tier) {
    case EnemyVariantTier::Deep:
        return EnemyDeepVariantLevelBonus;
    case EnemyVariantTier::Abyss:
        return EnemyAbyssVariantLevelBonus;
    case EnemyVariantTier::Normal:
        break;
    }
    return 0;
}

inline std::string_view enemyVariantNamePrefix(EnemyVariantTier tier)
{
    switch (tier) {
    case EnemyVariantTier::Deep:
        return "深層の";
    case EnemyVariantTier::Abyss:
        return "奈落の";
    case EnemyVariantTier::Normal:
        break;
    }
    return {};
}

inline std::string_view enemyVariantObjectIdSegment(EnemyVariantTier tier)
{
    switch (tier) {
    case EnemyVariantTier::Deep:
        return "deep_";
    case EnemyVariantTier::Abyss:
        return "abyss_";
    case EnemyVariantTier::Normal:
        break;
    }
    return {};
}

inline std::string enemyVariantDisplayName(std::string_view baseName, EnemyVariantTier tier)
{
    std::string name(enemyVariantNamePrefix(tier));
    name += baseName;
    return name;
}

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
    Approach,
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
    DebrisVolleyWindup,
    DebrisVolley,
};

enum class JunkCrabAttackAnimationKind {
    None,
    Claw,
    DebrisVolley,
};

inline constexpr int JunkCrabAttackFrameRate = 60;
inline constexpr int JunkCrabAttackFrameCount = 6;
inline constexpr std::array<int, JunkCrabAttackFrameCount> JunkCrabAttackFrameDurationsFrames{{
    9,
    18,
    54,
    6,
    6,
    80,
}};
inline constexpr std::array<float, JunkCrabAttackFrameCount> JunkCrabAttackForwardOffsetsPx{{
    -5.0f,
    -8.0f,
    -9.0f,
    48.0f,
    96.0f,
    108.0f,
}};
inline constexpr int JunkCrabAttackImpactFrames =
    JunkCrabAttackFrameDurationsFrames[0] +
    JunkCrabAttackFrameDurationsFrames[1] +
    JunkCrabAttackFrameDurationsFrames[2];
inline constexpr int JunkCrabAttackChargeStartFrames =
    JunkCrabAttackFrameDurationsFrames[0] +
    JunkCrabAttackFrameDurationsFrames[1];
inline constexpr int JunkCrabAttackTotalFrames =
    JunkCrabAttackImpactFrames +
    JunkCrabAttackFrameDurationsFrames[3] +
    JunkCrabAttackFrameDurationsFrames[4] +
    JunkCrabAttackFrameDurationsFrames[5];
inline constexpr float JunkCrabAttackImpactSeconds =
    static_cast<float>(JunkCrabAttackImpactFrames) / static_cast<float>(JunkCrabAttackFrameRate);
inline constexpr float JunkCrabAttackFollowThroughSeconds =
    static_cast<float>(JunkCrabAttackTotalFrames - JunkCrabAttackImpactFrames) /
    static_cast<float>(JunkCrabAttackFrameRate);
inline constexpr float JunkCrabAttackTotalSeconds =
    static_cast<float>(JunkCrabAttackTotalFrames) / static_cast<float>(JunkCrabAttackFrameRate);
inline constexpr float JunkCrabAttackChargeStartSeconds =
    static_cast<float>(JunkCrabAttackChargeStartFrames) / static_cast<float>(JunkCrabAttackFrameRate);
inline constexpr int JunkCrabAttackChargeFrameIndex = 2;
inline constexpr int JunkCrabAttackChargeShakeIntervalFrames = 4;
inline constexpr float JunkCrabAttackChargeShakePixels = 1.0f;
inline constexpr int JunkCrabDebrisVolleyAttackFrameDurationFrames = 12;
inline constexpr float JunkCrabDebrisVolleyAttackFrameDurationSeconds =
    static_cast<float>(JunkCrabDebrisVolleyAttackFrameDurationFrames) /
    static_cast<float>(JunkCrabAttackFrameRate);
inline constexpr float JunkCrabDebrisVolleyWindupSeconds =
    JunkCrabDebrisVolleyAttackFrameDurationSeconds * static_cast<float>(JunkCrabAttackFrameCount);

struct JunkCrabAttackFrameSample {
    int frameIndex = 0;
    float frameElapsedSeconds = 0.0f;
};

[[nodiscard]] inline JunkCrabAttackFrameSample sampleJunkCrabAttackFrame(float animationTimeSeconds)
{
    float remaining = std::max(0.0f, animationTimeSeconds);
    for (int i = 0; i < JunkCrabAttackFrameCount; ++i) {
        const float duration =
            static_cast<float>(JunkCrabAttackFrameDurationsFrames[static_cast<std::size_t>(i)]) /
            static_cast<float>(JunkCrabAttackFrameRate);
        if (remaining < duration) {
            return {
                .frameIndex = i,
                .frameElapsedSeconds = remaining,
            };
        }
        remaining -= duration;
    }

    return {
        .frameIndex = JunkCrabAttackFrameCount - 1,
        .frameElapsedSeconds =
            static_cast<float>(JunkCrabAttackFrameDurationsFrames[JunkCrabAttackFrameCount - 1]) /
            static_cast<float>(JunkCrabAttackFrameRate),
    };
}

[[nodiscard]] inline float junkCrabAttackForwardOffsetPx(float animationTimeSeconds)
{
    const JunkCrabAttackFrameSample sample = sampleJunkCrabAttackFrame(animationTimeSeconds);
    return JunkCrabAttackForwardOffsetsPx[static_cast<std::size_t>(sample.frameIndex)];
}

[[nodiscard]] inline float junkCrabAttackChargeShakeOffsetPx(float animationTimeSeconds)
{
    const JunkCrabAttackFrameSample sample = sampleJunkCrabAttackFrame(animationTimeSeconds);
    if (sample.frameIndex != JunkCrabAttackChargeFrameIndex) {
        return 0.0f;
    }

    const float shakeStepSeconds =
        static_cast<float>(JunkCrabAttackChargeShakeIntervalFrames) /
        static_cast<float>(JunkCrabAttackFrameRate);
    const int shakeStep = static_cast<int>(std::floor(sample.frameElapsedSeconds / shakeStepSeconds));
    return shakeStep % 2 == 0 ? -JunkCrabAttackChargeShakePixels : JunkCrabAttackChargeShakePixels;
}

[[nodiscard]] inline JunkCrabAttackAnimationKind junkCrabAttackAnimationKind(JunkCrabPhase phase)
{
    switch (phase) {
    case JunkCrabPhase::ClawWindup:
    case JunkCrabPhase::ClawStrike:
        return JunkCrabAttackAnimationKind::Claw;
    case JunkCrabPhase::DebrisVolleyWindup:
        return JunkCrabAttackAnimationKind::DebrisVolley;
    case JunkCrabPhase::None:
    case JunkCrabPhase::RingGuard:
    case JunkCrabPhase::DebrisVolley:
        break;
    }
    return JunkCrabAttackAnimationKind::None;
}

[[nodiscard]] inline bool junkCrabPhaseUsesAttackAnimation(JunkCrabPhase phase)
{
    return junkCrabAttackAnimationKind(phase) != JunkCrabAttackAnimationKind::None;
}

[[nodiscard]] inline bool junkCrabAttackLocksFacing(JunkCrabPhase phase, float attackAnimationSeconds)
{
    return junkCrabAttackAnimationKind(phase) == JunkCrabAttackAnimationKind::Claw &&
        attackAnimationSeconds >= JunkCrabAttackChargeStartSeconds;
}

enum class JunkCrabDebrisState {
    Inactive,
    Orbiting,
    ReturningToBoss,
    BouncingAway,
    Fading,
    VolleyProjectile,
};

enum class AstragnaPhase {
    None,
    Sealed,
    Downed,
    Rescued,
};

enum class AstragnaEmitterAttack {
    None,
    LaserBolt,
    FlameSweep,
};

enum class AstragnaEmitterPhase {
    Dormant,
    Telegraph,
    Active,
    Cooldown,
    Destroyed,
};

inline constexpr int JunkCrabMaxDebris = 12;
inline constexpr int AstragnaSealPartCount = 5;
inline constexpr int AstragnaMaxShellBlocks = 256;

struct JunkCrabDebrisRuntime {
    JunkCrabDebrisState state = JunkCrabDebrisState::Inactive;
    float angle = 0.0f;
    float radius = 0.0f;
    float orbitAngularSpeed = 0.0f;
    Vec2 position{};
    Vec2 velocity{};
    float visualAngle = 0.0f;
    float visualAngularSpeed = 0.0f;
    float timer = 0.0f;
    float lifetime = 0.0f;
    float altitude = 0.0f;
    float verticalVelocity = 0.0f;
    float alpha = 1.0f;
    int hp = 0;
    int maxHp = 0;
    int iconIndex = 0;
};

struct JunkCrabBossRuntime {
    JunkCrabPhase phase = JunkCrabPhase::None;
    float timer = 0.0f;
    float attackAnimationSeconds = 0.0f;
    float guardRespawnTimer = 0.0f;
    float guardRespawnDelaySeconds = 0.0f;
    float debrisVolleyTimer = 0.0f;
    float debrisVolleyLaunchTimer = 0.0f;
    bool actionFired = false;
    Vec2 appliedAttackMotionOffset{};
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

struct AstragnaSealEmitterRuntime {
    bool active = false;
    bool destroyed = false;
    int sealIndex = -1;
    AstragnaEmitterAttack attack = AstragnaEmitterAttack::None;
    AstragnaEmitterPhase phase = AstragnaEmitterPhase::Dormant;
    Vec2 position{};
    float localAngle = 0.0f;
    float orbitRadius = 0.0f;
    float radius = 0.0f;
    float timer = 0.0f;
    float shotTimer = 0.0f;
    float hitCooldown = 0.0f;
    float baseDirectionAngle = 0.0f;
    float sweepSign = 1.0f;
    int shotsFired = 0;
    int hp = 0;
    int maxHp = 0;
};

struct AstragnaShellBlockRuntime {
    bool active = false;
    bool repairing = false;
    int layerIndex = 0;
    int segmentIndex = 0;
    int segmentCount = 0;
    TileType tileType = TileType::HardRock;
    float localAngle = 0.0f;
    float angularSpan = 0.0f;
    float orbitRadius = 0.0f;
    float radius = 0.0f;
    float innerRadius = 0.0f;
    float outerRadius = 0.0f;
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
    int shellBlockCount = 0;
    bool initialized = false;
    bool rescueEventEmitted = false;
    std::array<AstragnaSealPartRuntime, AstragnaSealPartCount> sealParts{};
    std::array<AstragnaSealEmitterRuntime, AstragnaSealPartCount> sealEmitters{};
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
    bool previewOnly = false;
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
    EnemySpawnSource spawnSource = EnemySpawnSource::Ambient;
    bool screenSleepAllowed = false;
    bool dungeonEventBoss = false;
    bool dungeonEventSleeping = false;
    int id = 0;
    std::string dungeonEventId;
    std::string enemyId;
    std::string enemyName;
    const EnemyDefinition* definition = nullptr;
    EnemyVariantTier variantTier = EnemyVariantTier::Normal;
    int effectiveBaseLevel = 1;
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
    float chestBiteIntervalSeconds = 0.0f;
    float chestBiteTimer = 0.0f;
    float chestBiteTriggerRange = 0.0f;
    float chestBiteJumpDistance = 0.0f;
    float chestBiteJumpDurationSeconds = 0.0f;
    float chestBiteJumpArcHeight = 0.0f;
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
    EnemySpawnVisualKind spawnVisualKind = EnemySpawnVisualKind::Default;
    Vec2 spawnPresentationStartPosition{};
    Vec2 spawnPresentationEndPosition{};
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
