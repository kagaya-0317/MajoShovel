#pragma once

#include "engine/InputBinding.hpp"
#include "engine/Math.hpp"
#include <SDL3/SDL.h>
#include <array>

namespace majo {

class Renderer;

enum class InputDeviceKind {
    KeyboardMouse,
    Gamepad,
};

enum class InputModality {
    Keyboard,
    Mouse,
    Gamepad,
};

struct InputAutomationFrame {
    bool active = false;
    bool exclusive = true;
    Vec2 moveAxis{};
    Vec2 aimScreen{};
    bool throwPressed = false;
    bool ringOffsetHeld = false;
    bool confirmPressed = false;
    bool useItemPressed = false;
};

class Input {
public:
    Input() = default;
    ~Input();
    Input(const Input& other);
    Input& operator=(const Input& other);

    void shutdown();
    void beginFrame();
    void handleEvent(const SDL_Event& event);
    void handleConsumedEvent(const SDL_Event& event);
    void update(const Renderer* renderer);
    void applyAutomation(const InputAutomationFrame& frame);
    void setBindingMap(const InputBindingMap& bindings);
    const InputBindingMap& bindingMap() const { return bindings_; }

    bool quitRequested() const { return quitRequested_; }
    bool pressed(InputAction action) const;
    bool released(InputAction action) const;
    bool held(InputAction action) const;
    bool debugPressed() const { return pressed(InputAction::ToggleDebug); }
    bool debugPausePressed() const { return pressed(InputAction::ToggleDebugPause); }
    bool testRestartPressed() const { return pressed(InputAction::TestRestart); }
    bool testFreezePressed() const { return pressed(InputAction::ToggleTestFreeze); }
    bool openConsolePressed() const { return pressed(InputAction::OpenConsole); }
    bool toggleAutoReloadBlockPressed() const { return pressed(InputAction::ToggleAutoReloadBlock); }
    bool throwPressed() const { return pressed(InputAction::ThrowActiveRing); }
    bool inventoryPressed() const { return pressed(InputAction::OpenInventory); }
    bool pausePressed() const { return pressed(InputAction::Pause); }
    bool useItemPressed() const { return pressed(InputAction::UseSelectedItem); }
    bool discardItemPressed() const { return pressed(InputAction::DiscardSelectedItem); }
    bool confirmPressed() const { return pressed(InputAction::Confirm); }
    bool addRingPressed() const { return pressed(InputAction::PutSelectedItemOnRing); }
    bool grabOrPlacePressed() const { return pressed(InputAction::GrabOrPlaceItem); }
    bool arrangeItemsPressed() const { return pressed(InputAction::ArrangeItems); }
    bool removeAllRingItemsPressed() const;
    bool backPressed() const { return pressed(InputAction::Cancel) || pressed(InputAction::Pause); }
    bool backReleased() const { return released(InputAction::Cancel) || released(InputAction::Pause); }
    bool backHeld() const { return held(InputAction::Cancel) || held(InputAction::Pause); }
    bool ringOffsetHeld() const
    {
        return held(InputAction::OffsetRingCenter) || lengthSquared(ringShiftAxis_) > 0.0001f;
    }
    bool ringOffsetPointerHeld() const;
    bool mouseLeftPressed() const { return mouseLeftPressed_; }
    bool mouseLeftReleased() const { return mouseLeftReleased_; }
    bool mouseLeftHeld() const { return mouseLeftHeld_; }
    bool saveShortcutPressed() const { return ctrlSavePressed_; }
    bool undoShortcutPressed() const { return ctrlUndoPressed_; }
    bool redoShortcutPressed() const { return ctrlRedoPressed_; }
    bool copyShortcutPressed() const { return ctrlCopyPressed_; }
    bool pasteShortcutPressed() const { return ctrlPastePressed_; }
    int shortcutCursorDelta() const { return shortcutCursorDelta_; }
    int mouseWheelDelta() const { return mouseWheelDelta_; }
    int cycleDelta() const { return cycleDelta_; }
    Vec2 moveAxis() const { return moveAxis_; }
    Vec2 ringShiftAxis() const { return ringShiftAxis_; }
    Vec2 mouseScreen() const { return mouseScreen_; }
    InputDeviceKind lastActiveDevice() const { return lastActiveDevice_; }
    InputModality lastInputModality() const { return lastInputModality_; }
    bool uiNavigationCursorActive() const { return lastInputModality_ != InputModality::Mouse; }

private:
    static constexpr int ActionCount = InputActionCount;
    static constexpr int InputSourceCount = 3;

    enum class InputSource {
        Keyboard,
        Mouse,
        Gamepad,
    };

    bool openGamepad(SDL_JoystickID id);
    void openFirstGamepad();
    void closeGamepad();
    void clearSource(InputSource source);
    void addSourceHold(InputSource source, InputAction action);
    void removeSourceHold(InputSource source, InputAction action);
    void setSourceHeld(InputSource source, InputAction action, bool held);
    bool sourceHeld(InputSource source, InputAction action) const;
    bool anySourceHeld(InputAction action) const;
    Vec2 directionalAxisForSource(
        InputSource source,
        InputAction left,
        InputAction right,
        InputAction up,
        InputAction down) const;
    void handleGamepadButton(SDL_GamepadButton button, bool down);
    void updateGamepadState();
    void updateGamepadButtonBindings(std::array<bool, ActionCount>& gamepadHeld);
    void updateGamepadAxisBindings(int axis, float value, std::array<bool, ActionCount>& gamepadHeld);
    void updateKeyboardPolledBindings(const bool* keys);
    void updateConsumedPhysicalInputSuppression(const SDL_Event& event, bool consumed);
    void accumulateGamepadAnalogAxis(InputAction action, float amount);
    void press(InputAction action);
    void release(InputAction action);

    bool quitRequested_ = false;
    bool mouseLeftPressed_ = false;
    bool mouseLeftReleased_ = false;
    bool mouseLeftHeld_ = false;
    bool ctrlSavePressed_ = false;
    bool ctrlUndoPressed_ = false;
    bool ctrlRedoPressed_ = false;
    bool ctrlCopyPressed_ = false;
    bool ctrlPastePressed_ = false;
    InputDeviceKind lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
    InputModality lastInputModality_ = InputModality::Keyboard;
    SDL_Gamepad* gamepad_ = nullptr;
    SDL_JoystickID gamepadId_ = 0;
    std::array<bool, ActionCount> pressed_{};
    std::array<bool, ActionCount> released_{};
    std::array<bool, ActionCount> held_{};
    std::array<std::array<int, ActionCount>, InputSourceCount> sourceHoldCounts_{};
    std::array<bool, SDL_SCANCODE_COUNT> consumedKeyboardScancodes_{};
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> consumedGamepadButtons_{};
    std::array<bool, SDL_GAMEPAD_AXIS_COUNT> consumedGamepadAxes_{};
    InputBindingMap bindings_ = defaultInputBindings();
    int shortcutCursorDelta_ = 0;
    int mouseWheelDelta_ = 0;
    int cycleDelta_ = 0;
    Vec2 moveAxis_{};
    Vec2 leftStickAxis_{};
    Vec2 ringShiftAxis_{};
    Vec2 gamepadRingShiftAxis_{};
    Vec2 mouseScreen_{};
};

}
