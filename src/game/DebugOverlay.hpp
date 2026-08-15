#pragma once

#include "engine/Renderer.hpp"
#include "engine/Time.hpp"
#include "data/RuntimeBalance.hpp"
#include "data/StageCatalog.hpp"
#include "devtools/autosim/AutoSimulationTypes.hpp"
#include "game/EnemySystem.hpp"
#include "game/DungeonLayout.hpp"
#include "game/SpellRingSystem.hpp"
#include "game/Player.hpp"
#include "game/TileMap.hpp"

#include <cstddef>

namespace majo {

class DebugOverlay {
public:
    void toggle() { visible_ = !visible_; }
    bool visible() const { return visible_; }
    static void appendAutoSimulationText(
        char* buffer,
        std::size_t bufferSize,
        bool enabled,
        const autosim::AutoSimulationDebugSnapshot& debug,
        bool includeDungeonDetails);
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
        int unlockedWarpCount,
        bool hasLatestWarpPointPosition,
        Vec2 latestWarpPointPosition,
        bool hasRequestedWarpPointStartPosition,
        Vec2 requestedWarpPointStartPosition,
        int rewardNodeCount,
        int moneyNodeCount,
        int buriedVisibleNodeCount,
        int buriedHiddenNodeCount,
        int exposedEnemyNodeCount,
        int buriedEnemyNodeCount,
        int spawnedEnemyNodeCount,
        bool autoReloadBlocked,
        bool autoSimulationDebugActive,
        const autosim::AutoSimulationDebugSnapshot& autoSimulationDebug);

private:
    bool visible_ = false;
};

}
