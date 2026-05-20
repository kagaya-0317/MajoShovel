#include "game/EnemyImageRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

namespace {
constexpr std::string_view EnemyImageDir = "assets/enemies/";
constexpr std::string_view EnemyImagePrefix = "en_";
constexpr std::string_view EnemyImageExtension = ".png";
constexpr int EnemySpriteColumns = 2;
constexpr int EnemySpriteRows = 4;
constexpr int EnemyAnimationFrameInterval = 10;
constexpr float EnemyAnimationFps = 60.0f;
constexpr float EnemyImageScaleMin = 0.05f;
constexpr float EnemyImageScaleMax = 8.0f;

enum class EnemySpriteDirection {
    Down,
    Left,
    Right,
    Up,
};

EnemySpriteDirection directionFromVector(Vec2 direction)
{
    if (std::abs(direction.x) > std::abs(direction.y)) {
        return direction.x >= 0.0f ? EnemySpriteDirection::Right : EnemySpriteDirection::Left;
    }
    return direction.y >= 0.0f ? EnemySpriteDirection::Down : EnemySpriteDirection::Up;
}

EnemySpriteDirection directionFromFacing(float angle)
{
    const Vec2 facing{std::cos(angle), std::sin(angle)};
    return directionFromVector(facing);
}

EnemySpriteDirection directionForEnemy(const Enemy& enemy, const EnemyImageDrawOptions& options)
{
    if (options.directionOverrideEnabled) {
        return directionFromVector(options.directionOverride);
    }
    return directionFromFacing(enemy.facingAngle);
}

EnemySpriteDirection directionForIcon(const EnemyImageDrawOptions& options)
{
    if (options.directionOverrideEnabled) {
        return directionFromVector(options.directionOverride);
    }
    return EnemySpriteDirection::Down;
}

int rowForDirection(EnemySpriteDirection direction)
{
    switch (direction) {
    case EnemySpriteDirection::Down:
        return 0;
    case EnemySpriteDirection::Left:
        return 1;
    case EnemySpriteDirection::Right:
        return 2;
    case EnemySpriteDirection::Up:
        return 3;
    }
    return 0;
}

bool resolveEnemyImageFrame(
    Renderer& renderer,
    const Enemy& enemy,
    const EnemyImageDrawOptions& options,
    ImageHandle& outHandle,
    Vec2& outDrawSize,
    int& outFrameWidth,
    int& outFrameHeight)
{
    if (enemy.definition == nullptr) {
        return false;
    }

    const std::string path = enemyImagePath(*enemy.definition);
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

    const int textureWidth = static_cast<int>(std::round(sourceSize.x));
    const int textureHeight = static_cast<int>(std::round(sourceSize.y));
    if (textureWidth <= 0 || textureHeight <= 0 ||
        textureWidth % EnemySpriteColumns != 0 ||
        textureHeight % EnemySpriteRows != 0) {
        return false;
    }

    const int frameWidth = textureWidth / EnemySpriteColumns;
    const int frameHeight = textureHeight / EnemySpriteRows;
    if (frameWidth <= 0 || frameHeight <= 0) {
        return false;
    }

    const float bossScale = enemy.isBoss ? 1.35f : 1.0f;
    const float optionScale = std::clamp(options.scaleMultiplier, EnemyImageScaleMin, EnemyImageScaleMax);
    outHandle = handle;
    outDrawSize = {
        std::max(1.0f, static_cast<float>(frameWidth) * bossScale * optionScale),
        std::max(1.0f, static_cast<float>(frameHeight) * bossScale * optionScale),
    };
    outFrameWidth = frameWidth;
    outFrameHeight = frameHeight;
    return true;
}

bool resolveEnemyImageIconFrame(
    Renderer& renderer,
    int imageNumber,
    const EnemyImageDrawOptions& options,
    ImageHandle& outHandle,
    int& outFrameWidth,
    int& outFrameHeight)
{
    const std::string path = enemyImagePathFromNumber(imageNumber);
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

    const int textureWidth = static_cast<int>(std::round(sourceSize.x));
    const int textureHeight = static_cast<int>(std::round(sourceSize.y));
    if (textureWidth <= 0 || textureHeight <= 0 ||
        textureWidth % EnemySpriteColumns != 0 ||
        textureHeight % EnemySpriteRows != 0) {
        return false;
    }

    const int frameWidth = textureWidth / EnemySpriteColumns;
    const int frameHeight = textureHeight / EnemySpriteRows;
    if (frameWidth <= 0 || frameHeight <= 0) {
        return false;
    }

    outHandle = handle;
    outFrameWidth = frameWidth;
    outFrameHeight = frameHeight;
    return true;
}

ImageDrawOptions enemyImageDrawOptions(const EnemyImageDrawOptions& options)
{
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
    return drawOptions;
}
}

std::string enemyImagePathFromNumber(int imageNumber)
{
    if (imageNumber <= 0) {
        return {};
    }

    return std::string(EnemyImageDir) +
        std::string(EnemyImagePrefix) +
        std::to_string(imageNumber) +
        std::string(EnemyImageExtension);
}

std::string enemyImagePath(const EnemyDefinition& enemy)
{
    return enemyImagePathFromNumber(enemy.imageNumber);
}

bool enemyImageDrawSize(Renderer& renderer, const Enemy& enemy, const EnemyImageDrawOptions& options, Vec2& outDrawSize)
{
    ImageHandle handle{};
    int frameWidth = 0;
    int frameHeight = 0;
    return resolveEnemyImageFrame(renderer, enemy, options, handle, outDrawSize, frameWidth, frameHeight);
}

bool drawEnemyImage(
    Renderer& renderer,
    const Enemy& enemy,
    Vec2 center,
    float animationTimeSeconds,
    const EnemyImageDrawOptions& options,
    Vec2* outDrawSize)
{
    ImageHandle handle{};
    Vec2 drawSize{};
    int frameWidth = 0;
    int frameHeight = 0;
    if (!resolveEnemyImageFrame(renderer, enemy, options, handle, drawSize, frameWidth, frameHeight)) {
        return false;
    }

    const int animationTick = static_cast<int>(std::floor(std::max(0.0f, animationTimeSeconds) * EnemyAnimationFps));
    const int frameColumn = (animationTick / EnemyAnimationFrameInterval) % EnemySpriteColumns;
    const int frameRow = rowForDirection(directionForEnemy(enemy, options));
    const RectF sourceRect{
        static_cast<float>(frameColumn * frameWidth),
        static_cast<float>(frameRow * frameHeight),
        static_cast<float>(frameWidth),
        static_cast<float>(frameHeight),
    };

    if (outDrawSize != nullptr) {
        *outDrawSize = drawSize;
    }

    ImageDrawOptions drawOptions = enemyImageDrawOptions(options);
    if (options.selectedOutlineEnabled && options.selectedOutlinePx > 0 && options.selectedOutlineColor.a > 0) {
        ImageDrawOptions selectedOutlineOptions = drawOptions;
        selectedOutlineOptions.tint.a = 0;
        selectedOutlineOptions.maskOverlayColor.a = 0;
        selectedOutlineOptions.outlineEnabled = true;
        selectedOutlineOptions.outlineColor = options.selectedOutlineColor;
        selectedOutlineOptions.outlinePx = options.selectedOutlinePx;
        if (!renderer.drawImageRegion(handle, sourceRect, center, drawSize, selectedOutlineOptions)) {
            return false;
        }
    }
    return renderer.drawImageRegion(handle, sourceRect, center, drawSize, drawOptions);
}

bool drawEnemyImageIcon(
    Renderer& renderer,
    int imageNumber,
    Vec2 center,
    Vec2 maxSize,
    float animationTimeSeconds,
    const EnemyImageDrawOptions& options,
    Vec2* outDrawSize)
{
    if (maxSize.x <= 0.0f || maxSize.y <= 0.0f) {
        return false;
    }

    ImageHandle handle{};
    int frameWidth = 0;
    int frameHeight = 0;
    if (!resolveEnemyImageIconFrame(renderer, imageNumber, options, handle, frameWidth, frameHeight)) {
        return false;
    }

    float scale = options.fitToMaxSize
        ? std::min(maxSize.x / static_cast<float>(frameWidth), maxSize.y / static_cast<float>(frameHeight))
        : 1.0f;
    if (!options.allowUpscale) {
        scale = std::min(scale, 1.0f);
    }
    if (scale <= 0.0f) {
        return false;
    }

    const float optionScale = std::clamp(
        std::isfinite(options.scaleMultiplier) ? options.scaleMultiplier : 1.0f,
        EnemyImageScaleMin,
        EnemyImageScaleMax);
    const Vec2 drawSize{
        std::max(1.0f, static_cast<float>(std::round(static_cast<float>(frameWidth) * scale * optionScale))),
        std::max(1.0f, static_cast<float>(std::round(static_cast<float>(frameHeight) * scale * optionScale))),
    };

    const int animationTick = static_cast<int>(std::floor(std::max(0.0f, animationTimeSeconds) * EnemyAnimationFps));
    const int frameColumn = (animationTick / EnemyAnimationFrameInterval) % EnemySpriteColumns;
    const int frameRow = rowForDirection(directionForIcon(options));
    const RectF sourceRect{
        static_cast<float>(frameColumn * frameWidth),
        static_cast<float>(frameRow * frameHeight),
        static_cast<float>(frameWidth),
        static_cast<float>(frameHeight),
    };

    if (outDrawSize != nullptr) {
        *outDrawSize = drawSize;
    }

    const ImageDrawOptions drawOptions = enemyImageDrawOptions(options);
    if (options.selectedOutlineEnabled && options.selectedOutlinePx > 0 && options.selectedOutlineColor.a > 0) {
        ImageDrawOptions selectedOutlineOptions = drawOptions;
        selectedOutlineOptions.tint.a = 0;
        selectedOutlineOptions.maskOverlayColor.a = 0;
        selectedOutlineOptions.outlineEnabled = true;
        selectedOutlineOptions.outlineColor = options.selectedOutlineColor;
        selectedOutlineOptions.outlinePx = options.selectedOutlinePx;
        if (!renderer.drawImageRegion(handle, sourceRect, center, drawSize, selectedOutlineOptions)) {
            return false;
        }
    }

    return renderer.drawImageRegion(handle, sourceRect, center, drawSize, drawOptions);
}

}
