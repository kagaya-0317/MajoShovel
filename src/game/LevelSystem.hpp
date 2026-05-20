#pragma once

#include "game/Player.hpp"
#include "data/RuntimeBalance.hpp"

namespace majo {

inline constexpr int PlayerMaxLevel = 100;

struct LevelGainResult {
    int levelsGained = 0;
    bool reachedMaxLevel = false;
};

[[nodiscard]] int playerMaxHpForLevel(int level);
[[nodiscard]] int playerXpToNextForLevel(int level, const RuntimeBalance& balance);
[[nodiscard]] bool playerAtMaxLevel(const Player& player);

class LevelSystem {
public:
    LevelGainResult addXp(Player& player, int amount, const RuntimeBalance& balance);
    bool isChoosing() const { return choosing_; }
    void beginChoice() { choosing_ = true; }
    void finishChoice() { choosing_ = false; }

private:
    bool choosing_ = false;
};

}
