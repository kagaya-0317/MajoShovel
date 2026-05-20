#pragma once

#include "engine/Audio.hpp"
#include "engine/FileWatcher.hpp"
#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Time.hpp"
#include "debug/DebugConsole.hpp"
#include "game/Game.hpp"
#include <SDL3/SDL.h>
#include <string>

namespace majo {

class App {
public:
    App() = default;
    ~App();

    bool initialize(const char* title, int width, int height, bool testPlayMode = false);
    void run();
    bool restartRequested() const { return restartRequested_; }

private:
    bool loadAssets();
    void configureAssetWatcher();
    void checkAssetHotReload();
    bool reloadAssetForPath(const std::string& changedPath);
    void toggleFullscreen();
    void executeDebugCommand(const std::string& command);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdlRenderer_ = nullptr;
    Renderer* renderer_ = nullptr;
    AudioEngine audio_;
    Input input_;
    Time time_;
    FileWatcher assetWatcher_;
    DebugConsole debugConsole_;
    Game game_;
    bool running_ = false;
    bool testPlayMode_ = false;
    bool restartRequested_ = false;
    int width_ = 1280;
    int height_ = 720;
};

}
