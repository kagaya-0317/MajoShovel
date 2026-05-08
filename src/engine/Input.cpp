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
    {SDL_SCANCODE_C, InputAction::CaptureNet},
    {SDL_SCANCODE_P, InputAction::ToggleProtection},
    {SDL_SCANCODE_ESCAPE, InputAction::Pause},
    {SDL_SCANCODE_I, InputAction::OpenInventory},
    {SDL_SCANCODE_F1, InputAction::ToggleDebug},
    {SDL_SCANCODE_F5, InputAction::TestRestart},
    {SDL_SCANCODE_F6, InputAction::ToggleDebugPause},
};

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

}

void Input::beginFrame()
{
    pressed_.fill(false);
    shortcutCursorDelta_ = 0;
    shortcutSlotPressed_ = -1;
    activeRingDelta_ = 0;
}

void Input::handleEvent(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_QUIT) {
        quitRequested_ = true;
    }
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        for (const KeyBinding& binding : KeyBindings) {
            if (event.key.scancode == binding.key) {
                press(binding.action);
                setHeld(binding.action, true);
                break;
            }
        }
    }
    if (event.type == SDL_EVENT_KEY_UP) {
        for (const KeyBinding& binding : KeyBindings) {
            if (event.key.scancode == binding.key) {
                setHeld(binding.action, false);
                break;
            }
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            press(InputAction::ThrowActiveRing);
            setHeld(InputAction::ThrowActiveRing, true);
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            press(InputAction::OffsetRingCenter);
            setHeld(InputAction::OffsetRingCenter, true);
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            setHeld(InputAction::ThrowActiveRing, false);
        } else if (event.button.button == SDL_BUTTON_RIGHT) {
            setHeld(InputAction::OffsetRingCenter, false);
        }
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        if (event.wheel.y > 0.0f) {
            press(InputAction::ShortcutCursorLeft);
        } else if (event.wheel.y < 0.0f) {
            press(InputAction::ShortcutCursorRight);
        }
    }
}

void Input::update(int, int)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    held_[actionIndex(InputAction::MoveLeft)] = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    held_[actionIndex(InputAction::MoveRight)] = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
    held_[actionIndex(InputAction::MoveUp)] = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
    held_[actionIndex(InputAction::MoveDown)] = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];

    moveAxis_ = {};
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

bool Input::held(InputAction action) const
{
    return held_[actionIndex(action)];
}

bool Input::upgradePressed(int option) const
{
    return option >= 0 && option < 3 && shortcutSlotPressed_ == option;
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

void Input::setHeld(InputAction action, bool held)
{
    held_[actionIndex(action)] = held;
}

}
