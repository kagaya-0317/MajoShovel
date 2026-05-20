#pragma once

#include "engine/Math.hpp"
#include "data/EnemyCatalog.hpp"
#include "game/EntityStatus.hpp"

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

struct Enemy {
    bool active = false;
    bool isBoss = false;
    int id = 0;
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
    int contactAttackPower = 1;
    std::string contactDamageType = "blunt";
    float contactTimer = 0.0f;
    float contactDamageMultiplier = 1.0f;
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
    int countdownExplodeDamage = 0;
    int countdownExplodeTerrainDamage = 0;
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
    bool stealItemEnabled = false;
    std::string stealTarget;
    float stealRadius = 0.0f;
    float stealEscapeDistance = 0.0f;
    int stealMaxCarry = 0;
    int stolenMoney = 0;
    std::vector<std::string> stolenObjectIds;
    bool pendingDeath = false;
    float hitFlash = 0.0f;
    float hpBarTimer = 0.0f;
    float facingAngle = 0.0f;
    EnemyAwarenessState awareness = EnemyAwarenessState::Unaware;
    float loseSightTimer = 0.0f;
    float visionDistance = 120.0f;
    float visionAngle = 100.0f;
    float loseSightSeconds = 1.5f;
    EnemyAwarenessIcon awarenessIcon = EnemyAwarenessIcon::None;
    float awarenessIconTimer = 0.0f;
    Vec2 aiMoveDirection{1.0f, 0.0f};
    Vec2 patrolAnchor{};
    bool patrolAnchorInitialized = false;
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
