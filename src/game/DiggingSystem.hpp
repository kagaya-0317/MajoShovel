#pragma once

#include "data/ObjectCatalog.hpp"
#include "game/EffectDispatcher.hpp"
#include "game/Player.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/TileMap.hpp"
#include <vector>

namespace majo {

struct DugTile {
    Vec2 center{};
    TileType type = TileType::Dirt;
};
class EncyclopediaSystem;

class DiggingSystem {
public:
    void update(
        TileMap& map,
        SpellRingSystem& spellRing,
        Player& player,
        float totalTime,
        const ObjectCatalog& objectCatalog,
        const EffectDispatcher& effectDispatcher,
        std::vector<EffectDiscoveryEvent>* discoveryEvents = nullptr,
        const EncyclopediaSystem* encyclopedia = nullptr);
    const std::vector<Vec2>& openedTiles() const { return openedTiles_; }
    const std::vector<Vec2>& hitTiles() const { return hitTiles_; }
    const std::vector<DugTile>& dugTiles() const { return dugTiles_; }
    const std::vector<Vec2>& rewardDropRequests() const { return rewardDropRequests_; }
    const std::vector<Vec2>& capturedExplosionRequests() const { return capturedExplosionRequests_; }

private:
    std::vector<Vec2> openedTiles_;
    std::vector<Vec2> hitTiles_;
    std::vector<DugTile> dugTiles_;
    std::vector<Vec2> rewardDropRequests_;
    std::vector<Vec2> capturedExplosionRequests_;
};

}
