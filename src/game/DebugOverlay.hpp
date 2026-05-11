#pragma once

#include "engine/Renderer.hpp"
#include "engine/Time.hpp"
#include "data/RuntimeBalance.hpp"
#include "data/StageCatalog.hpp"
#include "game/EnemySystem.hpp"
#include "game/DungeonLayout.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/Player.hpp"
#include "game/TileMap.hpp"

namespace majo {

class DebugOverlay {
public:
    void toggle() { visible_ = !visible_; }
    bool visible() const { return visible_; }
    void render(
        Renderer& renderer,
        const Time& time,
        const EnemySystem& enemies,
        const TileMap& map,
        const SpellRingSystem& spellRing,
        const Player& player,
        const RuntimeBalance& balance,
        const DungeonLayout& dungeonLayout,
        const StageDefinition& currentStage,
        int nearestWarpIndex,
        bool nearestWarpDiscovered,
        int discoveredWarpCount,
        int rewardNodeCount,
        int moneyNodeCount,
        int buriedVisibleNodeCount,
        int buriedHiddenNodeCount,
        int exposedEnemyNodeCount,
        int buriedEnemyNodeCount,
        int spawnedEnemyNodeCount,
        bool autoReloadBlocked);

private:
    bool visible_ = true;
};

}
