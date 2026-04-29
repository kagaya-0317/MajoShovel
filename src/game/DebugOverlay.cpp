#include "game/DebugOverlay.hpp"

#include <cstdio>

namespace majo {

void DebugOverlay::render(Renderer& renderer, const Time& time, const EnemySystem& enemies, const TileMap& map, const SpellRingSystem& spellRing, const Player& player, const RuntimeBalance& balance)
{
    if (!visible_) {
        return;
    }
    char buffer[512];
    std::snprintf(buffer, sizeof(buffer),
        "FPS: %03d\n敵: %03d\nチャンク: %02d\nリング半径: %03d\nリング速度: %.2f\nレベル: %02d 経験値: %02d/%02d\n投げ待ち: %02d%%",
        static_cast<int>(time.fps()),
        enemies.activeCount(),
        map.activeChunkCount(),
        static_cast<int>(spellRing.radius()),
        spellRing.angularSpeed(),
        player.level,
        player.xp,
        player.xpToNext,
        static_cast<int>(spellRing.cooldownRatio(player, balance) * 100.0f));
    renderer.setScreenSpace();
    renderer.fillRect({10.0f, 10.0f}, {270.0f, 150.0f}, {0, 0, 0, 180});
    renderer.drawText({20.0f, 20.0f}, buffer, {220, 244, 224, 255}, 2);
}

}
