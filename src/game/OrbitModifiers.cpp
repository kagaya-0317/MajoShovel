#include "game/OrbitModifiers.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace majo {

namespace {

double multiplierValue(double value)
{
    if (!std::isfinite(value)) {
        return 1.0;
    }
    return value == 0.0 ? 1.0 : value;
}

int levelValue(double value)
{
    if (!std::isfinite(value)) {
        return 0;
    }
    return std::max(0, static_cast<int>(std::round(value)));
}

std::vector<int> equipmentTargetRingIndexes(std::string_view target)
{
    if (target == "equip_all") {
        return {0, 1, 2};
    }
    if (target == "equip_ring1") {
        return {0};
    }
    if (target == "equip_ring2") {
        return {1};
    }
    if (target == "equip_ring3") {
        return {2};
    }
    return {};
}

void applyRingEquipmentModifier(RingEquipmentModifiers& ring, std::string_view effect, double value)
{
    const double multiplier = multiplierValue(value);
    if (effect == "ring_speed_mul") {
        ring.ringSpeedMul *= multiplier;
    } else if (effect == "ring_radius_mul") {
        ring.ringRadiusMul *= multiplier;
    } else if (effect == "ring_weight_limit_add") {
        if (std::isfinite(value)) {
            ring.ringWeightLimitAdd += value;
        }
    } else if (effect == "ring_shift_distance_mul") {
        ring.ringShiftDistanceMul *= multiplier;
    } else if (effect == "ring_throw_distance_mul") {
        ring.ringThrowDistanceMul *= multiplier;
    } else if (effect == "ring_throw_speed_mul") {
        ring.ringThrowSpeedMul *= multiplier;
    } else if (effect == "ring_throw_cooldown_mul") {
        ring.ringThrowCooldownMul *= multiplier;
    } else if (effect == "ring_return_speed_mul") {
        ring.ringReturnSpeedMul *= multiplier;
    } else if (effect == "ring_output_mul") {
        ring.ringOutputMul *= multiplier;
    } else if (effect == "ring_anchor_mul") {
        ring.ringAnchorMul *= multiplier;
    } else if (effect == "ring_damage_speed_mul") {
        ring.ringDamageSpeedMul *= multiplier;
    } else if (effect == "light_radius_mul") {
        ring.lightRadiusMul *= multiplier;
    } else if (effect == "detect_range_mul") {
        ring.detectRangeMul *= multiplier;
    } else if (effect == "guard_area_mul") {
        ring.guardAreaMul *= multiplier;
    } else if (effect == "reflect_power_mul") {
        ring.reflectPowerMul *= multiplier;
    } else if (effect == "reflect_chance_add") {
        if (std::isfinite(value)) {
            ring.reflectChanceAdd += value;
        }
    } else if (effect == "metal_weight_penalty_mul") {
        ring.metalWeightPenaltyMul *= multiplier;
    } else if (effect == "dig_power_mul") {
        ring.digPowerMul *= multiplier;
    }
}

void applyGlobalEquipmentModifier(EquipmentModifiers& modifiers, std::string_view effect, double value)
{
    // TODO: durability/sell/visibility global modifiers are aggregated for logs/debug;
    // hook them into those systems when their equipment modifier pipeline is defined.
    const double multiplier = multiplierValue(value);
    if (effect == "durability_cost_mul") {
        modifiers.durabilityCostMul *= multiplier;
    } else if (effect == "sell_price_mul") {
        modifiers.sellPriceMul *= multiplier;
    } else if (effect == "money_visible_level") {
        modifiers.moneyVisibleLevel = std::max(modifiers.moneyVisibleLevel, levelValue(value));
    } else if (effect == "danger_hint_level") {
        modifiers.dangerHintLevel = std::max(modifiers.dangerHintLevel, levelValue(value));
    }
}

std::string formatRingEquipmentModifiers(const RingEquipmentModifiers& ring)
{
    std::ostringstream out;
    out << "speed=" << ring.ringSpeedMul
        << " radius=" << ring.ringRadiusMul
        << " weightAdd=" << ring.ringWeightLimitAdd
        << " shift=" << ring.ringShiftDistanceMul
        << " throwDist=" << ring.ringThrowDistanceMul
        << " throwSpeed=" << ring.ringThrowSpeedMul
        << " throwCooldown=" << ring.ringThrowCooldownMul
        << " return=" << ring.ringReturnSpeedMul
        << " output=" << ring.ringOutputMul
        << " dig=" << ring.digPowerMul
        << " damageSpeed=" << ring.ringDamageSpeedMul
        << " metalPenalty=" << ring.metalWeightPenaltyMul;
    return out.str();
}

}

void OrbitModifierAccumulator::clear()
{
    modifiers_ = OrbitModifiers{};
}

void OrbitModifierAccumulator::applyEffect(std::string_view effect, double value, std::string_view source)
{
    if (!isOrbitModifierEffect(effect)) {
        return;
    }

    const double multiplier = value == 0.0 ? 1.0 : value;
    if (effect == "orbit_speed") {
        modifiers_.speedMultiplier *= multiplier;
    } else if (effect == "orbit_power") {
        modifiers_.powerMultiplier *= multiplier;
    } else if (effect == "orbit_gravity") {
        modifiers_.gravityMultiplier *= multiplier;
    } else if (effect == "orbit_antigravity") {
        modifiers_.antigravityMultiplier *= multiplier;
    } else if (effect == "orbit_anchor") {
        modifiers_.anchorStrength += value;
    } else if (effect == "orbit_shift") {
        modifiers_.shiftMultiplier *= multiplier;
    } else if (effect == "damage_speed") {
        modifiers_.speedDamageMultiplier *= multiplier;
    }

    modifiers_.sources.push_back(OrbitModifierSource{
        .source = std::string(source),
        .effect = std::string(effect),
        .value = value,
    });
}

void OrbitModifierAccumulator::applyEffects(const std::vector<EffectSpec>& specs, std::string_view source)
{
    for (const EffectSpec& spec : specs) {
        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            const double value = index < spec.values.size() ? spec.values[index] : 0.0;
            applyEffect(spec.effects[index], value, source);
        }
    }
}

void OrbitModifierAccumulator::applyObject(const ObjectDefinition& object)
{
    applyEffects(object.orbitEffects, object.id);
}

const OrbitModifiers& OrbitModifierAccumulator::modifiers() const
{
    return modifiers_;
}

bool isOrbitModifierEffect(std::string_view effect)
{
    return effect == "orbit_speed" ||
        effect == "orbit_power" ||
        effect == "orbit_gravity" ||
        effect == "orbit_antigravity" ||
        effect == "orbit_anchor" ||
        effect == "orbit_shift" ||
        effect == "damage_speed";
}

OrbitModifiers collectOrbitModifiers(const std::vector<const ObjectDefinition*>& objects)
{
    OrbitModifierAccumulator accumulator;
    for (const ObjectDefinition* object : objects) {
        if (object != nullptr) {
            accumulator.applyObject(*object);
        }
    }
    return accumulator.modifiers();
}

bool isEquipmentModifierEffect(std::string_view effect)
{
    return effect == "ring_speed_mul" ||
        effect == "ring_radius_mul" ||
        effect == "ring_weight_limit_add" ||
        effect == "ring_shift_distance_mul" ||
        effect == "ring_throw_distance_mul" ||
        effect == "ring_throw_speed_mul" ||
        effect == "ring_throw_cooldown_mul" ||
        effect == "ring_return_speed_mul" ||
        effect == "ring_output_mul" ||
        effect == "ring_anchor_mul" ||
        effect == "ring_damage_speed_mul" ||
        effect == "light_radius_mul" ||
        effect == "detect_range_mul" ||
        effect == "guard_area_mul" ||
        effect == "reflect_power_mul" ||
        effect == "reflect_chance_add" ||
        effect == "metal_weight_penalty_mul" ||
        effect == "dig_power_mul" ||
        effect == "durability_cost_mul" ||
        effect == "sell_price_mul" ||
        effect == "money_visible_level" ||
        effect == "danger_hint_level";
}

const RingEquipmentModifiers& ringEquipmentModifiersForRing(const EquipmentModifiers& modifiers, int ringIndex)
{
    const int clampedRingIndex = std::clamp(ringIndex, 0, EquipmentModifierRingCount - 1);
    return modifiers.rings[static_cast<std::size_t>(clampedRingIndex)];
}

EquipmentModifiers collectStaffEquipmentModifiers(const ObjectDefinition& staffObject, std::string_view instanceId)
{
    EquipmentModifiers modifiers;
    modifiers.staffObjectId = staffObject.id;
    modifiers.staffInstanceId = std::string(instanceId);
    modifiers.staffName = staffObject.name.empty() ? staffObject.id : staffObject.name;

    for (const EffectSpec& spec : staffObject.normalEffects) {
        const std::vector<int> ringIndexes = equipmentTargetRingIndexes(spec.target);
        if (ringIndexes.empty()) {
            continue;
        }

        for (std::size_t index = 0; index < spec.effects.size(); ++index) {
            const std::string& effect = spec.effects[index];
            if (!isEquipmentModifierEffect(effect)) {
                continue;
            }
            const double value = index < spec.values.size() ? spec.values[index] : 0.0;
            applyGlobalEquipmentModifier(modifiers, effect, value);
            for (int ringIndex : ringIndexes) {
                if (ringIndex < 0 || ringIndex >= EquipmentModifierRingCount) {
                    continue;
                }
                applyRingEquipmentModifier(modifiers.rings[static_cast<std::size_t>(ringIndex)], effect, value);
            }
            modifiers.sources.push_back(EquipmentModifierSource{
                .source = staffObject.id,
                .target = std::string(spec.target),
                .effect = effect,
                .value = value,
            });
        }
    }

    return modifiers;
}

std::string equipmentModifiersDebugSummary(const EquipmentModifiers& modifiers)
{
    std::ostringstream out;
    if (modifiers.staffObjectId.empty()) {
        out << "none";
    } else {
        out << "staff=\"" << modifiers.staffName << "\" id=\"" << modifiers.staffObjectId
            << "\" instance=\"" << modifiers.staffInstanceId << "\" effects=" << modifiers.sources.size();
    }
    for (int ringIndex = 0; ringIndex < EquipmentModifierRingCount; ++ringIndex) {
        out << " r" << (ringIndex + 1) << "{" << formatRingEquipmentModifiers(
            modifiers.rings[static_cast<std::size_t>(ringIndex)]) << "}";
    }
    out << " global{durabilityCost=" << modifiers.durabilityCostMul
        << " sellPrice=" << modifiers.sellPriceMul
        << " moneyLevel=" << modifiers.moneyVisibleLevel
        << " dangerLevel=" << modifiers.dangerHintLevel
        << "}";
    return out.str();
}

}
