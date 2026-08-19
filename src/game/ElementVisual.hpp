#pragma once

#include "engine/RendererTypes.hpp"

#include <string_view>

namespace majo {

inline constexpr Color DefaultActionFlashColor{255, 244, 214, 255};

[[nodiscard]] constexpr Color elementVisualColor(std::string_view element)
{
    if (element == "fire") {
        return {255, 116, 32, 255};
    }
    if (element == "ice") {
        return {116, 214, 255, 255};
    }
    if (element == "thunder" || element == "paralyze") {
        return {255, 232, 80, 255};
    }
    if (element == "wind") {
        return {138, 238, 178, 255};
    }
    if (element == "earth") {
        return {190, 142, 82, 255};
    }
    if (element == "water") {
        return {78, 166, 255, 255};
    }
    if (element == "poison") {
        return {174, 104, 224, 255};
    }
    if (element == "magic" || element == "web") {
        return {190, 118, 255, 255};
    }
    return DefaultActionFlashColor;
}

} // namespace majo
