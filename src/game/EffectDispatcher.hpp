#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Math.hpp"
#include "game/EncyclopediaSystem.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace majo {

struct Enemy;
struct Player;
struct DugTile;
struct TerrainHitTile;
struct SpellRingItem;
class EffectSystem;
class MagicSystem;
class SpellRingSystem;
class TileMap;
class EncyclopediaSystem;

enum class EffectTriggerType {
    Unknown,
    NormalUse,
    Orbit,
    Hit,
    Debug,
};

enum class ProjectileKind {
    Physical,
    Magic,
    Water,
};

struct ProjectileContext {
    ProjectileKind kind = ProjectileKind::Physical;
    bool large = false;
    bool guarded = false;
    bool largeGuarded = false;
    bool reflected = false;
    Vec2 position{};
    Vec2 velocity{};
    std::string reflectedBy;
};

struct EffectContext {
    const ObjectDefinition* sourceObject = nullptr;
    Player* owner = nullptr;
    Enemy* targetEntity = nullptr;
    Enemy* hitTarget = nullptr;
    ProjectileContext* projectile = nullptr;
    SpellRingSystem* orbit = nullptr;
    SpellRingItem* orbitItem = nullptr;
    TileMap* tileMap = nullptr;
    EffectSystem* effects = nullptr;
    MagicSystem* magic = nullptr;
    std::vector<TerrainHitTile>* terrainHitTiles = nullptr;
    std::vector<Vec2>* terrainOpenedTiles = nullptr;
    std::vector<DugTile>* terrainDugTiles = nullptr;
    std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr;
    const EncyclopediaSystem* encyclopedia = nullptr;
    Vec2 position{};
    EffectTriggerType triggerType = EffectTriggerType::Unknown;
    bool logUnimplementedEffects = true;
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
