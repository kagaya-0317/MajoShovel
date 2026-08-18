#include "engine/Input.hpp"

#include "engine/Renderer.hpp"

#include <algorithm>
#include <cmath>

namespace majo {

namespace {

constexpr float AxisDeadzone = 0.22f;
constexpr float DigitalAxisThreshold = 0.55f;

constexpr int actionIndex(InputAction action)
{
    return inputActionIndex(action);
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
    ctrlCopyPressed_ = other.ctrlCopyPressed_;
    ctrlPastePressed_ = other.ctrlPastePressed_;
    lastActiveDevice_ = other.lastActiveDevice_;
    lastInputModality_ = other.lastInputModality_;
    pressed_ = other.pressed_;
    released_ = other.released_;
    held_ = other.held_;
    sourceHoldCounts_ = other.sourceHoldCounts_;
    consumedKeyboardScancodes_ = other.consumedKeyboardScancodes_;
    consumedGamepadButtons_ = other.consumedGamepadButtons_;
    consumedGamepadAxes_ = other.consumedGamepadAxes_;
    bindings_ = other.bindings_;
    shortcutCursorDelta_ = other.shortcutCursorDelta_;
    mouseWheelDelta_ = other.mouseWheelDelta_;
    cycleDelta_ = other.cycleDelta_;
    moveAxis_ = other.moveAxis_;
    leftStickAxis_ = other.leftStickAxis_;
    ringShiftAxis_ = other.ringShiftAxis_;
    gamepadRingShiftAxis_ = other.gamepadRingShiftAxis_;
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
    ctrlCopyPressed_ = false;
    ctrlPastePressed_ = false;
    shortcutCursorDelta_ = 0;
    mouseWheelDelta_ = 0;
    cycleDelta_ = 0;
}

void Input::handleEvent(const SDL_Event& event)
{
    updateConsumedPhysicalInputSuppression(event, false);

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
            if (event.key.scancode == SDL_SCANCODE_C) {
                ctrlCopyPressed_ = true;
                return;
            }
            if (event.key.scancode == SDL_SCANCODE_V) {
                ctrlPastePressed_ = true;
                return;
            }
        }
    }
    if (event.type == SDL_EVENT_MOUSE_MOTION &&
        (std::fabs(event.motion.xrel) >= 1.0f || std::fabs(event.motion.yrel) >= 1.0f)) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        lastInputModality_ = InputModality::Mouse;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        lastInputModality_ = InputModality::Mouse;
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
        lastInputModality_ = InputModality::Mouse;
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
        lastInputModality_ = InputModality::Mouse;
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
            lastInputModality_ = InputModality::Gamepad;
            handleGamepadButton(static_cast<SDL_GamepadButton>(event.gbutton.button), event.gbutton.down);
        }
    }
    if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION && event.gaxis.which == gamepadId_) {
        if (std::fabs(normalizeGamepadAxis(event.gaxis.value)) > 0.0f) {
            lastActiveDevice_ = InputDeviceKind::Gamepad;
            lastInputModality_ = InputModality::Gamepad;
        }
    }
}

void Input::handleConsumedEvent(const SDL_Event& event)
{
    updateConsumedPhysicalInputSuppression(event, true);
}

void Input::updateConsumedPhysicalInputSuppression(const SDL_Event& event, bool consumed)
{
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
        const int scancode = static_cast<int>(event.key.scancode);
        if (scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_SCANCODE_COUNT) {
            if (event.type == SDL_EVENT_KEY_UP) {
                consumedKeyboardScancodes_[static_cast<std::size_t>(scancode)] = false;
            } else if (consumed) {
                consumedKeyboardScancodes_[static_cast<std::size_t>(scancode)] = true;
            }
        }
        return;
    }

    if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
        const int button = event.gbutton.button;
        if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT) {
            if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
                consumedGamepadButtons_[static_cast<std::size_t>(button)] = false;
            } else if (consumed) {
                consumedGamepadButtons_[static_cast<std::size_t>(button)] = true;
            }
        }
        if (consumed) {
            lastActiveDevice_ = InputDeviceKind::Gamepad;
            lastInputModality_ = InputModality::Gamepad;
        }
        return;
    }

    if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
        const int axis = event.gaxis.axis;
        if (axis >= 0 && axis < SDL_GAMEPAD_AXIS_COUNT) {
            const bool neutral = normalizeGamepadAxis(event.gaxis.value) == 0.0f;
            if (neutral) {
                consumedGamepadAxes_[static_cast<std::size_t>(axis)] = false;
            } else if (consumed) {
                consumedGamepadAxes_[static_cast<std::size_t>(axis)] = true;
            }
        }
        if (consumed && event.gaxis.value != 0) {
            lastActiveDevice_ = InputDeviceKind::Gamepad;
            lastInputModality_ = InputModality::Gamepad;
        }
        return;
    }

    if (consumed && (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                        event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                        event.type == SDL_EVENT_MOUSE_MOTION)) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        lastInputModality_ = InputModality::Mouse;
        return;
    }

    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        consumedKeyboardScancodes_.fill(false);
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        consumedGamepadButtons_.fill(false);
        consumedGamepadAxes_.fill(false);
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

    ringShiftAxis_ = gamepadRingShiftAxis_;
    ringShiftAxis_ += directionalAxisForSource(
        InputSource::Keyboard,
        InputAction::ShiftRingLeft,
        InputAction::ShiftRingRight,
        InputAction::ShiftRingUp,
        InputAction::ShiftRingDown);
    if (lengthSquared(gamepadRingShiftAxis_) <= 0.0001f) {
        ringShiftAxis_ += directionalAxisForSource(
            InputSource::Gamepad,
            InputAction::ShiftRingLeft,
            InputAction::ShiftRingRight,
            InputAction::ShiftRingUp,
            InputAction::ShiftRingDown);
    }
    if (lengthSquared(ringShiftAxis_) > 1.0f) {
        ringShiftAxis_ = normalize(ringShiftAxis_);
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
        cycleDelta_ = 0;
        ringShiftAxis_ = {};
        gamepadRingShiftAxis_ = {};
    }

    moveAxis_ = frame.moveAxis;
    if (lengthSquared(moveAxis_) > 1.0f) {
        moveAxis_ = normalize(moveAxis_);
    }
    mouseScreen_ = frame.aimScreen;
    ringShiftAxis_ = frame.ringShiftAxis;
    if (lengthSquared(ringShiftAxis_) > 1.0f) {
        ringShiftAxis_ = normalize(ringShiftAxis_);
    }
    lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
    lastInputModality_ = InputModality::Keyboard;

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

bool Input::ringOffsetPointerHeld() const
{
    return sourceHeld(InputSource::Mouse, InputAction::OffsetRingCenter);
}

bool Input::removeAllRingItemsPressed() const
{
    return pressed(InputAction::PutSelectedItemOnRing) &&
        held(InputAction::SecondaryActionModifier);
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
    consumedGamepadButtons_.fill(false);
    consumedGamepadAxes_.fill(false);
    if (gamepad_ != nullptr) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
    }
    gamepadId_ = 0;
    leftStickAxis_ = {};
    ringShiftAxis_ = {};
    gamepadRingShiftAxis_ = {};
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
    std::array<int, SDL_SCANCODE_COUNT> bestModifierCounts{};
    bestModifierCounts.fill(-1);
    const bool textInputActive = textInputActiveForKeyboardFocus();
    const InputModifiers activeModifiers = inputModifiersFromSdl(SDL_GetModState());

    const auto bindingActive = [&](const InputBinding& binding) {
        if (binding.device != InputBindingDevice::Keyboard ||
            binding.code <= SDL_SCANCODE_UNKNOWN ||
            binding.code >= SDL_SCANCODE_COUNT ||
            consumedKeyboardScancodes_[static_cast<std::size_t>(binding.code)] ||
            !keys[binding.code] ||
            !inputModifiersContain(activeModifiers, binding.modifiers)) {
            return false;
        }
        return !textInputActive ||
            textInputAllowsPolledScancode(static_cast<SDL_Scancode>(binding.code));
    };

    for (int action = 0; action < ActionCount; ++action) {
        for (const InputBinding& binding : bindings_[action]) {
            if (!bindingActive(binding)) {
                continue;
            }
            bestModifierCounts[static_cast<std::size_t>(binding.code)] = std::max(
                bestModifierCounts[static_cast<std::size_t>(binding.code)],
                inputModifierCount(binding.modifiers));
        }
    }

    for (int action = 0; action < ActionCount; ++action) {
        for (const InputBinding& binding : bindings_[action]) {
            if (!bindingActive(binding) ||
                inputModifierCount(binding.modifiers) !=
                    bestModifierCounts[static_cast<std::size_t>(binding.code)]) {
                continue;
            }
            keyboardHeld[action] = true;
        }
    }

    const auto directionalActionPressed = [&](InputAction action) {
        return keyboardHeld[actionIndex(action)] &&
            !sourceHeld(InputSource::Keyboard, action);
    };
    if (directionalActionPressed(InputAction::MoveLeft) ||
        directionalActionPressed(InputAction::MoveRight) ||
        directionalActionPressed(InputAction::MoveUp) ||
        directionalActionPressed(InputAction::MoveDown)) {
        lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
        lastInputModality_ = InputModality::Keyboard;
    }

    const bool shortcutModifierHeld =
        keyboardHeld[actionIndex(InputAction::SecondaryActionModifier)] ||
        sourceHeld(InputSource::Mouse, InputAction::SecondaryActionModifier);
    if (shortcutModifierHeld) {
        keyboardHeld[actionIndex(InputAction::ShortcutCursorLeft)] =
            keyboardHeld[actionIndex(InputAction::MoveLeft)];
        keyboardHeld[actionIndex(InputAction::ShortcutCursorRight)] =
            keyboardHeld[actionIndex(InputAction::MoveRight)];
        keyboardHeld[actionIndex(InputAction::PreviousShortcutRow)] =
            keyboardHeld[actionIndex(InputAction::MoveUp)];
        keyboardHeld[actionIndex(InputAction::NextShortcutRow)] =
            keyboardHeld[actionIndex(InputAction::MoveDown)];

        // サブ操作中の方向入力はショートカット操作だけへ流す。
        keyboardHeld[actionIndex(InputAction::MoveLeft)] = false;
        keyboardHeld[actionIndex(InputAction::MoveRight)] = false;
        keyboardHeld[actionIndex(InputAction::MoveUp)] = false;
        keyboardHeld[actionIndex(InputAction::MoveDown)] = false;
    }

    for (int action = 0; action < ActionCount; ++action) {
        setSourceHeld(InputSource::Keyboard, static_cast<InputAction>(action), keyboardHeld[action]);
    }
}

Vec2 Input::directionalAxisForSource(
    InputSource source,
    InputAction left,
    InputAction right,
    InputAction up,
    InputAction down) const
{
    return {
        (sourceHeld(source, right) ? 1.0f : 0.0f) - (sourceHeld(source, left) ? 1.0f : 0.0f),
        (sourceHeld(source, down) ? 1.0f : 0.0f) - (sourceHeld(source, up) ? 1.0f : 0.0f),
    };
}

void Input::handleGamepadButton(SDL_GamepadButton button, bool down)
{
    const int buttonIndex = static_cast<int>(button);
    if (buttonIndex >= 0 && buttonIndex < SDL_GAMEPAD_BUTTON_COUNT &&
        consumedGamepadButtons_[static_cast<std::size_t>(buttonIndex)]) {
        return;
    }
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
        gamepadRingShiftAxis_ = {};
        return;
    }
    if (!SDL_GamepadConnected(gamepad_)) {
        closeGamepad();
        openFirstGamepad();
        return;
    }

    std::array<bool, ActionCount> gamepadHeld{};
    leftStickAxis_ = {};
    gamepadRingShiftAxis_ = {};
    updateGamepadButtonBindings(gamepadHeld);

    bool anyMappedAxisActive = false;
    for (int axis = 0; axis < SDL_GAMEPAD_AXIS_COUNT; ++axis) {
        const float value = normalizeGamepadAxis(SDL_GetGamepadAxis(gamepad_, static_cast<SDL_GamepadAxis>(axis)));
        if (value == 0.0f) {
            consumedGamepadAxes_[static_cast<std::size_t>(axis)] = false;
        }
        if (value != 0.0f) {
            anyMappedAxisActive = true;
        }
        const bool axisConsumed = consumedGamepadAxes_[static_cast<std::size_t>(axis)];
        if (!axisConsumed && axis == SDL_GAMEPAD_AXIS_RIGHTX) {
            gamepadRingShiftAxis_.x += value;
        } else if (!axisConsumed && axis == SDL_GAMEPAD_AXIS_RIGHTY) {
            gamepadRingShiftAxis_.y += value;
        }
        updateGamepadAxisBindings(axis, value, gamepadHeld);
    }
    if (lengthSquared(leftStickAxis_) > 1.0f) {
        leftStickAxis_ = normalize(leftStickAxis_);
    }
    if (lengthSquared(gamepadRingShiftAxis_) > 1.0f) {
        gamepadRingShiftAxis_ = normalize(gamepadRingShiftAxis_);
    }

    const auto dpadHeld = [&](SDL_GamepadButton button) {
        const std::size_t index = static_cast<std::size_t>(button);
        return !consumedGamepadButtons_[index] && SDL_GetGamepadButton(gamepad_, button);
    };
    if (gamepadHeld[actionIndex(InputAction::SecondaryActionModifier)]) {
        gamepadHeld[actionIndex(InputAction::ShortcutCursorLeft)] =
            dpadHeld(SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        gamepadHeld[actionIndex(InputAction::ShortcutCursorRight)] =
            dpadHeld(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        gamepadHeld[actionIndex(InputAction::PreviousShortcutRow)] =
            dpadHeld(SDL_GAMEPAD_BUTTON_DPAD_UP);
        gamepadHeld[actionIndex(InputAction::NextShortcutRow)] =
            dpadHeld(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    }
    for (int action = 0; action < ActionCount; ++action) {
        setSourceHeld(InputSource::Gamepad, static_cast<InputAction>(action), gamepadHeld[action]);
    }

    if (lengthSquared(leftStickAxis_) > 0.0001f ||
        lengthSquared(gamepadRingShiftAxis_) > 0.0001f ||
        anyMappedAxisActive) {
        lastActiveDevice_ = InputDeviceKind::Gamepad;
        lastInputModality_ = InputModality::Gamepad;
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
            if (consumedGamepadButtons_[static_cast<std::size_t>(binding.code)]) {
                continue;
            }
            gamepadHeld[action] = gamepadHeld[action] ||
                SDL_GetGamepadButton(gamepad_, static_cast<SDL_GamepadButton>(binding.code));
        }
    }
}

void Input::updateGamepadAxisBindings(int axis, float value, std::array<bool, ActionCount>& gamepadHeld)
{
    if (axis >= 0 && axis < SDL_GAMEPAD_AXIS_COUNT &&
        consumedGamepadAxes_[static_cast<std::size_t>(axis)]) {
        return;
    }
    for (int action = 0; action < ActionCount; ++action) {
        for (const InputBinding& binding : bindings_[action]) {
            if (binding.device != InputBindingDevice::GamepadAxis || binding.code != axis) {
                continue;
            }
            const float amount = binding.direction < 0 ? -value : value;
            if (amount > 0.0f) {
                accumulateGamepadAnalogAxis(static_cast<InputAction>(action), amount);
            }
            if (amount > binding.threshold) {
                gamepadHeld[action] = true;
            }
        }
    }
}

void Input::accumulateGamepadAnalogAxis(InputAction action, float amount)
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
    case InputAction::ShiftRingLeft:
        gamepadRingShiftAxis_.x -= amount;
        break;
    case InputAction::ShiftRingRight:
        gamepadRingShiftAxis_.x += amount;
        break;
    case InputAction::ShiftRingUp:
        gamepadRingShiftAxis_.y -= amount;
        break;
    case InputAction::ShiftRingDown:
        gamepadRingShiftAxis_.y += amount;
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
    } else if (action == InputAction::CyclePrevious) {
        --cycleDelta_;
    } else if (action == InputAction::CycleNext) {
        ++cycleDelta_;
    }

}

void Input::release(InputAction action)
{
    released_[actionIndex(action)] = true;
}

}
