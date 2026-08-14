#pragma once

#include "data/ObjectCatalog.hpp"
#include "game/EffectDispatcher.hpp"
#include "game/Player.hpp"
#include "game/RingImpactSound.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/TileMap.hpp"
#include <string>
#include <vector>

namespace majo {

struct DugTile {
    Vec2 center{};
    TileType type = TileType::Dirt;
    Color color{105, 68, 37, 255};
};

struct TerrainHitTile {
    Vec2 center{};
    Color color{105, 68, 37, 255};
};

struct CapturedRewardDropRequest {
    Vec2 position{};
    std::string profile;
};

struct CapturedExplosionRequest {
    Vec2 position{};
    float radius = 44.0f;
    int damage = 3;
    float terrainRadius = 28.0f;
    int terrainDamage = 1;
    bool destroyTerrain = false;
};

class EncyclopediaSystem;
struct HitboxCatalog;
class MagicSystem;

class DiggingSystem {
public:
    void update(
        TileMap& map,
        SpellRingSystem& spellRing,
        Player& player,
        float totalTime,
        float dt,
        const ObjectCatalog& objectCatalog,
        const HitboxCatalog* hitboxCatalog,
        const EffectDispatcher& effectDispatcher,
        MagicSystem* magic = nullptr,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    const std::vector<Vec2>& openedTiles() const { return openedTiles_; }
    const std::vector<TerrainHitTile>& hitTiles() const { return hitTiles_; }
    const std::vector<DugTile>& dugTiles() const { return dugTiles_; }
    const std::vector<RingImpactSoundEvent>& impactSoundEvents() const { return impactSoundEvents_; }
    const std::vector<CapturedRewardDropRequest>& rewardDropRequests() const { return rewardDropRequests_; }
    const std::vector<CapturedExplosionRequest>& capturedExplosionRequests() const { return capturedExplosionRequests_; }

private:
    std::vector<Vec2> openedTiles_;
    std::vector<TerrainHitTile> hitTiles_;
    std::vector<DugTile> dugTiles_;
    std::vector<RingImpactSoundEvent> impactSoundEvents_;
    std::vector<CapturedRewardDropRequest> rewardDropRequests_;
    std::vector<CapturedExplosionRequest> capturedExplosionRequests_;
};

}
