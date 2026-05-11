#include "game/EnemySystem.hpp"

#include "data/GameBalance.hpp"
#include "engine/Log.hpp"
#include "game/Collision.hpp"
#include "game/EffectSystem.hpp"
#include "game/WorldDropSystem.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

bool allowedDamageType(std::string_view damageType)
{
    return damageType == "none" ||
        damageType == "physical" ||
        damageType == "fire" ||
        damageType == "ice" ||
        damageType == "thunder" ||
        damageType == "wind" ||
        damageType == "earth" ||
        damageType == "water" ||
        damageType == "magic";
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
        behaviorId == "ring_slow_bite" ||
        behaviorId == "swarm_alert" ||
        behaviorId == "dig_move" ||
        behaviorId == "throw_stone" ||
        behaviorId == "shoot_poison" ||
        behaviorId == "shoot_web" ||
        behaviorId == "shoot_fire" ||
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

bool hasEnemyTag(const Enemy& enemy, std::string_view tag)
{
    return std::any_of(enemy.enemyTags.begin(), enemy.enemyTags.end(), [tag](const std::string& value) {
        return value == tag;
    });
}

bool isRangedBehavior(std::string_view behaviorId)
{
    return behaviorId == "throw_stone" ||
        behaviorId == "shoot_poison" ||
        behaviorId == "shoot_web" ||
        behaviorId == "shoot_fire" ||
        behaviorId == "radial_spike" ||
        behaviorId == "shoot_water" ||
        behaviorId == "wind_blow";
}

std::string_view fallbackProjectileForBehavior(std::string_view behaviorId)
{
    if (behaviorId == "throw_stone") {
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

bool frontGuardApplies(const Enemy& enemy, Vec2 hitPosition)
{
    const Vec2 facing = facingVector(enemy.facingAngle);
    const Vec2 hitDirection = normalize(hitPosition - enemy.position);
    return dot(facing, hitDirection) > 0.35f;
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
    item.digPower = definition.capturedDigPower;
    item.durability = definition.capturedDurability;
    item.weightKg = definition.capturedWeight;
    item.tags = definition.capturedTags;
    item.effectText = definition.capturedEffectText;
    item.capturedBehaviorIds = definition.capturedBehaviorIds;
    return item;
}

std::string visualEffectIdFor(const std::vector<EffectSpec>& specs)
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
    return {};
}

EnemyEvent makeEnemyEvent(EnemyEventType type, const Enemy& enemy, std::string effectId = {})
{
    return EnemyEvent{
        .type = type,
        .position = enemy.position,
        .enemyId = enemy.enemyId,
        .enemyName = enemy.enemyName,
        .effectId = std::move(effectId),
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
    std::vector<EffectDiscoveryEvent>* discoveryEvents)
{
    EffectContext context;
    context.sourceObject = &object;
    context.owner = &player;
    context.targetEntity = &enemy;
    context.hitTarget = &enemy;
    context.orbit = &spellRing;
    context.discoveryEvents = discoveryEvents;
    context.position = hitPosition;
    context.triggerType = EffectTriggerType::Hit;
    context.logUnimplementedEffects = false;

    if (item.hasCapturedBehavior("contact_slow") && !effectSpecsContain(object.orbitEffects, "status_slow")) {
        EffectSpec slow;
        slow.target = "enemy";
        slow.effects = {"status_slow"};
        slow.values = {0.85};
        slow.duration = 1.5;
        effectDispatcher.dispatch({slow}, context);
    }

    if (item.hasCapturedBehavior("rust_debuff") && !effectSpecsContain(object.orbitEffects, "debuff_defense")) {
        EffectSpec rust;
        rust.target = "enemy";
        rust.effects = {"debuff_defense"};
        rust.values = {0.8};
        rust.duration = 4.0;
        effectDispatcher.dispatch({rust}, context);
    }
}

bool capturedRewardAllowed(SpellRingItem& item, const Enemy& enemy, float totalTime)
{
    if (enemy.isBoss && item.capturedBossRewardCount >= CapturedBossRewardLimit) {
        return false;
    }
    if (totalTime - item.capturedRewardLastTime < CapturedRewardCooldown) {
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
        chance = CapturedStealChanceEnemy;
    } else if (item.hasCapturedBehavior("reward_drop")) {
        chance = CapturedRewardChanceEnemy;
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
    enemy.contactDamageType = "physical";
    enemy.facingAngle = 0.0f;
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
    if (allowedDamageType(definition->contactDamageType)) {
        enemy.contactDamageType = definition->contactDamageType;
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

void EnemySystem::spawnAt(Vec2 position, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog, bool detectedOnSpawn, Vec2 detectedTarget)
{
    Enemy* enemy = enemies_.acquire();
    if (!enemy) {
        return;
    }
    *enemy = Enemy{};
    enemy->active = true;
    enemy->id = nextEnemyId_++;
    enemy->isBoss = false;
    enemy->position = position;
    applyDefinition(*enemy, chooseEnemyDefinition(enemyCatalog), balance, enemyCatalog);
    enemy->spawnTimer = balance.enemySpawnWarmup;
    enemy->spawnDuration = balance.enemySpawnWarmup;
    if (detectedOnSpawn) {
        forceDetectInSight(*enemy, detectedTarget, true);
    }
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

void EnemySystem::spawnFromDugTiles(const std::vector<Vec2>& dugTiles, TileMap& map, Vec2 playerPosition, const RuntimeBalance& balance, const EnemyCatalog& enemyCatalog)
{
    if (activeCount() >= balance.enemySoftCap) {
        return;
    }
    const int minDugTiles = std::max(1, balance.enemyMinDugTiles);
    const int guaranteeDugTiles = std::max(minDugTiles, balance.enemyGuaranteeDugTiles);
    const int randomWindow = std::max(1, guaranteeDugTiles - minDugTiles + 1);
    std::uniform_int_distribution<int> randomTrigger(1, randomWindow);
    for (Vec2 tileCenter : dugTiles) {
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
        if (!findSpawnPosition(map, tileCenter, playerPosition, balance, spawnPosition)) {
            continue;
        }
        spawnAt(spawnPosition, balance, enemyCatalog, true, playerPosition);
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

    std::queue<std::pair<int, int>> open;
    flowDistance_[static_cast<std::size_t>(index(playerTileX, playerTileY))] = 0;
    open.emplace(playerTileX, playerTileY);

    constexpr std::array<std::pair<int, int>, 4> Directions{{
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    }};

    while (!open.empty()) {
        const auto [tx, ty] = open.front();
        open.pop();
        const int current = flowDistance_[static_cast<std::size_t>(index(tx, ty))];
        for (const auto [dx, dy] : Directions) {
            const int nx = tx + dx;
            const int ny = ty + dy;
            if (!inBounds(nx, ny) || map.isTileSolid(nx, ny)) {
                continue;
            }
            int& next = flowDistance_[static_cast<std::size_t>(index(nx, ny))];
            if (next >= 0) {
                continue;
            }
            next = current + 1;
            open.emplace(nx, ny);
        }
    }
}

Vec2 EnemySystem::flowDirectionFor(TileMap& map, Vec2 enemyPosition, Vec2 playerPosition) const
{
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
    constexpr std::array<std::pair<int, int>, 4> Directions{{
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    }};
    for (const auto [dx, dy] : Directions) {
        const int nx = enemyTileX + dx;
        const int ny = enemyTileY + dy;
        if (!inBounds(nx, ny)) {
            continue;
        }
        const int candidate = flowDistance_[static_cast<std::size_t>(index(nx, ny))];
        if (candidate >= 0 && candidate < bestDistance) {
            bestDistance = candidate;
            bestTarget = map.tileCenter(nx, ny);
        }
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

    const Vec2 toPlayer = normalize(playerPosition - enemy.position);
    const Vec2 origin = enemy.position + toPlayer * (enemy.radius + 6.0f);
    if (enemy.rangedBehaviorId == "radial_spike") {
        constexpr int Count = 8;
        for (int i = 0; i < Count; ++i) {
            const float angle = (static_cast<float>(i) / static_cast<float>(Count)) * Pi * 2.0f;
            projectiles.spawn(enemy.projectileId, enemy.position + fromAngle(angle) * (enemy.radius + 5.0f), fromAngle(angle), ProjectileOwnerType::Enemy, enemy.projectileEffects);
        }
        return;
    }

    projectiles.spawn(enemy.projectileId, origin, toPlayer, ProjectileOwnerType::Enemy, enemy.projectileEffects);
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
    const WorldDropSystem& worldDrops,
    const EffectDispatcher& effectDispatcher,
    ProjectileSystem& projectiles,
    std::vector<EffectDiscoveryEvent>* discoveryEvents)
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
                enemy.hitFlash = 0.12f;
                events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy));
                if (enemy.hp <= 0) {
                    pendingXp_ += enemy.xp;
                    events_.push_back(makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy));
                    for (auto& clearItem : spellRing.items()) {
                        clearItem.unlatchEnemy(enemy.id);
                    }
                    enemy.active = false;
                    continue;
                }
            }
        } else {
            enemy.poisonDamageAccumulator = 0.0;
        }
        enemy.hitFlash = std::max(0.0f, enemy.hitFlash - dt);
        if (enemy.spawnTimer > 0.0f) {
            enemy.spawnTimer = std::max(0.0f, enemy.spawnTimer - dt);
            continue;
        }
        if (enemy.knockbackTimer > 0.0f) {
            moveWithCollision(enemy, map, enemy.knockbackVelocity, dt);
            enemy.knockbackTimer = std::max(0.0f, enemy.knockbackTimer - dt);
            enemy.knockbackVelocity = enemy.knockbackVelocity * std::max(0.0f, 1.0f - 6.0f * dt);
            continue;
        }

        enemy.behaviorTimer += dt;
        enemy.awarenessIconTimer = std::max(0.0f, enemy.awarenessIconTimer - dt);
        if (enemy.awarenessIconTimer <= 0.0f) {
            enemy.awarenessIcon = EnemyAwarenessIcon::None;
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
        double baseSpeed = balance.enemySpeed;
        if (enemy.definition != nullptr && enemy.definition->moveSpeed > 0.0 && std::isfinite(enemy.definition->moveSpeed)) {
            baseSpeed = enemy.definition->moveSpeed;
        }
        if (enemy.enemyId == "mana_leech") {
            baseSpeed *= 0.72;
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
                if (map.damageTile(tx, ty, 1, opened)) {
                    events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy));
                }
                enemy.aiDigTimer = DigActionInterval;
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
                enemy.projectileTimer = enemy.projectileInterval > 0.0f ? enemy.projectileInterval : 2.4f;
            }
        }
        enemy.contactTimer = std::max(0.0f, enemy.contactTimer - dt);
        const bool touchedPlayer = resolvePlayerOverlap(player, enemy, map, balance);
        if (touchedPlayer && enemy.contactTimer <= 0.0f) {
            const int contactDamage = enemy.isBoss
                ? std::max(1, static_cast<int>(std::ceil(static_cast<double>(enemy.contactAttackPower * 2) * damageTypeMultiplier(enemy.contactDamageType))))
                : std::max(0, static_cast<int>(std::ceil(static_cast<double>(enemy.contactAttackPower) * damageTypeMultiplier(enemy.contactDamageType))));
            player.lastDamageEnemyName = enemy.enemyName.empty() ? std::string(DefaultEnemyName) : enemy.enemyName;
            player.applyDamage(
                applyDefenseModifier(player.status, contactDamage),
                enemy.isBoss ? DamageSource::SlimeAttack : DamageSource::SlimeContact);
            if (hasBehavior(enemy, "ring_slow_bite")) {
                spellRing.applyEnemyOrbitSpeedDebuff(
                    balance.enemyRingSlowBiteMultiplier,
                    balance.enemyRingSlowBiteDuration);
            }
            enemy.contactTimer = enemy.isBoss ? 1.0f : 0.8f;
        }

        for (auto& item : spellRing.items()) {
            if (item.broken()) {
                continue;
            }
            const bool overlappingItem = circlesOverlap(enemy.position, enemy.radius, item.worldPosition, item.hitRadius);
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
                spellRing.effectiveAngularSpeed() * 0.25f * static_cast<float>(spellRing.speedDamageMultiplier()));
            const int modifiedDamage = static_cast<int>(
                player.status.applyModifiers(
                    ModifierStat::Attack,
                    static_cast<double>(item.damage) *
                        damageTypeMultiplier(item.damageType) *
                        spellRing.effectivePowerMultiplier()));
            const int rawDamage = modifiedDamage + (item.type == SpellRingItemType::Shovel ? speedBonus : 0);
            int adjustedDamage = rawDamage;
            if (item.damageType == "physical" && hasBehavior(enemy, "physical_resist")) {
                adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * 0.55));
            }
            if (item.damageType == "physical" && hasBehavior(enemy, "magic_body")) {
                adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * 0.35));
            }
            if (hasBehavior(enemy, "front_guard") && frontGuardApplies(enemy, item.worldPosition)) {
                adjustedDamage = static_cast<int>(std::ceil(static_cast<double>(adjustedDamage) * 0.35));
            }
            const int damageDealt = applyDefenseModifier(enemy.status, adjustedDamage);
            enemy.hp -= damageDealt;
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
                    context.position = enemy.position;
                    context.triggerType = EffectTriggerType::Hit;
                    context.logUnimplementedEffects = false;
                    effectDispatcher.dispatchOrbitEffects(objectIt->second, context);
                    dispatchCapturedContactEffect(item, objectIt->second, enemy, player, spellRing, effectDispatcher, enemy.position, discoveryEvents);
                    if (damageDealt > 0) {
                        recordObjectEffectDiscovery(discoveryEvents, objectIt->second, "enemy_damage", "敵にダメージを与える", enemy.position);
                    }
                }
            }
            if (hitEffectId.empty()) {
                hitEffectId = visualEffectIdFor(item.addedEffects);
            }
            tryCapturedRewardFromEnemy(item, enemy, totalTime, events_);
            if (item.hasCapturedBehavior("charge_explode") && item.capturedExplodeSleepTimer <= 0.0f) {
                ++item.capturedExplodeCharge;
                if (item.capturedExplodeCharge >= CapturedExplosionChargeLimit) {
                    item.capturedExplodeCharge = 0;
                    item.capturedExplodeSleepTimer = CapturedExplosionSleepSeconds;
                    events_.push_back({EnemyEventType::CapturedExplosion, item.worldPosition});
                }
            }
            enemy.hitFlash = 0.12f;
            events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy, hitEffectId));
            if (enemy.hp <= 0) {
                pendingXp_ += enemy.xp;
                events_.push_back(makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy));
                for (auto& clearItem : spellRing.items()) {
                    clearItem.unlatchEnemy(enemy.id);
                }
                enemy.active = false;
                break;
            }
        }
    }
    spellRing.removeBrokenItems();
}

void EnemySystem::render(Renderer& renderer, const TileMap& map, Vec2 playerLight, const std::vector<LightSource>& extraLights)
{
    for (const Enemy& enemy : enemies_.items()) {
        if (!enemy.active) {
            continue;
        }
        if (!map.isLit(enemy.position, playerLight, extraLights)) {
            continue;
        }
        const std::string_view renderAiId = enemy.awareness == EnemyAwarenessState::Detected
            ? (enemy.aiId.empty() ? std::string_view("chase") : std::string_view(enemy.aiId))
            : (enemy.unawareAiId.empty() ? std::string_view("idle") : std::string_view(enemy.unawareAiId));
        Vec2 drawPosition = enemy.position;
        if (renderAiId == "hover_chase" || renderAiId == "hover_keep_distance") {
            drawPosition.y -= 4.0f + std::sin(enemy.behaviorTimer * 4.0f) * 3.0f;
        } else if (renderAiId == "phase_wander" || renderAiId == "phase_chase") {
            drawPosition.y -= 4.5f + std::sin(enemy.behaviorTimer * 5.0f) * 3.6f;
        } else if (renderAiId == "jump_chase") {
            const float phase = std::fmod(enemy.behaviorTimer, 1.05f);
            if (phase < 0.25f) {
                drawPosition.y -= std::sin((phase / 0.25f) * Pi) * 5.0f;
            }
        }
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
            continue;
        }
        Color color = enemy.hitFlash > 0.0f ? Color{255, 220, 220, 255} : (enemy.isBoss ? Color{142, 46, 160, 255} : colorForEnemy(enemy));
        if (enemy.hitFlash <= 0.0f && enemy.status.hasState("status_poison")) {
            color = {92, 184, 88, 255};
        } else if (enemy.hitFlash <= 0.0f && enemy.status.hasState("status_slow")) {
            color = {76, 132, 218, 255};
        }
        const float visualRadius = enemy.isBoss ? enemy.radius * BossVisualRadiusMultiplier : enemy.radius;
        renderer.fillCircle(drawPosition, visualRadius, color);
        renderer.drawCircle(drawPosition, visualRadius + 3.0f, enemy.isBoss ? Color{255, 210, 96, 255} : Color{80, 18, 28, 255});
        drawAwarenessIcon(visualRadius);
        if (enemy.isBoss) {
            const float hpRatio = enemy.maxHp > 0 ? clamp(static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHp), 0.0f, 1.0f) : 0.0f;
            const Vec2 barPos = drawPosition + Vec2{-28.0f, -visualRadius - 14.0f};
            renderer.fillRect(barPos, {56.0f, 5.0f}, {18, 10, 22, 220});
            renderer.fillRect(barPos, {56.0f * hpRatio, 5.0f}, {255, 190, 80, 255});
        }
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
    std::vector<EffectDiscoveryEvent>* discoveryEvents)
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
        enemy.hitFlash = 0.12f;

        if (!projectile.effects.empty()) {
            EffectContext context;
            context.owner = &player;
            context.targetEntity = &enemy;
            context.hitTarget = &enemy;
            context.orbit = &spellRing;
            context.discoveryEvents = discoveryEvents;
            context.position = projectile.position;
            context.triggerType = EffectTriggerType::Hit;
            context.logUnimplementedEffects = false;
            effectDispatcher.dispatch(projectile.effects, context);
        }

        events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy, visualEffectIdFor(projectile.effects)));
        if (enemy.hp <= 0) {
            pendingXp_ += enemy.xp;
            events_.push_back(makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy));
            for (auto& item : spellRing.items()) {
                item.unlatchEnemy(enemy.id);
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

        enemy.hp -= applyDefenseModifier(enemy.status, std::max(0, damage));
        enemy.hitFlash = 0.18f;
        enemy.knockbackVelocity = normalize(enemy.position - position) * 110.0f;
        enemy.knockbackTimer = std::max(enemy.knockbackTimer, 0.14f);
        events_.push_back(makeEnemyEvent(EnemyEventType::Hit, enemy));
        if (enemy.hp <= 0) {
            pendingXp_ += enemy.xp;
            events_.push_back(makeEnemyEvent(enemy.isBoss ? EnemyEventType::BossDeath : EnemyEventType::Death, enemy));
            for (auto& item : spellRing.items()) {
                item.unlatchEnemy(enemy.id);
            }
            enemy.active = false;
        }
    }
}

int EnemySystem::pullMetalEnemies(Vec2 center, TileMap& map, float dt)
{
    if (dt <= 0.0f) {
        return 0;
    }

    int pulled = 0;
    const float radiusSq = CapturedMagnetEnemyRadius * CapturedMagnetEnemyRadius;
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
        const float falloff = 1.0f - clamp(distance / CapturedMagnetEnemyRadius, 0.0f, 1.0f);
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
    for (Enemy& enemy : enemies_.items()) {
        if (!enemy.active) {
            continue;
        }
        enemy.status = EntityStatus{};
        enemy.poisonDamageAccumulator = 0.0;
        enemy.hitFlash = 0.0f;
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

    for (auto& item : spellRing.items()) {
        item.unlatchEnemy(best->id);
    }
    best->active = false;
    result.type = CaptureResultType::Success;
    return result;
}

}
