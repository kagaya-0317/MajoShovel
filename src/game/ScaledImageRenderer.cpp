#include "game/ScaledImageRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

namespace {
constexpr float ScaledImageScaleMin = 0.001f;
constexpr float ScaledImageScaleMax = 64.0f;
}

bool drawScaledImage(
    Renderer& renderer,
    std::string_view path,
    Vec2 center,
    Vec2 maxSize,
    const ScaledImageDrawOptions& options)
{
    if (path.empty() || maxSize.x <= 0.0f || maxSize.y <= 0.0f) {
        return false;
    }

    const ImageHandle handle = renderer.acquireImage(path, options.filter);
    if (!handle.valid()) {
        return false;
    }

    Vec2 sourceSize{};
    if (!renderer.getImageSize(handle, sourceSize) || sourceSize.x <= 0.0f || sourceSize.y <= 0.0f) {
        return false;
    }

    float scale = std::min(maxSize.x / sourceSize.x, maxSize.y / sourceSize.y);
    if (!options.allowUpscale) {
        scale = std::min(scale, 1.0f);
    }
    if (scale <= 0.0f) {
        return false;
    }

    const float optionScale = std::clamp(
        std::isfinite(options.scaleMultiplier) ? options.scaleMultiplier : 1.0f,
        ScaledImageScaleMin,
        ScaledImageScaleMax);
    const float finalScale = scale * optionScale;
    if (finalScale <= 0.0f) {
        return false;
    }

    const Vec2 sizeMultiplier{
        std::clamp(std::isfinite(options.sizeMultiplier.x) ? options.sizeMultiplier.x : 1.0f, ScaledImageScaleMin, ScaledImageScaleMax),
        std::clamp(std::isfinite(options.sizeMultiplier.y) ? options.sizeMultiplier.y : 1.0f, ScaledImageScaleMin, ScaledImageScaleMax),
    };
    const Vec2 drawSize{
        std::max(1.0f, static_cast<float>(std::round(sourceSize.x * finalScale * sizeMultiplier.x))),
        std::max(1.0f, static_cast<float>(std::round(sourceSize.y * finalScale * sizeMultiplier.y))),
    };

    ImageDrawOptions drawOptions;
    drawOptions.anchor = options.anchor;
    drawOptions.tint = options.tint;
    drawOptions.outlineEnabled = options.outlineEnabled;
    drawOptions.outlineColor = options.outlineColor;
    drawOptions.outlinePx = options.outlinePx;
    drawOptions.maskOverlayColor = options.maskOverlayColor;
    drawOptions.rotationDegrees = options.rotationDegrees;
    drawOptions.flipX = options.flipX;
    drawOptions.flipY = options.flipY;

    if (options.selectedOutlineEnabled && options.selectedOutlinePx > 0 && options.selectedOutlineColor.a > 0) {
        ImageDrawOptions selectedOutlineOptions = drawOptions;
        selectedOutlineOptions.tint.a = 0;
        selectedOutlineOptions.maskOverlayColor.a = 0;
        selectedOutlineOptions.outlineEnabled = true;
        selectedOutlineOptions.outlineColor = options.selectedOutlineColor;
        selectedOutlineOptions.outlinePx = options.selectedOutlinePx;
        if (!renderer.drawImage(handle, center, drawSize, selectedOutlineOptions)) {
            return false;
        }
    }

    return renderer.drawImage(handle, center, drawSize, drawOptions);
}

}
