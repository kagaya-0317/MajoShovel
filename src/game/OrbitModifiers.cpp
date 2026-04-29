#include "game/OrbitModifiers.hpp"

namespace majo {

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
        effect == "orbit_shift";
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

}
