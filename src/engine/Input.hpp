#pragma once

#include "engine/Math.hpp"
#include <SDL3/SDL.h>
#include <array>

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

enum class InputDeviceKind {
    KeyboardMouse,
    Gamepad,
};

class Input {
public:
    ~Input();

    void beginFrame();
    void handleEvent(const SDL_Event& event);
    void update(int windowWidth, int windowHeight);

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
    bool confirmPressed() const { return pressed(InputAction::Confirm); }
    bool addRingPressed() const { return pressed(InputAction::PutSelectedItemOnRing); }
    bool grabOrPlacePressed() const { return pressed(InputAction::GrabOrPlaceItem); }
    bool capturePressed() const { return pressed(InputAction::CaptureNet); }
    bool backPressed() const { return pressed(InputAction::Cancel) || pressed(InputAction::Pause); }
    bool backReleased() const { return released(InputAction::Cancel) || released(InputAction::Pause); }
    bool backHeld() const { return held(InputAction::Cancel) || held(InputAction::Pause); }
    bool ringOffsetHeld() const { return held(InputAction::OffsetRingCenter); }
    bool upgradePressed(int option) const;
    bool mouseLeftPressed() const { return mouseLeftPressed_; }
    bool mouseLeftReleased() const { return mouseLeftReleased_; }
    bool mouseLeftHeld() const { return mouseLeftHeld_; }
    bool saveShortcutPressed() const { return ctrlSavePressed_; }
    bool undoShortcutPressed() const { return ctrlUndoPressed_; }
    bool redoShortcutPressed() const { return ctrlRedoPressed_; }
    int shortcutCursorDelta() const { return shortcutCursorDelta_; }
    int mouseWheelDelta() const { return mouseWheelDelta_; }
    int shortcutSlotPressed() const { return shortcutSlotPressed_; }
    int activeRingDelta() const { return activeRingDelta_; }
    bool toggleShortcutRowPressed() const { return pressed(InputAction::ToggleShortcutRow); }
    Vec2 moveAxis() const { return moveAxis_; }
    Vec2 aimAxis() const { return aimAxis_; }
    bool hasAimAxis() const { return hasAimAxis_; }
    Vec2 mouseScreen() const { return mouseScreen_; }
    InputDeviceKind lastActiveDevice() const { return lastActiveDevice_; }

private:
    static constexpr int ActionCount = static_cast<int>(InputAction::Count);
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
    void handleGamepadButton(SDL_GamepadButton button, bool down);
    void updateGamepadState();
    void updateGamepadAxisAction(InputAction negativeAction, InputAction positiveAction, float value);
    void updateGamepadTriggerAction(InputAction action, float value);
    void press(InputAction action);
    void release(InputAction action);

    bool quitRequested_ = false;
    bool mouseLeftPressed_ = false;
    bool mouseLeftReleased_ = false;
    bool mouseLeftHeld_ = false;
    bool ctrlSavePressed_ = false;
    bool ctrlUndoPressed_ = false;
    bool ctrlRedoPressed_ = false;
    InputDeviceKind lastActiveDevice_ = InputDeviceKind::KeyboardMouse;
    SDL_Gamepad* gamepad_ = nullptr;
    SDL_JoystickID gamepadId_ = 0;
    std::array<bool, ActionCount> pressed_{};
    std::array<bool, ActionCount> released_{};
    std::array<bool, ActionCount> held_{};
    std::array<std::array<int, ActionCount>, InputSourceCount> sourceHoldCounts_{};
    int shortcutCursorDelta_ = 0;
    int mouseWheelDelta_ = 0;
    int shortcutSlotPressed_ = -1;
    int activeRingDelta_ = 0;
    Vec2 moveAxis_{};
    Vec2 leftStickAxis_{};
    Vec2 aimAxis_{};
    bool hasAimAxis_ = false;
    Vec2 mouseScreen_{};
};

}
