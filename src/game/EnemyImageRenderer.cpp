#include "game/EnemyImageRenderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace majo {

namespace {
constexpr std::string_view EnemyImageDir = "assets/enemies/";
constexpr std::string_view EnemyImagePrefix = "en_";
constexpr std::string_view EnemyImageExtension = ".png";
constexpr float EnemyAnimationFps = 60.0f;
constexpr int EnemySpriteDirectionCount = 4;
constexpr int DefaultEnemySpriteColumns = 2;
constexpr int DefaultEnemySpriteRows = 4;
constexpr int DefaultEnemySpriteFramesPerAnimation = 2;
constexpr float DefaultEnemySpriteFrameDurationSeconds = 10.0f / EnemyAnimationFps;
constexpr float DefaultEnemySpriteScale = 1.0f;
constexpr float DefaultEnemyBossSpriteScale = 1.35f;
constexpr std::string_view StardustMoleEnemyId = "stardust_mole";
constexpr std::string_view StardustMoleSpritePath = "assets/enemies/boss_1.png";
constexpr int StardustMoleSpriteColumns = 6;
constexpr int StardustMoleSpriteRows = 8;
constexpr int StardustMoleSpriteFramesPerAnimation = 6;
constexpr float StardustMoleSpriteFrameDurationSeconds = 24.0f / EnemyAnimationFps;
constexpr float StardustMoleChargeSpriteFrameDurationSeconds = 8.0f / EnemyAnimationFps;
constexpr float StardustMoleSpriteScale = 0.84f;
constexpr float EnemyImageScaleMin = 0.05f;
constexpr float EnemyImageScaleMax = 8.0f;
constexpr float EnemyImageStretchMin = 0.05f;
constexpr float EnemyImageStretchMax = 8.0f;

struct EnemySpriteSheetSpec {
    std::string path;
    int columns = DefaultEnemySpriteColumns;
    int rows = DefaultEnemySpriteRows;
    int framesPerAnimation = DefaultEnemySpriteFramesPerAnimation;
    float frameDurationSeconds = DefaultEnemySpriteFrameDurationSeconds;
    float scale = DefaultEnemySpriteScale;
    float bossScale = DefaultEnemyBossSpriteScale;
    std::array<int, EnemySpriteDirectionCount> idleRows{{0, 1, 2, 3}};
    std::array<int, EnemySpriteDirectionCount> walkRows{{0, 1, 2, 3}};
};

enum class EnemySpriteDirection {
    Down,
    Left,
    Right,
    Up,
};

enum class EnemySpriteMotion {
    Idle,
    Walk,
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

int directionIndex(EnemySpriteDirection direction)
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

std::string numberedEnemyImagePath(int imageNumber)
{
    if (imageNumber <= 0) {
        return {};
    }

    return std::string(EnemyImageDir) +
        std::string(EnemyImagePrefix) +
        std::to_string(imageNumber) +
        std::string(EnemyImageExtension);
}

bool isStardustMoleId(std::string_view enemyId)
{
    return enemyId == StardustMoleEnemyId;
}

EnemySpriteSheetSpec defaultEnemySpriteSheetSpec(int imageNumber)
{
    EnemySpriteSheetSpec spec;
    spec.path = numberedEnemyImagePath(imageNumber);
    return spec;
}

EnemySpriteSheetSpec stardustMoleSpriteSheetSpec()
{
    EnemySpriteSheetSpec spec;
    spec.path = std::string(StardustMoleSpritePath);
    spec.columns = StardustMoleSpriteColumns;
    spec.rows = StardustMoleSpriteRows;
    spec.framesPerAnimation = StardustMoleSpriteFramesPerAnimation;
    spec.frameDurationSeconds = StardustMoleSpriteFrameDurationSeconds;
    spec.scale = StardustMoleSpriteScale;
    spec.bossScale = StardustMoleSpriteScale;
    spec.idleRows = std::array<int, EnemySpriteDirectionCount>{{0, 2, 4, 6}};
    spec.walkRows = std::array<int, EnemySpriteDirectionCount>{{1, 3, 5, 7}};
    return spec;
}

EnemySpriteSheetSpec enemySpriteSheetSpec(const EnemyDefinition& enemy)
{
    if (isStardustMoleId(enemy.id)) {
        return stardustMoleSpriteSheetSpec();
    }
    return defaultEnemySpriteSheetSpec(enemy.imageNumber);
}

EnemySpriteSheetSpec enemySpriteSheetSpec(const Enemy& enemy)
{
    if (isStardustMoleId(enemy.enemyId) ||
        (enemy.definition != nullptr && isStardustMoleId(enemy.definition->id))) {
        return stardustMoleSpriteSheetSpec();
    }
    return enemy.definition != nullptr ? enemySpriteSheetSpec(*enemy.definition) : EnemySpriteSheetSpec{};
}

bool isStardustMoleEnemy(const Enemy& enemy)
{
    return isStardustMoleId(enemy.enemyId) ||
        (enemy.definition != nullptr && isStardustMoleId(enemy.definition->id));
}

bool validEnemySpriteSheetSpec(const EnemySpriteSheetSpec& spec)
{
    return !spec.path.empty() &&
        spec.columns > 0 &&
        spec.rows > 0 &&
        spec.framesPerAnimation > 0 &&
        spec.framesPerAnimation <= spec.columns &&
        spec.frameDurationSeconds > 0.0f &&
        spec.scale > 0.0f &&
        spec.bossScale > 0.0f;
}

EnemySpriteMotion motionForEnemy(const Enemy& enemy)
{
    return lengthSquared(enemy.velocity) > 1.0f || enemy.jumpActive
        ? EnemySpriteMotion::Walk
        : EnemySpriteMotion::Idle;
}

float frameDurationForEnemy(const EnemySpriteSheetSpec& spec, const Enemy& enemy, EnemySpriteMotion motion)
{
    if (motion == EnemySpriteMotion::Walk &&
        isStardustMoleEnemy(enemy) &&
        enemy.bossAction.phase == BossActionPhase::Charge) {
        return StardustMoleChargeSpriteFrameDurationSeconds;
    }
    return spec.frameDurationSeconds;
}

int rowForAnimation(const EnemySpriteSheetSpec& spec, EnemySpriteDirection direction, EnemySpriteMotion motion)
{
    const auto& rows = motion == EnemySpriteMotion::Walk ? spec.walkRows : spec.idleRows;
    return std::clamp(rows[static_cast<std::size_t>(directionIndex(direction))], 0, spec.rows - 1);
}

int animationFrameColumn(const EnemySpriteSheetSpec& spec, float animationTimeSeconds, float frameDurationSeconds)
{
    const int frameCount = std::clamp(spec.framesPerAnimation, 1, spec.columns);
    const float frameDuration = std::max(0.001f, frameDurationSeconds);
    const int step = static_cast<int>(std::floor(std::max(0.0f, animationTimeSeconds) / frameDuration));
    return std::clamp(step % frameCount, 0, spec.columns - 1);
}

Vec2 clampedStretchScale(Vec2 scale)
{
    return {
        std::clamp(std::isfinite(scale.x) ? scale.x : 1.0f, EnemyImageStretchMin, EnemyImageStretchMax),
        std::clamp(std::isfinite(scale.y) ? scale.y : 1.0f, EnemyImageStretchMin, EnemyImageStretchMax),
    };
}

bool resolveEnemyImageFrame(
    Renderer& renderer,
    const Enemy& enemy,
    const EnemyImageDrawOptions& options,
    ImageHandle& outHandle,
    Vec2& outDrawSize,
    int& outFrameWidth,
    int& outFrameHeight,
    EnemySpriteSheetSpec& outSpec)
{
    const EnemySpriteSheetSpec spec = enemySpriteSheetSpec(enemy);
    if (!validEnemySpriteSheetSpec(spec)) {
        return false;
    }

    const ImageHandle handle = renderer.acquireImage(spec.path, options.filter);
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
        textureWidth % spec.columns != 0 ||
        textureHeight % spec.rows != 0) {
        return false;
    }

    const int frameWidth = textureWidth / spec.columns;
    const int frameHeight = textureHeight / spec.rows;
    if (frameWidth <= 0 || frameHeight <= 0) {
        return false;
    }

    const float baseScale = enemy.isBoss ? spec.bossScale : spec.scale;
    const float optionScale = std::clamp(options.scaleMultiplier, EnemyImageScaleMin, EnemyImageScaleMax);
    const Vec2 stretchScale = clampedStretchScale(options.stretchScale);
    outHandle = handle;
    outDrawSize = {
        std::max(1.0f, static_cast<float>(frameWidth) * baseScale * optionScale * stretchScale.x),
        std::max(1.0f, static_cast<float>(frameHeight) * baseScale * optionScale * stretchScale.y),
    };
    outFrameWidth = frameWidth;
    outFrameHeight = frameHeight;
    outSpec = spec;
    return true;
}

bool resolveEnemyImageIconFrame(
    Renderer& renderer,
    int imageNumber,
    const EnemyImageDrawOptions& options,
    ImageHandle& outHandle,
    int& outFrameWidth,
    int& outFrameHeight,
    EnemySpriteSheetSpec& outSpec)
{
    const EnemySpriteSheetSpec spec = defaultEnemySpriteSheetSpec(imageNumber);
    if (!validEnemySpriteSheetSpec(spec)) {
        return false;
    }

    const ImageHandle handle = renderer.acquireImage(spec.path, options.filter);
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
        textureWidth % spec.columns != 0 ||
        textureHeight % spec.rows != 0) {
        return false;
    }

    const int frameWidth = textureWidth / spec.columns;
    const int frameHeight = textureHeight / spec.rows;
    if (frameWidth <= 0 || frameHeight <= 0) {
        return false;
    }

    outHandle = handle;
    outFrameWidth = frameWidth;
    outFrameHeight = frameHeight;
    outSpec = spec;
    return true;
}

ImageDrawOptions enemyImageDrawOptions(const EnemyImageDrawOptions& options)
{
    ImageDrawOptions drawOptions;
    drawOptions.anchor = options.anchor;
    drawOptions.tint = options.tint;
    drawOptions.maskOverlayColor = options.maskOverlayColor;
    drawOptions.rotationDegrees = options.rotationDegrees;
    drawOptions.flipX = options.flipX;
    drawOptions.flipY = options.flipY;
    return drawOptions;
}
}

std::string enemyImagePathFromNumber(int imageNumber)
{
    return numberedEnemyImagePath(imageNumber);
}

std::string enemyImagePath(const EnemyDefinition& enemy)
{
    return enemySpriteSheetSpec(enemy).path;
}

bool enemyImageDrawSize(Renderer& renderer, const Enemy& enemy, const EnemyImageDrawOptions& options, Vec2& outDrawSize)
{
    ImageHandle handle{};
    int frameWidth = 0;
    int frameHeight = 0;
    EnemySpriteSheetSpec spec;
    return resolveEnemyImageFrame(renderer, enemy, options, handle, outDrawSize, frameWidth, frameHeight, spec);
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
    EnemySpriteSheetSpec spec;
    if (!resolveEnemyImageFrame(renderer, enemy, options, handle, drawSize, frameWidth, frameHeight, spec)) {
        return false;
    }

    const EnemySpriteMotion motion = motionForEnemy(enemy);
    const int frameColumn = animationFrameColumn(spec, animationTimeSeconds, frameDurationForEnemy(spec, enemy, motion));
    const int frameRow = rowForAnimation(spec, directionForEnemy(enemy, options), motion);
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
    EnemySpriteSheetSpec spec;
    if (!resolveEnemyImageIconFrame(renderer, imageNumber, options, handle, frameWidth, frameHeight, spec)) {
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
    const Vec2 stretchScale = clampedStretchScale(options.stretchScale);
    const Vec2 drawSize{
        std::max(1.0f, static_cast<float>(std::round(static_cast<float>(frameWidth) * scale * optionScale * stretchScale.x))),
        std::max(1.0f, static_cast<float>(std::round(static_cast<float>(frameHeight) * scale * optionScale * stretchScale.y))),
    };

    const int frameColumn = animationFrameColumn(spec, animationTimeSeconds, spec.frameDurationSeconds);
    const int frameRow = rowForAnimation(spec, directionForIcon(options), EnemySpriteMotion::Idle);
    const RectF sourceRect{
        static_cast<float>(frameColumn * frameWidth),
        static_cast<float>(frameRow * frameHeight),
        static_cast<float>(frameWidth),
        static_cast<float>(frameHeight),
    };

    if (outDrawSize != nullptr) {
        *outDrawSize = drawSize;
    }

    return renderer.drawImageRegion(handle, sourceRect, center, drawSize, enemyImageDrawOptions(options));
}

}
