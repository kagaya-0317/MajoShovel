#include "game/EntityStatus.hpp"

#include <algorithm>
#include <array>

namespace majo {

namespace {

bool hasExpired(double duration)
{
    return duration == 0.0;
}

void mergeDuration(double& current, double incoming, StateApplyMode mode)
{
    if (incoming < 0.0) {
        current = incoming;
        return;
    }

    if (current < 0.0) {
        return;
    }

    switch (mode) {
    case StateApplyMode::Overwrite:
        current = incoming;
        break;
    case StateApplyMode::ExtendDuration:
        current += incoming;
        break;
    case StateApplyMode::KeepLonger:
        current = std::max(current, incoming);
        break;
    }
}

template <typename T>
bool idEquals(const T& item, std::string_view id)
{
    if constexpr (requires { item.stateId; }) {
        return item.stateId == id;
    } else {
        return item.modifierId == id;
    }
}

constexpr std::array<std::pair<ElementType, std::string_view>, 7> ElementTags{{
    {ElementType::Fire, "fire"},
    {ElementType::Ice, "ice"},
    {ElementType::Thunder, "thunder"},
    {ElementType::Wind, "wind"},
    {ElementType::Earth, "earth"},
    {ElementType::Water, "water"},
    {ElementType::Poison, "poison"},
}};

}

void EntityStatus::update(double dt)
{
    if (dt <= 0.0) {
        return;
    }

    for (EntityState& state : states_) {
        if (state.duration > 0.0) {
            state.duration = std::max(0.0, state.duration - dt);
        }
    }
    states_.erase(
        std::remove_if(states_.begin(), states_.end(), [](const EntityState& state) {
            return hasExpired(state.duration);
        }),
        states_.end());

    for (EntityModifier& modifier : modifiers_) {
        if (modifier.duration > 0.0) {
            modifier.duration = std::max(0.0, modifier.duration - dt);
        }
    }
    modifiers_.erase(
        std::remove_if(modifiers_.begin(), modifiers_.end(), [](const EntityModifier& modifier) {
            return hasExpired(modifier.duration);
        }),
        modifiers_.end());
}

void EntityStatus::clear()
{
    states_.clear();
    modifiers_.clear();
}

void EntityStatus::applyState(
    std::string stateId,
    double value,
    double duration,
    std::string source,
    StateApplyMode mode)
{
    if (stateId.empty()) {
        return;
    }

    for (EntityState& state : states_) {
        if (state.stateId != stateId) {
            continue;
        }
        state.value = value;
        state.source = std::move(source);
        mergeDuration(state.duration, duration, mode);
        return;
    }

    states_.push_back(EntityState{
        .stateId = std::move(stateId),
        .value = value,
        .duration = duration,
        .source = std::move(source),
    });
}

bool EntityStatus::removeState(std::string_view stateId)
{
    const auto previousSize = states_.size();
    states_.erase(
        std::remove_if(states_.begin(), states_.end(), [stateId](const EntityState& state) {
            return state.stateId == stateId;
        }),
        states_.end());
    return states_.size() != previousSize;
}

bool EntityStatus::hasState(std::string_view stateId) const
{
    return std::any_of(states_.begin(), states_.end(), [stateId](const EntityState& state) {
        return state.stateId == stateId;
    });
}

const std::vector<EntityState>& EntityStatus::states() const
{
    return states_;
}

void EntityStatus::applyModifier(
    std::string modifierId,
    ModifierStat stat,
    double multiplier,
    double flat,
    double duration,
    std::string source,
    StateApplyMode mode)
{
    if (modifierId.empty()) {
        return;
    }

    for (EntityModifier& modifier : modifiers_) {
        if (modifier.modifierId != modifierId) {
            continue;
        }
        modifier.stat = stat;
        modifier.multiplier = multiplier;
        modifier.flat = flat;
        modifier.source = std::move(source);
        mergeDuration(modifier.duration, duration, mode);
        return;
    }

    modifiers_.push_back(EntityModifier{
        .modifierId = std::move(modifierId),
        .stat = stat,
        .multiplier = multiplier,
        .flat = flat,
        .duration = duration,
        .source = std::move(source),
    });
}

bool EntityStatus::removeModifier(std::string_view modifierId)
{
    const auto previousSize = modifiers_.size();
    modifiers_.erase(
        std::remove_if(modifiers_.begin(), modifiers_.end(), [modifierId](const EntityModifier& modifier) {
            return modifier.modifierId == modifierId;
        }),
        modifiers_.end());
    return modifiers_.size() != previousSize;
}

int EntityStatus::removeModifiersBySourcePrefix(std::string_view sourcePrefix)
{
    const auto previousSize = modifiers_.size();
    modifiers_.erase(
        std::remove_if(modifiers_.begin(), modifiers_.end(), [sourcePrefix](const EntityModifier& modifier) {
            return modifier.source.size() >= sourcePrefix.size() &&
                std::string_view(modifier.source).substr(0, sourcePrefix.size()) == sourcePrefix;
        }),
        modifiers_.end());
    return static_cast<int>(previousSize - modifiers_.size());
}

double EntityStatus::multiplierFor(ModifierStat stat) const
{
    double result = 1.0;
    for (const EntityModifier& modifier : modifiers_) {
        if (modifier.stat == stat) {
            result *= modifier.multiplier;
        }
    }
    return result;
}

double EntityStatus::flatBonusFor(ModifierStat stat) const
{
    double result = 0.0;
    for (const EntityModifier& modifier : modifiers_) {
        if (modifier.stat == stat) {
            result += modifier.flat;
        }
    }
    return result;
}

double EntityStatus::applyModifiers(ModifierStat stat, double baseValue) const
{
    return baseValue * multiplierFor(stat) + flatBonusFor(stat);
}

double EntityStatus::movementMultiplierFromStates() const
{
    if (hasState("status_paralyze") || hasState("status_sleep") || hasState("status_stun") || hasState("status_frozen")) {
        return 0.0;
    }

    double result = 1.0;
    for (const EntityState& state : states_) {
        if (state.stateId == "status_slow") {
            const double slowMultiplier = state.value > 0.0 ? state.value : 0.65;
            result *= std::clamp(slowMultiplier, 0.0, 1.0);
        }
    }
    return result;
}

double EntityStatus::attackAccuracyMultiplierFromStates() const
{
    double result = 1.0;
    for (const EntityState& state : states_) {
        if (state.stateId == "status_blind") {
            const double accuracyMultiplier = state.value > 0.0 ? state.value : 0.5;
            result *= std::clamp(accuracyMultiplier, 0.0, 1.0);
        }
    }
    return result;
}

double EntityStatus::sizeMultiplierFromStates() const
{
    double result = 1.0;
    for (const EntityState& state : states_) {
        if (state.stateId == "status_giant") {
            const double strength = state.value > 0.0 ? state.value : 1.0;
            result = std::max(result, std::clamp(1.0 + strength * 0.5, 1.0, 2.0));
        }
    }
    return result;
}

double EntityStatus::poisonDamagePerSecond() const
{
    double result = 0.0;
    for (const EntityState& state : states_) {
        if (state.stateId == "status_poison") {
            result += state.value > 0.0 ? state.value : 1.0;
        }
    }
    return result;
}

double EntityStatus::hotDamagePerSecond() const
{
    double result = 0.0;
    for (const EntityState& state : states_) {
        if (state.stateId == "status_hot") {
            result += state.value > 0.0 ? state.value : 1.0;
        }
    }
    return result;
}

double EntityStatus::bleedDamagePerSecond() const
{
    double result = 0.0;
    for (const EntityState& state : states_) {
        if (state.stateId == "status_bleed") {
            result += state.value > 0.0 ? state.value : 1.0;
        }
    }
    return result;
}

const std::vector<EntityModifier>& EntityStatus::modifiers() const
{
    return modifiers_;
}

std::string_view elementTag(ElementType element)
{
    for (const auto& [candidate, tag] : ElementTags) {
        if (candidate == element) {
            return tag;
        }
    }
    return "";
}

bool isElementTag(std::string_view tag)
{
    return std::any_of(ElementTags.begin(), ElementTags.end(), [tag](const auto& entry) {
        return entry.second == tag;
    });
}

bool objectHasElementTag(const ObjectDefinition& object, ElementType element)
{
    const std::string_view expected = elementTag(element);
    return std::any_of(object.tags.begin(), object.tags.end(), [expected](const std::string& tag) {
        return tag == expected;
    });
}

std::vector<ElementType> elementTagsForObject(const ObjectDefinition& object)
{
    std::vector<ElementType> elements;
    for (const std::string& tag : object.tags) {
        for (const auto& [element, elementTagText] : ElementTags) {
            if (tag == elementTagText) {
                elements.push_back(element);
            }
        }
    }
    return elements;
}

bool modifierStatFromEffect(std::string_view effect, ModifierStat& outStat)
{
    if (effect == "buff_attack" || effect == "debuff_attack") {
        outStat = ModifierStat::Attack;
        return true;
    }
    if (effect == "buff_speed" || effect == "debuff_speed") {
        outStat = ModifierStat::Speed;
        return true;
    }
    if (effect == "buff_defense" || effect == "debuff_defense") {
        outStat = ModifierStat::Defense;
        return true;
    }
    return false;
}

std::string_view modifierStatName(ModifierStat stat)
{
    switch (stat) {
    case ModifierStat::Attack:
        return "attack";
    case ModifierStat::Speed:
        return "speed";
    case ModifierStat::Defense:
        return "defense";
    }
    return "unknown";
}

}
