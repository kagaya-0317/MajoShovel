#pragma once

#include "game/ElementVisual.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

inline constexpr float ActionFlashSeconds = 0.10f;

struct ActionFlashState {
    float remainingSeconds = 0.0f;
    Color color = DefaultActionFlashColor;
};

inline void triggerActionFlash(ActionFlashState& state, Color color = DefaultActionFlashColor)
{
    color.a = 255;
    state.remainingSeconds = ActionFlashSeconds;
    state.color = color;
}

inline void clearActionFlash(ActionFlashState& state)
{
    state = {};
}

inline void updateActionFlash(ActionFlashState& state, float dt)
{
    state.remainingSeconds = std::max(0.0f, state.remainingSeconds - std::max(0.0f, dt));
}

[[nodiscard]] inline float actionFlashStrength(const ActionFlashState& state)
{
    const float t = std::clamp(state.remainingSeconds / ActionFlashSeconds, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

[[nodiscard]] inline Color actionFlashOverlayColor(const ActionFlashState& state, float maximumAlpha)
{
    Color color = state.color;
    color.a = static_cast<unsigned char>(std::clamp(
        std::lround(std::max(0.0f, maximumAlpha) * actionFlashStrength(state)),
        0L,
        255L));
    return color;
}

} // namespace majo
