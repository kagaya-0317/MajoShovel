#pragma once

#include "game/Player.hpp"
#include "data/RuntimeBalance.hpp"

namespace majo {

enum class UpgradeChoice {
    Radius,
    Speed,
    ShovelPower
};

class LevelSystem {
public:
    void addXp(Player& player, int amount, const RuntimeBalance& balance);
    bool isChoosing() const { return choosing_; }
    void beginChoice() { choosing_ = true; }
    void finishChoice() { choosing_ = false; }

private:
    bool choosing_ = false;
};

}
