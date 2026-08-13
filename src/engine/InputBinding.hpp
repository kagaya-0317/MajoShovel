#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace majo {

enum class InputAction {
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    ThrowActiveRing,
    OffsetRingCenter,
    ShiftRingLeft,
    ShiftRingRight,
    ShiftRingUp,
    ShiftRingDown,
    SecondaryActionModifier,
    ShortcutCursorLeft,
    ShortcutCursorRight,
    PreviousShortcutRow,
    NextShortcutRow,
    UseSelectedItem,
    DiscardSelectedItem,
    Confirm,
    PutSelectedItemOnRing,
    GrabOrPlaceItem,
    ArrangeItems,
    CyclePrevious,
    CycleNext,
    ToggleProtection,
    Cancel,
    Pause,
    OpenInventory,
    OpenOptions,
    OpenCredits,
    ToggleFullscreen,
    ToggleDebug,
    ToggleDebugPause,
    TestRestart,
    ToggleTestFreeze,
    OpenConsole,
    ToggleAutoReloadBlock,
    Count
};

constexpr int InputActionCount = static_cast<int>(InputAction::Count);

constexpr int inputActionIndex(InputAction action)
{
    return static_cast<int>(action);
}

enum class InputBindingDevice {
    Keyboard,
    MouseButton,
    GamepadButton,
    GamepadAxis,
};

enum class InputModifiers : std::uint8_t {
    None = 0,
    Shift = 1 << 0,
    Ctrl = 1 << 1,
    Alt = 1 << 2,
    Gui = 1 << 3,
};

constexpr InputModifiers operator|(InputModifiers lhs, InputModifiers rhs)
{
    return static_cast<InputModifiers>(
        static_cast<std::uint8_t>(lhs) |
        static_cast<std::uint8_t>(rhs));
}

constexpr InputModifiers& operator|=(InputModifiers& lhs, InputModifiers rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr bool inputModifiersContain(InputModifiers active, InputModifiers required)
{
    return (static_cast<std::uint8_t>(active) & static_cast<std::uint8_t>(required)) ==
        static_cast<std::uint8_t>(required);
}

struct InputBinding {
    InputBindingDevice device = InputBindingDevice::Keyboard;
    int code = 0;
    int direction = 0;
    float threshold = 0.5f;
    InputModifiers modifiers = InputModifiers::None;
};

using InputBindingMap = std::array<std::vector<InputBinding>, InputActionCount>;

std::string_view inputActionName(InputAction action);
std::optional<InputAction> parseInputAction(std::string_view name);

std::string_view inputBindingDeviceName(InputBindingDevice device);
std::optional<InputBindingDevice> parseInputBindingDevice(std::string_view name);
std::string_view inputModifierName(InputModifiers modifier);
std::optional<InputModifiers> parseInputModifier(std::string_view name);
std::string inputModifierDisplayPrefix(InputModifiers modifiers);
int inputModifierCount(InputModifiers modifiers);

InputBindingMap defaultInputBindings();
InputBindingMap sanitizeInputBindings(InputBindingMap bindings);

bool inputBindingEquals(const InputBinding& lhs, const InputBinding& rhs);
bool inputBindingSamePhysicalInput(const InputBinding& lhs, const InputBinding& rhs);
bool inputActionRequiresBinding(InputAction action);
bool inputActionCanBeRemapped(InputAction action);
bool inputActionIsDeveloperOnly(InputAction action);
bool inputActionsConflict(InputAction lhs, InputAction rhs);
std::string inputBindingDisplayName(const InputBinding& binding);

std::string keyboardScancodeName(int scancode);
std::optional<int> parseKeyboardScancode(std::string_view name);
std::string mouseButtonName(int button);
std::optional<int> parseMouseButton(std::string_view name);
std::string gamepadButtonName(int button);
std::optional<int> parseGamepadButton(std::string_view name);
std::string gamepadAxisName(int axis);
std::optional<int> parseGamepadAxis(std::string_view name);

}
