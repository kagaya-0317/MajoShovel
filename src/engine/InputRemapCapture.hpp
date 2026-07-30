#pragma once

#include "engine/InputBinding.hpp"

#include <SDL3/SDL.h>

namespace majo {

enum class InputRemapCaptureDeviceGroup {
    KeyboardMouse,
    Gamepad,
};

enum class InputRemapCaptureResult {
    None,
    Captured,
    Cancelled,
    ClearRequested,
};

class InputRemapCapture {
public:
    void begin(InputAction action, InputRemapCaptureDeviceGroup deviceGroup);
    void cancel();

    bool active() const { return active_; }
    InputAction action() const { return action_; }
    InputRemapCaptureDeviceGroup deviceGroup() const { return deviceGroup_; }

    bool shouldConsumeEvent(const SDL_Event& event) const;
    InputRemapCaptureResult handleEvent(const SDL_Event& event, InputBinding& outBinding);

private:
    bool active_ = false;
    InputAction action_ = InputAction::Count;
    InputRemapCaptureDeviceGroup deviceGroup_ = InputRemapCaptureDeviceGroup::KeyboardMouse;
    SDL_Scancode pendingModifierScancode_ = SDL_SCANCODE_UNKNOWN;
};

}
