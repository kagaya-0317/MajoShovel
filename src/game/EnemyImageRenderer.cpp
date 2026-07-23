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
constexpr int StardustMoleSpriteRows = 6;
constexpr int StardustMoleSpriteFramesPerAnimation = 6;
constexpr float StardustMoleSpriteFrameDurationSeconds = 24.0f / EnemyAnimationFps;
constexpr float StardustMoleChargeSpriteFrameDurationSeconds = 8.0f / EnemyAnimationFps;
constexpr float StardustMoleSpriteScale = 0.84f;
constexpr std::string_view JunkCrabEnemyId = "junk_crab";
constexpr std::string_view JunkCrabSpritePath = "assets/enemies/boss_2.png";
constexpr int JunkCrabSpriteColumns = 6;
constexpr int JunkCrabSpriteRows = 9;
constexpr int JunkCrabSpriteFramesPerAnimation = 6;
constexpr float JunkCrabSpriteFrameDurationSeconds = 12.0f / EnemyAnimationFps;
constexpr float JunkCrabSpriteScale = 0.72f;
constexpr int EnemySpriteMaxAnimationFrames = 8;
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
    bool mirrorRightFromLeft = false;
    std::array<int, EnemySpriteDirectionCount> idleRows{{0, 1, 2, 3}};
    std::array<int, EnemySpriteDirectionCount> walkRows{{0, 1, 2, 3}};
    std::array<int, EnemySpriteDirectionCount> attackRows{{0, 1, 2, 3}};
    int attackFrameDurationCount = 0;
    std::array<float, EnemySpriteMaxAnimationFrames> attackFrameDurationsSeconds{};
    std::array<float, EnemySpriteMaxAnimationFrames> attackForwardOffsetsPx{};
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
    Attack,
};

EnemySpriteDirection directionFromVector(Vec2 direction)
{
    if (std::abs(direction.x) > std::abs(direction.y)) {
        return direction.x >= 0.0f ? EnemySpriteDirection::Right : EnemySpriteDirection::Left;
    }
    return direction.y >= 0.0f ? EnemySpriteDirection::Down : EnemySpriteDirection::Up;
}

Vec2 vectorForDirection(EnemySpriteDirection direction)
{
    switch (direction) {
    case EnemySpriteDirection::Down:
        return {0.0f, 1.0f};
    case EnemySpriteDirection::Left:
        return {-1.0f, 0.0f};
    case EnemySpriteDirection::Right:
        return {1.0f, 0.0f};
    case EnemySpriteDirection::Up:
        return {0.0f, -1.0f};
    }
    return {0.0f, 1.0f};
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

const char* directionDebugName(EnemySpriteDirection direction)
{
    switch (direction) {
    case EnemySpriteDirection::Down:
        return "D";
    case EnemySpriteDirection::Left:
        return "L";
    case EnemySpriteDirection::Right:
        return "R";
    case EnemySpriteDirection::Up:
        return "U";
    }
    return "?";
}

const char* motionDebugName(EnemySpriteMotion motion)
{
    switch (motion) {
    case EnemySpriteMotion::Idle:
        return "idle";
    case EnemySpriteMotion::Walk:
        return "walk";
    case EnemySpriteMotion::Attack:
        return "attack";
    }
    return "?";
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

bool isJunkCrabId(std::string_view enemyId)
{
    return enemyId == JunkCrabEnemyId;
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
    spec.mirrorRightFromLeft = true;
    spec.idleRows = std::array<int, EnemySpriteDirectionCount>{{0, 2, 2, 4}};
    spec.walkRows = std::array<int, EnemySpriteDirectionCount>{{1, 3, 3, 5}};
    spec.attackRows = spec.walkRows;
    return spec;
}

EnemySpriteSheetSpec junkCrabSpriteSheetSpec()
{
    EnemySpriteSheetSpec spec;
    spec.path = std::string(JunkCrabSpritePath);
    spec.columns = JunkCrabSpriteColumns;
    spec.rows = JunkCrabSpriteRows;
    spec.framesPerAnimation = JunkCrabSpriteFramesPerAnimation;
    spec.frameDurationSeconds = JunkCrabSpriteFrameDurationSeconds;
    spec.scale = JunkCrabSpriteScale;
    spec.bossScale = JunkCrabSpriteScale;
    spec.mirrorRightFromLeft = true;
    spec.idleRows = std::array<int, EnemySpriteDirectionCount>{{0, 3, 3, 6}};
    spec.walkRows = std::array<int, EnemySpriteDirectionCount>{{1, 4, 4, 7}};
    spec.attackRows = std::array<int, EnemySpriteDirectionCount>{{2, 5, 5, 8}};
    spec.attackFrameDurationCount = std::min(JunkCrabAttackFrameCount, EnemySpriteMaxAnimationFrames);
    for (int i = 0; i < spec.attackFrameDurationCount; ++i) {
        spec.attackFrameDurationsSeconds[static_cast<std::size_t>(i)] =
            static_cast<float>(JunkCrabAttackFrameDurationsFrames[static_cast<std::size_t>(i)]) /
            static_cast<float>(JunkCrabAttackFrameRate);
        spec.attackForwardOffsetsPx[static_cast<std::size_t>(i)] =
            JunkCrabAttackForwardOffsetsPx[static_cast<std::size_t>(i)];
    }
    return spec;
}

EnemySpriteSheetSpec enemySpriteSheetSpec(const EnemyDefinition& enemy)
{
    if (isStardustMoleId(enemy.id)) {
        return stardustMoleSpriteSheetSpec();
    }
    if (isJunkCrabId(enemy.id)) {
        return junkCrabSpriteSheetSpec();
    }
    return defaultEnemySpriteSheetSpec(enemy.imageNumber);
}

EnemySpriteSheetSpec enemySpriteSheetSpec(const Enemy& enemy)
{
    if (isStardustMoleId(enemy.enemyId) ||
        (enemy.definition != nullptr && isStardustMoleId(enemy.definition->id))) {
        return stardustMoleSpriteSheetSpec();
    }
    if (isJunkCrabId(enemy.enemyId) ||
        (enemy.definition != nullptr && isJunkCrabId(enemy.definition->id))) {
        return junkCrabSpriteSheetSpec();
    }
    return enemy.definition != nullptr ? enemySpriteSheetSpec(*enemy.definition) : EnemySpriteSheetSpec{};
}

bool isStardustMoleEnemy(const Enemy& enemy)
{
    return isStardustMoleId(enemy.enemyId) ||
        (enemy.definition != nullptr && isStardustMoleId(enemy.definition->id));
}

bool isJunkCrabEnemy(const Enemy& enemy)
{
    return isJunkCrabId(enemy.enemyId) ||
        (enemy.definition != nullptr && isJunkCrabId(enemy.definition->id));
}

bool validEnemySpriteSheetSpec(const EnemySpriteSheetSpec& spec)
{
    return !spec.path.empty() &&
        spec.columns > 0 &&
        spec.rows > 0 &&
        spec.framesPerAnimation > 0 &&
        spec.framesPerAnimation <= spec.columns &&
        spec.framesPerAnimation <= EnemySpriteMaxAnimationFrames &&
        spec.attackFrameDurationCount >= 0 &&
        spec.attackFrameDurationCount <= std::min(spec.columns, EnemySpriteMaxAnimationFrames) &&
        spec.frameDurationSeconds > 0.0f &&
        spec.scale > 0.0f &&
        spec.bossScale > 0.0f;
}

EnemySpriteMotion motionForEnemy(const Enemy& enemy)
{
    if (isJunkCrabEnemy(enemy) && junkCrabPhaseUsesAttackAnimation(enemy.bossAction.junkCrab.phase)) {
        return EnemySpriteMotion::Attack;
    }
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

float animationTimeForEnemyMotion(const Enemy& enemy, EnemySpriteMotion motion, float animationTimeSeconds)
{
    if (motion == EnemySpriteMotion::Attack && isJunkCrabEnemy(enemy)) {
        return enemy.bossAction.junkCrab.attackAnimationSeconds;
    }
    return animationTimeSeconds;
}

EnemySpriteDirection sourceDirectionForSpec(const EnemySpriteSheetSpec& spec, EnemySpriteDirection direction)
{
    if (spec.mirrorRightFromLeft && direction == EnemySpriteDirection::Right) {
        return EnemySpriteDirection::Left;
    }
    return direction;
}

bool flipXForSpecDirection(const EnemySpriteSheetSpec& spec, EnemySpriteDirection direction)
{
    return spec.mirrorRightFromLeft && direction == EnemySpriteDirection::Right;
}

int rowForAnimation(const EnemySpriteSheetSpec& spec, EnemySpriteDirection direction, EnemySpriteMotion motion)
{
    const EnemySpriteDirection sourceDirection = sourceDirectionForSpec(spec, direction);
    const auto& rows =
        motion == EnemySpriteMotion::Attack ? spec.attackRows :
        motion == EnemySpriteMotion::Walk ? spec.walkRows :
        spec.idleRows;
    return std::clamp(rows[static_cast<std::size_t>(directionIndex(sourceDirection))], 0, spec.rows - 1);
}

struct AnimationFrameSelection {
    int column = 0;
    float frameElapsedSeconds = 0.0f;
};

AnimationFrameSelection variableAnimationFrameColumn(const EnemySpriteSheetSpec& spec, float animationTimeSeconds)
{
    const int frameCount = std::clamp(spec.attackFrameDurationCount, 1, spec.columns);
    float remaining = std::max(0.0f, animationTimeSeconds);
    for (int i = 0; i < frameCount; ++i) {
        const float duration = std::max(0.001f, spec.attackFrameDurationsSeconds[static_cast<std::size_t>(i)]);
        if (remaining < duration) {
            return {i, remaining};
        }
        remaining -= duration;
    }
    return {frameCount - 1, std::max(0.0f, spec.attackFrameDurationsSeconds[static_cast<std::size_t>(frameCount - 1)])};
}

AnimationFrameSelection animationFrameColumn(const EnemySpriteSheetSpec& spec, float animationTimeSeconds, float frameDurationSeconds)
{
    const int frameCount = std::clamp(spec.framesPerAnimation, 1, spec.columns);
    const float frameDuration = std::max(0.001f, frameDurationSeconds);
    const float safeTime = std::max(0.0f, animationTimeSeconds);
    const int step = static_cast<int>(std::floor(safeTime / frameDuration));
    return {
        std::clamp(step % frameCount, 0, spec.columns - 1),
        std::fmod(safeTime, frameDuration),
    };
}

AnimationFrameSelection animationFrameForMotion(
    const EnemySpriteSheetSpec& spec,
    const Enemy& enemy,
    EnemySpriteMotion motion,
    float animationTimeSeconds)
{
    const float motionTime = animationTimeForEnemyMotion(enemy, motion, animationTimeSeconds);
    if (motion == EnemySpriteMotion::Attack && isJunkCrabEnemy(enemy)) {
        const JunkCrabAttackAnimationKind kind = junkCrabAttackAnimationKind(enemy.bossAction.junkCrab.phase);
        if (kind == JunkCrabAttackAnimationKind::Claw && spec.attackFrameDurationCount > 0) {
            return variableAnimationFrameColumn(spec, motionTime);
        }
        if (kind == JunkCrabAttackAnimationKind::DebrisVolley) {
            return animationFrameColumn(spec, motionTime, JunkCrabDebrisVolleyAttackFrameDurationSeconds);
        }
    }
    if (motion == EnemySpriteMotion::Attack && spec.attackFrameDurationCount > 0) {
        return variableAnimationFrameColumn(spec, motionTime);
    }
    return animationFrameColumn(spec, motionTime, frameDurationForEnemy(spec, enemy, motion));
}

Vec2 attackMotionDrawOffset(
    const EnemySpriteSheetSpec& spec,
    const Enemy& enemy,
    EnemySpriteDirection direction,
    const AnimationFrameSelection& frame)
{
    if (spec.attackFrameDurationCount <= 0 || frame.column < 0 || frame.column >= spec.attackFrameDurationCount) {
        return {};
    }
    if (isJunkCrabEnemy(enemy) &&
        junkCrabAttackAnimationKind(enemy.bossAction.junkCrab.phase) != JunkCrabAttackAnimationKind::Claw) {
        return {};
    }

    Vec2 offset = vectorForDirection(direction) * spec.attackForwardOffsetsPx[static_cast<std::size_t>(frame.column)];
    if (frame.column == JunkCrabAttackChargeFrameIndex) {
        const float shakeStepSeconds =
            static_cast<float>(JunkCrabAttackChargeShakeIntervalFrames) /
            static_cast<float>(JunkCrabAttackFrameRate);
        const int shakeStep = static_cast<int>(std::floor(std::max(0.0f, frame.frameElapsedSeconds) / shakeStepSeconds));
        offset.x += (shakeStep % 2 == 0 ? -JunkCrabAttackChargeShakePixels : JunkCrabAttackChargeShakePixels);
    }
    return offset;
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
    return numberedEnemyImagePath(imageNumber);
}

std::string enemyImagePath(const EnemyDefinition& enemy)
{
    return enemySpriteSheetSpec(enemy).path;
}

EnemyImageDebugInfo enemyImageDebugInfo(
    const Enemy& enemy,
    float animationTimeSeconds,
    const EnemyImageDrawOptions& options)
{
    EnemyImageDebugInfo info;
    const EnemySpriteSheetSpec spec = enemySpriteSheetSpec(enemy);
    if (!validEnemySpriteSheetSpec(spec)) {
        return info;
    }

    const EnemySpriteMotion motion = motionForEnemy(enemy);
    const EnemySpriteDirection direction = directionForEnemy(enemy, options);
    const EnemySpriteDirection facingDirection = directionFromFacing(enemy.facingAngle);
    info.valid = true;
    info.directionSource = options.directionOverrideEnabled ? "override" : "facing";
    info.directionName = directionDebugName(direction);
    info.facingDirectionName = directionDebugName(facingDirection);
    info.motionName = motionDebugName(motion);
    info.directionOverrideEnabled = options.directionOverrideEnabled;
    info.directionIndex = directionIndex(direction);
    info.frameRow = rowForAnimation(spec, direction, motion);
    info.frameColumn = animationFrameForMotion(spec, enemy, motion, animationTimeSeconds).column;
    info.sheetRows = spec.rows;
    info.sheetColumns = spec.columns;
    info.framesPerAnimation = spec.framesPerAnimation;
    info.facingAngleDegrees = enemy.facingAngle * (180.0f / Pi);
    info.facingVector = {std::cos(enemy.facingAngle), std::sin(enemy.facingAngle)};
    info.directionOverride = options.directionOverride;
    return info;
}

Vec2 junkCrabAttackForwardMotionOffset(
    const Enemy& enemy,
    float animationTimeSeconds,
    const EnemyImageDrawOptions& options)
{
    if (!isJunkCrabEnemy(enemy) ||
        junkCrabAttackAnimationKind(enemy.bossAction.junkCrab.phase) != JunkCrabAttackAnimationKind::Claw) {
        return {};
    }

    const EnemySpriteDirection direction = directionForEnemy(enemy, options);
    return vectorForDirection(direction) * junkCrabAttackForwardOffsetPx(animationTimeSeconds);
}

Vec2 junkCrabAttackChargeShakeOffset(
    const Enemy& enemy,
    float animationTimeSeconds,
    const EnemyImageDrawOptions& options)
{
    if (!isJunkCrabEnemy(enemy) ||
        junkCrabAttackAnimationKind(enemy.bossAction.junkCrab.phase) != JunkCrabAttackAnimationKind::Claw) {
        return {};
    }

    const EnemySpriteDirection direction = directionForEnemy(enemy, options);
    (void)direction;
    return {junkCrabAttackChargeShakeOffsetPx(animationTimeSeconds), 0.0f};
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
    const AnimationFrameSelection frame = animationFrameForMotion(spec, enemy, motion, animationTimeSeconds);
    const EnemySpriteDirection direction = directionForEnemy(enemy, options);
    const int frameColumn = frame.column;
    const int frameRow = rowForAnimation(spec, direction, motion);
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
    drawOptions.flipX = drawOptions.flipX != flipXForSpecDirection(spec, direction);
    Vec2 drawCenter = center;
    if (motion == EnemySpriteMotion::Attack) {
        if (isJunkCrabEnemy(enemy)) {
            drawCenter += junkCrabAttackChargeShakeOffset(enemy, animationTimeForEnemyMotion(enemy, motion, animationTimeSeconds), options);
        } else {
            drawCenter += attackMotionDrawOffset(spec, enemy, direction, frame);
        }
    }
    return renderer.drawImageRegion(handle, sourceRect, drawCenter, drawSize, drawOptions);
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

    const EnemySpriteDirection direction = directionForIcon(options);
    const int frameColumn = animationFrameColumn(spec, animationTimeSeconds, spec.frameDurationSeconds).column;
    const int frameRow = rowForAnimation(spec, direction, EnemySpriteMotion::Idle);
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
    drawOptions.flipX = drawOptions.flipX != flipXForSpecDirection(spec, direction);
    return renderer.drawImageRegion(handle, sourceRect, center, drawSize, drawOptions);
}

}
