#include "game/LevelSystem.hpp"

namespace majo {

void LevelSystem::addXp(Player& player, int amount, const RuntimeBalance& balance)
{
    player.xp += amount;
    while (player.xp >= player.xpToNext) {
        player.xp -= player.xpToNext;
        ++player.level;
        player.xpToNext = balance.xpBase + player.level * balance.xpPerLevel;
        choosing_ = true;
    }
}

}
