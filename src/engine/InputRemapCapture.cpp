#include "engine/InputRemapCapture.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

namespace {

constexpr float AxisDeadzone = 0.22f;
constexpr float AxisCaptureThreshold = 0.70f;
constexpr float StickDigitalThreshold = 0.55f;
constexpr float TriggerDigitalThreshold = 0.45f;
constexpr Uint64 GamepadCancelHoldMilliseconds = 1000;
constexpr int GamepadCancelButton = SDL_GAMEPAD_BUTTON_EAST;

float normalizeGamepadAxis(Sint16 value)
{
    const float raw = value < 0
        ? static_cast<float>(value) / 32768.0f
        : static_cast<float>(value) / 32767.0f;
    const float magnitude = std::fabs(raw);
    if (magnitude <= AxisDeadzone) {
        return 0.0f;
    }
    const float adjusted = (magnitude - AxisDeadzone) / (1.0f - AxisDeadzone);
    return raw < 0.0f ? -adjusted : adjusted;
}

bool isTriggerAxis(int axis)
{
    return axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
}

bool isModifierScancode(SDL_Scancode scancode)
{
    switch (scancode) {
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
    case SDL_SCANCODE_LGUI:
    case SDL_SCANCODE_RGUI:
        return true;
    default:
        return false;
    }
}

InputModifiers inputModifiersFromSdl(SDL_Keymod modifiers)
{
    InputModifiers result = InputModifiers::None;
    if ((modifiers & SDL_KMOD_SHIFT) != 0) {
        result |= InputModifiers::Shift;
    }
    if ((modifiers & SDL_KMOD_CTRL) != 0) {
        result |= InputModifiers::Ctrl;
    }
    if ((modifiers & SDL_KMOD_ALT) != 0) {
        result |= InputModifiers::Alt;
    }
    if ((modifiers & SDL_KMOD_GUI) != 0) {
        result |= InputModifiers::Gui;
    }
    return result;
}

}

void InputRemapCapture::begin(InputAction action, InputRemapCaptureDeviceGroup deviceGroup)
{
    active_ = true;
    action_ = action;
    deviceGroup_ = deviceGroup;
    pendingModifierScancode_ = SDL_SCANCODE_UNKNOWN;
    pendingGamepadCancelButton_ = -1;
    gamepadCancelHoldStartedAtMs_ = 0;
}

void InputRemapCapture::cancel()
{
    active_ = false;
    action_ = InputAction::Count;
    pendingModifierScancode_ = SDL_SCANCODE_UNKNOWN;
    pendingGamepadCancelButton_ = -1;
    gamepadCancelHoldStartedAtMs_ = 0;
}

bool InputRemapCapture::shouldConsumeEvent(const SDL_Event& event) const
{
    if (!active_) {
        return false;
    }
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        return true;
    default:
        return false;
    }
}

InputRemapCaptureResult InputRemapCapture::handleEvent(const SDL_Event& event, InputBinding& outBinding)
{
    if (!active_) {
        return InputRemapCaptureResult::None;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (deviceGroup_ == InputRemapCaptureDeviceGroup::KeyboardMouse) {
            if (event.key.scancode == SDL_SCANCODE_UNKNOWN) {
                return InputRemapCaptureResult::None;
            }
            if (isModifierScancode(event.key.scancode)) {
                pendingModifierScancode_ = event.key.scancode;
                return InputRemapCaptureResult::None;
            }
            outBinding = {
                .device = InputBindingDevice::Keyboard,
                .code = static_cast<int>(event.key.scancode),
                .modifiers = inputModifiersFromSdl(SDL_GetModState()),
            };
            cancel();
            return InputRemapCaptureResult::Captured;
        }
    }

    if (event.type == SDL_EVENT_KEY_UP &&
        deviceGroup_ == InputRemapCaptureDeviceGroup::KeyboardMouse &&
        event.key.scancode == pendingModifierScancode_) {
        outBinding = {
            .device = InputBindingDevice::Keyboard,
            .code = static_cast<int>(event.key.scancode),
        };
        cancel();
        return InputRemapCaptureResult::Captured;
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        deviceGroup_ == InputRemapCaptureDeviceGroup::KeyboardMouse &&
        event.button.button > 0) {
        outBinding = {
            .device = InputBindingDevice::MouseButton,
            .code = event.button.button,
        };
        cancel();
        return InputRemapCaptureResult::Captured;
    }

    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
        event.gbutton.button == GamepadCancelButton) {
        pendingGamepadCancelButton_ = event.gbutton.button;
        gamepadCancelHoldStartedAtMs_ = SDL_GetTicks();
        return InputRemapCaptureResult::None;
    }

    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
        event.gbutton.button == pendingGamepadCancelButton_) {
        const bool cancelRequested = gamepadCancelHoldProgress() >= 1.0f;
        pendingGamepadCancelButton_ = -1;
        gamepadCancelHoldStartedAtMs_ = 0;
        if (cancelRequested) {
            cancel();
            return InputRemapCaptureResult::Cancelled;
        }
        if (deviceGroup_ != InputRemapCaptureDeviceGroup::Gamepad) {
            return InputRemapCaptureResult::None;
        }
        outBinding = {
            .device = InputBindingDevice::GamepadButton,
            .code = event.gbutton.button,
        };
        cancel();
        return InputRemapCaptureResult::Captured;
    }

    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN && deviceGroup_ == InputRemapCaptureDeviceGroup::Gamepad) {
        outBinding = {
            .device = InputBindingDevice::GamepadButton,
            .code = event.gbutton.button,
        };
        cancel();
        return InputRemapCaptureResult::Captured;
    }

    if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && deviceGroup_ == InputRemapCaptureDeviceGroup::Gamepad) {
        const float value = normalizeGamepadAxis(event.gaxis.value);
        if (std::fabs(value) >= AxisCaptureThreshold) {
            const int axis = event.gaxis.axis;
            outBinding = {
                .device = InputBindingDevice::GamepadAxis,
                .code = axis,
                .direction = value < 0.0f ? -1 : 1,
                .threshold = isTriggerAxis(axis) ? TriggerDigitalThreshold : StickDigitalThreshold,
            };
            cancel();
            return InputRemapCaptureResult::Captured;
        }
    }

    return InputRemapCaptureResult::None;
}

InputRemapCaptureResult InputRemapCapture::update()
{
    if (!active_ || pendingGamepadCancelButton_ < 0 || gamepadCancelHoldProgress() < 1.0f) {
        return InputRemapCaptureResult::None;
    }
    cancel();
    return InputRemapCaptureResult::Cancelled;
}

float InputRemapCapture::gamepadCancelHoldProgress() const
{
    if (!active_ || pendingGamepadCancelButton_ < 0) {
        return 0.0f;
    }
    const Uint64 elapsed = SDL_GetTicks() - gamepadCancelHoldStartedAtMs_;
    return std::clamp(
        static_cast<float>(elapsed) / static_cast<float>(GamepadCancelHoldMilliseconds),
        0.0f,
        1.0f);
}

}
