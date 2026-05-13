#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Math.hpp"
#include "engine/Renderer.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

namespace majo {

struct ObjectImageDrawOptions {
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
    float rotationDegrees = 0.0f;
    bool flipX = false;
    bool flipY = false;
};

[[nodiscard]] constexpr Color selectedItemOutlineColor()
{
    return {255, 230, 150, 255};
}

[[nodiscard]] ObjectImageDrawOptions withSelectedItemOutline(
    const ObjectImageDrawOptions& base = {},
    Color outlineColor = selectedItemOutlineColor(),
    int outlinePx = 6);

void setObjectImageScaleOverrides(const std::unordered_map<std::string, float>* scaleByObjectId);

[[nodiscard]] std::string objectImagePathFromNumber(int imageNumber);
[[nodiscard]] std::string objectImagePath(const ObjectDefinition& object);
[[nodiscard]] bool drawObjectImage(
    Renderer& renderer,
    const ObjectDefinition& object,
    Vec2 center,
    Vec2 maxSize,
    const ObjectImageDrawOptions& options = {});
[[nodiscard]] bool drawObjectImageById(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    std::string_view objectId,
    Vec2 center,
    Vec2 maxSize,
    const ObjectImageDrawOptions& options = {});

}
