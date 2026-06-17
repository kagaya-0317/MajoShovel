#include "game/CharacterSprite.hpp"

#include "engine/Renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace majo {

namespace {

constexpr float TargetFps = 60.0f;
constexpr float MaxAnimationElapsedSeconds = 3600.0f;
constexpr std::array<int, 3> CharacterIdleFrames{{0, 1, 2}};
constexpr std::array<int, 6> CharacterWalkFrames{{3, 4, 5, 6, 7, 8}};
constexpr std::array<int, 3> CharacterDeathFrames{{9, 10, 11}};
constexpr SpriteFrameAnimationClip CharacterIdleClip{
    std::span<const int>{CharacterIdleFrames},
    12.0f / TargetFps,
    true,
};
constexpr SpriteFrameAnimationClip CharacterWalkClip{
    std::span<const int>{CharacterWalkFrames},
    6.0f / TargetFps,
    true,
};
constexpr SpriteFrameAnimationClip CharacterDeathClip{
    std::span<const int>{CharacterDeathFrames},
    25.0f / TargetFps,
    false,
};

[[nodiscard]] CharacterSpriteMotion characterSpriteMotionFromWalking(bool walking)
{
    return walking ? CharacterSpriteMotion::Walk : CharacterSpriteMotion::Idle;
}

[[nodiscard]] bool sheetFrameSize(
    Renderer& renderer,
    ImageHandle handle,
    CharacterSpriteSheetLayout layout,
    Vec2& outFrameSize)
{
    if (!handle.valid() || layout.columns <= 0 || layout.rows <= 0) {
        return false;
    }

    Vec2 sheetSize{};
    if (!renderer.getImageSize(handle, sheetSize)) {
        return false;
    }

    const int sheetWidth = static_cast<int>(std::round(sheetSize.x));
    const int sheetHeight = static_cast<int>(std::round(sheetSize.y));
    if (sheetWidth <= 0 ||
        sheetHeight <= 0 ||
        sheetWidth % layout.columns != 0 ||
        sheetHeight % layout.rows != 0) {
        return false;
    }

    outFrameSize = {
        static_cast<float>(sheetWidth / layout.columns),
        static_cast<float>(sheetHeight / layout.rows),
    };
    return true;
}

}

const SpriteFrameAnimationClip& characterSpriteAnimationClip(CharacterSpriteMotion motion)
{
    switch (motion) {
    case CharacterSpriteMotion::Death:
        return CharacterDeathClip;
    case CharacterSpriteMotion::Walk:
        return CharacterWalkClip;
    case CharacterSpriteMotion::Idle:
    default:
        return CharacterIdleClip;
    }
}

int spriteFrameAnimationIndex(const SpriteFrameAnimationClip& clip, float elapsedSeconds)
{
    if (clip.frames.empty()) {
        return 0;
    }
    const float frameDuration = std::max(0.001f, clip.frameDurationSeconds);
    const int step = static_cast<int>(std::floor(std::max(0.0f, elapsedSeconds) / frameDuration));
    const int frameCount = static_cast<int>(clip.frames.size());
    const int frameIndex = clip.loop
        ? (step % frameCount)
        : std::clamp(step, 0, frameCount - 1);
    return clip.frames[static_cast<std::size_t>(frameIndex)];
}

int characterSpriteFrameIndex(float elapsedSeconds, CharacterSpriteMotion motion)
{
    return spriteFrameAnimationIndex(characterSpriteAnimationClip(motion), elapsedSeconds);
}

int characterSpriteFrameIndex(float elapsedSeconds, bool walking)
{
    return characterSpriteFrameIndex(elapsedSeconds, characterSpriteMotionFromWalking(walking));
}

int characterSpriteIdleFrameIndex(float elapsedSeconds)
{
    return characterSpriteFrameIndex(elapsedSeconds, CharacterSpriteMotion::Idle);
}

bool characterSpriteFlipHorizontalFromFacing(
    Vec2 facing,
    bool currentFlipHorizontal,
    float epsilon)
{
    const float safeEpsilon = std::max(0.0f, epsilon);
    if (facing.x > safeEpsilon) {
        return true;
    }
    if (facing.x < -safeEpsilon) {
        return false;
    }
    return currentFlipHorizontal;
}

void updateCharacterSpriteAnimation(
    CharacterSpriteAnimationState& state,
    float dt,
    CharacterSpriteMotion motion)
{
    if (state.motion != motion) {
        state.motion = motion;
        state.elapsedSeconds = 0.0f;
        return;
    }
    state.elapsedSeconds = std::fmod(
        state.elapsedSeconds + std::max(0.0f, dt),
        MaxAnimationElapsedSeconds);
}

void updateCharacterSpriteAnimation(
    float& elapsedSeconds,
    bool& walkingState,
    float dt,
    bool walking)
{
    CharacterSpriteAnimationState state{
        elapsedSeconds,
        characterSpriteMotionFromWalking(walkingState),
    };
    updateCharacterSpriteAnimation(state, dt, characterSpriteMotionFromWalking(walking));
    elapsedSeconds = state.elapsedSeconds;
    walkingState = state.motion == CharacterSpriteMotion::Walk;
}

RectF characterSpriteDrawBounds(Vec2 anchorPosition, Vec2 drawSize, Vec2 anchor)
{
    return {
        anchorPosition.x - drawSize.x * anchor.x,
        anchorPosition.y - drawSize.y * anchor.y,
        drawSize.x,
        drawSize.y,
    };
}

bool characterSpriteSheetFrameSize(
    Renderer& renderer,
    std::string_view sheetPath,
    CharacterSpriteSheetLayout layout,
    Vec2& outFrameSize,
    TextureFilter filter)
{
    const ImageHandle handle = renderer.acquireImage(sheetPath, filter);
    return sheetFrameSize(renderer, handle, layout, outFrameSize);
}

bool characterSpriteSheetFrameSize(
    Renderer& renderer,
    ImageHandle sheetHandle,
    CharacterSpriteSheetLayout layout,
    Vec2& outFrameSize)
{
    return sheetFrameSize(renderer, sheetHandle, layout, outFrameSize);
}

float characterSpriteSheetVisualSize(
    Renderer& renderer,
    std::string_view sheetPath,
    CharacterSpriteSheetLayout layout,
    float scale,
    TextureFilter filter)
{
    Vec2 frameSize{};
    if (!characterSpriteSheetFrameSize(renderer, sheetPath, layout, frameSize, filter)) {
        return 0.0f;
    }
    return std::max(frameSize.x, frameSize.y) * std::max(0.0f, scale);
}

bool drawCharacterSpriteFrame(
    Renderer& renderer,
    std::string_view sheetPath,
    CharacterSpriteSheetLayout layout,
    const CharacterSpriteDrawOptions& options)
{
    const ImageHandle handle = renderer.acquireImage(sheetPath, options.filter);
    Vec2 frameSize{};
    if (!sheetFrameSize(renderer, handle, layout, frameSize)) {
        return false;
    }
    return drawCharacterSpriteFrame(renderer, handle, frameSize, layout, options);
}

bool drawCharacterSpriteFrame(
    Renderer& renderer,
    ImageHandle sheetHandle,
    Vec2 frameSize,
    CharacterSpriteSheetLayout layout,
    const CharacterSpriteDrawOptions& options)
{
    if (layout.columns <= 0 || layout.rows <= 0 || options.scale <= 0.0f) {
        return false;
    }
    const int frameCount = layout.columns * layout.rows;
    if (options.frameIndex < 0 || options.frameIndex >= frameCount) {
        return false;
    }

    if (!sheetHandle.valid() || frameSize.x <= 0.0f || frameSize.y <= 0.0f) {
        return false;
    }

    const int frameColumn = options.frameIndex % layout.columns;
    const int frameRow = options.frameIndex / layout.columns;
    const RectF source{
        static_cast<float>(frameColumn) * frameSize.x,
        static_cast<float>(frameRow) * frameSize.y,
        frameSize.x,
        frameSize.y,
    };

    ImageDrawOptions imageOptions;
    imageOptions.anchor = options.anchor;
    imageOptions.tint = options.tint;
    imageOptions.outlineEnabled = options.outlineEnabled;
    imageOptions.outlineColor = options.outlineColor;
    imageOptions.outlinePx = options.outlinePx;
    imageOptions.flipX = options.flipHorizontal;
    imageOptions.flipY = options.flipVertical;

    return renderer.drawImageRegion(
        sheetHandle,
        source,
        options.anchorPosition,
        frameSize * std::max(0.0f, options.scale),
        imageOptions);
}

}
