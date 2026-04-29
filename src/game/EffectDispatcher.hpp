#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Math.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

struct Enemy;
struct Player;
struct SpellRingItem;
class SpellRingSystem;

enum class EffectTriggerType {
    Unknown,
    NormalUse,
    Orbit,
    Hit,
    Debug,
};

struct EffectContext {
    const ObjectDefinition* sourceObject = nullptr;
    Player* owner = nullptr;
    Enemy* targetEntity = nullptr;
    Enemy* hitTarget = nullptr;
    SpellRingSystem* orbit = nullptr;
    SpellRingItem* orbitItem = nullptr;
    Vec2 position{};
    EffectTriggerType triggerType = EffectTriggerType::Unknown;
};

struct EffectInvocation {
    const EffectSpec* spec = nullptr;
    const EffectContext* context = nullptr;
    std::string_view target;
    std::string_view effect;
    double value = 0.0;
    double duration = 0.0;
};

class EffectDispatcher {
public:
    using Handler = std::function<void(const EffectInvocation&)>;

    void registerHandler(std::string effect, Handler handler);
    void unregisterHandler(std::string_view effect);
    void clearHandlers();
    void registerFoundationHandlers(const ObjectCatalog& catalog);

    [[nodiscard]] bool hasHandler(std::string_view effect) const;
    [[nodiscard]] std::size_t handlerCount() const;

    void dispatch(const std::vector<EffectSpec>& specs, const EffectContext& context) const;
    void dispatchNormalEffects(const ObjectDefinition& object, EffectContext context) const;
    void dispatchOrbitEffects(const ObjectDefinition& object, EffectContext context) const;

private:
    void dispatchOne(const EffectSpec& spec, std::size_t effectIndex, const EffectContext& context) const;
    void logUnimplemented(const EffectInvocation& invocation) const;

    std::unordered_map<std::string, Handler> handlers_;
};

std::string_view effectTriggerTypeName(EffectTriggerType triggerType);

}
