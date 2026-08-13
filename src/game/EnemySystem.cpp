#include "game/EnemySystem.hpp"

#include "data/GameBalance.hpp"
#include "engine/Log.hpp"
#include "engine/Ui.hpp"
#include "game/ActorVisualMotion.hpp"
#include "game/ActorVisual.hpp"
#include "game/Collision.hpp"
#include "game/EnemyImageRenderer.hpp"
#include "game/EnemyFacing.hpp"
#include "game/EncyclopediaSystem.hpp"
#include "game/EntityStatusVisuals.hpp"
#include "game/ExplosionWarning.hpp"
#include "game/EffectSystem.hpp"
#include "game/InventoryUiCommon.hpp"
#include "game/ItemImageRenderer.hpp"
#include "game/ObjectVisualPose.hpp"
#include "game/RingItemVisual.hpp"
#include "game/TerrainDigRules.hpp"
#include "game/WorldDropSystem.hpp"
#include "game/WorldIconRenderer.hpp"
#include "game/WetGroundSystem.hpp"
#include "data/StageWeight.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <sstream>
#include <utility>
#include <string_view>

namespace majo {

namespace {

constexpr int FlowRadiusTiles = 80;
constexpr float SpawnAvoidancePadding = 5.0f;
constexpr std::string_view AudioSeEnemySpawn = "se.enemy.spawn";
constexpr std::string_view AudioSeRatSteal = "se.enemy.rat_steal";
constexpr float PlayerPushShare = 0.35f;
constexpr float EnemyPushShare = 0.65f;
constexpr float BossRadiusMultiplier = 1.0f;
constexpr float BossVisualRadiusMultiplier = 1.35f;
constexpr double BossNormalIncomingDamageMultiplier = 0.75;
constexpr double BossWeakPointIncomingDamageMultiplier = 1.5;
constexpr float BossMinSpawnDistance = 96.0f;
constexpr std::string_view StardustMoleEnemyId = "stardust_mole";
constexpr std::string_view StardustMolePatternId = "stardust_mole";
constexpr std::string_view JunkCrabEnemyId = "junk_crab";
constexpr std::string_view JunkCrabPatternId = "junk_crab";
constexpr std::string_view AstragnaEnemyId = "astragna";
constexpr std::string_view AstragnaPatternId = "astragna";
constexpr std::string_view StarVeinDragonEnemyId = "star_vein_dragon";
constexpr std::string_view BombTsuchinokoEnemyId = "bomb_tsuchinoko";
constexpr float CountdownExplodeDefaultDelaySeconds = 4.0f;
constexpr float CountdownExplodeFallbackArmDistance = 120.0f;
constexpr float StardustMoleApproachMinSeconds = 1.50f;
constexpr float StardustMoleApproachMaxSeconds = 3.00f;
constexpr float StardustMoleApproachSpeed = 21.0f;
constexpr float StardustMoleApproachStopDistance = 44.0f;
constexpr float StardustMoleDiveJumpSeconds = 0.28f;
constexpr float StardustMoleDiveJumpHeight = 28.0f;
constexpr float StardustMoleUndergroundMinSeconds = 0.90f;
constexpr float StardustMoleUndergroundMaxSeconds = 1.80f;
constexpr float StardustMoleTelegraphSeconds = 0.65f;
constexpr float StardustMoleJumpSeconds = 0.55f;
constexpr float StardustMoleJumpHeight = 82.0f;
constexpr float StardustMoleLandingDelaySeconds = 0.50f;
constexpr float StardustMoleChargeSeconds = 2.40f;
constexpr float StardustMoleChargeSpeed = 330.0f;
constexpr float StardustMoleJumpContactDamageMultiplier = 1.5f;
constexpr float StardustMoleChargeContactDamageMultiplier = 2.0f;
constexpr float StardustMoleStunSeconds = 2.20f;
constexpr float StardustMoleRecoverSeconds = 0.30f;
constexpr float StardustMoleEmergeDistance = 124.0f;
constexpr float StardustMoleEmergeMinPlayerDistance = 78.0f;
constexpr int BossChargeTerrainDamage = 10000;
constexpr float JunkCrabOrbitAngularSpeed = 2.35f;
constexpr float JunkCrabOrbitRadiusMultiplier = 1.85f;
constexpr float JunkCrabDebrisRadius = 8.5f;
constexpr float JunkCrabDebrisImageMaxSize = 30.0f;
constexpr int JunkCrabDebrisMinCount = 8;
constexpr int JunkCrabDebrisHp = 40;
constexpr float JunkCrabGuardRespawnMinSeconds = 300.0f / 60.0f;
constexpr float JunkCrabGuardRespawnMaxSeconds = 420.0f / 60.0f;
constexpr float JunkCrabGuardMoveSpeed = 18.0f;
constexpr float JunkCrabClawRange = 62.0f;
constexpr float JunkCrabClawWindupSeconds = JunkCrabAttackImpactSeconds;
constexpr float JunkCrabClawStrikeSeconds = JunkCrabAttackFollowThroughSeconds;
constexpr float JunkCrabClawArcDegrees = 115.0f;
constexpr float JunkCrabDebrisSpinMinDegrees = 60.0f;
constexpr float JunkCrabDebrisSpinMaxDegrees = 120.0f;
constexpr float JunkCrabDebrisVolleyDelaySeconds = 12.0f;
constexpr float JunkCrabDebrisVolleyLaunchIntervalSeconds = 12.0f / 60.0f;
constexpr float JunkCrabDebrisVolleySpeed = 380.0f;
constexpr float JunkCrabDebrisVolleyLifetimeSeconds = 2.60f;
constexpr float JunkCrabDebrisReturnSpeed = 430.0f;
constexpr float JunkCrabDebrisReturnHitDistance = 16.0f;
constexpr float JunkCrabDebrisBounceMinSpeed = 170.0f;
constexpr float JunkCrabDebrisBounceMaxSpeed = 240.0f;
constexpr float JunkCrabDebrisBounceGravity = 520.0f;
constexpr float JunkCrabDebrisFadeSeconds = 0.55f;
constexpr std::array<WorldIconId, 4> JunkCrabDebrisIconCycle{{
    WorldIconId::JunkCrabDebrisCan,
    WorldIconId::JunkCrabDebrisGear,
    WorldIconId::JunkCrabDebrisBattery,
    WorldIconId::JunkCrabDebrisPipe,
}};
constexpr float AstragnaRotationSpeed = 0.08f;
constexpr float AstragnaCoreDiameterTiles = 10.0f;
constexpr float AstragnaShellThicknessTiles = 5.0f;
constexpr float AstragnaShellGapTiles = 0.0f;
constexpr float AstragnaSealOrbitGapTiles = 1.0f;
constexpr float AstragnaSealRadius = 15.0f;
constexpr int AstragnaSealMaxHp = 18;
constexpr float AstragnaShellHitRadius = static_cast<float>(balance::TileSize) * 0.56f;
constexpr int AstragnaShellMaxHp = 7;

float smoothStep01(float value)
{
    const float t = clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
constexpr float AstragnaSealedShellDamageMultiplier = 0.08f;
constexpr float AstragnaDownedShellDamageMultiplier = 1.0f;
constexpr float AstragnaDownedSeconds = 7.0f;
constexpr float AstragnaRepairIntervalSeconds = 1.35f;
constexpr float AstragnaRepairPlayerSafeRadius = 72.0f;
constexpr float AstragnaPlayerPushPadding = 1.5f;
constexpr std::string_view AstragnaLaserProjectileId = "astragna_laser_bolt";
constexpr std::string_view AstragnaGuardianStarImagePath = "assets/enemies/astragna_guardian_star.png";
constexpr std::string_view AstragnaWallOrnamentImagePath = "assets/enemies/astragna_wall_ornament.png";
constexpr std::string_view AstragnaDroneImagePath = "assets/enemies/astragna_drone.png";
constexpr float AstragnaWallOrnamentSourceOuterRadiusUv = 0.49f;
constexpr float AstragnaEmitterSpawnRangeTiles = 6.5f;
constexpr float AstragnaEmitterOrbitGapTiles = 1.25f;
constexpr float AstragnaEmitterAngleOffsetRadians = 22.0f * Pi / 180.0f;
constexpr float AstragnaEmitterRadius = 13.0f;
constexpr int AstragnaEmitterMaxHp = 7;
constexpr int AstragnaMaxActiveEmitters = 2;
constexpr int AstragnaMaxActiveFlameEmitters = 1;
constexpr float AstragnaEmitterLaserTelegraphSeconds = 0.45f;
constexpr float AstragnaEmitterLaserActiveSeconds = 0.34f;
constexpr float AstragnaEmitterLaserCooldownSeconds = 1.45f;
constexpr float AstragnaEmitterLaserShotIntervalSeconds = 0.14f;
constexpr int AstragnaEmitterLaserShotCount = 2;
constexpr float AstragnaEmitterLaserLeadSeconds = 0.16f;
constexpr float AstragnaEmitterFlameTelegraphSeconds = 0.55f;
constexpr float AstragnaEmitterFlameActiveSeconds = 1.60f;
constexpr float AstragnaEmitterFlameCooldownSeconds = 2.15f;
constexpr float AstragnaEmitterFlameRangeTiles = 5.5f;
constexpr float AstragnaEmitterFlameHalfAngleRadians = 17.0f * Pi / 180.0f;
constexpr float AstragnaEmitterFlameSweepRadians = 28.0f * Pi / 180.0f;
constexpr float AstragnaEmitterFlameHitIntervalSeconds = 0.35f;
constexpr int AstragnaEmitterFlameDamage = 1;
constexpr float AstragnaReflectedShellDamageMultiplier = 0.55f;
constexpr float AstragnaDownedReflectedShellDamageMultiplier = 1.0f;
constexpr std::string_view DefaultEnemyId = "default_enemy";
constexpr float KeepDistanceMin = 130.0f;
constexpr float KeepDistanceMax = 210.0f;
constexpr float CaptureReach = 100.0f;
constexpr float CaptureNetDefaultRetryInterval = 0.75f;
constexpr float InspectEnemyDefaultRetryInterval = 0.75f;
constexpr float CaptureNetMaxChance = 0.95f;
constexpr float CaptureTargetPadding = 8.0f;
constexpr float CaptureTargetMinRadius = 14.0f;
constexpr std::string_view DefaultEnemyName = "敵";
constexpr std::string_view EnemyHealBehaviorId = "enemy_heal";
constexpr std::string_view EnemyHealAnimationId = "heal_slug_hop";

bool isAstragnaBossAction(const Enemy& enemy);
float astragnaOuterVisualRadius(const Enemy& enemy);
constexpr float EnemyHealActionDurationSeconds = 0.58f;
constexpr float EnemyHealActionFireAtSeconds = 0.30f;
constexpr std::string_view ChestBiteBehaviorId = "chest_bite";
constexpr std::string_view ChestBiteAnimationId = "mimic_bite_lunge";
constexpr float ChestBiteActionDurationSeconds = 0.42f;
constexpr float ChestBiteActionFireAtSeconds = 0.28f;
constexpr float ChestBiteDefaultIntervalSeconds = 1.5f;
constexpr float ChestBiteDefaultTriggerRange = 86.0f;
constexpr float ChestBiteDefaultJumpDistance = 74.0f;
constexpr float ChestBiteDefaultJumpDurationSeconds = 0.22f;
constexpr float ChestBiteDefaultJumpArcHeight = 22.0f;
constexpr float CapturedRewardChanceEnemy = 0.10f;
constexpr float CapturedStealChanceEnemy = 0.12f;
constexpr float CapturedRewardCooldown = 0.80f;
constexpr float CapturedRewardWindowSeconds = 10.0f;
constexpr int CapturedRewardWindowLimit = 3;
constexpr int CapturedBossRewardLimit = 2;
constexpr float StealDropFlySeconds = 0.24f;
constexpr float StealDropFlyArcHeight = 18.0f;
constexpr float StealDropPickupDelaySeconds = 0.04f;
constexpr float StealItemPickupRadius = 23.0f;
constexpr int CapturedExplosionChargeLimit = 4;
constexpr float CapturedExplosionSleepSeconds = 2.4f;
constexpr float CapturedExplosionRadius = 44.0f;

std::string enemyDisplayName(const Enemy& enemy)
{
    return enemy.enemyName.empty() ? std::string(DefaultEnemyName) : enemy.enemyName;
}

const char* debugYesNo(bool value)
{
    return value ? "Y" : "N";
}

const char* debugAwarenessName(EnemyAwarenessState state)
{
    switch (state) {
    case EnemyAwarenessState::Unaware:
        return "unaware";
    case EnemyAwarenessState::Detected:
        return "detected";
    }
    return "?";
}

std::string_view nonEmptyOr(const std::string& value, std::string_view fallback)
{
    return value.empty() ? fallback : std::string_view(value.data(), value.size());
}

std::string_view currentEnemyAiIdForDebug(const Enemy& enemy)
{
    return enemy.awareness == EnemyAwarenessState::Detected
        ? nonEmptyOr(enemy.aiId, "chase")
        : nonEmptyOr(enemy.unawareAiId, "idle");
}

std::string_view activeEnemyActionNameForDebug(const Enemy& enemy)
{
    if (!enemy.action.active) {
        return "-";
    }
    if (!enemy.action.behaviorId.empty()) {
        return std::string_view(enemy.action.behaviorId.data(), enemy.action.behaviorId.size());
    }
    if (!enemy.action.animationId.empty()) {
        return std::string_view(enemy.action.animationId.data(), enemy.action.animationId.size());
    }
    return "active";
}

float wrapDebugDegrees(float degrees)
{
    if (!std::isfinite(degrees)) {
        return 0.0f;
    }
    degrees = std::fmod(degrees, 360.0f);
    if (degrees > 180.0f) {
        degrees -= 360.0f;
    } else if (degrees <= -180.0f) {
        degrees += 360.0f;
    }
    return degrees;
}

int roundedDebugFloat(float value)
{
    return static_cast<int>(std::round(std::isfinite(value) ? value : 0.0f));
}
constexpr int CapturedExplosionDamage = 3;
constexpr float CapturedExplosionTerrainRadius = 28.0f;
constexpr int CapturedExplosionTerrainDamage = 1;
constexpr float CapturedMagnetEnemyRadius = 160.0f;
constexpr float CapturedMagnetEnemyPullSpeed = 58.0f;
constexpr int CapturedMagnetEnemyLimit = 4;
constexpr float VacuumLightEnemyPullSpeed = 44.0f;
constexpr int VacuumLightEnemyLimit = 4;
constexpr float VacuumLightEnemyFallbackRadius = 10.0f;
constexpr int VacuumLightEnemyFallbackMaxHp = 3;
constexpr float WindLightEnemyPushSpeed = 58.0f;
constexpr int WindLightEnemyLimit = 5;
constexpr std::size_t MaxEnemyWindPulses = 16;
constexpr float WindBlowDefaultRadiusTiles = 4.0f;
constexpr float WindBlowDefaultDurationSeconds = 3.5f;
constexpr float WindBlowPlayerPushSpeed = 16.0f;
constexpr float WindBlowEnemyPushSpeed = 28.0f;
constexpr float ExplosionRadiusScale = 1.5f;
constexpr double HotAirStatusDurationSeconds = 12.0;
constexpr float DefaultVisionDistance = 120.0f;
constexpr float DefaultVisionAngle = 100.0f;
constexpr float DefaultLoseSightSeconds = 1.5f;
constexpr float AwarenessIconDuration = 0.75f;
constexpr float LineOfSightStep = static_cast<float>(balance::TileSize) * 0.35f;
constexpr float WanderRetargetMin = 0.9f;
constexpr float WanderRetargetMax = 1.8f;
constexpr int FleeSearchRadiusTiles = 12;
constexpr int FleeSearchDiameterTiles = FleeSearchRadiusTiles * 2 + 1;
constexpr int FleeSearchTileCount = FleeSearchDiameterTiles * FleeSearchDiameterTiles;
constexpr float FleeWaypointLookAheadTiles = 2.5f;
constexpr float FleeWaypointReachRadius = 8.0f;
constexpr float FleeReplanIntervalSeconds = 0.28f;
constexpr float FleeBlockedThresholdSeconds = 0.20f;
constexpr float FleeFailedDirectionPenaltySeconds = 0.80f;
constexpr float FleeTurnSpeedRadiansPerSecond = 8.0f;
constexpr float PatrolRetargetMin = 1.8f;
constexpr float PatrolRetargetMax = 3.2f;
constexpr float PatrolRadius = 120.0f;
constexpr float MovementLeashSteerThreshold = 0.72f;
constexpr float ItemSeekRadius = 240.0f;
constexpr float DigActionInterval = 0.11f;
constexpr float SwarmAlertRadius = 180.0f;
constexpr float JumpLandingBuffSeconds = 0.24f;
constexpr float JumpAttackDurationMin = 0.12f;
constexpr float JumpAttackDurationMax = 0.65f;
constexpr float JumpAttackDefaultDuration = 0.28f;
constexpr float JumpAttackDefaultArcHeight = 24.0f;
constexpr float JumpTargetMinDistance = 4.0f;
constexpr float ExternalBounceGroundedAltitudeEpsilon = 1.5f;
constexpr float ExternalBounceMinDuration = 0.34f;
constexpr float ExternalBounceMaxDuration = 0.62f;
constexpr float ExternalBounceMinArcHeight = 28.0f;
constexpr float ExternalBounceMaxArcHeight = 86.0f;
constexpr float ExternalBounceMinDistance = 8.0f;
constexpr float ExternalBounceMaxDistance = 30.0f;
constexpr double ExternalBounceMaxStrength = 3.0;
constexpr double FallDamageSynergyMaxMultiplier = 5.0;
constexpr double ShockedDefaultDurationSeconds = 1.2;
constexpr double ShockedContactTransferDurationSeconds = 4.0;
constexpr float HoverChaseAltitude = 18.0f;
constexpr float HoverKeepDistanceAltitude = 20.0f;
constexpr float PhaseAltitude = 12.0f;
constexpr float HoverBobAmplitude = 3.0f;
constexpr float PhaseBobAmplitude = 2.0f;
constexpr float HoverBobSpeed = 4.0f;
constexpr float PhaseBobSpeed = 5.0f;
constexpr float EnemyHpBarDisplaySeconds = 2.0f;
constexpr float EnemyHpBarHeight = 4.0f;
constexpr float EnemyHpBarMinWidth = 24.0f;
constexpr float EnemyHpBarMaxWidth = 42.0f;
constexpr float EnemyDeathMinSeconds = 40.0f / 60.0f;
constexpr float EnemyDeathMaxSeconds = 60.0f / 60.0f;
constexpr float EnemyDeathKnockbackSeconds = 0.22f;
constexpr float EnemyDeathKnockbackSpeed = 145.0f;
constexpr float EnemyDeathKnockbackDampingPerSecond = 7.0f;
constexpr float EnemyDeathShakeMaxPixels = 4.0f;
constexpr float StunWakeHopSeconds = 0.18f;
constexpr float StunWakeHopPixels = 8.0f;
constexpr double DefaultCriticalDamageMultiplier = 2.0;
constexpr double MaxCriticalDamageMultiplier = 5.0;
constexpr double MaxSleepingBonusDamageMultiplier = 8.0;
constexpr double MaxDryWetDamageMultiplier = 8.0;
constexpr float ConfusedRetargetMinSeconds = 0.25f;
constexpr float ConfusedRetargetMaxSeconds = 0.65f;
constexpr double ConfusedSpeedMultiplier = 0.75;
constexpr float MudZoneTickSeconds = 0.20f;
constexpr float MudZoneMaxDurationSeconds = 30.0f;
constexpr float MudZoneVisualYScale = 0.56f;
constexpr float MudZoneOutlineMinJitter = 0.78f;
constexpr float MudZoneOutlineMaxJitter = 1.18f;
constexpr float MudZoneOuterFadeSeconds = 0.85f;
constexpr float MagnetDisturbMaxRadius = 320.0f;
constexpr float ColdExposureFreezeThreshold = 1.0f;
constexpr float ColdExposureRatePerSecond = 0.55f;
constexpr float ColdExposureDecayPerSecond = 0.45f;
constexpr double FrozenDefaultDurationSeconds = 8.0;
constexpr float BlindProjectileMaxSpreadDegrees = 70.0f;
constexpr int SwarmSpawnCountMax = 8;
constexpr int BossWeakPointHintMinParticles = 7;
constexpr int BossWeakPointHintMaxParticles = 13;
constexpr int FlowOrthogonalCost = 10;
constexpr int FlowDiagonalCost = 14;
constexpr double SpawnBiasDefaultMultiplier = 1.0;
constexpr double SpawnBiasMinMultiplier = 0.10;
constexpr double SpawnBiasMaxMultiplier = 5.0;

struct FlowStep {
    int dx = 0;
    int dy = 0;
    int cost = 0;
};

constexpr std::array<FlowStep, 8> FlowDirections{{
    {1, 0, FlowOrthogonalCost},
    {-1, 0, FlowOrthogonalCost},
    {0, 1, FlowOrthogonalCost},
    {0, -1, FlowOrthogonalCost},
    {1, 1, FlowDiagonalCost},
    {-1, 1, FlowDiagonalCost},
    {1, -1, FlowDiagonalCost},
    {-1, -1, FlowDiagonalCost},
}};

struct FlowNode {
    int distance = 0;
    int tx = 0;
    int ty = 0;
};

struct FlowNodeGreater {
    bool operator()(const FlowNode& left, const FlowNode& right) const
    {
        return left.distance > right.distance;
    }
};

struct FleeSearchNode {
    int routeCost = 0;
    int localIndex = 0;
};

struct FleeSearchNodeGreater {
    bool operator()(const FleeSearchNode& left, const FleeSearchNode& right) const
    {
        return left.routeCost > right.routeCost;
    }
};

struct CriticalEffectSource {
    const ObjectDefinition* object = nullptr;
    std::string effectKey;
    Vec2 position{};
};

struct CriticalDamageSpec {
    double chancePercent = 0.0;
    double damageMultiplier = DefaultCriticalDamageMultiplier;
    bool hasPowerOverride = false;
    bool forced = false;
    std::vector<CriticalEffectSource> sources;
};

struct BounceGroundedHitSpec {
    bool active = false;
    double strength = 1.0;
    bool fallDamageActive = false;
    double fallDamageMultiplier = 1.0;
};

struct ShockWetHitSpec {
    bool active = false;
    double value = 1.0;
    double duration = ShockedDefaultDurationSeconds;
};

struct FlameBurstHitSpec {
    bool active = false;
    float radius = 0.0f;
    int damage = 0;
};

bool parseIntStrict(std::string_view text, int& value)
{
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool parseDoubleStrict(std::string_view text, double& value)
{
    if (text.empty()) {
        return false;
    }
    std::string copy(text);
    char* parsedEnd = nullptr;
    value = std::strtod(copy.c_str(), &parsedEnd);
    return parsedEnd == copy.c_str() + copy.size();
}

std::string trimAscii(std::string_view text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

bool equalsIgnoreCaseAscii(std::string_view left, std::string_view right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) != std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }
    return true;
}

std::string lowerAscii(std::string_view text)
{
    std::string lowered(text);
    for (char& ch : lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

bool containsAnyAscii(std::string_view text, std::initializer_list<std::string_view> needles)
{
    const std::string lowered = lowerAscii(text);
    for (std::string_view needle : needles) {
        if (lowered.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool hasTagAscii(const EnemyDefinition& definition, std::initializer_list<std::string_view> tags)
{
    for (const std::string& tag : definition.enemyTags) {
        for (std::string_view expected : tags) {
            if (equalsIgnoreCaseAscii(tag, expected)) {
                return true;
            }
        }
    }
    return false;
}

bool enemyDefinitionMatchesSpawnBiasGroup(const EnemyDefinition& definition, std::string_view group)
{
    if (group.empty()) {
        return false;
    }

    const std::string groupString(group);
    const std::string groupTag = "spawn_group_" + groupString;
    const std::string biasTag = "spawn_bias_" + groupString;
    for (const std::string& tag : definition.enemyTags) {
        if (equalsIgnoreCaseAscii(tag, group) ||
            equalsIgnoreCaseAscii(tag, groupTag) ||
            equalsIgnoreCaseAscii(tag, biasTag)) {
            return true;
        }
    }
    return false;
}

bool isExcludedFromNormalDugSpawn(const EnemyDefinition& definition)
{
    if (hasTagAscii(definition, {
            "boss",
            "boss_only",
            "no_normal_spawn",
            "event_only",
            "fixed_only",
            "base_npc",
            "hidden_npc",
            "npc",
            "unique",
        })) {
        return true;
    }
    for (const std::string& tag : definition.enemyTags) {
        if (tag == "ボス" ||
            tag == "通常スポーン除外" ||
            tag == "固定配置専用" ||
            tag == "イベント専用" ||
            tag == "NPC" ||
            tag == "拠点NPC" ||
            tag == "隠しNPC" ||
            tag == "固有") {
            return true;
        }
    }
    if (containsAnyAscii(definition.id, {
            "boss",
            "base_npc_",
            "hidden_npc_",
            "stardust_mole",
            "junk_crab",
            "junkrab",
            "astragna",
            "star_vein_dragon",
            "starvein_dragon",
        })) {
        return true;
    }
    return definition.name.find("星くずモグラ") != std::string::npos ||
        definition.name.find("ジャンクラブ") != std::string::npos ||
        definition.name.find("アストラグナ") != std::string::npos ||
        definition.name.find("星脈竜") != std::string::npos;
}

bool isRoguelikeEnemyLevelSelectionStage(std::string_view stageId)
{
    return stageId == "stage_04_astral_mine";
}

int roguelikeTargetBaseLevelForDepthRank(int depthRank)
{
    constexpr std::array<int, 9> InitialLevels{{3, 7, 11, 15, 20, 25, 30, 35, 40}};
    const int rank = std::max(1, depthRank);
    if (rank <= static_cast<int>(InitialLevels.size())) {
        return InitialLevels[static_cast<std::size_t>(rank - 1)];
    }
    return std::min(100, InitialLevels.back() + (rank - static_cast<int>(InitialLevels.size())) * 5);
}

std::string roguelikeEnemyPoolKey(std::string_view stageId, int depthRank)
{
    return std::string(stageId) + ":" + std::to_string(std::max(1, depthRank));
}

std::array<EnemyVariantTier, 3> enemySpawnVariantTiers()
{
    return {EnemyVariantTier::Normal, EnemyVariantTier::Deep, EnemyVariantTier::Abyss};
}

std::string baseEnemyName(const EnemyDefinition& definition)
{
    return definition.name.empty() ? definition.id : definition.name;
}

Color multiplyRgb(Color color, Color multiplier)
{
    color.r = static_cast<unsigned char>(static_cast<int>(color.r) * static_cast<int>(multiplier.r) / 255);
    color.g = static_cast<unsigned char>(static_cast<int>(color.g) * static_cast<int>(multiplier.g) / 255);
    color.b = static_cast<unsigned char>(static_cast<int>(color.b) * static_cast<int>(multiplier.b) / 255);
    return color;
}

Color enemyVariantTintMultiplier(EnemyVariantTier tier)
{
    switch (tier) {
    case EnemyVariantTier::Deep:
        return {170, 170, 192, 255};
    case EnemyVariantTier::Abyss:
        return {118, 118, 142, 255};
    case EnemyVariantTier::Normal:
        break;
    }
    return {255, 255, 255, 255};
}

enum class EnemyStatKind {
    Hp,
    ContactAttack,
    Xp,
    Money,
};

int enemyStatValue(const EnemyDefinition& definition, EnemyStatKind kind)
{
    switch (kind) {
    case EnemyStatKind::Hp:
        return definition.hp;
    case EnemyStatKind::ContactAttack:
        return definition.contactAttackPower;
    case EnemyStatKind::Xp:
        return definition.xp;
    case EnemyStatKind::Money:
        return definition.money;
    }
    return 0;
}

double enemyStatFallbackGrowthPerLevel(EnemyStatKind kind)
{
    switch (kind) {
    case EnemyStatKind::Hp:
        return 0.060;
    case EnemyStatKind::ContactAttack:
        return 0.040;
    case EnemyStatKind::Xp:
    case EnemyStatKind::Money:
        return 0.045;
    }
    return 0.045;
}

struct EnemyStatRegression {
    bool valid = false;
    double intercept = 0.0;
    double slope = 0.0;

    double estimate(int level, double fallback) const
    {
        if (!valid) {
            return std::max(1.0, fallback);
        }
        return std::max(1.0, intercept + slope * static_cast<double>(std::max(1, level)));
    }
};

EnemyStatRegression buildEnemyStatRegression(const EnemyCatalog& catalog, EnemyStatKind kind)
{
    double count = 0.0;
    double sumX = 0.0;
    double sumY = 0.0;
    double sumXX = 0.0;
    double sumXY = 0.0;
    for (const EnemyDefinition& definition : catalog.enemies) {
        if (isExcludedFromNormalDugSpawn(definition) || definition.baseLevel <= 0) {
            continue;
        }
        const int value = enemyStatValue(definition, kind);
        if (value <= 0) {
            continue;
        }
        const double x = static_cast<double>(definition.baseLevel);
        const double y = static_cast<double>(value);
        count += 1.0;
        sumX += x;
        sumY += y;
        sumXX += x * x;
        sumXY += x * y;
    }
    if (count < 2.0) {
        return {};
    }

    const double denom = count * sumXX - sumX * sumX;
    if (std::abs(denom) < 0.0001) {
        return {.valid = true, .intercept = sumY / count, .slope = 0.0};
    }
    const double slope = std::max(0.0, (count * sumXY - sumX * sumY) / denom);
    const double intercept = (sumY - slope * sumX) / count;
    return {.valid = true, .intercept = intercept, .slope = slope};
}

int scaledEnemyStatForEffectiveLevel(
    const EnemyCatalog& catalog,
    const EnemyDefinition& definition,
    EnemyStatKind kind,
    int baseValue,
    int effectiveBaseLevel)
{
    if (baseValue <= 0) {
        return 0;
    }
    const int baseLevel = std::max(1, definition.baseLevel);
    const int targetLevel = std::max(baseLevel, effectiveBaseLevel);
    if (targetLevel <= baseLevel) {
        return baseValue;
    }

    const EnemyStatRegression regression = buildEnemyStatRegression(catalog, kind);
    const double baseExpected = regression.estimate(baseLevel, static_cast<double>(baseValue));
    double targetExpected = regression.estimate(targetLevel, static_cast<double>(baseValue));
    if (targetExpected <= baseExpected) {
        targetExpected = baseExpected *
            (1.0 + static_cast<double>(targetLevel - baseLevel) * enemyStatFallbackGrowthPerLevel(kind));
    }

    const double relative = std::clamp(
        static_cast<double>(baseValue) / std::max(1.0, baseExpected),
        0.35,
        kind == EnemyStatKind::ContactAttack ? 4.0 : 3.0);
    const int scaled = static_cast<int>(std::lround(targetExpected * relative));
    return std::max(baseValue + 1, scaled);
}

int applyDefenseModifier(const EntityStatus& status, int damage)
{
    if (damage <= 0) {
        return 0;
    }

    const double defense = std::max(0.05, status.multiplierFor(ModifierStat::Defense) * status.defenseMultiplierFromStates());
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(damage) / defense)));
}

float effectiveEnemyRadius(const Enemy& enemy)
{
    return std::max(0.0f, enemy.radius * static_cast<float>(enemy.status.sizeMultiplierFromStates()));
}

float enemyPlacementRuntimeRadiusScale(const Enemy& enemy)
{
    float baseRadius = enemy.definition != nullptr &&
            enemy.definition->radius > 0.0 &&
            std::isfinite(enemy.definition->radius)
        ? static_cast<float>(enemy.definition->radius)
        : enemy.radius;
    baseRadius = std::max(0.001f, baseRadius);
    return std::max(0.1f, enemy.radius / baseRadius);
}

float enemyPassageRadius(const Enemy& enemy, const EnemyPlacementCatalog* placementCatalog)
{
    float radius = enemy.radius;
    if (const std::optional<float> placementRadius = enemyPlacementPassageRadiusFor(placementCatalog, enemy.enemyId)) {
        radius = *placementRadius * enemyPlacementRuntimeRadiusScale(enemy);
    }
    return std::max(0.0f, radius * static_cast<float>(enemy.status.sizeMultiplierFromStates()));
}

Vec2 enemyVisualOffset(const Enemy& enemy, const EnemyPlacementCatalog* placementCatalog)
{
    return resolvedEnemyVisualOffset(placementCatalog, enemy);
}

float enemyVisualRadius(const Enemy& enemy)
{
    const float radius = effectiveEnemyRadius(enemy);
    return enemy.isBoss ? radius * BossVisualRadiusMultiplier : radius;
}

float effectivePlayerRadius(const Player& player, const RuntimeBalance& balance)
{
    return player.effectiveRadius(balance.playerRadius);
}

float playerHitboxScale(const Player& player)
{
    return std::max(0.0f, static_cast<float>(player.status.sizeMultiplierFromStates()));
}

bool enemyHitboxOverlapsPlayer(
    const Enemy& enemy,
    const HitboxCatalog* catalog,
    const Player& player,
    const RuntimeBalance& balance,
    Vec2 centerOffset)
{
    if (const HitboxProfile* profile = playerHitboxProfileFor(catalog)) {
        return enemyHitboxOverlapsProfile(
            enemy,
            catalog,
            *profile,
            player.position,
            0.0f,
            playerHitboxScale(player),
            0.0f,
            centerOffset);
    }
    return enemyHitboxOverlapsCircle(enemy, catalog, player.position, effectivePlayerRadius(player, balance), centerOffset);
}

double damageTypeMultiplier(std::string_view damageType)
{
    if (damageType == "fire" || damageType == "thunder" || damageType == "magic") {
        return 1.10;
    }
    if (damageType == "earth") {
        return 1.05;
    }
    if (damageType == "ice" || damageType == "water") {
        return 0.95;
    }
    return 1.0;
}

bool isKnownAi(std::string_view aiId)
{
    return aiId.empty() ||
        aiId == "idle" ||
        aiId == "wander" ||
        aiId == "patrol" ||
        aiId == "buried" ||
        aiId == "dig_wander" ||
        aiId == "item_seek" ||
        aiId == "phase_wander" ||
        aiId == "chase" ||
        aiId == "shield_chase" ||
        aiId == "support" ||
        aiId == "dig_chase" ||
        aiId == "phase_chase" ||
        aiId == "burrow_ambush" ||
        aiId == "ambush" ||
        aiId == "jump_chase" ||
        aiId == "keep_distance" ||
        aiId == "hover_chase" ||
        aiId == "hover_keep_distance" ||
        aiId == "stationary" ||
        aiId == "flee";
}

bool isSupportedBehavior(std::string_view behaviorId)
{
    return behaviorId == "contact_basic" ||
        behaviorId == "physical_resist" ||
        behaviorId == "magic_body" ||
        behaviorId == "front_guard" ||
        behaviorId == "spike_contact" ||
        behaviorId == "jump_attack" ||
        behaviorId == "ring_slow_bite" ||
        behaviorId == "swarm_alert" ||
        behaviorId == "swarm_spawn" ||
        behaviorId == "dig_move" ||
        behaviorId == "light_slow" ||
        behaviorId == "magnet_disturb" ||
        behaviorId == "rust_touch" ||
        behaviorId == "steal_item" ||
        behaviorId == "chest_bite" ||
        behaviorId == "drop_item" ||
        behaviorId == "carry_loot" ||
        behaviorId == "drop_material" ||
        behaviorId == "throw_object" ||
        behaviorId == "throw_stone" ||
        behaviorId == "shoot_poison" ||
        behaviorId == "shoot_web" ||
        behaviorId == "shoot_fire" ||
        behaviorId == "shoot_paralyze" ||
        behaviorId == "shoot_mud" ||
        behaviorId == "radial_spike" ||
        behaviorId == "shoot_water" ||
        behaviorId == "shoot_bubble" ||
        behaviorId == "shoot_water_bubble" ||
        behaviorId == "wind_blow" ||
        behaviorId == "enemy_heal" ||
        behaviorId == "countdown_explode" ||
        behaviorId == "boss_sequence";
}

bool hasBehavior(const Enemy& enemy, std::string_view behaviorId)
{
    return std::any_of(enemy.behaviorIds.begin(), enemy.behaviorIds.end(), [behaviorId](const std::string& value) {
        return value == behaviorId;
    });
}

bool isBombTsuchinokoFamily(const Enemy& enemy)
{
    return enemy.enemyId.find(BombTsuchinokoEnemyId.data()) != std::string::npos;
}

bool canArmCountdownExplosion(const Enemy& enemy, Vec2 playerPosition)
{
    if (enemy.awareness != EnemyAwarenessState::Detected ||
        enemy.spawnTimer > 0.0f ||
        enemy.bossAction.hidden ||
        enemy.death.active) {
        return false;
    }

    const float armDistance = std::max(0.0f, enemy.countdownExplodeArmDistance);
    return armDistance <= 0.0f ||
        distanceSquared(enemy.position, playerPosition) <= armDistance * armDistance;
}

const EnemyBehaviorSpec* findEnemyBehaviorSpec(const Enemy& enemy, std::string_view behaviorId)
{
    if (enemy.definition == nullptr) {
        return nullptr;
    }
    for (const EnemyBehaviorSpec& spec : enemy.definition->enemyBehaviorSpecs) {
        if (spec.behavior == behaviorId) {
            return &spec;
        }
        if (behaviorId == "throw_stone" && spec.behavior == "throw_object") {
            return &spec;
        }
    }
    return nullptr;
}

double behaviorParamDouble(const Enemy& enemy, std::string_view behaviorId, std::string_view key, double fallbackValue)
{
    const EnemyBehaviorSpec* spec = findEnemyBehaviorSpec(enemy, behaviorId);
    if (spec == nullptr) {
        return fallbackValue;
    }
    const auto it = spec->params.find(std::string(key));
    if (it == spec->params.end()) {
        return fallbackValue;
    }
    double value = fallbackValue;
    if (!parseDoubleStrict(it->second, value) || !std::isfinite(value)) {
        return fallbackValue;
    }
    return value;
}

int behaviorParamInt(const Enemy& enemy, std::string_view behaviorId, std::string_view key, int fallbackValue)
{
    const EnemyBehaviorSpec* spec = findEnemyBehaviorSpec(enemy, behaviorId);
    if (spec == nullptr) {
        return fallbackValue;
    }
    const auto it = spec->params.find(std::string(key));
    if (it == spec->params.end()) {
        return fallbackValue;
    }
    int value = fallbackValue;
    if (!parseIntStrict(it->second, value)) {
        return fallbackValue;
    }
    return value;
}

std::string behaviorParamString(const Enemy& enemy, std::string_view behaviorId, std::string_view key, std::string_view fallbackValue)
{
    const EnemyBehaviorSpec* spec = findEnemyBehaviorSpec(enemy, behaviorId);
    if (spec == nullptr) {
        return std::string(fallbackValue);
    }
    const auto it = spec->params.find(std::string(key));
    if (it == spec->params.end()) {
        return std::string(fallbackValue);
    }
    return it->second;
}

bool filterContainsToken(std::string_view filter, std::string_view token)
{
    std::size_t start = 0;
    while (start <= filter.size()) {
        const std::size_t end = filter.find('|', start);
        const std::string part = trimAscii(filter.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (equalsIgnoreCaseAscii(part, token)) {
            return true;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return false;
}

bool objectHasTag(const ObjectDefinition& object, std::string_view tag)
{
    return std::any_of(object.tags.begin(), object.tags.end(), [tag](const std::string& value) {
        return value == tag;
    });
}

bool objectExcludedFromHeldDrops(const ObjectDefinition& object)
{
    return isCodexHiddenObject(object) ||
        objectHasTag(object, "no_drop") ||
        objectHasTag(object, "nodrop") ||
        objectHasTag(object, "shop_only") ||
        objectHasTag(object, "\xE3\x82\xB7\xE3\x83\xA7\xE3\x83\x83\xE3\x83\x97\xE5\xB0\x82\xE7\x94\xA8");
}

bool chestKindForHeldDropProfile(std::string_view profile, LootChestKind& outKind)
{
    if (profile.empty() || profile == "common" || profile == "box_common" || profile == "ore") {
        outKind = LootChestKind::Common;
        return true;
    }
    if (profile == "rare" || profile == "box_rare" || profile == "ore_rare") {
        outKind = LootChestKind::Rare;
        return true;
    }
    if (profile == "super" || profile == "super_rare" || profile == "box_super" || profile == "ore_super") {
        outKind = LootChestKind::SuperRare;
        return true;
    }
    return false;
}

std::string_view requiredTagForHeldDropProfile(std::string_view profile)
{
    if (profile == "ore" || profile == "ore_rare" || profile == "ore_super") {
        return "ore";
    }
    return {};
}

std::string chooseHeldObjectIdForProfile(
    const ObjectCatalog& catalog,
    std::string_view stageId,
    int depthRank,
    std::string_view profile,
    std::mt19937& rng)
{
    LootChestKind chestKind = LootChestKind::Common;
    if (!chestKindForHeldDropProfile(profile, chestKind) || stageId.empty()) {
        return {};
    }

    const std::string_view requiredTag = requiredTagForHeldDropProfile(profile);
    std::vector<const ObjectDefinition*> candidates;
    std::vector<double> weights;
    for (const ObjectDefinition& object : catalog.objects) {
        if (objectExcludedFromHeldDrops(object)) {
            continue;
        }
        if (!requiredTag.empty() && !objectHasTag(object, requiredTag)) {
            continue;
        }
        const double weight = lootWeightFor(object, stageId, depthRank, chestKind);
        if (weight < 1.0) {
            continue;
        }
        candidates.push_back(&object);
        weights.push_back(weight);
    }
    const std::optional<std::size_t> selected = selectWeightedIndex(weights, rng);
    if (!selected || *selected >= candidates.size()) {
        return {};
    }
    return candidates[*selected]->id;
}

WorldDropSpawnMotion makeStealDropMotion(Vec2 startPosition)
{
    return WorldDropSpawnMotion{
        .jump = true,
        .startPosition = startPosition,
        .jumpDurationSeconds = StealDropFlySeconds,
        .jumpArcHeight = StealDropFlyArcHeight,
        .pickupDelaySeconds = StealDropPickupDelaySeconds,
    };
}

bool enemyCanStealInView(
    const Enemy& enemy,
    const CollisionRect& stealViewBounds,
    const EnemyPlacementCatalog* placementCatalog)
{
    return circleIntersectsRect(enemy.position, enemyPassageRadius(enemy, placementCatalog), stealViewBounds);
}

const WorldDropItem* nearestStealableDropForEnemy(
    const Enemy& enemy,
    const ObjectCatalog& objectCatalog,
    const WorldDropSystem& worldDrops,
    const CollisionRect& stealViewBounds)
{
    return worldDrops.nearestStealableDrop(
        objectCatalog,
        enemy.position,
        std::max(enemy.stealRadius, enemy.stealSeekRadius),
        enemy.stealTarget,
        &stealViewBounds);
}

EnemyEvent makeEnemyStealEvent(const Enemy& enemy, const EnemyHeldDrop& stolenDrop)
{
    EnemyEvent event;
    event.type = EnemyEventType::Steal;
    event.position = enemy.position;
    event.enemyRuntimeId = enemy.id;
    event.dungeonEventId = enemy.dungeonEventId;
    event.enemyId = enemy.enemyId;
    event.enemyName = enemy.enemyName;
    if (stolenDrop.kind == EnemyHeldDropKind::Money) {
        event.moneyDrop = std::max(0, stolenDrop.quantity);
    } else if (stolenDrop.kind == EnemyHeldDropKind::Object) {
        event.objectDropId = stolenDrop.objectId;
        event.objectDropCount = 1;
        event.objectDropRuntimeItem = stolenDrop.runtimeItem;
    }
    return event;
}

bool inventoryItemMatchesStealTarget(const ItemData& item, std::string_view targetFilter)
{
    if (targetFilter.empty() ||
        filterContainsToken(targetFilter, "drop") ||
        filterContainsToken(targetFilter, "item") ||
        filterContainsToken(targetFilter, "object")) {
        return true;
    }
    return filterContainsToken(targetFilter, "treasure") &&
        (item.category == "\xE5\xAE\x9D" || objectHasTag(item, "treasure"));
}

bool takePlayerInventoryItem(
    InventorySystem& inventory,
    std::string_view targetFilter,
    std::mt19937& rng,
    EnemyHeldDrop& outDrop)
{
    struct Candidate {
        bool instance = false;
        std::string stableId;
    };
    std::vector<Candidate> candidates;
    for (const InventoryObjectStack& stack : inventory.objectStacks()) {
        if (stack.count <= 0 || stack.objectId.empty() ||
            objectExcludedFromHeldDrops(stack.item) ||
            !inventoryItemMatchesStealTarget(stack.item, targetFilter)) {
            continue;
        }
        candidates.push_back({false, stack.objectId});
    }
    for (const InventoryObjectInstance& entry : inventory.objectInstances()) {
        if (entry.instance.instanceId.empty() ||
            entry.instance.protectionEnabled ||
            inventory.isStaffEquipped(entry.instance.instanceId) ||
            objectExcludedFromHeldDrops(entry.item) ||
            !inventoryItemMatchesStealTarget(entry.item, targetFilter)) {
            continue;
        }
        candidates.push_back({true, entry.instance.instanceId});
    }
    if (candidates.empty()) {
        return false;
    }

    std::uniform_int_distribution<std::size_t> candidateDist(0, candidates.size() - 1);
    const Candidate candidate = candidates[candidateDist(rng)];
    if (!candidate.instance) {
        const auto stackIt = std::find_if(
            inventory.objectStacks().begin(),
            inventory.objectStacks().end(),
            [&candidate](const InventoryObjectStack& stack) {
                return stack.objectId == candidate.stableId;
            });
        if (stackIt == inventory.objectStacks().end()) {
            return false;
        }
        const ItemData item = stackIt->item;
        if (!inventory.removeObjectItemCount(candidate.stableId, 1)) {
            return false;
        }
        outDrop = EnemyHeldDrop{
            .kind = EnemyHeldDropKind::Object,
            .origin = EnemyHeldDropOrigin::PickedUp,
            .objectId = item.id.empty() ? candidate.stableId : item.id,
            .quantity = 1,
            .deathDropChance = 1.0f,
            .runtimeItem = item,
        };
        return true;
    }

    InventoryObjectInstance taken;
    if (!inventory.takeObjectInstance(candidate.stableId, taken)) {
        return false;
    }
    outDrop = EnemyHeldDrop{
        .kind = EnemyHeldDropKind::Object,
        .origin = EnemyHeldDropOrigin::PickedUp,
        .objectId = taken.item.id.empty() ? taken.instance.objectId : taken.item.id,
        .quantity = 1,
        .deathDropChance = 1.0f,
        .instance = std::move(taken.instance),
        .runtimeItem = std::move(taken.item),
    };
    return true;
}

bool takePlayerLoot(
    Enemy& enemy,
    InventorySystem& inventory,
    const std::function<int(int, Vec2)>& takeMoney,
    std::mt19937& rng,
    EnemyHeldDrop& outDrop)
{
    const bool canTakeItems = enemy.stealTarget.empty() ||
        filterContainsToken(enemy.stealTarget, "drop") ||
        filterContainsToken(enemy.stealTarget, "item") ||
        filterContainsToken(enemy.stealTarget, "object") ||
        filterContainsToken(enemy.stealTarget, "treasure");
    if (canTakeItems && takePlayerInventoryItem(inventory, enemy.stealTarget, rng, outDrop)) {
        return true;
    }

    const bool canTakeMoney = enemy.stealTarget.empty() ||
        filterContainsToken(enemy.stealTarget, "drop") ||
        filterContainsToken(enemy.stealTarget, "money");
    if (!canTakeMoney || !takeMoney) {
        return false;
    }
    const int requested = std::max(
        1,
        behaviorParamInt(enemy, "steal_item", "playerMoney", std::max(1, enemy.moneyDrop)));
    const int stolen = std::max(0, takeMoney(requested, enemy.position));
    if (stolen <= 0) {
        return false;
    }
    outDrop = EnemyHeldDrop{
        .kind = EnemyHeldDropKind::Money,
        .origin = EnemyHeldDropOrigin::PickedUp,
        .quantity = stolen,
        .deathDropChance = 1.0f,
    };
    return true;
}

bool heldDropMatchesFilter(const EnemyHeldDrop& drop, const ObjectCatalog& catalog, std::string_view targetFilter)
{
    if (targetFilter.empty() || filterContainsToken(targetFilter, "drop")) {
        return true;
    }
    if (drop.kind == EnemyHeldDropKind::Money) {
        return filterContainsToken(targetFilter, "money");
    }
    if (drop.kind != EnemyHeldDropKind::Object) {
        return false;
    }
    if (filterContainsToken(targetFilter, "object") || filterContainsToken(targetFilter, "item")) {
        return true;
    }
    if (filterContainsToken(targetFilter, "treasure")) {
        const ObjectDefinition* object = catalog.registry.findById(drop.objectId);
        return object != nullptr && objectHasTag(*object, "treasure");
    }
    return false;
}

bool addHeldDropToEnemy(Enemy& enemy, EnemyHeldDrop drop)
{
    if (drop.quantity <= 0) {
        return false;
    }
    if (drop.kind == EnemyHeldDropKind::Money) {
        for (EnemyHeldDrop& held : enemy.heldDrops) {
            if (held.kind == EnemyHeldDropKind::Money && held.origin == drop.origin) {
                held.quantity += drop.quantity;
                held.deathDropChance = std::max(held.deathDropChance, drop.deathDropChance);
                return true;
            }
        }
    } else if (drop.objectId.empty()) {
        return false;
    }
    if (static_cast<int>(enemy.heldDrops.size()) >= EnemyHeldDropCapacity) {
        return false;
    }
    enemy.heldDrops.push_back(std::move(drop));
    return true;
}

bool isNoneToken(std::string_view text)
{
    const std::string trimmed = trimAscii(text);
    return trimmed.empty() ||
        equalsIgnoreCaseAscii(trimmed, "none") ||
        equalsIgnoreCaseAscii(trimmed, "-");
}

struct EnemyActionProfile {
    std::string animationId;
    float durationSeconds = 0.0f;
    float fireAtSeconds = 0.0f;
    bool lockMovement = false;
    bool lockFacing = true;
};

EnemyActionProfile defaultRangedActionProfile(std::string_view behaviorId)
{
    if (behaviorId == "shoot_web") {
        return {"web_shoot", 0.32f, 0.18f, true, true};
    }
    if (behaviorId == "shoot_fire") {
        return {"fire_breath_hop", 0.45f, 0.26f, true, true};
    }
    if (behaviorId == "radial_spike") {
        return {"radial_spike_squash", 0.46f, 0.17f, true, true};
    }
    if (behaviorId == "shoot_poison") {
        return {"poison_frog_spit_squash", 0.66f, 0.42f, true, true};
    }
    return {};
}

EnemyActionProfile applyBehaviorActionParams(const Enemy& enemy, std::string_view behaviorId, EnemyActionProfile profile)
{
    if (behaviorId.empty()) {
        return profile;
    }

    const std::string configuredAnimation = behaviorParamString(enemy, behaviorId, "anim", profile.animationId);
    profile.animationId = isNoneToken(configuredAnimation) ? std::string{} : configuredAnimation;

    constexpr double MissingParam = -99999.0;
    const double windup = behaviorParamDouble(enemy, behaviorId, "windup", MissingParam);
    const double recovery = behaviorParamDouble(enemy, behaviorId, "recovery", MissingParam);
    if (windup != MissingParam || recovery != MissingParam) {
        const float fireAtSeconds = static_cast<float>(std::max(0.0, windup != MissingParam ? windup : static_cast<double>(profile.fireAtSeconds)));
        const float defaultRecovery = std::max(0.0f, profile.durationSeconds - profile.fireAtSeconds);
        const float recoverySeconds = static_cast<float>(std::max(0.0, recovery != MissingParam ? recovery : static_cast<double>(defaultRecovery)));
        profile.fireAtSeconds = fireAtSeconds;
        profile.durationSeconds = fireAtSeconds + recoverySeconds;
    }

    const double fireAt = behaviorParamDouble(enemy, behaviorId, "fireAt", MissingParam);
    if (fireAt != MissingParam) {
        profile.fireAtSeconds = static_cast<float>(std::max(0.0, fireAt));
    }

    const double duration = behaviorParamDouble(enemy, behaviorId, "duration", MissingParam);
    if (duration != MissingParam) {
        profile.durationSeconds = static_cast<float>(std::max(0.0, duration));
    }

    const float clipDuration = actorVisualMotionDuration(profile.animationId);
    if (profile.durationSeconds <= 0.0f && clipDuration > 0.0f) {
        profile.durationSeconds = clipDuration;
    }
    profile.durationSeconds = std::max(profile.durationSeconds, profile.fireAtSeconds);
    profile.fireAtSeconds = clamp(profile.fireAtSeconds, 0.0f, std::max(0.0f, profile.durationSeconds));
    profile.lockMovement = behaviorParamInt(enemy, behaviorId, "lockMove", profile.lockMovement ? 1 : 0) != 0;
    profile.lockFacing = behaviorParamInt(enemy, behaviorId, "lockFacing", profile.lockFacing ? 1 : 0) != 0;
    return profile;
}

EnemyActionProfile rangedActionProfileFor(const Enemy& enemy)
{
    return applyBehaviorActionParams(
        enemy,
        enemy.rangedBehaviorId,
        defaultRangedActionProfile(enemy.rangedBehaviorId));
}

EnemyActionProfile healActionProfileFor(const Enemy& enemy)
{
    return applyBehaviorActionParams(
        enemy,
        EnemyHealBehaviorId,
        {std::string(EnemyHealAnimationId), EnemyHealActionDurationSeconds, EnemyHealActionFireAtSeconds, true, true});
}

EnemyActionProfile chestBiteActionProfileFor(const Enemy& enemy)
{
    return applyBehaviorActionParams(
        enemy,
        ChestBiteBehaviorId,
        {std::string(ChestBiteAnimationId), ChestBiteActionDurationSeconds, ChestBiteActionFireAtSeconds, true, true});
}

bool hasActionProfile(const EnemyActionProfile& profile)
{
    return profile.durationSeconds > 0.0f || profile.fireAtSeconds > 0.0f;
}

bool isStardustMoleId(std::string_view enemyId)
{
    return enemyId == StardustMoleEnemyId;
}

bool isJunkCrabId(std::string_view enemyId)
{
    return enemyId == JunkCrabEnemyId;
}

bool isAstragnaId(std::string_view enemyId)
{
    return enemyId == AstragnaEnemyId;
}

bool isStarVeinDragonId(std::string_view enemyId)
{
    return enemyId == StarVeinDragonEnemyId;
}

bool isStardustMoleEnemy(const Enemy& enemy)
{
    return isStardustMoleId(enemy.enemyId) || enemy.enemyName.find("星くずモグラ") != std::string::npos;
}

bool isJunkCrabEnemy(const Enemy& enemy)
{
    return isJunkCrabId(enemy.enemyId) || enemy.enemyName.find("ジャンクラブ") != std::string::npos;
}

bool isAstragnaEnemy(const Enemy& enemy)
{
    return isAstragnaId(enemy.enemyId) || enemy.enemyName.find("アストラグナ") != std::string::npos;
}

std::string defaultBossActionPatternFor(const Enemy& enemy)
{
    if (isStardustMoleEnemy(enemy)) {
        return std::string(StardustMolePatternId);
    }
    if (isJunkCrabEnemy(enemy)) {
        return std::string(JunkCrabPatternId);
    }
    if (isAstragnaEnemy(enemy)) {
        return std::string(AstragnaPatternId);
    }
    return {};
}

bool enemyVisible(const Enemy& enemy)
{
    return enemy.active && !enemy.bossAction.hidden;
}

int enemyHeldDropCount(const Enemy& enemy)
{
    return static_cast<int>(enemy.heldDrops.size());
}

bool enemyScreenSleepEligible(const Enemy& enemy)
{
    return enemy.active &&
        enemy.screenSleepAllowed &&
        enemy.spawnSource == EnemySpawnSource::Ambient &&
        !enemy.isBoss &&
        !enemy.death.active &&
        !enemy.dungeonEventBoss &&
        enemy.dungeonEventId.empty();
}

bool enemyCanBeHit(const Enemy& enemy)
{
    return enemyVisible(enemy) &&
        !enemy.dungeonEventActivationLocked &&
        !enemy.death.active &&
        enemy.hp > 0 &&
        !enemy.bossAction.invulnerable;
}

bool hasEnemyTag(const Enemy& enemy, std::string_view tag)
{
    return std::any_of(enemy.enemyTags.begin(), enemy.enemyTags.end(), [tag](const std::string& value) {
        return value == tag;
    });
}

bool hasEnemyTagAny(const Enemy& enemy, std::initializer_list<std::string_view> tags)
{
    return std::any_of(enemy.enemyTags.begin(), enemy.enemyTags.end(), [tags](const std::string& value) {
        return std::any_of(tags.begin(), tags.end(), [&value](std::string_view tag) {
            return equalsIgnoreCaseAscii(value, tag);
        });
    });
}

bool enemyDeathKnockbackAllowed(const Enemy& enemy)
{
    if (hasEnemyTagAny(enemy, {"death_no_knockback", "no_death_knockback"})) {
        return false;
    }
    if (enemy.aiId == "stationary" || enemy.aiId == "idle" || enemy.aiId == "buried") {
        return false;
    }
    if (enemy.isBoss && !hasEnemyTagAny(enemy, {"death_knockback", "force_death_knockback"})) {
        return false;
    }
    return true;
}

std::uint32_t mixDeathSeed(std::uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

float deathUnitRandom(std::uint32_t seed, int frame, std::uint32_t salt)
{
    const std::uint32_t mixed = mixDeathSeed(seed ^ (static_cast<std::uint32_t>(frame) * 0x9e3779b9U) ^ salt);
    return static_cast<float>(mixed & 0xffffU) / 65535.0f;
}

float enemyDeathProgress(const Enemy& enemy)
{
    if (!enemy.death.active || enemy.death.durationSeconds <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(enemy.death.elapsedSeconds / enemy.death.durationSeconds, 0.0f, 1.0f);
}

unsigned char enemyDeathShade(const Enemy& enemy)
{
    const float progress = enemyDeathProgress(enemy);
    const float shade = 255.0f * (1.0f - progress);
    return static_cast<unsigned char>(std::clamp(std::round(shade), 0.0f, 255.0f));
}

Color darkenEnemyColorForDeath(Color color, const Enemy& enemy)
{
    const float multiplier = static_cast<float>(enemyDeathShade(enemy)) / 255.0f;
    color.r = static_cast<unsigned char>(std::clamp(std::round(static_cast<float>(color.r) * multiplier), 0.0f, 255.0f));
    color.g = static_cast<unsigned char>(std::clamp(std::round(static_cast<float>(color.g) * multiplier), 0.0f, 255.0f));
    color.b = static_cast<unsigned char>(std::clamp(std::round(static_cast<float>(color.b) * multiplier), 0.0f, 255.0f));
    return color;
}

Vec2 enemyDeathShakeOffset(const Enemy& enemy)
{
    if (!enemy.death.active) {
        return {};
    }
    const float progress = enemyDeathProgress(enemy);
    const float ramp = progress * progress * (3.0f - 2.0f * progress);
    const float amplitude = EnemyDeathShakeMaxPixels * ramp;
    if (amplitude <= 0.0f) {
        return {};
    }
    const int frame = static_cast<int>(std::floor(enemy.death.elapsedSeconds * 60.0f));
    const float x = deathUnitRandom(enemy.death.shakeSeed, frame, 0x51ed270bU) * 2.0f - 1.0f;
    const float y = deathUnitRandom(enemy.death.shakeSeed, frame, 0x68bc21ebU) * 2.0f - 1.0f;
    return {x * amplitude, y * amplitude};
}

bool canMoveLightEnemy(const Enemy& enemy)
{
    if (enemy.isBoss || hasEnemyTagAny(enemy, {"boss", "boss_only", "heavy", "large", "massive", "huge"})) {
        return false;
    }
    if (hasEnemyTagAny(enemy, {"small", "light", "tiny", "lightweight"})) {
        return true;
    }
    if (!enemy.enemyTags.empty()) {
        return false;
    }
    return effectiveEnemyRadius(enemy) <= VacuumLightEnemyFallbackRadius ||
        enemy.maxHp <= VacuumLightEnemyFallbackMaxHp;
}

bool pipeListContains(std::string_view text, std::string_view token)
{
    std::string current;
    auto flush = [&]() {
        const std::string trimmed = trimAscii(current);
        current.clear();
        return trimmed;
    };
    for (char ch : text) {
        if (ch == '|') {
            if (equalsIgnoreCaseAscii(flush(), token)) {
                return true;
            }
            continue;
        }
        current.push_back(ch);
    }
    return equalsIgnoreCaseAscii(flush(), token);
}

bool isRangedBehavior(std::string_view behaviorId)
{
    return behaviorId == "throw_stone" ||
        behaviorId == "throw_object" ||
        behaviorId == "shoot_poison" ||
        behaviorId == "shoot_web" ||
        behaviorId == "shoot_fire" ||
        behaviorId == "shoot_paralyze" ||
        behaviorId == "shoot_mud" ||
        behaviorId == "radial_spike" ||
        behaviorId == "shoot_water" ||
        behaviorId == "shoot_bubble" ||
        behaviorId == "shoot_water_bubble" ||
        behaviorId == "wind_blow";
}

bool isBubbleRangedBehavior(std::string_view behaviorId)
{
    return behaviorId == "shoot_bubble" ||
        behaviorId == "shoot_water_bubble";
}

std::string_view fallbackProjectileForBehavior(std::string_view behaviorId)
{
    if (behaviorId == "throw_stone" || behaviorId == "throw_object") {
        return "stone_bullet";
    }
    if (behaviorId == "shoot_poison") {
        return "poison_spit";
    }
    if (behaviorId == "shoot_web") {
        return "web_thread";
    }
    if (behaviorId == "shoot_fire") {
        return "fire_breath";
    }
    if (behaviorId == "shoot_paralyze") {
        return "paralyze_shot";
    }
    if (behaviorId == "shoot_mud") {
        return "mud_blob";
    }
    if (behaviorId == "radial_spike") {
        return "cactus_needle";
    }
    if (behaviorId == "shoot_water") {
        return "water_shot";
    }
    if (behaviorId == "shoot_bubble") {
        return "water_bubble";
    }
    if (behaviorId == "shoot_water_bubble") {
        return "water_shot";
    }
    if (behaviorId == "wind_blow") {
        return "wind_wave";
    }
    return "stone_bullet";
}

struct RangedEngagementRange {
    float minDistance = 0.0f;
    float maxDistance = 0.0f;
};

RangedEngagementRange rangedEngagementRange(std::string_view behaviorId)
{
    if (behaviorId == "shoot_fire") {
        return {60.0f, 170.0f};
    }
    if (behaviorId == "shoot_web" ||
        behaviorId == "shoot_poison" ||
        behaviorId == "shoot_paralyze" ||
        behaviorId == "shoot_mud") {
        return {80.0f, 230.0f};
    }
    if (behaviorId == "throw_stone" || behaviorId == "throw_object") {
        return {90.0f, 260.0f};
    }
    if (behaviorId == "shoot_water" || behaviorId == "shoot_water_bubble") {
        return {90.0f, 250.0f};
    }
    if (behaviorId == "shoot_bubble") {
        return {70.0f, 210.0f};
    }
    if (behaviorId == "wind_blow") {
        return {70.0f, 220.0f};
    }
    if (behaviorId == "radial_spike") {
        return {0.0f, 260.0f};
    }
    return {};
}

bool hasRangedEngagementRange(const RangedEngagementRange& range)
{
    return range.maxDistance > 0.0f && range.maxDistance >= range.minDistance;
}

bool isWithinRangedEngagementRange(const RangedEngagementRange& range, float distance)
{
    return !hasRangedEngagementRange(range) ||
        (distance >= range.minDistance && distance <= range.maxDistance);
}

float dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

Vec2 facingVector(float angle)
{
    return {std::cos(angle), std::sin(angle)};
}

float angleBetweenDegrees(Vec2 from, Vec2 to)
{
    const float fromLengthSq = lengthSquared(from);
    const float toLengthSq = lengthSquared(to);
    if (fromLengthSq <= 0.0001f || toLengthSq <= 0.0001f) {
        return 180.0f;
    }
    const float cosine = clamp(dot(from / std::sqrt(fromLengthSq), to / std::sqrt(toLengthSq)), -1.0f, 1.0f);
    return std::acos(cosine) * 180.0f / Pi;
}

float wrapAngle(float angle)
{
    while (angle > Pi) {
        angle -= Pi * 2.0f;
    }
    while (angle < -Pi) {
        angle += Pi * 2.0f;
    }
    return angle;
}

float rotateTowards(float current, float target, float maxStep)
{
    const float delta = wrapAngle(target - current);
    if (std::abs(delta) <= maxStep) {
        return target;
    }
    return current + (delta > 0.0f ? maxStep : -maxStep);
}

Vec2 randomDirection(std::mt19937& rng)
{
    std::uniform_real_distribution<float> angleDist(0.0f, Pi * 2.0f);
    return fromAngle(angleDist(rng));
}

bool hasClearSightLine(TileMap& map, Vec2 from, Vec2 to)
{
    const Vec2 delta = to - from;
    const float distance = length(delta);
    if (distance <= 0.0001f) {
        return true;
    }
    const Vec2 direction = delta / distance;
    const float step = std::max(2.0f, LineOfSightStep);
    for (float traveled = step; traveled < distance - step * 0.5f; traveled += step) {
        if (map.isSolidAt(from + direction * traveled)) {
            return false;
        }
    }
    return true;
}

bool canFireEnemyProjectile(const Enemy& enemy, TileMap& map, float distanceToPlayer, Vec2 playerPosition)
{
    if (enemy.rangedBehaviorId.empty()) {
        return false;
    }
    if (enemy.projectileId.empty() && enemy.rangedBehaviorId != "wind_blow") {
        return false;
    }
    if (enemy.awareness != EnemyAwarenessState::Detected) {
        return false;
    }
    const RangedEngagementRange range = rangedEngagementRange(enemy.rangedBehaviorId);
    if (!isWithinRangedEngagementRange(range, distanceToPlayer)) {
        return false;
    }
    return hasClearSightLine(map, enemy.position, playerPosition);
}

bool canUseFlowStep(TileMap& map, int tx, int ty, const FlowStep& step)
{
    if (step.dx == 0 || step.dy == 0) {
        return true;
    }
    return !map.isTileSolid(tx + step.dx, ty) && !map.isTileSolid(tx, ty + step.dy);
}

struct EnemyAltitudeProfile {
    float altitude = 0.0f;
    float bobAmplitude = 0.0f;
    float bobSpeed = 0.0f;
};

EnemyAltitudeProfile altitudeProfileForAi(std::string_view aiId)
{
    if (aiId == "hover_chase") {
        return {HoverChaseAltitude, HoverBobAmplitude, HoverBobSpeed};
    }
    if (aiId == "hover_keep_distance") {
        return {HoverKeepDistanceAltitude, HoverBobAmplitude, HoverBobSpeed};
    }
    if (aiId == "phase_wander" || aiId == "phase_chase") {
        return {PhaseAltitude, PhaseBobAmplitude, PhaseBobSpeed};
    }
    return {};
}

void configureEnemyAltitudeFromAi(Enemy& enemy)
{
    EnemyAltitudeProfile profile = altitudeProfileForAi(enemy.aiId);
    if (profile.altitude <= 0.0f && profile.bobAmplitude <= 0.0f) {
        profile = altitudeProfileForAi(enemy.unawareAiId);
    }
    enemy.hoverAltitude = profile.altitude;
    enemy.hoverBobAmplitude = profile.bobAmplitude;
    enemy.hoverBobSpeed = profile.bobSpeed;
    enemy.altitude = profile.altitude;
}

float jumpProgress(const Enemy& enemy)
{
    if (!enemy.jumpActive || enemy.jumpDurationSeconds <= 0.0f) {
        return 1.0f;
    }
    return clamp(enemy.jumpElapsedSeconds / enemy.jumpDurationSeconds, 0.0f, 1.0f);
}

float enemyJumpAltitude(const Enemy& enemy)
{
    if (!enemy.jumpActive) {
        return 0.0f;
    }
    return std::sin(jumpProgress(enemy) * Pi) * std::max(0.0f, enemy.jumpArcHeight);
}

float enemyHoverAltitude(const Enemy& enemy)
{
    const float base = std::max(0.0f, enemy.hoverAltitude);
    const float bob = enemy.hoverBobAmplitude > 0.0f && enemy.hoverBobSpeed > 0.0f
        ? std::sin(enemy.behaviorTimer * enemy.hoverBobSpeed) * enemy.hoverBobAmplitude
        : 0.0f;
    return std::max(0.0f, base + bob);
}

void updateEnemyAltitude(Enemy& enemy)
{
    enemy.altitude = std::max(0.0f, enemyHoverAltitude(enemy) + enemyJumpAltitude(enemy));
}

bool findJumpLandingPosition(
    TileMap& map,
    const Enemy& enemy,
    Vec2 direction,
    float distance,
    const EnemyPlacementCatalog* placementCatalog,
    Vec2& outPosition)
{
    if (lengthSquared(direction) <= 0.0001f || distance < JumpTargetMinDistance) {
        return false;
    }

    const Vec2 normalizedDirection = normalize(direction);
    const float radius = enemyPassageRadius(enemy, placementCatalog);
    constexpr std::array<float, 8> DistanceFactors{{1.0f, 0.88f, 0.76f, 0.64f, 0.52f, 0.40f, 0.28f, 0.16f}};
    for (float factor : DistanceFactors) {
        const Vec2 candidate = enemy.position + normalizedDirection * (distance * factor);
        if (!map.isCircleBlocked(candidate, radius)) {
            outPosition = candidate;
            return true;
        }
    }
    return false;
}

void startEnemyJumpToTarget(Enemy& enemy, Vec2 target, float durationSeconds, float arcHeight)
{
    enemy.jumpActive = true;
    enemy.jumpStartPosition = enemy.position;
    enemy.jumpTargetPosition = target;
    enemy.jumpElapsedSeconds = 0.0f;
    enemy.jumpDurationSeconds = clamp(durationSeconds, JumpAttackDurationMin, JumpAttackDurationMax);
    enemy.jumpArcHeight = std::max(0.0f, arcHeight);
    enemy.velocity = {};
}

bool beginEnemyJump(
    Enemy& enemy,
    TileMap& map,
    Vec2 direction,
    float distance,
    float durationSeconds,
    float arcHeight,
    const EnemyPlacementCatalog* placementCatalog)
{
    Vec2 target{};
    if (!findJumpLandingPosition(map, enemy, direction, distance, placementCatalog, target)) {
        return false;
    }

    startEnemyJumpToTarget(enemy, target, durationSeconds, arcHeight);
    return true;
}

bool updateBossActionJumpMotion(Enemy& enemy, float dt)
{
    if (!enemy.jumpActive) {
        updateEnemyAltitude(enemy);
        return true;
    }

    enemy.jumpElapsedSeconds = std::min(enemy.jumpDurationSeconds, enemy.jumpElapsedSeconds + std::max(0.0f, dt));
    const float t = jumpProgress(enemy);
    const Vec2 previousPosition = enemy.position;
    enemy.position = lerp(enemy.jumpStartPosition, enemy.jumpTargetPosition, t);
    enemy.velocity = {};
    if (lengthSquared(enemy.position - previousPosition) > 0.0001f) {
        enemy.facingAngle = std::atan2(enemy.position.y - previousPosition.y, enemy.position.x - previousPosition.x);
    }
    updateEnemyAltitude(enemy);
    if (t < 1.0f) {
        return false;
    }

    enemy.position = enemy.jumpTargetPosition;
    enemy.velocity = {};
    enemy.jumpActive = false;
    enemy.jumpElapsedSeconds = 0.0f;
    enemy.jumpDurationSeconds = 0.0f;
    enemy.jumpArcHeight = 0.0f;
    updateEnemyAltitude(enemy);
    return true;
}

bool frontGuardApplies(const Enemy& enemy, Vec2 hitPosition, float arcDegrees)
{
    const Vec2 facing = enemyFacingDirectionVector(enemy.facingAngle);
    const Vec2 hitDirection = normalize(hitPosition - enemy.position);
    const float clampedArc = clamp(arcDegrees, 10.0f, 360.0f);
    const float halfArcRadians = (clampedArc * 0.5f) * (Pi / 180.0f);
    const float threshold = std::cos(halfArcRadians);
    return dot(facing, hitDirection) >= threshold;
}

float captureChanceFor(const Enemy& enemy, float chanceMultiplier = 1.0f)
{
    const float hpRate = enemy.maxHp > 0
        ? clamp(static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHp), 0.0f, 1.0f)
        : 1.0f;
    const int difficulty = enemy.definition != nullptr ? std::max(0, enemy.definition->captureDifficulty) : 0;
    const float baseChance = clamp(0.15f + (1.0f - hpRate) * 0.75f - static_cast<float>(difficulty) * 0.04f, 0.05f, 0.90f);
    return clamp(baseChance * std::max(0.0f, chanceMultiplier), 0.0f, CaptureNetMaxChance);
}

std::string capturedObjectIdFor(const Enemy& enemy)
{
    return "captured_" +
        std::string(enemyVariantObjectIdSegment(enemy.variantTier)) +
        (enemy.enemyId.empty() ? std::string(DefaultEnemyId) : enemy.enemyId);
}

ItemData makeCapturedItemData(const Enemy& enemy)
{
    ItemData item;
    item.id = capturedObjectIdFor(enemy);
    item.name = enemy.enemyName;
    item.category = "\xE8\xBB\x8C\xE9\x81\x93";
    item.rarity = 1;
    item.roguelikeDropWeight = 0.0;
    item.roguelikeResidualWeight = 0.0;
    item.price = enemy.definition != nullptr
        ? balance::capturedEnemyItemBasePrice(enemy.definition->money)
        : 0;

    if (enemy.definition == nullptr) {
        item.damageType = "none";
        item.tags.push_back("no_drop");
        return item;
    }

    const EnemyDefinition& definition = *enemy.definition;
    item.visual.source = ItemVisualSource::Enemy;
    item.visual.imageNumber = definition.imageNumber;
    item.visual.sourceId = definition.id.empty() ? item.id : definition.id;
    item.visual.enemyVariantLevelBonus = enemyVariantLevelBonus(enemy.variantTier);
    item.description = definition.capturedDescription;
    item.normalEffects = definition.capturedNormalEffects;
    item.orbitEffects = definition.capturedOrbitEffects;
    item.attackPower = definition.capturedAttackPower;
    item.damageType = definition.capturedDamageType.empty() ? "none" : definition.capturedDamageType;
    const std::string normalizedDamageType = normalizeDamageType(item.damageType);
    if (normalizedDamageType.empty()) {
        if (item.damageType == "physical") {
            logError("[warning] EnemySystem: captured damage type physical is deprecated; using blunt");
            item.damageType = "blunt";
        } else {
            logError("[warning] EnemySystem: captured damage type \"" + item.damageType + "\" is invalid; using none");
            item.damageType = "none";
        }
    } else {
        if (item.damageType == "physical" && normalizedDamageType == "blunt") {
            logError("[warning] EnemySystem: captured damage type physical is deprecated; using blunt");
        }
        item.damageType = normalizedDamageType;
    }
    item.digPower = definition.capturedDigPower;
    item.durability = definition.capturedDurability;
    item.weightKg = definition.capturedWeight;
    item.tags = definition.capturedTags;
    if (std::find(item.tags.begin(), item.tags.end(), "no_drop") == item.tags.end()) {
        item.tags.push_back("no_drop");
    }
    if (enemy.variantTier != EnemyVariantTier::Normal) {
        item.tags.push_back("captured_variant");
        item.tags.push_back(enemy.variantTier == EnemyVariantTier::Abyss ? "captured_abyss" : "captured_deep");
        item.tags.push_back("codex_hidden");
    }
    item.effectText = definition.capturedEffectText;
    item.capturedBehaviorIds = definition.capturedBehaviorIds;
    item.capturedBehaviorSpecs.reserve(definition.capturedBehaviorSpecs.size());
    for (const EnemyBehaviorSpec& spec : definition.capturedBehaviorSpecs) {
        CapturedBehaviorSpec runtimeSpec;
        runtimeSpec.trigger = spec.trigger;
        runtimeSpec.behavior = spec.behavior;
        runtimeSpec.params = spec.params;
        runtimeSpec.intervalSeconds = spec.intervalSeconds;
        item.capturedBehaviorSpecs.push_back(std::move(runtimeSpec));
    }
    return item;
}

std::string visualEffectIdFor(const std::vector<EffectSpec>& specs, std::string_view damageType = {})
{
    for (const EffectSpec& spec : specs) {
        for (const std::string& effect : spec.effects) {
            if (effect == "status_poison" || effect == "status_poison_chance" ||
                effect == "status_slow" || effect == "status_slow_chance" ||
                effect == "status_glued" ||
                effect == "status_defense_down" || effect == "debuff_defense" ||
                effect == "status_paralyze" || effect == "status_paralyze_chance" ||
                effect == "status_bleed" || effect == "status_bleed_chance" ||
                effect == "status_sleep" || effect == "status_sleep_chance" ||
                effect == "status_stun" || effect == "status_stun_chance" ||
                effect == "status_confuse" || effect == "status_confuse_chance" ||
                effect == "status_blind" ||
                effect == "status_wet" ||
                effect == "status_hot" ||
                effect == "status_frozen" ||
                effect == "status_shocked" ||
                effect == "status_giant" ||
                effect == "flame_burst" ||
                effect == "bounce_grounded" ||
                effect == "fall_damage_synergy" ||
                effect == "shock_wet" ||
                effect == "conduct_water_puddle") {
                return effect;
            }
        }
    }
    if (!damageType.empty() && damageType != "none") {
        return std::string(damageType);
    }
    return {};
}

EnemyEvent makeEnemyEvent(EnemyEventType type, const Enemy& enemy, std::string effectId = {}, int damageAmount = -1, bool critical = false)
{
    return EnemyEvent{
        .type = type,
        .position = enemy.position,
        .enemyRuntimeId = enemy.id,
        .dungeonEventId = enemy.dungeonEventId,
        .enemyId = enemy.enemyId,
        .enemyName = enemy.enemyName,
        .effectId = std::move(effectId),
        .damageAmount = damageAmount,
        .critical = critical,
        .moneyDrop = enemy.moneyDrop,
    };
}

EnemyEvent makeEnemyEventAt(EnemyEventType type, const Enemy& enemy, Vec2 position, std::string effectId = {})
{
    EnemyEvent event = makeEnemyEvent(type, enemy, std::move(effectId));
    event.position = position;
    return event;
}

EnemyEvent makeTerrainEnemyEvent(
    EnemyEventType type,
    const Enemy& enemy,
    Vec2 position,
    Vec2 effectDirection,
    TileType tileType,
    Color tileColor)
{
    EnemyEvent event = makeEnemyEventAt(
        type,
        enemy,
        position,
        std::string(terrainAttributeCode(terrainAttributeForTileType(tileType))));
    event.effectDirection = effectDirection;
    event.terrainTileType = tileType;
    event.terrainColor = tileColor;
    return event;
}

EnemyEvent makeEnemyHealEvent(const Enemy& healer, const Enemy& target, int healAmount)
{
    EnemyEvent event = makeEnemyEventAt(EnemyEventType::Heal, healer, target.position);
    event.healAmount = healAmount;
    return event;
}

EnemyEvent makeEnemyHealCastEvent(const Enemy& healer)
{
    return makeEnemyEvent(EnemyEventType::HealCast, healer, std::string(EnemyHealBehaviorId));
}

Enemy* nearestWoundedEnemyForHeal(Enemy& healer, ObjectPool<Enemy, balance::MaxEnemies>& enemies)
{
    if (healer.enemyHealRadius <= 0.0f) {
        return nullptr;
    }

    const float radiusSq = healer.enemyHealRadius * healer.enemyHealRadius;
    Enemy* bestOther = nullptr;
    float bestOtherDistanceSq = radiusSq;
    Enemy* self = nullptr;
    for (Enemy& ally : enemies.items()) {
        if (!ally.active || ally.death.active || ally.hp >= ally.maxHp) {
            continue;
        }
        const float distanceSq = distanceSquared(ally.position, healer.position);
        if (distanceSq > radiusSq) {
            continue;
        }
        if (&ally == &healer) {
            self = &ally;
            continue;
        }
        if (bestOther == nullptr || distanceSq < bestOtherDistanceSq) {
            bestOther = &ally;
            bestOtherDistanceSq = distanceSq;
        }
    }
    return bestOther != nullptr ? bestOther : self;
}

bool applyEnemyHealPulse(
    Enemy& healer,
    ObjectPool<Enemy, balance::MaxEnemies>& enemies,
    std::vector<EnemyEvent>& events)
{
    if (healer.enemyHealRadius <= 0.0f || healer.enemyHealAmount <= 0.0f) {
        return false;
    }

    const float radiusSq = healer.enemyHealRadius * healer.enemyHealRadius;
    const int healAmount = std::max(1, static_cast<int>(std::round(healer.enemyHealAmount)));
    bool castEventPushed = false;
    for (Enemy& ally : enemies.items()) {
        if (!ally.active || ally.death.active || ally.hp >= ally.maxHp) {
            continue;
        }
        if (distanceSquared(ally.position, healer.position) > radiusSq) {
            continue;
        }
        const int beforeHp = ally.hp;
        ally.hp = std::min(ally.maxHp, ally.hp + healAmount);
        const int healed = ally.hp - beforeHp;
        if (healed <= 0) {
            continue;
        }
        if (!castEventPushed) {
            events.push_back(makeEnemyHealCastEvent(healer));
            castEventPushed = true;
        }
        events.push_back(makeEnemyHealEvent(healer, ally, healed));
    }
    return castEventPushed;
}

void recordObjectEffectDiscovery(
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const ObjectDefinition& object,
    std::string_view effectKey,
    std::string_view description,
    Vec2 position)
{
    if (discoveryEvents == nullptr || object.id.empty() || effectKey.empty()) {
        return;
    }
    discoveryEvents->push_back(EffectDiscoveryEvent{
        .objectId = object.id,
        .objectName = object.name,
        .effectKey = std::string(effectKey),
        .description = std::string(description),
        .note = {},
        .position = position,
    });
}

bool effectSpecsContain(const std::vector<EffectSpec>& specs, std::string_view effectId)
{
    for (const EffectSpec& spec : specs) {
        for (const std::string& effect : spec.effects) {
            if (effect == effectId) {
                return true;
            }
        }
    }
    return false;
}

bool effectSpecsContainForTarget(
    const std::vector<EffectSpec>& specs,
    std::string_view target,
    std::string_view effectId)
{
    for (const EffectSpec& spec : specs) {
        if (spec.target != target) {
            continue;
        }
        for (const std::string& effect : spec.effects) {
            if (effect == effectId) {
                return true;
            }
        }
    }
    return false;
}

struct CaptureNetSpec {
    bool active = false;
    float chanceMultiplier = 1.0f;
    float retryInterval = CaptureNetDefaultRetryInterval;
};

struct InspectEnemySpec {
    bool active = false;
    float retryInterval = InspectEnemyDefaultRetryInterval;
};

struct RingItemHitboxSpec {
    const HitboxProfile* profile = nullptr;
    Vec2 center{};
    float profileRotationRadians = 0.0f;
    float profileScale = 1.0f;
    float profileRadiusPadding = 0.0f;
    float fallbackCircleRadius = 1.0f;
};

bool captureNetTargetMatches(std::string_view target)
{
    return target == "enemy" || target == "target";
}

CaptureNetSpec collectCaptureNetSpec(const ObjectDefinition* object)
{
    CaptureNetSpec result;
    if (object == nullptr) {
        return result;
    }

    for (const EffectSpec& spec : object->orbitEffects) {
        if (!captureNetTargetMatches(spec.target)) {
            continue;
        }
        for (std::size_t i = 0; i < spec.effects.size(); ++i) {
            if (spec.effects[i] != "capture_net") {
                continue;
            }

            result.active = true;
            const double value = i < spec.values.size() ? spec.values[i] : 0.0;
            if (value > 0.0) {
                result.chanceMultiplier = std::max(result.chanceMultiplier, static_cast<float>(value));
            }
            if (spec.duration > 0.0) {
                result.retryInterval = std::min(result.retryInterval, static_cast<float>(spec.duration));
            }
        }
    }
    return result;
}

bool inspectEnemyTargetMatches(std::string_view target)
{
    return target == "enemy" || target == "target";
}

InspectEnemySpec collectInspectEnemySpec(const ObjectDefinition* object)
{
    InspectEnemySpec result;
    if (object == nullptr) {
        return result;
    }

    for (const EffectSpec& spec : object->orbitEffects) {
        if (!inspectEnemyTargetMatches(spec.target)) {
            continue;
        }
        for (std::size_t i = 0; i < spec.effects.size(); ++i) {
            if (spec.effects[i] != "inspect_enemy") {
                continue;
            }

            const double value = i < spec.values.size() ? spec.values[i] : 1.0;
            if (value <= 0.0) {
                continue;
            }
            result.active = true;
            if (spec.duration > 0.0) {
                result.retryInterval = std::min(result.retryInterval, static_cast<float>(spec.duration));
            }
        }
    }
    return result;
}

float ringItemHitboxScale(const SpellRingItem& item)
{
    const float scale = static_cast<float>(item.sizeModifier);
    return std::isfinite(scale) ? std::clamp(scale, 0.05f, 8.0f) : 1.0f;
}

float ringItemExtraHitboxPadding(const SpellRingItem& item)
{
    if (item.hasCapturedBehavior("jump_outward") && item.capturedJumpTimer > 0.0f) {
        return static_cast<float>(
            std::max(2.0, item.capturedBehaviorParamDouble("jump_outward", "landingRadius", 5.0)));
    }
    return 0.0f;
}

RingItemHitboxSpec ringItemHitboxSpec(
    const SpellRingItem& item,
    const ObjectDefinition* object,
    const HitboxCatalog* catalog,
    float totalTime,
    float radiusPadding)
{
    RingItemHitboxSpec spec;
    spec.center = ringItemDrawPosition(item, totalTime);
    spec.fallbackCircleRadius = std::max(0.0f, item.hitRadius + std::max(0.0f, radiusPadding));
    spec.profileRadiusPadding = std::max(0.0f, radiusPadding);
    spec.profileScale = ringItemHitboxScale(item);
    spec.profile = objectHitboxProfileFor(catalog, item.objectId);
    if (spec.profile != nullptr && object != nullptr) {
        ObjectImageDrawOptions baseImageOptions;
        baseImageOptions.rotationDegrees = ringItemRotationWobbleDegrees(item, totalTime);
        const ObjectImageDrawOptions imageOptions = objectRingImageOptions(
            *object,
            item.orbitOutward,
            item.worldVelocity,
            totalTime,
            baseImageOptions);
        spec.profileRotationRadians = imageOptions.rotationDegrees * (Pi / 180.0f);
    }
    return spec;
}

bool ringItemHitboxOverlapsEnemy(
    const Enemy& enemy,
    const HitboxCatalog* catalog,
    const SpellRingItem& item,
    const RingItemHitboxSpec& itemHitbox,
    Vec2 centerOffset)
{
    (void)item;
    if (itemHitbox.profile != nullptr) {
        return enemyHitboxOverlapsProfile(
            enemy,
            catalog,
            *itemHitbox.profile,
            itemHitbox.center,
            itemHitbox.profileRotationRadians,
            itemHitbox.profileScale,
            itemHitbox.profileRadiusPadding,
            centerOffset);
    }

    return enemyHitboxOverlapsCircle(
        enemy,
        catalog,
        itemHitbox.center,
        itemHitbox.fallbackCircleRadius,
        centerOffset);
}

bool ringItemHitboxOverlapsCircle(
    const SpellRingItem& item,
    const RingItemHitboxSpec& itemHitbox,
    Vec2 circleCenter,
    float circleRadius)
{
    (void)item;
    if (itemHitbox.profile != nullptr) {
        return hitboxProfileOverlapsCircle(
            *itemHitbox.profile,
            itemHitbox.center,
            itemHitbox.profileRotationRadians,
            itemHitbox.profileScale,
            itemHitbox.profileRadiusPadding,
            circleCenter,
            circleRadius);
    }

    const float radius = itemHitbox.fallbackCircleRadius + std::max(0.0f, circleRadius);
    return distanceSquared(itemHitbox.center, circleCenter) <= radius * radius;
}

bool enemyInspectionAlreadyQueued(const std::vector<EnemyEvent>& events, std::string_view enemyId)
{
    if (enemyId.empty()) {
        return false;
    }
    return std::any_of(events.begin(), events.end(), [enemyId](const EnemyEvent& event) {
        return event.type == EnemyEventType::Inspected && std::string_view(event.enemyId) == enemyId;
    });
}

bool criticalSpecTargetMatches(std::string_view target, std::string_view expected)
{
    return target == expected;
}

bool contactEnemyEffectTargetMatches(std::string_view target)
{
    return target == "enemy" || target == "target";
}

float flameBurstRadiusFromValue(double value)
{
    const double amount = std::max(1.0, value);
    return std::clamp(static_cast<float>(48.0 + (amount - 1.0) * 10.0), 42.0f, 84.0f);
}

int flameBurstDamageFromValue(double value)
{
    const double amount = std::max(1.0, value);
    return std::clamp(static_cast<int>(std::ceil(2.0 + amount * 2.0)), 1, 12);
}

void applyCriticalDamageMultiplier(CriticalDamageSpec& outSpec, double value)
{
    if (value <= 0.0) {
        return;
    }
    const double multiplier = std::clamp(value, 1.0, MaxCriticalDamageMultiplier);
    outSpec.damageMultiplier = outSpec.hasPowerOverride
        ? std::max(outSpec.damageMultiplier, multiplier)
        : multiplier;
    outSpec.hasPowerOverride = true;
}

void appendCriticalEffectsFromObject(
    const ObjectDefinition& object,
    std::string_view target,
    Vec2 position,
    CriticalDamageSpec& outSpec)
{
    for (const EffectSpec& spec : object.orbitEffects) {
        if (!criticalSpecTargetMatches(spec.target, target)) {
            continue;
        }
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            const std::string& effect = spec.effects[index];
            if (effect != "critical_chance" && effect != "critical_power" && effect != "forced_critical_hit") {
                continue;
            }
            const double value = index < spec.values.size() ? spec.values[index] : 0.0;
            if (effect == "critical_chance") {
                outSpec.chancePercent += std::max(0.0, value);
            } else if (effect == "critical_power") {
                applyCriticalDamageMultiplier(outSpec, value);
            } else {
                outSpec.forced = true;
                applyCriticalDamageMultiplier(outSpec, value);
            }
            outSpec.sources.push_back(CriticalEffectSource{
                .object = &object,
                .effectKey = effect,
                .position = position,
            });
        }
    }
}

CriticalDamageSpec collectCriticalDamageSpec(
    const SpellRingItem& hitItem,
    const ObjectDefinition* hitObject,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog)
{
    CriticalDamageSpec spec;
    if (hitObject != nullptr) {
        appendCriticalEffectsFromObject(*hitObject, "item", hitItem.worldPosition, spec);
        appendCriticalEffectsFromObject(*hitObject, "enemy", hitItem.worldPosition, spec);
    }

    const std::vector<const SpellRingItem*> runtimeItems = spellRing.runtimeItems();
    for (const SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr || itemPtr->broken() || itemPtr->objectId.empty()) {
            continue;
        }
        const ObjectDefinition* object = objectCatalog.registry.findById(itemPtr->objectId);
        if (object == nullptr) {
            continue;
        }
        appendCriticalEffectsFromObject(*object, "orbit", itemPtr->worldPosition, spec);
    }

    spec.chancePercent = spec.forced ? 100.0 : std::clamp(spec.chancePercent, 0.0, 100.0);
    if (!spec.hasPowerOverride) {
        spec.damageMultiplier = DefaultCriticalDamageMultiplier;
    }
    return spec;
}

FlameBurstHitSpec collectFlameBurstHitSpec(const ObjectDefinition* hitObject)
{
    FlameBurstHitSpec result;
    if (hitObject == nullptr) {
        return result;
    }
    for (const EffectSpec& spec : hitObject->orbitEffects) {
        if (spec.target != "area") {
            continue;
        }
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            if (spec.effects[index] != "flame_burst") {
                continue;
            }
            const double value = index < spec.values.size() ? spec.values[index] : 1.0;
            result.active = true;
            result.radius = std::max(result.radius, flameBurstRadiusFromValue(value));
            result.damage = std::min(24, result.damage + flameBurstDamageFromValue(value));
        }
    }
    return result;
}

double contactDamageMultiplierFor(
    const ObjectDefinition* hitObject,
    std::string_view effectKey,
    double maxMultiplier)
{
    double multiplier = 1.0;
    if (hitObject == nullptr) {
        return multiplier;
    }
    for (const EffectSpec& spec : hitObject->orbitEffects) {
        if (!contactEnemyEffectTargetMatches(spec.target)) {
            continue;
        }
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            if (spec.effects[index] != effectKey) {
                continue;
            }
            const double value = index < spec.values.size() ? spec.values[index] : 1.0;
            multiplier = std::max(multiplier, std::clamp(value, 1.0, maxMultiplier));
        }
    }
    return multiplier;
}

bool nonlethalHitApplies(const ObjectDefinition* hitObject)
{
    if (hitObject == nullptr) {
        return false;
    }
    for (const EffectSpec& spec : hitObject->orbitEffects) {
        if (!contactEnemyEffectTargetMatches(spec.target)) {
            continue;
        }
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            if (spec.effects[index] != "nonlethal_hit") {
                continue;
            }
            const double value = index < spec.values.size() ? spec.values[index] : 1.0;
            if (value > 0.0) {
                return true;
            }
        }
    }
    return false;
}

int clampNonlethalDamage(const Enemy& enemy, int damage)
{
    if (damage <= 0) {
        return 0;
    }
    return std::min(damage, std::max(0, enemy.hp - 1));
}

BounceGroundedHitSpec collectBounceGroundedHitSpec(const ObjectDefinition* hitObject)
{
    BounceGroundedHitSpec result;
    if (hitObject == nullptr) {
        return result;
    }
    for (const EffectSpec& spec : hitObject->orbitEffects) {
        if (!contactEnemyEffectTargetMatches(spec.target)) {
            continue;
        }
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            const double value = index < spec.values.size() ? spec.values[index] : 1.0;
            if (spec.effects[index] == "bounce_grounded") {
                result.active = true;
                result.strength = std::max(result.strength, std::max(0.0, value));
            } else if (spec.effects[index] == "fall_damage_synergy") {
                result.fallDamageActive = true;
                result.fallDamageMultiplier = std::max(
                    result.fallDamageMultiplier,
                    value > 0.0 ? value : 1.0);
            }
        }
    }
    result.strength = std::clamp(result.strength, 0.0, ExternalBounceMaxStrength);
    result.fallDamageMultiplier = std::clamp(result.fallDamageMultiplier, 1.0, FallDamageSynergyMaxMultiplier);
    return result;
}

ShockWetHitSpec collectShockWetHitSpec(const ObjectDefinition* hitObject)
{
    ShockWetHitSpec result;
    if (hitObject == nullptr) {
        return result;
    }
    for (const EffectSpec& spec : hitObject->orbitEffects) {
        if (!contactEnemyEffectTargetMatches(spec.target)) {
            continue;
        }
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            if (spec.effects[index] != "shock_wet") {
                continue;
            }
            const double value = index < spec.values.size() ? spec.values[index] : 1.0;
            result.active = true;
            result.value = std::max(result.value, value > 0.0 ? value : 1.0);
            result.duration = std::max(
                result.duration,
                spec.duration > 0.0 ? spec.duration : ShockedDefaultDurationSeconds);
        }
    }
    result.value = std::clamp(result.value, 0.25, 5.0);
    result.duration = std::clamp(result.duration, 0.1, 6.0);
    return result;
}

void queueStatusPopupEvent(
    std::vector<StatusPopupEvent>& events,
    Vec2 position,
    std::string_view stateId,
    StatusPopupTarget target,
    const EntityStateApplyResult& result)
{
    if (!shouldShowEntityStatusPopup(result) || entityStatusDisplayName(stateId).empty()) {
        return;
    }
    events.push_back(StatusPopupEvent{
        .position = position,
        .stateId = std::string(stateId),
        .target = target,
    });
}

bool applyShockedStateToEnemy(
    Enemy& enemy,
    double value,
    double duration,
    std::string_view source,
    EntityStateApplyResult* outResult = nullptr)
{
    if (enemy.isBoss) {
        if (outResult != nullptr) {
            *outResult = {};
        }
        return false;
    }
    const EntityStateApplyResult result = enemy.status.applyState(
        "status_shocked",
        std::clamp(value, 0.25, 5.0),
        std::clamp(duration, 0.1, 6.0),
        std::string(source),
        StateApplyMode::KeepLonger);
    if (outResult != nullptr) {
        *outResult = result;
    }
    enemy.hitFlash = 0.12f;
    return true;
}

void clearExternalBounceState(Enemy& enemy)
{
    enemy.externalBounceActive = false;
    enemy.externalBounceFallDamage = 0.0f;
    enemy.externalBounceFallDamageMultiplier = 1.0f;
}

bool canExternalBounceGroundedEnemy(const Enemy& enemy)
{
    if (enemy.isBoss ||
        enemy.jumpActive ||
        enemy.knockbackTimer > 0.0f ||
        enemy.hoverAltitude > 0.0f ||
        enemy.altitude > ExternalBounceGroundedAltitudeEpsilon ||
        enemy.status.hasState("status_frozen")) {
        return false;
    }
    if (hasEnemyTagAny(enemy, {"boss", "boss_only", "flying", "hover", "airborne", "floating", "phase"})) {
        return false;
    }
    return true;
}

int externalBounceFallDamage(double strength, float arcHeight)
{
    const double rawDamage = 3.0 + strength * 3.0 + static_cast<double>(arcHeight) * 0.08;
    return std::clamp(static_cast<int>(std::ceil(rawDamage)), 3, 16);
}

bool beginExternalGroundBounce(
    Enemy& enemy,
    TileMap& map,
    Vec2 hitOrigin,
    const BounceGroundedHitSpec& spec,
    const EnemyPlacementCatalog* placementCatalog)
{
    if (!spec.active || !canExternalBounceGroundedEnemy(enemy)) {
        return false;
    }

    const double strength = std::clamp(spec.strength <= 0.0 ? 1.0 : spec.strength, 0.25, ExternalBounceMaxStrength);
    Vec2 direction = enemy.position - hitOrigin;
    if (lengthSquared(direction) <= 0.0001f) {
        direction = facingVector(enemy.facingAngle);
    }
    if (lengthSquared(direction) <= 0.0001f) {
        direction = Vec2{0.0f, -1.0f};
    }

    const float radiusLift = enemyPassageRadius(enemy, placementCatalog) * 0.35f;
    const float arcHeight = std::clamp(
        static_cast<float>(34.0 + strength * 18.0) + radiusLift,
        ExternalBounceMinArcHeight,
        ExternalBounceMaxArcHeight);
    const float duration = std::clamp(
        static_cast<float>(0.42 + strength * 0.05),
        ExternalBounceMinDuration,
        ExternalBounceMaxDuration);
    const float distance = std::clamp(
        static_cast<float>(10.0 + strength * 8.0),
        ExternalBounceMinDistance,
        ExternalBounceMaxDistance);

    Vec2 target = enemy.position;
    (void)findJumpLandingPosition(map, enemy, direction, distance, placementCatalog, target);
    startEnemyJumpToTarget(enemy, target, duration, arcHeight);
    enemy.knockbackVelocity = {};
    enemy.knockbackTimer = 0.0f;
    enemy.externalBounceActive = true;
    enemy.externalBounceFallDamage = spec.fallDamageActive
        ? static_cast<float>(externalBounceFallDamage(strength, arcHeight))
        : 0.0f;
    enemy.externalBounceFallDamageMultiplier = static_cast<float>(spec.fallDamageMultiplier);
    updateEnemyAltitude(enemy);
    return true;
}

float externalBounceRotationDegrees(const Enemy& enemy)
{
    if (!enemy.externalBounceActive || !enemy.jumpActive) {
        return 0.0f;
    }
    const float direction = enemy.id % 2 == 0 ? -1.0f : 1.0f;
    return direction * jumpProgress(enemy) * 720.0f;
}

bool criticalRollSucceeds(double chancePercent, std::mt19937& rng)
{
    if (chancePercent <= 0.0) {
        return false;
    }
    if (chancePercent >= 100.0) {
        return true;
    }
    std::uniform_real_distribution<double> dist(0.0, 100.0);
    return dist(rng) < chancePercent;
}

bool discoveryQueueContainsEffect(
    const std::vector<EffectDiscoveryEvent>* discoveryEvents,
    std::string_view objectId,
    std::string_view effectKey)
{
    if (discoveryEvents == nullptr || objectId.empty() || effectKey.empty()) {
        return false;
    }
    for (const EffectDiscoveryEvent& event : *discoveryEvents) {
        if (event.objectId == objectId && event.effectKey == effectKey) {
            return true;
        }
    }
    return false;
}

void recordCriticalEffectDiscoveries(
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia,
    const std::vector<CriticalEffectSource>& sources,
    Vec2 fallbackPosition)
{
    if (discoveryEvents == nullptr) {
        return;
    }
    for (const CriticalEffectSource& source : sources) {
        if (source.object == nullptr || source.object->id.empty() || source.effectKey.empty()) {
            continue;
        }
        if (encyclopedia != nullptr && encyclopedia->hasObjectEffect(source.object->id, source.effectKey)) {
            continue;
        }
        if (discoveryQueueContainsEffect(discoveryEvents, source.object->id, source.effectKey)) {
            continue;
        }
        discoveryEvents->push_back(EffectDiscoveryEvent{
            .objectId = source.object->id,
            .objectName = source.object->name,
            .effectKey = source.effectKey,
            .description = {},
            .note = {},
            .position = source.position.x != 0.0f || source.position.y != 0.0f ? source.position : fallbackPosition,
        });
    }
}

void dispatchCapturedContactEffect(
    const SpellRingItem& item,
    const ObjectDefinition& object,
    Enemy& enemy,
    Player& player,
    SpellRingSystem& spellRing,
    const EffectDispatcher& effectDispatcher,
    Vec2 hitPosition,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia,
    std::vector<StatusPopupEvent>* statusPopupEvents)
{
    EffectContext context;
    context.sourceObject = &object;
    context.owner = &player;
    context.targetEntity = &enemy;
    context.hitTarget = &enemy;
    context.orbit = &spellRing;
    context.statusPopupEvents = statusPopupEvents;
    context.discoveryEvents = discoveryEvents;
    context.encyclopedia = encyclopedia;
    context.position = hitPosition;
    context.triggerType = EffectTriggerType::Hit;
    context.logUnimplementedEffects = false;

    if (item.hasCapturedBehavior("contact_slow") && !effectSpecsContain(object.orbitEffects, "status_slow")) {
        const double slowMultiplier = clamp(item.capturedBehaviorParamDouble("contact_slow", "speedMultiplier", 0.85), 0.05, 1.0);
        const double slowDuration = std::max(0.1, item.capturedBehaviorParamDouble("contact_slow", "duration", 1.5));
        EffectSpec slow;
        slow.target = "enemy";
        slow.effects = {"status_slow"};
        slow.values = {slowMultiplier};
        slow.duration = slowDuration;
        effectDispatcher.dispatch({slow}, context);
    }

    if (item.hasCapturedBehavior("rust_debuff") &&
        !effectSpecsContain(object.orbitEffects, "status_defense_down") &&
        !effectSpecsContain(object.orbitEffects, "debuff_defense")) {
        const double defenseMultiplier = clamp(item.capturedBehaviorParamDouble("rust_debuff", "defenseMultiplier", 0.8), 0.05, 1.0);
        const double debuffDuration = std::max(0.1, item.capturedBehaviorParamDouble("rust_debuff", "duration", 4.0));
        EffectSpec rust;
        rust.target = "enemy";
        rust.effects = {"status_defense_down"};
        rust.values = {defenseMultiplier};
        rust.duration = debuffDuration;
        effectDispatcher.dispatch({rust}, context);
    }
}

bool capturedRewardAllowed(SpellRingItem& item, const Enemy& enemy, float totalTime)
{
    if (enemy.isBoss && item.capturedBossRewardCount >= CapturedBossRewardLimit) {
        return false;
    }
    float interval = CapturedRewardCooldown;
    if (item.hasCapturedBehavior("reward_drop")) {
        interval = std::max(interval, static_cast<float>(item.capturedBehaviorInterval("reward_drop", CapturedRewardCooldown)));
    }
    if (item.hasCapturedBehavior("steal_or_dig")) {
        interval = std::max(interval, static_cast<float>(item.capturedBehaviorInterval("steal_or_dig", CapturedRewardCooldown)));
    }
    if (totalTime - item.capturedRewardLastTime < interval) {
        return false;
    }
    if (totalTime - item.capturedRewardWindowStart > CapturedRewardWindowSeconds) {
        item.capturedRewardWindowStart = totalTime;
        item.capturedRewardWindowCount = 0;
    }
    if (item.capturedRewardWindowCount >= CapturedRewardWindowLimit) {
        return false;
    }
    return true;
}

bool rollCapturedReward(float chance)
{
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng) <= chance;
}

std::string capturedRewardProfile(const SpellRingItem& item)
{
    if (item.hasCapturedBehavior("reward_drop")) {
        return item.capturedBehaviorParamString("reward_drop", "profile", "common");
    }
    return "common";
}

void recordCapturedBehaviorUse(SpellRingItem& item, const Enemy& enemy, float totalTime)
{
    item.capturedRewardLastTime = totalTime;
    if (totalTime - item.capturedRewardWindowStart > CapturedRewardWindowSeconds) {
        item.capturedRewardWindowStart = totalTime;
        item.capturedRewardWindowCount = 0;
    }
    ++item.capturedRewardWindowCount;
    if (enemy.isBoss) {
        ++item.capturedBossRewardCount;
    }
}

void recordCapturedReward(SpellRingItem& item, const Enemy& enemy, float totalTime, Vec2 position, std::vector<EnemyEvent>& events)
{
    recordCapturedBehaviorUse(item, enemy, totalTime);
    EnemyEvent event;
    event.type = EnemyEventType::RewardDrop;
    event.position = position;
    event.objectDropProfile = capturedRewardProfile(item);
    events.push_back(std::move(event));
}

void tryCapturedRewardFromEnemy(SpellRingItem& item, const Enemy& enemy, float totalTime, std::vector<EnemyEvent>& events)
{
    if (!item.hasCapturedBehavior("reward_drop")) {
        return;
    }
    const float chance = static_cast<float>(std::clamp(item.capturedBehaviorParamDouble("reward_drop", "chance", CapturedRewardChanceEnemy), 0.0, 1.0));

    if (!capturedRewardAllowed(item, enemy, totalTime) || !rollCapturedReward(chance)) {
        return;
    }

    recordCapturedReward(item, enemy, totalTime, enemy.position, events);
}

Color colorForEnemy(const Enemy& enemy)
{
    std::uint32_t hash = 2166136261u;
    const std::string_view key = enemy.enemyId.empty() ? std::string_view(DefaultEnemyId) : std::string_view(enemy.enemyId);
    for (unsigned char ch : key) {
        hash ^= ch;
        hash *= 16777619u;
    }

    const auto channel = [hash](int shift) {
        return static_cast<unsigned char>(80 + ((hash >> shift) & 0x7F));
    };
    return Color{channel(0), channel(8), channel(16), 255};
}

Color scaleColorAlpha(Color color, float scale)
{
    const float alpha = static_cast<float>(color.a) * std::max(0.0f, scale);
    color.a = static_cast<unsigned char>(std::clamp(std::lround(alpha), 0L, 255L));
    return color;
}

void revealEnemyHpBar(Enemy& enemy, int damage)
{
    if (damage <= 0 || enemy.isBoss || enemy.maxHp <= 0 || enemy.hp <= 0 || enemy.hp >= enemy.maxHp) {
        return;
    }
    enemy.hpBarTimer = EnemyHpBarDisplaySeconds;
}

void applyEnemyDamage(Enemy& enemy, int damage)
{
    if (damage <= 0) {
        return;
    }
    enemy.hp -= damage;
    enemy.dungeonEventSleeping = false;
    enemy.status.removeState("status_sleep");
}

bool fireDamageRemovesFrozen(std::string_view damageType)
{
    return damageType == "fire" ||
        damageType == "flame" ||
        damageType == "burn" ||
        damageType == "break_fire_burst" ||
        damageType == "flame_burst" ||
        damageType == "hot_air" ||
        damageType == "status_hot" ||
        damageType == "dry_wet_bonus_damage";
}

void applyEnemyDamageTyped(Enemy& enemy, int damage, std::string_view damageType)
{
    if (damage > 0 && fireDamageRemovesFrozen(damageType)) {
        enemy.status.removeState("status_frozen");
        enemy.coldExposure = 0.0f;
        enemy.coldExposureTouched = false;
    }
    applyEnemyDamage(enemy, damage);
}

float stunWakeHopOffset(float stunWakeTimer)
{
    if (stunWakeTimer <= 0.0f) {
        return 0.0f;
    }
    const float t = clamp(stunWakeTimer / StunWakeHopSeconds, 0.0f, 1.0f);
    return std::sin(t * Pi) * StunWakeHopPixels;
}

ActorVisualPose enemyActionVisualPose(const Enemy& enemy)
{
    if (!enemy.action.active || enemy.action.animationId.empty()) {
        return actorVisualPoseIdentity();
    }
    return sampleActorVisualMotion(enemy.action.animationId, enemy.action.elapsedSeconds);
}

float enemyVisualAltitude(const Enemy& enemy)
{
    const ActorVisualPose pose = enemyActionVisualPose(enemy);
    return std::max(0.0f, enemy.altitude + pose.visualAltitude);
}

Vec2 enemyBaseDrawPosition(const Enemy& enemy)
{
    const ActorVisualPose pose = enemyActionVisualPose(enemy);
    const Vec2 facingOffset = facingVector(enemy.facingAngle) * pose.forwardOffset;
    return elevatedDrawPosition(enemy.position, std::max(0.0f, enemy.altitude + pose.visualAltitude)) +
        pose.offset +
        facingOffset +
        entityStatusJitterOffset(enemy.status, enemy.behaviorTimer) +
        enemyDeathShakeOffset(enemy) +
        Vec2{0.0f, -stunWakeHopOffset(enemy.stunWakeTimer)};
}

Vec2 enemyDrawPosition(const Enemy& enemy, const EnemyPlacementCatalog* placementCatalog)
{
    return enemyBaseDrawPosition(enemy) + enemyVisualOffset(enemy, placementCatalog);
}

EnemyImageDrawOptions enemyImageOptionsFor(const Enemy& enemy)
{
    EnemyImageDrawOptions imageOptions;
    const ActorVisualPose pose = enemyActionVisualPose(enemy);
    const EntityStatusVisualStyle statusVisual = entityStatusVisualStyle(enemy.status);
    imageOptions.tint = {255, 255, 255, 255};
    imageOptions.scaleMultiplier = statusVisual.scaleMultiplier;
    imageOptions.stretchScale = pose.scale;
    imageOptions.flipY = statusVisual.flipVertical;
    imageOptions.rotationDegrees = externalBounceRotationDegrees(enemy) + pose.rotationDegrees;
    if (enemy.hitFlash > 0.0f) {
        const float flash = clamp(enemy.hitFlash / 0.12f, 0.0f, 1.0f);
        imageOptions.maskOverlayColor = {255, 255, 255, static_cast<unsigned char>(std::round(220.0f * flash))};
    }
    if (enemy.hitFlash <= 0.0f && statusVisual.hasTint) {
        imageOptions.tint = statusVisual.tint;
    }
    if (enemy.hitFlash <= 0.0f && !enemy.death.active && enemy.variantTier != EnemyVariantTier::Normal) {
        imageOptions.tint = multiplyRgb(imageOptions.tint, enemyVariantTintMultiplier(enemy.variantTier));
    }
    if (enemy.death.active) {
        const unsigned char shade = enemyDeathShade(enemy);
        imageOptions.tint = {shade, shade, shade, 255};
        imageOptions.maskOverlayColor = {0, 0, 0, 0};
    }
    return imageOptions;
}

Vec2 enemyVisualBoundsSize(Renderer& renderer, const Enemy& enemy)
{
    if (enemy.spawnTimer > 0.0f) {
        if (enemy.spawnVisualKind == EnemySpawnVisualKind::WalkIn) {
            Vec2 imageSize{};
            if (enemyImageDrawSize(renderer, enemy, enemyImageOptionsFor(enemy), imageSize)) {
                return imageSize;
            }
            const float visualRadius = enemyVisualRadius(enemy);
            const float diameter = (visualRadius + 3.0f) * 2.0f;
            return {diameter, diameter};
        }
        if (enemy.spawnVisualKind == EnemySpawnVisualKind::GroundEmerge) {
            Vec2 imageSize{};
            const float visualRadius = enemyVisualRadius(enemy);
            if (enemyImageDrawSize(renderer, enemy, enemyImageOptionsFor(enemy), imageSize)) {
                return {
                    std::max(imageSize.x, visualRadius * 2.8f),
                    std::max(imageSize.y + visualRadius * 1.2f, visualRadius * 3.4f),
                };
            }
            const float diameter = visualRadius * 3.4f;
            return {diameter, diameter};
        }
        const float ratio = enemy.spawnDuration > 0.0f ? enemy.spawnTimer / enemy.spawnDuration : 0.0f;
        const float pulse = 1.0f + (1.0f - ratio) * 0.9f;
        const float visualRadius = enemyVisualRadius(enemy);
        const float diameter = (visualRadius * pulse + 4.0f) * 2.0f;
        return {diameter, diameter};
    }

    if (isAstragnaBossAction(enemy)) {
        const float diameter = astragnaOuterVisualRadius(enemy) * 2.0f;
        return {diameter, diameter};
    }

    Vec2 imageSize{};
    if (enemyImageDrawSize(renderer, enemy, enemyImageOptionsFor(enemy), imageSize)) {
        return imageSize;
    }

    const float visualRadius = enemyVisualRadius(enemy);
    const float diameter = (visualRadius + 3.0f) * 2.0f;
    return {diameter, diameter};
}

float enemyShadowVisualSize(Renderer& renderer, const Enemy& enemy)
{
    const Vec2 visualSize = enemyVisualBoundsSize(renderer, enemy);
    const float baseSize = std::max(1.0f, std::max(visualSize.x, visualSize.y));
    return actorShadowVisualSizeForAltitude(baseSize, enemyVisualAltitude(enemy));
}


Vec2 enemyShadowBoundsSize(Renderer& renderer, const Enemy& enemy, const EnemyShadowSpec& shadow)
{
    const float visualSize = enemyShadowVisualSize(renderer, enemy);
    return {visualSize * 0.55f * shadow.scale.x, visualSize * 0.25f * shadow.scale.y};
}

Vec2 enemyShadowAnchor(
    const Enemy& enemy,
    const EnemyPlacementCatalog* placementCatalog,
    const EnemyShadowSpec& shadow)
{
    return enemy.position + enemyVisualOffset(enemy, placementCatalog) + shadow.offset;
}

void drawEnemyHpBar(Renderer& renderer, const Enemy& enemy, Vec2 drawPosition, float uiVisualRadius, bool detailsKnown)
{
    if (enemy.death.active || enemy.isBoss || enemy.maxHp <= 0 || enemy.hp <= 0) {
        return;
    }

    if (!detailsKnown) {
        return;
    }

    const float fade = 1.0f;

    const float hpRatio = clamp(static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHp), 0.0f, 1.0f);
    const float barWidth = std::clamp(uiVisualRadius * 1.65f, EnemyHpBarMinWidth, EnemyHpBarMaxWidth);
    const Vec2 barPos = drawPosition + Vec2{-barWidth * 0.5f, -uiVisualRadius - 12.0f};

    UiGaugeStyle hpGaugeStyle;
    hpGaugeStyle.fill.start = scaleColorAlpha({230, 58, 82, 255}, fade);
    hpGaugeStyle.fill.end = scaleColorAlpha({255, 124, 96, 255}, fade);
    hpGaugeStyle.track = scaleColorAlpha({14, 8, 16, 215}, fade);
    hpGaugeStyle.trackInner = scaleColorAlpha({44, 18, 28, 205}, fade);
    hpGaugeStyle.trackOuter = scaleColorAlpha({255, 220, 220, 54}, fade);
    hpGaugeStyle.shadow = scaleColorAlpha({0, 0, 0, 92}, fade);
    hpGaugeStyle.highlight = scaleColorAlpha({255, 238, 220, 62}, fade);
    hpGaugeStyle.outline = scaleColorAlpha({180, 167, 127, 255}, fade);
    hpGaugeStyle.outerOutline = scaleColorAlpha({0, 0, 0, 255}, fade);
    hpGaugeStyle.trackOuterExtra = 1.0f;
    hpGaugeStyle.trackInnerInset = 1.5f;
    hpGaugeStyle.shadowOffsetY = 1.0f;
    hpGaugeStyle.shadowExtra = 2.0f;

    drawUiGauge(renderer, {barPos, {barWidth, EnemyHpBarHeight}}, hpRatio, hpGaugeStyle);
}

enum class BossWeakPointKind {
    None,
    StardustMoleCrystal,
    JunkCrabHead,
};

struct BossWeakPointSpec {
    BossWeakPointKind kind = BossWeakPointKind::None;
    bool exposed = false;
    Vec2 center{};
    float radius = 0.0f;
    std::string_view effectId;
};

struct BossDamageAdjustment {
    int damage = 0;
    bool weakPointHit = false;
    std::string_view effectId;
};

struct RingContactDamageResult {
    std::string_view damageType = "none";
    int damageDealt = 0;
    bool criticalHit = false;
    bool sleepingBonusApplied = false;
    bool dryWetBonusApplied = false;
    bool nonlethalHit = false;
    bool frontGuarded = false;
    bool weakPointHit = false;
    std::string_view bossEffectId;
};

bool stardustMoleWeakPointExposed(const Enemy& enemy)
{
    return enemy.active &&
        enemy.isBoss &&
        isStardustMoleEnemy(enemy) &&
        !enemy.bossAction.hidden &&
        !enemy.bossAction.invulnerable &&
        enemy.spawnTimer <= 0.0f &&
        enemy.hp > 0;
}

bool junkCrabWeakPointExposed(const Enemy& enemy)
{
    const JunkCrabBossRuntime& crab = enemy.bossAction.junkCrab;
    return enemy.active &&
        enemy.isBoss &&
        enemy.bossAction.enabled &&
        enemy.bossAction.pattern == JunkCrabPatternId &&
        std::none_of(
            crab.debris.begin(),
            crab.debris.end(),
            [](const JunkCrabDebrisRuntime& debris) {
                return debris.state == JunkCrabDebrisState::Orbiting;
            }) &&
        !enemy.bossAction.hidden &&
        !enemy.bossAction.invulnerable &&
        enemy.spawnTimer <= 0.0f &&
        enemy.hp > 0;
}

bool applyCustomBossWeakPoint(
    BossWeakPointSpec& spec,
    const Enemy& enemy,
    const HitboxCatalog* hitboxCatalog,
    Vec2 centerOffset);

BossWeakPointSpec bossWeakPointFor(
    const Enemy& enemy,
    const HitboxCatalog* hitboxCatalog,
    const EnemyPlacementCatalog* placementCatalog)
{
    const Vec2 centerOffset = enemyVisualOffset(enemy, placementCatalog);
    const Vec2 hitboxCenter = enemy.position + centerOffset;
    if (stardustMoleWeakPointExposed(enemy)) {
        const float radius = effectiveEnemyRadius(enemy);
        BossWeakPointSpec spec{
            .kind = BossWeakPointKind::StardustMoleCrystal,
            .exposed = true,
            .center = hitboxCenter - facingVector(enemy.facingAngle) * (radius * 0.52f),
            .radius = std::max(7.0f, radius * 0.42f),
            .effectId = "stardust_crystal",
        };
        applyCustomBossWeakPoint(spec, enemy, hitboxCatalog, centerOffset);
        return spec;
    }

    if (junkCrabWeakPointExposed(enemy)) {
        const float radius = effectiveEnemyRadius(enemy);
        BossWeakPointSpec spec{
            .kind = BossWeakPointKind::JunkCrabHead,
            .exposed = true,
            .center = hitboxCenter + facingVector(enemy.facingAngle) * (radius * 0.72f),
            .radius = std::max(10.0f, radius * 0.46f),
            .effectId = "junk_head",
        };
        applyCustomBossWeakPoint(spec, enemy, hitboxCatalog, centerOffset);
        return spec;
    }

    return {};
}

bool bossWeakPointHit(const BossWeakPointSpec& weakPoint, Vec2 hitPosition, float hitRadius)
{
    return weakPoint.exposed &&
        weakPoint.radius > 0.0f &&
        circlesOverlap(weakPoint.center, weakPoint.radius, hitPosition, std::max(0.0f, hitRadius));
}

bool bossWeakPointHit(const BossWeakPointSpec& weakPoint, const SpellRingItem& item, const RingItemHitboxSpec& itemHitbox)
{
    return weakPoint.exposed &&
        weakPoint.radius > 0.0f &&
        ringItemHitboxOverlapsCircle(item, itemHitbox, weakPoint.center, weakPoint.radius);
}

int scaledPositiveDamage(int damage, double multiplier)
{
    if (damage <= 0) {
        return damage;
    }
    const double safeMultiplier = std::max(0.0, multiplier);
    if (safeMultiplier <= 0.0) {
        return 0;
    }
    return std::max(1, static_cast<int>(std::ceil(static_cast<double>(damage) * safeMultiplier)));
}

BossDamageAdjustment adjustBossIncomingDamageWithWeakPoint(
    const Enemy& enemy,
    int damage,
    const BossWeakPointSpec& weakPoint,
    bool weakPointHit)
{
    BossDamageAdjustment result{.damage = damage};
    if (damage <= 0) {
        return result;
    }

    if (weakPointHit) {
        result.damage = scaledPositiveDamage(damage, BossWeakPointIncomingDamageMultiplier);
        result.weakPointHit = true;
        result.effectId = weakPoint.effectId;
        return result;
    }

    if (enemy.isBoss) {
        result.damage = scaledPositiveDamage(damage, BossNormalIncomingDamageMultiplier);
    }
    return result;
}

bool applyCustomBossWeakPoint(
    BossWeakPointSpec& spec,
    const Enemy& enemy,
    const HitboxCatalog* hitboxCatalog,
    Vec2 centerOffset)
{
    const HitboxProfile* profile = bossWeakPointProfileFor(hitboxCatalog, enemy);
    if (profile == nullptr || profile->circles.empty()) {
        return false;
    }

    const HitCircle circle = profile->circles.front();
    const float scale = std::max(0.0f, static_cast<float>(enemy.status.sizeMultiplierFromStates()));
    spec.center = enemy.position + centerOffset + circle.offset * scale;
    spec.radius = std::max(1.0f, circle.radius * scale);
    return true;
}

BossDamageAdjustment adjustBossIncomingDamage(
    const Enemy& enemy,
    int damage,
    Vec2 hitPosition,
    float hitRadius,
    const HitboxCatalog* hitboxCatalog,
    const EnemyPlacementCatalog* placementCatalog)
{
    const BossWeakPointSpec weakPoint = bossWeakPointFor(enemy, hitboxCatalog, placementCatalog);
    return adjustBossIncomingDamageWithWeakPoint(
        enemy,
        damage,
        weakPoint,
        bossWeakPointHit(weakPoint, hitPosition, hitRadius));
}

BossDamageAdjustment adjustBossIncomingDamage(
    const Enemy& enemy,
    int damage,
    const SpellRingItem& item,
    const RingItemHitboxSpec& itemHitbox,
    const HitboxCatalog* hitboxCatalog,
    const EnemyPlacementCatalog* placementCatalog)
{
    const BossWeakPointSpec weakPoint = bossWeakPointFor(enemy, hitboxCatalog, placementCatalog);
    return adjustBossIncomingDamageWithWeakPoint(
        enemy,
        damage,
        weakPoint,
        bossWeakPointHit(weakPoint, item, itemHitbox));
}

RingContactDamageResult computeRingContactDamageAgainstEnemy(
    const Enemy& enemy,
    const SpellRingItem& item,
    const ObjectDefinition* hitObject,
    const RingItemHitboxSpec& itemHitbox,
    const Player& player,
    const SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const HitboxCatalog* hitboxCatalog,
    const EnemyPlacementCatalog* placementCatalog,
    std::mt19937& rng,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia,
    bool applyNonlethalCap)
{
    RingContactDamageResult result;
    result.damageType = item.damageType;
    if (item.magicAuraTimer > 0.0f && !item.magicAuraDamageType.empty()) {
        result.damageType = item.magicAuraDamageType;
    }

    const double ringOutputMultiplier = spellRing.ringOutputMultiplierForRing(item.ringIndex);
    const int speedBonus = static_cast<int>(
        item.damageMotionSpeed *
        0.25f *
        static_cast<float>(
            spellRing.speedDamageMultiplier() *
            spellRing.ringDamageSpeedMultiplierForRing(item.ringIndex)));
    const int modifiedDamage = static_cast<int>(
        player.status.applyModifiers(
            ModifierStat::Attack,
            static_cast<double>(item.damage) *
                damageTypeMultiplier(result.damageType) *
                item.slashDamageMultiplier *
                spellRing.effectivePowerMultiplier()));
    const int rawDamage = static_cast<int>(
        std::ceil(static_cast<double>(modifiedDamage + speedBonus) * ringOutputMultiplier));
    int adjustedDamage = rawDamage;
    const CriticalDamageSpec criticalSpec = collectCriticalDamageSpec(item, hitObject, spellRing, objectCatalog);
    if (rawDamage > 0 && criticalRollSucceeds(criticalSpec.chancePercent, rng)) {
        result.criticalHit = true;
        adjustedDamage = static_cast<int>(
            std::ceil(static_cast<double>(adjustedDamage) * criticalSpec.damageMultiplier));
        recordCriticalEffectDiscoveries(discoveryEvents, encyclopedia, criticalSpec.sources, enemy.position);
    }

    const double sleepingBonusMultiplier = contactDamageMultiplierFor(
        hitObject,
        "sleeping_bonus_damage",
        MaxSleepingBonusDamageMultiplier);
    result.sleepingBonusApplied = adjustedDamage > 0 &&
        sleepingBonusMultiplier > 1.0 &&
        enemy.status.hasState("status_sleep");
    if (result.sleepingBonusApplied) {
        adjustedDamage = static_cast<int>(
            std::ceil(static_cast<double>(adjustedDamage) * sleepingBonusMultiplier));
    }

    const double dryWetMultiplier = contactDamageMultiplierFor(
        hitObject,
        "dry_wet_bonus_damage",
        MaxDryWetDamageMultiplier);
    result.dryWetBonusApplied = adjustedDamage > 0 &&
        dryWetMultiplier > 1.0 &&
        enemy.status.hasState("status_wet");
    if (result.dryWetBonusApplied) {
        adjustedDamage = static_cast<int>(
            std::ceil(static_cast<double>(adjustedDamage) * dryWetMultiplier));
    }

    if (isPhysicalDamageType(result.damageType) && hasBehavior(enemy, "physical_resist")) {
        adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * enemy.physicalDamageMultiplier));
    }
    if (isPhysicalDamageType(result.damageType) && hasBehavior(enemy, "magic_body")) {
        adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * enemy.magicBodyPhysicalMultiplier));
    } else if (result.damageType == "magic" && hasBehavior(enemy, "magic_body")) {
        adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * enemy.magicBodyMagicMultiplier));
    }

    result.frontGuarded = !result.criticalHit &&
        hasBehavior(enemy, "front_guard") &&
        enemy.frontGuardDamageMultiplier < 0.999f &&
        frontGuardApplies(enemy, item.worldPosition, enemy.frontGuardArcDegrees);
    if (result.frontGuarded) {
        adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * enemy.frontGuardDamageMultiplier));
    }

    const BossDamageAdjustment bossDamage = adjustBossIncomingDamage(
        enemy,
        adjustedDamage,
        item,
        itemHitbox,
        hitboxCatalog,
        placementCatalog);
    result.weakPointHit = bossDamage.weakPointHit;
    result.bossEffectId = bossDamage.effectId;
    adjustedDamage = bossDamage.damage;

    result.nonlethalHit = applyNonlethalCap && nonlethalHitApplies(hitObject);
    result.damageDealt = applyDefenseModifier(enemy.status, adjustedDamage);
    if (result.nonlethalHit) {
        result.damageDealt = clampNonlethalDamage(enemy, result.damageDealt);
    }
    return result;
}

std::uint32_t mixBossWeakPointHintSeed(std::uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

float bossWeakPointHintRandom(std::uint32_t seed, std::uint32_t salt)
{
    const std::uint32_t mixed = mixBossWeakPointHintSeed(seed ^ salt);
    return static_cast<float>(mixed & 0xffffU) / 65535.0f;
}

std::uint32_t bossWeakPointHintSeed(const Enemy& enemy, const BossWeakPointSpec& weakPoint)
{
    std::uint32_t seed = 2166136261u;
    const std::string_view key = enemy.enemyId.empty() ? std::string_view(DefaultEnemyId) : std::string_view(enemy.enemyId);
    for (unsigned char ch : key) {
        seed ^= ch;
        seed *= 16777619u;
    }
    seed ^= static_cast<std::uint32_t>(std::max(0, enemy.id)) * 0x9e3779b9U;
    seed ^= static_cast<std::uint32_t>(weakPoint.kind) * 0x85ebca6bU;
    return mixBossWeakPointHintSeed(seed);
}

void drawBossWeakPointHintParticles(
    Renderer& renderer,
    const Enemy& enemy,
    const HitboxCatalog* hitboxCatalog,
    const EnemyPlacementCatalog* placementCatalog)
{
    const Vec2 centerOffset = enemyVisualOffset(enemy, placementCatalog);
    const BossWeakPointSpec weakPoint = bossWeakPointFor(enemy, hitboxCatalog, placementCatalog);
    if (!weakPoint.exposed) {
        return;
    }

    const Vec2 drawCenter = enemyDrawPosition(enemy, placementCatalog) + (weakPoint.center - (enemy.position + centerOffset));
    const float radius = std::max(4.0f, weakPoint.radius);
    const int particleCount = std::clamp(
        static_cast<int>(std::round(radius * 0.42f)),
        BossWeakPointHintMinParticles,
        BossWeakPointHintMaxParticles);
    const std::uint32_t seed = bossWeakPointHintSeed(enemy, weakPoint);
    const float time = enemy.behaviorTimer;

    for (int i = 0; i < particleCount; ++i) {
        const std::uint32_t salt = static_cast<std::uint32_t>(i + 1) * 0x45d9f3bU;
        const float baseAngle = bossWeakPointHintRandom(seed, salt + 1U) * Pi * 2.0f;
        const float baseDistance = radius * lerp(0.12f, 1.08f, bossWeakPointHintRandom(seed, salt + 2U));
        const float speed = lerp(0.18f, 0.38f, bossWeakPointHintRandom(seed, salt + 3U));
        const float phase = bossWeakPointHintRandom(seed, salt + 4U) * Pi * 2.0f;
        const float directionSign = (i % 2 == 0) ? 1.0f : -1.0f;
        const float angle = baseAngle + directionSign * time * speed * 0.42f +
            std::sin(time * (speed * 1.15f) + phase) * 0.12f;
        const Vec2 radial = fromAngle(angle);
        const Vec2 tangent{-radial.y, radial.x};
        const float drift = std::sin(time * lerp(0.74f, 1.32f, bossWeakPointHintRandom(seed, salt + 5U)) + phase) * radius * 0.10f;
        const float sideDrift = std::cos(time * lerp(0.58f, 1.05f, bossWeakPointHintRandom(seed, salt + 6U)) + phase) * radius * 0.08f;
        const Vec2 particlePosition = drawCenter + radial * (baseDistance + drift) + tangent * sideDrift;
        const float twinkle = 0.5f + 0.5f * std::sin(time * lerp(0.92f, 1.58f, bossWeakPointHintRandom(seed, salt + 7U)) + phase);
        const float particleRadius = std::max(0.75f, radius * lerp(0.028f, 0.062f, bossWeakPointHintRandom(seed, salt + 8U)));
        const float alphaScale = 0.55f + twinkle * 0.45f;

        renderer.fillSoftCircle(
            particlePosition,
            particleRadius * lerp(1.8f, 2.6f, twinkle),
            scaleColorAlpha({100, 196, 255, 82}, alphaScale));
        renderer.fillSoftCircle(
            particlePosition,
            particleRadius,
            scaleColorAlpha({230, 250, 255, 220}, alphaScale));

        if (i % 3 == 0) {
            const Vec2 tail = tangent * particleRadius * lerp(2.0f, 3.2f, bossWeakPointHintRandom(seed, salt + 9U));
            renderer.drawSoftLine(
                particlePosition - tail * 0.55f,
                particlePosition + tail * 0.25f,
                std::max(0.65f, particleRadius * 0.36f),
                scaleColorAlpha({188, 238, 255, 96}, alphaScale));
        }
    }
}

bool junkCrabDebrisShouldRender(const Enemy& enemy)
{
    return enemy.active &&
        enemy.bossAction.enabled &&
        enemy.bossAction.pattern == JunkCrabPatternId &&
        !enemy.bossAction.hidden;
}

bool junkCrabDebrisVisible(const JunkCrabDebrisRuntime& debris)
{
    return debris.state != JunkCrabDebrisState::Inactive && debris.alpha > 0.001f;
}

Vec2 junkCrabDebrisPosition(const Enemy& enemy, const JunkCrabDebrisRuntime& debris)
{
    if (debris.state == JunkCrabDebrisState::Orbiting) {
        return enemy.position + fromAngle(debris.angle) * debris.radius;
    }
    return debris.position + Vec2{0.0f, -std::max(0.0f, debris.altitude)};
}

Vec2 junkCrabDebrisShadowPosition(const Enemy& enemy, const JunkCrabDebrisRuntime& debris)
{
    if (debris.state == JunkCrabDebrisState::Orbiting) {
        return enemy.position + fromAngle(debris.angle) * debris.radius;
    }
    return debris.position;
}

WorldIconId junkCrabDebrisIconForIndex(int index)
{
    return JunkCrabDebrisIconCycle[static_cast<std::size_t>(index) % JunkCrabDebrisIconCycle.size()];
}

void drawJunkCrabDebris(Renderer& renderer, const Enemy& enemy)
{
    if (!junkCrabDebrisShouldRender(enemy)) {
        return;
    }

    static constexpr std::array<Color, 6> FillColors{{
        {156, 142, 122, 230},
        {118, 128, 138, 230},
        {182, 142, 84, 230},
        {104, 94, 128, 230},
        {194, 168, 88, 230},
        {132, 118, 100, 230},
    }};
    for (int i = 0; i < JunkCrabMaxDebris; ++i) {
        const JunkCrabDebrisRuntime& debris = enemy.bossAction.junkCrab.debris[static_cast<std::size_t>(i)];
        if (!junkCrabDebrisVisible(debris)) {
            continue;
        }
        const Vec2 position = junkCrabDebrisPosition(enemy, debris);
        const Vec2 shadowPosition = junkCrabDebrisShadowPosition(enemy, debris);
        const float pulse = debris.state == JunkCrabDebrisState::Orbiting
            ? 1.0f + 0.08f * std::sin(enemy.behaviorTimer * 5.0f + debris.angle)
            : 1.0f;
        const float shadowFade = 1.0f - clamp(std::max(0.0f, debris.altitude) / 96.0f, 0.0f, 1.0f);
        const float shadowAlpha = debris.alpha * shadowFade;
        const float shadowScale = 0.88f + 0.12f * shadowFade;
        const float shadowRadius = JunkCrabDebrisRadius * pulse * shadowScale;
        renderer.fillEllipse(
            shadowPosition + Vec2{0.0f, shadowRadius * 0.62f},
            {shadowRadius * 0.95f, shadowRadius * 0.34f},
            scaleColorAlpha({0, 0, 0, 92}, shadowAlpha));
        WorldIconDrawOptions options;
        options.filter = TextureFilter::Linear;
        options.scaleMultiplier = pulse;
        options.rotationDegrees = debris.visualAngle * 180.0f / Pi;
        options.tint = scaleColorAlpha({255, 255, 255, 255}, debris.alpha);
        if (drawWorldIcon(
                renderer,
                junkCrabDebrisIconForIndex(debris.iconIndex),
                position,
                {JunkCrabDebrisImageMaxSize, JunkCrabDebrisImageMaxSize},
                options)) {
            continue;
        }

        const float radius = JunkCrabDebrisRadius * pulse;
        const Color fill = scaleColorAlpha(FillColors[static_cast<std::size_t>(i) % FillColors.size()], debris.alpha);
        renderer.fillCircle(position, radius, fill);
        renderer.drawCircle(position, radius + 2.0f, scaleColorAlpha({34, 28, 24, 210}, debris.alpha));
    }
}

int countdownExplosionWarningTickIndex(const Enemy& enemy)
{
    if (!enemy.countdownExplodeArmed) {
        return -1;
    }
    return explosionWarningTickIndex(enemy.countdownExplodeInitialDelay, enemy.countdownExplodeDelay);
}

ExplosionWarningVisual countdownExplosionWarningVisual(const Enemy& enemy)
{
    if (enemy.countdownExplodeRadius <= 0.0f ||
        enemy.countdownExplodeInitialDelay <= 0.0f ||
        !enemy.countdownExplodeArmed ||
        enemy.countdownExploded ||
        enemy.death.active ||
        enemy.spawnTimer > 0.0f ||
        enemy.bossAction.hidden) {
        return {};
    }

    return explosionWarningVisual(enemy.countdownExplodeInitialDelay, enemy.countdownExplodeDelay);
}

void drawCountdownExplosionWarningAura(
    Renderer& renderer,
    Vec2 position,
    float visualRadius,
    const ExplosionWarningVisual& warning)
{
    if (!warning.active) {
        return;
    }

    const float auraRadius = visualRadius * (1.55f + warning.urgency * 0.45f + warning.pulse * 0.25f);
    const float outerRadius = visualRadius * (1.14f + warning.urgency * 0.20f + warning.pulse * 0.16f);
    renderer.fillSoftCircle(position, auraRadius, scaleColorAlpha({255, 36, 28, 118}, warning.intensity));
    renderer.fillSoftCircle(position, outerRadius, scaleColorAlpha({255, 78, 38, 92}, 0.30f + warning.intensity * 0.62f));

    const float ringRadius = visualRadius + 6.0f + warning.urgency * 5.0f + warning.pulse * (3.5f + warning.urgency * 4.0f);
    renderer.drawCircle(position, ringRadius, scaleColorAlpha({255, 52, 38, 170}, 0.26f + warning.intensity * 0.62f));
    renderer.drawCircle(position, ringRadius + 4.0f + warning.pulse * 3.0f, scaleColorAlpha({255, 128, 76, 120}, warning.intensity * 0.56f));
}

void drawCountdownExplosionWarningOverlay(
    Renderer& renderer,
    Vec2 position,
    float visualRadius,
    const ExplosionWarningVisual& warning)
{
    if (!warning.active) {
        return;
    }

    const float coreRadius = visualRadius * (0.78f + warning.pulse * 0.12f);
    renderer.fillSoftCircle(position, coreRadius, scaleColorAlpha({255, 32, 28, 92}, 0.20f + warning.intensity * 0.62f));
    renderer.drawCircle(position, visualRadius + 2.0f + warning.pulse * 3.0f, scaleColorAlpha({255, 36, 30, 210}, 0.34f + warning.intensity * 0.58f));
    renderer.drawCircle(position, visualRadius * (0.52f + warning.pulse * 0.10f), scaleColorAlpha({255, 206, 172, 160}, warning.intensity * 0.48f));

    const float rayAngle = warning.elapsed * (2.8f + warning.urgency * 5.0f);
    const float rayInner = visualRadius * (0.40f + warning.pulse * 0.08f);
    const float rayOuter = visualRadius * (1.12f + warning.urgency * 0.26f + warning.pulse * 0.18f);
    const Color rayColor = scaleColorAlpha({255, 72, 48, 150}, warning.intensity * 0.72f);
    for (int i = 0; i < 4; ++i) {
        const Vec2 ray = fromAngle(rayAngle + static_cast<float>(i) * Pi * 0.5f);
        renderer.drawLine(position + ray * rayInner, position + ray * rayOuter, rayColor);
    }
}

bool isAstragnaBossAction(const Enemy& enemy);
void drawAstragnaBoss(Renderer& renderer, const TileMap& map, const Enemy& enemy);

const EnemyHeldDrop* visibleCarriedLoot(const Enemy& enemy)
{
    const auto it = std::find_if(enemy.heldDrops.begin(), enemy.heldDrops.end(), [](const EnemyHeldDrop& held) {
        return held.origin == EnemyHeldDropOrigin::PickedUp && held.quantity > 0;
    });
    return it != enemy.heldDrops.end() ? &*it : nullptr;
}

void drawEnemyCarriedLoot(
    Renderer& renderer,
    const Enemy& enemy,
    const ObjectCatalog& objectCatalog,
    Vec2 drawPosition,
    float visualRadius)
{
    const EnemyHeldDrop* held = visibleCarriedLoot(enemy);
    if (held == nullptr) {
        return;
    }

    const Vec2 facing = enemyFacingDirectionVector(enemy.facingAngle);
    const Vec2 iconMaxSize = WorldItemImageMaxSize;
    const float iconExtent = std::max(iconMaxSize.x, iconMaxSize.y);
    const float bob = std::sin(enemy.behaviorTimer * 8.0f) * 1.2f;
    const Vec2 anchor = drawPosition - facing * (visualRadius * 0.55f) + Vec2{0.0f, -visualRadius * 0.45f + bob};
    renderer.fillEllipse(
        anchor + Vec2{0.0f, iconExtent * 0.18f},
        {iconExtent * 0.32f, iconExtent * 0.16f},
        {24, 14, 10, 112});

    bool drewImage = false;
    if (held->kind == EnemyHeldDropKind::Money) {
        drewImage = drawWorldIcon(
            renderer,
            moneyWorldIconForAmount(held->quantity),
            anchor,
            iconMaxSize);
    } else if (held->kind == EnemyHeldDropKind::Object) {
        const ItemData* catalogItem = objectCatalog.registry.findById(held->objectId);
        const ItemData* item = held->runtimeItem ? &*held->runtimeItem : catalogItem;
        if (item != nullptr) {
            const bool broken = held->instance && held->instance->isBroken;
            const ObjectImageDrawOptions options = itemImageOptionsWithBrokenState(
                objectGroundImageOptions(*item),
                broken);
            drewImage = catalogItem != nullptr
                ? drawObjectImage(renderer, *item, anchor, iconMaxSize, options)
                : drawItemImage(renderer, *item, anchor, iconMaxSize, options);
        }
    }
    if (!drewImage) {
        const Color fill = held->kind == EnemyHeldDropKind::Money
            ? Color{245, 206, 76, 255}
            : Color{255, 118, 148, 255};
        renderer.fillCircle(anchor, 7.0f, fill);
        renderer.drawCircle(anchor, 10.0f, {255, 246, 190, 220});
    }
}

float bossActionContactDamageMultiplier(const Enemy& enemy)
{
    if (enemy.bossAction.pattern != StardustMolePatternId) {
        return 1.0f;
    }
    if (enemy.bossAction.phase == BossActionPhase::Jump) {
        return StardustMoleJumpContactDamageMultiplier;
    }
    if (enemy.bossAction.phase == BossActionPhase::Charge) {
        return StardustMoleChargeContactDamageMultiplier;
    }
    return 1.0f;
}

void drawEnemyVisual(
    Renderer& renderer,
    const TileMap& map,
    const Enemy& enemy,
    const ObjectCatalog& objectCatalog,
    const HitboxCatalog* hitboxCatalog,
    const EnemyPlacementCatalog* placementCatalog,
    bool captureHighlighted,
    bool detailsKnown)
{
    const Vec2 drawPosition = enemyDrawPosition(enemy, placementCatalog);
    const auto drawAwarenessIcon = [&](float visualRadius) {
        if (enemy.awarenessIcon == EnemyAwarenessIcon::None || enemy.awarenessIconTimer <= 0.0f) {
            return;
        }
        const char* iconText = enemy.awarenessIcon == EnemyAwarenessIcon::Exclamation ? "!" : "?";
        const Color iconColor = enemy.awarenessIcon == EnemyAwarenessIcon::Exclamation
            ? Color{255, 236, 118, 255}
            : Color{180, 228, 255, 255};
        renderer.drawText(drawPosition + Vec2{-4.0f, -visualRadius - 28.0f}, iconText, iconColor, 2);
    };
    if (enemy.spawnTimer > 0.0f) {
        const float ratio = enemy.spawnDuration > 0.0f ? enemy.spawnTimer / enemy.spawnDuration : 0.0f;
        if (enemy.spawnVisualKind == EnemySpawnVisualKind::WalkIn) {
            const float visualRadius = enemyVisualRadius(enemy);
            Vec2 imageSize{};
            const bool drewImage = drawEnemyImage(renderer, enemy, drawPosition, enemy.behaviorTimer, enemyImageOptionsFor(enemy), &imageSize);
            if (!drewImage) {
                const Color color = enemy.isBoss ? Color{142, 46, 160, 255} : colorForEnemy(enemy);
                renderer.fillCircle(drawPosition, visualRadius, color);
                renderer.drawCircle(drawPosition, visualRadius + 3.0f, enemy.isBoss ? Color{255, 210, 96, 255} : Color{80, 18, 28, 255});
            }
            drawAwarenessIcon(drewImage ? std::max(visualRadius, imageSize.y * 0.5f) : visualRadius);
            return;
        }
        if (enemy.spawnVisualKind == EnemySpawnVisualKind::GroundEmerge) {
            const float progress = 1.0f - clamp(ratio, 0.0f, 1.0f);
            const float eased = smoothStep01(progress);
            const float visualRadius = enemyVisualRadius(enemy);
            const float rise = visualRadius * (1.20f - eased) + std::sin(progress * Pi) * visualRadius * 0.16f;
            const Vec2 emergePosition = drawPosition + Vec2{0.0f, rise};
            const Color dirt = enemy.isBoss ? Color{136, 88, 54, 222} : Color{118, 84, 58, 210};
            const Color darkDirt = {58, 42, 34, 190};
            const Color dust = {198, 154, 96, 126};
            renderer.fillEllipse(
                drawPosition + Vec2{0.0f, visualRadius * 0.42f},
                {visualRadius * (1.28f + eased * 0.36f), visualRadius * 0.34f},
                darkDirt);
            renderer.drawCircle(drawPosition + Vec2{0.0f, visualRadius * 0.25f}, visualRadius * (0.72f + eased * 0.28f), dirt);
            renderer.drawCircle(drawPosition + Vec2{0.0f, visualRadius * 0.24f}, visualRadius * (1.06f + eased * 0.22f), scaleColorAlpha(dust, 0.55f + eased * 0.32f));
            for (int i = 0; i < 7; ++i) {
                const float angle = static_cast<float>(i) * Pi * 2.0f / 7.0f + progress * 0.9f;
                const float radius = visualRadius * (0.38f + 0.08f * static_cast<float>(i % 3));
                const Vec2 chip = drawPosition + Vec2{std::cos(angle) * visualRadius * (0.62f + eased * 0.32f), visualRadius * 0.36f + std::sin(angle) * visualRadius * 0.16f};
                renderer.fillCircle(chip, radius * (0.22f + eased * 0.12f), dirt);
            }

            Vec2 imageSize{};
            const bool drewImage = drawEnemyImage(renderer, enemy, emergePosition, enemy.behaviorTimer, enemyImageOptionsFor(enemy), &imageSize);
            if (!drewImage) {
                const Color color = enemy.isBoss ? Color{142, 46, 160, 255} : colorForEnemy(enemy);
                renderer.fillCircle(emergePosition, visualRadius, color);
                renderer.drawCircle(emergePosition, visualRadius + 3.0f, enemy.isBoss ? Color{255, 210, 96, 255} : Color{80, 18, 28, 255});
            }
            renderer.drawCircle(drawPosition, visualRadius * (1.10f + progress * 0.18f), {255, 208, 112, 140});
            drawAwarenessIcon(visualRadius);
            return;
        }
        const float pulse = 1.0f + (1.0f - ratio) * 0.9f;
        const float visualRadius = enemyVisualRadius(enemy);
        Color spawnColor = enemy.isBoss ? Color{255, 180, 80, 230} : colorForEnemy(enemy);
        if (enemy.variantTier != EnemyVariantTier::Normal) {
            spawnColor = multiplyRgb(spawnColor, enemyVariantTintMultiplier(enemy.variantTier));
        }
        renderer.drawCircle(drawPosition, visualRadius * pulse + 4.0f, spawnColor);
        renderer.drawCircle(drawPosition, visualRadius * 0.55f, enemy.isBoss ? Color{255, 232, 140, 210} : Color{255, 160, 110, 190});
        drawAwarenessIcon(visualRadius);
        return;
    }
    Color color = enemy.hitFlash > 0.0f ? Color{255, 255, 255, 255} : (enemy.isBoss ? Color{142, 46, 160, 255} : colorForEnemy(enemy));
    const EntityStatusVisualStyle statusVisual = entityStatusVisualStyle(enemy.status);
    if (enemy.hitFlash <= 0.0f && statusVisual.hasTint) {
        color = statusVisual.tint;
    }
    if (enemy.death.active) {
        color = darkenEnemyColorForDeath(color, enemy);
    }
    if (enemy.hitFlash <= 0.0f && !enemy.death.active && enemy.variantTier != EnemyVariantTier::Normal) {
        color = multiplyRgb(color, enemyVariantTintMultiplier(enemy.variantTier));
    }
    const float visualRadius = enemyVisualRadius(enemy);
    const ExplosionWarningVisual explosionWarning = countdownExplosionWarningVisual(enemy);
    drawCountdownExplosionWarningAura(renderer, drawPosition, visualRadius, explosionWarning);
    if (enemy.dungeonEventBoss && !enemy.death.active) {
        renderer.drawCircle(drawPosition, visualRadius + 10.0f, {255, 188, 90, 190});
        renderer.drawCircle(drawPosition, visualRadius + 15.0f, {255, 94, 118, 115});
    }
    drawAstragnaBoss(renderer, map, enemy);
    drawJunkCrabDebris(renderer, enemy);
    const bool astragnaVisual = isAstragnaBossAction(enemy);
    EnemyImageDrawOptions imageOptions = enemyImageOptionsFor(enemy);
    if (captureHighlighted && !enemy.death.active) {
        imageOptions.maskOverlayColor = {255, 255, 255, 62};
    }
    Vec2 enemyImageDrawSize{};
    const bool drewImage = !astragnaVisual && drawEnemyImage(renderer, enemy, drawPosition, enemy.behaviorTimer, imageOptions, &enemyImageDrawSize);
    const float uiVisualRadius = drewImage ? std::max(visualRadius, enemyImageDrawSize.y * 0.5f) : visualRadius;
    if (!drewImage && !astragnaVisual) {
        renderer.fillCircle(drawPosition, visualRadius, color);
        if (enemy.externalBounceActive && enemy.jumpActive) {
            const float spinRadians = externalBounceRotationDegrees(enemy) * (Pi / 180.0f);
            const Vec2 axis = fromAngle(spinRadians) * (visualRadius * 0.72f);
            renderer.drawLine(drawPosition - axis, drawPosition + axis, Color{255, 255, 255, 210});
        }
        renderer.drawCircle(drawPosition, visualRadius + 3.0f, enemy.isBoss ? Color{255, 210, 96, 255} : Color{80, 18, 28, 255});
        if (captureHighlighted && !enemy.death.active) {
            renderer.drawCircle(drawPosition, visualRadius + 6.0f, {255, 255, 255, 245});
        }
    }
    if (!astragnaVisual) {
        drawCountdownExplosionWarningOverlay(renderer, drawPosition, visualRadius, explosionWarning);
    }
    drawEnemyCarriedLoot(renderer, enemy, objectCatalog, drawPosition, visualRadius);
    if (enemy.death.active) {
        return;
    }
    drawBossWeakPointHintParticles(renderer, enemy, hitboxCatalog, placementCatalog);
    renderEntityStatusOverlays(renderer, enemy.status, drawPosition, uiVisualRadius * 2.0f, enemy.behaviorTimer);
    if (enemy.bossAction.pattern == StardustMolePatternId &&
        enemy.bossAction.phase == BossActionPhase::Stun &&
        !enemy.status.hasState("status_confuse")) {
        renderConfuseStatusOverlay(renderer, drawPosition, uiVisualRadius * 2.0f, enemy.behaviorTimer);
    }
    drawEnemyHpBar(renderer, enemy, drawPosition, uiVisualRadius, detailsKnown);
    if (enemy.isBoss && !isAstragnaBossAction(enemy)) {
        const float hpRatio = enemy.maxHp > 0 ? clamp(static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHp), 0.0f, 1.0f) : 0.0f;
        const Vec2 barPos = drawPosition + Vec2{-28.0f, -uiVisualRadius - 14.0f};
        UiGaugeStyle bossHpGaugeStyle;
        bossHpGaugeStyle.fill.start = {255, 146, 72, 255};
        bossHpGaugeStyle.fill.end = {255, 222, 104, 255};
        bossHpGaugeStyle.track = {18, 10, 22, 220};
        bossHpGaugeStyle.trackInner = {32, 16, 30, 220};
        bossHpGaugeStyle.trackOuter = {255, 210, 96, 62};
        bossHpGaugeStyle.shadow = {0, 0, 0, 92};
        bossHpGaugeStyle.highlight = {255, 248, 214, 72};
        bossHpGaugeStyle.trackOuterExtra = 1.0f;
        bossHpGaugeStyle.trackInnerInset = 2.0f;
        bossHpGaugeStyle.shadowOffsetY = 1.0f;
        bossHpGaugeStyle.shadowExtra = 3.0f;
        drawUiGauge(renderer, {barPos, {56.0f, 5.0f}}, hpRatio, bossHpGaugeStyle);
    }
    drawAwarenessIcon(uiVisualRadius);
}

const EnemyDefinition* findEnemyDefinitionById(const EnemyCatalog& enemyCatalog, std::string_view enemyId)
{
    if (enemyId.empty()) {
        return nullptr;
    }
    const auto it = enemyCatalog.enemiesById.find(std::string(enemyId));
    if (it != enemyCatalog.enemiesById.end()) {
        return &it->second;
    }
    const auto vecIt = std::find_if(enemyCatalog.enemies.begin(), enemyCatalog.enemies.end(), [enemyId](const EnemyDefinition& definition) {
        return definition.id == enemyId;
    });
    return vecIt != enemyCatalog.enemies.end() ? &*vecIt : nullptr;
}

float enemyDefinitionSpawnRadius(
    const EnemyDefinition* definition,
    const RuntimeBalance& balance,
    const EnemyPlacementCatalog* placementCatalog,
    float radiusMultiplier = 1.0f)
{
    float radius = balance.enemyRadius;
    if (definition != nullptr && definition->radius > 0.0 && std::isfinite(definition->radius)) {
        radius = static_cast<float>(definition->radius);
    }
    if (definition != nullptr) {
        if (const std::optional<float> placementRadius = enemyPlacementPassageRadiusFor(placementCatalog, definition->id)) {
            radius = *placementRadius;
        }
    }
    return radius * std::max(0.1f, radiusMultiplier);
}

float bossSpawnRadiusFor(
    const EnemyCatalog& enemyCatalog,
    std::string_view bossEnemyId,
    const RuntimeBalance& balance,
    const EnemyPlacementCatalog* placementCatalog)
{
    if (const EnemyDefinition* definition = findEnemyDefinitionById(enemyCatalog, bossEnemyId)) {
        return enemyDefinitionSpawnRadius(definition, balance, placementCatalog, BossRadiusMultiplier);
    }
    if (isStardustMoleId(bossEnemyId)) {
        return std::max(balance.enemyRadius * 1.35f, 15.0f) * BossRadiusMultiplier;
    }
    if (isAstragnaId(bossEnemyId)) {
        return std::max(balance.enemyRadius * 1.9f, 24.0f) * BossRadiusMultiplier;
    }
    return balance.enemyRadius * BossRadiusMultiplier;
}

void applyFallbackBossDefinition(Enemy& enemy, std::string_view bossEnemyId, const RuntimeBalance& balance)
{
    if (!isStardustMoleId(bossEnemyId) &&
        !isJunkCrabId(bossEnemyId) &&
        !isAstragnaId(bossEnemyId) &&
        !isStarVeinDragonId(bossEnemyId)) {
        return;
    }

    if (isStardustMoleId(bossEnemyId)) {
        enemy.enemyId = std::string(StardustMoleEnemyId);
        enemy.enemyName = "星くずモグラ";
        enemy.enemyTags = {"boss", "boss_only", "no_normal_spawn", "unique"};
        enemy.aiId = "stationary";
        enemy.unawareAiId = "idle";
        enemy.radius = std::max(balance.enemyRadius * 1.35f, 15.0f);
        enemy.maxHp = 1440;
        enemy.hp = enemy.maxHp;
        enemy.xp = std::max(enemy.xp, 60);
        enemy.contactAttackPower = 2;
        enemy.contactDamageType = "blunt";
        configureEnemyAltitudeFromAi(enemy);
        return;
    }

    if (isAstragnaId(bossEnemyId)) {
        enemy.enemyId = std::string(AstragnaEnemyId);
        enemy.enemyName = "星封殻アストラグナ";
        enemy.enemyTags = {"boss", "boss_only", "no_normal_spawn", "unique", "large", "terrain_boss"};
        enemy.aiId = "stationary";
        enemy.unawareAiId = "idle";
        enemy.radius = std::max(balance.enemyRadius * 1.9f, 24.0f);
        enemy.maxHp = 1;
        enemy.hp = enemy.maxHp;
        enemy.xp = std::max(enemy.xp, 120);
        enemy.contactAttackPower = 0;
        enemy.contactDamageType = "none";
        configureEnemyAltitudeFromAi(enemy);
        return;
    }

    if (isStarVeinDragonId(bossEnemyId)) {
        enemy.enemyId = std::string(StarVeinDragonEnemyId);
        enemy.enemyName = "星脈竜";
        enemy.enemyTags = {"boss", "boss_only", "no_normal_spawn", "unique", "large", "heavy", "magic"};
        enemy.aiId = "hover_chase";
        enemy.unawareAiId = "idle";
        enemy.radius = std::max(balance.enemyRadius * 2.1f, 30.0f);
        enemy.maxHp = 4000;
        enemy.hp = enemy.maxHp;
        enemy.xp = std::max(enemy.xp, 180);
        enemy.contactAttackPower = 18;
        enemy.contactDamageType = "magic";
        configureEnemyAltitudeFromAi(enemy);
        return;
    }

    enemy.enemyId = std::string(JunkCrabEnemyId);
    enemy.enemyName = "廃品殻獣ジャンクラブ";
    enemy.enemyTags = {"boss", "boss_only", "no_normal_spawn", "unique", "large", "heavy"};
    enemy.aiId = "stationary";
    enemy.unawareAiId = "idle";
    enemy.radius = std::max(balance.enemyRadius * 1.65f, 18.0f);
    enemy.maxHp = 1680;
    enemy.hp = enemy.maxHp;
    enemy.xp = std::max(enemy.xp, 90);
    enemy.contactAttackPower = 4;
    enemy.contactDamageType = "blunt";
    configureEnemyAltitudeFromAi(enemy);
}

void enableDefaultBossActionIfNeeded(Enemy& enemy, std::string_view bossEnemyId)
{
    if (!enemy.bossAction.enabled) {
        enemy.bossAction.pattern = defaultBossActionPatternFor(enemy);
        if (enemy.bossAction.pattern.empty() && isStardustMoleId(bossEnemyId)) {
            enemy.bossAction.pattern = std::string(StardustMolePatternId);
        }
        enemy.bossAction.enabled = !enemy.bossAction.pattern.empty();
    }
    if (enemy.bossAction.enabled && enemy.bossAction.pattern.empty()) {
        enemy.bossAction.pattern = defaultBossActionPatternFor(enemy);
    }
    if (enemy.bossAction.pattern.empty()) {
        enemy.bossAction.enabled = false;
    }
}

float bossActionPhaseDuration(BossActionPhase phase, std::string_view pattern)
{
    if (pattern != StardustMolePatternId) {
        return 0.0f;
    }

    switch (phase) {
    case BossActionPhase::Approach: return StardustMoleApproachMinSeconds;
    case BossActionPhase::Submerge: return StardustMoleDiveJumpSeconds + StardustMoleUndergroundMinSeconds;
    case BossActionPhase::Telegraph: return StardustMoleTelegraphSeconds;
    case BossActionPhase::Jump: return StardustMoleJumpSeconds;
    case BossActionPhase::LandingDelay: return StardustMoleLandingDelaySeconds;
    case BossActionPhase::Charge: return StardustMoleChargeSeconds;
    case BossActionPhase::Stun: return StardustMoleStunSeconds;
    case BossActionPhase::Recover: return StardustMoleRecoverSeconds;
    case BossActionPhase::None: break;
    }
    return 0.0f;
}

float randomizedBossActionPhaseDuration(BossActionPhase phase, std::string_view pattern, std::mt19937& rng)
{
    if (pattern == StardustMolePatternId && phase == BossActionPhase::Approach) {
        std::uniform_real_distribution<float> approachDurationDist(
            StardustMoleApproachMinSeconds,
            StardustMoleApproachMaxSeconds);
        return approachDurationDist(rng);
    }
    if (pattern == StardustMolePatternId && phase == BossActionPhase::Submerge) {
        std::uniform_real_distribution<float> undergroundWaitDist(
            StardustMoleUndergroundMinSeconds,
            StardustMoleUndergroundMaxSeconds);
        return StardustMoleDiveJumpSeconds + undergroundWaitDist(rng);
    }
    return bossActionPhaseDuration(phase, pattern);
}

Vec2 safeDirection(Vec2 value, Vec2 fallback = {1.0f, 0.0f})
{
    return lengthSquared(value) > 0.0001f ? normalize(value) : fallback;
}

float windBlowRadiusFor(const Enemy& enemy)
{
    const float defaultRadius = WindBlowDefaultRadiusTiles * static_cast<float>(balance::TileSize);
    const double radiusTiles = behaviorParamDouble(enemy, "wind_blow", "radiusTiles", -1.0);
    if (radiusTiles > 0.0) {
        return static_cast<float>(std::max(0.25, radiusTiles) * static_cast<double>(balance::TileSize));
    }
    const double radius = behaviorParamDouble(enemy, "wind_blow", "radius", -1.0);
    if (radius > 0.0) {
        return std::max(defaultRadius, static_cast<float>(radius));
    }
    return defaultRadius;
}

float windBlowDurationFor(const Enemy& enemy)
{
    return static_cast<float>(std::clamp(
        behaviorParamDouble(enemy, "wind_blow", "duration", WindBlowDefaultDurationSeconds),
        0.05,
        3.5));
}

float windBlowStrengthFor(const Enemy& enemy)
{
    return static_cast<float>(std::clamp(
        behaviorParamDouble(enemy, "wind_blow", "strength", 1.0),
        0.1,
        4.0));
}

float directionalWindFalloff(Vec2 position, Vec2 center, float radius)
{
    if (radius <= 0.0f) {
        return 0.0f;
    }
    const float distanceSq = distanceSquared(position, center);
    const float radiusSq = radius * radius;
    if (distanceSq > radiusSq) {
        return 0.0f;
    }
    const float distance = std::sqrt(std::max(0.0f, distanceSq));
    return 0.35f + (1.0f - clamp(distance / radius, 0.0f, 1.0f)) * 0.65f;
}

float windEnemyMassMultiplier(const Enemy& enemy)
{
    const bool windSensitive =
        enemy.hoverAltitude > 0.0f ||
        enemy.altitude > 1.0f ||
        hasEnemyTagAny(enemy, {"flying", "hover", "airborne", "floating"});
    const float airborneMultiplier = windSensitive ? 2.5f : 1.0f;
    if (enemy.isBoss || hasEnemyTagAny(enemy, {"boss", "boss_only"})) {
        return 0.12f * airborneMultiplier;
    }
    if (hasEnemyTagAny(enemy, {"heavy", "large", "massive", "huge"})) {
        return 0.25f * airborneMultiplier;
    }
    if (hasEnemyTagAny(enemy, {"small", "light", "tiny", "lightweight"})) {
        return 0.55f * airborneMultiplier;
    }
    if (effectiveEnemyRadius(enemy) >= 18.0f || enemy.maxHp >= 80) {
        return 0.32f * airborneMultiplier;
    }
    return 0.38f * airborneMultiplier;
}

EnemyWindPulse makeEnemyWindPulse(const Enemy& enemy, Vec2 playerPosition)
{
    const float duration = windBlowDurationFor(enemy);
    return EnemyWindPulse{
        .center = playerPosition,
        .direction = safeDirection(playerPosition - enemy.position, facingVector(enemy.facingAngle)),
        .radius = windBlowRadiusFor(enemy),
        .strength = windBlowStrengthFor(enemy),
        .remainingSeconds = duration,
        .initialSeconds = duration,
        .sourceRuntimeId = enemy.id,
    };
}

Vec2 chooseStardustMoleEmergePosition(
    const Enemy& enemy,
    const Player& player,
    TileMap& map,
    const EnemyPlacementCatalog* placementCatalog)
{
    const float radius = enemyPassageRadius(enemy, placementCatalog);
    const Vec2 fromPlayer = safeDirection(enemy.position - player.position);
    const Vec2 perpendicular{-fromPlayer.y, fromPlayer.x};
    const std::array<Vec2, 10> candidates{{
        player.position + fromPlayer * StardustMoleEmergeDistance,
        player.position - fromPlayer * StardustMoleEmergeDistance,
        player.position + perpendicular * StardustMoleEmergeDistance,
        player.position - perpendicular * StardustMoleEmergeDistance,
        player.position + fromPlayer * (StardustMoleEmergeDistance * 0.72f),
        player.position - fromPlayer * (StardustMoleEmergeDistance * 0.72f),
        player.position + perpendicular * (StardustMoleEmergeDistance * 0.72f),
        player.position - perpendicular * (StardustMoleEmergeDistance * 0.72f),
        enemy.position,
        player.position + fromPlayer * (StardustMoleEmergeDistance * 1.25f),
    }};

    for (Vec2 candidate : candidates) {
        if (distanceSquared(candidate, player.position) < StardustMoleEmergeMinPlayerDistance * StardustMoleEmergeMinPlayerDistance) {
            continue;
        }
        if (!map.isCircleBlocked(candidate, radius)) {
            return candidate;
        }
    }
    return enemy.position;
}

void configureBossActionFlags(Enemy& enemy, BossActionPhase phase)
{
    enemy.bossAction.hidden = phase == BossActionPhase::Telegraph;
    enemy.bossAction.invulnerable =
        enemy.bossAction.hidden ||
        phase == BossActionPhase::Submerge ||
        phase == BossActionPhase::Jump ||
        phase == BossActionPhase::None;
}

bool bossActionControlsJump(const Enemy& enemy)
{
    if (!enemy.bossAction.enabled) {
        return false;
    }
    return enemy.bossAction.phase == BossActionPhase::Submerge ||
        enemy.bossAction.phase == BossActionPhase::Jump;
}

void enterBossActionPhase(
    Enemy& enemy,
    BossActionPhase phase,
    Player& player,
    TileMap& map,
    std::vector<EnemyEvent>& events,
    std::mt19937& rng,
    const EnemyPlacementCatalog* placementCatalog)
{
    enemy.bossAction.phase = phase;
    enemy.bossAction.timer = 0.0f;
    enemy.bossAction.phaseDuration = randomizedBossActionPhaseDuration(phase, enemy.bossAction.pattern, rng);
    configureBossActionFlags(enemy, phase);

    switch (phase) {
    case BossActionPhase::Approach:
        enemy.velocity = {};
        enemy.jumpActive = false;
        enemy.altitude = 0.0f;
        enemy.facingAngle = std::atan2(player.position.y - enemy.position.y, player.position.x - enemy.position.x);
        break;
    case BossActionPhase::Submerge:
        enemy.velocity = {};
        enemy.bossAction.hidden = false;
        enemy.bossAction.invulnerable = true;
        startEnemyJumpToTarget(enemy, enemy.position, StardustMoleDiveJumpSeconds, StardustMoleDiveJumpHeight);
        events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "burrow_start"));
        break;
    case BossActionPhase::Telegraph:
        enemy.velocity = {};
        enemy.jumpActive = false;
        enemy.altitude = 0.0f;
        enemy.bossAction.targetPosition = chooseStardustMoleEmergePosition(enemy, player, map, placementCatalog);
        events.push_back(makeEnemyEventAt(EnemyEventType::BossTelegraph, enemy, enemy.bossAction.targetPosition, "dust"));
        break;
    case BossActionPhase::Jump:
        enemy.position = enemy.bossAction.targetPosition;
        enemy.velocity = {};
        enemy.bossAction.chargeDirection = safeDirection(player.position - enemy.position, facingVector(enemy.facingAngle));
        enemy.facingAngle = std::atan2(enemy.bossAction.chargeDirection.y, enemy.bossAction.chargeDirection.x);
        startEnemyJumpToTarget(enemy, enemy.bossAction.targetPosition, StardustMoleJumpSeconds, StardustMoleJumpHeight);
        events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "jump_start"));
        break;
    case BossActionPhase::LandingDelay:
        enemy.velocity = {};
        enemy.jumpActive = false;
        enemy.altitude = 0.0f;
        events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "landing"));
        break;
    case BossActionPhase::Charge:
        enemy.bossAction.chargeDirection = safeDirection(enemy.bossAction.chargeDirection, facingVector(enemy.facingAngle));
        enemy.facingAngle = std::atan2(enemy.bossAction.chargeDirection.y, enemy.bossAction.chargeDirection.x);
        enemy.velocity = enemy.bossAction.chargeDirection * StardustMoleChargeSpeed;
        events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "charge_start"));
        break;
    case BossActionPhase::Stun:
        enemy.velocity = {};
        enemy.jumpActive = false;
        enemy.altitude = 0.0f;
        events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "wall_stun"));
        break;
    case BossActionPhase::Recover:
        enemy.velocity = {};
        break;
    case BossActionPhase::None:
        enemy.velocity = {};
        break;
    }
}

std::array<Vec2, 5> bossChargeSamplePoints(Vec2 center, Vec2 direction, float radius)
{
    const Vec2 dir = safeDirection(direction);
    const Vec2 perpendicular{-dir.y, dir.x};
    return {{
        center,
        center + dir * (radius * 0.72f),
        center + perpendicular * (radius * 0.55f),
        center - perpendicular * (radius * 0.55f),
        center + dir * (radius * 0.72f) + perpendicular * (radius * 0.35f),
    }};
}

bool breakSoftTilesForBossCharge(
    Enemy& enemy,
    TileMap& map,
    Vec2 center,
    Vec2 direction,
    std::vector<EnemyEvent>& events,
    const EnemyPlacementCatalog* placementCatalog)
{
    bool hitHardWall = false;
    const float radius = enemyPassageRadius(enemy, placementCatalog);
    for (Vec2 sample : bossChargeSamplePoints(center, direction, radius)) {
        const int tx = map.worldToTile(sample.x);
        const int ty = map.worldToTile(sample.y);
        const TerrainAttribute attribute = map.terrainAttributeAtTile(tx, ty);
        if (attribute == TerrainAttribute::Hard || attribute == TerrainAttribute::Ore) {
            hitHardWall = true;
            continue;
        }
        if (attribute != TerrainAttribute::Soft) {
            continue;
        }
        Vec2 opened{};
        TileType openedType = TileType::Dirt;
        if (map.damageTile(tx, ty, BossChargeTerrainDamage, opened, &openedType)) {
            events.push_back(makeEnemyEventAt(
                EnemyEventType::TerrainBreak,
                enemy,
                opened,
                std::string(terrainAttributeCode(terrainAttributeForTileType(openedType)))));
        }
    }
    return hitHardWall;
}

bool moveBossCharge(
    Enemy& enemy,
    TileMap& map,
    float dt,
    std::vector<EnemyEvent>& events,
    const EnemyPlacementCatalog* placementCatalog)
{
    const Vec2 direction = safeDirection(enemy.bossAction.chargeDirection);
    const float distance = std::max(0.0f, StardustMoleChargeSpeed * dt);
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / (static_cast<float>(balance::TileSize) * 0.35f))));
    const Vec2 step = direction * (distance / static_cast<float>(steps));

    for (int i = 0; i < steps; ++i) {
        const Vec2 next = enemy.position + step;
        if (breakSoftTilesForBossCharge(enemy, map, next, direction, events, placementCatalog)) {
            return false;
        }
        if (map.isCircleBlocked(next, enemyPassageRadius(enemy, placementCatalog))) {
            return false;
        }
        enemy.position = next;
    }
    enemy.velocity = direction * StardustMoleChargeSpeed;
    enemy.facingAngle = std::atan2(direction.y, direction.x);
    return true;
}

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

void moveStardustMoleApproach(
    Enemy& enemy,
    const Player& player,
    TileMap& map,
    float dt,
    const EnemyPlacementCatalog* placementCatalog)
{
    const Vec2 toPlayer = player.position - enemy.position;
    const float distanceToPlayer = length(toPlayer);
    const Vec2 direction = distanceToPlayer > 0.0001f ? toPlayer / distanceToPlayer : facingVector(enemy.facingAngle);
    enemy.facingAngle = std::atan2(direction.y, direction.x);

    const float allowedDistance = std::max(0.0f, distanceToPlayer - StardustMoleApproachStopDistance);
    const float moveDistance = std::min(std::max(0.0f, StardustMoleApproachSpeed * dt), allowedDistance);
    if (moveDistance <= 0.0001f) {
        enemy.velocity = {};
        return;
    }

    const Vec2 previous = enemy.position;
    const int steps = std::max(1, static_cast<int>(std::ceil(moveDistance / (static_cast<float>(balance::TileSize) * 0.35f))));
    const Vec2 step = direction * (moveDistance / static_cast<float>(steps));
    for (int i = 0; i < steps; ++i) {
        if (!tryMoveCircle(map, enemy.position, enemyPassageRadius(enemy, placementCatalog), step)) {
            break;
        }
    }

    const Vec2 moved = enemy.position - previous;
    if (lengthSquared(moved) > 0.0001f) {
        enemy.velocity = moved / std::max(0.001f, dt);
        enemy.facingAngle = std::atan2(moved.y, moved.x);
    } else {
        enemy.velocity = {};
    }
}

bool isJunkCrabBossAction(const Enemy& enemy)
{
    return enemy.bossAction.enabled && enemy.bossAction.pattern == JunkCrabPatternId;
}

float junkCrabPhaseSeconds(const Enemy& enemy, std::string_view key, float fallbackSeconds)
{
    return static_cast<float>(std::max(
        0.05,
        behaviorParamDouble(enemy, "boss_sequence", key, fallbackSeconds)));
}

bool junkCrabHasActiveDebris(const Enemy& enemy)
{
    if (!isJunkCrabBossAction(enemy)) {
        return false;
    }
    return std::any_of(
        enemy.bossAction.junkCrab.debris.begin(),
        enemy.bossAction.junkCrab.debris.end(),
        [](const JunkCrabDebrisRuntime& debris) {
            return debris.state != JunkCrabDebrisState::Inactive;
        });
}

bool junkCrabHasOrbitingDebris(const Enemy& enemy)
{
    if (!isJunkCrabBossAction(enemy)) {
        return false;
    }
    return std::any_of(
        enemy.bossAction.junkCrab.debris.begin(),
        enemy.bossAction.junkCrab.debris.end(),
        [](const JunkCrabDebrisRuntime& debris) {
            return debris.state == JunkCrabDebrisState::Orbiting;
        });
}

int junkCrabDebrisCount(const Enemy& enemy)
{
    const float hpRatio = enemy.maxHp > 0
        ? clamp(static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHp), 0.0f, 1.0f)
        : 1.0f;
    const int countRange = JunkCrabMaxDebris - JunkCrabDebrisMinCount;
    const int scaledCount = JunkCrabDebrisMinCount + static_cast<int>(std::lround((1.0f - hpRatio) * static_cast<float>(countRange)));
    return std::clamp(scaledCount, JunkCrabDebrisMinCount, JunkCrabMaxDebris);
}

float junkCrabOrbitRadius(const Enemy& enemy, const EnemyPlacementCatalog* placementCatalog)
{
    const float radius = enemyPassageRadius(enemy, placementCatalog);
    return std::max(
        radius + JunkCrabDebrisRadius + 8.0f,
        radius * JunkCrabOrbitRadiusMultiplier);
}

float randomJunkCrabGuardRespawnDelay(const Enemy& enemy, std::mt19937& rng)
{
    const float minSeconds = static_cast<float>(std::max(
        0.0,
        behaviorParamDouble(enemy, "boss_sequence", "guardRespawnMinSeconds", JunkCrabGuardRespawnMinSeconds)));
    const float maxSeconds = static_cast<float>(std::max(
        static_cast<double>(minSeconds),
        behaviorParamDouble(enemy, "boss_sequence", "guardRespawnMaxSeconds", JunkCrabGuardRespawnMaxSeconds)));
    std::uniform_real_distribution<float> dist(minSeconds, maxSeconds);
    return dist(rng);
}

float randomJunkCrabDebrisSpin(std::mt19937& rng)
{
    std::uniform_real_distribution<float> speedDist(JunkCrabDebrisSpinMinDegrees, JunkCrabDebrisSpinMaxDegrees);
    std::bernoulli_distribution signDist(0.5);
    const float sign = signDist(rng) ? 1.0f : -1.0f;
    return sign * speedDist(rng) * Pi / 180.0f;
}

void scheduleJunkCrabGuardRespawnIfNeeded(Enemy& enemy, std::mt19937& rng)
{
    JunkCrabBossRuntime& crab = enemy.bossAction.junkCrab;
    if (crab.guardRespawnDelaySeconds > 0.0f) {
        return;
    }
    crab.guardRespawnTimer = 0.0f;
    crab.guardRespawnDelaySeconds = randomJunkCrabGuardRespawnDelay(enemy, rng);
}

void initializeJunkCrabDebris(Enemy& enemy, std::mt19937& rng, const EnemyPlacementCatalog* placementCatalog)
{
    JunkCrabBossRuntime& crab = enemy.bossAction.junkCrab;
    const int count = junkCrabDebrisCount(enemy);
    const float orbitRadius = junkCrabOrbitRadius(enemy, placementCatalog);
    const int debrisHp = std::max(JunkCrabDebrisHp, behaviorParamInt(enemy, "boss_sequence", "debrisHp", JunkCrabDebrisHp));
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * Pi);
    for (int i = 0; i < JunkCrabMaxDebris; ++i) {
        JunkCrabDebrisRuntime& debris = crab.debris[static_cast<std::size_t>(i)];
        if (i >= count) {
            debris = {};
            continue;
        }
        debris = {};
        debris.state = JunkCrabDebrisState::Orbiting;
        debris.angle = static_cast<float>(i) / static_cast<float>(count) * 2.0f * Pi;
        debris.radius = orbitRadius + (i % 2 == 0 ? 0.0f : 6.0f);
        debris.orbitAngularSpeed = static_cast<float>(std::max(
            0.0,
            behaviorParamDouble(enemy, "boss_sequence", "orbitSpeed", JunkCrabOrbitAngularSpeed)));
        debris.position = junkCrabDebrisPosition(enemy, debris);
        debris.visualAngle = angleDist(rng);
        debris.visualAngularSpeed = randomJunkCrabDebrisSpin(rng);
        debris.alpha = 1.0f;
        debris.hp = debrisHp;
        debris.maxHp = debrisHp;
        debris.iconIndex = i;
    }
    crab.debrisVolleyTimer = 0.0f;
    crab.debrisVolleyLaunchTimer = 0.0f;
    crab.guardRespawnTimer = 0.0f;
    crab.guardRespawnDelaySeconds = 0.0f;
}

void enterJunkCrabPhase(
    Enemy& enemy,
    JunkCrabPhase phase,
    std::vector<EnemyEvent>& events)
{
    JunkCrabBossRuntime& crab = enemy.bossAction.junkCrab;
    const bool startsAttackAnimation =
        phase == JunkCrabPhase::ClawWindup ||
        phase == JunkCrabPhase::DebrisVolleyWindup;
    const bool continuesAttackAnimation = phase == JunkCrabPhase::ClawStrike;
    crab.phase = phase;
    crab.timer = 0.0f;
    crab.actionFired = false;
    if (phase == JunkCrabPhase::DebrisVolley) {
        crab.debrisVolleyLaunchTimer = 0.0f;
    }
    if (startsAttackAnimation) {
        crab.attackAnimationSeconds = 0.0f;
        crab.appliedAttackMotionOffset = {};
    } else if (!continuesAttackAnimation) {
        crab.attackAnimationSeconds = 0.0f;
        crab.appliedAttackMotionOffset = {};
    }
    enemy.bossAction.hidden = false;
    enemy.bossAction.invulnerable = false;
    enemy.velocity = {};

    switch (phase) {
    case JunkCrabPhase::RingGuard:
        break;
    case JunkCrabPhase::ClawWindup:
        events.push_back(makeEnemyEvent(EnemyEventType::Attack, enemy, "junk_claw_windup"));
        break;
    case JunkCrabPhase::ClawStrike:
        events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "junk_claw"));
        break;
    case JunkCrabPhase::DebrisVolleyWindup:
        events.push_back(makeEnemyEvent(EnemyEventType::Attack, enemy, "junk_throw_windup"));
        break;
    case JunkCrabPhase::DebrisVolley:
        break;
    case JunkCrabPhase::None:
        break;
    }
}

void beginJunkCrabDebrisFade(JunkCrabDebrisRuntime& debris)
{
    debris.state = JunkCrabDebrisState::Fading;
    debris.timer = 0.0f;
    debris.lifetime = JunkCrabDebrisFadeSeconds;
    debris.alpha = 1.0f;
}

bool tryHitJunkCrabDebris(
    Enemy& enemy,
    SpellRingItem& item,
    const ObjectDefinition* object,
    const RingItemHitboxSpec& itemHitbox,
    const Player& player,
    SpellRingSystem& spellRing,
    const ObjectCatalog& objectCatalog,
    const HitboxCatalog* hitboxCatalog,
    const EnemyPlacementCatalog* placementCatalog,
    std::vector<EnemyEvent>& events,
    std::vector<RingImpactSoundEvent>& impactSoundEvents,
    std::mt19937& rng,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    if (!isJunkCrabBossAction(enemy)) {
        return false;
    }

    JunkCrabBossRuntime& crab = enemy.bossAction.junkCrab;
    for (JunkCrabDebrisRuntime& debris : crab.debris) {
        if (debris.state != JunkCrabDebrisState::Orbiting || debris.hp <= 0) {
            continue;
        }
        if (debris.maxHp < JunkCrabDebrisHp) {
            debris.maxHp = JunkCrabDebrisHp;
            debris.hp = std::max(debris.hp, debris.maxHp);
        }
        const Vec2 debrisPosition = junkCrabDebrisPosition(enemy, debris);
        if (!ringItemHitboxOverlapsCircle(item, itemHitbox, debrisPosition, JunkCrabDebrisRadius)) {
            continue;
        }

        const RingContactDamageResult damage = computeRingContactDamageAgainstEnemy(
            enemy,
            item,
            object,
            itemHitbox,
            player,
            spellRing,
            objectCatalog,
            hitboxCatalog,
            placementCatalog,
            rng,
            discoveryEvents,
            encyclopedia,
            false);
        const int damageDealt = std::max(0, damage.damageDealt);
        debris.hp -= damageDealt;
        const bool broken = debris.hp <= 0 && damageDealt > 0;
        if (broken) {
            debris.position = debrisPosition;
            debris.velocity = safeDirection(enemy.position - debrisPosition, facingVector(enemy.facingAngle)) *
                static_cast<float>(std::max(
                    1.0,
                    behaviorParamDouble(enemy, "boss_sequence", "debrisReturnSpeed", JunkCrabDebrisReturnSpeed)));
            debris.state = JunkCrabDebrisState::ReturningToBoss;
            debris.timer = 0.0f;
            debris.lifetime = 0.0f;
            debris.altitude = 0.0f;
            debris.verticalVelocity = 0.0f;
            debris.alpha = 1.0f;
        }

        item.actionFlashTimer = SpellRingItemActionFlashSeconds;
        if (!item.hasCapturedBehavior("heavy_guard")) {
            spellRing.consumeItemDurability(item);
        }
        impactSoundEvents.push_back(makeEnemyRingImpactSoundEvent(
            item,
            object,
            enemy,
            broken ? RingImpactResult::Break : RingImpactResult::Hit,
            debrisPosition,
            static_cast<float>(damageDealt)));
        EnemyEvent event = makeEnemyEvent(
            EnemyEventType::AttackHit,
            enemy,
            broken ? "junk_debris_break" : "junk_debris_hit",
            damageDealt,
            damage.criticalHit);
        event.position = debrisPosition;
        event.ringItemImpact = true;
        events.push_back(std::move(event));
        return true;
    }
    return false;
}

void bounceJunkCrabDebrisAway(Enemy& enemy, JunkCrabDebrisRuntime& debris, std::mt19937& rng)
{
    std::uniform_real_distribution<float> sideDist(-0.65f, 0.65f);
    std::uniform_real_distribution<float> speedDist(JunkCrabDebrisBounceMinSpeed, JunkCrabDebrisBounceMaxSpeed);
    std::uniform_real_distribution<float> verticalDist(150.0f, 215.0f);
    const Vec2 away = safeDirection(debris.position - enemy.position, fromAngle(debris.angle));
    const Vec2 tangent{-away.y, away.x};
    const Vec2 direction = safeDirection(away + tangent * sideDist(rng), away);
    debris.state = JunkCrabDebrisState::BouncingAway;
    debris.velocity = direction * speedDist(rng);
    debris.timer = 0.0f;
    debris.lifetime = 0.0f;
    debris.altitude = 8.0f;
    debris.verticalVelocity = verticalDist(rng);
    debris.alpha = 1.0f;
}

void updateJunkCrabDebrisMotion(
    Enemy& enemy,
    Player& player,
    TileMap& map,
    float dt,
    std::vector<EnemyEvent>& events,
    std::mt19937& rng)
{
    if (!isJunkCrabBossAction(enemy) || dt <= 0.0f) {
        return;
    }

    JunkCrabBossRuntime& crab = enemy.bossAction.junkCrab;
    const float orbitSpeed = static_cast<float>(std::max(
        0.0,
        behaviorParamDouble(enemy, "boss_sequence", "orbitSpeed", JunkCrabOrbitAngularSpeed)));
    const float returnSpeed = static_cast<float>(std::max(
        1.0,
        behaviorParamDouble(enemy, "boss_sequence", "debrisReturnSpeed", JunkCrabDebrisReturnSpeed)));
    const float volleyLifetime = static_cast<float>(std::max(
        0.1,
        behaviorParamDouble(enemy, "boss_sequence", "debrisVolleyLifetime", JunkCrabDebrisVolleyLifetimeSeconds)));
    const float playerRadius = player.effectiveRadius(balance::PlayerRadius);

    for (JunkCrabDebrisRuntime& debris : crab.debris) {
        if (debris.state == JunkCrabDebrisState::Inactive) {
            continue;
        }
        debris.visualAngle += debris.visualAngularSpeed * dt;

        switch (debris.state) {
        case JunkCrabDebrisState::Orbiting:
            debris.orbitAngularSpeed = orbitSpeed;
            debris.angle = wrapAngle(debris.angle + debris.orbitAngularSpeed * dt);
            debris.position = junkCrabDebrisPosition(enemy, debris);
            break;
        case JunkCrabDebrisState::ReturningToBoss: {
            debris.timer += dt;
            const Vec2 toBoss = enemy.position - debris.position;
            const float distanceToBoss = length(toBoss);
            if (distanceToBoss <= JunkCrabDebrisReturnHitDistance) {
                debris.position = enemy.position;
                bounceJunkCrabDebrisAway(enemy, debris, rng);
                events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "junk_debris_return"));
                break;
            }
            const Vec2 direction = safeDirection(toBoss, facingVector(enemy.facingAngle));
            const float stepDistance = returnSpeed * dt;
            debris.velocity = direction * returnSpeed;
            debris.position += direction * std::min(distanceToBoss, stepDistance);
            break;
        }
        case JunkCrabDebrisState::BouncingAway:
            debris.timer += dt;
            debris.position += debris.velocity * dt;
            debris.velocity = debris.velocity * std::max(0.0f, 1.0f - dt * 0.55f);
            debris.altitude += debris.verticalVelocity * dt;
            debris.verticalVelocity -= JunkCrabDebrisBounceGravity * dt;
            if (debris.altitude <= 0.0f && debris.verticalVelocity <= 0.0f) {
                debris.altitude = 0.0f;
                beginJunkCrabDebrisFade(debris);
            }
            break;
        case JunkCrabDebrisState::Fading:
            debris.timer += dt;
            debris.position += debris.velocity * dt;
            debris.velocity = debris.velocity * std::max(0.0f, 1.0f - dt * 3.0f);
            debris.alpha = 1.0f - clamp(debris.timer / std::max(0.001f, debris.lifetime), 0.0f, 1.0f);
            if (debris.timer >= debris.lifetime) {
                debris = {};
            }
            break;
        case JunkCrabDebrisState::VolleyProjectile: {
            debris.timer += dt;
            debris.position += debris.velocity * dt;
            const bool hitPlayer = circlesOverlap(debris.position, JunkCrabDebrisRadius, player.position, playerRadius);
            if (hitPlayer) {
                const int damage = applyDefenseModifier(
                    player.status,
                    std::max(1, behaviorParamInt(enemy, "boss_sequence", "debrisVolleyDamage", std::max(1, enemy.contactAttackPower))));
                player.applyDamage(
                    damage,
                    DamageCause{
                        .source = DamageSource::SlimeAttack,
                        .actorName = enemyDisplayName(enemy),
                    });
                player.applyKnockback(debris.velocity, 155.0f, 0.14f);
                events.push_back(makeEnemyEvent(EnemyEventType::AttackHit, enemy, "junk_throw", damage));
                beginJunkCrabDebrisFade(debris);
                break;
            }
            if (debris.timer >= volleyLifetime || map.isCircleBlocked(debris.position, JunkCrabDebrisRadius)) {
                beginJunkCrabDebrisFade(debris);
            }
            break;
        }
        case JunkCrabDebrisState::Inactive:
            break;
        }
    }
}

float junkCrabClawReachDistance(
    const Enemy& enemy,
    const Player& player,
    const EnemyPlacementCatalog* placementCatalog)
{
    const float bodyRadius = enemyPassageRadius(enemy, placementCatalog);
    const float playerRadius = player.effectiveRadius(balance::PlayerRadius);
    return bodyRadius + playerRadius + JunkCrabClawRange;
}

bool playerWithinJunkCrabClaw(
    const Enemy& enemy,
    const Player& player,
    const EnemyPlacementCatalog* placementCatalog)
{
    const Vec2 toPlayer = player.position - enemy.position;
    const float distanceToPlayer = length(toPlayer);
    if (distanceToPlayer > junkCrabClawReachDistance(enemy, player, placementCatalog)) {
        return false;
    }
    if (distanceToPlayer <= 0.0001f) {
        return true;
    }
    return angleBetweenDegrees(facingVector(enemy.facingAngle), normalize(toPlayer)) <= JunkCrabClawArcDegrees * 0.5f;
}

void pushPlayerFromJunkCrab(Player& player, TileMap& map, Vec2 direction, float distance)
{
    const Vec2 delta = safeDirection(direction) * distance;
    const Vec2 next = player.position + delta;
    if (!map.isCircleBlocked(next, player.effectiveRadius(balance::PlayerRadius))) {
        player.position = next;
    }
}

void resolveJunkCrabClawStrike(
    Enemy& enemy,
    Player& player,
    TileMap& map,
    std::vector<EnemyEvent>& events,
    const EnemyPlacementCatalog* placementCatalog)
{
    if (!playerWithinJunkCrabClaw(enemy, player, placementCatalog)) {
        return;
    }
    const double baseAttackPower = static_cast<double>(std::max(1, enemy.contactAttackPower + 1)) * 1.5;
    const int damage = std::max(
        1,
        static_cast<int>(std::ceil(enemy.status.applyModifiers(ModifierStat::Attack, baseAttackPower) * damageTypeMultiplier(enemy.contactDamageType))));
    player.applyDamage(
        applyDefenseModifier(player.status, damage),
        DamageCause{
            .source = DamageSource::SlimeAttack,
            .actorName = enemyDisplayName(enemy),
        });

    const Vec2 pushDirection = safeDirection(player.position - enemy.position);
    pushPlayerFromJunkCrab(player, map, pushDirection, 20.0f);
    events.push_back(makeEnemyEvent(EnemyEventType::AttackHit, enemy, "junk_claw", damage));
}

int nearestOrbitingJunkCrabDebrisIndex(const Enemy& enemy, Vec2 target)
{
    const JunkCrabBossRuntime& crab = enemy.bossAction.junkCrab;
    int bestIndex = -1;
    float bestDistanceSq = std::numeric_limits<float>::max();
    for (int i = 0; i < JunkCrabMaxDebris; ++i) {
        const JunkCrabDebrisRuntime& debris = crab.debris[static_cast<std::size_t>(i)];
        if (debris.state != JunkCrabDebrisState::Orbiting) {
            continue;
        }
        const Vec2 position = junkCrabDebrisPosition(enemy, debris);
        const float distanceSq = distanceSquared(position, target);
        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestIndex = i;
        }
    }
    return bestIndex;
}

bool launchNearestJunkCrabDebrisAtPlayer(Enemy& enemy, const Player& player, std::vector<EnemyEvent>& events)
{
    JunkCrabBossRuntime& crab = enemy.bossAction.junkCrab;
    const int debrisIndex = nearestOrbitingJunkCrabDebrisIndex(enemy, player.position);
    if (debrisIndex < 0) {
        return false;
    }

    JunkCrabDebrisRuntime& debris = crab.debris[static_cast<std::size_t>(debrisIndex)];
    const Vec2 startPosition = junkCrabDebrisPosition(enemy, debris);
    const Vec2 direction = safeDirection(player.position - startPosition, facingVector(enemy.facingAngle));
    const float speed = static_cast<float>(std::max(
        1.0,
        behaviorParamDouble(enemy, "boss_sequence", "debrisVolleySpeed", JunkCrabDebrisVolleySpeed)));
    debris.state = JunkCrabDebrisState::VolleyProjectile;
    debris.position = startPosition;
    debris.velocity = direction * speed;
    debris.timer = 0.0f;
    debris.lifetime = 0.0f;
    debris.altitude = 0.0f;
    debris.verticalVelocity = 0.0f;
    debris.alpha = 1.0f;
    events.push_back(makeEnemyEventAt(EnemyEventType::Shoot, enemy, startPosition, "junk_throw"));
    return true;
}

void moveJunkCrabGuard(
    Enemy& enemy,
    const Player& player,
    TileMap& map,
    float dt,
    const EnemyPlacementCatalog* placementCatalog)
{
    const Vec2 toPlayer = player.position - enemy.position;
    const float distanceToPlayer = length(toPlayer);
    if (distanceToPlayer <= junkCrabClawReachDistance(enemy, player, placementCatalog)) {
        return;
    }
    const Vec2 direction = safeDirection(toPlayer, facingVector(enemy.facingAngle));
    const float speed = static_cast<float>(std::clamp(
        behaviorParamDouble(enemy, "boss_sequence", "guardMoveSpeed", JunkCrabGuardMoveSpeed),
        0.0,
        static_cast<double>(JunkCrabGuardMoveSpeed)));
    if (tryMoveCircle(map, enemy.position, enemyPassageRadius(enemy, placementCatalog), direction * (speed * std::max(0.0f, dt)))) {
        enemy.velocity = direction * speed;
    } else {
        enemy.velocity = {};
    }
    enemy.facingAngle = std::atan2(direction.y, direction.x);
}

void advanceJunkCrabAttackBodyMotion(
    Enemy& enemy,
    TileMap& map,
    const EnemyPlacementCatalog* placementCatalog)
{
    JunkCrabBossRuntime& crab = enemy.bossAction.junkCrab;
    if (junkCrabAttackAnimationKind(crab.phase) != JunkCrabAttackAnimationKind::Claw) {
        return;
    }

    const Vec2 targetOffset = junkCrabAttackForwardMotionOffset(enemy, crab.attackAnimationSeconds);
    const Vec2 delta = targetOffset - crab.appliedAttackMotionOffset;
    if (lengthSquared(delta) <= 0.0001f) {
        return;
    }

    const Vec2 previousPosition = enemy.position;
    tryMoveCircle(map, enemy.position, enemyPassageRadius(enemy, placementCatalog), delta);
    crab.appliedAttackMotionOffset += enemy.position - previousPosition;
}

bool updateJunkCrabBossActionSequence(
    Enemy& enemy,
    Player& player,
    TileMap& map,
    float dt,
    std::vector<EnemyEvent>& events,
    std::mt19937& rng,
    const EnemyPlacementCatalog* placementCatalog)
{
    if (!isJunkCrabBossAction(enemy)) {
        return false;
    }

    JunkCrabBossRuntime& crab = enemy.bossAction.junkCrab;
    if (crab.phase == JunkCrabPhase::None) {
        enterJunkCrabPhase(enemy, JunkCrabPhase::RingGuard, events);
    }

    const float safeDt = std::max(0.0f, dt);
    crab.timer += safeDt;
    const JunkCrabAttackAnimationKind attackAnimationKind = junkCrabAttackAnimationKind(crab.phase);
    if (attackAnimationKind != JunkCrabAttackAnimationKind::None) {
        const float animationCap = attackAnimationKind == JunkCrabAttackAnimationKind::DebrisVolley
            ? JunkCrabDebrisVolleyWindupSeconds
            : JunkCrabAttackTotalSeconds;
        crab.attackAnimationSeconds = std::min(
            animationCap,
            crab.attackAnimationSeconds + safeDt);
    }
    updateJunkCrabDebrisMotion(enemy, player, map, safeDt, events, rng);
    const bool activeDebris = junkCrabHasActiveDebris(enemy);
    if (activeDebris) {
        crab.guardRespawnTimer = 0.0f;
        crab.guardRespawnDelaySeconds = 0.0f;
    } else {
        scheduleJunkCrabGuardRespawnIfNeeded(enemy, rng);
        crab.guardRespawnTimer += safeDt;
    }

    const Vec2 toPlayer = player.position - enemy.position;
    const bool facingCanTrack =
        crab.phase == JunkCrabPhase::RingGuard ||
        crab.phase == JunkCrabPhase::DebrisVolleyWindup ||
        (attackAnimationKind == JunkCrabAttackAnimationKind::Claw &&
            !junkCrabAttackLocksFacing(crab.phase, crab.attackAnimationSeconds));
    if (facingCanTrack &&
        lengthSquared(toPlayer) > 0.0001f) {
        enemy.facingAngle = std::atan2(toPlayer.y, toPlayer.x);
    }

    switch (crab.phase) {
    case JunkCrabPhase::RingGuard: {
        moveJunkCrabGuard(enemy, player, map, safeDt, placementCatalog);
        if (!activeDebris) {
            if (crab.guardRespawnTimer >= crab.guardRespawnDelaySeconds) {
                initializeJunkCrabDebris(enemy, rng, placementCatalog);
                events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "junk_ring"));
                return true;
            }
        } else if (junkCrabHasOrbitingDebris(enemy)) {
            crab.debrisVolleyTimer += safeDt;
            const float volleyDelay = junkCrabPhaseSeconds(enemy, "debrisVolleyDelay", JunkCrabDebrisVolleyDelaySeconds);
            if (crab.debrisVolleyTimer >= volleyDelay) {
                enterJunkCrabPhase(enemy, JunkCrabPhase::DebrisVolleyWindup, events);
                return true;
            }
        }

        const float distanceToPlayer = length(player.position - enemy.position);
        if (distanceToPlayer <= junkCrabClawReachDistance(enemy, player, placementCatalog)) {
            enterJunkCrabPhase(enemy, JunkCrabPhase::ClawWindup, events);
            return true;
        }
        return true;
    }
    case JunkCrabPhase::ClawWindup:
        enemy.velocity = {};
        advanceJunkCrabAttackBodyMotion(enemy, map, placementCatalog);
        if (crab.timer >= junkCrabPhaseSeconds(enemy, "clawWindup", JunkCrabClawWindupSeconds)) {
            enterJunkCrabPhase(enemy, JunkCrabPhase::ClawStrike, events);
        }
        return true;
    case JunkCrabPhase::ClawStrike:
        enemy.velocity = {};
        advanceJunkCrabAttackBodyMotion(enemy, map, placementCatalog);
        if (!crab.actionFired) {
            crab.actionFired = true;
            resolveJunkCrabClawStrike(enemy, player, map, events, placementCatalog);
        }
        if (crab.timer >= junkCrabPhaseSeconds(enemy, "clawStrike", JunkCrabClawStrikeSeconds)) {
            enterJunkCrabPhase(enemy, JunkCrabPhase::RingGuard, events);
        }
        return true;
    case JunkCrabPhase::DebrisVolleyWindup:
        enemy.velocity = {};
        if (!junkCrabHasOrbitingDebris(enemy)) {
            enterJunkCrabPhase(enemy, JunkCrabPhase::RingGuard, events);
            return true;
        }
        if (crab.timer >= junkCrabPhaseSeconds(enemy, "debrisVolleyWindup", JunkCrabDebrisVolleyWindupSeconds)) {
            enterJunkCrabPhase(enemy, JunkCrabPhase::DebrisVolley, events);
        }
        return true;
    case JunkCrabPhase::DebrisVolley:
        enemy.velocity = {};
        if (!junkCrabHasOrbitingDebris(enemy)) {
            enterJunkCrabPhase(enemy, JunkCrabPhase::RingGuard, events);
            return true;
        }
        crab.debrisVolleyLaunchTimer -= safeDt;
        while (crab.debrisVolleyLaunchTimer <= 0.0f) {
            if (!launchNearestJunkCrabDebrisAtPlayer(enemy, player, events)) {
                enterJunkCrabPhase(enemy, JunkCrabPhase::RingGuard, events);
                return true;
            }
            crab.debrisVolleyLaunchTimer += junkCrabPhaseSeconds(
                enemy,
                "debrisVolleyInterval",
                JunkCrabDebrisVolleyLaunchIntervalSeconds);
            if (!junkCrabHasOrbitingDebris(enemy)) {
                enterJunkCrabPhase(enemy, JunkCrabPhase::RingGuard, events);
                return true;
            }
        }
        return true;
    case JunkCrabPhase::None:
        break;
    }
    return true;
}

bool isAstragnaBossAction(const Enemy& enemy)
{
    return enemy.bossAction.enabled && enemy.bossAction.pattern == AstragnaPatternId;
}

float astragnaParamFloat(const Enemy& enemy, std::string_view key, float fallback)
{
    return static_cast<float>(behaviorParamDouble(enemy, "boss_sequence", key, fallback));
}

int astragnaParamInt(const Enemy& enemy, std::string_view key, int fallback)
{
    return behaviorParamInt(enemy, "boss_sequence", key, fallback);
}

struct AstragnaShellConfig {
    float coreRadius = 0.0f;
    float innerRadius = 0.0f;
    float outerRadius = 0.0f;
    float layerThickness = 0.0f;
    int layerCount = 0;
    float shellHitRadius = 0.0f;
    TileType shellTileType = TileType::HardRock;
    bool mixedShellTileTypes = true;
};

bool astragnaShellTileTypeParamIsMixed(std::string_view value)
{
    return value.empty() || value == "mixed" || value == "random" || value == "auto" || value == "混合";
}

TileType astragnaShellTileTypeFromParam(std::string_view value)
{
    if (value == "dirt" || value == "Dirt" || value == "土") {
        return TileType::Dirt;
    }
    if (value == "rock" || value == "Rock" || value == "岩") {
        return TileType::Rock;
    }
    if (value == "ore" || value == "Ore" || value == "鉱石") {
        return TileType::Ore;
    }
    return TileType::HardRock;
}

std::uint32_t astragnaShellBlockHash(int layer, int segment)
{
    std::uint32_t h = 0x9E3779B9u;
    h ^= static_cast<std::uint32_t>(layer) + 0x85EBCA6Bu + (h << 6) + (h >> 2);
    h ^= static_cast<std::uint32_t>(segment) + 0xC2B2AE35u + (h << 6) + (h >> 2);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

TileType astragnaShellTileTypeForBlock(const AstragnaShellConfig& config, int layer, int segment)
{
    if (!config.mixedShellTileTypes) {
        return config.shellTileType;
    }

    const std::uint32_t roll = astragnaShellBlockHash(layer, segment) % 100U;
    if (roll < 14U) {
        return TileType::Dirt;
    }
    if (roll < 42U) {
        return TileType::Rock;
    }
    return TileType::HardRock;
}

AstragnaShellConfig astragnaShellConfig(const Enemy& enemy)
{
    const float tileSize = static_cast<float>(balance::TileSize);
    const float coreDiameterTiles = std::max(1.0f, astragnaParamFloat(enemy, "coreDiameterTiles", AstragnaCoreDiameterTiles));
    const float shellThicknessTiles = std::max(1.0f, astragnaParamFloat(enemy, "shellThicknessTiles", AstragnaShellThicknessTiles));
    const float shellGapTiles = std::max(0.0f, astragnaParamFloat(enemy, "shellGapTiles", AstragnaShellGapTiles));
    AstragnaShellConfig config;
    config.coreRadius = coreDiameterTiles * tileSize * 0.5f;
    config.innerRadius = std::max(tileSize, astragnaParamFloat(enemy, "innerShellRadius", config.coreRadius + shellGapTiles * tileSize));
    config.outerRadius = std::max(
        config.innerRadius + tileSize,
        astragnaParamFloat(enemy, "outerShellRadius", config.innerRadius + shellThicknessTiles * tileSize));
    config.layerCount = std::max(1, static_cast<int>(std::round((config.outerRadius - config.innerRadius) / tileSize)));
    config.layerThickness = (config.outerRadius - config.innerRadius) / static_cast<float>(config.layerCount);
    config.shellHitRadius = std::max(4.0f, astragnaParamFloat(enemy, "shellHitRadius", AstragnaShellHitRadius));
    const std::string shellTileTypeParam = behaviorParamString(enemy, "boss_sequence", "shellTileType", "mixed");
    config.mixedShellTileTypes = astragnaShellTileTypeParamIsMixed(shellTileTypeParam);
    config.shellTileType = astragnaShellTileTypeFromParam(shellTileTypeParam);
    return config;
}

float astragnaCoreRadius(const Enemy& enemy)
{
    return astragnaShellConfig(enemy).coreRadius;
}

float astragnaOuterVisualRadius(const Enemy& enemy)
{
    const AstragnaShellConfig config = astragnaShellConfig(enemy);
    const float sealOrbitRadius = std::max(
        config.outerRadius,
        astragnaParamFloat(enemy, "sealOrbitRadius", config.outerRadius + AstragnaSealOrbitGapTiles * static_cast<float>(balance::TileSize)));
    const float sealRadius = std::max(6.0f, astragnaParamFloat(enemy, "sealRadius", AstragnaSealRadius));
    return std::max(config.outerRadius, sealOrbitRadius + sealRadius + 8.0f);
}

float astragnaSealHpMultiplier(int reviveCount)
{
    constexpr std::array<float, 5> Multipliers{{1.0f, 0.85f, 0.70f, 0.60f, 0.50f}};
    return Multipliers[static_cast<std::size_t>(std::clamp(reviveCount, 0, static_cast<int>(Multipliers.size()) - 1))];
}

int astragnaSealMaxHpForRevive(const Enemy& enemy)
{
    const int baseHp = std::max(1, astragnaParamInt(enemy, "sealHp", AstragnaSealMaxHp));
    return std::max(1, static_cast<int>(std::ceil(static_cast<float>(baseHp) * astragnaSealHpMultiplier(enemy.bossAction.astragna.reviveCount))));
}

Vec2 astragnaOrbitPosition(const Enemy& enemy, float localAngle, float orbitRadius)
{
    return enemy.position + fromAngle(enemy.bossAction.astragna.rotationAngle + localAngle) * orbitRadius;
}

Vec2 astragnaSealPartPosition(const Enemy& enemy, const AstragnaSealPartRuntime& part)
{
    return astragnaOrbitPosition(enemy, part.localAngle, part.orbitRadius);
}

Vec2 astragnaShellBlockPosition(const Enemy& enemy, const AstragnaShellBlockRuntime& block)
{
    return astragnaOrbitPosition(enemy, block.localAngle, block.orbitRadius);
}

int astragnaAliveSealPartCount(const Enemy& enemy)
{
    const AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    return static_cast<int>(std::count_if(
        astragna.sealParts.begin(),
        astragna.sealParts.end(),
        [](const AstragnaSealPartRuntime& part) {
            return part.active && part.hp > 0;
        }));
}

void syncAstragnaBodyState(Enemy& enemy)
{
    if (!isAstragnaBossAction(enemy)) {
        return;
    }

    enemy.maxHp = 1;
    enemy.hp = 1;
    enemy.hpBarTimer = 0.0f;
}

void initializeAstragnaBoss(Enemy& enemy)
{
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    if (astragna.initialized) {
        return;
    }

    astragna = AstragnaBossRuntime{};
    astragna.initialized = true;
    astragna.phase = AstragnaPhase::None;
    astragna.repairTimer = astragnaParamFloat(enemy, "repairSeconds", AstragnaRepairIntervalSeconds);

    const AstragnaShellConfig shellConfig = astragnaShellConfig(enemy);
    const int sealMaxHp = astragnaSealMaxHpForRevive(enemy);
    const float sealOrbitRadius = std::max(
        shellConfig.outerRadius,
        astragnaParamFloat(
            enemy,
            "sealOrbitRadius",
            shellConfig.outerRadius + AstragnaSealOrbitGapTiles * static_cast<float>(balance::TileSize)));
    const float sealRadius = std::max(6.0f, astragnaParamFloat(enemy, "sealRadius", AstragnaSealRadius));
    for (int i = 0; i < AstragnaSealPartCount; ++i) {
        AstragnaSealPartRuntime& part = astragna.sealParts[static_cast<std::size_t>(i)];
        part.active = true;
        part.localAngle = -Pi * 0.5f + (2.0f * Pi * static_cast<float>(i)) / static_cast<float>(AstragnaSealPartCount);
        part.orbitRadius = sealOrbitRadius;
        part.radius = sealRadius;
        part.maxHp = sealMaxHp;
        part.hp = part.maxHp;
    }

    const int shellMaxHp = std::max(1, astragnaParamInt(enemy, "shellHp", AstragnaShellMaxHp));
    const float tileSize = static_cast<float>(balance::TileSize);
    astragna.shellBlocks = {};
    astragna.shellBlockCount = 0;
    for (int layer = 0; layer < shellConfig.layerCount; ++layer) {
        const float innerRadius = shellConfig.innerRadius + shellConfig.layerThickness * static_cast<float>(layer);
        const float outerRadius = layer + 1 == shellConfig.layerCount
            ? shellConfig.outerRadius
            : innerRadius + shellConfig.layerThickness;
        const float orbitRadius = (innerRadius + outerRadius) * 0.5f;
        const int segmentCount = std::max(8, static_cast<int>(std::round((2.0f * Pi * orbitRadius) / tileSize)));
        const float angularSpan = (2.0f * Pi) / static_cast<float>(segmentCount);
        const float layerOffset = (layer % 2 == 0) ? 0.0f : angularSpan * 0.5f;
        for (int segment = 0; segment < segmentCount; ++segment) {
            AstragnaShellBlockRuntime block;
            block.active = true;
            block.repairing = false;
            block.layerIndex = layer;
            block.segmentIndex = segment;
            block.segmentCount = segmentCount;
            block.tileType = astragnaShellTileTypeForBlock(shellConfig, layer, segment);
            block.localAngle = layerOffset + angularSpan * (static_cast<float>(segment) + 0.5f);
            block.angularSpan = angularSpan;
            block.orbitRadius = orbitRadius;
            block.radius = shellConfig.shellHitRadius;
            block.innerRadius = innerRadius;
            block.outerRadius = outerRadius;
            block.maxHp = shellMaxHp;
            block.hp = shellMaxHp;
            if (astragna.shellBlockCount >= AstragnaMaxShellBlocks) {
                break;
            }
            astragna.shellBlocks[static_cast<std::size_t>(astragna.shellBlockCount)] = block;
            ++astragna.shellBlockCount;
        }
        if (astragna.shellBlockCount >= AstragnaMaxShellBlocks) {
            break;
        }
    }

    syncAstragnaBodyState(enemy);
}

void reviveAstragnaSealParts(Enemy& enemy)
{
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    const int maxHp = astragnaSealMaxHpForRevive(enemy);
    astragna.sealEmitters = {};
    for (AstragnaSealPartRuntime& part : astragna.sealParts) {
        part.active = true;
        part.maxHp = maxHp;
        part.hp = maxHp;
    }
    syncAstragnaBodyState(enemy);
}

void enterAstragnaPhase(Enemy& enemy, AstragnaPhase phase, std::vector<EnemyEvent>& events)
{
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    const AstragnaPhase previous = astragna.phase;
    astragna.phase = phase;
    astragna.timer = 0.0f;
    enemy.bossAction.hidden = false;
    enemy.bossAction.invulnerable = false;
    enemy.velocity = {};

    switch (phase) {
    case AstragnaPhase::Sealed:
        if (previous == AstragnaPhase::Downed) {
            astragna.reviveCount = std::min(astragna.reviveCount + 1, 4);
            reviveAstragnaSealParts(enemy);
            events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "astragna_reseal"));
        } else {
            events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "astragna_awaken"));
        }
        astragna.repairTimer = astragnaParamFloat(enemy, "repairSeconds", AstragnaRepairIntervalSeconds);
        break;
    case AstragnaPhase::Downed:
        events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "astragna_down"));
        break;
    case AstragnaPhase::Rescued:
        events.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "astragna_rescue"));
        if (!astragna.rescueEventEmitted) {
            events.push_back(makeEnemyEventAt(EnemyEventType::BossResolved, enemy, enemy.position, "astragna_rescue"));
            astragna.rescueEventEmitted = true;
        }
        break;
    case AstragnaPhase::None:
        break;
    }
    syncAstragnaBodyState(enemy);
}

bool astragnaPlayerTooCloseForRepair(const Enemy& enemy, const Player& player, const AstragnaShellBlockRuntime& block)
{
    const float safeRadius = std::max(0.0f, astragnaParamFloat(enemy, "repairSafeRadius", AstragnaRepairPlayerSafeRadius));
    return distanceSquared(astragnaShellBlockPosition(enemy, block), player.position) <= safeRadius * safeRadius;
}

void updateAstragnaRepairs(Enemy& enemy, const Player& player, float dt)
{
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    if (astragna.phase != AstragnaPhase::Sealed) {
        return;
    }

    const int aliveParts = astragnaAliveSealPartCount(enemy);
    if (aliveParts <= 0) {
        return;
    }

    const float repairSeconds = std::max(0.10f, astragnaParamFloat(enemy, "repairSeconds", AstragnaRepairIntervalSeconds));
    astragna.repairTimer -= std::max(0.0f, dt);
    if (astragna.repairTimer > 0.0f) {
        return;
    }
    astragna.repairTimer = repairSeconds;

    int repaired = 0;
    const int blockCount = std::clamp(astragna.shellBlockCount, 0, AstragnaMaxShellBlocks);
    if (blockCount <= 0) {
        return;
    }
    const int attempts = blockCount;
    for (int attempt = 0; attempt < attempts && repaired < aliveParts; ++attempt) {
        const int index = (astragna.repairCursor + attempt) % blockCount;
        AstragnaShellBlockRuntime& block = astragna.shellBlocks[static_cast<std::size_t>(index)];
        if (block.active || astragnaPlayerTooCloseForRepair(enemy, player, block)) {
            continue;
        }
        block.active = true;
        block.repairing = false;
        block.hp = std::max(1, block.maxHp);
        ++repaired;
    }
    astragna.repairCursor = (astragna.repairCursor + std::max(1, attempts / 3)) % blockCount;
    if (repaired > 0) {
        syncAstragnaBodyState(enemy);
    }
}

void resolveAstragnaShellCollision(Enemy& enemy, Player& player, TileMap& map)
{
    if (!isAstragnaBossAction(enemy)) {
        return;
    }

    const float playerRadius = player.effectiveRadius(balance::PlayerRadius);
    for (const AstragnaShellBlockRuntime& block : enemy.bossAction.astragna.shellBlocks) {
        if (!block.active || block.maxHp <= 0) {
            continue;
        }
        const Vec2 blockPosition = astragnaShellBlockPosition(enemy, block);
        Vec2 pushDirection = player.position - blockPosition;
        const float distanceToPlayer = length(pushDirection);
        const float minDistance = playerRadius + block.radius;
        if (distanceToPlayer >= minDistance) {
            continue;
        }
        if (distanceToPlayer <= 0.0001f) {
            pushDirection = player.position - enemy.position;
        }
        if (lengthSquared(pushDirection) <= 0.0001f) {
            pushDirection = {1.0f, 0.0f};
        }
        tryMoveCircle(
            map,
            player.position,
            playerRadius,
            normalize(pushDirection) * (minDistance - distanceToPlayer + AstragnaPlayerPushPadding));
    }
}

bool astragnaGuardianTouched(const Enemy& enemy, const Player& player)
{
    const float starRadius = std::max(8.0f, astragnaParamFloat(enemy, "guardianRadius", astragnaCoreRadius(enemy)));
    const float playerRadius = player.effectiveRadius(balance::PlayerRadius);
    return circlesOverlap(enemy.position, starRadius, player.position, playerRadius);
}

bool astragnaSealPartAliveAt(const AstragnaBossRuntime& astragna, int sealIndex)
{
    if (sealIndex < 0 || sealIndex >= AstragnaSealPartCount) {
        return false;
    }
    const AstragnaSealPartRuntime& part = astragna.sealParts[static_cast<std::size_t>(sealIndex)];
    return part.active && part.hp > 0;
}

bool astragnaEmitterVulnerable(const AstragnaSealEmitterRuntime& emitter)
{
    return emitter.active && !emitter.destroyed && emitter.hp > 0 && emitter.maxHp > 0;
}

void updateAstragnaEmitterPosition(Enemy& enemy, AstragnaSealEmitterRuntime& emitter)
{
    emitter.position = astragnaOrbitPosition(enemy, emitter.localAngle, emitter.orbitRadius);
}

int astragnaActiveEmitterCount(const AstragnaBossRuntime& astragna)
{
    return static_cast<int>(std::count_if(
        astragna.sealEmitters.begin(),
        astragna.sealEmitters.end(),
        [](const AstragnaSealEmitterRuntime& emitter) {
            return astragnaEmitterVulnerable(emitter);
        }));
}

int astragnaActiveFlameEmitterCount(const AstragnaBossRuntime& astragna)
{
    return static_cast<int>(std::count_if(
        astragna.sealEmitters.begin(),
        astragna.sealEmitters.end(),
        [](const AstragnaSealEmitterRuntime& emitter) {
            return astragnaEmitterVulnerable(emitter) && emitter.attack == AstragnaEmitterAttack::FlameSweep;
        }));
}

float astragnaEmitterTelegraphSeconds(const Enemy& enemy, AstragnaEmitterAttack attack)
{
    if (attack == AstragnaEmitterAttack::FlameSweep) {
        return std::max(
            0.05f,
            astragnaParamFloat(enemy, "emitterFlameTelegraphSeconds", AstragnaEmitterFlameTelegraphSeconds));
    }
    return std::max(
        0.05f,
        astragnaParamFloat(enemy, "emitterLaserTelegraphSeconds", AstragnaEmitterLaserTelegraphSeconds));
}

float astragnaEmitterActiveSeconds(const Enemy& enemy, AstragnaEmitterAttack attack)
{
    if (attack == AstragnaEmitterAttack::FlameSweep) {
        return std::max(
            0.05f,
            astragnaParamFloat(enemy, "emitterFlameActiveSeconds", AstragnaEmitterFlameActiveSeconds));
    }
    return std::max(
        0.05f,
        astragnaParamFloat(enemy, "emitterLaserActiveSeconds", AstragnaEmitterLaserActiveSeconds));
}

float astragnaEmitterCooldownSeconds(const Enemy& enemy, AstragnaEmitterAttack attack)
{
    if (attack == AstragnaEmitterAttack::FlameSweep) {
        return std::max(
            0.05f,
            astragnaParamFloat(enemy, "emitterFlameCooldownSeconds", AstragnaEmitterFlameCooldownSeconds));
    }
    return std::max(
        0.05f,
        astragnaParamFloat(enemy, "emitterLaserCooldownSeconds", AstragnaEmitterLaserCooldownSeconds));
}

Vec2 astragnaEmitterAimTarget(const Player& player)
{
    return player.position + player.velocity * AstragnaEmitterLaserLeadSeconds;
}

float angleTo(Vec2 from, Vec2 to)
{
    const Vec2 delta = to - from;
    if (lengthSquared(delta) <= 0.0001f) {
        return 0.0f;
    }
    return std::atan2(delta.y, delta.x);
}

float astragnaEmitterFlameDirectionAngle(const Enemy& enemy, const AstragnaSealEmitterRuntime& emitter)
{
    const float duration = astragnaEmitterActiveSeconds(enemy, AstragnaEmitterAttack::FlameSweep);
    const float elapsed = clamp(duration - emitter.timer, 0.0f, duration);
    const float t = duration > 0.0001f ? elapsed / duration : 0.0f;
    const float sweep = astragnaParamFloat(enemy, "emitterFlameSweepRadians", AstragnaEmitterFlameSweepRadians);
    return emitter.baseDirectionAngle + std::sin(t * Pi * 2.0f) * sweep * emitter.sweepSign;
}

void beginAstragnaEmitterTelegraph(Enemy& enemy, AstragnaSealEmitterRuntime& emitter, const Player& player)
{
    emitter.phase = AstragnaEmitterPhase::Telegraph;
    emitter.timer = astragnaEmitterTelegraphSeconds(enemy, emitter.attack);
    emitter.shotTimer = 0.0f;
    emitter.shotsFired = 0;
    emitter.hitCooldown = 0.0f;
    emitter.baseDirectionAngle = angleTo(emitter.position, astragnaEmitterAimTarget(player));
}

void enterAstragnaEmitterCooldown(Enemy& enemy, AstragnaSealEmitterRuntime& emitter)
{
    emitter.phase = AstragnaEmitterPhase::Cooldown;
    emitter.timer = astragnaEmitterCooldownSeconds(enemy, emitter.attack);
    emitter.shotTimer = 0.0f;
    emitter.shotsFired = 0;
    emitter.hitCooldown = 0.0f;
}

void destroyAstragnaEmitter(AstragnaSealEmitterRuntime& emitter)
{
    emitter.active = false;
    emitter.destroyed = true;
    emitter.phase = AstragnaEmitterPhase::Destroyed;
    emitter.timer = 0.0f;
    emitter.shotTimer = 0.0f;
    emitter.hitCooldown = 0.0f;
    emitter.hp = 0;
}

void destroyAstragnaEmitterForSeal(AstragnaBossRuntime& astragna, int sealIndex)
{
    if (sealIndex < 0 || sealIndex >= AstragnaSealPartCount) {
        return;
    }
    destroyAstragnaEmitter(astragna.sealEmitters[static_cast<std::size_t>(sealIndex)]);
}

AstragnaEmitterAttack astragnaEmitterAttackForSeal(const AstragnaBossRuntime& astragna, int sealIndex)
{
    if ((sealIndex + astragna.reviveCount) % 3 == 1 &&
        astragnaActiveFlameEmitterCount(astragna) < AstragnaMaxActiveFlameEmitters) {
        return AstragnaEmitterAttack::FlameSweep;
    }
    return AstragnaEmitterAttack::LaserBolt;
}

float astragnaEmitterLocalAngleForSeal(const Enemy& enemy, const Player& player, float sealLocalAngle, float orbitRadius)
{
    const AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    const std::array<float, 2> candidates{{
        sealLocalAngle + AstragnaEmitterAngleOffsetRadians,
        sealLocalAngle - AstragnaEmitterAngleOffsetRadians,
    }};

    float bestAngle = candidates.front();
    float bestScore = -1.0f;
    for (float candidate : candidates) {
        const Vec2 position = enemy.position + fromAngle(astragna.rotationAngle + candidate) * orbitRadius;
        const Vec2 toPlayer = player.position - position;
        if (lengthSquared(toPlayer) <= 0.0001f) {
            continue;
        }
        const Vec2 radial = normalize(position - enemy.position);
        const Vec2 tangent{-radial.y, radial.x};
        const float diagonalScore = std::abs(dot(normalize(toPlayer), tangent));
        if (diagonalScore > bestScore) {
            bestScore = diagonalScore;
            bestAngle = candidate;
        }
    }
    return bestAngle;
}

void spawnAstragnaEmitterForSeal(Enemy& enemy, const Player& player, int sealIndex)
{
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    if (!astragnaSealPartAliveAt(astragna, sealIndex)) {
        return;
    }

    AstragnaSealEmitterRuntime& emitter = astragna.sealEmitters[static_cast<std::size_t>(sealIndex)];
    if (emitter.active || emitter.destroyed) {
        return;
    }

    const AstragnaShellConfig shellConfig = astragnaShellConfig(enemy);
    const float orbitRadius = std::max(
        shellConfig.outerRadius,
        astragnaParamFloat(
            enemy,
            "emitterOrbitRadius",
            shellConfig.outerRadius + AstragnaEmitterOrbitGapTiles * static_cast<float>(balance::TileSize)));
    const AstragnaSealPartRuntime& seal = astragna.sealParts[static_cast<std::size_t>(sealIndex)];

    emitter = AstragnaSealEmitterRuntime{};
    emitter.active = true;
    emitter.destroyed = false;
    emitter.sealIndex = sealIndex;
    emitter.attack = astragnaEmitterAttackForSeal(astragna, sealIndex);
    emitter.localAngle = astragnaEmitterLocalAngleForSeal(enemy, player, seal.localAngle, orbitRadius);
    emitter.orbitRadius = orbitRadius;
    emitter.radius = std::max(4.0f, astragnaParamFloat(enemy, "emitterRadius", AstragnaEmitterRadius));
    emitter.maxHp = std::max(1, astragnaParamInt(enemy, "emitterHp", AstragnaEmitterMaxHp));
    emitter.hp = emitter.maxHp;
    emitter.sweepSign = emitter.localAngle >= seal.localAngle ? 1.0f : -1.0f;
    updateAstragnaEmitterPosition(enemy, emitter);
    beginAstragnaEmitterTelegraph(enemy, emitter, player);
}

void updateAstragnaEmitterSpawns(Enemy& enemy, const Player& player)
{
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    if (astragna.phase != AstragnaPhase::Sealed) {
        return;
    }

    const float spawnRange = std::max(
        0.0f,
        astragnaParamFloat(
            enemy,
            "emitterSpawnRange",
            AstragnaEmitterSpawnRangeTiles * static_cast<float>(balance::TileSize)));
    const float spawnRangeSq = spawnRange * spawnRange;
    for (int sealIndex = 0; sealIndex < AstragnaSealPartCount; ++sealIndex) {
        if (astragnaActiveEmitterCount(astragna) >= AstragnaMaxActiveEmitters) {
            return;
        }
        if (!astragnaSealPartAliveAt(astragna, sealIndex)) {
            destroyAstragnaEmitterForSeal(astragna, sealIndex);
            continue;
        }
        const AstragnaSealPartRuntime& seal = astragna.sealParts[static_cast<std::size_t>(sealIndex)];
        if (distanceSquared(astragnaSealPartPosition(enemy, seal), player.position) > spawnRangeSq) {
            continue;
        }
        spawnAstragnaEmitterForSeal(enemy, player, sealIndex);
    }
}

void fireAstragnaEmitterLaser(
    Enemy& enemy,
    AstragnaSealEmitterRuntime& emitter,
    Player& player,
    ProjectileSystem& projectiles,
    std::vector<EnemyEvent>& events)
{
    const Vec2 target = astragnaEmitterAimTarget(player);
    Vec2 direction = target - emitter.position;
    if (lengthSquared(direction) <= 0.0001f) {
        direction = fromAngle(emitter.baseDirectionAngle);
    }
    direction = normalize(direction);

    ProjectileSpawnTuning tuning;
    tuning.speedMultiplier = std::max(0.05f, astragnaParamFloat(enemy, "emitterLaserSpeedMultiplier", 1.0f));
    tuning.damageMultiplier = std::max(0.0f, astragnaParamFloat(enemy, "emitterLaserDamageMultiplier", 1.0f));
    tuning.radiusScale = std::max(0.1f, astragnaParamFloat(enemy, "emitterLaserRadiusScale", 1.0f));
    const ProjectileSpawnMetadata metadata{.sourceActorName = enemyDisplayName(enemy)};
    static const std::vector<EffectSpec> NoEffects;
    if (projectiles.spawn(AstragnaLaserProjectileId, emitter.position + direction * (emitter.radius + 3.0f), direction, ProjectileOwnerType::Enemy, NoEffects, tuning, metadata)) {
        events.push_back(makeEnemyEventAt(EnemyEventType::Shoot, enemy, emitter.position, "astragna_laser"));
    }
}

bool astragnaFlameHitsPlayer(const Enemy& enemy, const AstragnaSealEmitterRuntime& emitter, const Player& player)
{
    const Vec2 toPlayer = player.position - emitter.position;
    const float range = std::max(
        1.0f,
        astragnaParamFloat(
            enemy,
            "emitterFlameRange",
            AstragnaEmitterFlameRangeTiles * static_cast<float>(balance::TileSize)));
    const float playerRadius = player.effectiveRadius(balance::PlayerRadius);
    if (lengthSquared(toPlayer) > (range + playerRadius) * (range + playerRadius)) {
        return false;
    }
    if (lengthSquared(toPlayer) <= 0.0001f) {
        return true;
    }
    const float halfAngle = std::max(
        0.01f,
        astragnaParamFloat(enemy, "emitterFlameHalfAngleRadians", AstragnaEmitterFlameHalfAngleRadians));
    const float directionAngle = astragnaEmitterFlameDirectionAngle(enemy, emitter);
    return std::abs(wrapAngle(std::atan2(toPlayer.y, toPlayer.x) - directionAngle)) <= halfAngle;
}

void updateAstragnaEmitterAttack(
    Enemy& enemy,
    AstragnaSealEmitterRuntime& emitter,
    Player& player,
    ProjectileSystem& projectiles,
    float dt,
    std::vector<EnemyEvent>& events)
{
    if (!astragnaEmitterVulnerable(emitter)) {
        return;
    }

    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    if (astragna.phase != AstragnaPhase::Sealed || !astragnaSealPartAliveAt(astragna, emitter.sealIndex)) {
        destroyAstragnaEmitter(emitter);
        return;
    }

    updateAstragnaEmitterPosition(enemy, emitter);
    const float safeDt = std::max(0.0f, dt);
    emitter.timer -= safeDt;
    emitter.hitCooldown = std::max(0.0f, emitter.hitCooldown - safeDt);

    switch (emitter.phase) {
    case AstragnaEmitterPhase::Telegraph:
        emitter.baseDirectionAngle = angleTo(emitter.position, astragnaEmitterAimTarget(player));
        if (emitter.timer <= 0.0f) {
            emitter.phase = AstragnaEmitterPhase::Active;
            emitter.timer = astragnaEmitterActiveSeconds(enemy, emitter.attack);
            emitter.shotTimer = 0.0f;
            emitter.shotsFired = 0;
            emitter.hitCooldown = 0.0f;
            emitter.baseDirectionAngle = angleTo(emitter.position, player.position);
            events.push_back(makeEnemyEventAt(
                EnemyEventType::BossTelegraph,
                enemy,
                emitter.position,
                emitter.attack == AstragnaEmitterAttack::FlameSweep ? "astragna_flame_start" : "astragna_laser_start"));
        }
        break;
    case AstragnaEmitterPhase::Active:
        if (emitter.attack == AstragnaEmitterAttack::LaserBolt) {
            const int shotCount = std::max(1, astragnaParamInt(enemy, "emitterLaserShotCount", AstragnaEmitterLaserShotCount));
            emitter.shotTimer -= safeDt;
            while (emitter.shotsFired < shotCount && emitter.shotTimer <= 0.0f) {
                fireAstragnaEmitterLaser(enemy, emitter, player, projectiles, events);
                ++emitter.shotsFired;
                emitter.shotTimer += std::max(
                    0.02f,
                    astragnaParamFloat(enemy, "emitterLaserShotIntervalSeconds", AstragnaEmitterLaserShotIntervalSeconds));
            }
        } else if (emitter.attack == AstragnaEmitterAttack::FlameSweep) {
            if (emitter.hitCooldown <= 0.0f && astragnaFlameHitsPlayer(enemy, emitter, player)) {
                const int damage = std::max(0, astragnaParamInt(enemy, "emitterFlameDamage", AstragnaEmitterFlameDamage));
                if (damage > 0) {
                    player.applyDamage(
                        applyDefenseModifier(player.status, damage),
                        DamageCause{
                            .source = DamageSource::Projectile,
                            .actorName = enemyDisplayName(enemy),
                            .objectName = "封印火炎",
                        });
                }
                emitter.hitCooldown = std::max(
                    0.05f,
                    astragnaParamFloat(enemy, "emitterFlameHitIntervalSeconds", AstragnaEmitterFlameHitIntervalSeconds));
            }
        }
        if (emitter.timer <= 0.0f) {
            enterAstragnaEmitterCooldown(enemy, emitter);
        }
        break;
    case AstragnaEmitterPhase::Cooldown:
        if (emitter.timer <= 0.0f) {
            beginAstragnaEmitterTelegraph(enemy, emitter, player);
        }
        break;
    case AstragnaEmitterPhase::Dormant:
        beginAstragnaEmitterTelegraph(enemy, emitter, player);
        break;
    case AstragnaEmitterPhase::Destroyed:
        break;
    }
}

void updateAstragnaEmitters(
    Enemy& enemy,
    Player& player,
    ProjectileSystem& projectiles,
    float dt,
    std::vector<EnemyEvent>& events)
{
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    if (astragna.phase != AstragnaPhase::Sealed) {
        for (AstragnaSealEmitterRuntime& emitter : astragna.sealEmitters) {
            if (astragnaEmitterVulnerable(emitter)) {
                destroyAstragnaEmitter(emitter);
            }
        }
        return;
    }

    updateAstragnaEmitterSpawns(enemy, player);
    for (AstragnaSealEmitterRuntime& emitter : astragna.sealEmitters) {
        updateAstragnaEmitterAttack(enemy, emitter, player, projectiles, dt, events);
    }
}

bool updateAstragnaBossActionSequence(
    Enemy& enemy,
    Player& player,
    TileMap& map,
    ProjectileSystem& projectiles,
    float dt,
    std::vector<EnemyEvent>& events)
{
    if (!isAstragnaBossAction(enemy)) {
        return false;
    }

    initializeAstragnaBoss(enemy);
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    const float safeDt = std::max(0.0f, dt);
    astragna.timer += safeDt;
    astragna.rotationAngle += safeDt * std::max(0.0f, astragnaParamFloat(enemy, "rotationSpeed", AstragnaRotationSpeed));
    enemy.velocity = {};

    if (enemy.bossAction.previewOnly) {
        enemy.bossAction.invulnerable = true;
        return true;
    }

    if (astragna.phase == AstragnaPhase::None) {
        enterAstragnaPhase(enemy, AstragnaPhase::Sealed, events);
        return true;
    }

    resolveAstragnaShellCollision(enemy, player, map);
    updateAstragnaEmitters(enemy, player, projectiles, safeDt, events);
    if (astragna.phase != AstragnaPhase::Rescued && astragnaGuardianTouched(enemy, player)) {
        enterAstragnaPhase(enemy, AstragnaPhase::Rescued, events);
        return true;
    }

    switch (astragna.phase) {
    case AstragnaPhase::Sealed:
        updateAstragnaRepairs(enemy, player, safeDt);
        if (astragnaAliveSealPartCount(enemy) <= 0) {
            enterAstragnaPhase(enemy, AstragnaPhase::Downed, events);
        }
        return true;
    case AstragnaPhase::Downed:
        if (astragna.timer >= std::max(0.1f, astragnaParamFloat(enemy, "downedSeconds", AstragnaDownedSeconds))) {
            enterAstragnaPhase(enemy, AstragnaPhase::Sealed, events);
        }
        return true;
    case AstragnaPhase::Rescued:
        syncAstragnaBodyState(enemy);
        return true;
    case AstragnaPhase::None:
        break;
    }
    return true;
}

int astragnaShellDigDamageForRingHit(
    const Enemy& enemy,
    const SpellRingItem& item,
    const ObjectDefinition* object,
    const SpellRingSystem& spellRing)
{
    const TerrainDigProfile profile = terrainDigProfileFor(object, &item);
    if (!profile.enabled) {
        return 0;
    }

    const int baseDamage = terrainDigDamageForRingHit(profile, item, spellRing, TerrainAttribute::Hard);
    if (baseDamage <= 0) {
        return 0;
    }

    const double multiplier = enemy.bossAction.astragna.phase == AstragnaPhase::Downed
        ? behaviorParamDouble(enemy, "boss_sequence", "downedShellDamageMultiplier", AstragnaDownedShellDamageMultiplier)
        : behaviorParamDouble(enemy, "boss_sequence", "sealedShellDamageMultiplier", AstragnaSealedShellDamageMultiplier);
    return std::max(1, static_cast<int>(std::ceil(static_cast<double>(baseDamage) * std::max(0.0, multiplier))));
}

int astragnaSealDamageForRingHit(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const Player& player)
{
    std::string_view damageType = item.damageType;
    if (item.magicAuraTimer > 0.0f && !item.magicAuraDamageType.empty()) {
        damageType = item.magicAuraDamageType;
    }
    const int speedBonus = static_cast<int>(
        item.damageMotionSpeed *
        0.25f *
        static_cast<float>(
            spellRing.speedDamageMultiplier() *
            spellRing.ringDamageSpeedMultiplierForRing(item.ringIndex)));
    const int modifiedDamage = static_cast<int>(
        player.status.applyModifiers(
            ModifierStat::Attack,
            static_cast<double>(item.damage) *
                damageTypeMultiplier(damageType) *
                item.slashDamageMultiplier *
                spellRing.effectivePowerMultiplier()));
    return std::max(
        0,
        static_cast<int>(std::ceil(static_cast<double>(modifiedDamage + speedBonus) * spellRing.ringOutputMultiplierForRing(item.ringIndex))));
}

bool astragnaAllSealPartsDestroyed(const Enemy& enemy)
{
    return isAstragnaBossAction(enemy) && astragnaAliveSealPartCount(enemy) <= 0;
}

int astragnaTypedProjectileDamage(const Projectile& projectile, int damage)
{
    return std::max(
        0,
        static_cast<int>(std::ceil(static_cast<double>(std::max(0, damage)) * damageTypeMultiplier(projectile.damageType))));
}

int astragnaEmitterDamageForRingHit(
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    const Player& player)
{
    return astragnaSealDamageForRingHit(item, spellRing, player);
}

int astragnaShellDamageForProjectile(const Enemy& enemy, const Projectile& projectile, int damage)
{
    int baseDamage = astragnaTypedProjectileDamage(projectile, damage);
    if (projectile.projectileId == AstragnaLaserProjectileId) {
        baseDamage = std::max(baseDamage, astragnaParamInt(enemy, "reflectedLaserShellDamage", 3));
    }
    if (baseDamage <= 0) {
        return 0;
    }

    const double multiplier = enemy.bossAction.astragna.phase == AstragnaPhase::Downed
        ? behaviorParamDouble(enemy, "boss_sequence", "downedReflectedShellDamageMultiplier", AstragnaDownedReflectedShellDamageMultiplier)
        : behaviorParamDouble(enemy, "boss_sequence", "reflectedShellDamageMultiplier", AstragnaReflectedShellDamageMultiplier);
    if (multiplier <= 0.0) {
        return 0;
    }
    return std::max(1, static_cast<int>(std::ceil(static_cast<double>(baseDamage) * multiplier)));
}

bool damageAstragnaEmitter(
    Enemy& enemy,
    AstragnaSealEmitterRuntime& emitter,
    int damage,
    std::vector<EnemyEvent>& events)
{
    if (!astragnaEmitterVulnerable(emitter) || damage <= 0) {
        return false;
    }

    emitter.hp = std::max(0, emitter.hp - damage);
    const bool broken = emitter.hp <= 0;
    if (broken) {
        destroyAstragnaEmitter(emitter);
    }

    enemy.hitFlash = 0.12f;
    events.push_back(makeEnemyEventAt(
        EnemyEventType::BossImpact,
        enemy,
        emitter.position,
        broken ? "astragna_emitter_break" : "astragna_emitter_hit"));
    return true;
}

bool damageAstragnaSealPart(
    Enemy& enemy,
    int sealIndex,
    int damage,
    std::vector<EnemyEvent>& events)
{
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    if (!astragnaSealPartAliveAt(astragna, sealIndex) || damage <= 0) {
        return false;
    }

    AstragnaSealPartRuntime& part = astragna.sealParts[static_cast<std::size_t>(sealIndex)];
    part.hp = std::max(0, part.hp - damage);
    const bool broken = part.hp <= 0;
    if (broken) {
        part.active = false;
        destroyAstragnaEmitterForSeal(astragna, sealIndex);
    }

    revealEnemyHpBar(enemy, damage);
    enemy.hitFlash = 0.12f;
    syncAstragnaBodyState(enemy);
    events.push_back(makeEnemyEventAt(
        EnemyEventType::AttackHit,
        enemy,
        astragnaSealPartPosition(enemy, part),
        broken ? "astragna_seal_break" : "astragna_seal_hit"));
    if (astragnaAllSealPartsDestroyed(enemy)) {
        enterAstragnaPhase(enemy, AstragnaPhase::Downed, events);
    }
    return true;
}

bool damageAstragnaShellBlock(
    Enemy& enemy,
    AstragnaShellBlockRuntime& block,
    int damage,
    std::vector<EnemyEvent>& events)
{
    if (!block.active || block.hp <= 0 || block.maxHp <= 0 || damage <= 0) {
        return false;
    }

    block.hp = std::max(0, block.hp - damage);
    const bool broken = block.hp <= 0;
    if (broken) {
        block.active = false;
    }

    const Vec2 blockPosition = astragnaShellBlockPosition(enemy, block);
    revealEnemyHpBar(enemy, damage);
    enemy.hitFlash = 0.12f;
    syncAstragnaBodyState(enemy);
    events.push_back(makeEnemyEventAt(
        EnemyEventType::BossImpact,
        enemy,
        blockPosition,
        broken ? "astragna_shell_break" : "astragna_shell_hit"));
    return true;
}

bool tryHitAstragnaBossComponent(
    Enemy& enemy,
    SpellRingItem& item,
    const ObjectDefinition* object,
    const RingItemHitboxSpec& itemHitbox,
    const Player& player,
    SpellRingSystem& spellRing,
    std::vector<EnemyEvent>& events,
    std::vector<RingImpactSoundEvent>& impactSoundEvents)
{
    if (!isAstragnaBossAction(enemy) ||
        enemy.bossAction.previewOnly ||
        enemy.bossAction.astragna.phase == AstragnaPhase::Rescued) {
        return false;
    }

    initializeAstragnaBoss(enemy);
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    for (AstragnaSealEmitterRuntime& emitter : astragna.sealEmitters) {
        if (!astragnaEmitterVulnerable(emitter)) {
            continue;
        }
        if (!ringItemHitboxOverlapsCircle(item, itemHitbox, emitter.position, emitter.radius)) {
            continue;
        }

        const int damage = astragnaEmitterDamageForRingHit(item, spellRing, player);
        if (damage <= 0) {
            return false;
        }
        const Vec2 hitPosition = emitter.position;
        const bool broken = damage >= emitter.hp;
        if (!damageAstragnaEmitter(enemy, emitter, damage, events)) {
            return false;
        }

        item.actionFlashTimer = SpellRingItemActionFlashSeconds;
        if (!item.hasCapturedBehavior("heavy_guard")) {
            spellRing.consumeItemDurability(item);
        }
        impactSoundEvents.push_back(makeEnemyRingImpactSoundEvent(
            item,
            object,
            enemy,
            broken ? RingImpactResult::Break : RingImpactResult::Hit,
            hitPosition,
            static_cast<float>(damage)));
        return true;
    }

    for (int sealIndex = 0; sealIndex < AstragnaSealPartCount; ++sealIndex) {
        AstragnaSealPartRuntime& part = astragna.sealParts[static_cast<std::size_t>(sealIndex)];
        if (!part.active || part.hp <= 0) {
            continue;
        }
        const Vec2 partPosition = astragnaSealPartPosition(enemy, part);
        if (!ringItemHitboxOverlapsCircle(item, itemHitbox, partPosition, part.radius)) {
            continue;
        }

        const int damage = astragnaSealDamageForRingHit(item, spellRing, player);
        if (damage <= 0) {
            return false;
        }
        damageAstragnaSealPart(enemy, sealIndex, damage, events);

        item.actionFlashTimer = SpellRingItemActionFlashSeconds;
        if (!item.hasCapturedBehavior("heavy_guard")) {
            spellRing.consumeItemDurability(item);
        }
        impactSoundEvents.push_back(makeEnemyRingImpactSoundEvent(
            item,
            object,
            enemy,
            RingImpactResult::Hit,
            partPosition,
            static_cast<float>(damage)));
        return true;
    }

    for (AstragnaShellBlockRuntime& block : astragna.shellBlocks) {
        if (!block.active || block.hp <= 0 || block.maxHp <= 0) {
            continue;
        }
        const Vec2 blockPosition = astragnaShellBlockPosition(enemy, block);
        if (!ringItemHitboxOverlapsCircle(item, itemHitbox, blockPosition, block.radius)) {
            continue;
        }

        const int damage = astragnaShellDigDamageForRingHit(enemy, item, object, spellRing);
        if (damage <= 0) {
            return false;
        }
        const bool broken = damage >= block.hp;
        damageAstragnaShellBlock(enemy, block, damage, events);

        item.actionFlashTimer = SpellRingItemActionFlashSeconds;
        if (!item.hasCapturedBehavior("heavy_guard")) {
            spellRing.consumeItemDurability(item);
        }
        impactSoundEvents.push_back(makeEnemyRingImpactSoundEvent(
            item,
            object,
            enemy,
            broken ? RingImpactResult::Break : RingImpactResult::Hit,
            blockPosition,
            static_cast<float>(damage)));
        return true;
    }

    return false;
}

bool tryHitAstragnaWithProjectile(
    Enemy& enemy,
    Projectile& projectile,
    int damage,
    std::vector<EnemyEvent>& events)
{
    if (!isAstragnaBossAction(enemy) ||
        enemy.bossAction.previewOnly ||
        enemy.bossAction.astragna.phase == AstragnaPhase::Rescued) {
        return false;
    }

    initializeAstragnaBoss(enemy);
    AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    for (AstragnaSealEmitterRuntime& emitter : astragna.sealEmitters) {
        if (!astragnaEmitterVulnerable(emitter)) {
            continue;
        }
        if (!circlesOverlap(emitter.position, emitter.radius, projectile.position, projectile.radius)) {
            continue;
        }

        const int typedDamage = astragnaTypedProjectileDamage(projectile, damage);
        return damageAstragnaEmitter(enemy, emitter, typedDamage, events);
    }

    for (int sealIndex = 0; sealIndex < AstragnaSealPartCount; ++sealIndex) {
        AstragnaSealPartRuntime& part = astragna.sealParts[static_cast<std::size_t>(sealIndex)];
        if (!part.active || part.hp <= 0) {
            continue;
        }
        const Vec2 partPosition = astragnaSealPartPosition(enemy, part);
        if (!circlesOverlap(partPosition, part.radius, projectile.position, projectile.radius)) {
            continue;
        }

        const int typedDamage = astragnaTypedProjectileDamage(projectile, damage);
        if (typedDamage <= 0) {
            return false;
        }
        return damageAstragnaSealPart(enemy, sealIndex, typedDamage, events);
    }

    for (AstragnaShellBlockRuntime& block : astragna.shellBlocks) {
        if (!block.active || block.hp <= 0 || block.maxHp <= 0) {
            continue;
        }
        const Vec2 blockPosition = astragnaShellBlockPosition(enemy, block);
        if (!circlesOverlap(blockPosition, block.radius, projectile.position, projectile.radius)) {
            continue;
        }

        const int shellDamage = astragnaShellDamageForProjectile(enemy, projectile, damage);
        return damageAstragnaShellBlock(enemy, block, shellDamage, events);
    }
    return false;
}

std::array<Vec2, 4> astragnaShellBlockCorners(const Enemy& enemy, const AstragnaShellBlockRuntime& block)
{
    const float startAngle = enemy.bossAction.astragna.rotationAngle + block.localAngle - block.angularSpan * 0.5f;
    const float endAngle = startAngle + block.angularSpan;
    return {{
        enemy.position + fromAngle(startAngle) * block.innerRadius,
        enemy.position + fromAngle(endAngle) * block.innerRadius,
        enemy.position + fromAngle(endAngle) * block.outerRadius,
        enemy.position + fromAngle(startAngle) * block.outerRadius,
    }};
}

bool astragnaShellHasActiveBlockAt(const AstragnaBossRuntime& astragna, int layer, float localAngle)
{
    if (layer < 0) {
        return false;
    }
    const int blockCount = std::clamp(astragna.shellBlockCount, 0, AstragnaMaxShellBlocks);
    for (int i = 0; i < blockCount; ++i) {
        const AstragnaShellBlockRuntime& candidate = astragna.shellBlocks[static_cast<std::size_t>(i)];
        if (candidate.layerIndex != layer || !candidate.active || candidate.maxHp <= 0) {
            continue;
        }
        const float halfSpan = std::max(candidate.angularSpan * 0.5f, 0.0001f);
        if (std::abs(wrapAngle(localAngle - candidate.localAngle)) <= halfSpan + 0.0005f) {
            return true;
        }
    }
    return false;
}

TerrainTileNeighbors astragnaShellTileNeighbors(const AstragnaBossRuntime& astragna, const AstragnaShellBlockRuntime& block)
{
    const float angle = block.localAngle;
    const float span = block.angularSpan;
    return TerrainTileNeighbors{
        .up = !astragnaShellHasActiveBlockAt(astragna, block.layerIndex - 1, angle),
        .down = !astragnaShellHasActiveBlockAt(astragna, block.layerIndex + 1, angle),
        .left = !astragnaShellHasActiveBlockAt(astragna, block.layerIndex, angle - span),
        .right = !astragnaShellHasActiveBlockAt(astragna, block.layerIndex, angle + span),
        .upLeft = !astragnaShellHasActiveBlockAt(astragna, block.layerIndex - 1, angle - span),
        .upRight = !astragnaShellHasActiveBlockAt(astragna, block.layerIndex - 1, angle + span),
        .downLeft = !astragnaShellHasActiveBlockAt(astragna, block.layerIndex + 1, angle - span),
        .downRight = !astragnaShellHasActiveBlockAt(astragna, block.layerIndex + 1, angle + span),
    };
}

Vec2 astragnaWallOrnamentTexCoord(const Enemy& enemy, Vec2 worldPosition, float outerRadius)
{
    const Vec2 delta = worldPosition - enemy.position;
    const float rotation = enemy.bossAction.astragna.rotationAngle;
    const float c = std::cos(rotation);
    const float s = std::sin(rotation);
    const Vec2 local{
        delta.x * c + delta.y * s,
        -delta.x * s + delta.y * c,
    };
    const float radius = std::max(1.0f, outerRadius);
    return {
        0.5f + (local.x / radius) * AstragnaWallOrnamentSourceOuterRadiusUv,
        0.5f + (local.y / radius) * AstragnaWallOrnamentSourceOuterRadiusUv,
    };
}

void drawAstragnaWallOrnament(Renderer& renderer, const Enemy& enemy, bool downed)
{
    const ImageHandle handle = renderer.acquireImage(AstragnaWallOrnamentImagePath, TextureFilter::Linear);
    if (!handle.valid()) {
        return;
    }

    const AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    const AstragnaShellConfig shellConfig = astragnaShellConfig(enemy);
    const Color tint = downed ? Color{220, 246, 255, 172} : Color{255, 255, 255, 212};
    constexpr std::array<int, 6> QuadIndices{{0, 1, 2, 0, 2, 3}};
    const int blockCount = std::clamp(astragna.shellBlockCount, 0, AstragnaMaxShellBlocks);
    for (int i = 0; i < blockCount; ++i) {
        const AstragnaShellBlockRuntime& block = astragna.shellBlocks[static_cast<std::size_t>(i)];
        if (!block.active || block.hp <= 0 || block.maxHp <= 0) {
            continue;
        }
        const std::array<Vec2, 4> corners = astragnaShellBlockCorners(enemy, block);
        const std::array<ImageTriangleVertex, 4> vertices{{
            {corners[0], astragnaWallOrnamentTexCoord(enemy, corners[0], shellConfig.outerRadius)},
            {corners[1], astragnaWallOrnamentTexCoord(enemy, corners[1], shellConfig.outerRadius)},
            {corners[2], astragnaWallOrnamentTexCoord(enemy, corners[2], shellConfig.outerRadius)},
            {corners[3], astragnaWallOrnamentTexCoord(enemy, corners[3], shellConfig.outerRadius)},
        }};
        renderer.drawImageTriangleList(handle, vertices.data(), vertices.size(), QuadIndices.data(), QuadIndices.size(), tint);
    }
}

float astragnaDronePulse(float timer, Vec2 position)
{
    const float phase = timer * 5.4f + position.x * 0.017f + position.y * 0.011f;
    return 0.5f + 0.5f * std::sin(phase);
}

void drawAstragnaDrone(Renderer& renderer, Vec2 position, float radius, float hpRatio, float timer, float alphaScale)
{
    const float pulse = astragnaDronePulse(timer, position);
    const float hover = std::sin(timer * 4.1f + position.x * 0.009f) * 1.6f;
    const Vec2 drawPosition = position + Vec2{0.0f, hover};
    const unsigned char glowAlpha = static_cast<unsigned char>(std::lround((52.0f + 92.0f * pulse) * alphaScale));
    const unsigned char coreAlpha = static_cast<unsigned char>(std::lround((112.0f + 96.0f * pulse) * alphaScale));

    renderer.fillSoftCircle(drawPosition, radius * (1.65f + 0.28f * pulse), {82, 230, 255, glowAlpha});
    renderer.drawSoftRing(drawPosition, radius * (1.02f + 0.04f * pulse), 2.0f + pulse * 1.8f, {190, 250, 255, coreAlpha});

    ImageDrawOptions options;
    options.tint = {255, 255, 255, static_cast<unsigned char>(std::lround(245.0f * alphaScale))};
    const float spriteDiameter = radius * (4.15f + 0.10f * std::sin(timer * 4.1f));
    const bool drewImage = renderer.drawImage(
        AstragnaDroneImagePath,
        drawPosition,
        {spriteDiameter, spriteDiameter},
        options,
        TextureFilter::Linear);
    if (!drewImage) {
        renderer.fillCircle(drawPosition, radius, {108, 222, 255, options.tint.a});
        renderer.drawCircle(drawPosition, radius + 3.0f, {234, 252, 255, 190});
    }

    const float clampedHp = clamp(hpRatio, 0.0f, 1.0f);
    if (clampedHp < 0.98f) {
        renderer.drawCircle(drawPosition, std::max(2.0f, radius * (1.25f + 0.18f * (1.0f - clampedHp))), {255, 128, 166, 210});
    }
}

void drawAstragnaEmitterSector(
    Renderer& renderer,
    Vec2 origin,
    float directionAngle,
    float range,
    float halfAngle,
    Color fill,
    Color edge)
{
    constexpr int SegmentCount = 14;
    std::array<Vec2, SegmentCount + 2> points{};
    points[0] = origin;
    for (int i = 0; i <= SegmentCount; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(SegmentCount);
        const float angle = directionAngle - halfAngle + halfAngle * 2.0f * t;
        points[static_cast<std::size_t>(i + 1)] = origin + fromAngle(angle) * range;
    }
    renderer.fillPolygon(points.data(), points.size(), fill);
    renderer.drawSoftLine(origin, points[1], 3.0f, edge);
    renderer.drawSoftLine(origin, points[points.size() - 1], 3.0f, edge);
}

void drawAstragnaEmitterAttack(Renderer& renderer, const Enemy& enemy, const AstragnaSealEmitterRuntime& emitter)
{
    if (!astragnaEmitterVulnerable(emitter)) {
        return;
    }

    const float telegraphPulse = 0.65f + 0.35f * std::sin(enemy.behaviorTimer * 10.0f);
    if (emitter.attack == AstragnaEmitterAttack::LaserBolt) {
        if (emitter.phase == AstragnaEmitterPhase::Telegraph) {
            const Vec2 direction = fromAngle(emitter.baseDirectionAngle);
            const float length = std::max(180.0f, emitter.orbitRadius * 1.35f);
            renderer.drawSoftLine(
                emitter.position,
                emitter.position + direction * length,
                2.0f + telegraphPulse * 2.0f,
                {126, 234, 255, static_cast<unsigned char>(90 + std::lround(72.0f * telegraphPulse))});
        }
        return;
    }

    if (emitter.attack != AstragnaEmitterAttack::FlameSweep) {
        return;
    }

    const float range = std::max(
        1.0f,
        astragnaParamFloat(
            enemy,
            "emitterFlameRange",
            AstragnaEmitterFlameRangeTiles * static_cast<float>(balance::TileSize)));
    const float halfAngle = std::max(
        0.01f,
        astragnaParamFloat(enemy, "emitterFlameHalfAngleRadians", AstragnaEmitterFlameHalfAngleRadians));
    if (emitter.phase == AstragnaEmitterPhase::Telegraph) {
        drawAstragnaEmitterSector(
            renderer,
            emitter.position,
            emitter.baseDirectionAngle,
            range,
            halfAngle,
            {255, 92, 42, static_cast<unsigned char>(36 + std::lround(34.0f * telegraphPulse))},
            {255, 168, 74, static_cast<unsigned char>(96 + std::lround(62.0f * telegraphPulse))});
    } else if (emitter.phase == AstragnaEmitterPhase::Active) {
        const float directionAngle = astragnaEmitterFlameDirectionAngle(enemy, emitter);
        drawAstragnaEmitterSector(
            renderer,
            emitter.position,
            directionAngle,
            range,
            halfAngle,
            {255, 104, 32, 116},
            {255, 226, 96, 190});
        renderer.drawSoftLine(
            emitter.position,
            emitter.position + fromAngle(directionAngle) * range,
            10.0f,
            {255, 178, 52, 150});
    }
}

void drawAstragnaEmitterBody(Renderer& renderer, const Enemy& enemy, const AstragnaSealEmitterRuntime& emitter)
{
    if (!astragnaEmitterVulnerable(emitter)) {
        return;
    }

    const float hpRatio = emitter.maxHp > 0 ? clamp(static_cast<float>(emitter.hp) / static_cast<float>(emitter.maxHp), 0.0f, 1.0f) : 1.0f;
    drawAstragnaDrone(renderer, emitter.position, emitter.radius, hpRatio, enemy.behaviorTimer, 1.0f);
}

void drawAstragnaCore(Renderer& renderer, const Enemy& enemy, bool downed)
{
    const float coreRadius = std::max(8.0f, astragnaParamFloat(enemy, "guardianRadius", astragnaCoreRadius(enemy)));
    const float starPulse = 1.0f + 0.035f * std::sin(enemy.behaviorTimer * 4.7f);
    const unsigned char auraAlpha = downed ? 112 : 72;
    renderer.fillSoftCircle(enemy.position, coreRadius * (0.62f + 0.05f * starPulse), {255, 198, 72, auraAlpha});

    ImageDrawOptions options;
    options.tint = {255, 255, 255, static_cast<unsigned char>(downed ? 255 : 238)};
    const float starDiameter = coreRadius * (2.03f + 0.04f * starPulse);
    const bool drewCoreImage = renderer.drawImage(
        AstragnaGuardianStarImagePath,
        enemy.position,
        {starDiameter, starDiameter},
        options,
        TextureFilter::Linear);

    if (!drewCoreImage) {
        const unsigned char starFillAlpha = downed ? 220 : 164;
        const unsigned char starRingAlpha = downed ? 190 : 108;
        renderer.fillCircle(enemy.position, coreRadius * starPulse, {255, 238, 152, starFillAlpha});
        renderer.drawCircle(enemy.position, coreRadius * starPulse + 4.0f, {118, 220, 255, starRingAlpha});
    }
}

void drawAstragnaBoss(Renderer& renderer, const TileMap& map, const Enemy& enemy)
{
    if (!isAstragnaBossAction(enemy) || !enemy.bossAction.astragna.initialized || enemy.spawnTimer > 0.0f) {
        return;
    }

    const AstragnaBossRuntime& astragna = enemy.bossAction.astragna;
    const bool downed = astragna.phase == AstragnaPhase::Downed;
    constexpr std::array<int, 6> QuadIndices{{0, 1, 2, 0, 2, 3}};
    const auto drawShellTile = [&](const std::array<Vec2, 4>& corners, const AstragnaShellBlockRuntime& block, Color tint) {
        if (!map.renderTileQuadAutotiled(
                renderer,
                corners,
                0,
                block.tileType,
                block.segmentIndex,
                block.layerIndex,
                tint,
                astragnaShellTileNeighbors(astragna, block))) {
            renderer.fillTriangleList(corners.data(), corners.size(), QuadIndices.data(), QuadIndices.size(), tint);
        }
    };
    for (const AstragnaShellBlockRuntime& block : astragna.shellBlocks) {
        if (block.maxHp <= 0) {
            continue;
        }
        const std::array<Vec2, 4> corners = astragnaShellBlockCorners(enemy, block);
        if (!block.active) {
            drawShellTile(corners, block, {118, 92, 148, 62});
            continue;
        }
        const Color tileTint = downed
            ? Color{156, 220, 255, 222}
            : Color{188, 168, 222, 238};
        drawShellTile(corners, block, tileTint);
    }

    drawAstragnaWallOrnament(renderer, enemy, downed);

    for (const AstragnaShellBlockRuntime& block : astragna.shellBlocks) {
        if (!block.active || block.hp <= 0 || block.maxHp <= 0) {
            continue;
        }
        const float hpRatio = block.maxHp > 0 ? clamp(static_cast<float>(block.hp) / static_cast<float>(block.maxHp), 0.0f, 1.0f) : 1.0f;
        if (hpRatio < 0.98f) {
            const std::array<Vec2, 4> corners = astragnaShellBlockCorners(enemy, block);
            renderer.fillTriangleList(
                corners.data(),
                corners.size(),
                QuadIndices.data(),
                QuadIndices.size(),
                {255, 246, 178, static_cast<unsigned char>(std::round((1.0f - hpRatio) * 92.0f))});
        }
    }

    for (const AstragnaSealEmitterRuntime& emitter : astragna.sealEmitters) {
        drawAstragnaEmitterAttack(renderer, enemy, emitter);
    }

    drawAstragnaCore(renderer, enemy, downed);

    for (const AstragnaSealPartRuntime& part : astragna.sealParts) {
        if (!part.active || part.hp <= 0) {
            continue;
        }
        const Vec2 position = astragnaSealPartPosition(enemy, part);
        const float hpRatio = part.maxHp > 0 ? clamp(static_cast<float>(part.hp) / static_cast<float>(part.maxHp), 0.0f, 1.0f) : 1.0f;
        drawAstragnaDrone(renderer, position, part.radius, hpRatio, enemy.behaviorTimer + part.localAngle, 0.92f);
    }

    for (const AstragnaSealEmitterRuntime& emitter : astragna.sealEmitters) {
        drawAstragnaEmitterBody(renderer, enemy, emitter);
    }
}

}

const EnemyDefinition* EnemySystem::chooseEnemyDefinition(const EnemyCatalog& enemyCatalog)
{
    if (enemyCatalog.enemies.empty()) {
        return nullptr;
    }
    std::uniform_int_distribution<std::size_t> dist(0, enemyCatalog.enemies.size() - 1);
    return &enemyCatalog.enemies[dist(rng_)];
}

const EnemyDefinition* EnemySystem::chooseNormalRandomEnemyDefinition(const EnemyCatalog& enemyCatalog)
{
    std::vector<const EnemyDefinition*> candidates;
    std::vector<double> weights;
    candidates.reserve(enemyCatalog.enemies.size());
    weights.reserve(enemyCatalog.enemies.size());
    for (const EnemyDefinition& definition : enemyCatalog.enemies) {
        if (!isExcludedFromNormalDugSpawn(definition)) {
            const double weight = spawnBiasMultiplierFor(definition);
            if (weight <= 0.0) {
                continue;
            }
            candidates.push_back(&definition);
            weights.push_back(weight);
        }
    }

    if (candidates.empty()) {
        logSpawnWeightFallbackOnce(
            "normal_random_no_candidates",
            "Enemy spawn weight fallback: no non-boss Enemies candidates; using legacy all-enemies random");
        return chooseEnemyDefinition(enemyCatalog);
    }

    const auto selected = selectWeightedIndex(weights, rng_);
    if (!selected || *selected >= candidates.size()) {
        logSpawnWeightFallbackOnce(
            "normal_random_select_failed",
            "Enemy spawn weight fallback: simple weighted selection failed; using legacy all-enemies random");
        return chooseEnemyDefinition(enemyCatalog);
    }
    return candidates[*selected];
}

EnemySystem::EnemySpawnSelection EnemySystem::chooseDugSpawnEnemy(const EnemyCatalog& enemyCatalog, std::string_view stageId, int depthRank)
{
    if (enemyCatalog.enemies.empty()) {
        logSpawnWeightFallbackOnce(
            "empty_catalog",
            "Enemy spawn weight fallback: EnemyCatalog is empty; using default runtime enemy");
        return {};
    }

    if (isRoguelikeEnemyLevelSelectionStage(stageId)) {
        const int targetBaseLevel = roguelikeTargetBaseLevelForDepthRank(depthRank);
        const std::string poolKey = roguelikeEnemyPoolKey(stageId, depthRank);

        auto poolIt = roguelikeEnemyPools_.find(poolKey);
        if (poolIt == roguelikeEnemyPools_.end()) {
            struct Candidate {
                const EnemyDefinition* definition = nullptr;
                EnemyVariantTier variantTier = EnemyVariantTier::Normal;
                int effectiveBaseLevel = 1;
                int distance = 0;
            };

            std::vector<Candidate> allCandidates;
            allCandidates.reserve(enemyCatalog.enemies.size() * enemySpawnVariantTiers().size());
            for (const EnemyDefinition& definition : enemyCatalog.enemies) {
                if (isExcludedFromNormalDugSpawn(definition) || definition.id.empty() || definition.baseLevel <= 0) {
                    continue;
                }
                for (EnemyVariantTier tier : enemySpawnVariantTiers()) {
                    const int effectiveLevel = definition.baseLevel + enemyVariantLevelBonus(tier);
                    allCandidates.push_back(Candidate{
                        .definition = &definition,
                        .variantTier = tier,
                        .effectiveBaseLevel = effectiveLevel,
                        .distance = std::abs(effectiveLevel - targetBaseLevel),
                    });
                }
            }

            std::vector<Candidate> filtered;
            constexpr int MinPoolSize = 6;
            constexpr int MaxPoolSize = 8;
            for (int range = 1; range <= 70; ++range) {
                filtered.clear();
                for (const Candidate& candidate : allCandidates) {
                    if (candidate.distance <= range) {
                        filtered.push_back(candidate);
                    }
                }
                if (static_cast<int>(filtered.size()) >= MinPoolSize || range == 70) {
                    break;
                }
            }

            std::vector<RoguelikeEnemyPoolEntry> pool;
            if (!filtered.empty()) {
                const int poolSize = std::min(
                    static_cast<int>(filtered.size()),
                    std::uniform_int_distribution<int>(MinPoolSize, MaxPoolSize)(rng_));
                pool.reserve(static_cast<std::size_t>(poolSize));
                while (static_cast<int>(pool.size()) < poolSize && !filtered.empty()) {
                    std::vector<double> weights;
                    weights.reserve(filtered.size());
                    for (const Candidate& candidate : filtered) {
                        weights.push_back(
                            std::max(0.0, spawnBiasMultiplierFor(*candidate.definition)) *
                            (1.0 / (1.0 + static_cast<double>(candidate.distance))));
                    }
                    const auto selected = selectWeightedIndex(weights, rng_);
                    if (!selected || *selected >= filtered.size()) {
                        break;
                    }
                    const Candidate& candidate = filtered[*selected];
                    pool.push_back(RoguelikeEnemyPoolEntry{
                        .enemyId = candidate.definition->id,
                        .variantTier = candidate.variantTier,
                        .effectiveBaseLevel = candidate.effectiveBaseLevel,
                    });
                    filtered.erase(filtered.begin() + static_cast<std::ptrdiff_t>(*selected));
                }
            }

            poolIt = roguelikeEnemyPools_.emplace(poolKey, std::move(pool)).first;
            if (poolIt->second.empty()) {
                logSpawnWeightFallbackOnce(
                    "roguelike_no_candidates:" + poolKey,
                    "Enemy spawn weight fallback: no roguelike level candidates stage=\"" + std::string(stageId) +
                        "\" depth=" + std::to_string(std::max(1, depthRank)) +
                        " target_level=" + std::to_string(targetBaseLevel) +
                        "; using simple random");
            }
        }

        if (!poolIt->second.empty()) {
            std::vector<const RoguelikeEnemyPoolEntry*> candidates;
            std::vector<double> weights;
            candidates.reserve(poolIt->second.size());
            weights.reserve(poolIt->second.size());
            for (const RoguelikeEnemyPoolEntry& entry : poolIt->second) {
                const auto definitionIt = enemyCatalog.enemiesById.find(entry.enemyId);
                if (definitionIt == enemyCatalog.enemiesById.end()) {
                    continue;
                }
                const double weight = std::max(0.0, spawnBiasMultiplierFor(definitionIt->second)) *
                    (1.0 / (1.0 + static_cast<double>(std::abs(entry.effectiveBaseLevel - targetBaseLevel))));
                if (weight <= 0.0) {
                    continue;
                }
                candidates.push_back(&entry);
                weights.push_back(weight);
            }
            const auto selected = selectWeightedIndex(weights, rng_);
            if (selected && *selected < candidates.size()) {
                const RoguelikeEnemyPoolEntry& entry = *candidates[*selected];
                const auto definitionIt = enemyCatalog.enemiesById.find(entry.enemyId);
                if (definitionIt != enemyCatalog.enemiesById.end()) {
                    return EnemySpawnSelection{
                        .definition = &definitionIt->second,
                        .variantTier = entry.variantTier,
                        .effectiveBaseLevel = std::max(1, entry.effectiveBaseLevel),
                    };
                }
            }
        }

        const EnemyDefinition* fallback = chooseNormalRandomEnemyDefinition(enemyCatalog);
        return EnemySpawnSelection{
            .definition = fallback,
            .variantTier = EnemyVariantTier::Normal,
            .effectiveBaseLevel = fallback != nullptr ? std::max(1, fallback->baseLevel) : 1,
        };
    }

    const std::string columnName = resolveEnemySpawnWeightColumnName(stageId, depthRank);
    if (columnName.empty()) {
        logSpawnWeightFallbackOnce(
            "unknown_stage:" + std::string(stageId),
            "Enemy spawn weight fallback: unknown stageId=\"" + std::string(stageId) + "\"; using simple random");
        const EnemyDefinition* fallback = chooseNormalRandomEnemyDefinition(enemyCatalog);
        return EnemySpawnSelection{
            .definition = fallback,
            .variantTier = EnemyVariantTier::Normal,
            .effectiveBaseLevel = fallback != nullptr ? std::max(1, fallback->baseLevel) : 1,
        };
    }

    std::vector<const EnemyDefinition*> candidates;
    std::vector<double> weights;
    candidates.reserve(enemyCatalog.enemies.size());
    weights.reserve(enemyCatalog.enemies.size());
    for (const EnemyDefinition& definition : enemyCatalog.enemies) {
        if (isExcludedFromNormalDugSpawn(definition)) {
            continue;
        }
        const auto weightIt = definition.spawnWeights.find(columnName);
        if (weightIt == definition.spawnWeights.end() || weightIt->second < 1.0) {
            continue;
        }
        const double weight = weightIt->second * spawnBiasMultiplierFor(definition);
        if (weight <= 0.0) {
            continue;
        }
        candidates.push_back(&definition);
        weights.push_back(weight);
    }

    if (candidates.empty()) {
        logSpawnWeightFallbackOnce(
            "no_candidates:" + columnName,
            "Enemy spawn weight fallback: no candidates for column=\"" + columnName + "\"; using simple random");
        const EnemyDefinition* fallback = chooseNormalRandomEnemyDefinition(enemyCatalog);
        return EnemySpawnSelection{
            .definition = fallback,
            .variantTier = EnemyVariantTier::Normal,
            .effectiveBaseLevel = fallback != nullptr ? std::max(1, fallback->baseLevel) : 1,
        };
    }

    const auto selected = selectWeightedIndex(weights, rng_);
    if (!selected || *selected >= candidates.size()) {
        logSpawnWeightFallbackOnce(
            "select_failed:" + columnName,
            "Enemy spawn weight fallback: weighted selection failed for column=\"" + columnName + "\"; using simple random");
        const EnemyDefinition* fallback = chooseNormalRandomEnemyDefinition(enemyCatalog);
        return EnemySpawnSelection{
            .definition = fallback,
            .variantTier = EnemyVariantTier::Normal,
            .effectiveBaseLevel = fallback != nullptr ? std::max(1, fallback->baseLevel) : 1,
        };
    }
    const EnemyDefinition* selectedDefinition = candidates[*selected];
    return EnemySpawnSelection{
        .definition = selectedDefinition,
        .variantTier = EnemyVariantTier::Normal,
        .effectiveBaseLevel = selectedDefinition != nullptr ? std::max(1, selectedDefinition->baseLevel) : 1,
    };
}

const EnemyDefinition* EnemySystem::chooseDugSpawnEnemyDefinition(
    const EnemyCatalog& enemyCatalog,
    std::string_view stageId,
    int depthRank)
{
    return chooseDugSpawnEnemy(enemyCatalog, stageId, depthRank).definition;
}

double EnemySystem::spawnBiasMultiplierFor(const EnemyDefinition& definition) const
{
    double result = SpawnBiasDefaultMultiplier;
    for (const auto& [group, multiplier] : spawnBiasMultipliers_) {
        if (enemyDefinitionMatchesSpawnBiasGroup(definition, group)) {
            result *= multiplier;
        }
    }
    return std::clamp(result, SpawnBiasMinMultiplier, SpawnBiasMaxMultiplier);
}

void EnemySystem::logSpawnWeightFallbackOnce(std::string key, std::string message)
{
    if (loggedSpawnWeightFallbacks_.insert(std::move(key)).second) {
        logError("[warning] " + std::move(message));
    }
}

void EnemySystem::queueEnemyObjectDrops(Enemy& enemy)
{
    if (!enemy.dropItemEnabled || enemy.dropItemConsumed) {
        return;
    }
    enemy.dropItemConsumed = true;
    if (enemy.dropItemProfile.empty()) {
        return;
    }
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
    if (chanceDist(rng_) > clamp(enemy.dropItemChance, 0.0f, 1.0f)) {
        return;
    }
    const int count = std::max(1, enemy.dropItemCount);
    const float scatterRadius = std::max(0.0f, enemy.dropItemScatterRadius);
    std::uniform_real_distribution<float> angleDist(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> radiusDist(0.0f, scatterRadius);
    for (int i = 0; i < count; ++i) {
        EnemyEvent event;
        event.type = EnemyEventType::ObjectDrop;
        event.enemyId = enemy.enemyId;
        event.enemyName = enemy.enemyName;
        event.objectDropProfile = enemy.dropItemProfile;
        event.objectDropCount = 1;
        if (scatterRadius > 0.0f) {
            const float angle = angleDist(rng_);
            const float radius = radiusDist(rng_);
            event.position = enemy.position + fromAngle(angle) * radius;
        } else {
            event.position = enemy.position;
        }
        events_.push_back(std::move(event));
    }
}

void EnemySystem::queueEnemyHeldDrops(Enemy& enemy)
{
    if (enemy.heldDrops.empty()) {
        return;
    }

    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
    for (EnemyHeldDrop& held : enemy.heldDrops) {
        const bool guaranteed = held.origin == EnemyHeldDropOrigin::PickedUp;
        if (!guaranteed && chanceDist(rng_) > clamp(held.deathDropChance, 0.0f, 1.0f)) {
            continue;
        }

        if (held.kind == EnemyHeldDropKind::Money) {
            EnemyEvent event;
            event.type = EnemyEventType::MoneyDrop;
            event.position = enemy.position;
            event.enemyId = enemy.enemyId;
            event.enemyName = enemy.enemyName;
            event.moneyDrop = std::max(0, held.quantity);
            if (event.moneyDrop > 0) {
                events_.push_back(std::move(event));
            }
            continue;
        }

        if (held.kind == EnemyHeldDropKind::Object && !held.objectId.empty()) {
            EnemyEvent event;
            event.type = EnemyEventType::ObjectDrop;
            event.position = enemy.position;
            event.enemyId = enemy.enemyId;
            event.enemyName = enemy.enemyName;
            event.objectDropId = held.objectId;
            event.objectDropCount = 1;
            if (held.instance) {
                event.objectDropInstance = std::move(held.instance);
            }
            if (held.runtimeItem) {
                event.objectDropRuntimeItem = std::move(held.runtimeItem);
            }
            events_.push_back(std::move(event));
        }
    }
    enemy.heldDrops.clear();
}

void EnemySystem::ensureEnemyHeldDropsInitialized(Enemy& enemy, const ObjectCatalog& objectCatalog)
{
    if (enemy.heldDropsInitialized) {
        return;
    }
    enemy.heldDropsInitialized = true;

    const auto addInitialObjectDrops = [&](std::string_view profile, int count, float dropChance) {
        int added = 0;
        const int targetCount = std::clamp(count, 0, EnemyHeldDropCapacity);
        for (int i = 0; i < targetCount; ++i) {
            if (static_cast<int>(enemy.heldDrops.size()) >= EnemyHeldDropCapacity) {
                break;
            }
            const std::string objectId = chooseHeldObjectIdForProfile(
                objectCatalog,
                enemy.lootStageId,
                enemy.lootDepthRank,
                profile,
                rng_);
            if (objectId.empty()) {
                continue;
            }
            if (addHeldDropToEnemy(enemy, EnemyHeldDrop{
                    .kind = EnemyHeldDropKind::Object,
                    .origin = EnemyHeldDropOrigin::Initial,
                    .objectId = objectId,
                    .quantity = 1,
                    .deathDropChance = dropChance,
                })) {
                ++added;
            }
        }
        return added;
    };

    int addedFromDropItem = 0;
    if (enemy.dropItemEnabled && !enemy.dropItemConsumed && !enemy.dropItemProfile.empty()) {
        addedFromDropItem = addInitialObjectDrops(enemy.dropItemProfile, enemy.dropItemCount, enemy.dropItemChance);
        if (addedFromDropItem > 0) {
            enemy.dropItemConsumed = true;
        }
    }

    if (hasBehavior(enemy, "carry_loot")) {
        const double chance = std::clamp(behaviorParamDouble(enemy, "carry_loot", "chance", 1.0), 0.0, 1.0);
        std::uniform_real_distribution<double> chanceDist(0.0, 1.0);
        if (chanceDist(rng_) <= chance) {
            const std::string profile = behaviorParamString(enemy, "carry_loot", "profile", "common");
            const int count = std::max(1, behaviorParamInt(enemy, "carry_loot", "count", 1));
            const float dropChance = static_cast<float>(std::clamp(
                behaviorParamDouble(enemy, "carry_loot", "dropChance", 0.35),
                0.0,
                1.0));
            addInitialObjectDrops(profile, count, dropChance);

            const double moneyChance = std::clamp(behaviorParamDouble(enemy, "carry_loot", "moneyChance", 0.0), 0.0, 1.0);
            if (static_cast<int>(enemy.heldDrops.size()) < EnemyHeldDropCapacity && chanceDist(rng_) <= moneyChance) {
                const int moneyMin = std::max(1, behaviorParamInt(enemy, "carry_loot", "moneyMin", 1));
                const int moneyMax = std::max(moneyMin, behaviorParamInt(enemy, "carry_loot", "moneyMax", moneyMin));
                std::uniform_int_distribution<int> moneyDist(moneyMin, moneyMax);
                addHeldDropToEnemy(enemy, EnemyHeldDrop{
                    .kind = EnemyHeldDropKind::Money,
                    .origin = EnemyHeldDropOrigin::Initial,
                    .quantity = moneyDist(rng_),
                    .deathDropChance = dropChance,
                });
            }
        }
    }
}

bool EnemySystem::tryStealHeldDrop(
    Enemy& enemy,
    WorldDropSystem& worldDrops,
    const ObjectCatalog& objectCatalog,
    Vec2 targetPosition,
    float spawnedAtSeconds,
    float chance,
    std::string_view targetFilter)
{
    if (!enemy.active || enemy.heldDrops.empty()) {
        return false;
    }
    const float normalizedChance = chance > 1.0f ? chance / 100.0f : chance;
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
    if (chanceDist(rng_) > clamp(normalizedChance, 0.0f, 1.0f)) {
        return false;
    }

    std::vector<std::size_t> candidates;
    for (std::size_t i = 0; i < enemy.heldDrops.size(); ++i) {
        if (heldDropMatchesFilter(enemy.heldDrops[i], objectCatalog, targetFilter)) {
            candidates.push_back(i);
        }
    }
    if (candidates.empty()) {
        return false;
    }

    std::uniform_int_distribution<std::size_t> indexDist(0, candidates.size() - 1);
    const std::size_t heldIndex = candidates[indexDist(rng_)];
    EnemyHeldDrop& held = enemy.heldDrops[heldIndex];
    const WorldDropSpawnMotion motion = makeStealDropMotion(enemy.position);
    bool spawned = false;
    if (held.kind == EnemyHeldDropKind::Money) {
        spawned = worldDrops.spawnMoneyDrop(std::max(0, held.quantity), targetPosition, spawnedAtSeconds, motion);
    } else if (held.kind == EnemyHeldDropKind::Object && !held.objectId.empty()) {
        if (held.instance) {
            spawned = worldDrops.spawnObjectInstanceDrop(
                objectCatalog,
                *held.instance,
                targetPosition,
                spawnedAtSeconds,
                motion,
                false,
                held.runtimeItem ? &*held.runtimeItem : nullptr);
        } else {
            spawned = worldDrops.spawnObjectDrop(objectCatalog, held.objectId, targetPosition, spawnedAtSeconds, motion);
        }
    }
    if (!spawned) {
        return false;
    }

    enemy.heldDrops.erase(enemy.heldDrops.begin() + static_cast<std::ptrdiff_t>(heldIndex));
    return true;
}

void EnemySystem::queueEnemyMaterialDrops(Enemy& enemy)
{
    if (!enemy.dropMaterialEnabled || enemy.dropMaterialConsumed) {
        return;
    }
    enemy.dropMaterialConsumed = true;
    if (enemy.dropMaterialType == MaterialType::Count) {
        return;
    }
    std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
    if (chanceDist(rng_) > clamp(enemy.dropMaterialChance, 0.0f, 1.0f)) {
        return;
    }

    const int minCount = std::max(1, std::min(enemy.dropMaterialMin, enemy.dropMaterialMax));
    const int maxCount = std::max(minCount, std::max(enemy.dropMaterialMin, enemy.dropMaterialMax));
    std::uniform_int_distribution<int> countDist(minCount, maxCount);
    const int count = countDist(rng_);

    const float scatterRadius = std::max(0.0f, enemy.dropMaterialScatterRadius);
    EnemyEvent event;
    event.type = EnemyEventType::MaterialDrop;
    event.enemyId = enemy.enemyId;
    event.enemyName = enemy.enemyName;
    event.materialDropType = enemy.dropMaterialType;
    event.materialDropCount = count;
    if (scatterRadius > 0.0f) {
        std::uniform_real_distribution<float> angleDist(0.0f, Pi * 2.0f);
        std::uniform_real_distribution<float> radiusDist(0.0f, scatterRadius);
        const float angle = angleDist(rng_);
        const float radius = radiusDist(rng_);
        event.position = enemy.position + fromAngle(angle) * radius;
    } else {
        event.position = enemy.position;
    }
    events_.push_back(std::move(event));
}

void EnemySystem::setAwareness(Enemy& enemy, EnemyAwarenessState nextState, bool showIcon)
{
    if (enemy.awareness == nextState) {
        return;
    }
    enemy.awareness = nextState;
    enemy.loseSightTimer = 0.0f;
    if (!showIcon) {
        return;
    }
    enemy.awarenessIcon = nextState == EnemyAwarenessState::Detected
        ? EnemyAwarenessIcon::Exclamation
        : EnemyAwarenessIcon::Question;
    enemy.awarenessIconTimer = AwarenessIconDuration;
    if (nextState == EnemyAwarenessState::Detected) {
        events_.push_back(makeEnemyEvent(EnemyEventType::Alert, enemy));
    }
}

void EnemySystem::forceDetectInSight(Enemy& enemy, Vec2 playerPosition, bool showIcon)
{
    if (lengthSquared(playerPosition - enemy.position) > 0.0001f) {
        const Vec2 toPlayer = normalize(playerPosition - enemy.position);
        enemy.facingAngle = std::atan2(toPlayer.y, toPlayer.x);
    }
    setAwareness(enemy, EnemyAwarenessState::Detected, showIcon);
}

void EnemySystem::wakeDungeonEventEnemy(Enemy& enemy, Vec2 playerPosition, bool showIcon)
{
    enemy.dungeonEventActivationLocked = false;
    enemy.dungeonEventSleeping = false;
    enemy.status.removeState("status_sleep");
    enemy.manualDetectionOnly = false;
    forceDetectInSight(enemy, playerPosition, showIcon);
}

void EnemySystem::applyDefinition(Enemy& enemy, const EnemyDefinition* definition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog)
{
    enemy.definition = definition;
    enemy.dungeonEventId.clear();
    enemy.dungeonEventBoss = false;
    enemy.dungeonEventSleeping = false;
    enemy.dungeonEventActivationLocked = false;
    enemy.enemyId = std::string(DefaultEnemyId);
    enemy.enemyName = std::string(DefaultEnemyName);
    enemy.behaviorId.clear();
    enemy.behaviorIds.clear();
    enemy.projectileId.clear();
    enemy.rangedBehaviorId.clear();
    enemy.projectileInterval = 0.0f;
    enemy.projectileSpeedMultiplier = 1.0f;
    enemy.projectileDamageOverride = -1;
    enemy.projectileRadiusScale = 1.0f;
    enemy.projectileBurstCount = 1;
    enemy.projectileBurstRemaining = 0;
    enemy.projectileBurstInterval = 0.12f;
    enemy.fireVolleyCount = 1;
    enemy.fireSpreadDegrees = 8.0f;
    enemy.projectileEffects.clear();
    enemy.aiId.clear();
    enemy.unawareAiId = "idle";
    enemy.behaviorTimer = 0.0f;
    enemy.projectileTimer = 1.2f;
    enemy.action = {};
    enemy.enemyTags.clear();
    enemy.radius = balance.enemyRadius;
    enemy.maxHp = balance.enemyHp + std::max(0, ambientActiveCount() / 12);
    enemy.hp = enemy.maxHp;
    enemy.xp = balance.enemyXp;
    enemy.moneyDrop = 0;
    enemy.heldDrops.clear();
    enemy.heldDropsInitialized = false;
    enemy.lootStageId.clear();
    enemy.lootDepthRank = 1;
    enemy.contactAttackPower = 1;
    enemy.contactDamageType = "blunt";
    enemy.facingAngle = 0.0f;
    enemy.contactDamageMultiplier = 1.0f;
    enemy.chestBiteIntervalSeconds = 0.0f;
    enemy.chestBiteTimer = 0.0f;
    enemy.chestBiteTriggerRange = 0.0f;
    enemy.chestBiteJumpDistance = 0.0f;
    enemy.chestBiteJumpDurationSeconds = 0.0f;
    enemy.chestBiteJumpArcHeight = 0.0f;
    enemy.frontGuardArcDegrees = 140.0f;
    enemy.frontGuardDamageMultiplier = 0.35f;
    enemy.physicalDamageMultiplier = 0.55f;
    enemy.magicBodyPhysicalMultiplier = 0.35f;
    enemy.magicBodyMagicMultiplier = 1.0f;
    enemy.ringSlowMultiplier = -1.0f;
    enemy.ringSlowDurationSeconds = -1.0f;
    enemy.digMovePower = 1;
    enemy.digMoveIntervalSeconds = DigActionInterval;
    enemy.enemyHealRadius = 0.0f;
    enemy.enemyHealAmount = 0.0f;
    enemy.enemyHealIntervalSeconds = 0.0f;
    enemy.enemyHealTimer = 0.0f;
    enemy.countdownExplodeRadius = 0.0f;
    enemy.countdownExplodeDelay = 0.0f;
    enemy.countdownExplodeInitialDelay = 0.0f;
    enemy.countdownExplodeArmDistance = 0.0f;
    enemy.countdownExplodeDamage = 0;
    enemy.countdownExplodeTerrainDamage = 0;
    enemy.countdownExplodeWarningTickIndex = -1;
    enemy.countdownExplodeArmed = false;
    enemy.countdownExplodeOnce = false;
    enemy.countdownExploded = false;
    enemy.jumpAttackDistance = 0.0f;
    enemy.jumpLandingRadius = 0.0f;
    enemy.jumpLandingDamageMultiplier = 1.0f;
    enemy.jumpAttackIntervalSeconds = 0.0f;
    enemy.jumpAttackTimer = 0.0f;
    enemy.jumpAttackDurationSeconds = JumpAttackDefaultDuration;
    enemy.jumpAttackArcHeight = JumpAttackDefaultArcHeight;
    enemy.jumpLandingBuffTimer = 0.0f;
    enemy.jumpActive = false;
    enemy.jumpStartPosition = {};
    enemy.jumpTargetPosition = {};
    enemy.jumpElapsedSeconds = 0.0f;
    enemy.jumpDurationSeconds = 0.0f;
    enemy.jumpArcHeight = 0.0f;
    clearExternalBounceState(enemy);
    enemy.altitude = 0.0f;
    enemy.hoverAltitude = 0.0f;
    enemy.hoverBobAmplitude = 0.0f;
    enemy.hoverBobSpeed = 0.0f;
    enemy.lightSpeedMultiplier = 1.0f;
    enemy.magnetRadius = 0.0f;
    enemy.magnetStrength = 0.0f;
    enemy.magnetTargetTag.clear();
    enemy.rustDefenseMultiplier = 1.0f;
    enemy.rustDurationSeconds = 0.0f;
    enemy.rustTargetTag.clear();
    enemy.chestBiteKnockback = 0.0f;
    enemy.swarmSpawnEnabled = false;
    enemy.swarmSpawnExecuted = false;
    enemy.swarmSpawnCount = 0;
    enemy.swarmSpawnRadius = 0.0f;
    enemy.dropItemEnabled = false;
    enemy.dropItemProfile.clear();
    enemy.dropItemChance = 0.0f;
    enemy.dropItemCount = 0;
    enemy.dropItemScatterRadius = 0.0f;
    enemy.dropItemConsumed = false;
    enemy.dropMaterialEnabled = false;
    enemy.dropMaterialType = MaterialType::Count;
    enemy.dropMaterialChance = 0.0f;
    enemy.dropMaterialMin = 0;
    enemy.dropMaterialMax = 0;
    enemy.dropMaterialScatterRadius = 0.0f;
    enemy.dropMaterialConsumed = false;
    enemy.stealItemEnabled = false;
    enemy.stealTarget.clear();
    enemy.stealRadius = 0.0f;
    enemy.stealSeekRadius = 0.0f;
    enemy.stealMaxCarry = 0;
    enemy.fleeNavigation = {};
    enemy.bossAction = {};
    enemy.death = {};
    enemy.awareness = EnemyAwarenessState::Unaware;
    enemy.manualDetectionOnly = false;
    enemy.loseSightTimer = 0.0f;
    enemy.visionDistance = DefaultVisionDistance;
    enemy.visionAngle = DefaultVisionAngle;
    enemy.loseSightSeconds = DefaultLoseSightSeconds;
    enemy.awarenessIcon = EnemyAwarenessIcon::None;
    enemy.awarenessIconTimer = 0.0f;
    enemy.aiMoveDirection = {1.0f, 0.0f};
    enemy.patrolAnchor = {};
    enemy.patrolAnchorInitialized = false;
    enemy.aiDecisionTimer = 0.0f;
    enemy.aiDigTimer = 0.0f;

    if (definition == nullptr) {
        return;
    }

    enemy.enemyId = definition->id.empty() ? std::string(DefaultEnemyId) : definition->id;
    enemy.enemyName = definition->name.empty() ? enemy.enemyId : definition->name;
    enemy.aiId = definition->enemyAi.empty() ? "chase" : definition->enemyAi;
    if (!isKnownAi(enemy.aiId)) {
        if (loggedUnknownAi_.insert(enemy.aiId).second) {
            logError("Enemy DB unknown ai \"" + enemy.aiId + "\"; using chase");
        }
        enemy.aiId = "chase";
    }
    enemy.unawareAiId = definition->unawareAiId.empty() ? "idle" : definition->unawareAiId;
    if (!isKnownAi(enemy.unawareAiId)) {
        if (loggedUnknownUnawareAi_.insert(enemy.unawareAiId).second) {
            logError("Enemy DB unknown unaware ai \"" + enemy.unawareAiId + "\"; using idle");
        }
        enemy.unawareAiId = "idle";
    }
    configureEnemyAltitudeFromAi(enemy);
    if (!definition->enemyBehaviorIds.empty()) {
        enemy.behaviorId = definition->enemyBehaviorIds.front();
    }
    enemy.behaviorIds = definition->enemyBehaviorIds;
    for (const std::string& behaviorId : enemy.behaviorIds) {
        if (!isSupportedBehavior(behaviorId) && loggedUnsupportedBehavior_.insert(behaviorId).second) {
            logError("Enemy DB unsupported behavior \"" + behaviorId + "\"; ignored");
        }
    }
    for (const std::string& behaviorId : enemy.behaviorIds) {
        const auto behaviorIt = enemyCatalog.behaviorsById.find(behaviorId);
        if (isRangedBehavior(behaviorId)) {
            enemy.rangedBehaviorId = behaviorId;
            enemy.projectileId = std::string(fallbackProjectileForBehavior(behaviorId));
            enemy.projectileInterval = 2.4f;
            if (behaviorIt != enemyCatalog.behaviorsById.end()) {
                if (!behaviorIt->second.defaultProjectileId.empty() &&
                    behaviorIt->second.defaultProjectileId != "none") {
                    enemy.projectileId = behaviorIt->second.defaultProjectileId;
                }
                if (behaviorIt->second.defaultIntervalSeconds > 0.0) {
                    enemy.projectileInterval = static_cast<float>(behaviorIt->second.defaultIntervalSeconds);
                }
                enemy.projectileEffects = behaviorIt->second.enemyDefaultEffects;
            }
            if (const EnemyBehaviorSpec* spec = findEnemyBehaviorSpec(enemy, behaviorId)) {
                if (spec->intervalSeconds > 0.0) {
                    enemy.projectileInterval = static_cast<float>(spec->intervalSeconds);
                }
            }
            break;
        }
    }
    if (hasBehavior(enemy, "contact_basic")) {
        enemy.contactDamageMultiplier = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "contact_basic", "multiplier", 1.0)));
    }
    if (hasBehavior(enemy, "jump_attack")) {
        enemy.jumpAttackDistance = static_cast<float>(std::max(4.0, behaviorParamDouble(enemy, "jump_attack", "jumpDistance", 42.0)));
        enemy.jumpLandingRadius = static_cast<float>(std::max(8.0, behaviorParamDouble(enemy, "jump_attack", "landingRadius", 28.0)));
        enemy.jumpLandingDamageMultiplier = static_cast<float>(std::max(1.0, behaviorParamDouble(enemy, "jump_attack", "landingDamageMultiplier", 1.45)));
        enemy.jumpAttackDurationSeconds = static_cast<float>(std::clamp(
            behaviorParamDouble(enemy, "jump_attack", "jumpDuration", JumpAttackDefaultDuration),
            static_cast<double>(JumpAttackDurationMin),
            static_cast<double>(JumpAttackDurationMax)));
        enemy.jumpAttackArcHeight = static_cast<float>(std::max(
            0.0,
            behaviorParamDouble(enemy, "jump_attack", "jumpHeight", JumpAttackDefaultArcHeight)));
        const EnemyBehaviorSpec* spec = findEnemyBehaviorSpec(enemy, "jump_attack");
        enemy.jumpAttackIntervalSeconds = static_cast<float>(std::max(0.2, spec != nullptr ? spec->intervalSeconds : 1.8));
        enemy.jumpAttackTimer = enemy.jumpAttackIntervalSeconds * 0.65f;
    }
    if (hasBehavior(enemy, "spike_contact")) {
        const double spikeMultiplier = behaviorParamDouble(enemy, "spike_contact", "multiplier", 1.35);
        enemy.contactDamageMultiplier *= static_cast<float>(std::max(0.0, spikeMultiplier));
    }
    if (hasBehavior(enemy, "chest_bite")) {
        const double biteMultiplier = behaviorParamDouble(enemy, "chest_bite", "damageMultiplier", 1.4);
        enemy.contactDamageMultiplier *= static_cast<float>(std::max(0.0, biteMultiplier));
        enemy.chestBiteKnockback = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "chest_bite", "knockback", 52.0)));
        const EnemyBehaviorSpec* spec = findEnemyBehaviorSpec(enemy, "chest_bite");
        enemy.chestBiteIntervalSeconds = static_cast<float>(std::max(
            0.2,
            spec != nullptr && spec->intervalSeconds > 0.0 ? spec->intervalSeconds : static_cast<double>(ChestBiteDefaultIntervalSeconds)));
        enemy.chestBiteTimer = enemy.chestBiteIntervalSeconds * 0.45f;
        enemy.chestBiteTriggerRange = static_cast<float>(std::max(
            18.0,
            behaviorParamDouble(enemy, "chest_bite", "range", ChestBiteDefaultTriggerRange)));
        enemy.chestBiteJumpDistance = static_cast<float>(std::max(
            static_cast<double>(JumpTargetMinDistance),
            behaviorParamDouble(enemy, "chest_bite", "jumpDistance", ChestBiteDefaultJumpDistance)));
        enemy.chestBiteJumpDurationSeconds = static_cast<float>(std::clamp(
            behaviorParamDouble(enemy, "chest_bite", "jumpDuration", ChestBiteDefaultJumpDurationSeconds),
            static_cast<double>(JumpAttackDurationMin),
            static_cast<double>(JumpAttackDurationMax)));
        enemy.chestBiteJumpArcHeight = static_cast<float>(std::max(
            0.0,
            behaviorParamDouble(enemy, "chest_bite", "jumpHeight", ChestBiteDefaultJumpArcHeight)));
    }
    if (hasBehavior(enemy, "front_guard")) {
        enemy.frontGuardArcDegrees = static_cast<float>(clamp(behaviorParamDouble(enemy, "front_guard", "arc", 140.0), 10.0, 360.0));
        enemy.frontGuardDamageMultiplier = static_cast<float>(clamp(behaviorParamDouble(enemy, "front_guard", "damageMultiplier", 0.35), 0.0, 1.0));
    }
    if (hasBehavior(enemy, "physical_resist")) {
        enemy.physicalDamageMultiplier = static_cast<float>(clamp(behaviorParamDouble(enemy, "physical_resist", "physicalMultiplier", 0.55), 0.0, 1.0));
    }
    if (hasBehavior(enemy, "magic_body")) {
        enemy.magicBodyPhysicalMultiplier = static_cast<float>(clamp(behaviorParamDouble(enemy, "magic_body", "physicalMultiplier", 0.35), 0.0, 1.0));
        enemy.magicBodyMagicMultiplier = static_cast<float>(clamp(behaviorParamDouble(enemy, "magic_body", "magicMultiplier", 1.0), 0.0, 2.0));
    }
    if (hasBehavior(enemy, "ring_slow_bite")) {
        constexpr double NoParam = -99999.0;
        const double configuredMultiplier = behaviorParamDouble(enemy, "ring_slow_bite", "multiplier", NoParam);
        if (configuredMultiplier != NoParam) {
            enemy.ringSlowMultiplier = static_cast<float>(clamp(configuredMultiplier, 0.05, 1.0));
        }
        const double configuredDuration = behaviorParamDouble(enemy, "ring_slow_bite", "duration", NoParam);
        if (configuredDuration != NoParam) {
            enemy.ringSlowDurationSeconds = static_cast<float>(std::max(0.0, configuredDuration));
        }
    }
    if (hasBehavior(enemy, "dig_move")) {
        enemy.digMovePower = std::max(1, behaviorParamInt(enemy, "dig_move", "digPower", 1));
        enemy.digMoveIntervalSeconds = static_cast<float>(std::max(0.02, behaviorParamDouble(enemy, "dig_move", "interval", DigActionInterval)));
    }
    if (hasBehavior(enemy, "light_slow")) {
        enemy.lightSpeedMultiplier = static_cast<float>(clamp(behaviorParamDouble(enemy, "light_slow", "lightSpeedMultiplier", 0.72), 0.05, 1.0));
    }
    if (hasBehavior(enemy, "magnet_disturb")) {
        enemy.magnetRadius = static_cast<float>(clamp(behaviorParamDouble(enemy, "magnet_disturb", "radius", 120.0), 8.0, static_cast<double>(MagnetDisturbMaxRadius)));
        enemy.magnetStrength = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "magnet_disturb", "strength", 0.6)));
        enemy.magnetTargetTag = behaviorParamString(enemy, "magnet_disturb", "targetTag", "metal");
    }
    if (hasBehavior(enemy, "rust_touch")) {
        enemy.rustDefenseMultiplier = static_cast<float>(clamp(behaviorParamDouble(enemy, "rust_touch", "defenseMultiplier", 0.8), 0.05, 1.0));
        enemy.rustDurationSeconds = static_cast<float>(std::max(0.1, behaviorParamDouble(enemy, "rust_touch", "duration", 4.0)));
        enemy.rustTargetTag = behaviorParamString(enemy, "rust_touch", "targetTag", "metal");
    }
    if (hasBehavior(enemy, "steal_item")) {
        enemy.stealItemEnabled = true;
        enemy.stealTarget = behaviorParamString(enemy, "steal_item", "target", "money|treasure|drop");
        enemy.stealRadius = static_cast<float>(std::max(
            12.0,
            behaviorParamDouble(enemy, "steal_item", "radius", StealItemPickupRadius)));
        enemy.stealSeekRadius = static_cast<float>(std::max(
            static_cast<double>(enemy.stealRadius),
            behaviorParamDouble(enemy, "steal_item", "seekRadius", ItemSeekRadius)));
        enemy.stealMaxCarry = std::clamp(behaviorParamInt(enemy, "steal_item", "maxCarry", EnemyHeldDropCapacity), 1, EnemyHeldDropCapacity);
    }
    if (hasBehavior(enemy, "drop_item")) {
        enemy.dropItemProfile = behaviorParamString(enemy, "drop_item", "profile", "");
        enemy.dropItemChance = static_cast<float>(clamp(behaviorParamDouble(enemy, "drop_item", "chance", 1.0), 0.0, 1.0));
        enemy.dropItemCount = std::max(1, behaviorParamInt(enemy, "drop_item", "count", 1));
        enemy.dropItemScatterRadius = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "drop_item", "scatterRadius", 28.0)));
        enemy.dropItemEnabled = !enemy.dropItemProfile.empty();
        enemy.dropItemConsumed = false;
        if (!enemy.dropItemEnabled) {
            logError("[warning] EnemySystem: drop_item profile is empty for enemy id=\"" + enemy.enemyId + "\"; behavior disabled");
        }
    }
    if (hasBehavior(enemy, "drop_material")) {
        const std::string materialId = behaviorParamString(enemy, "drop_material", "material", "enhancement_ore");
        MaterialType materialType = MaterialType::Count;
        enemy.dropMaterialEnabled = materialTypeFromSaveName(materialId, materialType);
        enemy.dropMaterialType = materialType;
        enemy.dropMaterialChance = static_cast<float>(std::clamp(behaviorParamDouble(enemy, "drop_material", "chance", 1.0), 0.0, 1.0));
        constexpr int MissingCount = -99999;
        const int count = behaviorParamInt(enemy, "drop_material", "count", MissingCount);
        enemy.dropMaterialMin = std::max(1, behaviorParamInt(enemy, "drop_material", "min", count != MissingCount ? count : 1));
        enemy.dropMaterialMax = std::max(enemy.dropMaterialMin, behaviorParamInt(enemy, "drop_material", "max", count != MissingCount ? count : enemy.dropMaterialMin));
        enemy.dropMaterialScatterRadius = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "drop_material", "scatterRadius", 28.0)));
        enemy.dropMaterialConsumed = false;
        if (!enemy.dropMaterialEnabled) {
            logError("[warning] EnemySystem: unknown drop_material material=\"" + materialId + "\" for enemy id=\"" + enemy.enemyId + "\"; behavior disabled");
        }
    }
    if (hasBehavior(enemy, "swarm_spawn")) {
        enemy.swarmSpawnEnabled = true;
        enemy.swarmSpawnExecuted = false;
        constexpr int MissingCount = -99999;
        const int explicitCount = behaviorParamInt(enemy, "swarm_spawn", "count", MissingCount);
        if (explicitCount != MissingCount) {
            enemy.swarmSpawnCount = std::clamp(explicitCount, 1, SwarmSpawnCountMax);
        } else {
            const int configuredMin = behaviorParamInt(
                enemy,
                "swarm_spawn",
                "groupMin",
                behaviorParamInt(enemy, "swarm_spawn", "min", 2));
            const int configuredMax = behaviorParamInt(
                enemy,
                "swarm_spawn",
                "groupMax",
                behaviorParamInt(enemy, "swarm_spawn", "max", 4));
            const int groupMin = std::clamp(std::min(configuredMin, configuredMax), 1, SwarmSpawnCountMax);
            const int groupMax = std::clamp(std::max(configuredMin, configuredMax), groupMin, SwarmSpawnCountMax);
            enemy.swarmSpawnCount = groupMin == groupMax
                ? groupMin
                : std::uniform_int_distribution<int>(groupMin, groupMax)(rng_);
        }
        enemy.swarmSpawnRadius = std::max(
            enemy.radius + 6.0f,
            static_cast<float>(behaviorParamDouble(enemy, "swarm_spawn", "radius", 36.0)));
    }
    if (hasBehavior(enemy, "enemy_heal")) {
        enemy.enemyHealRadius = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "enemy_heal", "radius", 120.0)));
        enemy.enemyHealAmount = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "enemy_heal", "amount", 1.0)));
        enemy.enemyHealIntervalSeconds = static_cast<float>(std::max(0.1, behaviorParamDouble(enemy, "enemy_heal", "interval", 3.0)));
        enemy.enemyHealTimer = enemy.enemyHealIntervalSeconds;
    }
    if (hasBehavior(enemy, "countdown_explode")) {
        enemy.countdownExplodeRadius = static_cast<float>(std::max(8.0, behaviorParamDouble(enemy, "countdown_explode", "radius", 44.0)));
        double countdownDelay = behaviorParamDouble(enemy, "countdown_explode", "delay", CountdownExplodeDefaultDelaySeconds);
        if (isBombTsuchinokoFamily(enemy)) {
            countdownDelay = std::max(countdownDelay, static_cast<double>(CountdownExplodeDefaultDelaySeconds));
        }
        enemy.countdownExplodeDelay = static_cast<float>(std::max(0.0, countdownDelay));
        enemy.countdownExplodeInitialDelay = enemy.countdownExplodeDelay;
        const double defaultArmDistance = enemy.visionDistance > 0.0f
            ? static_cast<double>(enemy.visionDistance)
            : static_cast<double>(CountdownExplodeFallbackArmDistance);
        enemy.countdownExplodeArmDistance = static_cast<float>(std::max(
            0.0,
            behaviorParamDouble(enemy, "countdown_explode", "armDistance", defaultArmDistance)));
        enemy.countdownExplodeDamage = std::max(0, behaviorParamInt(enemy, "countdown_explode", "damage", 3));
        enemy.countdownExplodeTerrainDamage = std::max(0, behaviorParamInt(enemy, "countdown_explode", "terrainDamage", 1));
        enemy.countdownExplodeWarningTickIndex = -1;
        enemy.countdownExplodeArmed = false;
        enemy.countdownExplodeOnce = behaviorParamInt(enemy, "countdown_explode", "once", 1) != 0;
        enemy.countdownExploded = false;
    }
    if (hasBehavior(enemy, "boss_sequence")) {
        enemy.bossAction.enabled = true;
        enemy.bossAction.pattern = behaviorParamString(enemy, "boss_sequence", "pattern", defaultBossActionPatternFor(enemy));
    }
    if (!enemy.rangedBehaviorId.empty()) {
        constexpr int NoDamageOverride = -99999;
        const std::string rangedId = (enemy.rangedBehaviorId == "throw_stone" || enemy.rangedBehaviorId == "throw_object")
            ? std::string("throw_object")
            : enemy.rangedBehaviorId;
        enemy.projectileSpeedMultiplier = static_cast<float>(std::max(0.05, behaviorParamDouble(enemy, rangedId, "projectileSpeed", 1.0)));
        if (enemy.rangedBehaviorId == "throw_stone" || enemy.rangedBehaviorId == "throw_object") {
            const std::string projectileId = behaviorParamString(enemy, "throw_object", "projectile", enemy.projectileId);
            if (!projectileId.empty()) {
                enemy.projectileId = projectileId;
            }
        }
        if (enemy.rangedBehaviorId == "shoot_poison") {
            const int overrideDamage = behaviorParamInt(enemy, "shoot_poison", "damage", NoDamageOverride);
            if (overrideDamage != NoDamageOverride) {
                enemy.projectileDamageOverride = std::max(0, overrideDamage);
            }
            const double poisonDuration = std::max(0.1, behaviorParamDouble(enemy, "shoot_poison", "duration", 2.0));
            const double poisonDps = std::max(0.1, behaviorParamDouble(enemy, "shoot_poison", "damagePerSecond", 1.0));
            if (enemy.projectileEffects.empty()) {
                EffectSpec poison;
                poison.target = "player";
                poison.effects = {"status_poison"};
                poison.values = {poisonDps};
                poison.duration = poisonDuration;
                enemy.projectileEffects.push_back(std::move(poison));
            }
        } else if (enemy.rangedBehaviorId == "shoot_web") {
            const int overrideDamage = behaviorParamInt(enemy, "shoot_web", "damage", NoDamageOverride);
            if (overrideDamage != NoDamageOverride) {
                enemy.projectileDamageOverride = std::max(0, overrideDamage);
            }
            const double slowDuration = std::max(0.1, behaviorParamDouble(enemy, "shoot_web", "duration", 2.0));
            const double slowMultiplier = clamp(behaviorParamDouble(enemy, "shoot_web", "speedMultiplier", 0.70), 0.0, 1.0);
            if (enemy.projectileEffects.empty()) {
                EffectSpec slow;
                slow.target = "player";
                slow.effects = {"status_slow"};
                slow.values = {slowMultiplier};
                slow.duration = slowDuration;
                enemy.projectileEffects.push_back(std::move(slow));
            }
        } else if (enemy.rangedBehaviorId == "shoot_fire") {
            enemy.projectileRadiusScale = static_cast<float>(std::max(0.2, behaviorParamDouble(enemy, "shoot_fire", "scale", 1.0)));
            enemy.fireVolleyCount = std::clamp(behaviorParamInt(enemy, "shoot_fire", "count", 1), 1, 5);
            enemy.fireSpreadDegrees = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "shoot_fire", "spread", 12.0)));
            const int overrideDamage = behaviorParamInt(enemy, "shoot_fire", "damage", NoDamageOverride);
            if (overrideDamage != NoDamageOverride) {
                enemy.projectileDamageOverride = std::max(0, overrideDamage);
            }
        } else if (enemy.rangedBehaviorId == "shoot_paralyze") {
            const double paralyzeDuration = std::max(0.1, behaviorParamDouble(enemy, "shoot_paralyze", "duration", 1.5));
            if (enemy.projectileEffects.empty()) {
                EffectSpec paralyze;
                paralyze.target = "player";
                paralyze.effects = {"status_paralyze"};
                paralyze.values = {1.0};
                paralyze.duration = paralyzeDuration;
                enemy.projectileEffects.push_back(std::move(paralyze));
            }
        } else if (enemy.rangedBehaviorId == "shoot_mud") {
            const double mudRadius = std::max(8.0, behaviorParamDouble(enemy, "shoot_mud", "radius", 38.0));
            const double mudDuration = clamp(behaviorParamDouble(enemy, "shoot_mud", "duration", 3.8), 0.2, static_cast<double>(MudZoneMaxDurationSeconds));
            const double mudSlow = clamp(behaviorParamDouble(enemy, "shoot_mud", "speedMultiplier", 0.65), 0.05, 1.0);
            const double mudDps = std::max(0.0, behaviorParamDouble(enemy, "shoot_mud", "damagePerSecond", 1.0));
            const std::string mudDamageType = behaviorParamString(enemy, "shoot_mud", "damageType", "poison");
            if (enemy.projectileEffects.empty()) {
                EffectSpec mudZone;
                mudZone.target = "area";
                mudZone.effects = {"mud_zone"};
                mudZone.values = {mudRadius, mudSlow, mudDps};
                mudZone.duration = mudDuration;
                enemy.projectileEffects.push_back(std::move(mudZone));
            }
            if (enemy.projectileEffects.size() == 1) {
                EffectSpec mudKind;
                mudKind.target = "area";
                mudKind.effects = {"mud_damage_type_" + mudDamageType};
                mudKind.values = {0.0};
                mudKind.duration = mudDuration;
                enemy.projectileEffects.push_back(std::move(mudKind));
            }
        } else if (enemy.rangedBehaviorId == "shoot_water") {
            enemy.projectileBurstCount = std::clamp(behaviorParamInt(enemy, "shoot_water", "burstCount", 1), 1, 6);
            enemy.projectileBurstRemaining = enemy.projectileBurstCount;
            enemy.projectileBurstInterval = static_cast<float>(std::max(0.02, behaviorParamDouble(enemy, "shoot_water", "burstInterval", 0.14)));
            const int overrideDamage = behaviorParamInt(enemy, "shoot_water", "damage", NoDamageOverride);
            if (overrideDamage != NoDamageOverride) {
                enemy.projectileDamageOverride = std::max(0, overrideDamage);
            }
        } else if (enemy.rangedBehaviorId == "shoot_bubble") {
            enemy.projectileId = "water_bubble";
            const int bubbleCount = std::clamp(behaviorParamInt(enemy, "shoot_bubble", "count", 6), 3, 12);
            enemy.fireVolleyCount = bubbleCount;
            enemy.projectileBurstCount = bubbleCount;
            enemy.projectileBurstRemaining = 0;
            enemy.projectileBurstInterval = static_cast<float>(std::max(
                0.02,
                behaviorParamDouble(
                    enemy,
                    "shoot_bubble",
                    "shotInterval",
                    behaviorParamDouble(enemy, "shoot_bubble", "burstInterval", 0.11))));
            enemy.fireSpreadDegrees = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "shoot_bubble", "spread", 14.0)));
            enemy.projectileRadiusScale = static_cast<float>(std::max(0.2, behaviorParamDouble(enemy, "shoot_bubble", "scale", 1.0)));
            const int overrideDamage = behaviorParamInt(enemy, "shoot_bubble", "damage", NoDamageOverride);
            if (overrideDamage != NoDamageOverride) {
                enemy.projectileDamageOverride = std::max(0, overrideDamage);
            }
        } else if (enemy.rangedBehaviorId == "shoot_water_bubble") {
            enemy.projectileId = "water_shot";
            const int bubbleCount = std::clamp(behaviorParamInt(enemy, "shoot_water_bubble", "bubbleCount", 4), 1, 10);
            enemy.fireVolleyCount = bubbleCount;
            enemy.projectileBurstCount = bubbleCount;
            enemy.projectileBurstRemaining = 0;
            enemy.projectileBurstInterval = static_cast<float>(std::max(
                0.02,
                behaviorParamDouble(
                    enemy,
                    "shoot_water_bubble",
                    "bubbleInterval",
                    behaviorParamDouble(enemy, "shoot_water_bubble", "burstInterval", 0.10))));
            enemy.fireSpreadDegrees = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "shoot_water_bubble", "bubbleSpread", 12.0)));
            enemy.projectileRadiusScale = static_cast<float>(std::max(0.2, behaviorParamDouble(enemy, "shoot_water_bubble", "scale", 1.0)));
            const int overrideDamage = behaviorParamInt(enemy, "shoot_water_bubble", "damage", NoDamageOverride);
            if (overrideDamage != NoDamageOverride) {
                enemy.projectileDamageOverride = std::max(0, overrideDamage);
            }
        } else if (enemy.rangedBehaviorId == "radial_spike") {
            enemy.fireVolleyCount = std::clamp(behaviorParamInt(enemy, "radial_spike", "count", 8), 1, 24);
            const int overrideDamage = behaviorParamInt(enemy, "radial_spike", "damage", NoDamageOverride);
            if (overrideDamage != NoDamageOverride) {
                enemy.projectileDamageOverride = std::max(0, overrideDamage);
            }
        }
    }
    enemy.enemyTags = definition->enemyTags;

    if (definition->radius > 0.0 && std::isfinite(definition->radius)) {
        enemy.radius = static_cast<float>(definition->radius);
    }
    if (definition->hp > 0) {
        enemy.maxHp = definition->hp;
        enemy.hp = enemy.maxHp;
    }
    if (definition->xp >= 0) {
        enemy.xp = definition->xp;
    }
    if (definition->money > 0) {
        enemy.moneyDrop = definition->money;
    }
    if (definition->contactAttackPower >= 0) {
        enemy.contactAttackPower = definition->contactAttackPower;
    }
    const std::string normalizedContactDamageType = normalizeDamageType(definition->contactDamageType);
    if (!normalizedContactDamageType.empty()) {
        if (definition->contactDamageType == "physical") {
            logError("[warning] EnemySystem: enemy_id=\"" + enemy.enemyId + "\" contactDamageType physical is deprecated; using blunt");
        }
        enemy.contactDamageType = normalizedContactDamageType;
    }
    if (definition->visionDistance > 0.0 && std::isfinite(definition->visionDistance)) {
        enemy.visionDistance = static_cast<float>(definition->visionDistance);
    }
    if (definition->visionAngle > 0.0 && std::isfinite(definition->visionAngle)) {
        enemy.visionAngle = static_cast<float>(definition->visionAngle);
    }
    if (definition->loseSightSeconds >= 0.0 && std::isfinite(definition->loseSightSeconds)) {
        enemy.loseSightSeconds = static_cast<float>(definition->loseSightSeconds);
    }
}

void applyEnemyVariant(
    Enemy& enemy,
    const EnemyCatalog& enemyCatalog,
    EnemyVariantTier variantTier,
    int effectiveBaseLevel)
{
    enemy.variantTier = variantTier;
    const int fallbackLevel = enemy.definition != nullptr
        ? std::max(1, enemy.definition->baseLevel + enemyVariantLevelBonus(variantTier))
        : std::max(1, effectiveBaseLevel);
    enemy.effectiveBaseLevel = std::max(1, effectiveBaseLevel > 0 ? effectiveBaseLevel : fallbackLevel);

    if (enemy.definition == nullptr || variantTier == EnemyVariantTier::Normal) {
        return;
    }

    const EnemyDefinition& definition = *enemy.definition;
    enemy.enemyName = enemyVariantDisplayName(baseEnemyName(definition), variantTier);
    enemy.maxHp = scaledEnemyStatForEffectiveLevel(
        enemyCatalog,
        definition,
        EnemyStatKind::Hp,
        std::max(1, definition.hp),
        enemy.effectiveBaseLevel);
    enemy.hp = enemy.maxHp;
    enemy.contactAttackPower = scaledEnemyStatForEffectiveLevel(
        enemyCatalog,
        definition,
        EnemyStatKind::ContactAttack,
        std::max(0, definition.contactAttackPower),
        enemy.effectiveBaseLevel);
    enemy.xp = scaledEnemyStatForEffectiveLevel(
        enemyCatalog,
        definition,
        EnemyStatKind::Xp,
        std::max(0, definition.xp),
        enemy.effectiveBaseLevel);
    enemy.moneyDrop = scaledEnemyStatForEffectiveLevel(
        enemyCatalog,
        definition,
        EnemyStatKind::Money,
        std::max(0, definition.money),
        enemy.effectiveBaseLevel);
}

bool EnemySystem::spawnDefinitionAt(
    Vec2 position,
    const EnemyDefinition* definition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    bool detectedOnSpawn,
    Vec2 detectedTarget,
    float spawnWarmupOverride,
    int* outRuntimeId,
    std::string_view lootStageId,
    int lootDepthRank,
    EnemyVariantTier variantTier,
    int effectiveBaseLevel,
    EnemySpawnSource spawnSource,
    bool screenSleepAllowed)
{
    Enemy* enemy = enemies_.acquire();
    if (!enemy) {
        return false;
    }
    *enemy = Enemy{};
    enemy->active = true;
    enemy->id = nextEnemyId_++;
    enemy->isBoss = false;
    enemy->spawnSource = spawnSource;
    enemy->screenSleepAllowed = spawnSource == EnemySpawnSource::Ambient && screenSleepAllowed;
    enemy->position = position;
    applyDefinition(*enemy, definition, balance, enemyCatalog);
    enemy->spawnSource = spawnSource;
    enemy->screenSleepAllowed = spawnSource == EnemySpawnSource::Ambient && screenSleepAllowed;
    applyEnemyVariant(*enemy, enemyCatalog, variantTier, effectiveBaseLevel);
    enemy->lootStageId = std::string(lootStageId);
    enemy->lootDepthRank = std::max(1, lootDepthRank);
    const float spawnWarmup = spawnWarmupOverride >= 0.0f
        ? spawnWarmupOverride
        : balance.enemySpawnWarmup;
    enemy->spawnTimer = spawnWarmup;
    enemy->spawnDuration = spawnWarmup;
    if (detectedOnSpawn) {
        forceDetectInSight(*enemy, detectedTarget, true);
    }
    if (outRuntimeId != nullptr) {
        *outRuntimeId = enemy->id;
    }
    soundEvents_.push_back(EnemySoundEvent{
        .cueId = std::string(AudioSeEnemySpawn),
        .position = position,
    });
    return true;
}

bool EnemySystem::queueSwarmSpawn(
    Enemy& enemy,
    Vec2 detectedTarget,
    std::vector<SwarmSpawnRequest>& outRequests)
{
    if (!enemy.swarmSpawnEnabled || enemy.swarmSpawnExecuted || enemy.definition == nullptr) {
        return false;
    }

    enemy.swarmSpawnExecuted = true;
    outRequests.push_back(SwarmSpawnRequest{
        .definition = enemy.definition,
        .origin = enemy.position,
        .spawnSource = enemy.spawnSource,
        .lootStageId = enemy.lootStageId,
        .lootDepthRank = enemy.lootDepthRank,
        .parentRuntimeId = enemy.id,
        .count = std::clamp(enemy.swarmSpawnCount, 1, SwarmSpawnCountMax),
        .radius = enemy.swarmSpawnRadius,
        .childPassageRadius = enemyPassageRadius(enemy, placementCatalog_),
        .detectedOnSpawn = enemy.awareness == EnemyAwarenessState::Detected,
        .detectedTarget = detectedTarget,
        .screenSleepAllowed = enemy.screenSleepAllowed,
    });
    return true;
}

int EnemySystem::processSwarmSpawnRequest(
    const SwarmSpawnRequest& request,
    TileMap& map,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog)
{
    if (request.definition == nullptr || request.count <= 0 || request.radius <= 0.0f) {
        return 0;
    }

    const int count = std::clamp(request.count, 1, SwarmSpawnCountMax);
    const float childRadius = request.childPassageRadius > 0.0f
        ? request.childPassageRadius
        : enemyDefinitionSpawnRadius(request.definition, balance, placementCatalog_);
    int spawnedChildren = 0;
    for (int i = 0; i < count; ++i) {
        const float angle = (Pi * 2.0f) * (static_cast<float>(i) / static_cast<float>(count));
        const Vec2 spawnPos = request.origin + fromAngle(angle) * request.radius;
        if (map.isCircleBlocked(spawnPos, childRadius)) {
            continue;
        }

        bool overlapsEnemy = false;
        for (const Enemy& other : enemies_.items()) {
            if (!other.active || other.id == request.parentRuntimeId) {
                continue;
            }
            const float minDistance = enemyPassageRadius(other, placementCatalog_) + childRadius + SpawnAvoidancePadding;
            if (distanceSquared(spawnPos, other.position) < minDistance * minDistance) {
                overlapsEnemy = true;
                break;
            }
        }
        if (overlapsEnemy) {
            continue;
        }

        Enemy* child = enemies_.acquire();
        if (child == nullptr) {
            break;
        }

        *child = Enemy{};
        child->active = true;
        child->id = nextEnemyId_++;
        child->spawnSource = request.spawnSource;
        child->screenSleepAllowed = request.spawnSource == EnemySpawnSource::Ambient && request.screenSleepAllowed;
        child->position = spawnPos;
        applyDefinition(*child, request.definition, balance, enemyCatalog);
        child->spawnSource = request.spawnSource;
        child->screenSleepAllowed = request.spawnSource == EnemySpawnSource::Ambient && request.screenSleepAllowed;
        child->lootStageId = request.lootStageId;
        child->lootDepthRank = request.lootDepthRank;
        child->spawnTimer = 0.4f;
        child->spawnDuration = child->spawnTimer;
        child->swarmSpawnEnabled = false;
        child->swarmSpawnExecuted = true;
        if (request.detectedOnSpawn) {
            forceDetectInSight(*child, request.detectedTarget, true);
        }
        ++spawnedChildren;
    }

    if (spawnedChildren > 0) {
        if (const Enemy* parent = findRuntimeEnemy(request.parentRuntimeId)) {
            events_.push_back(makeEnemyEvent(EnemyEventType::Spawn, *parent));
            soundEvents_.push_back(EnemySoundEvent{
                .cueId = std::string(AudioSeEnemySpawn),
                .position = parent->position,
            });
        }
    }
    return spawnedChildren;
}

void EnemySystem::spawnAt(Vec2 position, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn, Vec2 detectedTarget)
{
    spawnDefinitionAt(position, chooseEnemyDefinition(enemyCatalog), balance, enemyCatalog, detectedOnSpawn, detectedTarget);
}

bool EnemySystem::spawnBossAt(
    Vec2 position,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    std::string_view bossEnemyId,
    bool detectedOnSpawn,
    Vec2 detectedTarget,
    EnemyVariantTier variantTier,
    int effectiveBaseLevel,
    EnemySpawnVisualKind spawnVisualKind)
{
    Enemy* enemy = enemies_.acquire();
    if (!enemy) {
        return false;
    }
    *enemy = Enemy{};
    enemy->active = true;
    enemy->id = nextEnemyId_++;
    enemy->isBoss = true;
    enemy->spawnSource = EnemySpawnSource::Boss;
    enemy->position = position;
    const EnemyDefinition* definition = findEnemyDefinitionById(enemyCatalog, bossEnemyId);
    if (definition == nullptr && bossEnemyId.empty()) {
        definition = chooseEnemyDefinition(enemyCatalog);
    }
    applyDefinition(*enemy, definition, balance, enemyCatalog);
    enemy->spawnSource = EnemySpawnSource::Boss;
    if (definition == nullptr || (!bossEnemyId.empty() && enemy->enemyId != bossEnemyId)) {
        applyFallbackBossDefinition(*enemy, bossEnemyId, balance);
    }
    applyEnemyVariant(*enemy, enemyCatalog, variantTier, effectiveBaseLevel);
    enableDefaultBossActionIfNeeded(*enemy, bossEnemyId);
    enemy->radius *= BossRadiusMultiplier;
    enemy->maxHp = std::max(1, enemy->maxHp);
    enemy->hp = enemy->maxHp;
    if (isAstragnaBossAction(*enemy)) {
        enemy->maxHp = 1;
        enemy->hp = 1;
        enemy->hpBarTimer = 0.0f;
    }
    enemy->xp = std::max(0, enemy->xp);
    enemy->spawnTimer = balance.enemySpawnWarmup * 1.6f;
    enemy->spawnDuration = enemy->spawnTimer;
    enemy->spawnVisualKind = spawnVisualKind;
    const Vec2 toFacingTarget = detectedTarget - enemy->position;
    if (lengthSquared(toFacingTarget) > 0.0001f) {
        enemy->facingAngle = std::atan2(toFacingTarget.y, toFacingTarget.x);
    }
    if (detectedOnSpawn) {
        forceDetectInSight(*enemy, detectedTarget, true);
    }
    return true;
}

bool EnemySystem::findSpawnPosition(
    TileMap& map,
    Vec2 desiredPosition,
    Vec2 playerPosition,
    float radius,
    float minPlayerDistance,
    Vec2& outPosition) const
{
    const float spacing = radius * 2.4f;
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

    const float minPlayerDistanceSq = minPlayerDistance * minPlayerDistance;
    for (Vec2 offset : offsets) {
        const Vec2 candidate = desiredPosition + offset;
        if (distanceSquared(candidate, playerPosition) < minPlayerDistanceSq) {
            continue;
        }
        if (map.isCircleBlocked(candidate, radius)) {
            continue;
        }

        bool overlapsEnemy = false;
        for (const Enemy& enemy : enemies_.items()) {
            if (!enemy.active) {
                continue;
            }
            const float minDistance = enemyPassageRadius(enemy, placementCatalog_) + radius + SpawnAvoidancePadding;
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

bool EnemySystem::findSpawnPosition(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, Vec2& outPosition) const
{
    return findSpawnPosition(map, desiredPosition, playerPosition, balance.enemyRadius, balance.enemyMinSpawnDistance, outPosition);
}

bool EnemySystem::findBossSpawnPosition(TileMap& map, Vec2 playerPosition, const RuntimeBalance& balance, Vec2& outPosition) const
{
    const float radius = balance.enemyRadius * BossRadiusMultiplier;
    const int playerTileX = map.worldToTile(playerPosition.x);
    const int playerTileY = map.worldToTile(playerPosition.y);

    for (int ring = 6; ring <= 18; ++ring) {
        for (int dy = -ring; dy <= ring; ++dy) {
            for (int dx = -ring; dx <= ring; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != ring) {
                    continue;
                }
                const Vec2 candidate = map.tileCenter(playerTileX + dx, playerTileY + dy);
                if (distanceSquared(candidate, playerPosition) < BossMinSpawnDistance * BossMinSpawnDistance) {
                    continue;
                }
                if (map.isCircleBlocked(candidate, radius)) {
                    continue;
                }

                bool overlapsEnemy = false;
                for (const Enemy& enemy : enemies_.items()) {
                    if (!enemy.active) {
                        continue;
                    }
                    const float minDistance = enemyPassageRadius(enemy, placementCatalog_) + radius + SpawnAvoidancePadding;
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
        }
    }

    return findSpawnPosition(
        map,
        playerPosition + Vec2{BossMinSpawnDistance, 0.0f},
        playerPosition,
        radius,
        BossMinSpawnDistance,
        outPosition);
}

std::vector<DugEnemySpawnRequest> EnemySystem::collectDugSpawnRequests(
    const std::vector<DugEnemySpawnPoint>& dugTiles,
    TileMap& map,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    int reservedAmbientSpawns)
{
    std::vector<DugEnemySpawnRequest> requests;
    const int reservedCount = std::max(0, reservedAmbientSpawns);
    const int softCap = std::max(0, balance.enemySoftCap);
    if (ambientActiveCount() + reservedCount >= softCap) {
        return requests;
    }
    const int minDugTiles = std::max(1, balance.enemyMinDugTiles);
    const int guaranteeDugTiles = std::max(minDugTiles, balance.enemyGuaranteeDugTiles);
    const int randomWindow = std::max(1, guaranteeDugTiles - minDugTiles + 1);
    std::uniform_int_distribution<int> randomTrigger(1, randomWindow);
    for (const DugEnemySpawnPoint& spawnPoint : dugTiles) {
        ++dugSpawnCounter_;
        if (dugSpawnCounter_ < minDugTiles) {
            continue;
        }
        const bool guaranteed = dugSpawnCounter_ >= guaranteeDugTiles;
        const bool randomHit = !guaranteed && randomTrigger(rng_) == 1;
        if (!guaranteed && !randomHit) {
            continue;
        }
        Vec2 spawnPosition{};
        if (!findSpawnPosition(map, spawnPoint.tileCenter, playerPosition, balance, spawnPosition)) {
            continue;
        }
        requests.push_back(DugEnemySpawnRequest{
            .position = spawnPosition,
            .depthRank = spawnPoint.depthRank,
        });
        dugSpawnCounter_ = 0;
        if (ambientActiveCount() + reservedCount + static_cast<int>(requests.size()) >= softCap) {
            return requests;
        }
    }
    return requests;
}

bool EnemySystem::spawnNodeEnemy(
    TileMap& map,
    Vec2 desiredPosition,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    bool allowNearPlayer,
    bool detectedOnSpawn,
    std::string_view lootStageId,
    int lootDepthRank,
    float spawnWarmupOverride)
{
    if (ambientActiveCount() >= balance.enemySoftCap) {
        return false;
    }

    Vec2 spawnPosition{};
    const float minPlayerDistance = allowNearPlayer ? 0.0f : balance.enemyMinSpawnDistance;
    const EnemySpawnSelection selection = chooseDugSpawnEnemy(enemyCatalog, lootStageId, lootDepthRank);
    const float radius = enemyDefinitionSpawnRadius(selection.definition, balance, placementCatalog_);
    if (!findSpawnPosition(map, desiredPosition, playerPosition, radius, minPlayerDistance, spawnPosition)) {
        return false;
    }

    spawnDefinitionAt(
        spawnPosition,
        selection.definition,
        balance,
        enemyCatalog,
        detectedOnSpawn,
        playerPosition,
        spawnWarmupOverride,
        nullptr,
        lootStageId,
        lootDepthRank,
        selection.variantTier,
        selection.effectiveBaseLevel,
        EnemySpawnSource::Ambient,
        true);
    return true;
}

bool EnemySystem::spawnFixedNodeEnemy(
    TileMap& map,
    Vec2 desiredPosition,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    bool detectedOnSpawn,
    int* outRuntimeId,
    std::string_view lootStageId,
    int lootDepthRank)
{
    if (ambientActiveCount() >= balance.enemySoftCap) {
        return false;
    }

    const EnemySpawnSelection selection = chooseDugSpawnEnemy(enemyCatalog, lootStageId, lootDepthRank);
    const EnemyDefinition* definition = selection.definition;
    const float radius = enemyDefinitionSpawnRadius(definition, balance, placementCatalog_);

    if (map.isCircleBlocked(desiredPosition, radius)) {
        return false;
    }

    for (const Enemy& enemy : enemies_.items()) {
        if (!enemyVisible(enemy)) {
            continue;
        }
        const float minDistance = enemyPassageRadius(enemy, placementCatalog_) + radius + SpawnAvoidancePadding;
        if (distanceSquared(desiredPosition, enemy.position) < minDistance * minDistance) {
            return false;
        }
    }

    return spawnDefinitionAt(
        desiredPosition,
        definition,
        balance,
        enemyCatalog,
        detectedOnSpawn,
        playerPosition,
        0.0f,
        outRuntimeId,
        lootStageId,
        lootDepthRank,
        selection.variantTier,
        selection.effectiveBaseLevel,
        EnemySpawnSource::Ambient,
        outRuntimeId == nullptr);
}

bool EnemySystem::spawnSpecificEnemy(
    TileMap& map,
    std::string_view enemyId,
    Vec2 desiredPosition,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    bool allowNearPlayer,
    bool detectedOnSpawn,
    float spawnWarmupOverride,
    int* outRuntimeId,
    std::string_view lootStageId,
    int lootDepthRank)
{
    if (ambientActiveCount() >= balance.enemySoftCap) {
        return false;
    }

    const auto it = enemyCatalog.enemiesById.find(std::string(enemyId));
    if (it == enemyCatalog.enemiesById.end()) {
        return false;
    }

    Vec2 spawnPosition{};
    const float minPlayerDistance = allowNearPlayer ? 0.0f : balance.enemyMinSpawnDistance;
    if (!findSpawnPosition(map, desiredPosition, playerPosition, balance.enemyRadius, minPlayerDistance, spawnPosition)) {
        return false;
    }

    return spawnDefinitionAt(
        spawnPosition,
        &it->second,
        balance,
        enemyCatalog,
        detectedOnSpawn,
        playerPosition,
        spawnWarmupOverride,
        outRuntimeId,
        lootStageId,
        lootDepthRank,
        EnemyVariantTier::Normal,
        0,
        EnemySpawnSource::Ambient,
        outRuntimeId == nullptr);
}

bool EnemySystem::spawnSpecificEnemyAtPosition(
    TileMap& map,
    std::string_view enemyId,
    Vec2 position,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    bool detectedOnSpawn,
    float spawnWarmupOverride,
    int* outRuntimeId,
    std::string_view lootStageId,
    int lootDepthRank)
{
    if (ambientActiveCount() >= balance.enemySoftCap) {
        return false;
    }

    const auto it = enemyCatalog.enemiesById.find(std::string(enemyId));
    if (it == enemyCatalog.enemiesById.end()) {
        return false;
    }

    const EnemyDefinition& definition = it->second;
    const float radius = enemyDefinitionSpawnRadius(&definition, balance, placementCatalog_);

    if (map.isCircleBlocked(position, radius)) {
        return false;
    }

    for (const Enemy& enemy : enemies_.items()) {
        if (!enemyVisible(enemy)) {
            continue;
        }
        const float minDistance = enemyPassageRadius(enemy, placementCatalog_) + radius + SpawnAvoidancePadding;
        if (distanceSquared(position, enemy.position) < minDistance * minDistance) {
            return false;
        }
    }

    return spawnDefinitionAt(
        position,
        &definition,
        balance,
        enemyCatalog,
        detectedOnSpawn,
        playerPosition,
        spawnWarmupOverride,
        outRuntimeId,
        lootStageId,
        lootDepthRank,
        EnemyVariantTier::Normal,
        0,
        EnemySpawnSource::Ambient,
        outRuntimeId == nullptr);
}

bool EnemySystem::spawnEventEnemy(
    TileMap& map,
    Vec2 desiredPosition,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    const EventEnemySpawnOptions& options,
    int* outRuntimeId)
{
    EnemySpawnSelection selection;
    if (!options.enemyId.empty()) {
        const auto it = enemyCatalog.enemiesById.find(options.enemyId);
        if (it != enemyCatalog.enemiesById.end()) {
            selection.definition = &it->second;
            selection.effectiveBaseLevel = std::max(1, it->second.baseLevel);
        }
    }
    if (selection.definition == nullptr) {
        if (options.stageId.empty()) {
            selection.definition = chooseEnemyDefinition(enemyCatalog);
            selection.effectiveBaseLevel = selection.definition != nullptr ? std::max(1, selection.definition->baseLevel) : 1;
        } else {
            selection = chooseDugSpawnEnemy(enemyCatalog, options.stageId, options.depthRank);
        }
    }
    const EnemyDefinition* definition = selection.definition;

    const float radius = enemyDefinitionSpawnRadius(definition, balance, placementCatalog_, options.radiusMultiplier);

    Vec2 spawnPosition = desiredPosition;
    if (options.fixedPosition) {
        if (map.isCircleBlocked(spawnPosition, radius)) {
            return false;
        }
        for (const Enemy& enemy : enemies_.items()) {
            if (!enemy.active) {
                continue;
            }
            const float minDistance = enemyPassageRadius(enemy, placementCatalog_) + radius + SpawnAvoidancePadding;
            if (distanceSquared(spawnPosition, enemy.position) < minDistance * minDistance) {
                return false;
            }
        }
    } else {
        const float minPlayerDistance = options.allowNearPlayer ? 0.0f : balance.enemyMinSpawnDistance;
        if (!findSpawnPosition(map, desiredPosition, playerPosition, radius, minPlayerDistance, spawnPosition)) {
            return false;
        }
    }

    int runtimeId = 0;
    if (!spawnDefinitionAt(
            spawnPosition,
            definition,
            balance,
            enemyCatalog,
            options.detectedOnSpawn && !options.sleeping,
            playerPosition,
            (options.sleeping || options.activationLocked) ? 0.0f : -1.0f,
            &runtimeId,
            options.stageId,
            options.depthRank,
            selection.variantTier,
            selection.effectiveBaseLevel,
            EnemySpawnSource::Event)) {
        return false;
    }
    Enemy* enemy = findRuntimeEnemy(runtimeId);
    if (enemy == nullptr) {
        return false;
    }

    enemy->dungeonEventId = options.dungeonEventId;
    enemy->dungeonEventSleeping = options.sleeping;
    enemy->dungeonEventActivationLocked = options.activationLocked;
    enemy->dungeonEventBoss = options.bossVariant;
    if (options.sleeping) {
        (void)enemy->status.applyState("status_sleep", 1.0, -1.0, "dungeon_event:" + options.dungeonEventId, StateApplyMode::KeepLonger);
        enemy->awareness = EnemyAwarenessState::Unaware;
        enemy->awarenessIcon = EnemyAwarenessIcon::None;
        enemy->awarenessIconTimer = 0.0f;
    }
    if (options.activationLocked) {
        enemy->awareness = EnemyAwarenessState::Unaware;
        enemy->awarenessIcon = EnemyAwarenessIcon::None;
        enemy->awarenessIconTimer = 0.0f;
        enemy->velocity = {};
    }
    const float hpMultiplier = std::max(0.1f, options.hpMultiplier);
    if (std::abs(hpMultiplier - 1.0f) > 0.001f) {
        enemy->maxHp = std::max(1, static_cast<int>(std::ceil(static_cast<float>(enemy->maxHp) * hpMultiplier)));
        enemy->hp = enemy->maxHp;
    }
    enemy->contactDamageMultiplier *= std::max(0.1f, options.contactDamageMultiplier);
    enemy->radius *= std::max(0.1f, options.radiusMultiplier);
    const float xpMultiplier = std::max(0.0f, options.xpMultiplier);
    if (std::abs(xpMultiplier - 1.0f) > 0.001f) {
        enemy->xp = std::max(0, static_cast<int>(std::ceil(static_cast<float>(enemy->xp) * xpMultiplier)));
    }
    if (outRuntimeId != nullptr) {
        *outRuntimeId = runtimeId;
    }
    return true;
}

bool EnemySystem::spawnBoss(
    TileMap& map,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    std::string_view bossEnemyId,
    EnemyVariantTier variantTier,
    int effectiveBaseLevel,
    EnemySpawnVisualKind spawnVisualKind)
{
    if (bossActive()) {
        return false;
    }

    Vec2 spawnPosition{};
    if (!findBossSpawnPosition(map, playerPosition, balance, spawnPosition)) {
        return false;
    }

    return spawnBossAt(spawnPosition, balance, enemyCatalog, bossEnemyId, false, playerPosition, variantTier, effectiveBaseLevel, spawnVisualKind);
}

bool EnemySystem::spawnBossNear(
    TileMap& map,
    Vec2 desiredPosition,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    std::string_view bossEnemyId,
    EnemyVariantTier variantTier,
    int effectiveBaseLevel,
    EnemySpawnVisualKind spawnVisualKind)
{
    if (bossActive()) {
        return false;
    }

    Vec2 spawnPosition{};
    if (!findSpawnPosition(
            map,
            desiredPosition,
            playerPosition,
            bossSpawnRadiusFor(enemyCatalog, bossEnemyId, balance, placementCatalog_),
            0.0f,
            spawnPosition)) {
        return false;
    }

    return spawnBossAt(spawnPosition, balance, enemyCatalog, bossEnemyId, false, playerPosition, variantTier, effectiveBaseLevel, spawnVisualKind);
}

bool EnemySystem::spawnBossPreviewAt(
    Vec2 position,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    std::string_view bossEnemyId)
{
    if (bossActive()) {
        return false;
    }

    if (!spawnBossAt(
            position,
            balance,
            enemyCatalog,
            bossEnemyId,
            false,
            playerPosition,
            EnemyVariantTier::Normal,
            0,
            EnemySpawnVisualKind::Default)) {
        return false;
    }

    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || !enemy.isBoss) {
            continue;
        }
        if (!bossEnemyId.empty() && enemy.enemyId != bossEnemyId) {
            continue;
        }
        enemy.bossAction.previewOnly = true;
        enemy.bossAction.invulnerable = true;
        enemy.spawnTimer = 0.0f;
        enemy.spawnDuration = 0.0f;
        enemy.velocity = {};
        if (isAstragnaBossAction(enemy)) {
            initializeAstragnaBoss(enemy);
        }
        return true;
    }
    return false;
}

bool EnemySystem::activateBossPreview(std::string_view bossEnemyId)
{
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || !enemy.isBoss || !enemy.bossAction.previewOnly) {
            continue;
        }
        if (!bossEnemyId.empty() && enemy.enemyId != bossEnemyId) {
            continue;
        }
        enemy.bossAction.previewOnly = false;
        enemy.bossAction.invulnerable = false;
        enemy.spawnTimer = 0.0f;
        enemy.spawnDuration = 0.0f;
        enemy.velocity = {};
        return true;
    }
    return false;
}

bool EnemySystem::advanceBossSpawnPresentation(float dt)
{
    const float safeDt = std::max(0.0f, dt);
    bool activeAfterAdvance = false;
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || !enemy.isBoss || enemy.spawnTimer <= 0.0f) {
            continue;
        }
        enemy.action = {};
        enemy.spawnTimer = std::max(0.0f, enemy.spawnTimer - safeDt);
        enemy.behaviorTimer += safeDt;
        if (enemy.spawnVisualKind == EnemySpawnVisualKind::WalkIn) {
            const float duration = std::max(0.001f, enemy.spawnDuration);
            const float progress = 1.0f - clamp(enemy.spawnTimer / duration, 0.0f, 1.0f);
            const float eased = smoothStep01(progress);
            const Vec2 start = enemy.spawnPresentationStartPosition;
            const Vec2 end = enemy.spawnPresentationEndPosition;
            const Vec2 travel = end - start;
            const Vec2 direction = safeDirection(travel, facingVector(enemy.facingAngle));
            enemy.position = enemy.spawnTimer > 0.0f
                ? lerp(start, end, eased)
                : end;
            enemy.velocity = enemy.spawnTimer > 0.0f
                ? direction * (length(travel) / duration)
                : Vec2{};
            enemy.aiMoveDirection = direction;
            enemy.facingAngle = std::atan2(direction.y, direction.x);
        }
        activeAfterAdvance = activeAfterAdvance || enemy.spawnTimer > 0.0f;
    }
    return activeAfterAdvance;
}

bool EnemySystem::configureActiveBossWalkInPresentation(Vec2 startPosition, float durationSeconds)
{
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || !enemy.isBoss) {
            continue;
        }
        const float duration = std::max(0.1f, durationSeconds);
        const Vec2 endPosition = enemy.position;
        enemy.spawnVisualKind = EnemySpawnVisualKind::WalkIn;
        enemy.spawnPresentationStartPosition = startPosition;
        enemy.spawnPresentationEndPosition = endPosition;
        enemy.position = startPosition;
        enemy.spawnTimer = duration;
        enemy.spawnDuration = duration;
        const Vec2 direction = safeDirection(endPosition - startPosition, facingVector(enemy.facingAngle));
        enemy.velocity = direction * (length(endPosition - startPosition) / duration);
        enemy.aiMoveDirection = direction;
        enemy.facingAngle = std::atan2(direction.y, direction.x);
        enemy.behaviorTimer = 0.0f;
        return true;
    }
    return false;
}

void EnemySystem::updateBossStoryVisuals(float dt)
{
    const float safeDt = std::max(0.0f, dt);
    if (safeDt <= 0.0f) {
        return;
    }

    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || !enemy.isBoss || enemy.death.active || enemy.spawnTimer > 0.0f) {
            continue;
        }
        enemy.behaviorTimer += safeDt;
    }
}

bool EnemySystem::bossActive() const
{
    for (const Enemy& enemy : enemies_.items()) {
        if (enemy.active && enemy.isBoss) {
            return true;
        }
    }
    return false;
}

int EnemySystem::ambientActiveCount() const
{
    int count = 0;
    for (const Enemy& enemy : enemies_.items()) {
        if (enemy.active && enemy.spawnSource == EnemySpawnSource::Ambient) {
            ++count;
        }
    }
    return count;
}

int EnemySystem::syncScreenDormantEnemies(const CollisionRect& activeBounds, SpellRingSystem& spellRing)
{
    int changed = 0;
    for (Enemy& enemy : enemies_.items()) {
        if (!enemyScreenSleepEligible(enemy)) {
            continue;
        }
        const float radius = std::max(enemyPassageRadius(enemy, placementCatalog_), enemyVisualRadius(enemy));
        if (circleIntersectsRect(enemy.position, radius, activeBounds)) {
            continue;
        }

        dormantEnemies_.push_back(enemy);
        enemy = Enemy{};
        ++changed;
    }

    for (std::size_t i = 0; i < dormantEnemies_.size();) {
        Enemy& dormant = dormantEnemies_[i];
        const float radius = std::max(enemyPassageRadius(dormant, placementCatalog_), enemyVisualRadius(dormant));
        if (!circleIntersectsRect(dormant.position, radius, activeBounds)) {
            ++i;
            continue;
        }

        Enemy* slot = enemies_.acquire();
        if (slot == nullptr) {
            ++i;
            continue;
        }
        *slot = std::move(dormant);
        slot->active = true;
        if (i + 1 < dormantEnemies_.size()) {
            dormantEnemies_[i] = std::move(dormantEnemies_.back());
        }
        dormantEnemies_.pop_back();
        ++changed;
    }
    return changed;
}

int EnemySystem::eventActiveCount() const
{
    int count = 0;
    for (const Enemy& enemy : enemies_.items()) {
        if (enemy.active && enemy.spawnSource == EnemySpawnSource::Event) {
            ++count;
        }
    }
    return count;
}

int EnemySystem::bossSourceActiveCount() const
{
    int count = 0;
    for (const Enemy& enemy : enemies_.items()) {
        if (enemy.active && enemy.spawnSource == EnemySpawnSource::Boss) {
            ++count;
        }
    }
    return count;
}

void EnemySystem::appendMinimapMarkers(std::vector<EnemyMinimapMarker>& markers) const
{
    const auto appendMarker = [this, &markers](const Enemy& enemy) {
        if (!enemyVisible(enemy) || enemy.dungeonEventActivationLocked || enemy.death.active || enemy.spawnTimer > 0.0f) {
            return;
        }
        markers.push_back(EnemyMinimapMarker{
            .position = enemy.position,
            .radius = enemyPassageRadius(enemy, placementCatalog_),
            .jumpLandingRadius = enemy.jumpLandingRadius,
            .countdownExplodeRadius = enemy.countdownExplodeRadius,
            .contactAttackPower = enemy.contactAttackPower,
            .contactDamageMultiplier = enemy.contactDamageMultiplier,
            .ranged = isRangedBehavior(enemy.rangedBehaviorId),
            .boss = enemy.isBoss,
        });
    };
    for (const Enemy& enemy : enemies_.items()) {
        appendMarker(enemy);
    }
    for (const Enemy& enemy : dormantEnemies_) {
        appendMarker(enemy);
    }
}

bool EnemySystem::updateBossActionSequence(Enemy& enemy, Player& player, TileMap& map, ProjectileSystem& projectiles, float dt)
{
    if (!enemy.bossAction.enabled || enemy.bossAction.pattern.empty()) {
        return false;
    }
    if (enemy.bossAction.pattern == JunkCrabPatternId) {
        return updateJunkCrabBossActionSequence(enemy, player, map, dt, events_, rng_, placementCatalog_);
    }
    if (enemy.bossAction.pattern == AstragnaPatternId) {
        return updateAstragnaBossActionSequence(enemy, player, map, projectiles, dt, events_);
    }
    if (enemy.bossAction.pattern != StardustMolePatternId) {
        return false;
    }

    if (enemy.bossAction.phase == BossActionPhase::None) {
        enterBossActionPhase(enemy, BossActionPhase::Approach, player, map, events_, rng_, placementCatalog_);
        return true;
    }

    enemy.bossAction.timer += std::max(0.0f, dt);
    switch (enemy.bossAction.phase) {
    case BossActionPhase::Approach:
        moveStardustMoleApproach(enemy, player, map, dt, placementCatalog_);
        if (enemy.bossAction.timer >= enemy.bossAction.phaseDuration) {
            enterBossActionPhase(enemy, BossActionPhase::Submerge, player, map, events_, rng_, placementCatalog_);
        }
        return true;
    case BossActionPhase::Submerge:
        if (!enemy.bossAction.hidden) {
            const bool diveFinished =
                updateBossActionJumpMotion(enemy, dt) ||
                enemy.bossAction.timer >= StardustMoleDiveJumpSeconds;
            if (diveFinished) {
                enemy.bossAction.hidden = true;
                enemy.bossAction.invulnerable = true;
                enemy.velocity = {};
                enemy.jumpActive = false;
                enemy.jumpElapsedSeconds = 0.0f;
                enemy.jumpDurationSeconds = 0.0f;
                enemy.jumpArcHeight = 0.0f;
                enemy.altitude = 0.0f;
                events_.push_back(makeEnemyEventAt(EnemyEventType::BossImpact, enemy, enemy.position, "burrow"));
            }
        }
        if (enemy.bossAction.timer >= enemy.bossAction.phaseDuration) {
            enterBossActionPhase(enemy, BossActionPhase::Telegraph, player, map, events_, rng_, placementCatalog_);
        }
        return true;
    case BossActionPhase::Telegraph:
        if (enemy.bossAction.timer >= enemy.bossAction.phaseDuration) {
            enterBossActionPhase(enemy, BossActionPhase::Jump, player, map, events_, rng_, placementCatalog_);
        }
        return true;
    case BossActionPhase::Jump: {
        if (updateBossActionJumpMotion(enemy, dt)) {
            enterBossActionPhase(enemy, BossActionPhase::LandingDelay, player, map, events_, rng_, placementCatalog_);
        }
        return true;
    }
    case BossActionPhase::LandingDelay:
        if (enemy.bossAction.timer >= enemy.bossAction.phaseDuration) {
            enterBossActionPhase(enemy, BossActionPhase::Charge, player, map, events_, rng_, placementCatalog_);
        }
        return true;
    case BossActionPhase::Charge:
        if (!moveBossCharge(enemy, map, dt, events_, placementCatalog_)) {
            enterBossActionPhase(enemy, BossActionPhase::Stun, player, map, events_, rng_, placementCatalog_);
            return true;
        }
        if (enemy.bossAction.timer >= enemy.bossAction.phaseDuration) {
            enterBossActionPhase(enemy, BossActionPhase::Recover, player, map, events_, rng_, placementCatalog_);
        }
        return true;
    case BossActionPhase::Stun:
        if (enemy.bossAction.timer >= enemy.bossAction.phaseDuration) {
            enterBossActionPhase(enemy, BossActionPhase::Recover, player, map, events_, rng_, placementCatalog_);
        }
        return true;
    case BossActionPhase::Recover:
        if (enemy.bossAction.timer >= enemy.bossAction.phaseDuration) {
            enterBossActionPhase(enemy, BossActionPhase::Approach, player, map, events_, rng_, placementCatalog_);
        }
        return true;
    case BossActionPhase::None:
        break;
    }
    return true;
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

    std::priority_queue<FlowNode, std::vector<FlowNode>, FlowNodeGreater> open;
    flowDistance_[static_cast<std::size_t>(index(playerTileX, playerTileY))] = 0;
    open.push({0, playerTileX, playerTileY});

    while (!open.empty()) {
        const FlowNode node = open.top();
        open.pop();
        const int tx = node.tx;
        const int ty = node.ty;
        const int current = flowDistance_[static_cast<std::size_t>(index(tx, ty))];
        if (current < 0 || node.distance != current) {
            continue;
        }
        for (const FlowStep& step : FlowDirections) {
            const int nx = tx + step.dx;
            const int ny = ty + step.dy;
            if (!inBounds(nx, ny) || map.isTileSolid(nx, ny)) {
                continue;
            }
            if (!canUseFlowStep(map, tx, ty, step)) {
                continue;
            }
            int& next = flowDistance_[static_cast<std::size_t>(index(nx, ny))];
            const int nextDistance = current + step.cost;
            if (next >= 0 && next <= nextDistance) {
                continue;
            }
            next = nextDistance;
            open.push({nextDistance, nx, ny});
        }
    }
}

Vec2 EnemySystem::flowDirectionFor(TileMap& map, Vec2 enemyPosition, Vec2 playerPosition) const
{
    const Vec2 toPlayer = playerPosition - enemyPosition;
    if (lengthSquared(toPlayer) <= 0.0001f) {
        return {};
    }
    if (hasClearSightLine(map, enemyPosition, playerPosition)) {
        return normalize(toPlayer);
    }

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
    Vec2 blendedDirection{};
    for (const FlowStep& step : FlowDirections) {
        const int nx = enemyTileX + step.dx;
        const int ny = enemyTileY + step.dy;
        if (!inBounds(nx, ny)) {
            continue;
        }
        if (!canUseFlowStep(map, enemyTileX, enemyTileY, step)) {
            continue;
        }
        const int candidate = flowDistance_[static_cast<std::size_t>(index(nx, ny))];
        if (candidate >= 0 && candidate < bestDistance) {
            bestDistance = candidate;
            bestTarget = map.tileCenter(nx, ny);
        }
        if (candidate >= 0 && candidate < current) {
            const Vec2 targetDirection = normalize(map.tileCenter(nx, ny) - enemyPosition);
            blendedDirection += targetDirection * static_cast<float>(std::max(1, current - candidate));
        }
    }

    if (lengthSquared(blendedDirection) > 0.0001f) {
        return normalize(blendedDirection);
    }
    if (bestDistance == current) {
        bestTarget = playerPosition;
    }
    return normalize(bestTarget - enemyPosition);
}

bool EnemySystem::planFleeWaypoint(TileMap& map, const Enemy& enemy, Vec2 playerPosition, Vec2& outWaypoint) const
{
    if (flowDistance_.empty() || flowWidth_ <= 0 || flowHeight_ <= 0) {
        return false;
    }

    const Vec2 awayFromPlayer = enemy.position - playerPosition;
    if (lengthSquared(awayFromPlayer) <= 0.0001f) {
        return false;
    }

    const int startTileX = map.worldToTile(enemy.position.x);
    const int startTileY = map.worldToTile(enemy.position.y);
    const float passageRadius = enemyPassageRadius(enemy, placementCatalog_);
    const Vec2 away = normalize(awayFromPlayer);
    const Vec2 previousHeading = lengthSquared(enemy.fleeNavigation.heading) > 0.0001f
        ? normalize(enemy.fleeNavigation.heading)
        : away;
    const bool avoidFailedDirection =
        enemy.fleeNavigation.failedDirectionTimer > 0.0f &&
        lengthSquared(enemy.fleeNavigation.failedDirection) > 0.0001f;
    const Vec2 failedDirection = avoidFailedDirection
        ? normalize(enemy.fleeNavigation.failedDirection)
        : Vec2{};

    const auto inFlowBounds = [&](int tx, int ty) {
        return tx >= flowMinX_ && ty >= flowMinY_ &&
            tx < flowMinX_ + flowWidth_ && ty < flowMinY_ + flowHeight_;
    };
    const auto flowIndex = [&](int tx, int ty) {
        return (ty - flowMinY_) * flowWidth_ + (tx - flowMinX_);
    };
    if (!inFlowBounds(startTileX, startTileY)) {
        return false;
    }
    const int startFlowDistance = flowDistance_[static_cast<std::size_t>(flowIndex(startTileX, startTileY))];
    if (startFlowDistance < 0) {
        return false;
    }

    const auto localIndex = [](int localX, int localY) {
        return localY * FleeSearchDiameterTiles + localX;
    };
    const auto tileForLocalIndex = [&](int index) {
        const int localX = index % FleeSearchDiameterTiles;
        const int localY = index / FleeSearchDiameterTiles;
        return std::pair{
            startTileX + localX - FleeSearchRadiusTiles,
            startTileY + localY - FleeSearchRadiusTiles,
        };
    };
    const int startLocalIndex = localIndex(FleeSearchRadiusTiles, FleeSearchRadiusTiles);
    std::array<int, FleeSearchTileCount> routeCosts;
    std::array<int, FleeSearchTileCount> parents;
    routeCosts.fill(std::numeric_limits<int>::max());
    parents.fill(-1);
    routeCosts[static_cast<std::size_t>(startLocalIndex)] = 0;

    std::priority_queue<FleeSearchNode, std::vector<FleeSearchNode>, FleeSearchNodeGreater> open;
    open.push({0, startLocalIndex});

    int bestImprovingIndex = -1;
    float bestImprovingScore = -std::numeric_limits<float>::infinity();
    int bestFallbackIndex = -1;
    float bestFallbackScore = -std::numeric_limits<float>::infinity();
    while (!open.empty()) {
        const FleeSearchNode node = open.top();
        open.pop();
        if (node.routeCost != routeCosts[static_cast<std::size_t>(node.localIndex)]) {
            continue;
        }

        const auto [tx, ty] = tileForLocalIndex(node.localIndex);
        if (node.localIndex != startLocalIndex) {
            const int candidateFlowDistance = flowDistance_[static_cast<std::size_t>(flowIndex(tx, ty))];
            const Vec2 candidatePosition = map.tileCenter(tx, ty);
            const Vec2 candidateOffset = candidatePosition - enemy.position;
            if (candidateFlowDistance >= 0 && lengthSquared(candidateOffset) > 0.0001f) {
                const Vec2 routeDirection = normalize(candidateOffset);
                int openness = 0;
                for (const FlowStep& opennessStep : FlowDirections) {
                    const int openX = tx + opennessStep.dx;
                    const int openY = ty + opennessStep.dy;
                    if (!inFlowBounds(openX, openY) ||
                        !canUseFlowStep(map, tx, ty, opennessStep) ||
                        map.isCircleBlocked(map.tileCenter(openX, openY), passageRadius)) {
                        continue;
                    }
                    ++openness;
                }

                const int flowProgress = candidateFlowDistance - startFlowDistance;
                const float failedPenalty = avoidFailedDirection
                    ? std::max(0.0f, dot(routeDirection, failedDirection)) * 28.0f
                    : 0.0f;
                const float improvingScore =
                    static_cast<float>(flowProgress) * 3.2f -
                    static_cast<float>(node.routeCost) * 0.22f +
                    static_cast<float>(openness) * 3.0f +
                    dot(routeDirection, away) * 18.0f +
                    dot(routeDirection, previousHeading) * 6.0f -
                    failedPenalty;
                if (flowProgress >= FlowOrthogonalCost && improvingScore > bestImprovingScore) {
                    bestImprovingScore = improvingScore;
                    bestImprovingIndex = node.localIndex;
                }

                const float fallbackScore =
                    static_cast<float>(flowProgress) * 0.9f +
                    static_cast<float>(node.routeCost) * 0.12f +
                    static_cast<float>(openness) * 5.0f +
                    dot(routeDirection, away) * 10.0f +
                    dot(routeDirection, previousHeading) * 4.0f -
                    failedPenalty;
                if (fallbackScore > bestFallbackScore) {
                    bestFallbackScore = fallbackScore;
                    bestFallbackIndex = node.localIndex;
                }
            }
        }

        const int localX = node.localIndex % FleeSearchDiameterTiles;
        const int localY = node.localIndex / FleeSearchDiameterTiles;
        for (const FlowStep& step : FlowDirections) {
            const int nextLocalX = localX + step.dx;
            const int nextLocalY = localY + step.dy;
            if (nextLocalX < 0 || nextLocalY < 0 ||
                nextLocalX >= FleeSearchDiameterTiles || nextLocalY >= FleeSearchDiameterTiles) {
                continue;
            }

            const int nextTileX = tx + step.dx;
            const int nextTileY = ty + step.dy;
            if (!inFlowBounds(nextTileX, nextTileY) ||
                flowDistance_[static_cast<std::size_t>(flowIndex(nextTileX, nextTileY))] < 0 ||
                !canUseFlowStep(map, tx, ty, step) ||
                map.isCircleBlocked(map.tileCenter(nextTileX, nextTileY), passageRadius)) {
                continue;
            }

            const int nextLocalIndex = localIndex(nextLocalX, nextLocalY);
            const int nextRouteCost = node.routeCost + step.cost;
            if (nextRouteCost >= routeCosts[static_cast<std::size_t>(nextLocalIndex)]) {
                continue;
            }
            routeCosts[static_cast<std::size_t>(nextLocalIndex)] = nextRouteCost;
            parents[static_cast<std::size_t>(nextLocalIndex)] = node.localIndex;
            open.push({nextRouteCost, nextLocalIndex});
        }
    }

    const int goalIndex = bestImprovingIndex >= 0 ? bestImprovingIndex : bestFallbackIndex;
    if (goalIndex < 0) {
        return false;
    }

    std::array<int, FleeSearchTileCount> reversePath;
    int pathLength = 0;
    int cursor = goalIndex;
    while (cursor != startLocalIndex && cursor >= 0 && pathLength < FleeSearchTileCount) {
        reversePath[static_cast<std::size_t>(pathLength++)] = cursor;
        cursor = parents[static_cast<std::size_t>(cursor)];
    }
    if (cursor != startLocalIndex || pathLength <= 0) {
        return false;
    }

    const float maxLookAheadDistance = static_cast<float>(balance::TileSize) * FleeWaypointLookAheadTiles;
    const auto segmentIsClear = [&](Vec2 target) {
        const Vec2 travel = target - enemy.position;
        const float travelDistance = length(travel);
        if (travelDistance <= 0.0001f) {
            return true;
        }
        const Vec2 travelDirection = travel / travelDistance;
        const float sampleSpacing = std::max(4.0f, passageRadius * 0.55f);
        const int sampleCount = std::max(1, static_cast<int>(std::ceil(travelDistance / sampleSpacing)));
        for (int sample = 1; sample <= sampleCount; ++sample) {
            const float sampleDistance = travelDistance * static_cast<float>(sample) / static_cast<float>(sampleCount);
            if (map.isCircleBlocked(enemy.position + travelDirection * sampleDistance, passageRadius)) {
                return false;
            }
        }
        return true;
    };

    const auto [firstTileX, firstTileY] = tileForLocalIndex(reversePath[static_cast<std::size_t>(pathLength - 1)]);
    outWaypoint = map.tileCenter(firstTileX, firstTileY);
    for (int pathIndex = pathLength - 2; pathIndex >= 0; --pathIndex) {
        const auto [pathTileX, pathTileY] = tileForLocalIndex(reversePath[static_cast<std::size_t>(pathIndex)]);
        const Vec2 candidateWaypoint = map.tileCenter(pathTileX, pathTileY);
        if (length(candidateWaypoint - enemy.position) > maxLookAheadDistance || !segmentIsClear(candidateWaypoint)) {
            break;
        }
        outWaypoint = candidateWaypoint;
    }
    return true;
}

Vec2 EnemySystem::updateFleeDirection(TileMap& map, Enemy& enemy, Vec2 playerPosition, float dt)
{
    EnemyFleeNavigationRuntime& navigation = enemy.fleeNavigation;
    navigation.replanTimer = std::max(0.0f, navigation.replanTimer - std::max(0.0f, dt));
    navigation.failedDirectionTimer = std::max(0.0f, navigation.failedDirectionTimer - std::max(0.0f, dt));
    if (navigation.failedDirectionTimer <= 0.0f) {
        navigation.failedDirection = {};
    }

    const Vec2 awayFromPlayer = enemy.position - playerPosition;
    if (lengthSquared(awayFromPlayer) <= 0.0001f) {
        return navigation.heading;
    }
    const Vec2 away = normalize(awayFromPlayer);
    if (lengthSquared(navigation.heading) <= 0.0001f) {
        navigation.heading = away;
    }

    const float waypointReachRadius = std::max(FleeWaypointReachRadius, enemyPassageRadius(enemy, placementCatalog_) * 0.45f);
    if (navigation.waypointActive &&
        distanceSquared(enemy.position, navigation.waypoint) <= waypointReachRadius * waypointReachRadius) {
        navigation.waypointActive = false;
        navigation.replanTimer = 0.0f;
    }
    if (navigation.waypointActive &&
        map.isCircleBlocked(navigation.waypoint, enemyPassageRadius(enemy, placementCatalog_))) {
        navigation.waypointActive = false;
        navigation.replanTimer = 0.0f;
    }
    if (navigation.waypointActive && navigation.replanTimer <= 0.0f) {
        navigation.replanTimer = FleeReplanIntervalSeconds;
    }

    if (!navigation.waypointActive && navigation.replanTimer <= 0.0f) {
        Vec2 waypoint{};
        navigation.waypointActive = planFleeWaypoint(map, enemy, playerPosition, waypoint);
        navigation.waypoint = navigation.waypointActive
            ? waypoint
            : enemy.position + away * static_cast<float>(balance::TileSize);
        navigation.replanTimer = FleeReplanIntervalSeconds;
    }

    const Vec2 toWaypoint = navigation.waypoint - enemy.position;
    const Vec2 desiredDirection = lengthSquared(toWaypoint) > 0.0001f ? normalize(toWaypoint) : away;
    const float currentAngle = std::atan2(navigation.heading.y, navigation.heading.x);
    const float desiredAngle = std::atan2(desiredDirection.y, desiredDirection.x);
    const float steeredAngle = rotateTowards(
        currentAngle,
        desiredAngle,
        std::max(0.0f, dt) * FleeTurnSpeedRadiansPerSecond);
    navigation.heading = fromAngle(steeredAngle);
    return navigation.heading;
}

void EnemySystem::updateFleeProgress(Enemy& enemy, Vec2 actualMovement, float expectedDistance, float dt)
{
    EnemyFleeNavigationRuntime& navigation = enemy.fleeNavigation;
    const float progressThreshold = std::max(0.12f, std::max(0.0f, expectedDistance) * 0.16f);
    if (expectedDistance > 0.05f && lengthSquared(actualMovement) <= progressThreshold * progressThreshold) {
        navigation.blockedSeconds += std::max(0.0f, dt);
    } else {
        navigation.blockedSeconds = 0.0f;
    }

    if (navigation.blockedSeconds < FleeBlockedThresholdSeconds) {
        return;
    }

    navigation.failedDirection = navigation.heading;
    navigation.failedDirectionTimer = FleeFailedDirectionPenaltySeconds;
    navigation.waypointActive = false;
    navigation.replanTimer = 0.0f;
    navigation.blockedSeconds = 0.0f;
}

Vec2 EnemySystem::separationFor(const Enemy& enemy) const
{
    Vec2 separation{};
    const float enemyRadius = enemyPassageRadius(enemy, placementCatalog_);
    for (std::size_t i = 0; i < enemies_.items().size(); ++i) {
        const Enemy& other = enemies_.items()[i];
        if (!other.active || &other == &enemy || other.spawnTimer > 0.0f) {
            continue;
        }

        Vec2 away = enemy.position - other.position;
        const float minDistance = enemyRadius + enemyPassageRadius(other, placementCatalog_) + SpawnAvoidancePadding;
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

    const float radius = enemyPassageRadius(enemy, placementCatalog_);
    Vec2 next = enemy.position + delta;
    if (!map.isCircleBlocked(next, radius)) {
        enemy.position = next;
        return;
    }

    next = enemy.position + Vec2{delta.x, 0.0f};
    if (!map.isCircleBlocked(next, radius)) {
        enemy.position = next;
        return;
    }
    next = enemy.position + Vec2{0.0f, delta.y};
    if (!map.isCircleBlocked(next, radius)) {
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
        if (!map.isCircleBlocked(next, radius)) {
            enemy.position = next;
            return;
        }
    }
}

Vec2 enemyAimDirection(const Enemy& enemy, Vec2 playerPosition, std::mt19937& rng)
{
    const Vec2 baseDirection = normalize(playerPosition - enemy.position);
    const double accuracy = enemy.status.attackAccuracyMultiplierFromStates();
    if (accuracy >= 0.999) {
        return baseDirection;
    }

    const float baseAngle = std::atan2(baseDirection.y, baseDirection.x);
    const float maxSpread = (1.0f - static_cast<float>(std::clamp(accuracy, 0.0, 1.0))) *
        BlindProjectileMaxSpreadDegrees * (Pi / 180.0f);
    std::uniform_real_distribution<float> spreadDist(-maxSpread, maxSpread);
    return fromAngle(baseAngle + spreadDist(rng));
}

bool fireEnemyProjectile(Enemy& enemy, ProjectileSystem& projectiles, Vec2 playerPosition, std::mt19937& rng)
{
    if (enemy.projectileId.empty() || enemy.rangedBehaviorId.empty()) {
        return false;
    }
    if (enemy.rangedBehaviorId == "wind_blow") {
        return false;
    }

    ProjectileSpawnTuning tuning;
    tuning.speedMultiplier = std::max(0.05f, enemy.projectileSpeedMultiplier);
    tuning.damageOverride = enemy.projectileDamageOverride;
    tuning.damageMultiplier = std::max(0.0, enemy.status.multiplierFor(ModifierStat::Attack));
    tuning.radiusScale = std::max(0.1f, enemy.projectileRadiusScale);
    const ProjectileSpawnMetadata metadata{.sourceActorName = enemyDisplayName(enemy)};

    const Vec2 toPlayer = enemyAimDirection(enemy, playerPosition, rng);
    const float radius = effectiveEnemyRadius(enemy);
    const Vec2 origin = enemy.position + toPlayer * (radius + 6.0f);
    bool spawned = false;
    if (isBubbleRangedBehavior(enemy.rangedBehaviorId)) {
        const int bubbleCount = std::max(1, enemy.projectileBurstCount > 1 ? enemy.projectileBurstCount : enemy.fireVolleyCount);
        const int remaining = std::clamp(
            enemy.projectileBurstRemaining > 0 ? enemy.projectileBurstRemaining : bubbleCount,
            1,
            bubbleCount);
        const int shotIndex = bubbleCount - remaining;
        const float spreadRadians = clamp(enemy.fireSpreadDegrees, 0.0f, 42.0f) * (Pi / 180.0f);
        const float baseAngle = std::atan2(toPlayer.y, toPlayer.x);
        std::uniform_real_distribution<float> spreadDist(-spreadRadians * 0.5f, spreadRadians * 0.5f);
        std::uniform_real_distribution<float> jitter(-0.018f, 0.018f);
        const Vec2 dir = fromAngle(baseAngle + spreadDist(rng) + jitter(rng));
        ProjectileSpawnTuning bubbleTuning = tuning;
        bubbleTuning.speedMultiplier *= std::uniform_real_distribution<float>(0.94f, 1.06f)(rng);
        bubbleTuning.radiusScale *= std::uniform_real_distribution<float>(0.96f, 1.04f)(rng);
        spawned = projectiles.spawn(
            "water_bubble",
            enemy.position + dir * (radius + 5.0f),
            dir,
            ProjectileOwnerType::Enemy,
            enemy.projectileEffects,
            bubbleTuning,
            metadata) || spawned;
        if (enemy.rangedBehaviorId == "shoot_bubble" || shotIndex > 0) {
            return spawned;
        }
    }
    if (enemy.rangedBehaviorId == "radial_spike") {
        const int count = std::clamp(enemy.fireVolleyCount, 1, 24);
        for (int i = 0; i < count; ++i) {
            const float angle = (static_cast<float>(i) / static_cast<float>(count)) * Pi * 2.0f;
            spawned = projectiles.spawn(
                enemy.projectileId,
                enemy.position + fromAngle(angle) * (radius + 5.0f),
                fromAngle(angle),
                ProjectileOwnerType::Enemy,
                enemy.projectileEffects,
                tuning,
                metadata) || spawned;
        }
        return spawned;
    }

    if (enemy.rangedBehaviorId == "shoot_fire" && enemy.fireVolleyCount > 1) {
        const int count = std::clamp(enemy.fireVolleyCount, 1, 8);
        const float spreadRadians = clamp(enemy.fireSpreadDegrees, 0.0f, 90.0f) * (Pi / 180.0f);
        const float start = -spreadRadians * 0.5f;
        const float step = count > 1 ? spreadRadians / static_cast<float>(count - 1) : 0.0f;
        const float baseAngle = std::atan2(toPlayer.y, toPlayer.x);
        for (int i = 0; i < count; ++i) {
            const float angle = baseAngle + start + step * static_cast<float>(i);
            const Vec2 dir = fromAngle(angle);
            spawned = projectiles.spawn(enemy.projectileId, enemy.position + dir * (radius + 6.0f), dir, ProjectileOwnerType::Enemy, enemy.projectileEffects, tuning, metadata) || spawned;
        }
        return spawned;
    }

    return projectiles.spawn(enemy.projectileId, origin, toPlayer, ProjectileOwnerType::Enemy, enemy.projectileEffects, tuning, metadata);
}

float enemyProjectileCooldownSeconds(const Enemy& enemy)
{
    return enemy.projectileInterval > 0.0f ? enemy.projectileInterval : 2.4f;
}

void clearEnemyAction(Enemy& enemy)
{
    enemy.action = {};
}

void beginEnemyAction(Enemy& enemy, std::string_view behaviorId, const EnemyActionProfile& profile)
{
    enemy.action.active = true;
    enemy.action.behaviorId = std::string(behaviorId);
    enemy.action.animationId = profile.animationId;
    enemy.action.elapsedSeconds = 0.0f;
    enemy.action.durationSeconds = std::max(0.0f, profile.durationSeconds);
    enemy.action.fireAtSeconds = clamp(profile.fireAtSeconds, 0.0f, enemy.action.durationSeconds);
    enemy.action.fired = false;
    enemy.action.lockMovement = profile.lockMovement;
    enemy.action.lockFacing = profile.lockFacing;
}

bool beginEnemyRangedAction(Enemy& enemy)
{
    const EnemyActionProfile profile = rangedActionProfileFor(enemy);
    if (!hasActionProfile(profile)) {
        return false;
    }

    beginEnemyAction(enemy, enemy.rangedBehaviorId, profile);
    return true;
}

bool beginEnemyHealAction(Enemy& enemy)
{
    const EnemyActionProfile profile = healActionProfileFor(enemy);
    if (!hasActionProfile(profile)) {
        return false;
    }

    beginEnemyAction(enemy, EnemyHealBehaviorId, profile);
    return true;
}

bool beginEnemyChestBiteAction(Enemy& enemy)
{
    const EnemyActionProfile profile = chestBiteActionProfileFor(enemy);
    if (!hasActionProfile(profile)) {
        return false;
    }

    beginEnemyAction(enemy, ChestBiteBehaviorId, profile);
    enemy.chestBiteTimer = std::max(0.2f, enemy.chestBiteIntervalSeconds);
    return true;
}

struct EnemyActionUpdateResult {
    bool fired = false;
    bool finished = false;
    float completedDurationSeconds = 0.0f;
};

EnemyActionUpdateResult updateEnemyTimedAction(
    Enemy& enemy,
    float dt,
    bool attackBlocked,
    std::string_view behaviorId)
{
    EnemyActionUpdateResult result;
    if (!enemy.action.active) {
        return result;
    }

    if (std::string_view(enemy.action.behaviorId) != behaviorId) {
        return result;
    }

    if (attackBlocked) {
        clearEnemyAction(enemy);
        return result;
    }

    const float previousElapsed = enemy.action.elapsedSeconds;
    enemy.action.elapsedSeconds = std::min(
        std::max(0.0f, enemy.action.durationSeconds),
        enemy.action.elapsedSeconds + std::max(0.0f, dt));

    if (!enemy.action.fired &&
        previousElapsed <= enemy.action.fireAtSeconds &&
        enemy.action.elapsedSeconds >= enemy.action.fireAtSeconds) {
        enemy.action.fired = true;
        result.fired = true;
    }

    if (enemy.action.elapsedSeconds >= enemy.action.durationSeconds) {
        result.completedDurationSeconds = std::max(0.0f, enemy.action.durationSeconds);
        clearEnemyAction(enemy);
        result.finished = true;
    }
    return result;
}

EnemyActionUpdateResult updateEnemyRangedAction(
    Enemy& enemy,
    TileMap& map,
    Vec2 playerPosition,
    float distanceToPlayer,
    float dt,
    bool attackBlocked)
{
    EnemyActionUpdateResult result = updateEnemyTimedAction(enemy, dt, attackBlocked, enemy.rangedBehaviorId);
    if (result.fired && !canFireEnemyProjectile(enemy, map, distanceToPlayer, playerPosition)) {
        result.fired = false;
    }
    return result;
}

EnemyActionUpdateResult updateEnemyHealAction(
    Enemy& enemy,
    ObjectPool<Enemy, balance::MaxEnemies>& enemies,
    std::vector<EnemyEvent>& events,
    float dt,
    bool attackBlocked)
{
    EnemyActionUpdateResult result = updateEnemyTimedAction(enemy, dt, attackBlocked, EnemyHealBehaviorId);

    if (result.fired) {
        if (applyEnemyHealPulse(enemy, enemies, events)) {
            result.fired = true;
        } else {
            result.fired = false;
        }
    }
    return result;
}

EnemyActionUpdateResult updateEnemyChestBiteAction(
    Enemy& enemy,
    TileMap& map,
    Vec2 playerPosition,
    float distanceToPlayer,
    float dt,
    bool attackBlocked,
    std::vector<EnemyEvent>& events,
    const EnemyPlacementCatalog* placementCatalog)
{
    EnemyActionUpdateResult result = updateEnemyTimedAction(enemy, dt, attackBlocked, ChestBiteBehaviorId);
    if (!result.fired) {
        return result;
    }

    const Vec2 toPlayer = playerPosition - enemy.position;
    if (lengthSquared(toPlayer) <= 0.0001f) {
        result.fired = false;
        return result;
    }

    const float lungeDistance = std::max(
        JumpTargetMinDistance,
        std::min(enemy.chestBiteJumpDistance, distanceToPlayer + 8.0f));
    if (beginEnemyJump(
            enemy,
            map,
            normalize(toPlayer),
            lungeDistance,
            enemy.chestBiteJumpDurationSeconds,
            enemy.chestBiteJumpArcHeight,
            placementCatalog)) {
        events.push_back(makeEnemyEvent(EnemyEventType::Attack, enemy, "chest_bite_lunge"));
        return result;
    }

    result.fired = false;
    return result;
}

bool EnemySystem::resolvePlayerOverlap(Player& player, Enemy& enemy, TileMap& map, const RuntimeBalance& balance)
{
    Vec2 fromPlayer = enemy.position - player.position;
    const float enemyRadius = enemyPassageRadius(enemy, placementCatalog_);
    const float playerRadius = effectivePlayerRadius(player, balance);
    const float minimumDistance = enemyRadius + playerRadius;
    float distSq = lengthSquared(fromPlayer);
    if (distSq >= minimumDistance * minimumDistance) {
        return false;
    }

    if (distSq <= 0.0001f) {
        fromPlayer = facingVector(enemy.facingAngle);
        distSq = 1.0f;
    }

    const float dist = std::sqrt(distSq);
    const Vec2 normal = fromPlayer / dist;
    const float overlap = minimumDistance - dist + 0.5f;

    float playerShare = PlayerPushShare;
    float enemyShare = EnemyPushShare;
    bool movedPlayer = tryMoveCircle(map, player.position, playerRadius, normal * (-overlap * playerShare));
    bool movedEnemy = tryMoveCircle(map, enemy.position, enemyRadius, normal * (overlap * enemyShare));

    if (!movedPlayer && movedEnemy) {
        tryMoveCircle(map, enemy.position, enemyRadius, normal * (overlap * playerShare));
    } else if (!movedEnemy && movedPlayer) {
        tryMoveCircle(map, player.position, playerRadius, normal * (-overlap * enemyShare));
    } else if (!movedEnemy && !movedPlayer) {
        tryMoveCircle(map, enemy.position, enemyRadius, normal * overlap);
    }

    return true;
}

void EnemySystem::update(
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
    bool allowBossCapture,
    std::string_view bossCaptureObjectId,
    const std::unordered_set<std::string>* allowedCaptureEnemyIds,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    events_.clear();
    impactSoundEvents_.clear();
    captureResults_.clear();
    if (paused) {
        return;
    }

    std::vector<SwarmSpawnRequest> pendingSwarmSpawns;

    flowTimer_ -= dt;
    if (flowTimer_ <= 0.0f || flowDistance_.empty()) {
        rebuildFlowField(map, player.position);
        flowTimer_ = 0.20f;
    }

    mudZones_.erase(
        std::remove_if(mudZones_.begin(), mudZones_.end(), [dt](MudZone& zone) {
            zone.remainingSeconds = std::max(0.0f, zone.remainingSeconds - dt);
            return zone.remainingSeconds <= 0.0f || zone.radius <= 0.0f;
        }),
        mudZones_.end());

    float mudSlowMultiplier = 1.0f;
    double mudDamagePerSecond = 0.0;
    DamageCause mudDamageCause{.source = DamageSource::Poison, .objectName = "毒の泥"};
    for (const MudZone& zone : mudZones_) {
        if (distanceSquared(player.position, zone.position) > zone.radius * zone.radius) {
            continue;
        }
        mudSlowMultiplier = std::min(mudSlowMultiplier, clamp(zone.speedMultiplier, 0.05f, 1.0f));
        const float zoneDamagePerSecond = std::max(0.0f, zone.damagePerSecond);
        if (zoneDamagePerSecond > 0.0f) {
            mudDamagePerSecond += zoneDamagePerSecond;
            mudDamageCause = zone.damageCause;
        }
    }
    if (mudSlowMultiplier < 1.0f) {
        const EntityStateApplyResult result = player.status.applyState(
            "status_slow",
            mudSlowMultiplier,
            0.25,
            "enemy:mud_zone",
            StateApplyMode::KeepLonger);
        queueStatusPopupEvent(
            statusPopupEvents_,
            player.position,
            "status_slow",
            StatusPopupTarget::Player,
            result);
    }
    if (mudDamagePerSecond > 0.0) {
        mudDamageAccumulator_ += mudDamagePerSecond * static_cast<double>(dt);
        const int mudDamage = static_cast<int>(std::floor(mudDamageAccumulator_));
        if (mudDamage > 0) {
            player.applyDamage(
                mudDamage,
                mudDamageCause);
            mudDamageAccumulator_ -= static_cast<double>(mudDamage);
        }
    } else {
        mudDamageAccumulator_ = 0.0;
    }

    const auto processEnemyDeath = [&](Enemy& enemy, std::optional<Vec2> hitOrigin = std::nullopt, bool suppressRewards = false) {
        beginEnemyDeath(enemy, spellRing, hitOrigin, suppressRewards);
    };
    const auto sleepingEnemyWakeTriggered = [&](const Enemy& enemy) {
        if (enemyHitboxOverlapsPlayer(enemy, hitboxCatalog_, player, balance, enemyVisualOffset(enemy, placementCatalog_))) {
            return true;
        }
        for (SpellRingItem* itemPtr : spellRing.runtimeItemsMutable()) {
            if (itemPtr == nullptr) {
                continue;
            }
            const SpellRingItem& item = *itemPtr;
            if (item.broken()) {
                continue;
            }
            const ObjectDefinition* hitObject = nullptr;
            if (!item.objectId.empty()) {
                const auto objectIt = objectCatalog.objectsById.find(item.objectId);
                if (objectIt != objectCatalog.objectsById.end()) {
                    hitObject = &objectIt->second;
                }
            }
            const RingItemHitboxSpec itemHitbox = ringItemHitboxSpec(
                item,
                hitObject,
                hitboxCatalog_,
                totalTime,
                ringItemExtraHitboxPadding(item));
            if (ringItemHitboxOverlapsEnemy(enemy, hitboxCatalog_, item, itemHitbox, enemyVisualOffset(enemy, placementCatalog_))) {
                return true;
            }
        }
        return false;
    };

    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active) {
            continue;
        }
        if (enemy.dungeonEventActivationLocked) {
            clearEnemyAction(enemy);
            enemy.hitFlash = 0.0f;
            enemy.hpBarTimer = 0.0f;
            enemy.awarenessIconTimer = 0.0f;
            enemy.awarenessIcon = EnemyAwarenessIcon::None;
            enemy.velocity = {};
            continue;
        }
        ensureEnemyHeldDropsInitialized(enemy, objectCatalog);
        if (enemy.death.active) {
            updateEnemyDeath(enemy, map, spellRing, dt);
            continue;
        }
        if (enemy.dungeonEventSleeping) {
            if (sleepingEnemyWakeTriggered(enemy) || !enemy.status.hasState("status_sleep")) {
                wakeDungeonEventEnemy(enemy, player.position, true);
            } else {
                clearEnemyAction(enemy);
                enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
                enemy.hpBarTimer = std::max(0.0f, enemy.hpBarTimer - dt);
                enemy.awarenessIconTimer = 0.0f;
                enemy.awarenessIcon = EnemyAwarenessIcon::None;
                enemy.velocity = {};
                continue;
            }
        }
        enemy.stunWakeTimer = std::max(0.0f, enemy.stunWakeTimer - dt);
        const bool wasStunned = enemy.status.hasState("status_stun");
        enemy.status.update(dt);
        if (wasStunned && !enemy.status.hasState("status_stun")) {
            enemy.stunWakeTimer = StunWakeHopSeconds;
        }
        if (enemy.status.hasState("status_frozen")) {
            enemy.coldExposure = 0.0f;
            enemy.coldExposureTouched = false;
        } else if (enemy.coldExposureTouched) {
            enemy.coldExposureTouched = false;
        } else {
            enemy.coldExposure = std::max(0.0f, enemy.coldExposure - ColdExposureDecayPerSecond * dt);
        }
        const double poisonDps = enemy.status.poisonDamagePerSecond();
        if (poisonDps > 0.0) {
            enemy.poisonDamageAccumulator += poisonDps * static_cast<double>(dt);
            const int poisonDamage = static_cast<int>(std::floor(enemy.poisonDamageAccumulator));
            if (poisonDamage > 0) {
                applyEnemyDamage(enemy, poisonDamage);
                enemy.poisonDamageAccumulator -= static_cast<double>(poisonDamage);
                revealEnemyHpBar(enemy, poisonDamage);
                enemy.hitFlash = 0.12f;
                events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy, {}, poisonDamage));
                if (enemy.hp <= 0) {
                    processEnemyDeath(enemy);
                    continue;
                }
            }
        } else {
            enemy.poisonDamageAccumulator = 0.0;
        }
        const double hotDps = enemy.status.hotDamagePerSecond();
        if (hotDps > 0.0) {
            enemy.hotDamageAccumulator += hotDps * static_cast<double>(dt);
            const int hotDamage = static_cast<int>(std::floor(enemy.hotDamageAccumulator));
            if (hotDamage > 0) {
                applyEnemyDamageTyped(enemy, hotDamage, "fire");
                enemy.hotDamageAccumulator -= static_cast<double>(hotDamage);
                revealEnemyHpBar(enemy, hotDamage);
                enemy.hitFlash = 0.12f;
                events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy, "status_hot", hotDamage));
                if (enemy.hp <= 0) {
                    processEnemyDeath(enemy);
                    continue;
                }
            }
        } else {
            enemy.hotDamageAccumulator = 0.0;
        }
        const double bleedDps = enemy.status.bleedDamagePerSecond();
        if (bleedDps > 0.0) {
            const bool movementStopped =
                enemy.status.hasState("status_sleep") ||
                enemy.status.hasState("status_stun") ||
                enemy.status.hasState("status_paralyze") ||
                enemy.status.hasState("status_shocked") ||
                enemy.status.hasState("status_frozen");
            const double movementScale = !movementStopped && lengthSquared(enemy.velocity) > 1.0f ? 1.5 : 0.5;
            enemy.bleedDamageAccumulator += bleedDps * movementScale * static_cast<double>(dt);
            const int bleedDamage = static_cast<int>(std::floor(enemy.bleedDamageAccumulator));
            if (bleedDamage > 0) {
                applyEnemyDamage(enemy, bleedDamage);
                enemy.bleedDamageAccumulator -= static_cast<double>(bleedDamage);
                revealEnemyHpBar(enemy, bleedDamage);
                enemy.hitFlash = 0.12f;
                events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy, "status_bleed", bleedDamage));
                if (enemy.hp <= 0) {
                    processEnemyDeath(enemy);
                    continue;
                }
            }
        } else {
            enemy.bleedDamageAccumulator = 0.0;
        }
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
        enemy.hpBarTimer = std::max(0.0f, enemy.hpBarTimer - dt);
        if (enemy.spawnTimer > 0.0f) {
            clearEnemyAction(enemy);
            enemy.spawnTimer = std::max(0.0f, enemy.spawnTimer - dt);
            if (enemy.spawnTimer <= 0.0f) {
                queueSwarmSpawn(enemy, player.position, pendingSwarmSpawns);
            }
            continue;
        }
        queueSwarmSpawn(enemy, player.position, pendingSwarmSpawns);
        if (enemy.knockbackTimer > 0.0f) {
            clearEnemyAction(enemy);
            enemy.jumpActive = false;
            clearExternalBounceState(enemy);
            moveWithCollision(enemy, map, enemy.knockbackVelocity, dt);
            enemy.knockbackTimer = std::max(0.0f, enemy.knockbackTimer - dt);
            enemy.knockbackVelocity = enemy.knockbackVelocity * std::max(0.0f, 1.0f - 6.0f * dt);
            updateEnemyAltitude(enemy);
            continue;
        }

        enemy.behaviorTimer += dt;
        updateEnemyAltitude(enemy);
        enemy.awarenessIconTimer = std::max(0.0f, enemy.awarenessIconTimer - dt);
        if (enemy.awarenessIconTimer <= 0.0f) {
            enemy.awarenessIcon = EnemyAwarenessIcon::None;
        }
        if (enemy.jumpActive && !bossActionControlsJump(enemy)) {
            clearEnemyAction(enemy);
            enemy.jumpElapsedSeconds = std::min(enemy.jumpDurationSeconds, enemy.jumpElapsedSeconds + dt);
            const float t = jumpProgress(enemy);
            const Vec2 previousPosition = enemy.position;
            enemy.position = lerp(enemy.jumpStartPosition, enemy.jumpTargetPosition, t);
            enemy.velocity = t < 1.0f && enemy.jumpDurationSeconds > 0.0f
                ? (enemy.jumpTargetPosition - enemy.jumpStartPosition) / enemy.jumpDurationSeconds
                : Vec2{};
            if (lengthSquared(enemy.position - previousPosition) > 0.0001f) {
                enemy.facingAngle = std::atan2(enemy.position.y - previousPosition.y, enemy.position.x - previousPosition.x);
            }
            updateEnemyAltitude(enemy);
            if (t >= 1.0f) {
                const bool externalBounceLanding = enemy.externalBounceActive;
                const int queuedFallDamage = externalBounceLanding
                    ? std::max(0, static_cast<int>(std::ceil(
                        static_cast<double>(enemy.externalBounceFallDamage) *
                        static_cast<double>(enemy.externalBounceFallDamageMultiplier))))
                    : 0;
                enemy.position = enemy.jumpTargetPosition;
                enemy.velocity = {};
                enemy.jumpActive = false;
                enemy.jumpElapsedSeconds = 0.0f;
                enemy.jumpDurationSeconds = 0.0f;
                enemy.jumpArcHeight = 0.0f;
                enemy.jumpLandingBuffTimer = externalBounceLanding ? 0.0f : JumpLandingBuffSeconds;
                clearExternalBounceState(enemy);
                updateEnemyAltitude(enemy);
                events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy));
                if (queuedFallDamage > 0 && enemy.hp > 0) {
                    const int adjustedFallDamage = enemy.isBoss
                        ? scaledPositiveDamage(queuedFallDamage, BossNormalIncomingDamageMultiplier)
                        : queuedFallDamage;
                    const int damageDealt = applyDefenseModifier(enemy.status, adjustedFallDamage);
                    applyEnemyDamageTyped(enemy, damageDealt, "blunt");
                    revealEnemyHpBar(enemy, damageDealt);
                    if (damageDealt > 0) {
                        enemy.hitFlash = 0.12f;
                    }
                    events_.push_back(makeEnemyEvent(
                        EnemyEventType::AttackHit,
                        enemy,
                        "fall_damage_synergy",
                        damageDealt));
                    if (enemy.hp <= 0) {
                        processEnemyDeath(enemy);
                        continue;
                    }
                }
            }
            continue;
        }

        Vec2 direction{};
        const Vec2 toPlayer = player.position - enemy.position;
        const float distanceToPlayer = length(toPlayer);
        const Vec2 directToPlayer = distanceToPlayer > 0.0001f ? toPlayer / distanceToPlayer : Vec2{};
        const float enemyRadius = enemyPassageRadius(enemy, placementCatalog_);
        const float playerRadius = effectivePlayerRadius(player, balance);
        const bool confused = enemy.status.hasState("status_confuse");
        const bool actionBlocked =
            enemy.status.hasState("status_sleep") ||
            enemy.status.hasState("status_stun") ||
            enemy.status.hasState("status_paralyze") ||
            enemy.status.hasState("status_shocked") ||
            enemy.status.hasState("status_frozen");
        bool attackBlocked = actionBlocked || confused;
        const bool bossActionControlled = updateBossActionSequence(enemy, player, map, projectiles, dt);
        if (enemy.hp <= 0) {
            processEnemyDeath(enemy);
            continue;
        }
        if (enemy.bossAction.hidden) {
            continue;
        }
        if (enemy.bossAction.phase == BossActionPhase::Submerge ||
            enemy.bossAction.phase == BossActionPhase::Stun) {
            attackBlocked = true;
        }
        if (bossActionControlled) {
            clearEnemyAction(enemy);
        }
        if (!bossActionControlled) {
        const float detectedMultiplier = std::max(1.0f, balance.enemyDetectedVisionMultiplier);
        const float unawareVisionDistance = std::max(0.0f, enemy.visionDistance);
        const float unawareVisionAngle = std::max(0.0f, enemy.visionAngle);
        const float detectedVisionDistance = unawareVisionDistance * detectedMultiplier;
        const float detectedVisionAngle = std::min(360.0f, unawareVisionAngle * detectedMultiplier);
        const Vec2 facing = facingVector(enemy.facingAngle);
        const auto canDetectPlayer = [&](float visionDistance, float visionAngle) {
            if (distanceToPlayer > visionDistance) {
                return false;
            }
            if (angleBetweenDegrees(facing, toPlayer) > visionAngle * 0.5f) {
                return false;
            }
            return hasClearSightLine(map, enemy.position, player.position);
        };
        const auto isAmbushTriggerDistance = [&]() {
            const float triggerDistance = enemyRadius + playerRadius + 18.0f;
            return distanceToPlayer <= triggerDistance;
        };
        const auto alertNearbySwarm = [&](const Enemy& source) {
            const float radiusSq = SwarmAlertRadius * SwarmAlertRadius;
            for (Enemy& ally : enemies_.items()) {
                if (!ally.active || ally.death.active || ally.id == source.id || ally.spawnTimer > 0.0f) {
                    continue;
                }
                if (ally.enemyId != source.enemyId || ally.awareness != EnemyAwarenessState::Unaware) {
                    continue;
                }
                if (distanceSquared(ally.position, source.position) > radiusSq) {
                    continue;
                }
                forceDetectInSight(ally, player.position, true);
            }
        };

        if (enemy.awareness == EnemyAwarenessState::Unaware) {
            const bool proximityTriggeredAmbush =
                enemy.aiId == "ambush" &&
                isAmbushTriggerDistance();
            if (!enemy.manualDetectionOnly &&
                (canDetectPlayer(unawareVisionDistance, unawareVisionAngle) || proximityTriggeredAmbush)) {
                setAwareness(enemy, EnemyAwarenessState::Detected, true);
                if (hasBehavior(enemy, "swarm_alert")) {
                    alertNearbySwarm(enemy);
                }
            }
        } else if (canDetectPlayer(detectedVisionDistance, detectedVisionAngle)) {
            enemy.loseSightTimer = 0.0f;
        } else {
            enemy.loseSightTimer += dt;
            if (enemy.loseSightTimer >= std::max(0.0f, enemy.loseSightSeconds)) {
                setAwareness(enemy, EnemyAwarenessState::Unaware, true);
            }
        }

        enemy.aiDecisionTimer = std::max(0.0f, enemy.aiDecisionTimer - dt);
        enemy.aiDigTimer = std::max(0.0f, enemy.aiDigTimer - dt);
        enemy.chestBiteTimer = std::max(0.0f, enemy.chestBiteTimer - dt);
        const std::string_view aiId = enemy.awareness == EnemyAwarenessState::Detected
            ? (enemy.aiId.empty() ? std::string_view("chase") : std::string_view(enemy.aiId))
            : (enemy.unawareAiId.empty() ? std::string_view("idle") : std::string_view(enemy.unawareAiId));
        const RangedEngagementRange rangedRange = rangedEngagementRange(enemy.rangedBehaviorId);
        const bool usesRangedEngagementRange =
            enemy.awareness == EnemyAwarenessState::Detected &&
            hasRangedEngagementRange(rangedRange);
        const auto fireRangedAttack = [&](Enemy& firingEnemy) {
            if (firingEnemy.rangedBehaviorId == "wind_blow") {
                if (windPulses_.size() >= MaxEnemyWindPulses) {
                    windPulses_.erase(windPulses_.begin());
                }
                EnemyWindPulse pulse = makeEnemyWindPulse(firingEnemy, player.position);
                EnemyEvent event = makeEnemyEventAt(EnemyEventType::Shoot, firingEnemy, pulse.center, "wind_blow");
                event.effectRadius = pulse.radius;
                windPulses_.push_back(std::move(pulse));
                events_.push_back(std::move(event));
                return true;
            }
            if (fireEnemyProjectile(firingEnemy, projectiles, player.position, rng_)) {
                events_.push_back(makeEnemyEvent(EnemyEventType::Shoot, firingEnemy));
                return true;
            }
            return false;
        };
        const EnemyActionUpdateResult rangedActionResult = updateEnemyRangedAction(
            enemy,
            map,
            player.position,
            distanceToPlayer,
            dt,
            attackBlocked);
        if (rangedActionResult.fired) {
            fireRangedAttack(enemy);
        }
        if (rangedActionResult.finished) {
            enemy.projectileTimer = std::max(
                0.0f,
                enemyProjectileCooldownSeconds(enemy) - rangedActionResult.completedDurationSeconds);
        }
        const EnemyActionUpdateResult healActionResult = updateEnemyHealAction(
            enemy,
            enemies_,
            events_,
            dt,
            attackBlocked);
        if (healActionResult.finished) {
            enemy.enemyHealTimer = std::max(
                0.0f,
                enemy.enemyHealIntervalSeconds - healActionResult.completedDurationSeconds);
        }
        updateEnemyChestBiteAction(
            enemy,
            map,
            player.position,
            distanceToPlayer,
            dt,
            attackBlocked,
            events_,
            placementCatalog_);
        if (!enemy.action.active &&
            !enemy.jumpActive &&
            !attackBlocked &&
            enemy.awareness == EnemyAwarenessState::Detected &&
            hasBehavior(enemy, ChestBiteBehaviorId) &&
            enemy.chestBiteTimer <= 0.0f &&
            distanceToPlayer > enemyRadius + playerRadius + 2.0f &&
            distanceToPlayer <= std::max(enemyRadius + playerRadius + 8.0f, enemy.chestBiteTriggerRange) &&
            lengthSquared(directToPlayer) > 0.0001f &&
            hasClearSightLine(map, enemy.position, player.position)) {
            enemy.facingAngle = std::atan2(directToPlayer.y, directToPlayer.x);
            beginEnemyChestBiteAction(enemy);
        }
        const bool actionLocksMovement = enemy.action.active && enemy.action.lockMovement;
        std::optional<Vec2> stealDropTargetPosition;
        if (enemy.stealItemEnabled &&
            enemyHeldDropCount(enemy) < std::max(1, enemy.stealMaxCarry) &&
            enemyCanStealInView(enemy, stealViewBounds, placementCatalog_)) {
            const WorldDropItem* targetDrop = nearestStealableDropForEnemy(
                enemy,
                objectCatalog,
                worldDrops,
                stealViewBounds);
            if (targetDrop != nullptr) {
                stealDropTargetPosition = targetDrop->position;
            }
            if (targetDrop != nullptr &&
                distanceSquared(enemy.position, targetDrop->position) <= enemy.stealRadius * enemy.stealRadius) {
                WorldDropItem stolenDrop;
                const bool removed = worldDrops.stealNearestDrop(
                    objectCatalog,
                    enemy.position,
                    std::max(8.0f, enemy.stealRadius),
                    enemy.stealTarget,
                    stolenDrop,
                    &stealViewBounds);
                bool stolen = false;
                EnemyHeldDrop heldDrop;
                if (removed && stolenDrop.kind == WorldDropKind::Money) {
                    heldDrop = EnemyHeldDrop{
                        .kind = EnemyHeldDropKind::Money,
                        .origin = EnemyHeldDropOrigin::PickedUp,
                        .quantity = std::max(0, stolenDrop.quantity),
                        .deathDropChance = 1.0f,
                    };
                    stolen = addHeldDropToEnemy(enemy, heldDrop);
                } else if (removed && stolenDrop.kind == WorldDropKind::Object && !stolenDrop.id.empty()) {
                    heldDrop = EnemyHeldDrop{
                        .kind = EnemyHeldDropKind::Object,
                        .origin = EnemyHeldDropOrigin::PickedUp,
                        .objectId = stolenDrop.id,
                        .quantity = 1,
                        .deathDropChance = 1.0f,
                        .instance = std::move(stolenDrop.instance),
                        .runtimeItem = std::move(stolenDrop.runtimeItem),
                    };
                    stolen = addHeldDropToEnemy(enemy, heldDrop);
                }
                if (stolen) {
                    events_.push_back(makeEnemyStealEvent(enemy, heldDrop));
                    soundEvents_.push_back(EnemySoundEvent{
                        .cueId = std::string(AudioSeRatSteal),
                        .position = enemy.position,
                    });
                    setAwareness(enemy, EnemyAwarenessState::Detected, false);
                    stealDropTargetPosition.reset();
                }
            }
        }
        const auto chooseWanderDirection = [&]() {
            if (enemy.aiDecisionTimer <= 0.0f || lengthSquared(enemy.aiMoveDirection) <= 0.0001f) {
                enemy.aiMoveDirection = randomDirection(rng_);
                std::uniform_real_distribution<float> retarget(WanderRetargetMin, WanderRetargetMax);
                enemy.aiDecisionTimer = retarget(rng_);
            }
            return enemy.aiMoveDirection;
        };
        const auto chooseFleeDirection = [&]() {
            return updateFleeDirection(map, enemy, player.position, dt);
        };
        const bool stealEscaping = enemy.stealItemEnabled && enemyHeldDropCount(enemy) > 0;
        const bool fleeNavigating = !confused && (stealEscaping || aiId == "flee");
        if (!fleeNavigating) {
            enemy.fleeNavigation = {};
        }
        if (stealEscaping) {
            direction = fleeNavigating ? chooseFleeDirection() : Vec2{};
        } else if (enemy.stealItemEnabled && stealDropTargetPosition) {
            direction = normalize(*stealDropTargetPosition - enemy.position);
        } else if (enemy.stealItemEnabled && enemy.awareness == EnemyAwarenessState::Detected) {
            direction = flowDirectionFor(map, enemy.position, player.position);
        } else if (enemy.stealItemEnabled) {
            direction = chooseWanderDirection();
        } else if (aiId == "idle" || aiId == "stationary") {
            direction = {};
        } else if (aiId == "buried") {
            direction = {};
        } else if (aiId == "flee") {
            direction = fleeNavigating ? chooseFleeDirection() : Vec2{};
        } else if (aiId == "wander") {
            direction = chooseWanderDirection();
        } else if (aiId == "patrol") {
            if (!enemy.patrolAnchorInitialized) {
                enemy.patrolAnchor = enemy.position;
                enemy.patrolAnchorInitialized = true;
            }
            const Vec2 toAnchor = enemy.patrolAnchor - enemy.position;
            if (length(toAnchor) > PatrolRadius) {
                direction = normalize(toAnchor);
            } else {
                if (enemy.aiDecisionTimer <= 0.0f || lengthSquared(enemy.aiMoveDirection) <= 0.0001f) {
                    enemy.aiMoveDirection = randomDirection(rng_);
                    std::uniform_real_distribution<float> retarget(PatrolRetargetMin, PatrolRetargetMax);
                    enemy.aiDecisionTimer = retarget(rng_);
                }
                direction = enemy.aiMoveDirection;
            }
        } else if (aiId == "item_seek") {
            const WorldDropItem* nearestDrop = nullptr;
            float nearestDistanceSq = ItemSeekRadius * ItemSeekRadius;
            for (const WorldDropItem& drop : worldDrops.drops()) {
                const float d2 = distanceSquared(enemy.position, drop.position);
                if (d2 >= nearestDistanceSq) {
                    continue;
                }
                nearestDistanceSq = d2;
                nearestDrop = &drop;
            }
            if (nearestDrop != nullptr) {
                direction = normalize(nearestDrop->position - enemy.position);
            } else {
                direction = chooseWanderDirection();
            }
        } else if (aiId == "phase_wander") {
            direction = chooseWanderDirection();
        } else if (aiId == "dig_wander") {
            const int centerTx = map.worldToTile(enemy.position.x);
            const int centerTy = map.worldToTile(enemy.position.y);
            Vec2 wallDirection{};
            float bestWallDistance = 1.0e9f;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    const int tx = centerTx + dx;
                    const int ty = centerTy + dy;
                    if (!map.isTileSolid(tx, ty)) {
                        continue;
                    }
                    const Vec2 target = map.tileCenter(tx, ty) - enemy.position;
                    const float d2 = lengthSquared(target);
                    if (d2 >= bestWallDistance || d2 <= 0.0001f) {
                        continue;
                    }
                    wallDirection = normalize(target);
                    bestWallDistance = d2;
                }
            }
            direction = lengthSquared(wallDirection) > 0.0001f ? wallDirection : chooseWanderDirection();
        } else if (aiId == "support") {
            const Enemy* woundedAlly = nullptr;
            float woundedDistanceSq = 260.0f * 260.0f;
            for (const Enemy& ally : enemies_.items()) {
                if (!ally.active || ally.death.active || ally.id == enemy.id || ally.hp >= ally.maxHp) {
                    continue;
                }
                const float d2 = distanceSquared(enemy.position, ally.position);
                if (d2 >= woundedDistanceSq) {
                    continue;
                }
                woundedDistanceSq = d2;
                woundedAlly = &ally;
            }
            if (woundedAlly != nullptr) {
                direction = normalize(woundedAlly->position - enemy.position);
            } else if (distanceToPlayer < KeepDistanceMin) {
                direction = directToPlayer * -1.0f;
            } else {
                direction = {};
            }
        } else if (aiId == "shield_chase") {
            direction = directToPlayer;
        } else if (aiId == "dig_chase") {
            direction = directToPlayer;
        } else if (aiId == "phase_chase") {
            direction = directToPlayer;
        } else if (aiId == "burrow_ambush" || aiId == "ambush") {
            direction = directToPlayer;
        } else if (aiId == "keep_distance" || aiId == "hover_keep_distance") {
            const float keepMinDistance = usesRangedEngagementRange ? rangedRange.minDistance : KeepDistanceMin;
            const float keepMaxDistance = usesRangedEngagementRange ? rangedRange.maxDistance : KeepDistanceMax;
            if (distanceToPlayer < keepMinDistance) {
                direction = directToPlayer * -1.0f;
            } else if (distanceToPlayer > keepMaxDistance) {
                direction = flowDirectionFor(map, enemy.position, player.position);
            }
        } else {
            direction = flowDirectionFor(map, enemy.position, player.position);
        }
        if (confused) {
            if (enemy.aiDecisionTimer <= 0.0f || lengthSquared(enemy.aiMoveDirection) <= 0.0001f) {
                enemy.aiMoveDirection = randomDirection(rng_);
                std::uniform_real_distribution<float> retarget(ConfusedRetargetMinSeconds, ConfusedRetargetMaxSeconds);
                enemy.aiDecisionTimer = retarget(rng_);
            }
            direction = enemy.aiMoveDirection;
        }
        if (enemy.movementLeashEnabled && enemy.movementLeashRadius > 0.0f) {
            const Vec2 toLeashCenter = enemy.movementLeashCenter - enemy.position;
            const float leashDistance = length(toLeashCenter);
            const float steerDistance = enemy.movementLeashRadius * MovementLeashSteerThreshold;
            if (leashDistance > steerDistance && leashDistance > 0.0001f) {
                const Vec2 leashDirection = toLeashCenter / leashDistance;
                if (lengthSquared(direction) <= 0.0001f || dot(normalize(direction), leashDirection) < 0.0f) {
                    direction = leashDirection;
                } else {
                    const float leashWeight = leashDistance > enemy.movementLeashRadius ? 2.0f : 1.25f;
                    direction = normalize(direction + leashDirection * leashWeight);
                }
            }
        }
        if (actionLocksMovement) {
            direction = {};
        }
        double baseSpeed = balance.enemySpeed;
        if (enemy.definition != nullptr && enemy.definition->moveSpeed > 0.0 && std::isfinite(enemy.definition->moveSpeed)) {
            baseSpeed = enemy.definition->moveSpeed;
        }
        if (enemy.enemyId == "mana_leech") {
            baseSpeed *= 0.72;
        }
        if (hasBehavior(enemy, "light_slow") && map.isLit(enemy.position, playerLight, extraLights)) {
            baseSpeed *= clamp(enemy.lightSpeedMultiplier, 0.05f, 1.0f);
        }
        if (aiId == "jump_chase") {
            const float phase = std::fmod(enemy.behaviorTimer, 1.05f);
            baseSpeed *= phase < 0.25f ? 1.65 : 0.70;
        }
        if (aiId == "support") {
            baseSpeed *= 0.85;
        }
        if (confused) {
            baseSpeed *= ConfusedSpeedMultiplier;
        }
        const float enemySpeed = static_cast<float>(
            enemy.status.applyModifiers(ModifierStat::Speed, baseSpeed) *
            enemy.status.movementMultiplierFromStates() *
            (enemy.isBoss ? 0.78 : 1.0));
        if (!actionLocksMovement && aiId == "shield_chase" && lengthSquared(directToPlayer) > 0.0001f) {
            const float targetAngle = std::atan2(directToPlayer.y, directToPlayer.x);
            enemy.facingAngle = rotateTowards(enemy.facingAngle, targetAngle, dt * 1.8f);
            direction = facingVector(enemy.facingAngle);
        }

        enemy.velocity = lengthSquared(direction) > 0.0001f ? normalize(direction) * enemySpeed : Vec2{};
        if (!actionLocksMovement && !actionBlocked && aiId != "stationary" && aiId != "idle" && aiId != "buried") {
            enemy.velocity += separationFor(enemy) * balance.enemySeparationStrength;
        }
        enemy.jumpLandingBuffTimer = std::max(0.0f, enemy.jumpLandingBuffTimer - dt);
        if (!attackBlocked && !enemy.action.active && !enemy.jumpActive && hasBehavior(enemy, "jump_attack") && enemy.awareness == EnemyAwarenessState::Detected) {
            enemy.jumpAttackTimer = std::max(0.0f, enemy.jumpAttackTimer - dt);
            if (enemy.jumpAttackTimer <= 0.0f && lengthSquared(directToPlayer) > 0.0001f) {
                beginEnemyJump(
                    enemy,
                    map,
                    directToPlayer,
                    std::max(JumpTargetMinDistance, enemy.jumpAttackDistance),
                    enemy.jumpAttackDurationSeconds,
                    enemy.jumpAttackArcHeight,
                    placementCatalog_);
                enemy.jumpAttackTimer = std::max(0.2f, enemy.jumpAttackIntervalSeconds);
            }
        }
        const float maxSpeed = enemySpeed * 1.75f;
        if (lengthSquared(enemy.velocity) > maxSpeed * maxSpeed) {
            enemy.velocity = normalize(enemy.velocity) * maxSpeed;
        }
        const float expectedMovementDistance = length(enemy.velocity) * std::max(0.0f, dt);
        const bool ignoresWallCollision = !confused && (aiId == "phase_wander" || aiId == "phase_chase");
        const bool digsThroughWall = !confused && (aiId == "dig_chase" || aiId == "dig_wander") && hasBehavior(enemy, "dig_move");
        const Vec2 previousPosition = enemy.position;
        if (ignoresWallCollision) {
            enemy.position += enemy.velocity * dt;
        } else if (digsThroughWall) {
            moveWithCollision(enemy, map, enemy.velocity, dt);
            if (distanceSquared(enemy.position, previousPosition) <= 0.0004f &&
                enemy.aiDigTimer <= 0.0f &&
                lengthSquared(enemy.velocity) > 0.0001f) {
                const Vec2 digDirection = normalize(enemy.velocity);
                const Vec2 ahead = enemy.position + digDirection * (enemyRadius + static_cast<float>(balance::TileSize) * 0.6f);
                const int tx = map.worldToTile(ahead.x);
                const int ty = map.worldToTile(ahead.y);
                const Vec2 tileCenter = map.tileCenter(tx, ty);
                const TerrainDebugInfo terrain = map.terrainDebugAtWorld(tileCenter);
                if (terrain.type != TileType::Empty) {
                    const Color tileColor = map.tileColorAtTile(tx, ty);
                    events_.push_back(makeTerrainEnemyEvent(
                        EnemyEventType::TerrainHit,
                        enemy,
                        tileCenter,
                        tileCenter - enemy.position,
                        terrain.type,
                        tileColor));

                    Vec2 opened{};
                    TileType openedType = terrain.type;
                    if (map.damageTile(tx, ty, std::max(1, enemy.digMovePower), opened, &openedType)) {
                        events_.push_back(makeTerrainEnemyEvent(
                            EnemyEventType::TerrainBreak,
                            enemy,
                            opened,
                            digDirection,
                            openedType,
                            tileColor));
                    }
                }
                enemy.aiDigTimer = std::max(0.02f, enemy.digMoveIntervalSeconds);
            }
        } else {
            moveWithCollision(enemy, map, enemy.velocity, dt);
        }
        if (enemy.movementLeashEnabled && enemy.movementLeashRadius > 0.0f) {
            const Vec2 fromLeashCenter = enemy.position - enemy.movementLeashCenter;
            const float leashDistance = length(fromLeashCenter);
            if (leashDistance > enemy.movementLeashRadius && leashDistance > 0.0001f) {
                enemy.position = enemy.movementLeashCenter + fromLeashCenter / leashDistance * enemy.movementLeashRadius;
                enemy.velocity = {};
                enemy.aiDecisionTimer = 0.0f;
            }
        }

        const Vec2 actualMovement = enemy.position - previousPosition;
        if (!actionLocksMovement && fleeNavigating) {
            updateFleeProgress(enemy, actualMovement, expectedMovementDistance, dt);
        }

        if (!actionLocksMovement && !fleeNavigating &&
            (confused ||
                aiId == "wander" ||
                aiId == "patrol" ||
                aiId == "item_seek" ||
                aiId == "dig_wander") &&
            distanceSquared(enemy.position, previousPosition) <= 0.0004f) {
            enemy.aiDecisionTimer = 0.0f;
            enemy.aiMoveDirection = randomDirection(rng_);
        }

        const bool actionLocksFacing = enemy.action.active && enemy.action.lockFacing;
        if (!actionLocksFacing && fleeNavigating && lengthSquared(actualMovement) > 0.0001f) {
            enemy.facingAngle = std::atan2(actualMovement.y, actualMovement.x);
        } else if (!actionLocksFacing && !fleeNavigating && !confused && enemy.awareness == EnemyAwarenessState::Detected && lengthSquared(toPlayer) > 0.0001f && aiId != "shield_chase") {
            enemy.facingAngle = std::atan2(toPlayer.y, toPlayer.x);
        } else if (!actionLocksFacing && !fleeNavigating && (confused || (aiId != "stationary" && aiId != "idle" && aiId != "buried")) && lengthSquared(enemy.velocity) > 0.0001f) {
            enemy.facingAngle = std::atan2(enemy.velocity.y, enemy.velocity.x);
        }
        if (!enemy.action.active &&
            !rangedActionResult.finished &&
            !attackBlocked &&
            canFireEnemyProjectile(enemy, map, distanceToPlayer, player.position)) {
            enemy.projectileTimer = std::max(0.0f, enemy.projectileTimer - dt);
            if (enemy.projectileTimer <= 0.0f) {
                if (!beginEnemyRangedAction(enemy)) {
                    const bool bubbleBurst = isBubbleRangedBehavior(enemy.rangedBehaviorId) && enemy.projectileBurstCount > 1;
                    if (bubbleBurst && enemy.projectileBurstRemaining <= 0) {
                        enemy.projectileBurstRemaining = enemy.projectileBurstCount;
                    }
                    fireRangedAttack(enemy);
                    if (bubbleBurst) {
                        if (enemy.projectileBurstRemaining > 1) {
                            --enemy.projectileBurstRemaining;
                            enemy.projectileTimer = std::max(0.02f, enemy.projectileBurstInterval);
                        } else {
                            enemy.projectileBurstRemaining = 0;
                            enemy.projectileTimer = enemyProjectileCooldownSeconds(enemy);
                        }
                    } else if (enemy.projectileBurstCount > 1 && enemy.rangedBehaviorId == "shoot_water") {
                        if (enemy.projectileBurstRemaining <= 1) {
                            enemy.projectileBurstRemaining = enemy.projectileBurstCount;
                            enemy.projectileTimer = enemyProjectileCooldownSeconds(enemy);
                        } else {
                            --enemy.projectileBurstRemaining;
                            enemy.projectileTimer = std::max(0.02f, enemy.projectileBurstInterval);
                        }
                    } else {
                        enemy.projectileTimer = enemyProjectileCooldownSeconds(enemy);
                    }
                }
            }
        }

        if (hasBehavior(enemy, "magnet_disturb") &&
            enemy.magnetStrength > 0.0f &&
            enemy.magnetRadius > 0.0f) {
            const float radius = std::max(8.0f, enemy.magnetRadius);
            const float strengthDt = dt * enemy.magnetStrength;
            const bool affectMetal = enemy.magnetTargetTag.empty() || pipeListContains(enemy.magnetTargetTag, "metal");
            if (affectMetal) {
                worldDrops.pullMetalDrops(objectCatalog, enemy.position, strengthDt, radius);
                pullMetalEnemies(enemy.position, map, strengthDt, radius);
                projectiles.pullMetalProjectiles(enemy.position, strengthDt, radius);
            }
        }

        if (!attackBlocked &&
            !enemy.action.active &&
            hasBehavior(enemy, "enemy_heal") &&
            enemy.enemyHealRadius > 0.0f &&
            enemy.enemyHealAmount > 0.0f &&
            enemy.enemyHealIntervalSeconds > 0.0f) {
            enemy.enemyHealTimer = std::max(0.0f, enemy.enemyHealTimer - dt);
            if (enemy.enemyHealTimer <= 0.0f) {
                Enemy* healTarget = nearestWoundedEnemyForHeal(enemy, enemies_);
                if (healTarget == nullptr) {
                    enemy.enemyHealTimer = enemy.enemyHealIntervalSeconds;
                } else {
                    const Vec2 toTarget = healTarget->position - enemy.position;
                    if (lengthSquared(toTarget) > 0.0001f) {
                        enemy.facingAngle = std::atan2(toTarget.y, toTarget.x);
                    }
                    if (!beginEnemyHealAction(enemy)) {
                        if (applyEnemyHealPulse(enemy, enemies_, events_)) {
                            enemy.enemyHealTimer = enemy.enemyHealIntervalSeconds;
                        } else {
                            enemy.enemyHealTimer = std::min(0.25f, enemy.enemyHealIntervalSeconds);
                        }
                    }
                }
            }
        }

        if (!attackBlocked && hasBehavior(enemy, "countdown_explode") && (!enemy.countdownExploded || !enemy.countdownExplodeOnce)) {
            if (!enemy.countdownExplodeArmed && canArmCountdownExplosion(enemy, player.position)) {
                enemy.countdownExplodeArmed = true;
                enemy.countdownExplodeDelay = std::max(0.0f, enemy.countdownExplodeInitialDelay);
                enemy.countdownExplodeWarningTickIndex = -1;
            }

            if (enemy.countdownExplodeArmed) {
                enemy.countdownExplodeDelay = std::max(0.0f, enemy.countdownExplodeDelay - dt);
                if (enemy.countdownExplodeDelay > 0.0f) {
                    const int tickIndex = countdownExplosionWarningTickIndex(enemy);
                    if (tickIndex > enemy.countdownExplodeWarningTickIndex) {
                        enemy.countdownExplodeWarningTickIndex = tickIndex;
                        events_.push_back(makeEnemyEvent(EnemyEventType::ExplosionWarningTick, enemy));
                    }
                }
                if (enemy.countdownExplodeDelay <= 0.0f) {
                    const float radius = std::max(8.0f, enemy.countdownExplodeRadius * ExplosionRadiusScale);
                    const float playerExplosionRadius = radius + playerRadius;
                    if (enemy.countdownExplodeDamage > 0 &&
                        distanceSquared(player.position, enemy.position) <= playerExplosionRadius * playerExplosionRadius) {
                        player.applyDamage(
                            applyDefenseModifier(player.status, enemy.countdownExplodeDamage),
                            DamageCause{
                                .source = DamageSource::Explosion,
                                .actorName = enemyDisplayName(enemy),
                                .objectName = "爆発",
                            });
                        player.applyKnockback(
                            player.position - enemy.position,
                            std::clamp(132.0f + radius * 1.20f, 150.0f, 250.0f),
                            0.16f);
                    }
                    if (enemy.countdownExplodeTerrainDamage > 0) {
                        map.destroyCircle(enemy.position, radius);
                    }
                    enemy.countdownExploded = true;
                    EnemyEvent explodeEvent = makeEnemyEvent(EnemyEventType::Explode, enemy);
                    explodeEvent.effectRadius = radius;
                    explodeEvent.damageAmount = enemy.countdownExplodeDamage;
                    events_.push_back(std::move(explodeEvent));
                    applyExplosionDamage(enemy.position, radius, spellRing, enemy.countdownExplodeDamage, enemy.id);
                    processEnemyDeath(enemy, std::nullopt, true);
                    continue;
                }
            }
        }
        }

        enemy.contactTimer = std::max(0.0f, enemy.contactTimer - dt);
        const bool contactEnabled =
            !enemy.bossAction.hidden &&
            enemy.bossAction.phase != BossActionPhase::Submerge &&
            enemy.bossAction.phase != BossActionPhase::Stun &&
            !isAstragnaBossAction(enemy);
        const Vec2 enemyHitboxOffset = enemyVisualOffset(enemy, placementCatalog_);
        const bool touchedPlayerBeforeResolve = contactEnabled &&
            enemyHitboxOverlapsPlayer(enemy, hitboxCatalog_, player, balance, enemyHitboxOffset);
        if (contactEnabled) {
            resolvePlayerOverlap(player, enemy, map, balance);
        }
        bool touchedPlayer = contactEnabled &&
            (touchedPlayerBeforeResolve || enemyHitboxOverlapsPlayer(enemy, hitboxCatalog_, player, balance, enemyHitboxOffset));
        if (!touchedPlayer &&
            enemy.jumpLandingBuffTimer > 0.0f &&
            enemy.jumpLandingRadius > 0.0f &&
            distanceSquared(player.position, enemy.position) <= enemy.jumpLandingRadius * enemy.jumpLandingRadius) {
            touchedPlayer = true;
        }
        if (touchedPlayer && enemy.status.hasState("status_shocked")) {
            const EntityStateApplyResult result = player.status.applyState(
                "status_shocked",
                1.0,
                ShockedContactTransferDurationSeconds,
                "enemy:status_shocked:" + enemy.enemyId,
                StateApplyMode::KeepLonger);
            queueStatusPopupEvent(
                statusPopupEvents_,
                player.position,
                "status_shocked",
                StatusPopupTarget::Player,
                result);
        }
        if (!attackBlocked && touchedPlayer && enemy.contactTimer <= 0.0f) {
            bool attackHit = true;
            bool ringSlowBiteApplied = false;
            const double accuracy = enemy.status.attackAccuracyMultiplierFromStates();
            if (accuracy < 0.999) {
                std::uniform_real_distribution<double> accuracyDist(0.0, 1.0);
                attackHit = accuracyDist(rng_) <= std::clamp(accuracy, 0.0, 1.0);
            }
            if (attackHit) {
                const double baseAttackPower = static_cast<double>(enemy.contactAttackPower) * (enemy.isBoss ? 2.0 : 1.0);
                const double modifiedAttackPower = enemy.status.applyModifiers(ModifierStat::Attack, baseAttackPower);
                int contactDamage = enemy.isBoss
                    ? std::max(1, static_cast<int>(std::ceil(std::max(0.0, modifiedAttackPower) * damageTypeMultiplier(enemy.contactDamageType))))
                    : std::max(0, static_cast<int>(std::ceil(std::max(0.0, modifiedAttackPower) * damageTypeMultiplier(enemy.contactDamageType))));
                float contactMultiplier = std::max(0.0f, enemy.contactDamageMultiplier);
                if (enemy.jumpLandingBuffTimer > 0.0f) {
                    contactMultiplier *= std::max(1.0f, enemy.jumpLandingDamageMultiplier);
                }
                contactMultiplier *= bossActionContactDamageMultiplier(enemy);
                contactDamage = std::max(0, static_cast<int>(std::ceil(static_cast<double>(contactDamage) * contactMultiplier)));
                player.applyDamage(
                    applyDefenseModifier(player.status, contactDamage),
                    DamageCause{
                        .source = enemy.isBoss ? DamageSource::SlimeAttack : DamageSource::SlimeContact,
                        .actorName = enemyDisplayName(enemy),
                    });
                if (hasBehavior(enemy, "rust_touch")) {
                    const EntityStateApplyResult rustResult = player.status.applyState(
                        "status_defense_down",
                        clamp(enemy.rustDefenseMultiplier, 0.05f, 1.0f),
                        std::max(0.1f, enemy.rustDurationSeconds),
                        "enemy:rust_touch:" + enemy.enemyId,
                        StateApplyMode::KeepLonger);
                    queueStatusPopupEvent(
                        statusPopupEvents_,
                        player.position,
                        "status_defense_down",
                        StatusPopupTarget::Player,
                        rustResult);
                }
                if (hasBehavior(enemy, "ring_slow_bite")) {
                    spellRing.applyEnemyOrbitSpeedDebuff(
                        enemy.ringSlowMultiplier > 0.0f ? enemy.ringSlowMultiplier : balance.enemyRingSlowBiteMultiplier,
                        enemy.ringSlowDurationSeconds >= 0.0f ? enemy.ringSlowDurationSeconds : balance.enemyRingSlowBiteDuration);
                    ringSlowBiteApplied = true;
                }
                if (hasBehavior(enemy, "chest_bite") && enemy.chestBiteKnockback > 0.0f) {
                    const Vec2 push = normalize(player.position - enemy.position) * enemy.chestBiteKnockback;
                    tryMoveCircle(map, player.position, playerRadius, push);
                }
                if (enemy.stealItemEnabled &&
                    enemyHeldDropCount(enemy) < std::max(1, enemy.stealMaxCarry) &&
                    (!enemyCanStealInView(enemy, stealViewBounds, placementCatalog_) ||
                        nearestStealableDropForEnemy(enemy, objectCatalog, worldDrops, stealViewBounds) == nullptr)) {
                    EnemyHeldDrop stolenLoot;
                    if (takePlayerLoot(enemy, inventory, takeMoney, rng_, stolenLoot) &&
                        addHeldDropToEnemy(enemy, stolenLoot)) {
                        events_.push_back(makeEnemyStealEvent(enemy, stolenLoot));
                        soundEvents_.push_back(EnemySoundEvent{
                            .cueId = std::string(AudioSeRatSteal),
                            .position = enemy.position,
                        });
                        setAwareness(enemy, EnemyAwarenessState::Detected, false);
                    }
                }
            }
            events_.push_back(makeEnemyEvent(
                EnemyEventType::Attack,
                enemy,
                ringSlowBiteApplied ? std::string("ring_slow_bite") : std::string{}));
            enemy.contactTimer = enemy.isBoss ? 1.0f : 0.8f;
        }

        if (!enemyCanBeHit(enemy)) {
            continue;
        }
        std::vector<SpellRingItem*> runtimeItems = spellRing.runtimeItemsMutable();
        for (SpellRingItem* itemPtr : runtimeItems) {
            if (itemPtr == nullptr) {
                continue;
            }
            SpellRingItem& item = *itemPtr;
            if (item.broken()) {
                continue;
            }
            const ObjectDefinition* hitObject = nullptr;
            if (!item.objectId.empty()) {
                const auto objectIt = objectCatalog.objectsById.find(item.objectId);
                if (objectIt != objectCatalog.objectsById.end()) {
                    hitObject = &objectIt->second;
                }
            }
            const RingItemHitboxSpec itemHitbox = ringItemHitboxSpec(
                item,
                hitObject,
                hitboxCatalog_,
                totalTime,
                ringItemExtraHitboxPadding(item));
            const CaptureNetSpec captureNetSpec = collectCaptureNetSpec(hitObject);
            const InspectEnemySpec inspectEnemySpec = collectInspectEnemySpec(hitObject);
            const bool specialContactEffect = captureNetSpec.active || inspectEnemySpec.active;
            float enemyHitInterval = item.hitInterval;
            if (captureNetSpec.active) {
                enemyHitInterval = std::max(0.05f, captureNetSpec.retryInterval);
            } else if (inspectEnemySpec.active) {
                enemyHitInterval = std::max(0.05f, inspectEnemySpec.retryInterval);
            }
            if (!specialContactEffect &&
                item.enemyHitReady(enemy.id, totalTime, enemyHitInterval) &&
                tryHitAstragnaBossComponent(
                    enemy,
                    item,
                    hitObject,
                    itemHitbox,
                    player,
                    spellRing,
                    events_,
                    impactSoundEvents_)) {
                item.recordEnemyHit(enemy.id, totalTime);
                continue;
            }
            if (!specialContactEffect &&
                item.enemyHitReady(enemy.id, totalTime, enemyHitInterval) &&
                tryHitJunkCrabDebris(
                    enemy,
                    item,
                    hitObject,
                    itemHitbox,
                    player,
                    spellRing,
                    objectCatalog,
                    hitboxCatalog_,
                    placementCatalog_,
                    events_,
                    impactSoundEvents_,
                    rng_,
                    discoveryEvents,
                    encyclopedia)) {
                item.recordEnemyHit(enemy.id, totalTime);
                continue;
            }
            if (isAstragnaBossAction(enemy)) {
                continue;
            }
            const bool overlappingItem = ringItemHitboxOverlapsEnemy(enemy, hitboxCatalog_, item, itemHitbox, enemyVisualOffset(enemy, placementCatalog_));
            if (!overlappingItem) {
                continue;
            }
            if (!item.enemyHitReady(enemy.id, totalTime, enemyHitInterval)) {
                continue;
            }
            item.recordEnemyHit(enemy.id, totalTime);
            if (captureNetSpec.active) {
                impactSoundEvents_.push_back(makeEnemyRingImpactSoundEvent(
                    item,
                    hitObject,
                    enemy,
                    RingImpactResult::Hit,
                    enemy.position,
                    0.0f));
                CaptureResult capture = tryCaptureTarget(
                    &enemy,
                    player,
                    spellRing,
                    inventory,
                    allowBossCapture,
                    bossCaptureObjectId,
                    CaptureAttemptOptions{
                        .requirePlayerReach = false,
                        .chanceMultiplier = captureNetSpec.chanceMultiplier,
                        .allowedEnemyIds = allowedCaptureEnemyIds,
                    });
                if (capture.type != CaptureResultType::NoTarget) {
                    item.actionFlashTimer = SpellRingItemActionFlashSeconds;
                    if (capture.type != CaptureResultType::KnowledgeLocked) {
                        spellRing.consumeItemDurability(item);
                    }
                    captureResults_.push_back(std::move(capture));
                }
                if (!enemy.active || enemy.hp <= 0) {
                    break;
                }
                continue;
            }
            if (inspectEnemySpec.active) {
                impactSoundEvents_.push_back(makeEnemyRingImpactSoundEvent(
                    item,
                    hitObject,
                    enemy,
                    RingImpactResult::Hit,
                    enemy.position,
                    0.0f));

                const bool alreadyInspected = encyclopedia != nullptr &&
                    encyclopedia->enemyStage(enemy.enemyId) == EncyclopediaStage::Complete;
                if (!alreadyInspected && !enemyInspectionAlreadyQueued(events_, enemy.enemyId)) {
                    item.actionFlashTimer = SpellRingItemActionFlashSeconds;
                    spellRing.consumeItemDurability(item);
                    events_.push_back(makeEnemyEvent(EnemyEventType::Inspected, enemy, "inspect_enemy", 0));
                }
                continue;
            }
            const FlameBurstHitSpec flameBurstSpec = collectFlameBurstHitSpec(hitObject);
            const BounceGroundedHitSpec bounceGroundedSpec = collectBounceGroundedHitSpec(hitObject);
            const ShockWetHitSpec shockWetSpec = collectShockWetHitSpec(hitObject);
            const RingContactDamageResult damageResult = computeRingContactDamageAgainstEnemy(
                enemy,
                item,
                hitObject,
                itemHitbox,
                player,
                spellRing,
                objectCatalog,
                hitboxCatalog_,
                placementCatalog_,
                rng_,
                discoveryEvents,
                encyclopedia,
                true);
            const std::string_view contactDamageType = damageResult.damageType;
            const bool frontGuarded = damageResult.frontGuarded;
            const bool sleepingBonusApplied = damageResult.sleepingBonusApplied;
            const bool dryWetBonusApplied = damageResult.dryWetBonusApplied;
            const bool nonlethalHit = damageResult.nonlethalHit;
            const bool criticalHit = damageResult.criticalHit;
            const std::string_view bossEffectId = damageResult.bossEffectId;
            const bool weakPointHit = damageResult.weakPointHit;
            const int damageDealt = damageResult.damageDealt;
            impactSoundEvents_.push_back(makeEnemyRingImpactSoundEvent(
                item,
                hitObject,
                enemy,
                frontGuarded ? RingImpactResult::Guard : RingImpactResult::Hit,
                enemy.position,
                static_cast<float>(std::max(0, damageDealt))));
            applyEnemyDamageTyped(enemy, damageDealt, contactDamageType);
            if (dryWetBonusApplied) {
                enemy.status.removeState("status_wet");
            }
            revealEnemyHpBar(enemy, damageDealt);
            if (damageDealt > 0) {
                item.actionFlashTimer = SpellRingItemActionFlashSeconds;
            }
            if (item.hasCapturedBehavior("heavy_guard")) {
                enemy.knockbackVelocity = normalize(enemy.position - item.worldPosition) * 90.0f;
                enemy.knockbackTimer = std::max(enemy.knockbackTimer, 0.10f);
            } else {
                spellRing.consumeItemDurability(item);
            }
            std::string hitEffectId;
            if (hitObject != nullptr) {
                hitEffectId = visualEffectIdFor(hitObject->orbitEffects);
                EffectContext context;
                context.sourceObject = hitObject;
                context.owner = &player;
                context.targetEntity = &enemy;
                context.hitTarget = &enemy;
                context.orbit = &spellRing;
                context.orbitItem = &item;
                context.enemies = this;
                context.magic = &magic;
                context.worldDrops = &worldDrops;
                context.grantMoney = grantMoney;
                context.objectCatalog = &objectCatalog;
                context.statusPopupEvents = &statusPopupEvents_;
                context.discoveryEvents = discoveryEvents;
                context.encyclopedia = encyclopedia;
                context.position = enemy.position;
                context.dropSpawnedAtSeconds = totalTime;
                context.triggerType = EffectTriggerType::Hit;
                context.logUnimplementedEffects = false;
                effectDispatcher.dispatchOrbitEffects(*hitObject, context);
                dispatchCapturedContactEffect(
                    item,
                    *hitObject,
                    enemy,
                    player,
                    spellRing,
                    effectDispatcher,
                    enemy.position,
                    discoveryEvents,
                    encyclopedia,
                    &statusPopupEvents_);
                if (shockWetSpec.active && enemy.status.hasState("status_wet")) {
                    const std::string shockSource = hitObject->id.empty()
                        ? std::string("orbit:shock_wet")
                        : "orbit:" + hitObject->id;
                    EntityStateApplyResult shockResult;
                    if (applyShockedStateToEnemy(enemy, shockWetSpec.value, shockWetSpec.duration, shockSource, &shockResult)) {
                        queueStatusPopupEvent(
                            statusPopupEvents_,
                            enemy.position,
                            "status_shocked",
                            StatusPopupTarget::Enemy,
                            shockResult);
                        if (hitEffectId.empty()) {
                            hitEffectId = "shock_wet";
                        }
                        recordObjectEffectDiscovery(discoveryEvents, *hitObject, "shock_wet", "", enemy.position);
                        if (item.conductWaterPuddleRadius > 0.0f && item.conductWaterPuddleStrength > 0.0f) {
                            const int conducted = applyConductiveShock(
                                enemy.position,
                                item.conductWaterPuddleRadius,
                                shockWetSpec.value,
                                shockWetSpec.duration,
                                enemy.id,
                                shockSource);
                            if (conducted > 0) {
                                recordObjectEffectDiscovery(discoveryEvents, *hitObject, "conduct_water_puddle", "", enemy.position);
                            }
                        }
                    }
                }
                if (flameBurstSpec.active && flameBurstSpec.damage > 0 && flameBurstSpec.radius > 0.0f) {
                    EnemyMagicHitSpec burst;
                    burst.position = enemy.position;
                    burst.radius = flameBurstSpec.radius;
                    burst.damage = flameBurstSpec.damage;
                    burst.damageType = "fire";
                    burst.effectId = "flame_burst";
                    burst.excludedRuntimeId = enemy.id;
                    applyMagicArea(burst, spellRing);
                    recordObjectEffectDiscovery(discoveryEvents, *hitObject, "flame_burst", "", enemy.position);
                }
                if (damageDealt > 0) {
                    recordObjectEffectDiscovery(discoveryEvents, *hitObject, "basic_attack", "", enemy.position);
                    if (effectSpecsContainForTarget(hitObject->orbitEffects, "item", "slash_power")) {
                        recordObjectEffectDiscovery(discoveryEvents, *hitObject, "slash_power", "", enemy.position);
                    }
                    if (sleepingBonusApplied) {
                        recordObjectEffectDiscovery(discoveryEvents, *hitObject, "sleeping_bonus_damage", "", enemy.position);
                    }
                    if (dryWetBonusApplied) {
                        recordObjectEffectDiscovery(discoveryEvents, *hitObject, "dry_wet_bonus_damage", "", enemy.position);
                    }
                    if (nonlethalHit) {
                        recordObjectEffectDiscovery(discoveryEvents, *hitObject, "nonlethal_hit", "", enemy.position);
                    }
                }
                if (enemy.hp > 0 && beginExternalGroundBounce(enemy, map, item.worldPosition, bounceGroundedSpec, placementCatalog_)) {
                    if (hitEffectId.empty()) {
                        hitEffectId = "bounce_grounded";
                    }
                    recordObjectEffectDiscovery(discoveryEvents, *hitObject, "bounce_grounded", "", enemy.position);
                    if (bounceGroundedSpec.fallDamageActive) {
                        recordObjectEffectDiscovery(discoveryEvents, *hitObject, "fall_damage_synergy", "", enemy.position);
                    }
                }
            }
            if (hitEffectId.empty()) {
                hitEffectId = visualEffectIdFor(item.addedEffects);
            }
            if (!bossEffectId.empty()) {
                hitEffectId = std::string(bossEffectId);
            }
            if (hitEffectId.empty() && !contactDamageType.empty() && contactDamageType != "none") {
                hitEffectId = std::string(contactDamageType);
            }
            if (item.hasCapturedBehavior("steal_or_dig") && capturedRewardAllowed(item, enemy, totalTime)) {
                const double fallbackChance = item.capturedBehaviorParamDouble("steal_or_dig", "chance", CapturedStealChanceEnemy);
                const float stealChance = static_cast<float>(std::clamp(
                    item.capturedBehaviorParamDouble("steal_or_dig", "stealChance", fallbackChance),
                    0.0,
                    1.0));
                if (tryStealHeldDrop(enemy, worldDrops, objectCatalog, player.position, totalTime, stealChance)) {
                    recordCapturedBehaviorUse(item, enemy, totalTime);
                    item.actionFlashTimer = SpellRingItemActionFlashSeconds;
                    if (hitEffectId.empty()) {
                        hitEffectId = "steal";
                    }
                    if (hitObject != nullptr) {
                        recordObjectEffectDiscovery(discoveryEvents, *hitObject, "steal", "敵から所持品を盗む", enemy.position);
                    }
                }
            }
            tryCapturedRewardFromEnemy(item, enemy, totalTime, events_);
            if (item.hasCapturedBehavior("charge_explode") && item.capturedExplodeSleepTimer <= 0.0f) {
                const int requiredHits = std::max(
                    1,
                    item.capturedBehaviorParamInt(
                        "charge_explode",
                        "count",
                        item.capturedBehaviorParamInt("charge_explode", "charges", CapturedExplosionChargeLimit)));
                const float restSeconds = static_cast<float>(std::max(0.1, item.capturedBehaviorParamDouble("charge_explode", "rest", CapturedExplosionSleepSeconds)));
                ++item.capturedExplodeCharge;
                if (item.capturedExplodeCharge >= requiredHits) {
                    item.capturedExplodeCharge = 0;
                    item.capturedExplodeSleepTimer = restSeconds;
                    const float explosionRadius = static_cast<float>(std::max(
                        8.0,
                        item.capturedBehaviorParamDouble("charge_explode", "radius", CapturedExplosionRadius)));
                    const int explosionDamage = std::max(
                        0,
                        item.capturedBehaviorParamInt("charge_explode", "damage", CapturedExplosionDamage));
                    const float terrainRadius = static_cast<float>(std::max(
                        0.0,
                        item.capturedBehaviorParamDouble("charge_explode", "terrainRadius", CapturedExplosionTerrainRadius)));
                    const int terrainDamage = std::max(
                        0,
                        item.capturedBehaviorParamInt("charge_explode", "terrainDamage", CapturedExplosionTerrainDamage));
                    events_.push_back(EnemyEvent{
                        .type = EnemyEventType::CapturedExplosion,
                        .position = item.worldPosition,
                        .damageAmount = explosionDamage,
                        .effectRadius = explosionRadius,
                        .terrainRadius = terrainRadius,
                        .terrainDamage = terrainDamage,
                    });
                }
            }
            enemy.hitFlash = 0.12f;
            EnemyEvent attackHitEvent = makeEnemyEvent(EnemyEventType::AttackHit, enemy, hitEffectId, damageDealt, criticalHit);
            attackHitEvent.ringItemImpact = true;
            attackHitEvent.frontGuarded = frontGuarded;
            attackHitEvent.weakPointHit = weakPointHit;
            events_.push_back(std::move(attackHitEvent));
            if (enemy.hp <= 0) {
                processEnemyDeath(enemy, item.worldPosition);
                break;
            }
        }
    }

    for (const SwarmSpawnRequest& request : pendingSwarmSpawns) {
        processSwarmSpawnRequest(request, map, balance, enemyCatalog);
    }

    if (!windPulses_.empty()) {
        const float safeDt = std::max(0.0f, dt);
        for (EnemyWindPulse& pulse : windPulses_) {
            const float pulseDt = std::min(safeDt, std::max(0.0f, pulse.remainingSeconds));
            if (pulseDt <= 0.0f || pulse.radius <= 0.0f || pulse.strength <= 0.0f) {
                pulse.remainingSeconds = 0.0f;
                continue;
            }

            const Vec2 windDirection = safeDirection(pulse.direction);
            const float playerFalloff = directionalWindFalloff(player.position, pulse.center, pulse.radius);
            if (playerFalloff > 0.0f) {
                const Vec2 delta = windDirection * (WindBlowPlayerPushSpeed * pulse.strength * playerFalloff * pulseDt);
                tryMoveCircle(map, player.position, effectivePlayerRadius(player, balance), delta);
            }

            for (Enemy& windEnemy : enemies_.items()) {
                if (!windEnemy.active ||
                    windEnemy.id == pulse.sourceRuntimeId ||
                    windEnemy.death.active ||
                    windEnemy.hp <= 0) {
                    continue;
                }
                const float enemyFalloff = directionalWindFalloff(windEnemy.position, pulse.center, pulse.radius);
                if (enemyFalloff <= 0.0f) {
                    continue;
                }
                const float massMultiplier = windEnemyMassMultiplier(windEnemy);
                if (massMultiplier <= 0.0f) {
                    continue;
                }
                const Vec2 delta = windDirection * (WindBlowEnemyPushSpeed * pulse.strength * enemyFalloff * massMultiplier * pulseDt);
                if (tryMoveCircle(map, windEnemy.position, enemyPassageRadius(windEnemy, placementCatalog_), delta)) {
                    windEnemy.aiDecisionTimer = std::min(windEnemy.aiDecisionTimer, 0.08f);
                }
            }

            worldDrops.pushDropsInDirection(objectCatalog, pulse.center, windDirection, pulseDt, pulse.radius, pulse.strength);
            projectiles.pushProjectilesInDirection(pulse.center, windDirection, pulseDt, pulse.radius, pulse.strength);
            spellRing.applyDirectionalWind(pulse.center, windDirection, pulseDt, pulse.radius, pulse.strength);
            pulse.remainingSeconds = std::max(0.0f, pulse.remainingSeconds - safeDt);
        }
        windPulses_.erase(
            std::remove_if(windPulses_.begin(), windPulses_.end(), [](const EnemyWindPulse& pulse) {
                return pulse.remainingSeconds <= 0.0f || pulse.radius <= 0.0f;
            }),
            windPulses_.end());
    }

    spellRing.removeBrokenItems();
}

void EnemySystem::render(
    Renderer& renderer,
    const TileMap& map,
    const ObjectCatalog& objectCatalog,
    Vec2 playerLight,
    const std::vector<LightSource>& extraLights,
    int highlightedEnemyId,
    const EncyclopediaSystem* encyclopedia)
{
    std::vector<DepthRenderEntry> entries;
    renderShadows(renderer, map, playerLight, extraLights);
    appendRenderEntries(entries, renderer, map, objectCatalog, playerLight, extraLights, highlightedEnemyId, encyclopedia);
    std::stable_sort(entries.begin(), entries.end(), [](const DepthRenderEntry& left, const DepthRenderEntry& right) {
        return left.sortY < right.sortY;
    });
    for (const DepthRenderEntry& entry : entries) {
        entry.draw();
    }
}

void EnemySystem::renderShadows(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights) const
{
    for (const Enemy& enemy : enemies_.items()) {
        if (!enemyVisible(enemy)) {
            continue;
        }
        const EnemyShadowSpec shadow = resolvedEnemyShadowSpec(shadowCatalog_, enemy.enemyId);
        const Vec2 shadowAnchor = enemyShadowAnchor(enemy, placementCatalog_, shadow);
        const Vec2 shadowBounds = enemyShadowBoundsSize(renderer, enemy, shadow);
        const bool walkInPresentation =
            enemy.spawnTimer > 0.0f &&
            enemy.spawnVisualKind == EnemySpawnVisualKind::WalkIn;
        if (!walkInPresentation && !map.isRectLit(shadowAnchor, shadowBounds, playerLight, extraLights)) {
            continue;
        }
        renderer.drawActorShadow(shadowAnchor, enemyShadowVisualSize(renderer, enemy), shadow.scale);
    }
}

void EnemySystem::appendRenderEntries(
    std::vector<DepthRenderEntry>& entries,
    Renderer& renderer,
    const TileMap& map,
    const ObjectCatalog& objectCatalog,
    Vec2 playerLight,
    const std::vector<LightSource>& extraLights,
    int highlightedEnemyId,
    const EncyclopediaSystem* encyclopedia) const
{
    for (const MudZone& zone : mudZones_) {
        if (zone.remainingSeconds <= 0.0f || zone.radius <= 0.0f) {
            continue;
        }
        const Vec2 visualBounds{zone.radius * 2.25f, zone.radius * 1.35f};
        if (!map.isRectLit(zone.position, visualBounds, playerLight, extraLights)) {
            continue;
        }
        const MudZone drawable = zone;
        entries.push_back(DepthRenderEntry{
            zone.position.y - zone.radius * 0.30f,
            [&renderer, drawable]() {
                const float lifeRatio = drawable.initialSeconds > 0.0f
                    ? clamp(drawable.remainingSeconds / drawable.initialSeconds, 0.0f, 1.0f)
                    : 1.0f;
                const float fade = std::min(1.0f, lifeRatio * 2.6f);
                const float shapeScale = 0.86f + fade * 0.14f;
                const bool poisonMud = drawable.damageType == "poison" || drawable.damagePerSecond > 0.0f;
                const Color shadow = poisonMud ? Color{10, 20, 12, 88} : Color{20, 16, 12, 72};
                const Color rim = poisonMud ? Color{92, 38, 112, 138} : Color{72, 46, 28, 120};
                const Color base = poisonMud ? Color{42, 82, 30, 166} : Color{82, 62, 42, 144};
                const Color core = poisonMud ? Color{112, 166, 38, 118} : Color{126, 92, 48, 94};
                const Color acid = poisonMud ? Color{190, 255, 72, 106} : Color{184, 142, 72, 72};
                const Color bubble = poisonMud ? Color{214, 255, 112, 132} : Color{198, 164, 104, 98};

                renderer.fillSoftCircle(drawable.position, drawable.radius * 1.12f, scaleColorAlpha(shadow, fade));

                decltype(drawable.outlineOffsets) outerPoints{};
                decltype(drawable.outlineOffsets) innerPoints{};
                for (std::size_t i = 0; i < drawable.outlineOffsets.size(); ++i) {
                    outerPoints[i] = drawable.position + drawable.outlineOffsets[i] * (shapeScale * 1.07f);
                    innerPoints[i] = drawable.position + drawable.outlineOffsets[i] * shapeScale;
                }
                renderer.fillPolygon(outerPoints.data(), outerPoints.size(), scaleColorAlpha(rim, fade));
                renderer.fillPolygon(innerPoints.data(), innerPoints.size(), scaleColorAlpha(base, fade));

                renderer.fillEllipse(
                    drawable.position + Vec2{-drawable.radius * 0.18f, -drawable.radius * 0.03f},
                    {drawable.radius * 0.58f * shapeScale, drawable.radius * 0.22f * shapeScale},
                    scaleColorAlpha(core, fade));
                renderer.fillEllipse(
                    drawable.position + Vec2{drawable.radius * 0.24f, drawable.radius * 0.08f},
                    {drawable.radius * 0.34f * shapeScale, drawable.radius * 0.13f * shapeScale},
                    scaleColorAlpha(acid, fade));

                for (const MudZoneBubble& bubbleSpec : drawable.bubbles) {
                    if (bubbleSpec.radius <= 0.0f) {
                        continue;
                    }
                    const float pulse = 0.82f + 0.18f * std::sin((1.0f - lifeRatio) * Pi * 5.0f + bubbleSpec.phase);
                    renderer.fillSoftCircle(
                        drawable.position + bubbleSpec.offset * shapeScale,
                        bubbleSpec.radius * pulse,
                        scaleColorAlpha(bubble, fade));
                }
            },
        });
    }

    for (const EnemyWindPulse& pulse : windPulses_) {
        if (pulse.remainingSeconds <= 0.0f || pulse.radius <= 0.0f) {
            continue;
        }
        const Vec2 visualBounds{pulse.radius * 2.0f, pulse.radius * 2.0f};
        if (!map.isRectLit(pulse.center, visualBounds, playerLight, extraLights)) {
            continue;
        }
        const EnemyWindPulse drawable = pulse;
        entries.push_back(DepthRenderEntry{
            drawable.center.y - drawable.radius * 0.35f,
            [&renderer, drawable]() {
                const float fade = drawable.initialSeconds > 0.0f
                    ? clamp(drawable.remainingSeconds / drawable.initialSeconds, 0.0f, 1.0f)
                    : 1.0f;
                const float phase = 1.0f - fade;
                const Vec2 dir = safeDirection(drawable.direction);
                const Vec2 side{-dir.y, dir.x};
                const Color fieldColor = scaleColorAlpha({118, 220, 196, 74}, fade);
                const Color ringColor = scaleColorAlpha({164, 246, 224, 132}, fade);
                const Color lineColor = scaleColorAlpha({210, 255, 240, 116}, fade);
                renderer.fillSoftCircle(drawable.center, drawable.radius, fieldColor);
                renderer.drawSoftRing(drawable.center, drawable.radius * (0.82f + phase * 0.18f), std::max(3.0f, drawable.radius * 0.035f), ringColor);
                for (int lane = -1; lane <= 1; ++lane) {
                    const float laneOffset = static_cast<float>(lane) * drawable.radius * 0.24f;
                    const float streamShift = (phase - 0.5f) * drawable.radius * 0.30f;
                    const Vec2 start = drawable.center - dir * (drawable.radius * 0.58f) + side * laneOffset + dir * streamShift;
                    const Vec2 end = drawable.center + dir * (drawable.radius * 0.58f) + side * (laneOffset + drawable.radius * 0.08f) + dir * streamShift;
                    renderer.drawSoftLine(start, end, std::max(2.0f, drawable.radius * 0.025f), lineColor);
                }
            },
        });
    }

    for (const Enemy& enemy : enemies_.items()) {
        if (!enemyVisible(enemy)) {
            continue;
        }
        const Vec2 drawPosition = enemyDrawPosition(enemy, placementCatalog_);
        Vec2 visualSize = enemyVisualBoundsSize(renderer, enemy);
        if (visibleCarriedLoot(enemy) != nullptr) {
            visualSize.x = std::max(visualSize.x, WorldItemImageMaxSize.x + enemyVisualRadius(enemy));
            visualSize.y = std::max(visualSize.y, WorldItemImageMaxSize.y + enemyVisualRadius(enemy));
        }
        const bool walkInPresentation =
            enemy.spawnTimer > 0.0f &&
            enemy.spawnVisualKind == EnemySpawnVisualKind::WalkIn;
        if (!walkInPresentation && !map.isRectLit(drawPosition, visualSize, playerLight, extraLights)) {
            continue;
        }
        const bool captureHighlighted = highlightedEnemyId != 0 && enemy.id == highlightedEnemyId;
        const bool detailsKnown = !enemy.isBoss &&
            encyclopedia != nullptr &&
            encyclopedia->enemyStage(enemy.enemyId) == EncyclopediaStage::Complete;
        entries.push_back(DepthRenderEntry{
            enemy.position.y,
            [&renderer, &map, &objectCatalog, &enemy, hitboxCatalog = hitboxCatalog_, placementCatalog = placementCatalog_, captureHighlighted, detailsKnown]() {
                drawEnemyVisual(renderer, map, enemy, objectCatalog, hitboxCatalog, placementCatalog, captureHighlighted, detailsKnown);
            },
        });
    }
}

void EnemySystem::emitStatusParticles(EffectSystem& effects) const
{
    for (const Enemy& enemy : enemies_.items()) {
        if (!enemyVisible(enemy) || enemy.death.active || enemy.spawnTimer > 0.0f) {
            continue;
        }
        emitEntityStatusAuras(enemy.status, enemy.position, effects);
    }
}

void EnemySystem::appendWetGroundEmitters(std::vector<WetGroundEmitter>& emitters) const
{
    for (const Enemy& enemy : enemies_.items()) {
        if (!enemyVisible(enemy) || enemy.death.active || enemy.spawnTimer > 0.0f ||
            !enemy.status.hasState("status_wet")) {
            continue;
        }

        emitters.push_back(WetGroundEmitter{
            .sourceKey = "enemy:" + std::to_string(enemy.id),
            .position = enemy.position,
            .radius = std::clamp(enemyPassageRadius(enemy, placementCatalog_) * 1.35f, 12.0f, 38.0f),
            .strength = 1.0f,
        });
    }
}

std::vector<StatusPopupEvent> EnemySystem::consumeStatusPopupEvents()
{
    std::vector<StatusPopupEvent> consumed;
    consumed.swap(statusPopupEvents_);
    return consumed;
}

std::vector<EnemySoundEvent> EnemySystem::consumeSoundEvents()
{
    std::vector<EnemySoundEvent> consumed;
    consumed.swap(soundEvents_);
    return consumed;
}

std::vector<CaptureResult> EnemySystem::consumeCaptureResults()
{
    std::vector<CaptureResult> consumed;
    consumed.swap(captureResults_);
    return consumed;
}

bool EnemySystem::hitByPlayerProjectile(
    Projectile& projectile,
    Player& player,
    SpellRingSystem& spellRing,
    int damage,
    const EffectDispatcher& effectDispatcher,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    if (!projectile.active || projectile.ownerType != ProjectileOwnerType::PlayerOrbit) {
        return false;
    }

    for (Enemy& enemy : enemies_.items()) {
        if (!enemyCanBeHit(enemy) || enemy.spawnTimer > 0.0f) {
            continue;
        }
        if (isAstragnaBossAction(enemy)) {
            if (tryHitAstragnaWithProjectile(enemy, projectile, std::max(0, damage), events_)) {
                return true;
            }
            continue;
        }
        const Vec2 enemyHitboxOffset = enemyVisualOffset(enemy, placementCatalog_);
        if (!enemyHitboxOverlapsCircle(enemy, hitboxCatalog_, projectile.position, projectile.radius, enemyHitboxOffset)) {
            continue;
        }

        const BossDamageAdjustment bossDamage = adjustBossIncomingDamage(
            enemy,
            std::max(0, damage),
            projectile.position,
            projectile.radius,
            hitboxCatalog_,
            placementCatalog_);
        const int adjustedDamage = applyDefenseModifier(enemy.status, bossDamage.damage);
        applyEnemyDamageTyped(enemy, adjustedDamage, projectile.damageType);
        revealEnemyHpBar(enemy, adjustedDamage);
        enemy.hitFlash = 0.12f;

        if (!projectile.effects.empty()) {
            EffectContext context;
            context.owner = &player;
            context.targetEntity = &enemy;
            context.hitTarget = &enemy;
            context.orbit = &spellRing;
            context.statusPopupEvents = &statusPopupEvents_;
            context.discoveryEvents = discoveryEvents;
            context.encyclopedia = encyclopedia;
            context.position = projectile.position;
            context.triggerType = EffectTriggerType::Hit;
            context.logUnimplementedEffects = false;
            effectDispatcher.dispatch(projectile.effects, context);
        }

        std::string hitEffectId = visualEffectIdFor(projectile.effects, projectile.damageType);
        if (!bossDamage.effectId.empty()) {
            hitEffectId = std::string(bossDamage.effectId);
        }
        EnemyEvent hitEvent = makeEnemyEvent(EnemyEventType::AttackHit, enemy, hitEffectId, adjustedDamage);
        hitEvent.weakPointHit = bossDamage.weakPointHit;
        events_.push_back(std::move(hitEvent));
        if (enemy.hp <= 0) {
            beginEnemyDeath(enemy, spellRing, projectile.position);
        }
        return true;
    }

    return false;
}

int EnemySystem::applyObjectBreakShardDamage(
    Vec2 position,
    float radius,
    int damage,
    std::string_view damageType,
    std::string_view effectId,
    SpellRingSystem& spellRing)
{
    if (damage <= 0 || radius <= 0.0f) {
        return 0;
    }

    EnemyMagicHitSpec spec;
    spec.position = position;
    spec.radius = radius;
    spec.damage = damage;
    spec.damageType = std::string(damageType);
    spec.effectId = std::string(effectId);
    return applyMagicArea(spec, spellRing);
}

int EnemySystem::applyColdAirAura(
    Vec2 position,
    float radius,
    float strength,
    float dt,
    std::string_view source,
    int* outFrozenCount)
{
    if (outFrozenCount != nullptr) {
        *outFrozenCount = 0;
    }
    if (radius <= 0.0f || strength <= 0.0f || dt <= 0.0f) {
        return 0;
    }

    int touched = 0;
    int frozen = 0;
    const float clampedStrength = std::max(0.0f, strength);
    for (Enemy& enemy : enemies_.items()) {
        if (!enemyCanBeHit(enemy) || enemy.spawnTimer > 0.0f) {
            continue;
        }
        if (!enemyHitboxOverlapsCircle(enemy, hitboxCatalog_, position, radius, enemyVisualOffset(enemy, placementCatalog_))) {
            continue;
        }

        ++touched;
        enemy.coldExposureTouched = true;
        if (enemy.status.hasState("status_frozen")) {
            enemy.coldExposure = 0.0f;
            continue;
        }

        enemy.coldExposure = std::min(
            ColdExposureFreezeThreshold,
            enemy.coldExposure + clampedStrength * ColdExposureRatePerSecond * dt);
        if (enemy.coldExposure >= ColdExposureFreezeThreshold) {
            const EntityStateApplyResult result = enemy.status.applyState(
                "status_frozen",
                clampedStrength,
                FrozenDefaultDurationSeconds,
                std::string(source),
                StateApplyMode::KeepLonger);
            queueStatusPopupEvent(
                statusPopupEvents_,
                enemy.position,
                "status_frozen",
                StatusPopupTarget::Enemy,
                result);
            enemy.coldExposure = 0.0f;
            enemy.hitFlash = 0.12f;
            ++frozen;
        }
    }

    if (outFrozenCount != nullptr) {
        *outFrozenCount = frozen;
    }
    return touched;
}

int EnemySystem::applyConductiveShock(Vec2 position, float radius, double value, double duration, int excludedEnemyId, std::string_view source)
{
    if (radius <= 0.0f) {
        return 0;
    }

    int shocked = 0;
    const double shockValue = std::clamp(value, 0.25, 5.0);
    const double shockDuration = std::clamp(duration, 0.1, 6.0);
    for (Enemy& enemy : enemies_.items()) {
        if (!enemyCanBeHit(enemy) || enemy.spawnTimer > 0.0f || enemy.id == excludedEnemyId) {
            continue;
        }
        if (!enemy.status.hasState("status_wet")) {
            continue;
        }

        if (!enemyHitboxOverlapsCircle(enemy, hitboxCatalog_, position, radius, enemyVisualOffset(enemy, placementCatalog_))) {
            continue;
        }
        EntityStateApplyResult shockResult;
        if (!applyShockedStateToEnemy(enemy, shockValue, shockDuration, source, &shockResult)) {
            continue;
        }
        queueStatusPopupEvent(
            statusPopupEvents_,
            enemy.position,
            "status_shocked",
            StatusPopupTarget::Enemy,
            shockResult);

        ++shocked;
        events_.push_back(makeEnemyEvent(EnemyEventType::AttackHit, enemy, "conduct_water_puddle"));
    }
    return shocked;
}

int EnemySystem::applyHotAir(
    Vec2 position,
    float radius,
    float strength,
    float dt,
    std::string_view source,
    int* outHotCount)
{
    if (outHotCount != nullptr) {
        *outHotCount = 0;
    }
    if (radius <= 0.0f || strength <= 0.0f || dt <= 0.0f) {
        return 0;
    }

    int touched = 0;
    int heated = 0;
    const double hotValue = std::max(0.25, static_cast<double>(strength));
    for (Enemy& enemy : enemies_.items()) {
        if (!enemyCanBeHit(enemy) || enemy.spawnTimer > 0.0f) {
            continue;
        }
        const Vec2 enemyHitboxOffset = enemyVisualOffset(enemy, placementCatalog_);
        if (!enemyHitboxOverlapsCircle(enemy, hitboxCatalog_, position, radius, enemyHitboxOffset)) {
            continue;
        }

        ++touched;
        if (!enemy.isBoss) {
            const EntityStateApplyResult result = enemy.status.applyState(
                "status_hot",
                hotValue,
                HotAirStatusDurationSeconds,
                std::string(source),
                StateApplyMode::KeepLonger);
            queueStatusPopupEvent(
                statusPopupEvents_,
                enemy.position,
                "status_hot",
                StatusPopupTarget::Enemy,
                result);
            if (result.added) {
                ++heated;
            }
        }
    }

    if (outHotCount != nullptr) {
        *outHotCount = heated;
    }
    return touched;
}

void EnemySystem::beginEnemyDeath(
    Enemy& enemy,
    SpellRingSystem& spellRing,
    std::optional<Vec2> hitOrigin,
    bool suppressRewards)
{
    if (!enemy.active || enemy.death.active) {
        return;
    }

    enemy.hp = 0;
    enemy.death = {};
    enemy.death.active = true;
    enemy.death.suppressRewards = suppressRewards;
    std::uniform_real_distribution<float> durationDist(EnemyDeathMinSeconds, EnemyDeathMaxSeconds);
    enemy.death.durationSeconds = durationDist(rng_);
    enemy.death.shakeSeed = mixDeathSeed(
        static_cast<std::uint32_t>(enemy.id) ^
        static_cast<std::uint32_t>(rng_()));
    enemy.velocity = {};
    enemy.hitFlash = 0.0f;
    enemy.hpBarTimer = 0.0f;
    enemy.contactTimer = 0.0f;
    enemy.awarenessIcon = EnemyAwarenessIcon::None;
    enemy.awarenessIconTimer = 0.0f;
    enemy.poisonDamageAccumulator = 0.0;
    enemy.hotDamageAccumulator = 0.0;
    enemy.bleedDamageAccumulator = 0.0;
    clearEnemyAction(enemy);
    enemy.jumpActive = false;
    clearExternalBounceState(enemy);

    if (hitOrigin && enemyDeathKnockbackAllowed(enemy)) {
        Vec2 direction = enemy.position - *hitOrigin;
        if (lengthSquared(direction) <= 0.0001f) {
            direction = randomDirection(rng_);
        }
        enemy.death.knockbackVelocity = normalize(direction) * EnemyDeathKnockbackSpeed;
        enemy.death.knockbackTimer = EnemyDeathKnockbackSeconds;
    }

}

void EnemySystem::updateEnemyDeath(Enemy& enemy, TileMap& map, SpellRingSystem& spellRing, float dt)
{
    if (!enemy.active || !enemy.death.active) {
        return;
    }

    enemy.death.elapsedSeconds += std::max(0.0f, dt);
    enemy.behaviorTimer += std::max(0.0f, dt);
    enemy.hitFlash = 0.0f;
    enemy.hpBarTimer = 0.0f;
    enemy.velocity = {};
    enemy.awarenessIcon = EnemyAwarenessIcon::None;
    enemy.awarenessIconTimer = 0.0f;
    if (enemy.death.knockbackTimer > 0.0f && lengthSquared(enemy.death.knockbackVelocity) > 0.0001f) {
        moveWithCollision(enemy, map, enemy.death.knockbackVelocity, dt);
        enemy.death.knockbackTimer = std::max(0.0f, enemy.death.knockbackTimer - dt);
        enemy.death.knockbackVelocity = enemy.death.knockbackVelocity *
            std::max(0.0f, 1.0f - EnemyDeathKnockbackDampingPerSecond * dt);
    } else {
        enemy.death.knockbackVelocity = {};
        enemy.death.knockbackTimer = 0.0f;
    }
    updateEnemyAltitude(enemy);

    if (enemy.death.elapsedSeconds >= std::max(0.0f, enemy.death.durationSeconds)) {
        finishEnemyDeath(enemy, spellRing);
    }
}

void EnemySystem::finishEnemyDeath(Enemy& enemy, SpellRingSystem& spellRing)
{
    if (!enemy.active) {
        return;
    }

    const bool suppressRewards = enemy.death.suppressRewards;
    if (!suppressRewards) {
        pendingXp_ += enemy.xp;
        if (!enemy.dropItemConsumed) {
            queueEnemyObjectDrops(enemy);
        }
        queueEnemyHeldDrops(enemy);
        if (!enemy.dropMaterialConsumed) {
            queueEnemyMaterialDrops(enemy);
        }
    }
    EnemyEvent deathEvent = makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy);
    deathEvent.suppressRewards = suppressRewards;
    if (suppressRewards) {
        deathEvent.moneyDrop = 0;
    }
    events_.push_back(std::move(deathEvent));
    enemy.death = {};
    enemy.active = false;
}

int EnemySystem::applyMagicArea(const EnemyMagicHitSpec& spec, SpellRingSystem& spellRing)
{
    if (spec.damage <= 0 &&
        spec.statusEffect.empty() &&
        spec.knockbackStrength <= 0.0f) {
        return 0;
    }

    int hits = 0;
    const float radius = std::max(0.0f, spec.radius);
    std::uniform_real_distribution<double> chanceDist(0.0, 100.0);
    for (Enemy& enemy : enemies_.items()) {
        if (!enemyCanBeHit(enemy) || enemy.spawnTimer > 0.0f) {
            continue;
        }
        if (isAstragnaBossAction(enemy)) {
            continue;
        }
        if (spec.excludedRuntimeId != 0 && enemy.id == spec.excludedRuntimeId) {
            continue;
        }
        const Vec2 enemyHitboxOffset = enemyVisualOffset(enemy, placementCatalog_);
        if (!enemyHitboxOverlapsCircle(enemy, hitboxCatalog_, spec.position, radius, enemyHitboxOffset)) {
            continue;
        }

        if (spec.damage > 0) {
            const int typedDamage = std::max(
                0,
                static_cast<int>(std::ceil(static_cast<double>(spec.damage) * damageTypeMultiplier(spec.damageType))));
            const BossDamageAdjustment bossDamage = adjustBossIncomingDamage(
                enemy,
                typedDamage,
                spec.position,
                radius,
                hitboxCatalog_,
                placementCatalog_);
            const int damageDealt = applyDefenseModifier(enemy.status, bossDamage.damage);
            applyEnemyDamageTyped(enemy, damageDealt, spec.damageType.empty() ? spec.effectId : spec.damageType);
            revealEnemyHpBar(enemy, damageDealt);
            enemy.hitFlash = 0.12f;
            EnemyEvent hitEvent = makeEnemyEvent(
                EnemyEventType::AttackHit,
                enemy,
                bossDamage.effectId.empty()
                    ? (spec.effectId.empty() ? spec.damageType : spec.effectId)
                    : std::string(bossDamage.effectId),
                damageDealt);
            hitEvent.weakPointHit = bossDamage.weakPointHit;
            events_.push_back(std::move(hitEvent));
        }

        if (!spec.statusEffect.empty() && enemy.hp > 0 && !enemy.isBoss && chanceDist(rng_) <= std::clamp(spec.statusChance, 0.0, 100.0)) {
            const EntityStateApplyResult result = enemy.status.applyState(
                spec.statusEffect,
                spec.statusValue,
                spec.statusDuration,
                spec.damageType.empty() ? "magic" : "magic:" + spec.damageType,
                StateApplyMode::KeepLonger);
            queueStatusPopupEvent(
                statusPopupEvents_,
                enemy.position,
                spec.statusEffect,
                StatusPopupTarget::Enemy,
                result);
        }
        if (spec.knockbackStrength > 0.0f) {
            Vec2 direction = spec.knockbackDirection;
            if (lengthSquared(direction) <= 0.0001f) {
                direction = enemy.position - spec.position;
            }
            enemy.knockbackVelocity = normalize(direction) * spec.knockbackStrength;
            enemy.knockbackTimer = std::max(enemy.knockbackTimer, 0.14f);
        }

        ++hits;
        if (enemy.hp <= 0) {
            beginEnemyDeath(enemy, spellRing, spec.position);
        }
        if (spec.maxHits > 0 && hits >= spec.maxHits) {
            break;
        }
    }
    return hits;
}

bool EnemySystem::applyMagicNearest(Vec2 origin, float range, EnemyMagicHitSpec spec, SpellRingSystem& spellRing, Vec2* outTargetPosition)
{
    Enemy* best = nullptr;
    float bestDistanceSq = std::max(0.0f, range) * std::max(0.0f, range);
    for (Enemy& enemy : enemies_.items()) {
        if (!enemyCanBeHit(enemy) || enemy.spawnTimer > 0.0f) {
            continue;
        }
        if (isAstragnaBossAction(enemy)) {
            continue;
        }
        if (spec.excludedRuntimeId != 0 && enemy.id == spec.excludedRuntimeId) {
            continue;
        }
        const float distanceSq = distanceSquared(enemy.position, origin);
        if (distanceSq <= bestDistanceSq) {
            best = &enemy;
            bestDistanceSq = distanceSq;
        }
    }

    if (best == nullptr) {
        return false;
    }

    spec.position = best->position;
    spec.radius = std::max(spec.radius, effectiveEnemyRadius(*best) + 2.0f);
    spec.maxHits = 1;
    const int hitCount = applyMagicArea(spec, spellRing);
    if (hitCount <= 0) {
        return false;
    }
    if (outTargetPosition != nullptr) {
        *outTargetPosition = spec.position;
    }
    return true;
}

void EnemySystem::applyExplosionDamage(Vec2 position, float radius, SpellRingSystem& spellRing, int damage, int excludedEnemyRuntimeId)
{
    const float safeRadius = std::max(0.0f, radius);
    if (safeRadius <= 0.0f || damage <= 0) {
        return;
    }

    spellRing.applyExplosionDamageToItems(position, safeRadius, damage);

    for (Enemy& enemy : enemies_.items()) {
        if (!enemyCanBeHit(enemy) || enemy.spawnTimer > 0.0f || enemy.id == excludedEnemyRuntimeId) {
            continue;
        }
        if (isAstragnaBossAction(enemy)) {
            continue;
        }
        if (!enemyHitboxOverlapsCircle(enemy, hitboxCatalog_, position, safeRadius, enemyVisualOffset(enemy, placementCatalog_))) {
            continue;
        }

        const BossDamageAdjustment bossDamage = adjustBossIncomingDamage(
            enemy,
            std::max(0, damage),
            position,
            safeRadius,
            hitboxCatalog_,
            placementCatalog_);
        const int damageDealt = applyDefenseModifier(enemy.status, bossDamage.damage);
        applyEnemyDamageTyped(enemy, damageDealt, "fire");
        revealEnemyHpBar(enemy, damageDealt);
        enemy.hitFlash = 0.18f;
        enemy.knockbackVelocity = normalize(enemy.position - position) * 110.0f;
        enemy.knockbackTimer = std::max(enemy.knockbackTimer, 0.14f);
        EnemyEvent hitEvent = makeEnemyEvent(
            EnemyEventType::AttackHit,
            enemy,
            bossDamage.effectId.empty() ? "fire" : std::string(bossDamage.effectId),
            damageDealt);
        hitEvent.ringItemImpact = true;
        hitEvent.weakPointHit = bossDamage.weakPointHit;
        events_.push_back(std::move(hitEvent));
        if (enemy.hp <= 0) {
            beginEnemyDeath(enemy, spellRing, position);
        }
    }
}

void EnemySystem::addMudZone(
    Vec2 position,
    float radius,
    float duration,
    float speedMultiplier,
    float damagePerSecond,
    std::string damageType,
    DamageCause damageCause)
{
    if (!(radius > 0.0f) || !(duration > 0.0f)) {
        return;
    }
    MudZone zone;
    zone.position = position;
    zone.radius = std::max(8.0f, radius);
    zone.initialSeconds = std::clamp(duration, 0.1f, MudZoneMaxDurationSeconds);
    zone.remainingSeconds = zone.initialSeconds;
    zone.speedMultiplier = clamp(speedMultiplier, 0.05f, 1.0f);
    zone.damagePerSecond = std::max(0.0f, damagePerSecond);
    zone.damageType = std::move(damageType);
    zone.damageCause = std::move(damageCause);

    std::uniform_real_distribution<float> rotationDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> outlineJitterDistribution(MudZoneOutlineMinJitter, MudZoneOutlineMaxJitter);
    const float rotation = rotationDistribution(rng_);
    const float cosRotation = std::cos(rotation);
    const float sinRotation = std::sin(rotation);
    for (std::size_t i = 0; i < zone.outlineOffsets.size(); ++i) {
        const float angle = static_cast<float>(i) / static_cast<float>(zone.outlineOffsets.size()) * Pi * 2.0f;
        const float pointRadius = zone.radius * outlineJitterDistribution(rng_);
        const Vec2 local{
            std::cos(angle) * pointRadius,
            std::sin(angle) * pointRadius * MudZoneVisualYScale,
        };
        zone.outlineOffsets[i] = {
            local.x * cosRotation - local.y * sinRotation,
            local.x * sinRotation + local.y * cosRotation,
        };
    }

    std::uniform_real_distribution<float> bubbleAngleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> bubbleDistanceDistribution(0.10f, 0.72f);
    std::uniform_real_distribution<float> bubbleRadiusDistribution(0.045f, 0.095f);
    for (MudZoneBubble& bubble : zone.bubbles) {
        const float angle = bubbleAngleDistribution(rng_);
        const float distance = zone.radius * bubbleDistanceDistribution(rng_);
        const Vec2 local{
            std::cos(angle) * distance,
            std::sin(angle) * distance * MudZoneVisualYScale,
        };
        bubble.offset = {
            local.x * cosRotation - local.y * sinRotation,
            local.x * sinRotation + local.y * cosRotation,
        };
        bubble.radius = zone.radius * bubbleRadiusDistribution(rng_);
        bubble.phase = bubbleAngleDistribution(rng_);
    }
    mudZones_.push_back(std::move(zone));
}

int EnemySystem::pullMetalEnemies(Vec2 center, TileMap& map, float dt, float radius)
{
    if (dt <= 0.0f) {
        return 0;
    }

    int pulled = 0;
    const float effectiveRadius = std::max(8.0f, radius);
    const float radiusSq = effectiveRadius * effectiveRadius;
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || enemy.dungeonEventActivationLocked || enemy.spawnTimer > 0.0f || !hasEnemyTag(enemy, "metal")) {
            continue;
        }
        const Vec2 toCenter = center - enemy.position;
        const float distanceSq = lengthSquared(toCenter);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }

        const float distance = std::sqrt(distanceSq);
        const float falloff = 1.0f - clamp(distance / effectiveRadius, 0.0f, 1.0f);
        const Vec2 delta = normalize(toCenter) * (CapturedMagnetEnemyPullSpeed * falloff * dt);
        if (tryMoveCircle(map, enemy.position, enemyPassageRadius(enemy, placementCatalog_), delta)) {
            ++pulled;
            if (pulled >= CapturedMagnetEnemyLimit) {
                break;
            }
        }
    }
    return pulled;
}

int EnemySystem::pullLightEnemies(Vec2 center, TileMap& map, float dt, float radius, float strength)
{
    if (dt <= 0.0f || radius <= 0.0f || strength <= 0.0f) {
        return 0;
    }

    int pulled = 0;
    const float effectiveRadius = std::max(8.0f, radius);
    const float radiusSq = effectiveRadius * effectiveRadius;
    const float pullSpeed = VacuumLightEnemyPullSpeed * std::clamp(strength, 0.1f, 3.0f);
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || enemy.dungeonEventActivationLocked || enemy.spawnTimer > 0.0f || !canMoveLightEnemy(enemy)) {
            continue;
        }
        const Vec2 toCenter = center - enemy.position;
        const float distanceSq = lengthSquared(toCenter);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }

        const float distance = std::sqrt(distanceSq);
        const float falloff = 0.35f + (1.0f - clamp(distance / effectiveRadius, 0.0f, 1.0f)) * 0.65f;
        const Vec2 delta = normalize(toCenter) * (pullSpeed * falloff * dt);
        if (tryMoveCircle(map, enemy.position, enemyPassageRadius(enemy, placementCatalog_), delta)) {
            ++pulled;
            if (pulled >= VacuumLightEnemyLimit) {
                break;
            }
        }
    }
    return pulled;
}

int EnemySystem::pushLightEnemies(Vec2 center, TileMap& map, float dt, float radius, float strength)
{
    if (dt <= 0.0f || radius <= 0.0f || strength <= 0.0f) {
        return 0;
    }

    int pushed = 0;
    const float effectiveRadius = std::max(8.0f, radius);
    const float radiusSq = effectiveRadius * effectiveRadius;
    const float pushSpeed = WindLightEnemyPushSpeed * std::clamp(strength, 0.1f, 3.0f);
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || enemy.dungeonEventActivationLocked || enemy.spawnTimer > 0.0f || !canMoveLightEnemy(enemy)) {
            continue;
        }
        const Vec2 away = enemy.position - center;
        const float distanceSq = lengthSquared(away);
        if (distanceSq <= 1.0f || distanceSq > radiusSq) {
            continue;
        }

        const float distance = std::sqrt(distanceSq);
        const float falloff = 0.35f + (1.0f - clamp(distance / effectiveRadius, 0.0f, 1.0f)) * 0.65f;
        const Vec2 delta = normalize(away) * (pushSpeed * falloff * dt);
        if (tryMoveCircle(map, enemy.position, enemyPassageRadius(enemy, placementCatalog_), delta)) {
            ++pushed;
            if (pushed >= WindLightEnemyLimit) {
                break;
            }
        }
    }
    return pushed;
}

void EnemySystem::clearSpawnBiases()
{
    spawnBiasMultipliers_.clear();
}

void EnemySystem::applySpawnBias(std::string_view group, double multiplier)
{
    if (group.empty()) {
        return;
    }

    const double sanitizedMultiplier = std::isfinite(multiplier) && multiplier > 0.0
        ? multiplier
        : SpawnBiasDefaultMultiplier;
    auto& current = spawnBiasMultipliers_[std::string(group)];
    if (current <= 0.0 || !std::isfinite(current)) {
        current = SpawnBiasDefaultMultiplier;
    }
    current = std::clamp(
        current * std::clamp(sanitizedMultiplier, SpawnBiasMinMultiplier, SpawnBiasMaxMultiplier),
        SpawnBiasMinMultiplier,
        SpawnBiasMaxMultiplier);
}

void EnemySystem::activateDungeonEventEnemies(std::string_view eventId, bool wakeSleepingEnemies)
{
    if (eventId.empty()) {
        return;
    }
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || enemy.dungeonEventId != eventId) {
            continue;
        }

        enemy.dungeonEventActivationLocked = false;
        if (enemy.dungeonEventSleeping) {
            if (wakeSleepingEnemies) {
                wakeDungeonEventEnemy(enemy, enemy.position + Vec2{1.0f, 0.0f}, true);
            }
            continue;
        }
        forceDetectInSight(enemy, enemy.position + Vec2{1.0f, 0.0f}, true);
    }
}

void EnemySystem::wakeDungeonEventEnemies(std::string_view eventId)
{
    if (eventId.empty()) {
        return;
    }
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || enemy.dungeonEventId != eventId) {
            continue;
        }
        if (!enemy.dungeonEventSleeping) {
            continue;
        }
        wakeDungeonEventEnemy(enemy, enemy.position + Vec2{1.0f, 0.0f}, true);
    }
}

bool EnemySystem::setManualDetectionOnlyNear(Vec2 position, float radius, bool manualOnly)
{
    Enemy* enemy = findActiveEnemyNear(position, radius);
    if (enemy == nullptr) {
        return false;
    }
    enemy->manualDetectionOnly = manualOnly;
    return true;
}

bool EnemySystem::setManualDetectionOnlyForRuntimeEnemy(int runtimeId, bool manualOnly)
{
    Enemy* enemy = findRuntimeEnemy(runtimeId);
    if (enemy == nullptr) {
        return false;
    }
    enemy->manualDetectionOnly = manualOnly;
    return true;
}

bool EnemySystem::forceDetectRuntimeEnemy(int runtimeId, Vec2 playerPosition, bool showIcon)
{
    Enemy* enemy = findRuntimeEnemy(runtimeId);
    if (enemy == nullptr) {
        return false;
    }
    enemy->manualDetectionOnly = false;
    forceDetectInSight(*enemy, playerPosition, showIcon);
    return true;
}

bool EnemySystem::forceDetectEnemyNear(Vec2 position, float radius, Vec2 playerPosition, bool showIcon)
{
    Enemy* enemy = findActiveEnemyNear(position, radius);
    if (enemy == nullptr) {
        return false;
    }
    enemy->manualDetectionOnly = false;
    forceDetectInSight(*enemy, playerPosition, showIcon);
    return true;
}

bool EnemySystem::setRuntimeEnemyMovementLeash(int runtimeId, Vec2 center, float radius)
{
    Enemy* enemy = findRuntimeEnemy(runtimeId);
    if (enemy == nullptr) {
        return false;
    }
    enemy->movementLeashEnabled = radius > 0.0f;
    enemy->movementLeashCenter = center;
    enemy->movementLeashRadius = std::max(0.0f, radius);
    if (!enemy->movementLeashEnabled) {
        return true;
    }
    const Vec2 fromLeashCenter = enemy->position - center;
    const float leashDistance = length(fromLeashCenter);
    if (leashDistance > enemy->movementLeashRadius && leashDistance > 0.0001f) {
        enemy->position = center + fromLeashCenter / leashDistance * enemy->movementLeashRadius;
        enemy->velocity = {};
        enemy->aiDecisionTimer = 0.0f;
    }
    return true;
}

int EnemySystem::activeDungeonEventEnemyCount(std::string_view eventId) const
{
    if (eventId.empty()) {
        return 0;
    }
    int count = 0;
    for (const Enemy& enemy : enemies_.items()) {
        if (enemy.active && enemy.dungeonEventId == eventId) {
            ++count;
        }
    }
    return count;
}

int EnemySystem::activeSleepingDungeonEventEnemyCount(std::string_view eventId) const
{
    if (eventId.empty()) {
        return 0;
    }
    int count = 0;
    for (const Enemy& enemy : enemies_.items()) {
        if (enemy.active && enemy.dungeonEventId == eventId && enemy.dungeonEventSleeping) {
            ++count;
        }
    }
    return count;
}

int EnemySystem::activeRuntimeEnemyCount(const std::vector<int>& runtimeIds) const
{
    int count = 0;
    for (int runtimeId : runtimeIds) {
        if (runtimeEnemyActive(runtimeId)) {
            ++count;
        }
    }
    return count;
}

bool EnemySystem::runtimeEnemyActive(int runtimeId) const
{
    return findRuntimeEnemy(runtimeId) != nullptr;
}

bool EnemySystem::runtimeEnemyPosition(int runtimeId, Vec2& outPosition) const
{
    const Enemy* enemy = findRuntimeEnemy(runtimeId);
    if (enemy == nullptr) {
        return false;
    }
    outPosition = enemy->position;
    return true;
}

bool EnemySystem::setRuntimeEnemyHp(int runtimeId, int hp)
{
    Enemy* enemy = findRuntimeEnemy(runtimeId);
    if (enemy == nullptr) {
        return false;
    }
    if (enemy->death.active) {
        return false;
    }
    enemy->hp = std::clamp(hp, 1, std::max(1, enemy->maxHp));
    return true;
}

int EnemySystem::consumePendingXp()
{
    const int xp = pendingXp_;
    pendingXp_ = 0;
    return xp;
}

void EnemySystem::clearTemporaryState()
{
    events_.clear();
    impactSoundEvents_.clear();
    soundEvents_.clear();
    statusPopupEvents_.clear();
    pendingXp_ = 0;
    mudZones_.clear();
    windPulses_.clear();
    mudDamageAccumulator_ = 0.0;
    const auto clearEnemyTemporaryState = [&](Enemy& enemy) {
        if (!enemy.active) {
            return;
        }
        enemy.status = EntityStatus{};
        enemy.poisonDamageAccumulator = 0.0;
        enemy.hotDamageAccumulator = 0.0;
        enemy.bleedDamageAccumulator = 0.0;
        enemy.stunWakeTimer = 0.0f;
        enemy.hitFlash = 0.0f;
        enemy.hpBarTimer = 0.0f;
        enemy.knockbackVelocity = {};
        enemy.knockbackTimer = 0.0f;
        enemy.death = {};
        enemy.fleeNavigation = {};
        clearExternalBounceState(enemy);
        enemy.contactTimer = 0.0f;
    };
    for (Enemy& enemy : enemies_.items()) {
        clearEnemyTemporaryState(enemy);
    }
    for (Enemy& enemy : dormantEnemies_) {
        clearEnemyTemporaryState(enemy);
    }
}

std::string EnemySystem::debugEnemySummary(Vec2 playerPosition) const
{
    std::ostringstream out;
    int shown = 0;
    for (const Enemy& enemy : enemies_.items()) {
        if (!enemy.active) {
            continue;
        }
        if (shown > 0) {
            out << "\n";
        }
        const EnemyImageDrawOptions drawOptions = enemyImageOptionsFor(enemy);
        const EnemyImageDebugInfo drawDebug = enemyImageDebugInfo(enemy, enemy.behaviorTimer, drawOptions);
        const Vec2 toPlayer = playerPosition - enemy.position;
        const float distanceToPlayer = length(toPlayer);
        EnemyImageDebugInfo toPlayerDebug;
        if (distanceToPlayer > 0.001f) {
            EnemyImageDrawOptions toPlayerOptions = drawOptions;
            toPlayerOptions.directionOverrideEnabled = true;
            toPlayerOptions.directionOverride = toPlayer;
            toPlayerDebug = enemyImageDebugInfo(enemy, enemy.behaviorTimer, toPlayerOptions);
        }
        const float speed = length(enemy.velocity);
        EnemyImageDebugInfo velocityDebug;
        if (speed > 0.001f) {
            EnemyImageDrawOptions velocityOptions = drawOptions;
            velocityOptions.directionOverrideEnabled = true;
            velocityOptions.directionOverride = enemy.velocity;
            velocityDebug = enemyImageDebugInfo(enemy, enemy.behaviorTimer, velocityOptions);
        }
        const bool actionLocksFacing = enemy.action.active && enemy.action.lockFacing;
        const char* runtimeState = "normal";
        if (enemy.death.active) {
            runtimeState = "death";
        } else if (enemy.spawnTimer > 0.0f) {
            runtimeState = "spawn";
        } else if (enemy.knockbackTimer > 0.0f) {
            runtimeState = "knock";
        } else if (enemy.jumpActive) {
            runtimeState = "jump";
        }
        const Vec2 debugFacingVector = drawDebug.valid ? drawDebug.facingVector : facingVector(enemy.facingAngle);
        const float debugFacingDegrees = drawDebug.valid
            ? drawDebug.facingAngleDegrees
            : enemy.facingAngle * (180.0f / Pi);
        const float faceToPlayerDiff = distanceToPlayer > 0.001f
            ? angleBetweenDegrees(debugFacingVector, toPlayer)
            : 0.0f;

        out << (enemy.enemyName.empty() ? enemy.enemyId : enemy.enemyName)
            << " id=" << enemy.enemyId
            << " hp=" << enemy.hp << "/" << enemy.maxHp
            << " aware=" << debugAwarenessName(enemy.awareness)
            << " ai=" << currentEnemyAiIdForDebug(enemy)
            << " state=" << runtimeState
            << " act=" << activeEnemyActionNameForDebug(enemy)
            << " lockF=" << debugYesNo(actionLocksFacing);
        if (drawDebug.valid) {
            out << "\n  draw=" << drawDebug.directionName
                << " row=" << drawDebug.frameRow << "/" << drawDebug.sheetRows
                << " col=" << drawDebug.frameColumn << "/" << drawDebug.sheetColumns
                << " src=" << drawDebug.directionSource
                << " motion=" << drawDebug.motionName
                << " ov=" << debugYesNo(drawDebug.directionOverrideEnabled);
        } else {
            out << "\n  draw=invalid";
        }
        out << " face=" << roundedDebugFloat(wrapDebugDegrees(debugFacingDegrees))
            << "(" << (drawDebug.valid ? drawDebug.facingDirectionName : "?") << ")"
            << " toP=" << (toPlayerDebug.valid ? toPlayerDebug.directionName : "-")
            << " d=" << roundedDebugFloat(distanceToPlayer)
            << " diff=" << roundedDebugFloat(faceToPlayerDiff)
            << " vel=" << (velocityDebug.valid ? velocityDebug.directionName : "-")
            << " spd=" << roundedDebugFloat(speed);
        ++shown;
        if (shown >= 4) {
            break;
        }
    }
    if (!dormantEnemies_.empty()) {
        if (shown > 0) {
            out << "\n";
        }
        out << "休眠敵 count=" << dormantEnemies_.size();
        ++shown;
    }
    if (shown == 0) {
        return "no active enemies";
    }
    return out.str();
}

Enemy* EnemySystem::findCaptureTarget(Vec2 targetWorld)
{
    Enemy* best = nullptr;
    float bestDistanceSq = std::numeric_limits<float>::max();
    for (Enemy& enemy : enemies_.items()) {
        if (!enemyCanBeHit(enemy)) {
            continue;
        }
        const Vec2 targetCenter = enemy.position + enemyVisualOffset(enemy, placementCatalog_);
        const float targetRadius = std::max(CaptureTargetMinRadius, enemyHitboxBoundsRadius(enemy, hitboxCatalog_) + CaptureTargetPadding);
        const float targetDistanceSq = distanceSquared(targetCenter, targetWorld);
        if (targetDistanceSq <= targetRadius * targetRadius && targetDistanceSq <= bestDistanceSq) {
            bestDistanceSq = targetDistanceSq;
            best = &enemy;
        }
    }
    return best;
}

Enemy* EnemySystem::findRuntimeEnemy(int runtimeId)
{
    if (runtimeId <= 0) {
        return nullptr;
    }
    for (Enemy& enemy : enemies_.items()) {
        if (enemy.active && enemy.id == runtimeId) {
            return &enemy;
        }
    }
    return nullptr;
}

const Enemy* EnemySystem::findRuntimeEnemy(int runtimeId) const
{
    if (runtimeId <= 0) {
        return nullptr;
    }
    for (const Enemy& enemy : enemies_.items()) {
        if (enemy.active && enemy.id == runtimeId) {
            return &enemy;
        }
    }
    return nullptr;
}

const Enemy* EnemySystem::findCaptureTarget(Vec2 targetWorld) const
{
    const Enemy* best = nullptr;
    float bestDistanceSq = std::numeric_limits<float>::max();
    for (const Enemy& enemy : enemies_.items()) {
        if (!enemyCanBeHit(enemy)) {
            continue;
        }
        const Vec2 targetCenter = enemy.position + enemyVisualOffset(enemy, placementCatalog_);
        const float targetRadius = std::max(CaptureTargetMinRadius, enemyHitboxBoundsRadius(enemy, hitboxCatalog_) + CaptureTargetPadding);
        const float targetDistanceSq = distanceSquared(targetCenter, targetWorld);
        if (targetDistanceSq <= targetRadius * targetRadius && targetDistanceSq <= bestDistanceSq) {
            bestDistanceSq = targetDistanceSq;
            best = &enemy;
        }
    }
    return best;
}

Enemy* EnemySystem::findCaptureTargetInDirection(Vec2 origin, Vec2 direction)
{
    return const_cast<Enemy*>(std::as_const(*this).findCaptureTargetInDirection(origin, direction));
}

const Enemy* EnemySystem::findCaptureTargetInDirection(Vec2 origin, Vec2 direction) const
{
    if (lengthSquared(direction) <= 0.0001f) {
        return nullptr;
    }

    const Vec2 aim = normalize(direction);
    const Enemy* best = nullptr;
    float bestScore = std::numeric_limits<float>::max();
    for (const Enemy& enemy : enemies_.items()) {
        if (!enemyCanBeHit(enemy)) {
            continue;
        }
        const Vec2 targetCenter = enemy.position + enemyVisualOffset(enemy, placementCatalog_);
        const Vec2 toEnemy = targetCenter - origin;
        const float along = dot(toEnemy, aim);
        if (along < 0.0f || along > CaptureReach) {
            continue;
        }

        const float distanceSq = lengthSquared(toEnemy);
        const float perpendicularSq = std::max(0.0f, distanceSq - along * along);
        const float targetRadius = std::max(CaptureTargetMinRadius, enemyHitboxBoundsRadius(enemy, hitboxCatalog_) + CaptureTargetPadding);
        if (perpendicularSq > targetRadius * targetRadius) {
            continue;
        }

        const float score = along + std::sqrt(perpendicularSq) * 0.35f;
        if (score < bestScore) {
            bestScore = score;
            best = &enemy;
        }
    }
    return best;
}

Enemy* EnemySystem::findActiveEnemyNear(Vec2 position, float radius)
{
    Enemy* best = nullptr;
    float bestDistanceSq = std::max(0.0f, radius);
    bestDistanceSq *= bestDistanceSq;
    for (Enemy& enemy : enemies_.items()) {
        if (!enemyVisible(enemy) || enemy.death.active) {
            continue;
        }
        const float targetDistanceSq = distanceSquared(enemy.position, position);
        if (targetDistanceSq <= bestDistanceSq) {
            bestDistanceSq = targetDistanceSq;
            best = &enemy;
        }
    }
    return best;
}

CaptureTargetPreview EnemySystem::previewCaptureTarget(
    const Enemy* target,
    const Player& player,
    bool allowBossCapture,
    std::string_view bossCaptureObjectId) const
{
    if (target == nullptr) {
        return {};
    }

    CaptureTargetPreview preview{
        .enemyRuntimeId = target->id,
        .blockedReason = CaptureResultType::Failed,
        .challengeable = true,
        .position = target->position,
    };
    if (distanceSquared(player.position, target->position) > CaptureReach * CaptureReach) {
        preview.blockedReason = CaptureResultType::OutOfRange;
        preview.challengeable = false;
        return preview;
    }
    if (target->isBoss && !allowBossCapture) {
        preview.blockedReason = bossCaptureObjectId.empty()
            ? CaptureResultType::BossLocked
            : CaptureResultType::BossAlreadyOwned;
        preview.challengeable = false;
        return preview;
    }
    return preview;
}

CaptureTargetPreview EnemySystem::previewCaptureAt(
    Vec2 targetWorld,
    const Player& player,
    bool allowBossCapture,
    std::string_view bossCaptureObjectId) const
{
    return previewCaptureTarget(findCaptureTarget(targetWorld), player, allowBossCapture, bossCaptureObjectId);
}

CaptureTargetPreview EnemySystem::previewCaptureInDirection(
    Vec2 origin,
    Vec2 direction,
    const Player& player,
    bool allowBossCapture,
    std::string_view bossCaptureObjectId) const
{
    return previewCaptureTarget(findCaptureTargetInDirection(origin, direction), player, allowBossCapture, bossCaptureObjectId);
}

CaptureResult EnemySystem::tryCaptureTarget(
    Enemy* best,
    Player& player,
    SpellRingSystem& spellRing,
    InventorySystem& /*inventory*/,
    bool allowBossCapture,
    std::string_view bossCaptureObjectId,
    const CaptureAttemptOptions& options)
{
    if (best == nullptr) {
        return {};
    }

    const float chance = captureChanceFor(*best, options.chanceMultiplier);
    CaptureResult result{
        .type = CaptureResultType::Failed,
        .enemyName = best->enemyName,
        .chance = chance,
        .position = best->position,
    };
    if (options.requirePlayerReach && distanceSquared(player.position, best->position) > CaptureReach * CaptureReach) {
        result.type = CaptureResultType::OutOfRange;
        return result;
    }
    if (best->isBoss && !allowBossCapture) {
        result.type = bossCaptureObjectId.empty()
            ? CaptureResultType::BossLocked
            : CaptureResultType::BossAlreadyOwned;
        return result;
    }
    if (options.allowedEnemyIds != nullptr) {
        const std::string& enemyId = best->enemyId;
        if (enemyId.empty() || options.allowedEnemyIds->find(enemyId) == options.allowedEnemyIds->end()) {
            result.type = CaptureResultType::KnowledgeLocked;
            return result;
        }
    }

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    if (dist(rng_) > chance) {
        return result;
    }

    ItemData capturedItem = makeCapturedItemData(*best);
    if (best->isBoss) {
        if (!bossCaptureObjectId.empty()) {
            capturedItem.id = std::string(bossCaptureObjectId);
        }
        capturedItem.tags.push_back("captured_boss");
    }

    Enemy capturedEnemy = *best;

    if (best->isBoss) {
        finishEnemyDeath(*best, spellRing);
    } else {
        best->active = false;
    }
    result.type = CaptureResultType::Success;
    result.objectId = capturedItem.id;
    result.capturedItem = std::move(capturedItem);
    result.capturedEnemy = std::move(capturedEnemy);
    return result;
}

CaptureResult EnemySystem::tryCaptureAt(
    Vec2 targetWorld,
    Player& player,
    SpellRingSystem& spellRing,
    InventorySystem& inventory,
    bool allowBossCapture,
    std::string_view bossCaptureObjectId)
{
    return tryCaptureTarget(
        findCaptureTarget(targetWorld),
        player,
        spellRing,
        inventory,
        allowBossCapture,
        bossCaptureObjectId,
        CaptureAttemptOptions{});
}

CaptureResult EnemySystem::tryCaptureInDirection(
    Vec2 origin,
    Vec2 direction,
    Player& player,
    SpellRingSystem& spellRing,
    InventorySystem& inventory,
    bool allowBossCapture,
    std::string_view bossCaptureObjectId)
{
    return tryCaptureTarget(
        findCaptureTargetInDirection(origin, direction),
        player,
        spellRing,
        inventory,
        allowBossCapture,
        bossCaptureObjectId,
        CaptureAttemptOptions{});
}

}
