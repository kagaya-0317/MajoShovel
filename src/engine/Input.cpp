#include "engine/Input.hpp"

namespace majo {

void Input::beginFrame()
{
    debugPressed_ = false;
    throwPressed_ = false;
    inventoryPressed_ = false;
    menuUpPressed_ = false;
    menuDownPressed_ = false;
    useItemPressed_ = false;
    addRingPressed_ = false;
    for (bool& pressed : upgradePressed_) {
        pressed = false;
    }
}

void Input::handleEvent(const SDL_Event& event)
{
    if (event.type == SDL_EVENT_QUIT) {
        quitRequested_ = true;
    }
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        switch (event.key.scancode) {
        case SDL_SCANCODE_ESCAPE: quitRequested_ = true; break;
        case SDL_SCANCODE_F1: debugPressed_ = true; break;
        case SDL_SCANCODE_J: throwPressed_ = true; break;
        case SDL_SCANCODE_I:
        case SDL_SCANCODE_TAB: inventoryPressed_ = true; break;
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_W: menuUpPressed_ = true; break;
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_S: menuDownPressed_ = true; break;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER: useItemPressed_ = true; break;
        case SDL_SCANCODE_E: addRingPressed_ = true; break;
        case SDL_SCANCODE_1: upgradePressed_[0] = true; break;
        case SDL_SCANCODE_2: upgradePressed_[1] = true; break;
        case SDL_SCANCODE_3: upgradePressed_[2] = true; break;
        default: break;
        }
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
        throwPressed_ = true;
    }
}

void Input::update(int, int)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    moveAxis_ = {};
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) moveAxis_.x -= 1.0f;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) moveAxis_.x += 1.0f;
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) moveAxis_.y -= 1.0f;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) moveAxis_.y += 1.0f;
    if (lengthSquared(moveAxis_) > 1.0f) {
        moveAxis_ = normalize(moveAxis_);
    }
    shiftRingHeld_ = keys[SDL_SCANCODE_SPACE];

    float mx = 0.0f;
    float my = 0.0f;
    SDL_GetMouseState(&mx, &my);
    mouseScreen_ = {mx, my};
}

bool Input::upgradePressed(int option) const
{
    return option >= 0 && option < 3 && upgradePressed_[option];
}

}
