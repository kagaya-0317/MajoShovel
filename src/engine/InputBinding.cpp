#include "engine/InputBinding.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <exception>
#include <optional>
#include <string>

namespace majo {

namespace {

constexpr float StickDigitalThreshold = 0.55f;
constexpr float TriggerDigitalThreshold = 0.45f;

struct ActionNameEntry {
    InputAction action;
    std::string_view name;
};

constexpr ActionNameEntry ActionNames[] = {
    {InputAction::MoveLeft, "MoveLeft"},
    {InputAction::MoveRight, "MoveRight"},
    {InputAction::MoveUp, "MoveUp"},
    {InputAction::MoveDown, "MoveDown"},
    {InputAction::ThrowActiveRing, "ThrowActiveRing"},
    {InputAction::OffsetRingCenter, "OffsetRingCenter"},
    {InputAction::ShiftRingLeft, "ShiftRingLeft"},
    {InputAction::ShiftRingRight, "ShiftRingRight"},
    {InputAction::ShiftRingUp, "ShiftRingUp"},
    {InputAction::ShiftRingDown, "ShiftRingDown"},
    {InputAction::RingCommandModifier, "RingCommandModifier"},
    {InputAction::ShortcutCursorLeft, "ShortcutCursorLeft"},
    {InputAction::ShortcutCursorRight, "ShortcutCursorRight"},
    {InputAction::DirectShortcut1, "DirectShortcut1"},
    {InputAction::DirectShortcut2, "DirectShortcut2"},
    {InputAction::DirectShortcut3, "DirectShortcut3"},
    {InputAction::DirectShortcut4, "DirectShortcut4"},
    {InputAction::DirectShortcut5, "DirectShortcut5"},
    {InputAction::DirectShortcut6, "DirectShortcut6"},
    {InputAction::DirectShortcut7, "DirectShortcut7"},
    {InputAction::DirectShortcut8, "DirectShortcut8"},
    {InputAction::PreviousShortcutRow, "PreviousShortcutRow"},
    {InputAction::NextShortcutRow, "NextShortcutRow"},
    {InputAction::ToggleShortcutRow, "ToggleShortcutRow"},
    {InputAction::UseSelectedItem, "UseSelectedItem"},
    {InputAction::Confirm, "Confirm"},
    {InputAction::PutSelectedItemOnRing, "PutSelectedItemOnRing"},
    {InputAction::GrabOrPlaceItem, "GrabOrPlaceItem"},
    {InputAction::ArrangeItems, "ArrangeItems"},
    {InputAction::PreviousActiveRing, "PreviousActiveRing"},
    {InputAction::NextActiveRing, "NextActiveRing"},
    {InputAction::CaptureNet, "CaptureNet"},
    {InputAction::ToggleProtection, "ToggleProtection"},
    {InputAction::Cancel, "Cancel"},
    {InputAction::Pause, "Pause"},
    {InputAction::OpenInventory, "OpenInventory"},
    {InputAction::OpenOptions, "OpenOptions"},
    {InputAction::OpenCredits, "OpenCredits"},
    {InputAction::ToggleDebug, "ToggleDebug"},
    {InputAction::ToggleDebugPause, "ToggleDebugPause"},
    {InputAction::TestRestart, "TestRestart"},
    {InputAction::ToggleTestFreeze, "ToggleTestFreeze"},
    {InputAction::OpenConsole, "OpenConsole"},
    {InputAction::ToggleAutoReloadBlock, "ToggleAutoReloadBlock"},
};

struct GamepadNameEntry {
    int code;
    std::string_view name;
};

constexpr GamepadNameEntry GamepadButtonNames[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, "south"},
    {SDL_GAMEPAD_BUTTON_EAST, "east"},
    {SDL_GAMEPAD_BUTTON_WEST, "west"},
    {SDL_GAMEPAD_BUTTON_NORTH, "north"},
    {SDL_GAMEPAD_BUTTON_BACK, "back"},
    {SDL_GAMEPAD_BUTTON_START, "start"},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK, "left_stick"},
    {SDL_GAMEPAD_BUTTON_RIGHT_STICK, "right_stick"},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, "left_shoulder"},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "right_shoulder"},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, "dpad_up"},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, "dpad_down"},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, "dpad_left"},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, "dpad_right"},
};

constexpr GamepadNameEntry GamepadAxisNames[] = {
    {SDL_GAMEPAD_AXIS_LEFTX, "leftx"},
    {SDL_GAMEPAD_AXIS_LEFTY, "lefty"},
    {SDL_GAMEPAD_AXIS_RIGHTX, "rightx"},
    {SDL_GAMEPAD_AXIS_RIGHTY, "righty"},
    {SDL_GAMEPAD_AXIS_LEFT_TRIGGER, "left_trigger"},
    {SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, "right_trigger"},
};

std::string lowerAscii(std::string_view text)
{
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

std::optional<int> parseInt(std::string_view text)
{
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(std::string(text), &consumed);
        if (consumed == text.size()) {
            return value;
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

void addKeyboard(
    InputBindingMap& bindings,
    InputAction action,
    SDL_Scancode scancode,
    InputModifiers modifiers = InputModifiers::None)
{
    bindings[inputActionIndex(action)].push_back({
        .device = InputBindingDevice::Keyboard,
        .code = static_cast<int>(scancode),
        .modifiers = modifiers,
    });
}

void addMouse(InputBindingMap& bindings, InputAction action, int button)
{
    bindings[inputActionIndex(action)].push_back({
        .device = InputBindingDevice::MouseButton,
        .code = button,
    });
}

void addGamepadButton(InputBindingMap& bindings, InputAction action, SDL_GamepadButton button)
{
    bindings[inputActionIndex(action)].push_back({
        .device = InputBindingDevice::GamepadButton,
        .code = static_cast<int>(button),
    });
}

void addGamepadAxis(
    InputBindingMap& bindings,
    InputAction action,
    SDL_GamepadAxis axis,
    int direction,
    float threshold)
{
    bindings[inputActionIndex(action)].push_back({
        .device = InputBindingDevice::GamepadAxis,
        .code = static_cast<int>(axis),
        .direction = direction < 0 ? -1 : 1,
        .threshold = threshold,
    });
}

bool bindingValid(const InputBinding& binding)
{
    switch (binding.device) {
    case InputBindingDevice::Keyboard:
        return binding.code > SDL_SCANCODE_UNKNOWN && binding.code < SDL_SCANCODE_COUNT;
    case InputBindingDevice::MouseButton:
        return binding.code == SDL_BUTTON_LEFT ||
            binding.code == SDL_BUTTON_MIDDLE ||
            binding.code == SDL_BUTTON_RIGHT ||
            binding.code == SDL_BUTTON_X1 ||
            binding.code == SDL_BUTTON_X2;
    case InputBindingDevice::GamepadButton:
        return binding.code >= 0 && binding.code < SDL_GAMEPAD_BUTTON_COUNT;
    case InputBindingDevice::GamepadAxis:
        return binding.code >= 0 && binding.code < SDL_GAMEPAD_AXIS_COUNT && binding.direction != 0;
    }
    return false;
}

bool requiresBinding(InputAction action)
{
    switch (action) {
    case InputAction::MoveLeft:
    case InputAction::MoveRight:
    case InputAction::MoveUp:
    case InputAction::MoveDown:
    case InputAction::ShiftRingLeft:
    case InputAction::ShiftRingRight:
    case InputAction::ShiftRingUp:
    case InputAction::ShiftRingDown:
    case InputAction::RingCommandModifier:
    case InputAction::Confirm:
    case InputAction::Cancel:
    case InputAction::Pause:
    case InputAction::OpenInventory:
        return true;
    default:
        return false;
    }
}

} // namespace

std::string_view inputActionName(InputAction action)
{
    for (const ActionNameEntry& entry : ActionNames) {
        if (entry.action == action) {
            return entry.name;
        }
    }
    return "Unknown";
}

std::optional<InputAction> parseInputAction(std::string_view name)
{
    const std::string normalized = lowerAscii(name);
    for (const ActionNameEntry& entry : ActionNames) {
        if (lowerAscii(entry.name) == normalized) {
            return entry.action;
        }
    }
    return std::nullopt;
}

std::string_view inputBindingDeviceName(InputBindingDevice device)
{
    switch (device) {
    case InputBindingDevice::Keyboard: return "keyboard";
    case InputBindingDevice::MouseButton: return "mouse";
    case InputBindingDevice::GamepadButton: return "gamepad_button";
    case InputBindingDevice::GamepadAxis: return "gamepad_axis";
    }
    return "keyboard";
}

std::optional<InputBindingDevice> parseInputBindingDevice(std::string_view name)
{
    const std::string normalized = lowerAscii(name);
    if (normalized == "keyboard" || normalized == "key") {
        return InputBindingDevice::Keyboard;
    }
    if (normalized == "mouse" || normalized == "mouse_button" || normalized == "mouse-button") {
        return InputBindingDevice::MouseButton;
    }
    if (normalized == "gamepad_button" || normalized == "gamepad-button" || normalized == "pad_button" || normalized == "pad-button" || normalized == "button") {
        return InputBindingDevice::GamepadButton;
    }
    if (normalized == "gamepad_axis" || normalized == "gamepad-axis" || normalized == "pad_axis" || normalized == "pad-axis" || normalized == "axis") {
        return InputBindingDevice::GamepadAxis;
    }
    return std::nullopt;
}

std::string_view inputModifierName(InputModifiers modifier)
{
    switch (modifier) {
    case InputModifiers::Shift: return "shift";
    case InputModifiers::Ctrl: return "ctrl";
    case InputModifiers::Alt: return "alt";
    case InputModifiers::Gui: return "gui";
    case InputModifiers::None: return "none";
    }
    return "none";
}

std::optional<InputModifiers> parseInputModifier(std::string_view name)
{
    const std::string normalized = lowerAscii(name);
    if (normalized == "shift") {
        return InputModifiers::Shift;
    }
    if (normalized == "ctrl" || normalized == "control") {
        return InputModifiers::Ctrl;
    }
    if (normalized == "alt") {
        return InputModifiers::Alt;
    }
    if (normalized == "gui" || normalized == "win" || normalized == "meta") {
        return InputModifiers::Gui;
    }
    if (normalized == "none") {
        return InputModifiers::None;
    }
    return std::nullopt;
}

std::string inputModifierDisplayPrefix(InputModifiers modifiers)
{
    std::string result;
    const auto append = [&](InputModifiers modifier, std::string_view label) {
        if (inputModifiersContain(modifiers, modifier)) {
            result += label;
            result += '+';
        }
    };
    append(InputModifiers::Ctrl, "Ctrl");
    append(InputModifiers::Alt, "Alt");
    append(InputModifiers::Shift, "Shift");
    append(InputModifiers::Gui, "Gui");
    return result;
}

int inputModifierCount(InputModifiers modifiers)
{
    std::uint8_t bits = static_cast<std::uint8_t>(modifiers);
    int count = 0;
    while (bits != 0) {
        count += bits & 1u;
        bits >>= 1u;
    }
    return count;
}

InputBindingMap defaultInputBindings()
{
    InputBindingMap bindings{};

    addKeyboard(bindings, InputAction::MoveLeft, SDL_SCANCODE_A);
    addKeyboard(bindings, InputAction::MoveLeft, SDL_SCANCODE_LEFT);
    addKeyboard(bindings, InputAction::MoveRight, SDL_SCANCODE_D);
    addKeyboard(bindings, InputAction::MoveRight, SDL_SCANCODE_RIGHT);
    addKeyboard(bindings, InputAction::MoveUp, SDL_SCANCODE_W);
    addKeyboard(bindings, InputAction::MoveUp, SDL_SCANCODE_UP);
    addKeyboard(bindings, InputAction::MoveDown, SDL_SCANCODE_S);
    addKeyboard(bindings, InputAction::MoveDown, SDL_SCANCODE_DOWN);
    addKeyboard(
        bindings,
        InputAction::ShortcutCursorLeft,
        SDL_SCANCODE_LEFT,
        InputModifiers::Shift);
    addKeyboard(
        bindings,
        InputAction::ShortcutCursorRight,
        SDL_SCANCODE_RIGHT,
        InputModifiers::Shift);
    addKeyboard(bindings, InputAction::DirectShortcut1, SDL_SCANCODE_1);
    addKeyboard(bindings, InputAction::DirectShortcut2, SDL_SCANCODE_2);
    addKeyboard(bindings, InputAction::DirectShortcut3, SDL_SCANCODE_3);
    addKeyboard(bindings, InputAction::DirectShortcut4, SDL_SCANCODE_4);
    addKeyboard(bindings, InputAction::DirectShortcut5, SDL_SCANCODE_5);
    addKeyboard(bindings, InputAction::DirectShortcut6, SDL_SCANCODE_6);
    addKeyboard(bindings, InputAction::DirectShortcut7, SDL_SCANCODE_7);
    addKeyboard(bindings, InputAction::DirectShortcut8, SDL_SCANCODE_8);
    addKeyboard(
        bindings,
        InputAction::PreviousShortcutRow,
        SDL_SCANCODE_UP,
        InputModifiers::Shift);
    addKeyboard(
        bindings,
        InputAction::NextShortcutRow,
        SDL_SCANCODE_DOWN,
        InputModifiers::Shift);
    addKeyboard(bindings, InputAction::ToggleShortcutRow, SDL_SCANCODE_TAB);
    addKeyboard(bindings, InputAction::UseSelectedItem, SDL_SCANCODE_F);
    addKeyboard(bindings, InputAction::Confirm, SDL_SCANCODE_RETURN);
    addKeyboard(bindings, InputAction::Confirm, SDL_SCANCODE_KP_ENTER);
    addKeyboard(bindings, InputAction::PutSelectedItemOnRing, SDL_SCANCODE_R);
    addKeyboard(bindings, InputAction::GrabOrPlaceItem, SDL_SCANCODE_G);
    addKeyboard(bindings, InputAction::ArrangeItems, SDL_SCANCODE_T);
    addKeyboard(bindings, InputAction::PreviousActiveRing, SDL_SCANCODE_Z);
    addKeyboard(bindings, InputAction::NextActiveRing, SDL_SCANCODE_X);
    addKeyboard(bindings, InputAction::ThrowActiveRing, SDL_SCANCODE_C);
    addKeyboard(bindings, InputAction::ToggleProtection, SDL_SCANCODE_P);
    addKeyboard(bindings, InputAction::Cancel, SDL_SCANCODE_BACKSPACE);
    addKeyboard(bindings, InputAction::Pause, SDL_SCANCODE_ESCAPE);
    addKeyboard(bindings, InputAction::OpenInventory, SDL_SCANCODE_I);
    addKeyboard(bindings, InputAction::OpenOptions, SDL_SCANCODE_F9);
    addKeyboard(bindings, InputAction::OpenCredits, SDL_SCANCODE_F10);
    addKeyboard(bindings, InputAction::ToggleDebug, SDL_SCANCODE_F1);
    addKeyboard(bindings, InputAction::TestRestart, SDL_SCANCODE_F5);
    addKeyboard(bindings, InputAction::ToggleDebugPause, SDL_SCANCODE_F6);
    addKeyboard(bindings, InputAction::ToggleTestFreeze, SDL_SCANCODE_F7);
    addKeyboard(bindings, InputAction::OpenConsole, SDL_SCANCODE_F8);
    addKeyboard(bindings, InputAction::ToggleAutoReloadBlock, SDL_SCANCODE_F2);

    addMouse(bindings, InputAction::OffsetRingCenter, SDL_BUTTON_RIGHT);
    addMouse(bindings, InputAction::Cancel, SDL_BUTTON_RIGHT);

    addGamepadButton(bindings, InputAction::Confirm, SDL_GAMEPAD_BUTTON_SOUTH);
    addGamepadButton(bindings, InputAction::UseSelectedItem, SDL_GAMEPAD_BUTTON_SOUTH);
    addGamepadButton(bindings, InputAction::Cancel, SDL_GAMEPAD_BUTTON_EAST);
    addGamepadButton(bindings, InputAction::OpenInventory, SDL_GAMEPAD_BUTTON_NORTH);
    addGamepadButton(bindings, InputAction::ArrangeItems, SDL_GAMEPAD_BUTTON_WEST);
    addGamepadButton(bindings, InputAction::ToggleProtection, SDL_GAMEPAD_BUTTON_BACK);
    addGamepadButton(bindings, InputAction::Pause, SDL_GAMEPAD_BUTTON_START);
    addGamepadButton(bindings, InputAction::GrabOrPlaceItem, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    addGamepadButton(bindings, InputAction::GrabOrPlaceItem, SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    addGamepadButton(bindings, InputAction::PreviousActiveRing, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    addGamepadButton(bindings, InputAction::NextActiveRing, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    addGamepadButton(bindings, InputAction::OpenOptions, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    addGamepadButton(bindings, InputAction::OpenCredits, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    addGamepadButton(bindings, InputAction::ShortcutCursorLeft, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    addGamepadButton(bindings, InputAction::ShortcutCursorRight, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    addGamepadButton(bindings, InputAction::NextShortcutRow, SDL_GAMEPAD_BUTTON_DPAD_UP);
    addGamepadButton(bindings, InputAction::ToggleShortcutRow, SDL_GAMEPAD_BUTTON_DPAD_UP);
    addGamepadButton(bindings, InputAction::PutSelectedItemOnRing, SDL_GAMEPAD_BUTTON_DPAD_DOWN);

    addGamepadAxis(bindings, InputAction::MoveLeft, SDL_GAMEPAD_AXIS_LEFTX, -1, StickDigitalThreshold);
    addGamepadAxis(bindings, InputAction::MoveRight, SDL_GAMEPAD_AXIS_LEFTX, 1, StickDigitalThreshold);
    addGamepadAxis(bindings, InputAction::MoveUp, SDL_GAMEPAD_AXIS_LEFTY, -1, StickDigitalThreshold);
    addGamepadAxis(bindings, InputAction::MoveDown, SDL_GAMEPAD_AXIS_LEFTY, 1, StickDigitalThreshold);
    addGamepadAxis(bindings, InputAction::ShiftRingLeft, SDL_GAMEPAD_AXIS_RIGHTX, -1, StickDigitalThreshold);
    addGamepadAxis(bindings, InputAction::ShiftRingRight, SDL_GAMEPAD_AXIS_RIGHTX, 1, StickDigitalThreshold);
    addGamepadAxis(bindings, InputAction::ShiftRingUp, SDL_GAMEPAD_AXIS_RIGHTY, -1, StickDigitalThreshold);
    addGamepadAxis(bindings, InputAction::ShiftRingDown, SDL_GAMEPAD_AXIS_RIGHTY, 1, StickDigitalThreshold);
    addGamepadAxis(bindings, InputAction::RingCommandModifier, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 1, TriggerDigitalThreshold);
    addGamepadAxis(bindings, InputAction::ThrowActiveRing, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1, TriggerDigitalThreshold);

    return bindings;
}

InputBindingMap sanitizeInputBindings(InputBindingMap bindings)
{
    const InputBindingMap defaults = defaultInputBindings();
    for (int actionIndex = 0; actionIndex < InputActionCount; ++actionIndex) {
        auto& actionBindings = bindings[actionIndex];
        actionBindings.erase(
            std::remove_if(actionBindings.begin(), actionBindings.end(), [](const InputBinding& binding) {
                return !bindingValid(binding);
            }),
            actionBindings.end());
        for (InputBinding& binding : actionBindings) {
            constexpr std::uint8_t ValidModifierBits =
                static_cast<std::uint8_t>(InputModifiers::Shift) |
                static_cast<std::uint8_t>(InputModifiers::Ctrl) |
                static_cast<std::uint8_t>(InputModifiers::Alt) |
                static_cast<std::uint8_t>(InputModifiers::Gui);
            binding.modifiers = binding.device == InputBindingDevice::Keyboard
                ? static_cast<InputModifiers>(
                    static_cast<std::uint8_t>(binding.modifiers) & ValidModifierBits)
                : InputModifiers::None;
            if (binding.device == InputBindingDevice::GamepadAxis) {
                binding.direction = binding.direction < 0 ? -1 : 1;
                binding.threshold = std::clamp(binding.threshold, 0.05f, 1.0f);
            } else {
                binding.direction = 0;
                binding.threshold = 0.5f;
            }
        }
        const InputAction action = static_cast<InputAction>(actionIndex);
        if (requiresBinding(action) && actionBindings.empty()) {
            actionBindings = defaults[actionIndex];
        }
    }
    return bindings;
}

bool inputBindingEquals(const InputBinding& lhs, const InputBinding& rhs)
{
    return lhs.device == rhs.device &&
        lhs.code == rhs.code &&
        lhs.direction == rhs.direction &&
        std::fabs(lhs.threshold - rhs.threshold) < 0.0001f &&
        lhs.modifiers == rhs.modifiers;
}

bool inputActionRequiresBinding(InputAction action)
{
    return requiresBinding(action);
}

std::string inputBindingDisplayName(const InputBinding& binding)
{
    switch (binding.device) {
    case InputBindingDevice::Keyboard:
        return "Key:" +
            inputModifierDisplayPrefix(binding.modifiers) +
            keyboardScancodeName(binding.code);
    case InputBindingDevice::MouseButton:
        return "Mouse:" + mouseButtonName(binding.code);
    case InputBindingDevice::GamepadButton:
        return "Pad:" + gamepadButtonName(binding.code);
    case InputBindingDevice::GamepadAxis:
        return "PadAxis:" + gamepadAxisName(binding.code) + (binding.direction < 0 ? "-" : "+");
    }
    return "Unknown";
}

std::string keyboardScancodeName(int scancode)
{
    const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode));
    if (name != nullptr && name[0] != '\0') {
        return name;
    }
    return "scancode:" + std::to_string(scancode);
}

std::optional<int> parseKeyboardScancode(std::string_view name)
{
    const std::string normalized = lowerAscii(name);
    constexpr std::string_view Prefix = "scancode:";
    if (normalized.rfind(std::string(Prefix), 0) == 0) {
        return parseInt(normalized.substr(Prefix.size()));
    }
    const SDL_Scancode scancode = SDL_GetScancodeFromName(std::string(name).c_str());
    if (scancode != SDL_SCANCODE_UNKNOWN) {
        return static_cast<int>(scancode);
    }
    return std::nullopt;
}

std::string mouseButtonName(int button)
{
    switch (button) {
    case SDL_BUTTON_LEFT: return "left";
    case SDL_BUTTON_MIDDLE: return "middle";
    case SDL_BUTTON_RIGHT: return "right";
    case SDL_BUTTON_X1: return "x1";
    case SDL_BUTTON_X2: return "x2";
    default: return "button:" + std::to_string(button);
    }
}

std::optional<int> parseMouseButton(std::string_view name)
{
    const std::string normalized = lowerAscii(name);
    if (normalized == "left") return SDL_BUTTON_LEFT;
    if (normalized == "middle") return SDL_BUTTON_MIDDLE;
    if (normalized == "right") return SDL_BUTTON_RIGHT;
    if (normalized == "x1") return SDL_BUTTON_X1;
    if (normalized == "x2") return SDL_BUTTON_X2;
    constexpr std::string_view Prefix = "button:";
    if (normalized.rfind(std::string(Prefix), 0) == 0) {
        return parseInt(normalized.substr(Prefix.size()));
    }
    return std::nullopt;
}

std::string gamepadButtonName(int button)
{
    for (const GamepadNameEntry& entry : GamepadButtonNames) {
        if (entry.code == button) {
            return std::string(entry.name);
        }
    }
    return "button:" + std::to_string(button);
}

std::optional<int> parseGamepadButton(std::string_view name)
{
    const std::string normalized = lowerAscii(name);
    for (const GamepadNameEntry& entry : GamepadButtonNames) {
        if (entry.name == normalized) {
            return entry.code;
        }
    }
    constexpr std::string_view Prefix = "button:";
    if (normalized.rfind(std::string(Prefix), 0) == 0) {
        return parseInt(normalized.substr(Prefix.size()));
    }
    return std::nullopt;
}

std::string gamepadAxisName(int axis)
{
    for (const GamepadNameEntry& entry : GamepadAxisNames) {
        if (entry.code == axis) {
            return std::string(entry.name);
        }
    }
    return "axis:" + std::to_string(axis);
}

std::optional<int> parseGamepadAxis(std::string_view name)
{
    const std::string normalized = lowerAscii(name);
    for (const GamepadNameEntry& entry : GamepadAxisNames) {
        if (entry.name == normalized) {
            return entry.code;
        }
    }
    constexpr std::string_view Prefix = "axis:";
    if (normalized.rfind(std::string(Prefix), 0) == 0) {
        return parseInt(normalized.substr(Prefix.size()));
    }
    return std::nullopt;
}

}
