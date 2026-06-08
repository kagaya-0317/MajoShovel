#include "game/PlayerEquipmentVisual.hpp"

#include "game/ActorVisual.hpp"
#include "game/ItemImageRenderer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace majo {

namespace {

constexpr Vec2 HeldStaffBaseOffset = {22.0f, -35.0f};
constexpr float HeldStaffRotationDegrees = 51.0f;

constexpr std::array<Vec2, 9> HeldStaffFrameOffsets{{
    {0.0f, 0.0f},
    {0.0f, 0.0f},
    {0.0f, 1.0f},
    {0.0f, 2.0f},
    {1.0f, 3.0f},
    {1.0f, 4.0f},
    {0.0f, 2.0f},
    {1.0f, 3.0f},
    {1.0f, 4.0f},
}};

Vec2 heldStaffFrameOffset(int spriteFrame)
{
    if (spriteFrame < 0 || spriteFrame >= static_cast<int>(HeldStaffFrameOffsets.size())) {
        return {};
    }
    return HeldStaffFrameOffsets[static_cast<std::size_t>(spriteFrame)];
}

}

bool drawPlayerHeldStaff(
    Renderer& renderer,
    const ItemData& staffItem,
    const PlayerHeldStaffDrawContext& context)
{
    const float scale = std::isfinite(context.scale) ? std::max(0.0f, context.scale) : 1.0f;
    if (scale <= 0.0f) {
        return false;
    }

    const float side = context.flipHorizontal ? 1.0f : -1.0f;
    const Vec2 frameOffset = heldStaffFrameOffset(context.spriteFrame);
    const Vec2 drawOffset{
        (HeldStaffBaseOffset.x + frameOffset.x) * side,
        HeldStaffBaseOffset.y + frameOffset.y,
    };
    const Vec2 center = context.footAnchor + drawOffset * scale;
    const Vec2 maxSize{WorldItemImageMaxSize.x * scale, WorldItemImageMaxSize.y * scale};

    ObjectImageDrawOptions options;
    options.tint = context.staffTint;
    options.rotationDegrees = HeldStaffRotationDegrees * side;
    options.outlineEnabled = true;
    options.outlinePx = 1;

    return drawItemImage(renderer, staffItem, center, maxSize, options);
}

void drawPlayerHeldStaffHandOverlay(
    Renderer& renderer,
    const PlayerHeldStaffDrawContext& context)
{
    const float scale = std::isfinite(context.scale) ? std::max(0.0f, context.scale) : 1.0f;
    if (scale <= 0.0f) {
        return;
    }

    renderer.drawPlayerHandSpriteNaturalSize(
        context.spriteFrame,
        context.footAnchor,
        scale,
        context.flipHorizontal,
        context.handTint,
        context.spriteAnchor);
}

}
