#include "game/EffectDispatcher.hpp"

#include "engine/Log.hpp"
#include "game/Enemy.hpp"
#include "game/EntityStatus.hpp"
#include "game/OrbitModifiers.hpp"
#include "game/Player.hpp"
#include "game/SpellRingSystem.hpp"

#include <sstream>
#include <string>
#include <utility>

namespace majo {

namespace {

std::string_view objectIdOrNone(const EffectContext& context)
{
    if (context.sourceObject == nullptr) {
        return "<none>";
    }
    return context.sourceObject->id;
}

bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::string sourceIdFor(const EffectInvocation& invocation)
{
    if (invocation.context->sourceObject == nullptr) {
        return {};
    }
    return invocation.context->sourceObject->id;
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
    if (context.hitTarget != nullptr) {
        return &context.hitTarget->status;
    }
    if (context.targetEntity != nullptr) {
        return &context.targetEntity->status;
    }
    return context.owner != nullptr ? &context.owner->status : nullptr;
}

void applyStateInvocation(const EffectInvocation& invocation)
{
    EntityStatus* status = statusForInvocation(invocation);
    if (status == nullptr) {
        return;
    }
    status->applyState(
        std::string(invocation.effect),
        invocation.value,
        invocation.duration,
        sourceIdFor(invocation),
        StateApplyMode::KeepLonger);
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

    const double multiplier = invocation.value == 0.0 ? 1.0 : invocation.value;
    status->applyModifier(
        std::string(invocation.effect),
        stat,
        multiplier,
        0.0,
        invocation.duration,
        sourceIdFor(invocation),
        StateApplyMode::KeepLonger);
}

void applyOrbitModifierInvocation(const EffectInvocation& invocation)
{
    if (invocation.context->orbit == nullptr) {
        return;
    }

    invocation.context->orbit->applyOrbitModifierEffect(
        invocation.effect,
        invocation.value,
        sourceIdFor(invocation));
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
    for (const auto& [effect, definition] : catalog.effectCodes) {
        (void)definition;
        if (startsWith(effect, "status_") && effect.find("_chance") == std::string::npos) {
            registerHandler(effect, applyStateInvocation);
            continue;
        }

        ModifierStat stat = ModifierStat::Attack;
        if (modifierStatFromEffect(effect, stat)) {
            registerHandler(effect, applyModifierInvocation);
            continue;
        }

        if (isOrbitModifierEffect(effect)) {
            registerHandler(effect, applyOrbitModifierInvocation);
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
        logUnimplemented(invocation);
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
