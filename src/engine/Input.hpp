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
    Pause,
    OpenInventory,
    ToggleDebug,
    ToggleDebugPause,
    Count
};

class Input {
public:
    void beginFrame();
    void handleEvent(const SDL_Event& event);
    void update(int windowWidth, int windowHeight);

    bool quitRequested() const { return quitRequested_; }
    bool pressed(InputAction action) const;
    bool held(InputAction action) const;
    bool debugPressed() const { return pressed(InputAction::ToggleDebug); }
    bool debugPausePressed() const { return pressed(InputAction::ToggleDebugPause); }
    bool throwPressed() const { return pressed(InputAction::ThrowActiveRing); }
    bool inventoryPressed() const { return pressed(InputAction::OpenInventory); }
    bool pausePressed() const { return pressed(InputAction::Pause); }
    bool useItemPressed() const { return pressed(InputAction::UseSelectedItem); }
    bool confirmPressed() const { return pressed(InputAction::Confirm); }
    bool addRingPressed() const { return pressed(InputAction::PutSelectedItemOnRing); }
    bool grabOrPlacePressed() const { return pressed(InputAction::GrabOrPlaceItem); }
    bool backPressed() const { return pressed(InputAction::Pause) || pressed(InputAction::OffsetRingCenter); }
    bool ringOffsetHeld() const { return held(InputAction::OffsetRingCenter); }
    bool upgradePressed(int option) const;
    bool mouseLeftPressed() const { return pressed(InputAction::ThrowActiveRing); }
    bool mouseLeftHeld() const { return held(InputAction::ThrowActiveRing); }
    int shortcutCursorDelta() const { return shortcutCursorDelta_; }
    int shortcutSlotPressed() const { return shortcutSlotPressed_; }
    int activeRingDelta() const { return activeRingDelta_; }
    bool toggleShortcutRowPressed() const { return pressed(InputAction::ToggleShortcutRow); }
    Vec2 moveAxis() const { return moveAxis_; }
    Vec2 mouseScreen() const { return mouseScreen_; }

private:
    static constexpr int ActionCount = static_cast<int>(InputAction::Count);

    void press(InputAction action);
    void setHeld(InputAction action, bool held);

    bool quitRequested_ = false;
    std::array<bool, ActionCount> pressed_{};
    std::array<bool, ActionCount> held_{};
    int shortcutCursorDelta_ = 0;
    int shortcutSlotPressed_ = -1;
    int activeRingDelta_ = 0;
    Vec2 moveAxis_{};
    Vec2 mouseScreen_{};
};

}
