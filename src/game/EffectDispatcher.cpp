#include "game/EffectDispatcher.hpp"

#include "engine/Log.hpp"
#include "game/Enemy.hpp"
#include "game/EntityStatus.hpp"
#include "game/OrbitModifiers.hpp"
#include "game/Player.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/DiggingSystem.hpp"
#include "game/EffectSystem.hpp"
#include "game/TileMap.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <algorithm>
#include <array>
#include <cmath>
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

void recordEffectDiscovery(const EffectInvocation& invocation, std::string_view description = {})
{
    const EffectContext& context = *invocation.context;
    if (context.discoveryEvents == nullptr || context.sourceObject == nullptr || context.sourceObject->id.empty()) {
        return;
    }
    context.discoveryEvents->push_back(EffectDiscoveryEvent{
        .objectId = context.sourceObject->id,
        .objectName = context.sourceObject->name,
        .effectKey = std::string(invocation.effect),
        .description = description.empty() ? std::string{} : std::string(description),
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

    const int damage = adjustedTerrainDigDamage(
        baseDamage,
        map.terrainAttributeAtTile(tileX, tileY),
        terrainDigModifierForEffect(invocation.effect));
    if (damage <= 0) {
        return;
    }

    const Vec2 tileCenter = map.tileCenter(tileX, tileY);
    if (invocation.context->terrainHitTiles != nullptr) {
        invocation.context->terrainHitTiles->push_back(tileCenter);
    } else if (invocation.context->effects != nullptr) {
        invocation.context->effects->spawnDigHit(tileCenter);
    }

    Vec2 openedTileCenter{};
    TileType openedTileType = TileType::Dirt;
    if (map.damageTile(tileX, tileY, damage, openedTileCenter, &openedTileType)) {
        if (invocation.context->terrainOpenedTiles != nullptr) {
            invocation.context->terrainOpenedTiles->push_back(openedTileCenter);
        }
        if (invocation.context->terrainDugTiles != nullptr) {
            invocation.context->terrainDugTiles->push_back({openedTileCenter, openedTileType});
        }
        if (invocation.context->terrainOpenedTiles == nullptr && invocation.context->effects != nullptr) {
            invocation.context->effects->spawnTileBreak(openedTileCenter);
        }
    }
    recordEffectDiscovery(invocation, invocation.effect == "dig_hard" ? "硬い地形に強い" : "地形を掘削できる");
}

struct StatusDefinition {
    std::string_view effect;
    double defaultDuration = 0.0;
};

constexpr std::array<StatusDefinition, 3> StatusDefinitions{{
    {"status_poison", 8.0},
    {"status_slow", 8.0},
    {"status_bleed", 8.0},
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
    recordEffectDiscovery(invocation, "状態異常を付与する");
}

void applyHealInvocation(const EffectInvocation& invocation)
{
    const int amount = std::max(0, static_cast<int>(invocation.value));
    if (amount <= 0 || invocation.duration != 0.0) {
        return;
    }

    const EffectContext& context = *invocation.context;
    if ((invocation.target == "player" || invocation.target == "owner" || invocation.target == "self") && context.owner != nullptr) {
        const int beforeHp = context.owner->hp;
        context.owner->hp = std::min(context.owner->maxHp, context.owner->hp + amount);
        if (context.owner->hp > beforeHp) {
            recordEffectDiscovery(invocation, "使うとHPを回復する");
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
        if (enemy->hp > beforeHp) {
            recordEffectDiscovery(invocation, "対象を回復する");
        }
    }
}

void applyStateInvocation(const EffectInvocation& invocation)
{
    const double stateValue = invocation.value == 0.0 ? 1.0 : invocation.value;
    applyStatus(invocation, invocation.effect, stateValue);
}

void applyChanceStateInvocation(const EffectInvocation& invocation)
{
    static std::mt19937 rng{std::random_device{}()};
    const double chance = std::clamp(invocation.value, 0.0, 100.0);
    std::uniform_real_distribution<double> dist(0.0, 100.0);
    if (dist(rng) > chance) {
        return;
    }

    std::string_view statusEffect;
    if (invocation.effect == "status_poison_chance") {
        statusEffect = "status_poison";
    } else if (invocation.effect == "status_slow_chance") {
        statusEffect = "status_slow";
    } else if (invocation.effect == "status_bleed_chance") {
        statusEffect = "status_bleed";
    } else {
        return;
    }

    applyStatus(invocation, statusEffect, 1.0);
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
    }
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

    if (catalog.effectCodes.find("light") != catalog.effectCodes.end()) {
        registerHandler("light", applyAreaInvocation);
    }

    for (std::string_view effect : {"orbit_speed", "orbit_power", "damage_speed"}) {
        if (catalog.effectCodes.find(std::string(effect)) != catalog.effectCodes.end()) {
            registerHandler(std::string(effect), applyOrbitModifierInvocation);
        }
    }

    for (std::string_view effect : {"status_poison", "status_slow"}) {
        registerHandler(std::string(effect), applyStateInvocation);
    }

    for (std::string_view effect : {"status_poison_chance", "status_slow_chance", "status_bleed_chance"}) {
        registerHandler(std::string(effect), applyChanceStateInvocation);
    }

    for (std::string_view effect : {"buff_attack", "debuff_attack", "buff_speed", "debuff_speed", "buff_defense", "debuff_defense"}) {
        registerHandler(std::string(effect), applyModifierInvocation);
    }

    if (catalog.effectCodes.find("knockback") != catalog.effectCodes.end()) {
        registerHandler("knockback", applyKnockbackInvocation);
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
