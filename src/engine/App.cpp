#include "engine/App.hpp"

#include "engine/Log.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
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

std::string trimAscii(std::string text)
{
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), isSpace));
    text.erase(std::find_if_not(text.rbegin(), text.rend(), isSpace).base(), text.end());
    return text;
}
}

App::~App()
{
    setLogSink({});
    debugConsole_.shutdown();
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
        logError(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }
    if (!SDL_CreateWindowAndRenderer(title, width_, height_, SDL_WINDOW_RESIZABLE, &window_, &sdlRenderer_)) {
        logError(std::string("SDL_CreateWindowAndRenderer failed: ") + SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(sdlRenderer_, 1);
    renderer_ = new Renderer(sdlRenderer_);
    if (testPlayMode_) {
        debugConsole_.initialize();
        setLogSink([this](LogLevel level, std::string_view message) {
            debugConsole_.appendLog(level, message);
        });
        logInfo("Test-play debug console enabled. Press F8 to show or hide it.");
    }
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
        logError(renderer_->lastAssetError());
        ok = false;
    }
    if (!renderer_->loadPlayerSheet("assets/majo.png")) {
        logError(renderer_->lastAssetError());
        ok = false;
    }
    if (!renderer_->loadUiWindowTexture("assets/UI_window1.png")) {
        logError(renderer_->lastAssetError());
        ok = false;
    }
    if (!renderer_->loadUiSubWindowTexture("assets/UI_window2.png")) {
        logError(renderer_->lastAssetError());
        ok = false;
    }
    if (!renderer_->loadUiButtonTexture("assets/UI_buttons.png")) {
        logError(renderer_->lastAssetError());
        ok = false;
    }
    if (!renderer_->loadTextFont("assets/fonts/craftmincho.otf")) {
        logError(renderer_->lastAssetError());
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
        logInfo("Asset hot reload: " + changedPath);
    } else {
        logError("Asset hot reload failed: " + changedPath + "\n" + renderer_->lastAssetError());
    }
    configureAssetWatcher();
}

void App::toggleFullscreen()
{
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
    const bool fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    if (!SDL_SetWindowFullscreen(window_, !fullscreen)) {
        logError(std::string("SDL_SetWindowFullscreen failed: ") + SDL_GetError());
    }
}

void App::executeDebugCommand(const std::string& command)
{
    const std::string normalized = lowerAscii(trimAscii(command));
    if (normalized == "restart") {
        logInfo("Debug command: restart");
        restartRequested_ = true;
        running_ = false;
        return;
    }
    if (normalized == "quit" || normalized == "exit") {
        logInfo("Debug command: quit");
        running_ = false;
        return;
    }
    if (game_.executeDebugCommand(command)) {
        return;
    }

    logWarning("Unknown debug command: " + command);
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
        if (testPlayMode_ && input_.openConsolePressed()) {
            debugConsole_.toggleVisible();
        }
        if (testPlayMode_) {
            while (std::optional<std::string> command = debugConsole_.pollCommand()) {
                executeDebugCommand(*command);
            }
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
