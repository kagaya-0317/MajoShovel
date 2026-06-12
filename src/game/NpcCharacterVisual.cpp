#include "game/NpcCharacterVisual.hpp"

#include "engine/Renderer.hpp"

#include <algorithm>
#include <array>

namespace majo {

namespace {

constexpr std::array<NpcCharacterVisual, 8> NpcCharacterVisuals{{
    {"base_merchant", "assets/pola.png", {3, 1}, false},
    {"base_processor", "assets/ines.png", {3, 1}, false},
    {"base_monica", "assets/monica.png", {3, 1}, false},
    {"base_elder", "assets/sontyo.png", {3, 1}, true},
    {"mob_1", "assets/mob_1.png", {3, 1}, false},
    {"mob_2", "assets/mob_2.png", {3, 1}, false},
    {"mob_3", "assets/mob_3.png", {3, 1}, false},
    {"mob_4", "assets/mob_4.png", {3, 1}, false},
}};

} // namespace

const NpcCharacterVisual* findNpcCharacterVisual(std::string_view visualId)
{
    const auto it = std::find_if(
        NpcCharacterVisuals.begin(),
        NpcCharacterVisuals.end(),
        [visualId](const NpcCharacterVisual& visual) {
            return visual.id == visualId;
        });
    return it == NpcCharacterVisuals.end() ? nullptr : &*it;
}

bool npcCharacterFrameSize(Renderer& renderer, const NpcCharacterVisual& visual, Vec2& outFrameSize)
{
    return characterSpriteSheetFrameSize(renderer, visual.imagePath, visual.layout, outFrameSize);
}

float npcCharacterScaleToFit(Renderer& renderer, const NpcCharacterVisual& visual, Vec2 fitSize)
{
    Vec2 frameSize{};
    if (!npcCharacterFrameSize(renderer, visual, frameSize) || frameSize.x <= 0.0f || frameSize.y <= 0.0f) {
        return 1.0f;
    }
    return std::max(0.001f, std::min(fitSize.x / frameSize.x, fitSize.y / frameSize.y));
}

Vec2 npcCharacterDrawSize(Renderer& renderer, const NpcCharacterVisual& visual, float scale)
{
    Vec2 frameSize{};
    if (!npcCharacterFrameSize(renderer, visual, frameSize) || frameSize.x <= 0.0f || frameSize.y <= 0.0f) {
        return {};
    }
    return frameSize * std::max(0.0f, scale);
}

bool drawNpcCharacterSprite(Renderer& renderer, const NpcCharacterVisual& visual, const NpcCharacterDrawOptions& options)
{
    CharacterSpriteDrawOptions spriteOptions;
    spriteOptions.frameIndex = options.frameIndex;
    spriteOptions.anchorPosition = options.anchorPosition;
    spriteOptions.scale = options.scale;
    spriteOptions.anchor = visual.anchor;
    spriteOptions.tint = options.tint;
    spriteOptions.flipHorizontal = options.flipHorizontal;
    spriteOptions.outlineEnabled = options.outlineEnabled;
    spriteOptions.outlineColor = options.outlineColor;
    spriteOptions.outlinePx = options.outlinePx;
    spriteOptions.filter = options.filter;
    return drawCharacterSpriteFrame(renderer, visual.imagePath, visual.layout, spriteOptions);
}

}
