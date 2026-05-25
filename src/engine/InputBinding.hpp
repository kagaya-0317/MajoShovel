#pragma once

#include <array>
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
    AimPointer,
    ThrowActiveRing,
    OffsetRingCenter,
    ShortcutCursorLeft,
    ShortcutCursorRight,
    DirectShortcut1,
    DirectShortcut2,
    DirectShortcut3,
    DirectShortcut4,
    DirectShortcut5,
    DirectShortcut6,
    DirectShortcut7,
    DirectShortcut8,
    ToggleShortcutRow,
    UseSelectedItem,
    Confirm,
    PutSelectedItemOnRing,
    GrabOrPlaceItem,
    PreviousActiveRing,
    NextActiveRing,
    CaptureNet,
    ToggleProtection,
    Cancel,
    Pause,
    OpenInventory,
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

struct InputBinding {
    InputBindingDevice device = InputBindingDevice::Keyboard;
    int code = 0;
    int direction = 0;
    float threshold = 0.5f;
};

using InputBindingMap = std::array<std::vector<InputBinding>, InputActionCount>;

std::string_view inputActionName(InputAction action);
std::optional<InputAction> parseInputAction(std::string_view name);

std::string_view inputBindingDeviceName(InputBindingDevice device);
std::optional<InputBindingDevice> parseInputBindingDevice(std::string_view name);

InputBindingMap defaultInputBindings();
InputBindingMap sanitizeInputBindings(InputBindingMap bindings);

bool inputBindingEquals(const InputBinding& lhs, const InputBinding& rhs);
bool inputActionRequiresBinding(InputAction action);
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
