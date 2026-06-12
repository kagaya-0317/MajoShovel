#pragma once

#include "game/SpellRingSystem.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace majo {

inline int clampedRingDisplayUnlockedCount(int unlockedRingCount)
{
    return std::clamp(unlockedRingCount, 1, SpellRingCount);
}

inline std::string_view namedRingDisplayName(int ringIndex)
{
    constexpr std::array<std::string_view, SpellRingCount> Names{{
        "リング0",
        "リング8",
        "リングC",
    }};
    return Names[static_cast<std::size_t>(std::clamp(ringIndex, 0, SpellRingCount - 1))];
}

inline std::string_view ringDisplayName(int ringIndex, int unlockedRingCount)
{
    if (clampedRingDisplayUnlockedCount(unlockedRingCount) <= 1) {
        return "リング";
    }
    return namedRingDisplayName(ringIndex);
}

inline std::string ringDisplayNameWithSuffix(int ringIndex, int unlockedRingCount, std::string_view suffix)
{
    std::string name(ringDisplayName(ringIndex, unlockedRingCount));
    name += suffix;
    return name;
}

inline std::string ringDisplayNameWithSpaceSuffix(int ringIndex, int unlockedRingCount, std::string_view suffix)
{
    std::string name(ringDisplayName(ringIndex, unlockedRingCount));
    if (!suffix.empty()) {
        name += ' ';
        name += suffix;
    }
    return name;
}

}
