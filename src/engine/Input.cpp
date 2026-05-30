#include "engine/Input.hpp"

#include "engine/Renderer.hpp"

#include <cmath>

namespace majo {

namespace {

constexpr float AxisDeadzone = 0.22f;
constexpr float DigitalAxisThreshold = 0.55f;

constexpr int actionIndex(InputAction action)
{
    return inputActionIndex(action);
}

int shortcutSlotForAction(InputAction action)
{
    const int first = actionIndex(InputAction::DirectShortcut1);
    const int index = actionIndex(action) - first;
    return index >= 0 && index < 8 ? index : -1;
}

int ringPresetRegisterSlotForScancode(SDL_Scancode scancode)
{
    switch (scancode) {
    case SDL_SCANCODE_1: return 0;
    case SDL_SCANCODE_2: return 1;
    case SDL_SCANCODE_3: return 2;
    default: return -1;
    }
}

bool textInputAllowsPolledScancode(SDL_Scancode scancode)
{
    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F24) {
        return true;
    }
    switch (scancode) {
    case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
    case SDL_SCANCODE_UP:
    case SDL_SCANCODE_DOWN:
    case SDL_SCANCODE_LEFT:
    case SDL_SCANCODE_RIGHT:
    case SDL_SCANCODE_PAGEUP:
    case SDL_SCANCODE_PAGEDOWN:
    case SDL_SCANCODE_HOME:
    case SDL_SCANCODE_END:
        return true;
    default:
        return false;
    }
}

bool textInputActiveForKeyboardFocus()
{
    SDL_Window* window = SDL_GetKeyboardFocus();
    return window != nullptr && SDL_TextInputActive(window);
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
    shutdown();
}

Input::Input(const Input& other)
{
    *this = other;
}

Input& Input::operator=(const Input& other)
{
    if (this == &other) {
        return *this;
    }

    shutdown();
    quitRequested_ = other.quitRequested_;
    mouseLeftPressed_ = other.mouseLeftPressed_;
    mouseLeftReleased_ = other.mouseLeftReleased_;
    mouseLeftHeld_ = other.mouseLeftHeld_;
    ctrlSavePressed_ = other.ctrlSavePressed_;
    ctrlUndoPressed_ = other.ctrlUndoPressed_;
    ctrlRedoPressed_ = other.ctrlRedoPressed_;
    suppressDirectShortcutThisFrame_ = other.suppressDirectShortcutThisFrame_;
    lastActiveDevice_ = other.lastActiveDevice_;
    pressed_ = other.pressed_;
    released_ = other.released_;
    held_ = other.held_;
    sourceHoldCounts_ = other.sourceHoldCounts_;
    bindings_ = other.bindings_;
    shortcutCursorDelta_ = other.shortcutCursorDelta_;
    mouseWheelDelta_ = other.mouseWheelDelta_;
    shortcutSlotPressed_ = other.shortcutSlotPressed_;
    ringPresetRegisterSlotPressed_ = other.ringPresetRegisterSlotPressed_;
    activeRingDelta_ = other.activeRingDelta_;
    moveAxis_ = other.moveAxis_;
    leftStickAxis_ = other.leftStickAxis_;
    aimAxis_ = other.aimAxis_;
    hasAimAxis_ = other.hasAimAxis_;
    mouseScreen_ = other.mouseScreen_;
    gamepad_ = nullptr;
    gamepadId_ = 0;
    return *this;
}

void Input::shutdown()
{
    closeGamepad();
}

void Input::setBindingMap(const InputBindingMap& bindings)
{
    clearSource(InputSource::Keyboard);
    clearSource(InputSource::Mouse);
    clearSource(InputSource::Gamepad);
    bindings_ = sanitizeInputBindings(bindings);
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
    suppressDirectShortcutThisFrame_ = false;
    shortcutCursorDelta_ = 0;
    mouseWheelDelta_ = 0;
    shortcutSlotPressed_ = -1;
    ringPresetRegisterSlotPressed_ = -1;
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
        const bool shiftDown = (mods & SDL_KMOD_SHIFT) != 0;
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
        if (shiftDown) {
            const int registerSlot = ringPresetRegisterSlotForScancode(event.key.scancode);
            if (registerSlot >= 0) {
                ringPresetRegisterSlotPressed_ = registerSlot;
                suppressDirectShortcutThisFrame_ = true;
                return;
            }
        }
        for (int action = 0; action < ActionCount; ++action) {
            for (const InputBinding& binding : bindings_[action]) {
                if (binding.device == InputBindingDevice::Keyboard &&
                    binding.code == static_cast<int>(event.key.scancode)) {
                    setSourceHeld(InputSource::Keyboard, static_cast<InputAction>(action), true);
                }
            }
        }
    }
    if (event.type == SDL_EVENT_KEY_UP) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        if (event.button.button == SDL_BUTTON_LEFT) {
            mouseLeftPressed_ = true;
            mouseLeftHeld_ = true;
        }
        for (int action = 0; action < ActionCount; ++action) {
            for (const InputBinding& binding : bindings_[action]) {
                if (binding.device == InputBindingDevice::MouseButton &&
                    binding.code == event.button.button) {
                    setSourceHeld(InputSource::Mouse, static_cast<InputAction>(action), true);
                }
            }
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        if (event.button.button == SDL_BUTTON_LEFT) {
            mouseLeftHeld_ = false;
            mouseLeftReleased_ = true;
        }
        for (int action = 0; action < ActionCount; ++action) {
            for (const InputBinding& binding : bindings_[action]) {
                if (binding.device == InputBindingDevice::MouseButton &&
                    binding.code == event.button.button) {
                    setSourceHeld(InputSource::Mouse, static_cast<InputAction>(action), false);
                }
            }
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

void Input::update(const Renderer* renderer)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    updateKeyboardPolledBindings(keys);

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
    mouseScreen_ = renderer != nullptr
        ? renderer->windowToRenderCoordinates({mx, my})
        : Vec2{mx, my};
}

void Input::applyAutomation(const InputAutomationFrame& frame)
{
    if (!frame.active) {
        return;
    }

    const auto clearAction = [&](InputAction action) {
        const int index = actionIndex(action);
        pressed_[index] = false;
        released_[index] = false;
        held_[index] = false;
        for (int source = 0; source < InputSourceCount; ++source) {
            sourceHoldCounts_[source][index] = 0;
        }
    };
    const auto keepDuringExclusive = [](InputAction action) {
        return action == InputAction::ToggleDebug ||
            action == InputAction::ToggleDebugPause ||
            action == InputAction::TestRestart ||
            action == InputAction::ToggleTestFreeze ||
            action == InputAction::OpenConsole ||
            action == InputAction::ToggleAutoReloadBlock;
    };
    const auto setAutomatedHeld = [&](InputAction action, bool held) {
        const int index = actionIndex(action);
        held_[index] = held;
        sourceHoldCounts_[static_cast<int>(InputSource::Keyboard)][index] = held ? 1 : 0;
    };
    const auto pressAutomated = [&](InputAction action) {
        const int index = actionIndex(action);
        pressed_[index] = true;
        held_[index] = true;
        sourceHoldCounts_[static_cast<int>(InputSource::Keyboard)][index] = 1;
    };

    if (frame.exclusive) {
        for (int action = 0; action < ActionCount; ++action) {
            const auto inputAction = static_cast<InputAction>(action);
            if (!keepDuringExclusive(inputAction)) {
                clearAction(inputAction);
            }
        }
        mouseLeftPressed_ = false;
        mouseLeftReleased_ = false;
        mouseLeftHeld_ = false;
        ctrlSavePressed_ = false;
        ctrlUndoPressed_ = false;
        ctrlRedoPressed_ = false;
        shortcutCursorDelta_ = 0;
        mouseWheelDelta_ = 0;
        shortcutSlotPressed_ = -1;
        ringPresetRegisterSlotPressed_ = -1;
        suppressDirectShortcutThisFrame_ = false;
        activeRingDelta_ = 0;
        aimAxis_ = {};
        hasAimAxis_ = false;
    }

    moveAxis_ = frame.moveAxis;
    if (lengthSquared(moveAxis_) > 1.0f) {
        moveAxis_ = normalize(moveAxis_);
    }
    mouseScreen_ = frame.aimScreen;
    lastActiveDevice_ = InputDeviceKind::KeyboardMouse;

    setAutomatedHeld(InputAction::MoveLeft, moveAxis_.x < -DigitalAxisThreshold);
    setAutomatedHeld(InputAction::MoveRight, moveAxis_.x > DigitalAxisThreshold);
    setAutomatedHeld(InputAction::MoveUp, moveAxis_.y < -DigitalAxisThreshold);
    setAutomatedHeld(InputAction::MoveDown, moveAxis_.y > DigitalAxisThreshold);
    setAutomatedHeld(InputAction::OffsetRingCenter, frame.ringOffsetHeld);

    if (frame.throwPressed) {
        pressAutomated(InputAction::ThrowActiveRing);
    }
    if (frame.confirmPressed) {
        pressAutomated(InputAction::Confirm);
    }
    if (frame.useItemPressed) {
        pressAutomated(InputAction::UseSelectedItem);
    }
    if (frame.capturePressed) {
        pressAutomated(InputAction::CaptureNet);
    }
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

bool Input::removeAllRingItemsPressed() const
{
    const bool shiftHeld = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
    const bool gamepadOffsetHeld = sourceHeld(InputSource::Gamepad, InputAction::OffsetRingCenter);
    return pressed(InputAction::PutSelectedItemOnRing) && (shiftHeld || gamepadOffsetHeld);
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

void Input::updateKeyboardPolledBindings(const bool* keys)
{
    std::array<bool, ActionCount> keyboardHeld{};
    const bool textInputActive = textInputActiveForKeyboardFocus();
    for (int action = 0; action < ActionCount; ++action) {
        for (const InputBinding& binding : bindings_[action]) {
            if (binding.device != InputBindingDevice::Keyboard) {
                continue;
            }
            if (suppressDirectShortcutThisFrame_ &&
                shortcutSlotForAction(static_cast<InputAction>(action)) >= 0) {
                continue;
            }
            if (binding.code <= SDL_SCANCODE_UNKNOWN || binding.code >= SDL_SCANCODE_COUNT) {
                continue;
            }
            if (textInputActive && !textInputAllowsPolledScancode(static_cast<SDL_Scancode>(binding.code))) {
                continue;
            }
            keyboardHeld[action] = keyboardHeld[action] || keys[binding.code];
        }
    }
    for (int action = 0; action < ActionCount; ++action) {
        setSourceHeld(InputSource::Keyboard, static_cast<InputAction>(action), keyboardHeld[action]);
    }
}

void Input::handleGamepadButton(SDL_GamepadButton button, bool down)
{
    for (int action = 0; action < ActionCount; ++action) {
        for (const InputBinding& binding : bindings_[action]) {
            if (binding.device == InputBindingDevice::GamepadButton &&
                binding.code == static_cast<int>(button)) {
                setSourceHeld(InputSource::Gamepad, static_cast<InputAction>(action), down);
            }
        }
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

    std::array<bool, ActionCount> gamepadHeld{};
    leftStickAxis_ = {};
    updateGamepadButtonBindings(gamepadHeld);

    const float rightX = normalizeGamepadAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTX));
    const float rightY = normalizeGamepadAxis(SDL_GetGamepadAxis(gamepad_, SDL_GAMEPAD_AXIS_RIGHTY));
    aimAxis_ = {rightX, rightY};
    hasAimAxis_ = lengthSquared(aimAxis_) > 0.0001f;
    if (hasAimAxis_ && lengthSquared(aimAxis_) > 1.0f) {
        aimAxis_ = normalize(aimAxis_);
    }

    bool anyMappedAxisActive = false;
    for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
        const float value = normalizeGamepadAxis(SDL_GetGamepadAxis(gamepad_, static_cast<SDL_GamepadAxis>(axis)));
        if (value != 0.0f) {
            anyMappedAxisActive = true;
        }
        updateGamepadAxisBindings(axis, value, gamepadHeld);
    }
    if (lengthSquared(leftStickAxis_) > 1.0f) {
        leftStickAxis_ = normalize(leftStickAxis_);
    }
    for (int action = 0; action < ActionCount; ++action) {
        setSourceHeld(InputSource::Gamepad, static_cast<InputAction>(action), gamepadHeld[action]);
    }

    if (lengthSquared(leftStickAxis_) > 0.0001f || hasAimAxis_ || anyMappedAxisActive) {
        lastActiveDevice_ = InputDeviceKind::Gamepad;
    }
}

void Input::updateGamepadButtonBindings(std::array<bool, ActionCount>& gamepadHeld)
{
    for (int action = 0; action < ActionCount; ++action) {
        for (const InputBinding& binding : bindings_[action]) {
            if (binding.device != InputBindingDevice::GamepadButton) {
                continue;
            }
            if (binding.code < 0 || binding.code >= SDL_GAMEPAD_BUTTON_COUNT) {
                continue;
            }
            gamepadHeld[action] = gamepadHeld[action] ||
                SDL_GetGamepadButton(gamepad_, static_cast<SDL_GamepadButton>(binding.code));
        }
    }
}

void Input::updateGamepadAxisBindings(int axis, float value, std::array<bool, ActionCount>& gamepadHeld)
{
    for (int action = 0; action < ActionCount; ++action) {
        for (const InputBinding& binding : bindings_[action]) {
            if (binding.device != InputBindingDevice::GamepadAxis || binding.code != axis) {
                continue;
            }
            const float amount = binding.direction < 0 ? -value : value;
            if (amount > 0.0f) {
                accumulateGamepadMoveAxis(static_cast<InputAction>(action), amount);
            }
            if (amount > binding.threshold) {
                gamepadHeld[action] = true;
            }
        }
    }
}

void Input::accumulateGamepadMoveAxis(InputAction action, float amount)
{
    switch (action) {
    case InputAction::MoveLeft:
        leftStickAxis_.x -= amount;
        break;
    case InputAction::MoveRight:
        leftStickAxis_.x += amount;
        break;
    case InputAction::MoveUp:
        leftStickAxis_.y -= amount;
        break;
    case InputAction::MoveDown:
        leftStickAxis_.y += amount;
        break;
    default:
        break;
    }
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
