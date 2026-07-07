#pragma once

#include "engine/RendererTypes.hpp"

namespace majo {

inline constexpr bool ArtworkOutlineEnabled = true;
inline constexpr Color ArtworkOutlineColor{0, 0, 0, 255};
inline constexpr int ArtworkOutlinePx = 2;

[[nodiscard]] inline ImageDrawOptions artworkImageDrawOptions(ImageDrawOptions options = {})
{
    options.outlineEnabled = ArtworkOutlineEnabled;
    options.outlineColor = ArtworkOutlineColor;
    options.outlinePx = ArtworkOutlinePx;
    return options;
}

}
