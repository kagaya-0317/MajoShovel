#include "engine/Input.hpp"

namespace majo {

namespace {

struct KeyBinding {
    SDL_Scancode key;
    InputAction action;
};

constexpr KeyBinding KeyBindings[] = {
    {SDL_SCANCODE_A, InputAction::MoveLeft},
    {SDL_SCANCODE_LEFT, InputAction::MoveLeft},
    {SDL_SCANCODE_D, InputAction::MoveRight},
    {SDL_SCANCODE_RIGHT, InputAction::MoveRight},
    {SDL_SCANCODE_W, InputAction::MoveUp},
    {SDL_SCANCODE_UP, InputAction::MoveUp},
    {SDL_SCANCODE_S, InputAction::MoveDown},
    {SDL_SCANCODE_DOWN, InputAction::MoveDown},
    {SDL_SCANCODE_Q, InputAction::ShortcutCursorLeft},
    {SDL_SCANCODE_E, InputAction::ShortcutCursorRight},
    {SDL_SCANCODE_1, InputAction::DirectShortcut1},
    {SDL_SCANCODE_2, InputAction::DirectShortcut2},
    {SDL_SCANCODE_3, InputAction::DirectShortcut3},
    {SDL_SCANCODE_4, InputAction::DirectShortcut4},
    {SDL_SCANCODE_5, InputAction::DirectShortcut5},
    {SDL_SCANCODE_6, InputAction::DirectShortcut6},
    {SDL_SCANCODE_7, InputAction::DirectShortcut7},
    {SDL_SCANCODE_8, InputAction::DirectShortcut8},
    {SDL_SCANCODE_TAB, InputAction::ToggleShortcutRow},
    {SDL_SCANCODE_F, InputAction::UseSelectedItem},
    {SDL_SCANCODE_RETURN, InputAction::Confirm},
    {SDL_SCANCODE_KP_ENTER, InputAction::Confirm},
    {SDL_SCANCODE_R, InputAction::PutSelectedItemOnRing},
    {SDL_SCANCODE_G, InputAction::GrabOrPlaceItem},
    {SDL_SCANCODE_Z, InputAction::PreviousActiveRing},
    {SDL_SCANCODE_X, InputAction::NextActiveRing},
    {SDL_SCANCODE_C, InputAction::ThrowActiveRing},
    {SDL_SCANCODE_P, InputAction::ToggleProtection},
    {SDL_SCANCODE_BACKSPACE, InputAction::Cancel},
    {SDL_SCANCODE_ESCAPE, InputAction::Pause},
    {SDL_SCANCODE_I, InputAction::OpenInventory},
    {SDL_SCANCODE_F1, InputAction::ToggleDebug},
    {SDL_SCANCODE_F5, InputAction::TestRestart},
    {SDL_SCANCODE_F6, InputAction::ToggleDebugPause},
    {SDL_SCANCODE_F7, InputAction::ToggleTestFreeze},
    {SDL_SCANCODE_F8, InputAction::OpenConsole},
    {SDL_SCANCODE_F2, InputAction::ToggleAutoReloadBlock},
};

constexpr float AxisDeadzone = 0.24f;
constexpr float DigitalAxisThreshold = 0.45f;
constexpr float TriggerThreshold = 0.35f;

constexpr int actionIndex(InputAction action)
{
    return static_cast<int>(action);
}

int shortcutSlotForAction(InputAction action)
{
    const int first = actionIndex(InputAction::DirectShortcut1);
    const int index = actionIndex(action) - first;
    return index >= 0 && index < 8 ? index : -1;
}

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

}

Input::~Input()
{
    closeGamepad();
}

void Input::beginFrame()
{
    pressed_.fill(false);
    released_.fill(false);
    mouseLeftPressed_ = false;
    mouseLeftReleased_ = false;
    ctrlSavePressed_ = false;
    ctrlUndoPressed_ = false;
    ctrlRedoPressed_ = false;
    shortcutCursorDelta_ = 0;
    mouseWheelDelta_ = 0;
    shortcutSlotPressed_ = -1;
    activeRingDelta_ = 0;
}

void Input::handleEvent(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_QUIT) {
        quitRequested_ = true;
    }
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (gamepad_ == nullptr) {
            openGamepad(event.gdevice.which);
        }
    }
    if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        if (event.gdevice.which == gamepadId_) {
            closeGamepad();
            openFirstGamepad();
        }
    }
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        const SDL_Keymod mods = SDL_GetModState();
        const bool ctrlDown = (mods & SDL_KMOD_CTRL) != 0;
        if (ctrlDown) {
            if (event.key.scancode == SDL_SCANCODE_S) {
                ctrlSavePressed_ = true;
                return;
            }
            if (event.key.scancode == SDL_SCANCODE_Z) {
                ctrlUndoPressed_ = true;
                return;
            }
            if (event.key.scancode == SDL_SCANCODE_Y) {
                ctrlRedoPressed_ = true;
                return;
            }
        }
        for (const KeyBinding& binding : KeyBindings) {
            if (event.key.scancode == binding.key) {
                addSourceHold(InputSource::Keyboard, binding.action);
                break;
            }
        }
    }
    if (event.type == SDL_EVENT_KEY_UP) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        for (const KeyBinding& binding : KeyBindings) {
            if (event.key.scancode == binding.key) {
                removeSourceHold(InputSource::Keyboard, binding.action);
                break;
            }
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        if (event.button.button == SDL_BUTTON_LEFT) {
            mouseLeftPressed_ = true;
            mouseLeftHeld_ = true;
            setSourceHeld(InputSource::Mouse, InputAction::CaptureNet, true);
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            setSourceHeld(InputSource::Mouse, InputAction::OffsetRingCenter, true);
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        if (event.button.button == SDL_BUTTON_LEFT) {
            mouseLeftHeld_ = false;
            setSourceHeld(InputSource::Mouse, InputAction::CaptureNet, false);
            mouseLeftReleased_ = true;
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            setSourceHeld(InputSource::Mouse, InputAction::OffsetRingCenter, false);
        }
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        if (event.wheel.y > 0.0f) {
            --mouseWheelDelta_;
            press(InputAction::ShortcutCursorLeft);
        } else if (event.wheel.y < 0.0f) {
            ++mouseWheelDelta_;
            press(InputAction::ShortcutCursorRight);
        }
    }
    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        if (gamepad_ == nullptr) {
            openGamepad(event.gbutton.which);
        }
        if (event.gbutton.which == gamepadId_) {
            lastActiveDevice_ = InputDeviceKind::Gamepad;
            handleGamepadButton(static_cast<SDL_GamepadButton>(event.gbutton.button), event.gbutton.down);
        }
    }
    if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event.gaxis.which == gamepadId_) {
        if (std::fabs(normalizeGamepadAxis(event.gaxis.value)) > 0.0f) {
            lastActiveDevice_ = InputDeviceKind::Gamepad;
        }
    }
}

void Input::update(int, int)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    setSourceHeld(InputSource::Keyboard, InputAction::MoveLeft, keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]);
    setSourceHeld(InputSource::Keyboard, InputAction::MoveRight, keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]);
    setSourceHeld(InputSource::Keyboard, InputAction::MoveUp, keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]);
    setSourceHeld(InputSource::Keyboard, InputAction::MoveDown, keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]);

    updateGamepadState();

    moveAxis_ = leftStickAxis_;
    if (held(InputAction::MoveLeft)) moveAxis_.x -= 1.0f;
    if (held(InputAction::MoveRight)) moveAxis_.x += 1.0f;
    if (held(InputAction::MoveUp)) moveAxis_.y -= 1.0f;
    if (held(InputAction::MoveDown)) moveAxis_.y += 1.0f;
    if (lengthSquared(moveAxis_) > 1.0f) {
        moveAxis_ = normalize(moveAxis_);
    }

    float mx = 0.0f;
    float my = 0.0f;
    SDL_GetMouseState(&mx, &my);
    mouseScreen_ = {mx, my};
}

bool Input::pressed(InputAction action) const
{
    return pressed_[actionIndex(action)];
}

bool Input::released(InputAction action) const
{
    return released_[actionIndex(action)];
}

bool Input::held(InputAction action) const
{
    return held_[actionIndex(action)];
}

bool Input::upgradePressed(int option) const
{
    return option >= 0 && option < 3 && shortcutSlotPressed_ == option;
}

bool Input::openGamepad(SDL_JoystickID id)
{
    if (gamepad_ != nullptr && id == gamepadId_) {
        return true;
    }
    closeGamepad();
    gamepad_ = SDL_OpenGamepad(id);
    if (gamepad_ == nullptr) {
        gamepadId_ = 0;
        return false;
    }
    gamepadId_ = SDL_GetGamepadID(gamepad_);
    return true;
}

void Input::openFirstGamepad()
{
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids == nullptr) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (SDL_IsGamepad(ids[i]) && openGamepad(ids[i])) {
            break;
        }
    }
    SDL_free(ids);
}

void Input::closeGamepad()
{
    clearSource(InputSource::Gamepad);
    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
    }
    gamepadId_ = 0;
    leftStickAxis_ = {};
    aimAxis_ = {};
    hasAimAxis_ = false;
}

void Input::clearSource(InputSource source)
{
    const int sourceIndex = static_cast<int>(source);
    for (int action = 0; action < ActionCount; ++action) {
        if (sourceHoldCounts_[sourceIndex][action] <= 0) {
            continue;
        }
        const bool wasHeld = anySourceHeld(static_cast<InputAction>(action));
        sourceHoldCounts_[sourceIndex][action] = 0;
        const bool nowHeld = anySourceHeld(static_cast<InputAction>(action));
        held_[action] = nowHeld;
        if (wasHeld && !nowHeld) {
            release(static_cast<InputAction>(action));
        }
    }
}

void Input::addSourceHold(InputSource source, InputAction action)
{
    const int sourceIndex = static_cast<int>(source);
    const int index = actionIndex(action);
    const bool wasHeld = anySourceHeld(action);
    ++sourceHoldCounts_[sourceIndex][index];
    held_[index] = true;
    if (!wasHeld) {
        press(action);
    }
}

void Input::removeSourceHold(InputSource source, InputAction action)
{
    const int sourceIndex = static_cast<int>(source);
    const int index = actionIndex(action);
    const bool wasHeld = anySourceHeld(action);
    if (sourceHoldCounts_[sourceIndex][index] > 0) {
        --sourceHoldCounts_[sourceIndex][index];
    }
    const bool nowHeld = anySourceHeld(action);
    held_[index] = nowHeld;
    if (wasHeld && !nowHeld) {
        release(action);
    }
}

void Input::setSourceHeld(InputSource source, InputAction action, bool held)
{
    const int sourceIndex = static_cast<int>(source);
    const int index = actionIndex(action);
    const bool sourceWasHeld = sourceHoldCounts_[sourceIndex][index] > 0;
    if (held == sourceWasHeld) {
        return;
    }
    const bool wasHeld = anySourceHeld(action);
    sourceHoldCounts_[sourceIndex][index] = held ? 1 : 0;
    const bool nowHeld = anySourceHeld(action);
    held_[index] = nowHeld;
    if (!wasHeld && nowHeld) {
        press(action);
    } else if (wasHeld && !nowHeld) {
        release(action);
    }
}

bool Input::sourceHeld(InputSource source, InputAction action) const
{
    return sourceHoldCounts_[static_cast<int>(source)][actionIndex(action)] > 0;
}

bool Input::anySourceHeld(InputAction action) const
{
    const int index = actionIndex(action);
    for (int source = 0; source < InputSourceCount; ++source) {
        if (sourceHoldCounts_[source][index] > 0) {
            return true;
        }
    }
    return false;
}

void Input::handleGamepadButton(SDL_GamepadButton button, bool down)
{
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        setSourceHeld(InputSource::Gamepad, InputAction::Confirm, down);
        break;
    case SDL_GAMEPAD_BUTTON_EAST:
        setSourceHeld(InputSource::Gamepad, InputAction::Cancel, down);
        break;
    case SDL_GAMEPAD_BUTTON_WEST:
        setSourceHeld(InputSource::Gamepad, InputAction::GrabOrPlaceItem, down);
        break;
    case SDL_GAMEPAD_BUTTON_NORTH:
        setSourceHeld(InputSource::Gamepad, InputAction::UseSelectedItem, down);
        break;
    case SDL_GAMEPAD_BUTTON_BACK:
        setSourceHeld(InputSource::Gamepad, InputAction::OpenInventory, down);
        break;
    case SDL_GAMEPAD_BUTTON_START:
        setSourceHeld(InputSource::Gamepad, InputAction::Pause, down);
        break;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        setSourceHeld(InputSource::Gamepad, InputAction::PreviousActiveRing, down);
        break;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        setSourceHeld(InputSource::Gamepad, InputAction::NextActiveRing, down);
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        setSourceHeld(InputSource::Gamepad, InputAction::ShortcutCursorLeft, down);
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        setSourceHeld(InputSource::Gamepad, InputAction::ShortcutCursorRight, down);
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        setSourceHeld(InputSource::Gamepad, InputAction::ToggleShortcutRow, down);
        break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        setSourceHeld(InputSource::Gamepad, InputAction::ThrowActiveRing, down);
        break;
    default:
        break;
    }
}

void Input::updateGamepadState()
{
    if (gamepad_ == nullptr) {
        openFirstGamepad();
    }
    if (gamepad_ == nullptr) {
        leftStickAxis_ = {};
        aimAxis_ = {};
        hasAimAxis_ = false;
        return;
    }
    if (!SDL_GamepadConnected(gamepad_)) {
        closeGamepad();
        openFirstGamepad();
        return;
    }

    const float leftX = normalizeGamepadAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTX));
    const float leftY = normalizeGamepadAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFTY));
    leftStickAxis_ = {leftX, leftY};
    if (lengthSquared(leftStickAxis_) > 1.0f) {
        leftStickAxis_ = normalize(leftStickAxis_);
    }

    updateGamepadAxisAction(InputAction::MoveLeft, InputAction::MoveRight, leftX);
    updateGamepadAxisAction(InputAction::MoveUp, InputAction::MoveDown, leftY);

    const float rightX = normalizeGamepadAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTX));
    const float rightY = normalizeGamepadAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTY));
    aimAxis_ = {rightX, rightY};
    hasAimAxis_ = lengthSquared(aimAxis_) > 0.0001f;
    if (hasAimAxis_ && lengthSquared(aimAxis_) > 1.0f) {
        aimAxis_ = normalize(aimAxis_);
    }

    const float leftTrigger = normalizeGamepadAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    const float rightTrigger = normalizeGamepadAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    updateGamepadTriggerAction(InputAction::OffsetRingCenter, leftTrigger);
    updateGamepadTriggerAction(InputAction::CaptureNet, rightTrigger);

    setSourceHeld(InputSource::Gamepad, InputAction::ShortcutCursorLeft, SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_DPAD_LEFT));
    setSourceHeld(InputSource::Gamepad, InputAction::ShortcutCursorRight, SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_DPAD_RIGHT));
    setSourceHeld(InputSource::Gamepad, InputAction::ToggleShortcutRow, SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_DPAD_UP));
    setSourceHeld(InputSource::Gamepad, InputAction::ThrowActiveRing, SDL_GetGamepadButton(gamepad_, SDL_GAMEPAD_BUTTON_DPAD_DOWN));

    if (lengthSquared(leftStickAxis_) > 0.0001f || hasAimAxis_ || leftTrigger > 0.0f || rightTrigger > 0.0f) {
        lastActiveDevice_ = InputDeviceKind::Gamepad;
    }
}

void Input::updateGamepadAxisAction(InputAction negativeAction, InputAction positiveAction, float value)
{
    setSourceHeld(InputSource::Gamepad, negativeAction, value < -DigitalAxisThreshold);
    setSourceHeld(InputSource::Gamepad, positiveAction, value > DigitalAxisThreshold);
}

void Input::updateGamepadTriggerAction(InputAction action, float value)
{
    setSourceHeld(InputSource::Gamepad, action, value > TriggerThreshold);
}

void Input::press(InputAction action)
{
    pressed_[actionIndex(action)] = true;
    if (action == InputAction::ShortcutCursorLeft) {
        --shortcutCursorDelta_;
    } else if (action == InputAction::ShortcutCursorRight) {
        ++shortcutCursorDelta_;
    } else if (action == InputAction::PreviousActiveRing) {
        --activeRingDelta_;
    } else if (action == InputAction::NextActiveRing) {
        ++activeRingDelta_;
    }

    const int shortcutSlot = shortcutSlotForAction(action);
    if (shortcutSlot >= 0) {
        shortcutSlotPressed_ = shortcutSlot;
    }
}

void Input::release(InputAction action)
{
    released_[actionIndex(action)] = true;
}

}
