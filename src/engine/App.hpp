#pragma once

#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Time.hpp"
#include "game/Game.hpp"
#include <SDL3/SDL.h>

namespace majo {

class App {
public:
    App() = default;
    ~App();

    bool initialize(const char* title, int width, int height);
    void run();

private:
    void toggleFullscreen();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdlRenderer_ = nullptr;
    Renderer* renderer_ = nullptr;
    Input input_;
    Time time_;
    Game game_;
    bool running_ = false;
    int width_ = 1280;
    int height_ = 720;
};

}
