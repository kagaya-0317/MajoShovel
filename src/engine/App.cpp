#include "engine/App.hpp"

#include <SDL3/SDL.h>
#include <cstdio>

namespace majo {

App::~App()
{
    delete renderer_;
    if (sdlRenderer_) {
        SDL_DestroyRenderer(sdlRenderer_);
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

bool App::initialize(const char* title, int width, int height)
{
    width_ = width;
    height_ = height;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    if (!SDL_CreateWindowAndRenderer(title, width_, height_, SDL_WINDOW_RESIZABLE, &window_, &sdlRenderer_)) {
        std::fprintf(stderr, "SDL_CreateWindowAndRenderer failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(sdlRenderer_, 1);
    renderer_ = new Renderer(sdlRenderer_);
    if (!renderer_->loadIconSheet("assets/icon.png")) {
        std::fprintf(stderr, "%s\n", renderer_->lastAssetError().c_str());
    }
    if (!renderer_->loadTextFont("assets/fonts/craftmincho.otf")) {
        std::fprintf(stderr, "%s\n", renderer_->lastAssetError().c_str());
    }
    game_.initialize(width_, height_);
    time_.reset();
    running_ = true;
    return true;
}

void App::toggleFullscreen()
{
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
    const bool fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    if (!SDL_SetWindowFullscreen(window_, !fullscreen)) {
        std::fprintf(stderr, "SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
    }
}

void App::run()
{
    while (running_) {
        input_.beginFrame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input_.handleEvent(event);
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_F4) {
                toggleFullscreen();
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                width_ = event.window.data1;
                height_ = event.window.data2;
                game_.resize(width_, height_);
            }
        }
        input_.update(width_, height_);
        if (input_.quitRequested()) {
            running_ = false;
        }

        time_.tick();
        game_.update(input_, time_);
        if (game_.quitRequested()) {
            running_ = false;
        }
        game_.render(*renderer_, time_);
    }
}

}
