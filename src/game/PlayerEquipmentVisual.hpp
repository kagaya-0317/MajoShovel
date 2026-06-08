#pragma once

#include "data/ObjectCatalog.hpp"
#include "engine/Math.hpp"
#include "engine/RendererTypes.hpp"

namespace majo {

class Renderer;

struct PlayerHeldStaffDrawContext {
    Vec2 footAnchor{};
    int spriteFrame = 0;
    bool flipHorizontal = false;
    float scale = 1.0f;
    Color staffTint{255, 255, 255, 255};
    Color handTint{255, 255, 255, 255};
    Vec2 spriteAnchor{0.5f, 0.95f};
};

[[nodiscard]] bool drawPlayerHeldStaff(
    Renderer& renderer,
    const ItemData& staffItem,
    const PlayerHeldStaffDrawContext& context);
void drawPlayerHeldStaffHandOverlay(
    Renderer& renderer,
    const PlayerHeldStaffDrawContext& context);

}
