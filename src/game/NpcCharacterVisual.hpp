#pragma once

#include "engine/Math.hpp"
#include "engine/RendererTypes.hpp"
#include "game/ArtworkOutline.hpp"
#include "game/CharacterSprite.hpp"

#include <string_view>

namespace majo {

class Renderer;

struct NpcCharacterVisual {
    std::string_view id;
    std::string_view imagePath;
    CharacterSpriteSheetLayout layout{3, 1};
    bool defaultFlipHorizontal = false;
    Vec2 anchor{0.5f, 0.95f};
};

struct NpcCharacterDrawOptions {
    int frameIndex = 0;
    Vec2 anchorPosition{};
    float scale = 1.0f;
    Color tint{255, 255, 255, 255};
    bool flipHorizontal = false;
    bool outlineEnabled = ArtworkOutlineEnabled;
    Color outlineColor{ArtworkOutlineColor};
    int outlinePx = ArtworkOutlinePx;
    TextureFilter filter = TextureFilter::Nearest;
};

[[nodiscard]] const NpcCharacterVisual* findNpcCharacterVisual(std::string_view visualId);
[[nodiscard]] bool npcCharacterFrameSize(Renderer& renderer, const NpcCharacterVisual& visual, Vec2& outFrameSize);
[[nodiscard]] float npcCharacterScaleToFit(Renderer& renderer, const NpcCharacterVisual& visual, Vec2 fitSize);
[[nodiscard]] Vec2 npcCharacterDrawSize(Renderer& renderer, const NpcCharacterVisual& visual, float scale);
bool drawNpcCharacterSprite(Renderer& renderer, const NpcCharacterVisual& visual, const NpcCharacterDrawOptions& options);
bool drawNpcCharacterSprite(
    Renderer& renderer,
    const NpcCharacterVisual& visual,
    ImageHandle sheetHandle,
    Vec2 frameSize,
    const NpcCharacterDrawOptions& options);

}
