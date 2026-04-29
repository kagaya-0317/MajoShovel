#pragma once

#include "data/ObjectCatalog.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace majo {

struct OrbitModifierSource {
    std::string source;
    std::string effect;
    double value = 0.0;
};

struct OrbitModifiers {
    double speedMultiplier = 1.0;
    double powerMultiplier = 1.0;
    double gravityMultiplier = 1.0;
    double antigravityMultiplier = 1.0;
    double anchorStrength = 0.0;
    double shiftMultiplier = 1.0;
    std::vector<OrbitModifierSource> sources;
};

class OrbitModifierAccumulator {
public:
    void clear();
    void applyEffect(std::string_view effect, double value, std::string_view source);
    void applyEffects(const std::vector<EffectSpec>& specs, std::string_view source);
    void applyObject(const ObjectDefinition& object);

    [[nodiscard]] const OrbitModifiers& modifiers() const;

private:
    OrbitModifiers modifiers_;
};

[[nodiscard]] bool isOrbitModifierEffect(std::string_view effect);
[[nodiscard]] OrbitModifiers collectOrbitModifiers(const std::vector<const ObjectDefinition*>& objects);

}
