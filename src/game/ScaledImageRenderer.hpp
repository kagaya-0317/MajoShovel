#pragma once

#include "engine/Math.hpp"
#include "engine/Renderer.hpp"

#include <string_view>

namespace majo {

struct ScaledImageDrawOptions {
    Vec2 anchor{0.5f, 0.5f};
    Color tint{255, 255, 255, 255};
    TextureFilter filter = TextureFilter::Nearest;
    bool allowUpscale = false;
    float scaleMultiplier = 1.0f;
    bool outlineEnabled = true;
    Color outlineColor{0, 0, 0, 255};
    int outlinePx = 1;
    bool selectedOutlineEnabled = false;
    Color selectedOutlineColor{255, 230, 150, 255};
    int selectedOutlinePx = 6;
    Color maskOverlayColor{255, 255, 255, 0};
    float rotationDegrees = 0.0f;
    bool flipX = false;
    bool flipY = false;
};

[[nodiscard]] bool drawScaledImage(
    Renderer& renderer,
    std::string_view path,
    Vec2 center,
    Vec2 maxSize,
    const ScaledImageDrawOptions& options = {});

}