#pragma once

#include "engine/Math.hpp"
#include <SDL3/SDL.h>

namespace majo {

class Input {
public:
    void beginFrame();
    void handleEvent(const SDL_Event& event);
    void update(int windowWidth, int windowHeight);

    bool quitRequested() const { return quitRequested_; }
    bool debugPressed() const { return debugPressed_; }
    bool throwPressed() const { return throwPressed_; }
    bool inventoryPressed() const { return inventoryPressed_; }
    bool menuUpPressed() const { return menuUpPressed_; }
    bool menuDownPressed() const { return menuDownPressed_; }
    bool useItemPressed() const { return useItemPressed_; }
    bool addRingPressed() const { return addRingPressed_; }
    bool shiftRingHeld() const { return shiftRingHeld_; }
    bool upgradePressed(int option) const;
    Vec2 moveAxis() const { return moveAxis_; }
    Vec2 mouseScreen() const { return mouseScreen_; }

private:
    bool quitRequested_ = false;
    bool debugPressed_ = false;
    bool throwPressed_ = false;
    bool inventoryPressed_ = false;
    bool menuUpPressed_ = false;
    bool menuDownPressed_ = false;
    bool useItemPressed_ = false;
    bool addRingPressed_ = false;
    bool shiftRingHeld_ = false;
    bool upgradePressed_[3]{};
    Vec2 moveAxis_{};
    Vec2 mouseScreen_{};
};

}
