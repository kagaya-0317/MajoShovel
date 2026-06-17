#pragma once

#include "engine/Math.hpp"
#include "engine/RendererTypes.hpp"

#include <span>
#include <string_view>

namespace majo {

class Renderer;

enum class CharacterSpriteMotion {
    Idle,
    Walk,
    Death,
};

struct SpriteFrameAnimationClip {
    std::span<const int> frames;
    float frameDurationSeconds = 0.0f;
    bool loop = true;
};

struct CharacterSpriteAnimationState {
    float elapsedSeconds = 0.0f;
    CharacterSpriteMotion motion = CharacterSpriteMotion::Idle;
};

struct CharacterSpriteSheetLayout {
    int columns = 3;
    int rows = 1;
};

struct CharacterSpriteDrawOptions {
    int frameIndex = 0;
    Vec2 anchorPosition{};
    float scale = 1.0f;
    Vec2 anchor{0.5f, 0.95f};
    Color tint{255, 255, 255, 255};
    bool flipHorizontal = false;
    bool flipVertical = false;
    bool outlineEnabled = false;
    Color outlineColor{255, 255, 255, 245};
    int outlinePx = 1;
    TextureFilter filter = TextureFilter::Nearest;
};

inline constexpr float CharacterSpriteHorizontalFacingEpsilon = 0.05f;

[[nodiscard]] const SpriteFrameAnimationClip& characterSpriteAnimationClip(CharacterSpriteMotion motion);
[[nodiscard]] int spriteFrameAnimationIndex(const SpriteFrameAnimationClip& clip, float elapsedSeconds);
[[nodiscard]] int characterSpriteFrameIndex(float elapsedSeconds, CharacterSpriteMotion motion);
[[nodiscard]] int characterSpriteFrameIndex(float elapsedSeconds, bool walking);
[[nodiscard]] int characterSpriteIdleFrameIndex(float elapsedSeconds);
[[nodiscard]] bool characterSpriteFlipHorizontalFromFacing(
    Vec2 facing,
    bool currentFlipHorizontal,
    float epsilon = CharacterSpriteHorizontalFacingEpsilon);
void updateCharacterSpriteAnimation(
    CharacterSpriteAnimationState& state,
    float dt,
    CharacterSpriteMotion motion);
void updateCharacterSpriteAnimation(
    float& elapsedSeconds,
    bool& walkingState,
    float dt,
    bool walking);

[[nodiscard]] RectF characterSpriteDrawBounds(Vec2 anchorPosition, Vec2 drawSize, Vec2 anchor);
[[nodiscard]] bool characterSpriteSheetFrameSize(
    Renderer& renderer,
    std::string_view sheetPath,
    CharacterSpriteSheetLayout layout,
    Vec2& outFrameSize,
    TextureFilter filter = TextureFilter::Nearest);
[[nodiscard]] bool characterSpriteSheetFrameSize(
    Renderer& renderer,
    ImageHandle sheetHandle,
    CharacterSpriteSheetLayout layout,
    Vec2& outFrameSize);
[[nodiscard]] float characterSpriteSheetVisualSize(
    Renderer& renderer,
    std::string_view sheetPath,
    CharacterSpriteSheetLayout layout,
    float scale = 1.0f,
    TextureFilter filter = TextureFilter::Nearest);
bool drawCharacterSpriteFrame(
    Renderer& renderer,
    std::string_view sheetPath,
    CharacterSpriteSheetLayout layout,
    const CharacterSpriteDrawOptions& options);
bool drawCharacterSpriteFrame(
    Renderer& renderer,
    ImageHandle sheetHandle,
    Vec2 frameSize,
    CharacterSpriteSheetLayout layout,
    const CharacterSpriteDrawOptions& options);

}
