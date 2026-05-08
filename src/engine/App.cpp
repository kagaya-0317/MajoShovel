#include "engine/App.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>

namespace majo {

namespace {
std::string lowerAscii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string filenameOf(const std::string& path)
{
    return lowerAscii(std::filesystem::path(path).filename().string());
}
}

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

bool App::initialize(const char* title, int width, int height, bool testPlayMode)
{
    width_ = width;
    height_ = height;
    testPlayMode_ = testPlayMode;
    restartRequested_ = false;
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
    loadAssets();
    configureAssetWatcher();
    game_.initialize(width_, height_);
    time_.reset();
    running_ = true;
    return true;
}

bool App::loadAssets()
{
    bool ok = true;
    if (!renderer_->loadIconSheet("assets/icon.png")) {
        std::fprintf(stderr, "%s\n", renderer_->lastAssetError().c_str());
        ok = false;
    }
    if (!renderer_->loadPlayerSheet("assets/majo.png")) {
        std::fprintf(stderr, "%s\n", renderer_->lastAssetError().c_str());
        ok = false;
    }
    if (!renderer_->loadUiWindowTexture("assets/UI_window1.png")) {
        std::fprintf(stderr, "%s\n", renderer_->lastAssetError().c_str());
        ok = false;
    }
    if (!renderer_->loadUiSubWindowTexture("assets/UI_window2.png")) {
        std::fprintf(stderr, "%s\n", renderer_->lastAssetError().c_str());
        ok = false;
    }
    if (!renderer_->loadUiButtonTexture("assets/UI_buttons.png")) {
        std::fprintf(stderr, "%s\n", renderer_->lastAssetError().c_str());
        ok = false;
    }
    if (!renderer_->loadTextFont("assets/fonts/craftmincho.otf")) {
        std::fprintf(stderr, "%s\n", renderer_->lastAssetError().c_str());
        ok = false;
    }
    return ok;
}

void App::configureAssetWatcher()
{
    assetWatcher_ = FileWatcher{};
    assetWatcher_.watchPath("assets");
    assetWatcher_.reset();
}

bool App::reloadAssetForPath(const std::string& changedPath)
{
    const std::string fileName = filenameOf(changedPath);
    const std::string extension = lowerAscii(std::filesystem::path(changedPath).extension().string());

    if (fileName == "icon.png") {
        return renderer_->loadIconSheet("assets/icon.png");
    }
    if (fileName == "majo.png") {
        return renderer_->loadPlayerSheet("assets/majo.png");
    }
    if (fileName == "ui_window1.png") {
        return renderer_->loadUiWindowTexture("assets/UI_window1.png");
    }
    if (fileName == "ui_window2.png") {
        return renderer_->loadUiSubWindowTexture("assets/UI_window2.png");
    }
    if (fileName == "ui_buttons.png") {
        return renderer_->loadUiButtonTexture("assets/UI_buttons.png");
    }
    if (extension == ".otf" || extension == ".ttf") {
        return renderer_->loadTextFont("assets/fonts/craftmincho.otf");
    }

    return loadAssets();
}

void App::checkAssetHotReload()
{
    std::string changedPath;
    if (!assetWatcher_.poll(changedPath)) {
        return;
    }

    if (reloadAssetForPath(changedPath)) {
        std::fprintf(stderr, "Asset hot reload: %s\n", changedPath.c_str());
    } else {
        std::fprintf(stderr, "Asset hot reload failed: %s\n%s\n", changedPath.c_str(), renderer_->lastAssetError().c_str());
    }
    configureAssetWatcher();
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
        if (testPlayMode_ && input_.testRestartPressed()) {
            restartRequested_ = true;
            running_ = false;
            continue;
        }

        checkAssetHotReload();
        time_.tick();
        game_.update(input_, time_);
        if (game_.quitRequested()) {
            running_ = false;
        }
        game_.render(*renderer_, time_);
    }
}

}
