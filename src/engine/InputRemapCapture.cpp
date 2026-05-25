#include "engine/InputRemapCapture.hpp"

#include <cmath>

namespace majo {

namespace {

constexpr float AxisDeadzone = 0.22f;
constexpr float AxisCaptureThreshold = 0.70f;
constexpr float StickDigitalThreshold = 0.55f;
constexpr float TriggerDigitalThreshold = 0.45f;

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

}

void InputRemapCapture::begin(InputAction action, InputRemapCaptureDeviceGroup deviceGroup)
{
    active_ = true;
    action_ = action;
    deviceGroup_ = deviceGroup;
}

void InputRemapCapture::cancel()
{
    active_ = false;
    action_ = InputAction::Count;
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
        if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
            cancel();
            return InputRemapCaptureResult::Cancelled;
        }
        if (event.key.scancode == SDL_SCANCODE_BACKSPACE || event.key.scancode == SDL_SCANCODE_DELETE) {
            cancel();
            return InputRemapCaptureResult::ClearRequested;
        }
        if (deviceGroup_ == InputRemapCaptureDeviceGroup::KeyboardMouse) {
            outBinding = {
                .device = InputBindingDevice::Keyboard,
                .code = static_cast<int>(event.key.scancode),
            };
            cancel();
            return InputRemapCaptureResult::Captured;
        }
    }

    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && deviceGroup_ == InputRemapCaptureDeviceGroup::KeyboardMouse) {
        outBinding = {
            .device = InputBindingDevice::MouseButton,
            .code = event.button.button,
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

}
