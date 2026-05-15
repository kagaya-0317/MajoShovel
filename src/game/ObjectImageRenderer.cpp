#include "game/ObjectImageRenderer.hpp"
#include "game/ScaledImageRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

namespace {
constexpr std::string_view ObjectImageDir = "assets/objects/";
constexpr std::string_view ObjectImagePrefix = "obj_";
constexpr std::string_view ObjectImageExtension = ".png";
constexpr float ObjectImageScaleMin = 0.05f;
constexpr float ObjectImageScaleMax = 8.0f;
const std::unordered_map<std::string, float>* gObjectImageScaleOverrides = nullptr;
}

ObjectImageDrawOptions withSelectedItemOutline(
    const ObjectImageDrawOptions& base,
    Color outlineColor,
    int outlinePx)
{
    ObjectImageDrawOptions options = base;
    options.selectedOutlineEnabled = true;
    options.selectedOutlineColor = outlineColor;
    options.selectedOutlineColor.a = 255;
    options.selectedOutlinePx = std::max(1, outlinePx);
    return options;
}

void setObjectImageScaleOverrides(const std::unordered_map<std::string, float>* scaleByObjectId)
{
    gObjectImageScaleOverrides = scaleByObjectId;
}

std::string objectImagePathFromNumber(int imageNumber)
{
    if (imageNumber <= 0) {
        return {};
    }

    return std::string(ObjectImageDir) +
        std::string(ObjectImagePrefix) +
        std::to_string(imageNumber) +
        std::string(ObjectImageExtension);
}

std::string objectImagePath(const ObjectDefinition& object)
{
    return objectImagePathFromNumber(object.imageNumber);
}

bool drawObjectImage(
    Renderer& renderer,
    const ObjectDefinition& object,
    Vec2 center,
    Vec2 maxSize,
    const ObjectImageDrawOptions& options)
{
    const std::string path = objectImagePath(object);
    if (path.empty()) {
        return false;
    }

    float objectScale = 1.0f;
    if (gObjectImageScaleOverrides != nullptr && !object.id.empty()) {
        const auto it = gObjectImageScaleOverrides->find(object.id);
        if (it != gObjectImageScaleOverrides->end()) {
            objectScale = it->second;
        }
    }
    objectScale = std::clamp(objectScale, ObjectImageScaleMin, ObjectImageScaleMax);
    const float optionScale = std::clamp(options.scaleMultiplier, ObjectImageScaleMin, ObjectImageScaleMax);

    ScaledImageDrawOptions scaledOptions;
    scaledOptions.anchor = options.anchor;
    scaledOptions.tint = options.tint;
    scaledOptions.filter = options.filter;
    scaledOptions.allowUpscale = options.allowUpscale;
    scaledOptions.scaleMultiplier = objectScale * optionScale;
    scaledOptions.outlineEnabled = options.outlineEnabled;
    scaledOptions.outlineColor = options.outlineColor;
    scaledOptions.outlinePx = options.outlinePx;
    scaledOptions.selectedOutlineEnabled = options.selectedOutlineEnabled;
    scaledOptions.selectedOutlineColor = options.selectedOutlineColor;
    scaledOptions.selectedOutlinePx = options.selectedOutlinePx;
    scaledOptions.rotationDegrees = options.rotationDegrees;
    scaledOptions.flipX = options.flipX;
    scaledOptions.flipY = options.flipY;
    return drawScaledImage(renderer, path, center, maxSize, scaledOptions);
}

bool drawObjectImageById(
    Renderer& renderer,
    const ObjectCatalog& catalog,
    std::string_view objectId,
    Vec2 center,
    Vec2 maxSize,
    const ObjectImageDrawOptions& options)
{
    const ObjectDefinition* object = catalog.registry.findById(objectId);
    if (object == nullptr) {
        return false;
    }
    return drawObjectImage(renderer, *object, center, maxSize, options);
}

}
