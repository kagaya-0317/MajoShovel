#pragma once

#include "game/SpellRingSystem.hpp"

#include <algorithm>

namespace majo {

enum class MenuIconImage {
    ScreenSettings = 25,
    Volume = 26,
    Gamepad = 27,
    Status = 28,
    Backpack = 29,
    Options = 30,
    QuitGame = 31,
    StorageChest = 32,
    Ring0 = 33,
    Ring8 = 34,
    RingC = 35,
};

inline constexpr int menuIconImageNumber(MenuIconImage image)
{
    return static_cast<int>(image);
}

inline int ringMenuIconImageNumber(int ringIndex)
{
    return menuIconImageNumber(static_cast<MenuIconImage>(
        menuIconImageNumber(MenuIconImage::Ring0) + std::clamp(ringIndex, 0, SpellRingCount - 1)));
}

}
