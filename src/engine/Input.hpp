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
    bool shiftOrbitHeld() const { return shiftOrbitHeld_; }
    bool upgradePressed(int option) const;
    Vec2 moveAxis() const { return moveAxis_; }
    Vec2 mouseScreen() const { return mouseScreen_; }

private:
    bool quitRequested_ = false;
    bool debugPressed_ = false;
    bool throwPressed_ = false;
    bool shiftOrbitHeld_ = false;
    bool upgradePressed_[3]{};
    Vec2 moveAxis_{};
    Vec2 mouseScreen_{};
};

}
