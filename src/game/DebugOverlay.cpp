#include "game/DebugOverlay.hpp"

#include <cstdio>

namespace majo {

void DebugOverlay::render(Renderer& renderer, const Time& time, const EnemySystem& enemies, const TileMap& map, const OrbitSystem& orbit, const Player& player, const RuntimeBalance& balance)
{
    if (!visible_) {
        return;
    }
    char buffer[512];
    std::snprintf(buffer, sizeof(buffer),
        "FPS: %03d\nENEMIES: %03d\nCHUNKS: %02d\nRADIUS: %03d\nSPEED: %.2f\nLV: %02d XP: %02d/%02d\nTHROW CD: %02d%%",
        static_cast<int>(time.fps()),
        enemies.activeCount(),
        map.activeChunkCount(),
        static_cast<int>(orbit.radius()),
        orbit.angularSpeed(),
        player.level,
        player.xp,
        player.xpToNext,
        static_cast<int>(orbit.cooldownRatio(player, balance) * 100.0f));
    renderer.setScreenSpace();
    renderer.fillRect({10.0f, 10.0f}, {270.0f, 150.0f}, {0, 0, 0, 180});
    renderer.drawText({20.0f, 20.0f}, buffer, {220, 244, 224, 255}, 2);
}

}
