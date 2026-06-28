#include "game/TerrainDigRules.hpp"

#include "data/ObjectCatalog.hpp"
#include "game/SpellRingItem.hpp"
#include "game/SpellRingSystem.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

namespace {

constexpr double MultiDigPowerScale = 1.0 / 3.0;

std::optional<TerrainDigMode> explicitTerrainDigModeForOrbitEffects(const ObjectDefinition* object)
{
    if (object == nullptr) {
        return std::nullopt;
    }

    bool hasMulti = false;
    for (const EffectSpec& spec : object->orbitEffects) {
        if (!isTerrainDigTarget(spec.target)) {
            continue;
        }
        for (const std::string& effect : spec.effects) {
            if (effect == "dig_hard") {
                return TerrainDigMode::HardSpecialist;
            }
            if (effect == "dig_multi") {
                hasMulti = true;
            }
        }
    }
    if (hasMulti) {
        return TerrainDigMode::Multi;
    }
    return std::nullopt;
}

} // namespace

bool isTerrainDigTarget(std::string_view target)
{
    return target == "terrain" || target == "ground";
}

bool isTerrainDigEffect(std::string_view effect)
{
    return effect == "dig" || effect == "dig_hard" || effect == "dig_multi";
}

std::optional<TerrainDigMode> terrainDigModeForEffect(std::string_view effect)
{
    if (effect == "dig_hard") {
        return TerrainDigMode::HardSpecialist;
    }
    if (effect == "dig_multi") {
        return TerrainDigMode::Multi;
    }
    if (effect == "dig") {
        return TerrainDigMode::Normal;
    }
    return std::nullopt;
}

std::string_view terrainDigEffectForMode(TerrainDigMode mode)
{
    switch (mode) {
    case TerrainDigMode::HardSpecialist:
        return "dig_hard";
    case TerrainDigMode::Multi:
        return "dig_multi";
    case TerrainDigMode::Normal:
        return "dig";
    }
    return "dig";
}

TerrainDigModifier terrainDigModifierForMode(TerrainDigMode mode)
{
    return mode == TerrainDigMode::HardSpecialist
        ? TerrainDigModifier::HardSpecialist
        : TerrainDigModifier::Normal;
}

int terrainDigBasePowerForMode(int digPower, TerrainDigMode mode)
{
    if (digPower <= 0) {
        return 0;
    }
    if (mode == TerrainDigMode::Multi) {
        return std::max(1, static_cast<int>(std::round(static_cast<double>(digPower) * MultiDigPowerScale)));
    }
    return digPower;
}

TerrainDigProfile terrainDigProfileFor(const ObjectDefinition* object, const SpellRingItem* item)
{
    const int power = item != nullptr
        ? std::max(0, item->digPower)
        : object != nullptr ? std::max(0, object->digPower) : 0;
    if (power <= 0) {
        return {};
    }

    return {
        .enabled = true,
        .mode = explicitTerrainDigModeForOrbitEffects(object).value_or(TerrainDigMode::Normal),
        .power = power,
    };
}

int terrainDigDamageForRingHit(
    const TerrainDigProfile& profile,
    const SpellRingItem& item,
    const SpellRingSystem& spellRing,
    TerrainAttribute attribute)
{
    if (!profile.enabled) {
        return 0;
    }

    int baseDamage = terrainDigBasePowerForMode(profile.power, profile.mode);
    if (baseDamage <= 0) {
        return 0;
    }

    double powerMultiplier = std::max(0.0, spellRing.effectivePowerMultiplier());
    powerMultiplier *= spellRing.digPowerMultiplierForRing(item.ringIndex);
    powerMultiplier *= spellRing.ringOutputMultiplierForRing(item.ringIndex);
    baseDamage = std::max(1, static_cast<int>(std::round(static_cast<double>(baseDamage) * powerMultiplier)));

    return adjustedTerrainDigDamage(
        baseDamage,
        attribute,
        terrainDigModifierForMode(profile.mode));
}

}
