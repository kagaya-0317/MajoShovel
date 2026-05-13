#include "game/ObjectImageRenderer.hpp"

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
    if (maxSize.x <= 0.0f || maxSize.y <= 0.0f) {
        return false;
    }

    const std::string path = objectImagePath(object);
    if (path.empty()) {
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

    float objectScale = 1.0f;
    if (gObjectImageScaleOverrides != nullptr && !object.id.empty()) {
        const auto it = gObjectImageScaleOverrides->find(object.id);
        if (it != gObjectImageScaleOverrides->end()) {
            objectScale = it->second;
        }
    }
    objectScale = std::clamp(objectScale, ObjectImageScaleMin, ObjectImageScaleMax);
    const float optionScale = std::clamp(options.scaleMultiplier, ObjectImageScaleMin, ObjectImageScaleMax);
    const float finalScale = scale * objectScale * optionScale;
    if (finalScale <= 0.0f) {
        return false;
    }

    const Vec2 drawSize{
        std::max(1.0f, static_cast<float>(std::round(sourceSize.x * finalScale))),
        std::max(1.0f, static_cast<float>(std::round(sourceSize.y * finalScale))),
    };
    ImageDrawOptions drawOptions;
    drawOptions.anchor = options.anchor;
    drawOptions.tint = options.tint;
    drawOptions.outlineEnabled = options.outlineEnabled;
    drawOptions.outlineColor = options.outlineColor;
    drawOptions.outlinePx = options.outlinePx;
    drawOptions.rotationDegrees = options.rotationDegrees;
    drawOptions.flipX = options.flipX;
    drawOptions.flipY = options.flipY;
    if (options.selectedOutlineEnabled && options.selectedOutlinePx > 0 && options.selectedOutlineColor.a > 0) {
        ImageDrawOptions selectedOutlineOptions = drawOptions;
        selectedOutlineOptions.tint.a = 0;
        selectedOutlineOptions.outlineEnabled = true;
        selectedOutlineOptions.outlineColor = options.selectedOutlineColor;
        selectedOutlineOptions.outlinePx = options.selectedOutlinePx;
        if (!renderer.drawImage(handle, center, drawSize, selectedOutlineOptions)) {
            return false;
        }
    }
    return renderer.drawImage(handle, center, drawSize, drawOptions);
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
