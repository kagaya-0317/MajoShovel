#pragma once

#include "game/Player.hpp"
#include "data/RuntimeBalance.hpp"

#include <algorithm>

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
    bool isChoosing() const { return pendingChoiceCount_ > 0; }
    int pendingChoiceCount() const { return pendingChoiceCount_; }
    void beginChoice() { pendingChoiceCount_ = std::max(1, pendingChoiceCount_); }
    void finishChoice() { pendingChoiceCount_ = std::max(0, pendingChoiceCount_ - 1); }
    void clearChoices() { pendingChoiceCount_ = 0; }
    void setPendingChoiceCount(int count) { pendingChoiceCount_ = std::max(0, count); }

private:
    int pendingChoiceCount_ = 0;
};

}
