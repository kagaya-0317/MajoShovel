#include "game/EnemySystem.hpp"

#include "data/GameBalance.hpp"
#include "engine/Log.hpp"
#include "engine/Ui.hpp"
#include "game/ActorVisual.hpp"
#include "game/Collision.hpp"
#include "game/EnemyImageRenderer.hpp"
#include "game/EffectSystem.hpp"
#include "game/WorldDropSystem.hpp"
#include "data/StageWeight.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <queue>
#include <random>
#include <sstream>
#include <utility>
#include <string_view>

namespace majo {

namespace {

constexpr int FlowRadiusTiles = 80;
constexpr float SpawnAvoidancePadding = 5.0f;
constexpr float PlayerPushShare = 0.35f;
constexpr float EnemyPushShare = 0.65f;
constexpr float BossRadiusMultiplier = 1.0f;
constexpr float BossVisualRadiusMultiplier = 1.35f;
constexpr int BossHpMultiplier = 8;
constexpr int BossXpMultiplier = 6;
constexpr float BossMinSpawnDistance = 96.0f;
constexpr std::string_view DefaultEnemyId = "default_enemy";
constexpr float KeepDistanceMin = 130.0f;
constexpr float KeepDistanceMax = 210.0f;
constexpr float CaptureRange = 58.0f;
constexpr float CaptureForwardOffset = 42.0f;
constexpr std::string_view DefaultEnemyName = "敵";
constexpr float CapturedRewardChanceEnemy = 0.10f;
constexpr float CapturedStealChanceEnemy = 0.12f;
constexpr float CapturedRewardCooldown = 0.80f;
constexpr float CapturedRewardWindowSeconds = 10.0f;
constexpr int CapturedRewardWindowLimit = 3;
constexpr int CapturedBossRewardLimit = 2;
constexpr int CapturedExplosionChargeLimit = 4;
constexpr float CapturedExplosionSleepSeconds = 2.4f;
constexpr float CapturedExplosionRadius = 44.0f;
constexpr float CapturedMagnetEnemyRadius = 160.0f;
constexpr float CapturedMagnetEnemyPullSpeed = 58.0f;
constexpr int CapturedMagnetEnemyLimit = 4;
constexpr float DefaultVisionDistance = 120.0f;
constexpr float DefaultVisionAngle = 100.0f;
constexpr float DefaultLoseSightSeconds = 1.5f;
constexpr float AwarenessIconDuration = 0.75f;
constexpr float LineOfSightStep = static_cast<float>(balance::TileSize) * 0.35f;
constexpr float WanderRetargetMin = 0.9f;
constexpr float WanderRetargetMax = 1.8f;
constexpr float PatrolRetargetMin = 1.8f;
constexpr float PatrolRetargetMax = 3.2f;
constexpr float PatrolRadius = 120.0f;
constexpr float ItemSeekRadius = 240.0f;
constexpr float DigActionInterval = 0.11f;
constexpr float SwarmAlertRadius = 180.0f;
constexpr float JumpLandingBuffSeconds = 0.24f;
constexpr float JumpAttackDurationMin = 0.12f;
constexpr float JumpAttackDurationMax = 0.65f;
constexpr float JumpAttackDefaultDuration = 0.28f;
constexpr float JumpAttackDefaultArcHeight = 24.0f;
constexpr float JumpTargetMinDistance = 4.0f;
constexpr float HoverChaseAltitude = 18.0f;
constexpr float HoverKeepDistanceAltitude = 20.0f;
constexpr float PhaseAltitude = 12.0f;
constexpr float HoverBobAmplitude = 3.0f;
constexpr float PhaseBobAmplitude = 2.0f;
constexpr float HoverBobSpeed = 4.0f;
constexpr float PhaseBobSpeed = 5.0f;
constexpr float EnemyHpBarDisplaySeconds = 2.0f;
constexpr float EnemyHpBarFadeSeconds = 0.35f;
constexpr float EnemyHpBarHeight = 4.0f;
constexpr float EnemyHpBarMinWidth = 24.0f;
constexpr float EnemyHpBarMaxWidth = 42.0f;
constexpr float MudZoneTickSeconds = 0.20f;
constexpr float MudZoneMaxDurationSeconds = 30.0f;
constexpr float MagnetDisturbMaxRadius = 320.0f;
constexpr int SwarmSpawnCountMax = 6;
constexpr int FlowOrthogonalCost = 10;
constexpr int FlowDiagonalCost = 14;

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

bool isExcludedFromNormalDugSpawn(const EnemyDefinition& definition)
{
    if (hasTagAscii(definition, {"boss", "boss_only", "no_normal_spawn", "event_only", "fixed_only"})) {
        return true;
    }
    for (const std::string& tag : definition.enemyTags) {
        if (tag == "ボス" || tag == "通常スポーン除外" || tag == "固定配置専用" || tag == "イベント専用") {
            return true;
        }
    }
    if (containsAnyAscii(definition.id, {
            "boss",
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

int applyDefenseModifier(const EntityStatus& status, int damage)
{
    if (damage <= 0) {
        return 0;
    }

    const double defense = std::max(0.05, status.multiplierFor(ModifierStat::Defense));
    return std::max(0, static_cast<int>(std::ceil(static_cast<double>(damage) / defense)));
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
        behaviorId == "throw_object" ||
        behaviorId == "throw_stone" ||
        behaviorId == "shoot_poison" ||
        behaviorId == "shoot_web" ||
        behaviorId == "shoot_fire" ||
        behaviorId == "shoot_paralyze" ||
        behaviorId == "shoot_mud" ||
        behaviorId == "radial_spike" ||
        behaviorId == "shoot_water" ||
        behaviorId == "wind_blow" ||
        behaviorId == "enemy_heal" ||
        behaviorId == "countdown_explode";
}

bool hasBehavior(const Enemy& enemy, std::string_view behaviorId)
{
    return std::any_of(enemy.behaviorIds.begin(), enemy.behaviorIds.end(), [behaviorId](const std::string& value) {
        return value == behaviorId;
    });
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

bool hasEnemyTag(const Enemy& enemy, std::string_view tag)
{
    return std::any_of(enemy.enemyTags.begin(), enemy.enemyTags.end(), [tag](const std::string& value) {
        return value == tag;
    });
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
        behaviorId == "wind_blow";
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
    if (behaviorId == "wind_blow") {
        return "wind_wave";
    }
    return "stone_bullet";
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

bool findJumpLandingPosition(TileMap& map, const Enemy& enemy, Vec2 direction, float distance, Vec2& outPosition)
{
    if (lengthSquared(direction) <= 0.0001f || distance < JumpTargetMinDistance) {
        return false;
    }

    const Vec2 normalizedDirection = normalize(direction);
    constexpr std::array<float, 8> DistanceFactors{{1.0f, 0.88f, 0.76f, 0.64f, 0.52f, 0.40f, 0.28f, 0.16f}};
    for (float factor : DistanceFactors) {
        const Vec2 candidate = enemy.position + normalizedDirection * (distance * factor);
        if (!map.isCircleBlocked(candidate, enemy.radius)) {
            outPosition = candidate;
            return true;
        }
    }
    return false;
}

bool beginEnemyJump(Enemy& enemy, TileMap& map, Vec2 direction, float distance, float durationSeconds, float arcHeight)
{
    Vec2 target{};
    if (!findJumpLandingPosition(map, enemy, direction, distance, target)) {
        return false;
    }

    enemy.jumpActive = true;
    enemy.jumpStartPosition = enemy.position;
    enemy.jumpTargetPosition = target;
    enemy.jumpElapsedSeconds = 0.0f;
    enemy.jumpDurationSeconds = clamp(durationSeconds, JumpAttackDurationMin, JumpAttackDurationMax);
    enemy.jumpArcHeight = std::max(0.0f, arcHeight);
    enemy.velocity = {};
    return true;
}

bool frontGuardApplies(const Enemy& enemy, Vec2 hitPosition, float arcDegrees)
{
    const Vec2 facing = facingVector(enemy.facingAngle);
    const Vec2 hitDirection = normalize(hitPosition - enemy.position);
    const float clampedArc = clamp(arcDegrees, 10.0f, 360.0f);
    const float halfArcRadians = (clampedArc * 0.5f) * (Pi / 180.0f);
    const float threshold = std::cos(halfArcRadians);
    return dot(facing, hitDirection) >= threshold;
}

float captureChanceFor(const Enemy& enemy)
{
    const float hpRate = enemy.maxHp > 0
        ? clamp(static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHp), 0.0f, 1.0f)
        : 1.0f;
    const int difficulty = enemy.definition != nullptr ? std::max(0, enemy.definition->captureDifficulty) : 0;
    return clamp(0.15f + (1.0f - hpRate) * 0.75f - static_cast<float>(difficulty) * 0.04f, 0.05f, 0.90f);
}

std::string capturedObjectIdFor(const Enemy& enemy)
{
    return "captured_" + (enemy.enemyId.empty() ? std::string(DefaultEnemyId) : enemy.enemyId);
}

ItemData makeCapturedItemData(const Enemy& enemy)
{
    ItemData item;
    item.id = capturedObjectIdFor(enemy);
    item.name = enemy.enemyName;
    item.category = "\xE8\xBB\x8C\xE9\x81\x93";
    item.rarity = 1;
    item.price = 0;

    if (enemy.definition == nullptr) {
        item.damageType = "none";
        return item;
    }

    const EnemyDefinition& definition = *enemy.definition;
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
                effect == "status_bleed" || effect == "status_bleed_chance") {
                return effect;
            }
        }
    }
    if (!damageType.empty() && damageType != "none") {
        return std::string(damageType);
    }
    return {};
}

EnemyEvent makeEnemyEvent(EnemyEventType type, const Enemy& enemy, std::string effectId = {}, int damageAmount = -1)
{
    return EnemyEvent{
        .type = type,
        .position = enemy.position,
        .enemyId = enemy.enemyId,
        .enemyName = enemy.enemyName,
        .effectId = std::move(effectId),
        .damageAmount = damageAmount,
        .moneyDrop = enemy.moneyDrop,
    };
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

void dispatchCapturedContactEffect(
    const SpellRingItem& item,
    const ObjectDefinition& object,
    Enemy& enemy,
    Player& player,
    SpellRingSystem& spellRing,
    const EffectDispatcher& effectDispatcher,
    Vec2 hitPosition,
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    EffectContext context;
    context.sourceObject = &object;
    context.owner = &player;
    context.targetEntity = &enemy;
    context.hitTarget = &enemy;
    context.orbit = &spellRing;
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

    if (item.hasCapturedBehavior("rust_debuff") && !effectSpecsContain(object.orbitEffects, "debuff_defense")) {
        const double defenseMultiplier = clamp(item.capturedBehaviorParamDouble("rust_debuff", "defenseMultiplier", 0.8), 0.05, 1.0);
        const double debuffDuration = std::max(0.1, item.capturedBehaviorParamDouble("rust_debuff", "duration", 4.0));
        EffectSpec rust;
        rust.target = "enemy";
        rust.effects = {"debuff_defense"};
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

void recordCapturedReward(SpellRingItem& item, const Enemy& enemy, float totalTime, Vec2 position, std::vector<EnemyEvent>& events)
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
    events.push_back({EnemyEventType::RewardDrop, position});
}

void tryCapturedRewardFromEnemy(SpellRingItem& item, const Enemy& enemy, float totalTime, std::vector<EnemyEvent>& events)
{
    float chance = 0.0f;
    if (item.hasCapturedBehavior("steal_or_dig")) {
        chance = static_cast<float>(std::clamp(item.capturedBehaviorParamDouble("steal_or_dig", "chance", CapturedStealChanceEnemy), 0.0, 1.0));
    } else if (item.hasCapturedBehavior("reward_drop")) {
        chance = static_cast<float>(std::clamp(item.capturedBehaviorParamDouble("reward_drop", "chance", CapturedRewardChanceEnemy), 0.0, 1.0));
    } else {
        return;
    }

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

Vec2 enemyDrawPosition(const Enemy& enemy)
{
    return elevatedDrawPosition(enemy.position, enemy.altitude);
}

EnemyImageDrawOptions enemyImageOptionsFor(const Enemy& enemy)
{
    EnemyImageDrawOptions imageOptions;
    imageOptions.tint = {255, 255, 255, 255};
    if (enemy.hitFlash > 0.0f) {
        const float flash = clamp(enemy.hitFlash / 0.12f, 0.0f, 1.0f);
        imageOptions.maskOverlayColor = {255, 255, 255, static_cast<unsigned char>(std::round(220.0f * flash))};
    }
    if (enemy.hitFlash <= 0.0f && enemy.status.hasState("status_poison")) {
        imageOptions.tint = {160, 255, 160, 255};
    } else if (enemy.hitFlash <= 0.0f && enemy.status.hasState("status_slow")) {
        imageOptions.tint = {160, 190, 255, 255};
    }
    imageOptions.outlineColor = enemy.isBoss ? Color{255, 210, 96, 255} : Color{80, 18, 28, 255};
    return imageOptions;
}

Vec2 enemyVisualBoundsSize(Renderer& renderer, const Enemy& enemy)
{
    if (enemy.spawnTimer > 0.0f) {
        const float ratio = enemy.spawnDuration > 0.0f ? enemy.spawnTimer / enemy.spawnDuration : 0.0f;
        const float pulse = 1.0f + (1.0f - ratio) * 0.9f;
        const float visualRadius = enemy.isBoss ? enemy.radius * BossVisualRadiusMultiplier : enemy.radius;
        const float diameter = (visualRadius * pulse + 4.0f) * 2.0f;
        return {diameter, diameter};
    }

    Vec2 imageSize{};
    if (enemyImageDrawSize(renderer, enemy, enemyImageOptionsFor(enemy), imageSize)) {
        return imageSize;
    }

    const float visualRadius = enemy.isBoss ? enemy.radius * BossVisualRadiusMultiplier : enemy.radius;
    const float diameter = (visualRadius + 3.0f) * 2.0f;
    return {diameter, diameter};
}

float enemyShadowVisualSize(Renderer& renderer, const Enemy& enemy)
{
    const Vec2 visualSize = enemyVisualBoundsSize(renderer, enemy);
    const float baseSize = std::max(1.0f, std::max(visualSize.x, visualSize.y));
    return actorShadowVisualSizeForAltitude(baseSize, enemy.altitude);
}

Vec2 enemyShadowBoundsSize(Renderer& renderer, const Enemy& enemy)
{
    const float visualSize = enemyShadowVisualSize(renderer, enemy);
    return {visualSize * 0.55f, visualSize * 0.25f};
}

void drawEnemyShadow(Renderer& renderer, const Enemy& enemy)
{
    renderer.drawActorShadow(actorShadowAnchor(enemy.position, EnemyShadowGroundOffsetY), enemyShadowVisualSize(renderer, enemy));
}

void drawEnemyHpBar(Renderer& renderer, const Enemy& enemy, Vec2 drawPosition, float uiVisualRadius)
{
    if (enemy.isBoss || enemy.hpBarTimer <= 0.0f || enemy.maxHp <= 0 || enemy.hp <= 0 || enemy.hp >= enemy.maxHp) {
        return;
    }

    const float fade = clamp(enemy.hpBarTimer / EnemyHpBarFadeSeconds, 0.0f, 1.0f);
    if (fade <= 0.0f) {
        return;
    }

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
    hpGaugeStyle.capGlow = scaleColorAlpha({255, 112, 112, 44}, fade);
    hpGaugeStyle.capCore = {255, 240, 220, 0};
    hpGaugeStyle.outline = scaleColorAlpha({255, 255, 255, 255}, fade);
    hpGaugeStyle.trackOuterExtra = 1.0f;
    hpGaugeStyle.trackInnerInset = 1.5f;
    hpGaugeStyle.shadowOffsetY = 1.0f;
    hpGaugeStyle.shadowExtra = 2.0f;

    drawUiGauge(renderer, {barPos, {barWidth, EnemyHpBarHeight}}, hpRatio, hpGaugeStyle);
}

void drawEnemyVisual(Renderer& renderer, const Enemy& enemy)
{
    const Vec2 drawPosition = enemyDrawPosition(enemy);
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
        const float pulse = 1.0f + (1.0f - ratio) * 0.9f;
        const float visualRadius = enemy.isBoss ? enemy.radius * BossVisualRadiusMultiplier : enemy.radius;
        const Color spawnColor = enemy.isBoss ? Color{255, 180, 80, 230} : colorForEnemy(enemy);
        renderer.drawCircle(drawPosition, visualRadius * pulse + 4.0f, spawnColor);
        renderer.drawCircle(drawPosition, visualRadius * 0.55f, enemy.isBoss ? Color{255, 232, 140, 210} : Color{255, 160, 110, 190});
        drawAwarenessIcon(visualRadius);
        return;
    }
    Color color = enemy.hitFlash > 0.0f ? Color{255, 255, 255, 255} : (enemy.isBoss ? Color{142, 46, 160, 255} : colorForEnemy(enemy));
    if (enemy.hitFlash <= 0.0f && enemy.status.hasState("status_poison")) {
        color = {92, 184, 88, 255};
    } else if (enemy.hitFlash <= 0.0f && enemy.status.hasState("status_slow")) {
        color = {76, 132, 218, 255};
    }
    const float visualRadius = enemy.isBoss ? enemy.radius * BossVisualRadiusMultiplier : enemy.radius;
    const EnemyImageDrawOptions imageOptions = enemyImageOptionsFor(enemy);
    Vec2 enemyImageDrawSize{};
    const bool drewImage = drawEnemyImage(renderer, enemy, drawPosition, enemy.behaviorTimer, imageOptions, &enemyImageDrawSize);
    const float uiVisualRadius = drewImage ? std::max(visualRadius, enemyImageDrawSize.y * 0.5f) : visualRadius;
    if (!drewImage) {
        renderer.fillCircle(drawPosition, visualRadius, color);
        renderer.drawCircle(drawPosition, visualRadius + 3.0f, enemy.isBoss ? Color{255, 210, 96, 255} : Color{80, 18, 28, 255});
    }
    drawEnemyHpBar(renderer, enemy, drawPosition, uiVisualRadius);
    if (enemy.isBoss) {
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
        bossHpGaugeStyle.capGlow = {255, 190, 80, 46};
        bossHpGaugeStyle.capCore = {255, 248, 210, 0};
        bossHpGaugeStyle.trackOuterExtra = 1.0f;
        bossHpGaugeStyle.trackInnerInset = 2.0f;
        bossHpGaugeStyle.shadowOffsetY = 1.0f;
        bossHpGaugeStyle.shadowExtra = 3.0f;
        drawUiGauge(renderer, {barPos, {56.0f, 5.0f}}, hpRatio, bossHpGaugeStyle);
    }
    drawAwarenessIcon(uiVisualRadius);
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
    candidates.reserve(enemyCatalog.enemies.size());
    for (const EnemyDefinition& definition : enemyCatalog.enemies) {
        if (!isExcludedFromNormalDugSpawn(definition)) {
            candidates.push_back(&definition);
        }
    }

    if (candidates.empty()) {
        logSpawnWeightFallbackOnce(
            "normal_random_no_candidates",
            "Enemy spawn weight fallback: no non-boss Enemies candidates; using legacy all-enemies random");
        return chooseEnemyDefinition(enemyCatalog);
    }

    std::uniform_int_distribution<std::size_t> dist(0, candidates.size() - 1);
    return candidates[dist(rng_)];
}

const EnemyDefinition* EnemySystem::chooseDugSpawnEnemyDefinition(const EnemyCatalog& enemyCatalog, std::string_view stageId, int depthRank)
{
    if (enemyCatalog.enemies.empty()) {
        logSpawnWeightFallbackOnce(
            "empty_catalog",
            "Enemy spawn weight fallback: EnemyCatalog is empty; using default runtime enemy");
        return nullptr;
    }

    const std::string columnName = resolveEnemySpawnWeightColumnName(stageId, depthRank);
    if (columnName.empty()) {
        logSpawnWeightFallbackOnce(
            "unknown_stage:" + std::string(stageId),
            "Enemy spawn weight fallback: unknown stageId=\"" + std::string(stageId) + "\"; using simple random");
        return chooseNormalRandomEnemyDefinition(enemyCatalog);
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
        candidates.push_back(&definition);
        weights.push_back(weightIt->second);
    }

    if (candidates.empty()) {
        logSpawnWeightFallbackOnce(
            "no_candidates:" + columnName,
            "Enemy spawn weight fallback: no candidates for column=\"" + columnName + "\"; using simple random");
        return chooseNormalRandomEnemyDefinition(enemyCatalog);
    }

    const auto selected = selectWeightedIndex(weights, rng_);
    if (!selected || *selected >= candidates.size()) {
        logSpawnWeightFallbackOnce(
            "select_failed:" + columnName,
            "Enemy spawn weight fallback: weighted selection failed for column=\"" + columnName + "\"; using simple random");
        return chooseNormalRandomEnemyDefinition(enemyCatalog);
    }
    return candidates[*selected];
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
}

void EnemySystem::forceDetectInSight(Enemy& enemy, Vec2 playerPosition, bool showIcon)
{
    if (lengthSquared(playerPosition - enemy.position) > 0.0001f) {
        const Vec2 toPlayer = normalize(playerPosition - enemy.position);
        enemy.facingAngle = std::atan2(toPlayer.y, toPlayer.x);
    }
    setAwareness(enemy, EnemyAwarenessState::Detected, showIcon);
}

void EnemySystem::applyDefinition(Enemy& enemy, const EnemyDefinition* definition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog)
{
    enemy.definition = definition;
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
    enemy.enemyTags.clear();
    enemy.radius = balance.enemyRadius;
    enemy.maxHp = balance.enemyHp + std::max(0, activeCount() / 12);
    enemy.hp = enemy.maxHp;
    enemy.xp = balance.enemyXp;
    enemy.moneyDrop = 0;
    enemy.contactAttackPower = 1;
    enemy.contactDamageType = "blunt";
    enemy.facingAngle = 0.0f;
    enemy.contactDamageMultiplier = 1.0f;
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
    enemy.countdownExplodeDamage = 0;
    enemy.countdownExplodeTerrainDamage = 0;
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
    enemy.stealItemEnabled = false;
    enemy.stealTarget.clear();
    enemy.stealRadius = 0.0f;
    enemy.stealEscapeDistance = 0.0f;
    enemy.stealMaxCarry = 0;
    enemy.stolenMoney = 0;
    enemy.stolenObjectIds.clear();
    enemy.pendingDeath = false;
    enemy.awareness = EnemyAwarenessState::Unaware;
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
        enemy.stealRadius = static_cast<float>(std::max(12.0, behaviorParamDouble(enemy, "steal_item", "radius", 120.0)));
        enemy.stealEscapeDistance = static_cast<float>(std::max(16.0, behaviorParamDouble(enemy, "steal_item", "escapeDistance", 120.0)));
        enemy.stealMaxCarry = std::max(1, behaviorParamInt(enemy, "steal_item", "maxCarry", 2));
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
    if (hasBehavior(enemy, "swarm_spawn")) {
        enemy.swarmSpawnEnabled = true;
        enemy.swarmSpawnExecuted = false;
        enemy.swarmSpawnCount = std::clamp(behaviorParamInt(enemy, "swarm_spawn", "count", 2), 1, SwarmSpawnCountMax);
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
        enemy.countdownExplodeDelay = static_cast<float>(std::max(0.0, behaviorParamDouble(enemy, "countdown_explode", "delay", 2.0)));
        enemy.countdownExplodeDamage = std::max(0, behaviorParamInt(enemy, "countdown_explode", "damage", 3));
        enemy.countdownExplodeTerrainDamage = std::max(0, behaviorParamInt(enemy, "countdown_explode", "terrainDamage", 1));
        enemy.countdownExplodeOnce = behaviorParamInt(enemy, "countdown_explode", "once", 1) != 0;
        enemy.countdownExploded = false;
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

bool EnemySystem::spawnDefinitionAt(Vec2 position, const EnemyDefinition* definition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn, Vec2 detectedTarget)
{
    Enemy* enemy = enemies_.acquire();
    if (!enemy) {
        return false;
    }
    *enemy = Enemy{};
    enemy->active = true;
    enemy->id = nextEnemyId_++;
    enemy->isBoss = false;
    enemy->position = position;
    applyDefinition(*enemy, definition, balance, enemyCatalog);
    enemy->spawnTimer = balance.enemySpawnWarmup;
    enemy->spawnDuration = balance.enemySpawnWarmup;
    if (detectedOnSpawn) {
        forceDetectInSight(*enemy, detectedTarget, true);
    }
    return true;
}

void EnemySystem::spawnAt(Vec2 position, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn, Vec2 detectedTarget)
{
    spawnDefinitionAt(position, chooseEnemyDefinition(enemyCatalog), balance, enemyCatalog, detectedOnSpawn, detectedTarget);
}

bool EnemySystem::spawnBossAt(Vec2 position, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn, Vec2 detectedTarget)
{
    Enemy* enemy = enemies_.acquire();
    if (!enemy) {
        return false;
    }
    *enemy = Enemy{};
    enemy->active = true;
    enemy->id = nextEnemyId_++;
    enemy->isBoss = true;
    enemy->position = position;
    applyDefinition(*enemy, chooseEnemyDefinition(enemyCatalog), balance, enemyCatalog);
    enemy->radius *= BossRadiusMultiplier;
    enemy->maxHp = std::max(12, enemy->maxHp * BossHpMultiplier);
    enemy->hp = enemy->maxHp;
    enemy->xp = std::max(enemy->xp, enemy->xp * BossXpMultiplier);
    enemy->spawnTimer = balance.enemySpawnWarmup * 1.6f;
    enemy->spawnDuration = enemy->spawnTimer;
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
            const float minDistance = enemy.radius + radius + SpawnAvoidancePadding;
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
                    const float minDistance = enemy.radius + radius + SpawnAvoidancePadding;
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

void EnemySystem::spawnFromDugTiles(
    const std::vector<DugEnemySpawnPoint>& dugTiles,
    TileMap& map,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    std::string_view stageId)
{
    if (activeCount() >= balance.enemySoftCap) {
        return;
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
        spawnDefinitionAt(
            spawnPosition,
            chooseDugSpawnEnemyDefinition(enemyCatalog, stageId, spawnPoint.depthRank),
            balance,
            enemyCatalog,
            true,
            playerPosition);
        dugSpawnCounter_ = 0;
        if (activeCount() >= balance.enemySoftCap) {
            return;
        }
    }
}

bool EnemySystem::spawnNodeEnemy(
    TileMap& map,
    Vec2 desiredPosition,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    bool allowNearPlayer,
    bool detectedOnSpawn)
{
    if (activeCount() >= balance.enemySoftCap) {
        return false;
    }

    Vec2 spawnPosition{};
    const float minPlayerDistance = allowNearPlayer ? 0.0f : balance.enemyMinSpawnDistance;
    if (!findSpawnPosition(map, desiredPosition, playerPosition, balance.enemyRadius, minPlayerDistance, spawnPosition)) {
        return false;
    }

    spawnAt(spawnPosition, balance, enemyCatalog, detectedOnSpawn, playerPosition);
    return true;
}

bool EnemySystem::spawnSpecificEnemy(
    TileMap& map,
    std::string_view enemyId,
    Vec2 desiredPosition,
    Vec2 playerPosition,
    const RuntimeBalance& balance,
    const EnemyCatalog& enemyCatalog,
    bool allowNearPlayer,
    bool detectedOnSpawn)
{
    if (activeCount() >= balance.enemySoftCap) {
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

    return spawnDefinitionAt(spawnPosition, &it->second, balance, enemyCatalog, detectedOnSpawn, playerPosition);
}

bool EnemySystem::spawnBoss(TileMap& map, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog)
{
    if (bossActive()) {
        return false;
    }

    Vec2 spawnPosition{};
    if (!findBossSpawnPosition(map, playerPosition, balance, spawnPosition)) {
        return false;
    }

    return spawnBossAt(spawnPosition, balance, enemyCatalog);
}

bool EnemySystem::spawnBossNear(TileMap& map, Vec2 desiredPosition, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog)
{
    if (bossActive()) {
        return false;
    }

    Vec2 spawnPosition{};
    if (!findSpawnPosition(
            map,
            desiredPosition,
            playerPosition,
            balance.enemyRadius * BossRadiusMultiplier,
            0.0f,
            spawnPosition)) {
        return false;
    }

    return spawnBossAt(spawnPosition, balance, enemyCatalog);
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

Vec2 EnemySystem::separationFor(const Enemy& enemy) const
{
    Vec2 separation{};
    for (std::size_t i = 0; i < enemies_.items().size(); ++i) {
        const Enemy& other = enemies_.items()[i];
        if (!other.active || &other == &enemy || other.spawnTimer > 0.0f) {
            continue;
        }

        Vec2 away = enemy.position - other.position;
        const float minDistance = enemy.radius + other.radius + SpawnAvoidancePadding;
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

    Vec2 next = enemy.position + delta;
    if (!map.isCircleBlocked(next, enemy.radius)) {
        enemy.position = next;
        return;
    }

    next = enemy.position + Vec2{delta.x, 0.0f};
    if (!map.isCircleBlocked(next, enemy.radius)) {
        enemy.position = next;
        return;
    }
    next = enemy.position + Vec2{0.0f, delta.y};
    if (!map.isCircleBlocked(next, enemy.radius)) {
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
        if (!map.isCircleBlocked(next, enemy.radius)) {
            enemy.position = next;
            return;
        }
    }
}

void fireEnemyProjectile(Enemy& enemy, ProjectileSystem& projectiles, Vec2 playerPosition)
{
    if (enemy.projectileId.empty() || enemy.rangedBehaviorId.empty()) {
        return;
    }

    ProjectileSpawnTuning tuning;
    tuning.speedMultiplier = std::max(0.05f, enemy.projectileSpeedMultiplier);
    tuning.damageOverride = enemy.projectileDamageOverride;
    tuning.radiusScale = std::max(0.1f, enemy.projectileRadiusScale);

    const Vec2 toPlayer = normalize(playerPosition - enemy.position);
    const Vec2 origin = enemy.position + toPlayer * (enemy.radius + 6.0f);
    if (enemy.rangedBehaviorId == "radial_spike") {
        const int count = std::clamp(enemy.fireVolleyCount, 1, 24);
        for (int i = 0; i < count; ++i) {
            const float angle = (static_cast<float>(i) / static_cast<float>(count)) * Pi * 2.0f;
            projectiles.spawn(
                enemy.projectileId,
                enemy.position + fromAngle(angle) * (enemy.radius + 5.0f),
                fromAngle(angle),
                ProjectileOwnerType::Enemy,
                enemy.projectileEffects,
                tuning);
        }
        return;
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
            projectiles.spawn(enemy.projectileId, enemy.position + dir * (enemy.radius + 6.0f), dir, ProjectileOwnerType::Enemy, enemy.projectileEffects, tuning);
        }
        return;
    }

    projectiles.spawn(enemy.projectileId, origin, toPlayer, ProjectileOwnerType::Enemy, enemy.projectileEffects, tuning);
}

bool EnemySystem::resolvePlayerOverlap(Player& player, Enemy& enemy, TileMap& map, const RuntimeBalance& balance)
{
    Vec2 fromPlayer = enemy.position - player.position;
    const float minimumDistance = enemy.radius + balance.playerRadius;
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
    bool movedPlayer = tryMoveCircle(map, player.position, balance.playerRadius, normal * (-overlap * playerShare));
    bool movedEnemy = tryMoveCircle(map, enemy.position, enemy.radius, normal * (overlap * enemyShare));

    if (!movedPlayer && movedEnemy) {
        tryMoveCircle(map, enemy.position, enemy.radius, normal * (overlap * playerShare));
    } else if (!movedEnemy && movedPlayer) {
        tryMoveCircle(map, player.position, balance.playerRadius, normal * (-overlap * enemyShare));
    } else if (!movedEnemy && !movedPlayer) {
        tryMoveCircle(map, enemy.position, enemy.radius, normal * overlap);
    }

    return true;
}

void EnemySystem::update(
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
    std::vector<EffectDiscoveryEvent>* discoveryEvents,
    const EncyclopediaSystem* encyclopedia)
{
    events_.clear();
    if (paused) {
        return;
    }

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
    for (const MudZone& zone : mudZones_) {
        if (distanceSquared(player.position, zone.position) > zone.radius * zone.radius) {
            continue;
        }
        mudSlowMultiplier = std::min(mudSlowMultiplier, clamp(zone.speedMultiplier, 0.05f, 1.0f));
        mudDamagePerSecond += std::max(0.0f, zone.damagePerSecond);
    }
    if (mudSlowMultiplier < 1.0f) {
        player.status.applyState("status_slow", mudSlowMultiplier, 0.25, "enemy:mud_zone", StateApplyMode::KeepLonger);
    }
    if (mudDamagePerSecond > 0.0) {
        mudDamageAccumulator_ += mudDamagePerSecond * static_cast<double>(dt);
        const int mudDamage = static_cast<int>(std::floor(mudDamageAccumulator_));
        if (mudDamage > 0) {
            player.applyDamage(mudDamage, DamageSource::Poison);
            mudDamageAccumulator_ -= static_cast<double>(mudDamage);
        }
    } else {
        mudDamageAccumulator_ = 0.0;
    }

    const auto processEnemyDeath = [&](Enemy& enemy) {
        if (!enemy.active) {
            return;
        }
        pendingXp_ += enemy.xp;
        if (!enemy.dropItemConsumed) {
            queueEnemyObjectDrops(enemy);
        }
        if (enemy.stolenMoney > 0) {
            EnemyEvent dropMoney = makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy);
            dropMoney.moneyDrop += enemy.stolenMoney;
            enemy.stolenMoney = 0;
            events_.push_back(std::move(dropMoney));
        } else {
            events_.push_back(makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy));
        }
        for (const std::string& objectId : enemy.stolenObjectIds) {
            if (objectId.empty()) {
                continue;
            }
            EnemyEvent objectDrop;
            objectDrop.type = EnemyEventType::ObjectDrop;
            objectDrop.position = enemy.position;
            objectDrop.enemyId = enemy.enemyId;
            objectDrop.enemyName = enemy.enemyName;
            objectDrop.objectDropId = objectId;
            objectDrop.objectDropCount = 1;
            events_.push_back(std::move(objectDrop));
        }
        enemy.stolenObjectIds.clear();
        std::vector<SpellRingItem*> clearItems = spellRing.runtimeItemsMutable();
        for (SpellRingItem* clearItemPtr : clearItems) {
            if (clearItemPtr == nullptr) {
                continue;
            }
            clearItemPtr->unlatchEnemy(enemy.id);
        }
        enemy.active = false;
    };

    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active) {
            continue;
        }
        enemy.status.update(dt);
        const double poisonDps = enemy.status.poisonDamagePerSecond();
        if (poisonDps > 0.0) {
            enemy.poisonDamageAccumulator += poisonDps * static_cast<double>(dt);
            const int poisonDamage = static_cast<int>(std::floor(enemy.poisonDamageAccumulator));
            if (poisonDamage > 0) {
                enemy.hp -= poisonDamage;
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
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
        enemy.hpBarTimer = std::max(0.0f, enemy.hpBarTimer - dt);
        if (enemy.spawnTimer > 0.0f) {
            enemy.spawnTimer = std::max(0.0f, enemy.spawnTimer - dt);
            if (enemy.spawnTimer <= 0.0f && enemy.swarmSpawnEnabled && !enemy.swarmSpawnExecuted) {
                enemy.swarmSpawnExecuted = true;
                const int count = std::clamp(enemy.swarmSpawnCount, 1, SwarmSpawnCountMax);
                for (int i = 0; i < count; ++i) {
                    Enemy* child = enemies_.acquire();
                    if (child == nullptr) {
                        break;
                    }
                    const float angle = (Pi * 2.0f) * (static_cast<float>(i) / static_cast<float>(count));
                    const Vec2 spawnPos = enemy.position + fromAngle(angle) * enemy.swarmSpawnRadius;
                    if (map.isCircleBlocked(spawnPos, enemy.radius)) {
                        child->active = false;
                        continue;
                    }
                    *child = Enemy{};
                    child->active = true;
                    child->id = nextEnemyId_++;
                    child->position = spawnPos;
                    applyDefinition(*child, enemy.definition, balance, EnemyCatalog{});
                    child->spawnTimer = 0.4f;
                    child->spawnDuration = 0.4f;
                    child->swarmSpawnEnabled = false;
                    child->swarmSpawnExecuted = true;
                }
            }
            continue;
        }
        if (enemy.knockbackTimer > 0.0f) {
            enemy.jumpActive = false;
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
        if (enemy.jumpActive) {
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
                enemy.position = enemy.jumpTargetPosition;
                enemy.velocity = {};
                enemy.jumpActive = false;
                enemy.jumpElapsedSeconds = 0.0f;
                enemy.jumpDurationSeconds = 0.0f;
                enemy.jumpArcHeight = 0.0f;
                enemy.jumpLandingBuffTimer = JumpLandingBuffSeconds;
                updateEnemyAltitude(enemy);
                events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy));
            }
            continue;
        }

        Vec2 direction{};
        const Vec2 toPlayer = player.position - enemy.position;
        const float distanceToPlayer = length(toPlayer);
        const Vec2 directToPlayer = distanceToPlayer > 0.0001f ? toPlayer / distanceToPlayer : Vec2{};
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
            const float triggerDistance = enemy.radius + balance.playerRadius + 18.0f;
            return distanceToPlayer <= triggerDistance;
        };
        const auto alertNearbySwarm = [&](const Enemy& source) {
            const float radiusSq = SwarmAlertRadius * SwarmAlertRadius;
            for (Enemy& ally : enemies_.items()) {
                if (!ally.active || ally.id == source.id || ally.spawnTimer > 0.0f) {
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
            if (canDetectPlayer(unawareVisionDistance, unawareVisionAngle) || proximityTriggeredAmbush) {
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
        const std::string_view aiId = enemy.awareness == EnemyAwarenessState::Detected
            ? (enemy.aiId.empty() ? std::string_view("chase") : std::string_view(enemy.aiId))
            : (enemy.unawareAiId.empty() ? std::string_view("idle") : std::string_view(enemy.unawareAiId));
        const auto carriedCount = [&]() {
            return static_cast<int>(enemy.stolenObjectIds.size()) + (enemy.stolenMoney > 0 ? 1 : 0);
        };
        if (enemy.stealItemEnabled && carriedCount() < std::max(1, enemy.stealMaxCarry)) {
            WorldDropItem stolenDrop;
            if (worldDrops.stealNearestDrop(
                    objectCatalog,
                    enemy.position,
                    std::max(8.0f, enemy.stealRadius),
                    enemy.stealTarget,
                    stolenDrop)) {
                if (stolenDrop.kind == WorldDropKind::Money) {
                    enemy.stolenMoney += std::max(0, stolenDrop.quantity);
                } else if (stolenDrop.kind == WorldDropKind::Object && !stolenDrop.id.empty()) {
                    enemy.stolenObjectIds.push_back(stolenDrop.id);
                }
                setAwareness(enemy, EnemyAwarenessState::Detected, false);
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
        if (aiId == "idle" || aiId == "stationary") {
            direction = {};
        } else if (aiId == "buried") {
            direction = {};
        } else if (aiId == "flee") {
            direction = directToPlayer * -1.0f;
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
                if (!ally.active || ally.id == enemy.id || ally.hp >= ally.maxHp) {
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
            if (distanceToPlayer < KeepDistanceMin) {
                direction = directToPlayer * -1.0f;
            } else if (distanceToPlayer > KeepDistanceMax) {
                direction = flowDirectionFor(map, enemy.position, player.position);
            }
        } else {
            direction = flowDirectionFor(map, enemy.position, player.position);
        }
        if (enemy.stealItemEnabled && carriedCount() > 0 && distanceToPlayer < enemy.stealEscapeDistance) {
            direction = directToPlayer * -1.0f;
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
        const float enemySpeed = static_cast<float>(
            enemy.status.applyModifiers(ModifierStat::Speed, baseSpeed) *
            enemy.status.movementMultiplierFromStates() *
            (enemy.isBoss ? 0.78 : 1.0));
        if (aiId == "shield_chase" && lengthSquared(directToPlayer) > 0.0001f) {
            const float targetAngle = std::atan2(directToPlayer.y, directToPlayer.x);
            enemy.facingAngle = rotateTowards(enemy.facingAngle, targetAngle, dt * 1.8f);
            direction = facingVector(enemy.facingAngle);
        }

        enemy.velocity = lengthSquared(direction) > 0.0001f ? normalize(direction) * enemySpeed : Vec2{};
        if (aiId != "stationary" && aiId != "idle" && aiId != "buried") {
            enemy.velocity += separationFor(enemy) * balance.enemySeparationStrength;
        }
        enemy.jumpLandingBuffTimer = std::max(0.0f, enemy.jumpLandingBuffTimer - dt);
        if (!enemy.jumpActive && hasBehavior(enemy, "jump_attack") && enemy.awareness == EnemyAwarenessState::Detected) {
            enemy.jumpAttackTimer = std::max(0.0f, enemy.jumpAttackTimer - dt);
            if (enemy.jumpAttackTimer <= 0.0f && lengthSquared(directToPlayer) > 0.0001f) {
                beginEnemyJump(
                    enemy,
                    map,
                    directToPlayer,
                    std::max(JumpTargetMinDistance, enemy.jumpAttackDistance),
                    enemy.jumpAttackDurationSeconds,
                    enemy.jumpAttackArcHeight);
                enemy.jumpAttackTimer = std::max(0.2f, enemy.jumpAttackIntervalSeconds);
            }
        }
        const float maxSpeed = enemySpeed * 1.75f;
        if (lengthSquared(enemy.velocity) > maxSpeed * maxSpeed) {
            enemy.velocity = normalize(enemy.velocity) * maxSpeed;
        }
        const bool ignoresWallCollision = aiId == "phase_wander" || aiId == "phase_chase";
        const bool digsThroughWall = (aiId == "dig_chase" || aiId == "dig_wander") && hasBehavior(enemy, "dig_move");
        const Vec2 previousPosition = enemy.position;
        if (ignoresWallCollision) {
            enemy.position += enemy.velocity * dt;
        } else if (digsThroughWall) {
            moveWithCollision(enemy, map, enemy.velocity, dt);
            if (distanceSquared(enemy.position, previousPosition) <= 0.0004f &&
                enemy.aiDigTimer <= 0.0f &&
                lengthSquared(enemy.velocity) > 0.0001f) {
                const Vec2 ahead = enemy.position + normalize(enemy.velocity) * (enemy.radius + static_cast<float>(balance::TileSize) * 0.6f);
                const int tx = map.worldToTile(ahead.x);
                const int ty = map.worldToTile(ahead.y);
                Vec2 opened{};
                if (map.damageTile(tx, ty, std::max(1, enemy.digMovePower), opened)) {
                    events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy));
                }
                enemy.aiDigTimer = std::max(0.02f, enemy.digMoveIntervalSeconds);
            }
        } else {
            moveWithCollision(enemy, map, enemy.velocity, dt);
        }

        if ((aiId == "wander" || aiId == "patrol" || aiId == "item_seek" || aiId == "dig_wander") &&
            distanceSquared(enemy.position, previousPosition) <= 0.0004f) {
            enemy.aiDecisionTimer = 0.0f;
            enemy.aiMoveDirection = randomDirection(rng_);
        }

        if (enemy.awareness == EnemyAwarenessState::Detected && lengthSquared(toPlayer) > 0.0001f && aiId != "shield_chase") {
            enemy.facingAngle = std::atan2(toPlayer.y, toPlayer.x);
        } else if (aiId != "stationary" && aiId != "idle" && aiId != "buried" && lengthSquared(enemy.velocity) > 0.0001f) {
            enemy.facingAngle = std::atan2(enemy.velocity.y, enemy.velocity.x);
        }
        if (!enemy.projectileId.empty()) {
            enemy.projectileTimer = std::max(0.0f, enemy.projectileTimer - dt);
            if (enemy.projectileTimer <= 0.0f) {
                fireEnemyProjectile(enemy, projectiles, player.position);
                if (enemy.projectileBurstCount > 1 && enemy.rangedBehaviorId == "shoot_water") {
                    if (enemy.projectileBurstRemaining <= 1) {
                        enemy.projectileBurstRemaining = enemy.projectileBurstCount;
                        enemy.projectileTimer = enemy.projectileInterval > 0.0f ? enemy.projectileInterval : 2.4f;
                    } else {
                        --enemy.projectileBurstRemaining;
                        enemy.projectileTimer = std::max(0.02f, enemy.projectileBurstInterval);
                    }
                } else {
                    enemy.projectileTimer = enemy.projectileInterval > 0.0f ? enemy.projectileInterval : 2.4f;
                }
            }
        }

        if (hasBehavior(enemy, "magnet_disturb") &&
            enemy.magnetStrength > 0.0f &&
            enemy.magnetRadius > 0.0f) {
            const float radius = std::max(8.0f, enemy.magnetRadius);
            const float radiusSq = radius * radius;
            const float force = enemy.magnetStrength * 120.0f * dt;
            for (Enemy& other : enemies_.items()) {
                if (!other.active || other.id == enemy.id) {
                    continue;
                }
                if (!enemy.magnetTargetTag.empty()) {
                    bool matchedTag = false;
                    for (const std::string& tag : other.enemyTags) {
                        if (pipeListContains(enemy.magnetTargetTag, tag)) {
                            matchedTag = true;
                            break;
                        }
                    }
                    if (!matchedTag) {
                        continue;
                    }
                }
                const Vec2 toCenter = enemy.position - other.position;
                const float d2 = lengthSquared(toCenter);
                if (d2 <= 1.0f || d2 > radiusSq) {
                    continue;
                }
                other.velocity += normalize(toCenter) * force;
            }
            if (pipeListContains(enemy.magnetTargetTag, "metal")) {
                projectiles.pullMetalProjectiles(enemy.position, dt * std::max(0.1f, enemy.magnetStrength));
            } else {
                projectiles.deflectEnemyProjectiles(enemy.position, dt * std::max(0.1f, enemy.magnetStrength));
            }
        }

        if (hasBehavior(enemy, "enemy_heal") &&
            enemy.enemyHealRadius > 0.0f &&
            enemy.enemyHealAmount > 0.0f &&
            enemy.enemyHealIntervalSeconds > 0.0f) {
            enemy.enemyHealTimer = std::max(0.0f, enemy.enemyHealTimer - dt);
            if (enemy.enemyHealTimer <= 0.0f) {
                const float radiusSq = enemy.enemyHealRadius * enemy.enemyHealRadius;
                const int healAmount = std::max(1, static_cast<int>(std::round(enemy.enemyHealAmount)));
                for (Enemy& ally : enemies_.items()) {
                    if (!ally.active || ally.hp >= ally.maxHp) {
                        continue;
                    }
                    if (distanceSquared(ally.position, enemy.position) > radiusSq) {
                        continue;
                    }
                    ally.hp = std::min(ally.maxHp, ally.hp + healAmount);
                }
                enemy.enemyHealTimer = enemy.enemyHealIntervalSeconds;
            }
        }

        if (hasBehavior(enemy, "countdown_explode") && (!enemy.countdownExploded || !enemy.countdownExplodeOnce)) {
            enemy.countdownExplodeDelay = std::max(0.0f, enemy.countdownExplodeDelay - dt);
            if (enemy.countdownExplodeDelay <= 0.0f) {
                const float radius = std::max(8.0f, enemy.countdownExplodeRadius);
                const float radiusSq = radius * radius;
                if (enemy.countdownExplodeDamage > 0 &&
                    distanceSquared(player.position, enemy.position) <= radiusSq) {
                    player.lastDamageEnemyName = enemy.enemyName.empty() ? std::string(DefaultEnemyName) : enemy.enemyName;
                    player.applyDamage(
                        applyDefenseModifier(player.status, enemy.countdownExplodeDamage),
                        DamageSource::Trap);
                }
                if (enemy.countdownExplodeTerrainDamage > 0) {
                    const int centerTx = map.worldToTile(enemy.position.x);
                    const int centerTy = map.worldToTile(enemy.position.y);
                    const int tileRadius = std::max(1, static_cast<int>(std::ceil(radius / static_cast<float>(balance::TileSize))));
                    for (int dy = -tileRadius; dy <= tileRadius; ++dy) {
                        for (int dx = -tileRadius; dx <= tileRadius; ++dx) {
                            const Vec2 tileCenter = map.tileCenter(centerTx + dx, centerTy + dy);
                            if (distanceSquared(tileCenter, enemy.position) > radiusSq) {
                                continue;
                            }
                            Vec2 opened{};
                            map.damageTile(centerTx + dx, centerTy + dy, enemy.countdownExplodeTerrainDamage, opened);
                        }
                    }
                }
                enemy.countdownExploded = true;
                processEnemyDeath(enemy);
                continue;
            }
        }

        enemy.contactTimer = std::max(0.0f, enemy.contactTimer - dt);
        bool touchedPlayer = resolvePlayerOverlap(player, enemy, map, balance);
        if (!touchedPlayer &&
            enemy.jumpLandingBuffTimer > 0.0f &&
            enemy.jumpLandingRadius > 0.0f &&
            distanceSquared(player.position, enemy.position) <= enemy.jumpLandingRadius * enemy.jumpLandingRadius) {
            touchedPlayer = true;
        }
        if (touchedPlayer && enemy.contactTimer <= 0.0f) {
            int contactDamage = enemy.isBoss
                ? std::max(1, static_cast<int>(std::ceil(static_cast<double>(enemy.contactAttackPower * 2) * damageTypeMultiplier(enemy.contactDamageType))))
                : std::max(0, static_cast<int>(std::ceil(static_cast<double>(enemy.contactAttackPower) * damageTypeMultiplier(enemy.contactDamageType))));
            float contactMultiplier = std::max(0.0f, enemy.contactDamageMultiplier);
            if (enemy.jumpLandingBuffTimer > 0.0f) {
                contactMultiplier *= std::max(1.0f, enemy.jumpLandingDamageMultiplier);
            }
            contactDamage = std::max(0, static_cast<int>(std::ceil(static_cast<double>(contactDamage) * contactMultiplier)));
            player.lastDamageEnemyName = enemy.enemyName.empty() ? std::string(DefaultEnemyName) : enemy.enemyName;
            player.applyDamage(
                applyDefenseModifier(player.status, contactDamage),
                enemy.isBoss ? DamageSource::SlimeAttack : DamageSource::SlimeContact);
            if (hasBehavior(enemy, "rust_touch")) {
                player.status.applyModifier(
                    "rust_touch",
                    ModifierStat::Defense,
                    clamp(enemy.rustDefenseMultiplier, 0.05f, 1.0f),
                    0.0,
                    std::max(0.1f, enemy.rustDurationSeconds),
                    "enemy:rust_touch:" + enemy.enemyId,
                    StateApplyMode::KeepLonger);
            }
            if (hasBehavior(enemy, "ring_slow_bite")) {
                spellRing.applyEnemyOrbitSpeedDebuff(
                    enemy.ringSlowMultiplier > 0.0f ? enemy.ringSlowMultiplier : balance.enemyRingSlowBiteMultiplier,
                    enemy.ringSlowDurationSeconds >= 0.0f ? enemy.ringSlowDurationSeconds : balance.enemyRingSlowBiteDuration);
            }
            if (hasBehavior(enemy, "chest_bite") && enemy.chestBiteKnockback > 0.0f) {
                const Vec2 push = normalize(player.position - enemy.position) * enemy.chestBiteKnockback;
                tryMoveCircle(map, player.position, balance.playerRadius, push);
            }
            enemy.contactTimer = enemy.isBoss ? 1.0f : 0.8f;
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
            float capturedHitRadius = item.hitRadius;
            if (item.hasCapturedBehavior("jump_outward") && item.capturedJumpTimer > 0.0f) {
                const float bonusRadius = static_cast<float>(std::max(2.0, item.capturedBehaviorParamDouble("jump_outward", "landingRadius", 5.0)));
                capturedHitRadius += bonusRadius;
            }
            const bool overlappingItem = circlesOverlap(enemy.position, enemy.radius, item.worldPosition, capturedHitRadius);
            if (item.type == SpellRingItemType::Shovel && !overlappingItem) {
                item.unlatchEnemy(enemy.id);
                continue;
            }
            if (!overlappingItem) {
                continue;
            }
            if (item.type == SpellRingItemType::Shovel) {
                if (item.isEnemyLatched(enemy.id)) {
                    continue;
                }
                item.latchEnemy(enemy.id);
            } else if (totalTime - item.lastEnemyHitTime < item.hitInterval) {
                continue;
            }
            item.lastEnemyHitTime = totalTime;
            const int speedBonus = static_cast<int>(
                item.orbitMotionSpeed * 0.25f * static_cast<float>(spellRing.speedDamageMultiplier()));
            const int modifiedDamage = static_cast<int>(
                player.status.applyModifiers(
                    ModifierStat::Attack,
                    static_cast<double>(item.damage) *
                        damageTypeMultiplier(item.damageType) *
                        spellRing.effectivePowerMultiplier()));
            const int rawDamage = modifiedDamage + (item.type == SpellRingItemType::Shovel ? speedBonus : 0);
            int adjustedDamage = rawDamage;
            if (isPhysicalDamageType(item.damageType) && hasBehavior(enemy, "physical_resist")) {
                adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * enemy.physicalDamageMultiplier));
            }
            if (isPhysicalDamageType(item.damageType) && hasBehavior(enemy, "magic_body")) {
                adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * enemy.magicBodyPhysicalMultiplier));
            } else if (item.damageType == "magic" && hasBehavior(enemy, "magic_body")) {
                adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * enemy.magicBodyMagicMultiplier));
            }
            if (hasBehavior(enemy, "front_guard") && frontGuardApplies(enemy, item.worldPosition, enemy.frontGuardArcDegrees)) {
                adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * enemy.frontGuardDamageMultiplier));
            }
            const int damageDealt = applyDefenseModifier(enemy.status, adjustedDamage);
            enemy.hp -= damageDealt;
            revealEnemyHpBar(enemy, damageDealt);
            if (item.hasCapturedBehavior("heavy_guard")) {
                enemy.knockbackVelocity = normalize(enemy.position - item.worldPosition) * 90.0f;
                enemy.knockbackTimer = std::max(enemy.knockbackTimer, 0.10f);
            } else {
                item.consumeDurability();
            }
            std::string hitEffectId;
            if (!item.objectId.empty()) {
                const auto objectIt = objectCatalog.objectsById.find(item.objectId);
                if (objectIt != objectCatalog.objectsById.end()) {
                    hitEffectId = visualEffectIdFor(objectIt->second.orbitEffects);
                    EffectContext context;
                    context.sourceObject = &objectIt->second;
                    context.owner = &player;
                    context.targetEntity = &enemy;
                    context.hitTarget = &enemy;
                    context.orbit = &spellRing;
                    context.orbitItem = &item;
                    context.discoveryEvents = discoveryEvents;
                    context.encyclopedia = encyclopedia;
                    context.position = enemy.position;
                    context.triggerType = EffectTriggerType::Hit;
                    context.logUnimplementedEffects = false;
                    effectDispatcher.dispatchOrbitEffects(objectIt->second, context);
                    dispatchCapturedContactEffect(
                        item,
                        objectIt->second,
                        enemy,
                        player,
                        spellRing,
                        effectDispatcher,
                        enemy.position,
                        discoveryEvents,
                        encyclopedia);
                    if (damageDealt > 0) {
                        recordObjectEffectDiscovery(discoveryEvents, objectIt->second, "basic_attack", "", enemy.position);
                    }
                }
            }
            if (hitEffectId.empty()) {
                hitEffectId = visualEffectIdFor(item.addedEffects);
            }
            if (hitEffectId.empty() && !item.damageType.empty() && item.damageType != "none") {
                hitEffectId = item.damageType;
            }
            tryCapturedRewardFromEnemy(item, enemy, totalTime, events_);
            if (item.hasCapturedBehavior("charge_explode") && item.capturedExplodeSleepTimer <= 0.0f) {
                const int requiredHits = std::max(1, item.capturedBehaviorParamInt("charge_explode", "count", CapturedExplosionChargeLimit));
                const float restSeconds = static_cast<float>(std::max(0.1, item.capturedBehaviorParamDouble("charge_explode", "rest", CapturedExplosionSleepSeconds)));
                ++item.capturedExplodeCharge;
                if (item.capturedExplodeCharge >= requiredHits) {
                    item.capturedExplodeCharge = 0;
                    item.capturedExplodeSleepTimer = restSeconds;
                    events_.push_back({EnemyEventType::CapturedExplosion, item.worldPosition});
                }
            }
            enemy.hitFlash = 0.12f;
            events_.push_back(makeEnemyEvent(EnemyEventType::AttackHit, enemy, hitEffectId, damageDealt));
            if (enemy.hp <= 0) {
                processEnemyDeath(enemy);
                break;
            }
        }
    }
    spellRing.removeBrokenItems();
}

void EnemySystem::render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights)
{
    std::vector<DepthRenderEntry> entries;
    renderShadows(renderer, map, playerLight, extraLights);
    appendRenderEntries(entries, renderer, map, playerLight, extraLights);
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
        if (!enemy.active) {
            continue;
        }
        const Vec2 shadowBounds = enemyShadowBoundsSize(renderer, enemy);
        if (!map.isRectLit(enemy.position, shadowBounds, playerLight, extraLights)) {
            continue;
        }
        drawEnemyShadow(renderer, enemy);
    }
}

void EnemySystem::appendRenderEntries(
    std::vector<DepthRenderEntry>& entries,
    Renderer& renderer,
    const TileMap& map,
    Vec2 playerLight,
    const std::vector<LightSource>& extraLights) const
{
    for (const Enemy& enemy : enemies_.items()) {
        if (!enemy.active) {
            continue;
        }
        const Vec2 drawPosition = enemyDrawPosition(enemy);
        const Vec2 visualSize = enemyVisualBoundsSize(renderer, enemy);
        if (!map.isRectLit(drawPosition, visualSize, playerLight, extraLights)) {
            continue;
        }
        entries.push_back(DepthRenderEntry{
            enemy.position.y,
            [&renderer, &enemy]() {
                drawEnemyVisual(renderer, enemy);
            },
        });
    }
}

void EnemySystem::emitStatusParticles(EffectSystem& effects) const
{
    for (const Enemy& enemy : enemies_.items()) {
        if (!enemy.active || enemy.spawnTimer > 0.0f) {
            continue;
        }
        if (enemy.status.hasState("status_poison")) {
            effects.spawnStatusAura(enemy.position, "status_poison");
        }
        if (enemy.status.hasState("status_slow")) {
            effects.spawnStatusAura(enemy.position, "status_slow");
        }
        if (enemy.status.hasState("status_bleed")) {
            effects.spawnStatusAura(enemy.position, "status_bleed");
        }
    }
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
        if (!enemy.active || enemy.spawnTimer > 0.0f) {
            continue;
        }
        if (!circlesOverlap(projectile.position, projectile.radius, enemy.position, enemy.radius)) {
            continue;
        }

        const int adjustedDamage = applyDefenseModifier(enemy.status, std::max(0, damage));
        enemy.hp -= adjustedDamage;
        revealEnemyHpBar(enemy, adjustedDamage);
        enemy.hitFlash = 0.12f;

        if (!projectile.effects.empty()) {
            EffectContext context;
            context.owner = &player;
            context.targetEntity = &enemy;
            context.hitTarget = &enemy;
            context.orbit = &spellRing;
            context.discoveryEvents = discoveryEvents;
            context.encyclopedia = encyclopedia;
            context.position = projectile.position;
            context.triggerType = EffectTriggerType::Hit;
            context.logUnimplementedEffects = false;
            effectDispatcher.dispatch(projectile.effects, context);
        }

        events_.push_back(makeEnemyEvent(EnemyEventType::AttackHit, enemy, visualEffectIdFor(projectile.effects, projectile.damageType), adjustedDamage));
        if (enemy.hp <= 0) {
            pendingXp_ += enemy.xp;
            if (!enemy.dropItemConsumed) {
                queueEnemyObjectDrops(enemy);
            }
            if (enemy.stolenMoney > 0) {
                EnemyEvent death = makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy);
                death.moneyDrop += enemy.stolenMoney;
                enemy.stolenMoney = 0;
                events_.push_back(std::move(death));
            } else {
                events_.push_back(makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy));
            }
            for (const std::string& objectId : enemy.stolenObjectIds) {
                if (objectId.empty()) {
                    continue;
                }
                EnemyEvent objectDrop;
                objectDrop.type = EnemyEventType::ObjectDrop;
                objectDrop.position = enemy.position;
                objectDrop.enemyId = enemy.enemyId;
                objectDrop.enemyName = enemy.enemyName;
                objectDrop.objectDropId = objectId;
                objectDrop.objectDropCount = 1;
                events_.push_back(std::move(objectDrop));
            }
            enemy.stolenObjectIds.clear();
            std::vector<SpellRingItem*> runtimeItems = spellRing.runtimeItemsMutable();
            for (SpellRingItem* itemPtr : runtimeItems) {
                if (itemPtr == nullptr) {
                    continue;
                }
                itemPtr->unlatchEnemy(enemy.id);
            }
            enemy.active = false;
        }
        return true;
    }

    return false;
}

void EnemySystem::applyCapturedExplosion(Vec2 position, SpellRingSystem& spellRing, int damage)
{
    const float radiusSq = CapturedExplosionRadius * CapturedExplosionRadius;
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || enemy.spawnTimer > 0.0f) {
            continue;
        }
        if (distanceSquared(enemy.position, position) > radiusSq) {
            continue;
        }

        const int damageDealt = applyDefenseModifier(enemy.status, std::max(0, damage));
        enemy.hp -= damageDealt;
        revealEnemyHpBar(enemy, damageDealt);
        enemy.hitFlash = 0.18f;
        enemy.knockbackVelocity = normalize(enemy.position - position) * 110.0f;
        enemy.knockbackTimer = std::max(enemy.knockbackTimer, 0.14f);
        events_.push_back(makeEnemyEvent(EnemyEventType::AttackHit, enemy, "fire", damageDealt));
        if (enemy.hp <= 0) {
            pendingXp_ += enemy.xp;
            if (!enemy.dropItemConsumed) {
                queueEnemyObjectDrops(enemy);
            }
            if (enemy.stolenMoney > 0) {
                EnemyEvent death = makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy);
                death.moneyDrop += enemy.stolenMoney;
                enemy.stolenMoney = 0;
                events_.push_back(std::move(death));
            } else {
                events_.push_back(makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy));
            }
            for (const std::string& objectId : enemy.stolenObjectIds) {
                if (objectId.empty()) {
                    continue;
                }
                EnemyEvent objectDrop;
                objectDrop.type = EnemyEventType::ObjectDrop;
                objectDrop.position = enemy.position;
                objectDrop.enemyId = enemy.enemyId;
                objectDrop.enemyName = enemy.enemyName;
                objectDrop.objectDropId = objectId;
                objectDrop.objectDropCount = 1;
                events_.push_back(std::move(objectDrop));
            }
            enemy.stolenObjectIds.clear();
            std::vector<SpellRingItem*> runtimeItems = spellRing.runtimeItemsMutable();
            for (SpellRingItem* itemPtr : runtimeItems) {
                if (itemPtr == nullptr) {
                    continue;
                }
                itemPtr->unlatchEnemy(enemy.id);
            }
            enemy.active = false;
        }
    }
}

void EnemySystem::addMudZone(Vec2 position, float radius, float duration, float speedMultiplier, float damagePerSecond, std::string damageType)
{
    if (!(radius > 0.0f) || !(duration > 0.0f)) {
        return;
    }
    MudZone zone;
    zone.position = position;
    zone.radius = std::max(8.0f, radius);
    zone.remainingSeconds = std::clamp(duration, 0.1f, MudZoneMaxDurationSeconds);
    zone.speedMultiplier = clamp(speedMultiplier, 0.05f, 1.0f);
    zone.damagePerSecond = std::max(0.0f, damagePerSecond);
    zone.damageType = std::move(damageType);
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
        if (!enemy.active || enemy.spawnTimer > 0.0f || !hasEnemyTag(enemy, "metal")) {
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
        if (tryMoveCircle(map, enemy.position, enemy.radius, delta)) {
            ++pulled;
            if (pulled >= CapturedMagnetEnemyLimit) {
                break;
            }
        }
    }
    return pulled;
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
    pendingXp_ = 0;
    mudZones_.clear();
    mudDamageAccumulator_ = 0.0;
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active) {
            continue;
        }
        enemy.status = EntityStatus{};
        enemy.poisonDamageAccumulator = 0.0;
        enemy.hitFlash = 0.0f;
        enemy.hpBarTimer = 0.0f;
        enemy.knockbackVelocity = {};
        enemy.knockbackTimer = 0.0f;
        enemy.contactTimer = 0.0f;
    }
}

std::string EnemySystem::debugEnemySummary() const
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
        out << (enemy.enemyName.empty() ? enemy.enemyId : enemy.enemyName)
            << " id=" << enemy.enemyId
            << " hp=" << enemy.hp << "/" << enemy.maxHp
            << " r=" << enemy.radius
            << " ai=" << enemy.aiId
            << " unaware_ai=" << enemy.unawareAiId
            << " enemy_code=" << (enemy.definition != nullptr ? enemy.definition->enemyBehaviorCode : std::string{})
            << " captured_code=" << (enemy.definition != nullptr ? enemy.definition->capturedBehaviorCode : std::string{})
            << " behavior=" << enemy.behaviorId;
        ++shown;
        if (shown >= 4) {
            break;
        }
    }
    if (shown == 0) {
        return "no active enemies";
    }
    return out.str();
}

CaptureResult EnemySystem::tryCapture(Player& player, SpellRingSystem& spellRing, InventorySystem& inventory)
{
    const Vec2 center = player.position + normalize(player.facing) * CaptureForwardOffset;
    Enemy* best = nullptr;
    float bestDistanceSq = CaptureRange * CaptureRange;
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active || enemy.definition == nullptr) {
            continue;
        }
        const float distanceSq = distanceSquared(enemy.position, center);
        if (distanceSq <= bestDistanceSq) {
            bestDistanceSq = distanceSq;
            best = &enemy;
        }
    }

    if (best == nullptr) {
        return {};
    }

    const float chance = captureChanceFor(*best);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    CaptureResult result{
        .type = CaptureResultType::Failed,
        .enemyName = best->enemyName,
        .chance = chance,
        .position = best->position,
    };
    if (dist(rng_) > chance) {
        return result;
    }

    if (!inventory.addRuntimeObjectItem(makeCapturedItemData(*best))) {
        result.type = CaptureResultType::InventoryFull;
        return result;
    }

    std::vector<SpellRingItem*> runtimeItems = spellRing.runtimeItemsMutable();
    for (SpellRingItem* itemPtr : runtimeItems) {
        if (itemPtr == nullptr) {
            continue;
        }
        itemPtr->unlatchEnemy(best->id);
    }
    best->active = false;
    result.type = CaptureResultType::Success;
    return result;
}

}
