#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

struct InputHelpStyle {
    Color text{255, 255, 255, 255};
    Color outline{0, 0, 0, 170};
    int scale = 2;
    int outlinePx = 3;
    float iconHeight = 24.0f;
    bool outlineEnabled = false;
};

enum class InputHelpDeviceMode {
    Auto,
    KeyboardMouse,
    Gamepad,
};

enum class InputHelpGroup {
    Primary,
    Back,
    Cycle,
    Other,
};

struct InputHelpEntry {
    InputHelpGroup group = InputHelpGroup::Other;
    std::vector<InputAction> actions;
    std::string label;
    std::string bindingTag;
};

void setInputHelpContext(const Input* input);
[[nodiscard]] const Input* inputHelpContext();
void setInputHelpDeviceMode(InputHelpDeviceMode mode);
[[nodiscard]] InputHelpDeviceMode inputHelpDeviceMode();
[[nodiscard]] std::string inlineInputActionTag(InputAction action);
[[nodiscard]] std::string inlineInputActionsTag(std::initializer_list<InputAction> actions);
[[nodiscard]] std::string inlineRingRemoveAllInputTag();
[[nodiscard]] std::string buildInputHelpText(std::initializer_list<InputHelpEntry> entries);
[[nodiscard]] std::string buildInputHelpText(const std::vector<InputHelpEntry>& entries);
[[nodiscard]] bool inputHelpExplicitTagAt(
    std::string_view text,
    std::size_t offset,
    std::size_t& outEnd,
    const Input* input = nullptr);

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
