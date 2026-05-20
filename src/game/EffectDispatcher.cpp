#include "game/EffectDispatcher.hpp"

#include "engine/Log.hpp"
#include "game/Enemy.hpp"
#include "game/EnemySystem.hpp"
#include "game/EntityStatus.hpp"
#include "game/GroundLineSystem.hpp"
#include "game/OrbitModifiers.hpp"
#include "game/Player.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/DiggingSystem.hpp"
#include "game/EffectSystem.hpp"
#include "game/MagicSystem.hpp"
#include "game/TileMap.hpp"
#include "game/WorldDropSystem.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <random>

namespace majo {

namespace {

std::string_view objectIdOrNone(const EffectContext& context)
{
    if (context.sourceObject == nullptr) {
        return "<none>";
    }
    return context.sourceObject->id;
}

std::string sourceIdFor(const EffectInvocation& invocation)
{
    if (invocation.context->sourceObject == nullptr) {
        return {};
    }
    if (invocation.context->triggerType == EffectTriggerType::Orbit) {
        return "orbit:" + invocation.context->sourceObject->id;
    }
    return invocation.context->sourceObject->id;
}

bool eventQueueContainsEffect(
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

bool isEffectDiscovered(const EffectContext& context, std::string_view effectKey)
{
    if (context.sourceObject == nullptr || context.sourceObject->id.empty() || effectKey.empty()) {
        return false;
    }
    if (context.encyclopedia != nullptr && context.encyclopedia->hasObjectEffect(context.sourceObject->id, effectKey)) {
        return true;
    }
    return eventQueueContainsEffect(context.discoveryEvents, context.sourceObject->id, effectKey);
}

void recordEffectDiscovery(const EffectInvocation& invocation, std::string_view description = {})
{
    const EffectContext& context = *invocation.context;
    if (context.discoveryEvents == nullptr || context.sourceObject == nullptr || context.sourceObject->id.empty()) {
        return;
    }
    std::string effectKey = std::string(invocation.effect);
    if (effectKey == "guard") {
        effectKey = "guard_projectile";
    }
    context.discoveryEvents->push_back(EffectDiscoveryEvent{
        .objectId = context.sourceObject->id,
        .objectName = context.sourceObject->name,
        .effectKey = std::move(effectKey),
        .description = description.empty() ? std::string{} : std::string(description),
        .note = {},
        .position = context.position,
    });
}

void recordEffectDiscoveryWithNote(
    const EffectInvocation& invocation,
    std::string_view effectKey,
    std::string_view note)
{
    const EffectContext& context = *invocation.context;
    if (context.discoveryEvents == nullptr || context.sourceObject == nullptr || context.sourceObject->id.empty()) {
        return;
    }
    if (effectKey.empty()) {
        return;
    }
    context.discoveryEvents->push_back(EffectDiscoveryEvent{
        .objectId = context.sourceObject->id,
        .objectName = context.sourceObject->name,
        .effectKey = std::string(effectKey),
        .description = {},
        .note = std::string(note),
        .position = context.position,
    });
}

bool isTerrainTarget(std::string_view target)
{
    return target == "terrain" || target == "ground";
}

EntityStatus* statusForInvocation(const EffectInvocation& invocation)
{
    const EffectContext& context = *invocation.context;
    if (invocation.target == "player" || invocation.target == "owner" || invocation.target == "self") {
        return context.owner != nullptr ? &context.owner->status : nullptr;
    }
    if (invocation.target == "enemy" || invocation.target == "target") {
        if (context.hitTarget != nullptr) {
            return &context.hitTarget->status;
        }
        return context.targetEntity != nullptr ? &context.targetEntity->status : nullptr;
    }
    if (isTerrainTarget(invocation.target) || invocation.target == "area" || invocation.target == "orbit" || invocation.target == "projectile") {
        return nullptr;
    }
    if (context.hitTarget != nullptr) {
        return &context.hitTarget->status;
    }
    if (context.targetEntity != nullptr) {
        return &context.targetEntity->status;
    }
    return context.owner != nullptr ? &context.owner->status : nullptr;
}

int positiveIntPower(double value, int fallback = 1)
{
    if (value <= 0.0) {
        return fallback;
    }
    return std::max(1, static_cast<int>(std::round(value)));
}

float areaRadiusFromValue(double value)
{
    return static_cast<float>(std::max(0.0, value) * 48.0);
}

std::string groundLineSourceKey(const EffectInvocation& invocation)
{
    if (invocation.context == nullptr || invocation.context->orbitItem == nullptr) {
        return {};
    }
    const SpellRingItem& item = *invocation.context->orbitItem;
    if (!item.instanceId.empty()) {
        return "item:" + item.instanceId;
    }
    if (!item.objectId.empty()) {
        return "object:" + item.objectId + ":ring:" + std::to_string(item.ringIndex);
    }
    return "ring:" + std::to_string(item.ringIndex);
}

int magicCircleDamage(double value, const MagicCircleCandidate& circle)
{
    const double multiplier = value > 0.0 ? value : 1.0;
    const double baseDamage = 8.0 + static_cast<double>(circle.radius) * 0.08;
    return std::clamp(
        static_cast<int>(std::ceil(baseDamage * multiplier * static_cast<double>(circle.power))),
        1,
        60);
}

Vec2 scatterEffectDropPosition(Vec2 center, std::mt19937& rng)
{
    std::uniform_real_distribution<float> angleDistribution(0.0f, Pi * 2.0f);
    std::uniform_real_distribution<float> radiusDistribution(10.0f, 30.0f);
    return center + fromAngle(angleDistribution(rng)) * radiusDistribution(rng);
}

WorldDropSpawnMotion makeEffectDropJumpMotion(Vec2 center, std::mt19937& rng)
{
    std::uniform_real_distribution<float> durationDistribution(0.34f, 0.48f);
    std::uniform_real_distribution<float> heightDistribution(24.0f, 40.0f);
    const float duration = durationDistribution(rng);
    return {
        .jump = true,
        .startPosition = center,
        .jumpDurationSeconds = duration,
        .jumpArcHeight = heightDistribution(rng),
        .pickupDelaySeconds = duration * 0.75f,
    };
}

TerrainDigModifier terrainDigModifierForEffect(std::string_view effect)
{
    return effect == "dig_hard" ? TerrainDigModifier::HardSpecialist : TerrainDigModifier::Normal;
}

void recordTerrainHit(const EffectInvocation& invocation, int tileX, int tileY, int baseDamage)
{
    if (invocation.context->tileMap == nullptr || baseDamage <= 0) {
        return;
    }

    TileMap& map = *invocation.context->tileMap;
    if (!map.isTileSolid(tileX, tileY)) {
        return;
    }

    int effectiveBaseDamage = baseDamage;
    if (invocation.context->triggerType == EffectTriggerType::Hit && invocation.context->orbit != nullptr) {
        const double powerMultiplier = std::max(0.0, invocation.context->orbit->effectivePowerMultiplier());
        effectiveBaseDamage = std::max(1, static_cast<int>(std::round(static_cast<double>(baseDamage) * powerMultiplier)));
    }

    const int damage = adjustedTerrainDigDamage(
        effectiveBaseDamage,
        map.terrainAttributeAtTile(tileX, tileY),
        terrainDigModifierForEffect(invocation.effect));
    if (damage <= 0) {
        return;
    }

    const Vec2 tileCenter = map.tileCenter(tileX, tileY);
    const Color tileColor = map.tileColorAtTile(tileX, tileY);
    if (invocation.context->terrainHitTiles != nullptr) {
        invocation.context->terrainHitTiles->push_back({tileCenter, tileColor});
    } else if (invocation.context->effects != nullptr) {
        invocation.context->effects->spawnDigHit(tileCenter, {1.0f, 0.0f}, tileColor);
    }

    Vec2 openedTileCenter{};
    TileType openedTileType = TileType::Dirt;
    if (map.damageTile(tileX, tileY, damage, openedTileCenter, &openedTileType)) {
        if (invocation.context->terrainOpenedTiles != nullptr) {
            invocation.context->terrainOpenedTiles->push_back(openedTileCenter);
        }
        if (invocation.context->terrainDugTiles != nullptr) {
            invocation.context->terrainDugTiles->push_back({openedTileCenter, openedTileType, tileColor});
        }
        if (invocation.context->terrainOpenedTiles == nullptr && invocation.context->effects != nullptr) {
            invocation.context->effects->spawnTileBreak(openedTileCenter, openedTileType, tileColor);
        }
    }
    recordEffectDiscovery(invocation, invocation.effect == "dig_hard" ? "硬い土を掘れる" : "土を掘れる");
}

struct StatusDefinition {
    std::string_view effect;
    double defaultDuration = 0.0;
    double defaultValue = 1.0;
};

constexpr std::array<StatusDefinition, 14> StatusDefinitions{{
    {"status_poison", 8.0, 1.0},
    {"status_slow", 8.0, 0.65},
    {"status_glued", 4.0, 0.45},
    {"status_bleed", 8.0, 1.0},
    {"status_giant", 8.0, 1.0},
    {"status_paralyze", 1.5, 1.0},
    {"status_shocked", 1.2, 1.0},
    {"status_sleep", 4.0, 1.0},
    {"status_stun", 0.6, 1.0},
    {"status_confuse", 3.0, 1.0},
    {"status_blind", 4.0, 0.5},
    {"status_wet", 4.0, 1.0},
    {"status_hot", 4.0, 1.0},
    {"status_frozen", 2.5, 1.0},
}};

const StatusDefinition* findStatusDefinition(std::string_view effect)
{
    for (const StatusDefinition& definition : StatusDefinitions) {
        if (definition.effect == effect) {
            return &definition;
        }
    }
    return nullptr;
}

void applyStatus(
    const EffectInvocation& invocation,
    std::string_view statusEffect,
    double stateValue)
{
    EntityStatus* status = statusForInvocation(invocation);
    if (status == nullptr) {
        return;
    }

    const StatusDefinition* definition = findStatusDefinition(statusEffect);
    if (definition == nullptr) {
        return;
    }

    const double duration = invocation.duration > 0.0 ? invocation.duration : definition->defaultDuration;
    status->applyState(
        std::string(statusEffect),
        stateValue,
        duration,
        sourceIdFor(invocation),
        StateApplyMode::KeepLonger);
    recordEffectDiscovery(invocation, statusEffect == "status_giant" ? "巨大化状態を付与する" : "状態異常を付与する");
}

void applyHealInvocation(const EffectInvocation& invocation)
{
    const int amount = std::max(0, static_cast<int>(invocation.value));
    if (amount <= 0 || invocation.duration != 0.0) {
        return;
    }

    const EffectContext& context = *invocation.context;
    if ((invocation.target == "player" || invocation.target == "owner" || invocation.target == "self") && context.owner != nullptr) {
        if (context.owner->heal(amount) > 0) {
            recordEffectDiscovery(invocation, "HPを回復する");
        }
        return;
    }

    Enemy* enemy = nullptr;
    if (invocation.target == "enemy" || invocation.target == "target") {
        enemy = context.hitTarget != nullptr ? context.hitTarget : context.targetEntity;
    } else if (context.hitTarget != nullptr || context.targetEntity != nullptr) {
        enemy = context.hitTarget != nullptr ? context.hitTarget : context.targetEntity;
    }

    if (enemy != nullptr) {
        const int beforeHp = enemy->hp;
        enemy->hp = std::min(enemy->maxHp, enemy->hp + amount);
        const int healed = enemy->hp - beforeHp;
        if (healed > 0) {
            if (context.effects != nullptr) {
                context.effects->spawnDamagePopup(enemy->position, healed, DamagePopupStyle::Heal);
            }
            recordEffectDiscovery(invocation, "対象を回復する");
        }
    }
}

void applyStateInvocation(const EffectInvocation& invocation)
{
    const StatusDefinition* definition = findStatusDefinition(invocation.effect);
    const double defaultValue = definition != nullptr ? definition->defaultValue : 1.0;
    const double stateValue = invocation.value == 0.0 ? defaultValue : invocation.value;
    applyStatus(invocation, invocation.effect, stateValue);
}

void applyChanceStateInvocation(const EffectInvocation& invocation)
{
    static std::mt19937 rng{std::random_device{}()};
    std::string_view statusEffect;
    if (invocation.effect == "status_poison_chance") {
        statusEffect = "status_poison";
    } else if (invocation.effect == "status_slow_chance") {
        statusEffect = "status_slow";
    } else if (invocation.effect == "status_paralyze_chance") {
        statusEffect = "status_paralyze";
    } else if (invocation.effect == "status_bleed_chance") {
        statusEffect = "status_bleed";
    } else if (invocation.effect == "status_sleep_chance") {
        statusEffect = "status_sleep";
    } else if (invocation.effect == "status_stun_chance") {
        statusEffect = "status_stun";
    } else if (invocation.effect == "status_confuse_chance") {
        statusEffect = "status_confuse";
    } else {
        return;
    }

    if (statusForInvocation(invocation) == nullptr) {
        return;
    }

    Enemy* enemy = nullptr;
    if (invocation.target == "enemy" || invocation.target == "target") {
        enemy = invocation.context->hitTarget != nullptr
            ? invocation.context->hitTarget
            : invocation.context->targetEntity;
    }

    const bool firstDiscoveryGuarantee = invocation.context->discoveryEvents != nullptr &&
        invocation.context->sourceObject != nullptr &&
        !isEffectDiscovered(*invocation.context, invocation.effect);
    if (enemy != nullptr && enemy->isBoss) {
        recordEffectDiscoveryWithNote(invocation, invocation.effect, "※ただし、この敵には効かなかった");
        return;
    }

    if (!firstDiscoveryGuarantee) {
        const double chance = std::clamp(invocation.value, 0.0, 100.0);
        std::uniform_real_distribution<double> dist(0.0, 100.0);
        if (dist(rng) > chance) {
            return;
        }
    }

    const StatusDefinition* definition = findStatusDefinition(statusEffect);
    applyStatus(invocation, statusEffect, definition != nullptr ? definition->defaultValue : 1.0);
}

void applyModifierInvocation(const EffectInvocation& invocation)
{
    EntityStatus* status = statusForInvocation(invocation);
    if (status == nullptr) {
        return;
    }

    ModifierStat stat = ModifierStat::Attack;
    if (!modifierStatFromEffect(invocation.effect, stat)) {
        return;
    }

    const double multiplier = invocation.value > 0.0 ? invocation.value : 1.0;
    const double duration = invocation.duration > 0.0 ? invocation.duration : 4.0;
    status->applyModifier(
        std::string(invocation.effect),
        stat,
        multiplier,
        0.0,
        duration,
        sourceIdFor(invocation),
        StateApplyMode::KeepLonger);
    recordEffectDiscovery(invocation, "能力変化を与える");
}

void applyOrbitModifierInvocation(const EffectInvocation& invocation)
{
    if (invocation.context->triggerType != EffectTriggerType::Orbit) {
        return;
    }
    if (invocation.target != "orbit") {
        return;
    }
    if (invocation.context->orbit == nullptr) {
        return;
    }

    invocation.context->orbit->applyOrbitModifierEffect(
        invocation.effect,
        invocation.value,
        sourceIdFor(invocation));
    recordEffectDiscovery(invocation);
}

std::string spawnBiasGroupFromEffect(std::string_view effect)
{
    constexpr std::string_view Prefix = "spawn_bias_";
    if (effect.rfind(Prefix, 0) != 0 || effect.size() <= Prefix.size()) {
        return {};
    }
    return std::string(effect.substr(Prefix.size()));
}

void applySpawnBiasInvocation(const EffectInvocation& invocation)
{
    if (invocation.context->triggerType != EffectTriggerType::Orbit) {
        return;
    }
    if (invocation.target != "ring") {
        return;
    }
    if (invocation.context->enemies == nullptr) {
        return;
    }

    const std::string group = spawnBiasGroupFromEffect(invocation.effect);
    if (group.empty()) {
        return;
    }

    invocation.context->enemies->applySpawnBias(group, invocation.value);
    recordEffectDiscovery(invocation);
}

void applyItemParameterInvocation(const EffectInvocation& invocation)
{
    if (invocation.context->triggerType != EffectTriggerType::Orbit ||
        invocation.target != "item" ||
        invocation.context->orbitItem == nullptr) {
        return;
    }

    if (invocation.effect == "dry_wet_bonus_damage") {
        const int bonusDamage = std::max(0, static_cast<int>(std::ceil(std::max(0.0, invocation.value))));
        invocation.context->orbitItem->dryWetBonusDamage += bonusDamage;
    }
}

void applyDigInvocation(const EffectInvocation& invocation)
{
    if (!isTerrainTarget(invocation.target) || invocation.context->tileMap == nullptr) {
        return;
    }

    const int damage = positiveIntPower(invocation.value);

    const int tileX = invocation.context->tileMap->worldToTile(invocation.context->position.x);
    const int tileY = invocation.context->tileMap->worldToTile(invocation.context->position.y);
    if (invocation.effect == "dig_multi") {
        constexpr std::array<std::pair<int, int>, 5> Offsets{{
            {0, 0},
            {1, 0},
            {-1, 0},
            {0, 1},
            {0, -1},
        }};
        for (const auto [dx, dy] : Offsets) {
            recordTerrainHit(invocation, tileX + dx, tileY + dy, damage);
        }
        return;
    }

    recordTerrainHit(invocation, tileX, tileY, damage);
}

void applyAreaInvocation(const EffectInvocation& invocation)
{
    if (invocation.target != "area" || invocation.context->orbitItem == nullptr) {
        return;
    }

    const float radius = areaRadiusFromValue(invocation.value);
    if (invocation.effect == "light") {
        const float beforeRadius = invocation.context->orbitItem->lightRadius;
        invocation.context->orbitItem->lightRadius = std::max(invocation.context->orbitItem->lightRadius, radius);
        if (invocation.context->orbitItem->lightRadius > beforeRadius) {
            recordEffectDiscovery(invocation, "リング上で周囲を照らす");
        }
        return;
    }
    if (invocation.effect == "detect_hidden") {
        invocation.context->orbitItem->hiddenDetectionRadius =
            std::max(invocation.context->orbitItem->hiddenDetectionRadius, radius);
        return;
    }
    if (invocation.effect == "detect_treasure" || invocation.effect == "detect") {
        invocation.context->orbitItem->treasureDetectionRadius =
            std::max(invocation.context->orbitItem->treasureDetectionRadius, radius);
        return;
    }
    if (invocation.effect == "cold_air_aura") {
        invocation.context->orbitItem->coldAirRadius =
            std::max(invocation.context->orbitItem->coldAirRadius, std::max(radius, invocation.context->orbitItem->hitRadius + 24.0f));
        invocation.context->orbitItem->coldAirStrength += static_cast<float>(std::max(0.0, invocation.value));
        return;
    }
    if (invocation.effect == "vacuum_pull_light") {
        invocation.context->orbitItem->vacuumPullRadius =
            std::max(invocation.context->orbitItem->vacuumPullRadius, std::max(radius, invocation.context->orbitItem->hitRadius + 36.0f));
        invocation.context->orbitItem->vacuumPullStrength += static_cast<float>(std::max(1.0, invocation.value));
        return;
    }
    if (invocation.effect == "hot_air") {
        invocation.context->orbitItem->hotAirRadius =
            std::max(invocation.context->orbitItem->hotAirRadius, std::max(radius, invocation.context->orbitItem->hitRadius + 30.0f));
        invocation.context->orbitItem->hotAirStrength += static_cast<float>(std::max(1.0, invocation.value));
        return;
    }
    if (invocation.effect == "wind_push_light") {
        invocation.context->orbitItem->windPushRadius =
            std::max(invocation.context->orbitItem->windPushRadius, std::max(radius, invocation.context->orbitItem->hitRadius + 36.0f));
        invocation.context->orbitItem->windPushStrength += static_cast<float>(std::max(1.0, invocation.value));
        return;
    }
    if (invocation.effect == "conduct_water_puddle") {
        invocation.context->orbitItem->conductWaterPuddleRadius =
            std::max(invocation.context->orbitItem->conductWaterPuddleRadius, std::max(radius, invocation.context->orbitItem->hitRadius + 36.0f));
        invocation.context->orbitItem->conductWaterPuddleStrength += static_cast<float>(std::max(1.0, invocation.value));
        return;
    }
}

void applyGroundInvocation(const EffectInvocation& invocation)
{
    if (invocation.target != "ground" ||
        invocation.context == nullptr ||
        invocation.context->orbitItem == nullptr ||
        invocation.context->groundLines == nullptr) {
        return;
    }

    const std::string sourceKey = groundLineSourceKey(invocation);
    if (sourceKey.empty()) {
        return;
    }

    const Vec2 position = invocation.context->orbitItem->worldPosition;
    if (invocation.effect == "draw_white_line") {
        const float strength = static_cast<float>(std::max(0.25, invocation.value));
        const float width = std::clamp(2.0f + strength * 1.7f, 2.0f, 8.0f);
        const float lifetime = invocation.duration > 0.0
            ? static_cast<float>(invocation.duration)
            : 0.0f;
        if (invocation.context->groundLines->drawWhiteLine(sourceKey, position, width, strength, lifetime)) {
            recordEffectDiscovery(invocation, "地面に白い線を書く");
        }
        return;
    }

    if (invocation.effect != "complete_magic_circle" ||
        invocation.context->enemies == nullptr ||
        invocation.context->orbit == nullptr) {
        return;
    }

    const std::optional<MagicCircleCandidate> circle =
        invocation.context->groundLines->findCompletedCircleNear(position, 64.0f, sourceKey);
    if (!circle.has_value()) {
        return;
    }

    EnemyMagicHitSpec spec;
    spec.position = circle->center;
    spec.radius = std::clamp(circle->radius, 28.0f, 180.0f);
    spec.damage = magicCircleDamage(invocation.value, *circle);
    spec.damageType = "magic";
    spec.effectId = "complete_magic_circle";
    const int hitCount = invocation.context->enemies->applyMagicArea(spec, *invocation.context->orbit);
    invocation.context->groundLines->consumeSegments(circle->segmentIds);
    if (invocation.context->effects != nullptr) {
        invocation.context->effects->spawnAreaPulse(circle->center, spec.radius, {238, 246, 255, 196});
    }
    recordEffectDiscovery(invocation, hitCount > 0 ? "白線で囲んだ魔法陣を発動する" : "白線の魔法陣を完成させる");
}

void applyKnockbackInvocation(const EffectInvocation& invocation)
{
    if (invocation.target != "enemy" && invocation.target != "target") {
        return;
    }

    Enemy* enemy = invocation.context->hitTarget != nullptr ? invocation.context->hitTarget : invocation.context->targetEntity;
    if (enemy == nullptr) {
        return;
    }

    Vec2 direction = enemy->position - invocation.context->position;
    if (lengthSquared(direction) <= 0.0001f && invocation.context->owner != nullptr) {
        direction = enemy->position - invocation.context->owner->position;
    }

    const float strength = static_cast<float>(std::max(1.0, invocation.value) * 120.0);
    enemy->knockbackVelocity = normalize(direction) * strength;
    enemy->knockbackTimer = 0.16f + static_cast<float>(std::min(0.18, std::max(0.0, invocation.value) * 0.03));
    recordEffectDiscovery(invocation, "敵をノックバックさせる");
}

void applyCoinDropChanceInvocation(const EffectInvocation& invocation)
{
    if (invocation.target != "enemy" && invocation.target != "target") {
        return;
    }

    const EffectContext& context = *invocation.context;
    Enemy* enemy = context.hitTarget != nullptr ? context.hitTarget : context.targetEntity;
    if (enemy == nullptr || context.worldDrops == nullptr) {
        return;
    }

    const bool firstDiscoveryGuarantee = context.discoveryEvents != nullptr &&
        context.sourceObject != nullptr &&
        !isEffectDiscovered(context, invocation.effect);
    if (!firstDiscoveryGuarantee) {
        const double chance = std::clamp(invocation.value, 0.0, 100.0);
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<double> dist(0.0, 100.0);
        if (dist(rng) > chance) {
            return;
        }
    }

    static std::mt19937 amountRng{std::random_device{}()};
    std::uniform_int_distribution<int> amountDist(1, 3);
    if (context.worldDrops->spawnMoneyDrop(
            amountDist(amountRng),
            scatterEffectDropPosition(enemy->position, amountRng),
            context.dropSpawnedAtSeconds,
            makeEffectDropJumpMotion(enemy->position, amountRng))) {
        recordEffectDiscovery(
            invocation,
            invocation.effect == "hit_coin_spill" ? "敵から小銭をこぼさせる" : "敵から少額のお金を落とす");
    }
}

void applyCastMagicInvocation(const EffectInvocation& invocation)
{
    const EffectContext& context = *invocation.context;
    if (context.magic == nullptr) {
        return;
    }

    MagicElement element = MagicElement::Fire;
    if (!magicElementFromCastEffect(invocation.effect, element)) {
        return;
    }

    Vec2 origin = context.position;
    Vec2 direction{1.0f, 0.0f};
    if (context.orbitItem != nullptr) {
        origin = context.orbitItem->worldPosition;
        if (context.orbit != nullptr) {
            direction = origin - context.orbit->center();
        } else if (context.owner != nullptr) {
            direction = origin - context.owner->position;
        }
    } else if (context.owner != nullptr) {
        direction = context.owner->facing;
        origin = context.owner->position + normalize(direction) * 20.0f;
    }
    if (lengthSquared(direction) <= 0.0001f && context.owner != nullptr) {
        direction = context.owner->facing;
    }

    const int power = positiveIntPower(invocation.value, 1);
    context.magic->cast(element, origin, direction, power, context.orbitItem);
    recordEffectDiscovery(invocation, "属性魔法を発動する");
}

}

void EffectDispatcher::registerHandler(std::string effect, Handler handler)
{
    if (effect.empty()) {
        return;
    }

    if (handler) {
        handlers_[std::move(effect)] = std::move(handler);
    } else {
        handlers_.erase(effect);
    }
}

void EffectDispatcher::unregisterHandler(std::string_view effect)
{
    handlers_.erase(std::string(effect));
}

void EffectDispatcher::clearHandlers()
{
    handlers_.clear();
}

void EffectDispatcher::registerFoundationHandlers(const ObjectCatalog& catalog)
{
    if (catalog.effectCodes.find("heal") != catalog.effectCodes.end()) {
        registerHandler("heal", applyHealInvocation);
    }

    for (std::string_view effect : {"dig", "dig_hard", "dig_multi"}) {
        if (catalog.effectCodes.find(std::string(effect)) != catalog.effectCodes.end()) {
            registerHandler(std::string(effect), applyDigInvocation);
        }
    }

    for (std::string_view effect : {"light", "detect_hidden", "detect_treasure", "detect", "cold_air_aura", "vacuum_pull_light", "hot_air", "wind_push_light", "conduct_water_puddle"}) {
        if (catalog.effectCodes.find(std::string(effect)) != catalog.effectCodes.end()) {
            registerHandler(std::string(effect), applyAreaInvocation);
        }
    }

    for (std::string_view effect : {"draw_white_line", "complete_magic_circle"}) {
        if (catalog.effectCodes.find(std::string(effect)) != catalog.effectCodes.end()) {
            registerHandler(std::string(effect), applyGroundInvocation);
        }
    }

    for (std::string_view effect : {"orbit_speed", "orbit_power", "orbit_gravity", "orbit_antigravity", "orbit_anchor", "orbit_shift", "damage_speed"}) {
        if (catalog.effectCodes.find(std::string(effect)) != catalog.effectCodes.end()) {
            registerHandler(std::string(effect), applyOrbitModifierInvocation);
        }
    }

    for (std::string_view effect : {"slash_power", "item_orbit_offset", "dry_wet_bonus_damage"}) {
        if (catalog.effectCodes.find(std::string(effect)) != catalog.effectCodes.end()) {
            registerHandler(std::string(effect), applyItemParameterInvocation);
        }
    }

    for (std::string_view effect : {"status_poison", "status_slow", "status_glued", "status_giant", "status_paralyze", "status_shocked", "status_sleep", "status_bleed", "status_stun", "status_confuse", "status_blind", "status_wet", "status_hot", "status_frozen"}) {
        registerHandler(std::string(effect), applyStateInvocation);
    }

    for (std::string_view effect : {"status_poison_chance", "status_slow_chance", "status_paralyze_chance", "status_bleed_chance", "status_sleep_chance", "status_stun_chance", "status_confuse_chance"}) {
        registerHandler(std::string(effect), applyChanceStateInvocation);
    }

    for (std::string_view effect : {"buff_attack", "debuff_attack", "buff_speed", "debuff_speed", "buff_defense", "debuff_defense"}) {
        registerHandler(std::string(effect), applyModifierInvocation);
    }

    if (catalog.effectCodes.find("knockback") != catalog.effectCodes.end()) {
        registerHandler("knockback", applyKnockbackInvocation);
    }

    for (std::string_view effect : {"coin_drop_chance", "hit_coin_spill"}) {
        if (catalog.effectCodes.find(std::string(effect)) != catalog.effectCodes.end()) {
            registerHandler(std::string(effect), applyCoinDropChanceInvocation);
        }
    }

    for (std::string_view effect : {"cast_fire", "cast_ice", "cast_thunder", "cast_wind", "cast_earth"}) {
        registerHandler(std::string(effect), applyCastMagicInvocation);
    }

    for (const auto& [effect, definition] : catalog.effectCodes) {
        (void)definition;
        if (effect.rfind("spawn_bias_", 0) == 0) {
            registerHandler(effect, applySpawnBiasInvocation);
        }
    }
}

bool EffectDispatcher::hasHandler(std::string_view effect) const
{
    return handlers_.find(std::string(effect)) != handlers_.end();
}

std::size_t EffectDispatcher::handlerCount() const
{
    return handlers_.size();
}

void EffectDispatcher::dispatch(const std::vector<EffectSpec>& specs, const EffectContext& context) const
{
    for (const EffectSpec& spec : specs) {
        for (std::size_t effectIndex = 0; effectIndex < spec.effects.size(); ++effectIndex) {
            dispatchOne(spec, effectIndex, context);
        }
    }
}

void EffectDispatcher::dispatchTargetEffects(const std::vector<EffectSpec>& specs, std::string_view target, const EffectContext& context) const
{
    for (const EffectSpec& spec : specs) {
        if (spec.target != target) {
            continue;
        }
        for (std::size_t effectIndex = 0; effectIndex < spec.effects.size(); ++effectIndex) {
            dispatchOne(spec, effectIndex, context);
        }
    }
}

void EffectDispatcher::dispatchNormalEffects(const ObjectDefinition& object, EffectContext context) const
{
    context.sourceObject = &object;
    if (context.triggerType == EffectTriggerType::Unknown) {
        context.triggerType = EffectTriggerType::NormalUse;
    }
    dispatch(object.normalEffects, context);
}

void EffectDispatcher::dispatchOrbitEffects(const ObjectDefinition& object, EffectContext context) const
{
    context.sourceObject = &object;
    if (context.triggerType == EffectTriggerType::Unknown) {
        context.triggerType = EffectTriggerType::Orbit;
    }
    dispatch(object.orbitEffects, context);
}

void EffectDispatcher::dispatchOne(const EffectSpec& spec, std::size_t effectIndex, const EffectContext& context) const
{
    if (effectIndex >= spec.effects.size()) {
        return;
    }

    const std::string& effect = spec.effects[effectIndex];
    if (effect.empty() || effect == "none") {
        return;
    }

    const double value = effectIndex < spec.values.size() ? spec.values[effectIndex] : 0.0;
    EffectInvocation invocation{
        .spec = &spec,
        .context = &context,
        .target = spec.target,
        .effect = effect,
        .value = value,
        .duration = spec.duration,
    };

    const auto handler = handlers_.find(effect);
    if (handler == handlers_.end()) {
        if (context.logUnimplementedEffects) {
            logUnimplemented(invocation);
        }
        return;
    }

    handler->second(invocation);
}

void EffectDispatcher::logUnimplemented(const EffectInvocation& invocation) const
{
    std::ostringstream line;
    line << "[debug] EffectDispatcher unimplemented effect"
        << " source=\"" << objectIdOrNone(*invocation.context)
        << "\" trigger=\"" << effectTriggerTypeName(invocation.context->triggerType)
        << "\" target=\"" << invocation.target
        << "\" effect=\"" << invocation.effect
        << "\" value=" << invocation.value
        << " duration=" << invocation.duration;
    logError(line.str());
}

std::string_view effectTriggerTypeName(EffectTriggerType triggerType)
{
    switch (triggerType) {
    case EffectTriggerType::Unknown:
        return "unknown";
    case EffectTriggerType::NormalUse:
        return "normal_use";
    case EffectTriggerType::Orbit:
        return "orbit";
    case EffectTriggerType::Hit:
        return "hit";
    case EffectTriggerType::Debug:
        return "debug";
    }
    return "unknown";
}

}
