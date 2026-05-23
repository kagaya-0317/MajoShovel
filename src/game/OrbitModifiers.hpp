#pragma once

#include "data/ObjectCatalog.hpp"

#include <array>
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
    double speedDamageMultiplier = 1.0;
    std::vector<OrbitModifierSource> sources;
};

constexpr int EquipmentModifierRingCount = 3;

struct EquipmentModifierSource {
    std::string source;
    std::string target;
    std::string effect;
    double value = 0.0;
};

struct RingEquipmentModifiers {
    double ringSpeedMul = 1.0;
    double ringRadiusMul = 1.0;
    double ringWeightLimitAdd = 0.0;
    double ringShiftDistanceMul = 1.0;
    double ringThrowDistanceMul = 1.0;
    double ringThrowSpeedMul = 1.0;
    double ringThrowCooldownMul = 1.0;
    double ringReturnSpeedMul = 1.0;
    double ringOutputMul = 1.0;
    double ringAnchorMul = 1.0;
    double ringDamageSpeedMul = 1.0;
    double lightRadiusMul = 1.0;
    double detectRangeMul = 1.0;
    double guardAreaMul = 1.0;
    double reflectPowerMul = 1.0;
    double reflectChanceAdd = 0.0;
    double metalWeightPenaltyMul = 1.0;
    double digPowerMul = 1.0;
};

struct EquipmentModifiers {
    std::array<RingEquipmentModifiers, EquipmentModifierRingCount> rings{};
    double durabilityCostMul = 1.0;
    double sellPriceMul = 1.0;
    int moneyVisibleLevel = 0;
    int dangerHintLevel = 0;
    std::string staffObjectId;
    std::string staffInstanceId;
    std::string staffName;
    std::vector<EquipmentModifierSource> sources;
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
[[nodiscard]] bool isEquipmentModifierEffect(std::string_view effect);
[[nodiscard]] const RingEquipmentModifiers& ringEquipmentModifiersForRing(const EquipmentModifiers& modifiers, int ringIndex);
[[nodiscard]] EquipmentModifiers collectStaffEquipmentModifiers(const ObjectDefinition& staffObject, std::string_view instanceId);
[[nodiscard]] std::string equipmentModifiersDebugSummary(const EquipmentModifiers& modifiers);

}
