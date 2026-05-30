#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"

#include <string>
#include <string_view>

namespace majo {

struct InputHelpStyle {
    Color text{255, 255, 255, 255};
    Color outline{0, 0, 0, 170};
    int scale = 2;
    int outlinePx = 3;
    float iconHeight = 24.0f;
    bool outlineEnabled = false;
};

void setInputHelpContext(const Input* input);
[[nodiscard]] const Input* inputHelpContext();

[[nodiscard]] Vec2 measureInputHelpText(
    Renderer& renderer,
    std::string_view text,
    const InputHelpStyle& style = {},
    const Input* input = nullptr);

[[nodiscard]] std::string fittedInputHelpText(
    Renderer& renderer,
    std::string text,
    float maxWidth,
    const InputHelpStyle& style = {},
    const Input* input = nullptr);

void drawInputHelpText(
    Renderer& renderer,
    Vec2 pos,
    std::string_view text,
    const InputHelpStyle& style = {},
    const Input* input = nullptr);

} // namespace majo
