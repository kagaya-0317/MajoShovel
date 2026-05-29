#pragma once

#include "engine/Math.hpp"
#include "engine/RendererTypes.hpp"
#include "game/EntityStatus.hpp"

#include <string>
#include <string_view>

namespace majo {

class EffectSystem;
class Renderer;

enum class StatusPopupTarget {
    Enemy,
    Player,
};

struct StatusPopupEvent {
    Vec2 position{};
    std::string stateId;
    StatusPopupTarget target = StatusPopupTarget::Enemy;
};

struct EntityStatusVisualStyle {
    Color tint{255, 255, 255, 255};
    bool hasTint = false;
    bool flipVertical = false;
    float scaleMultiplier = 1.0f;
};

[[nodiscard]] EntityStatusVisualStyle entityStatusVisualStyle(const EntityStatus& status);
[[nodiscard]] Vec2 entityStatusJitterOffset(const EntityStatus& status, double totalSeconds);
[[nodiscard]] std::string_view entityStatusDisplayName(std::string_view stateId);
[[nodiscard]] Color entityStatusPopupColor(std::string_view stateId, StatusPopupTarget target);
[[nodiscard]] bool shouldShowEntityStatusPopup(const EntityStateApplyResult& result);

void emitEntityStatusAuras(const EntityStatus& status, Vec2 position, EffectSystem& effects);
void renderEntityStatusOverlays(
    Renderer& renderer,
    const EntityStatus& status,
    Vec2 footAnchor,
    float visualSize,
    double totalSeconds);

} // namespace majo
