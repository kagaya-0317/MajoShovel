#include "game/LevelSystem.hpp"

#include <algorithm>
#include <array>

namespace majo {

namespace {

constexpr std::array<int, PlayerMaxLevel> PlayerMaxHpTable{{
    10, 13, 16, 18, 22, 25, 27, 30, 33, 36,
    38, 40, 43, 44, 46, 48, 49, 52, 54, 56,
    58, 59, 61, 62, 64, 65, 66, 68, 70, 72,
    73, 75, 76, 77, 79, 80, 81, 82, 84, 85,
    86, 87, 89, 90, 91, 92, 93, 94, 95, 96,
    97, 98, 99, 101, 102, 103, 104, 105, 106, 107,
    108, 109, 111, 112, 113, 114, 115, 116, 117, 118,
    119, 120, 121, 122, 124, 125, 126, 127, 128, 129,
    130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
    140, 141, 142, 144, 145, 146, 147, 148, 149, 150,
}};

}

int playerMaxHpForLevel(int level)
{
    const int clampedLevel = std::clamp(level, 1, PlayerMaxLevel);
    return PlayerMaxHpTable[static_cast<std::size_t>(clampedLevel - 1)];
}

int playerXpToNextForLevel(int level, const RuntimeBalance& balance)
{
    const int clampedLevel = std::clamp(level, 1, PlayerMaxLevel);
    if (clampedLevel >= PlayerMaxLevel) {
        return 0;
    }
    return std::max(1, balance.xpBase + clampedLevel * balance.xpPerLevel);
}

bool playerAtMaxLevel(const Player& player)
{
    return player.level >= PlayerMaxLevel;
}

LevelGainResult LevelSystem::addXp(Player& player, int amount, const RuntimeBalance& balance)
{
    LevelGainResult result;
    player.level = std::clamp(player.level, 1, PlayerMaxLevel);
    player.xpToNext = playerXpToNextForLevel(player.level, balance);
    if (playerAtMaxLevel(player)) {
        player.xp = 0;
        result.reachedMaxLevel = true;
        return result;
    }
    if (amount <= 0) {
        return result;
    }

    player.xp += amount;
    while (!playerAtMaxLevel(player) && player.xpToNext > 0 && player.xp >= player.xpToNext) {
        player.xp -= player.xpToNext;
        ++player.level;
        ++result.levelsGained;
        player.xpToNext = playerXpToNextForLevel(player.level, balance);
        choosing_ = true;
    }
    if (playerAtMaxLevel(player)) {
        player.level = PlayerMaxLevel;
        player.xp = 0;
        player.xpToNext = 0;
        result.reachedMaxLevel = true;
    }
    return result;
}

}
