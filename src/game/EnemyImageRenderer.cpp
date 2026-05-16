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

EnemySpriteDirection directionFromFacing(float angle)
{
    const Vec2 facing{std::cos(angle), std::sin(angle)};
    if (std::abs(facing.x) > std::abs(facing.y)) {
        return facing.x >= 0.0f ? EnemySpriteDirection::Right : EnemySpriteDirection::Left;
    }
    return facing.y >= 0.0f ? EnemySpriteDirection::Down : EnemySpriteDirection::Up;
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
    const int frameRow = rowForDirection(directionFromFacing(enemy.facingAngle));
    const RectF sourceRect{
        static_cast<float>(frameColumn * frameWidth),
        static_cast<float>(frameRow * frameHeight),
        static_cast<float>(frameWidth),
        static_cast<float>(frameHeight),
    };

    if (outDrawSize != nullptr) {
        *outDrawSize = drawSize;
    }

    ImageDrawOptions drawOptions;
    drawOptions.tint = options.tint;
    drawOptions.outlineEnabled = options.outlineEnabled;
    drawOptions.outlineColor = options.outlineColor;
    drawOptions.outlinePx = options.outlinePx;
    drawOptions.maskOverlayColor = options.maskOverlayColor;
    return renderer.drawImageRegion(handle, sourceRect, center, drawSize, drawOptions);
}

}
