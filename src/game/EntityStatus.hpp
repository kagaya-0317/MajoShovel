#pragma once

#include "data/ObjectCatalog.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace majo {

enum class StateApplyMode {
    Overwrite,
    ExtendDuration,
    KeepLonger,
};

enum class ModifierStat {
    Attack,
    Speed,
    Defense,
};

struct EntityState {
    std::string stateId;
    double value = 0.0;
    double duration = 0.0;
    std::string source;
};

struct EntityModifier {
    std::string modifierId;
    ModifierStat stat = ModifierStat::Attack;
    double multiplier = 1.0;
    double flat = 0.0;
    double duration = 0.0;
    std::string source;
};

class EntityStatus {
public:
    void update(double dt);
    void clear();

    void applyState(
        std::string stateId,
        double value,
        double duration,
        std::string source,
        StateApplyMode mode = StateApplyMode::Overwrite);
    bool removeState(std::string_view stateId);
    [[nodiscard]] bool hasState(std::string_view stateId) const;
    [[nodiscard]] const std::vector<EntityState>& states() const;

    void applyModifier(
        std::string modifierId,
        ModifierStat stat,
        double multiplier,
        double flat,
        double duration,
        std::string source,
        StateApplyMode mode = StateApplyMode::Overwrite);
    bool removeModifier(std::string_view modifierId);
    [[nodiscard]] double multiplierFor(ModifierStat stat) const;
    [[nodiscard]] double flatBonusFor(ModifierStat stat) const;
    [[nodiscard]] double applyModifiers(ModifierStat stat, double baseValue) const;
    [[nodiscard]] const std::vector<EntityModifier>& modifiers() const;

private:
    std::vector<EntityState> states_;
    std::vector<EntityModifier> modifiers_;
};

enum class ElementType {
    Fire,
    Ice,
    Thunder,
    Wind,
    Earth,
    Water,
    Poison,
};

[[nodiscard]] std::string_view elementTag(ElementType element);
[[nodiscard]] bool isElementTag(std::string_view tag);
[[nodiscard]] bool objectHasElementTag(const ObjectDefinition& object, ElementType element);
[[nodiscard]] std::vector<ElementType> elementTagsForObject(const ObjectDefinition& object);
[[nodiscard]] bool modifierStatFromEffect(std::string_view effect, ModifierStat& outStat);
[[nodiscard]] std::string_view modifierStatName(ModifierStat stat);

}
