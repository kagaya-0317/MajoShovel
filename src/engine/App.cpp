#include "engine/App.hpp"

#include "engine/Log.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

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

std::filesystem::path devSettingsRootPath()
{
#if defined(_WIN32)
    char* localAppData = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&localAppData, &length, "LOCALAPPDATA") == 0) {
        if (localAppData != nullptr && localAppData[0] != '\0') {
            const std::filesystem::path path = std::filesystem::path(localAppData) / "MajoShovel";
            std::free(localAppData);
            return path;
        }
        if (localAppData != nullptr) {
            std::free(localAppData);
        }
    }
#else
    const char* localAppData = std::getenv("LOCALAPPDATA");
    if (localAppData != nullptr && localAppData[0] != '\0') {
        return std::filesystem::path(localAppData) / "MajoShovel";
    }
#endif
    return std::filesystem::path(".local") / "MajoShovel";
}

std::filesystem::path devBuildConfigPath()
{
    return devSettingsRootPath() / "dev_build_config.txt";
}

std::filesystem::path devAutoReloadBlockPath()
{
    return devSettingsRootPath() / "dev_auto_reload_blocked.txt";
}

enum class DevLaunchMode {
    Base,
    Dungeon,
    EnemyTest,
};

std::filesystem::path devLaunchModePath()
{
    return devSettingsRootPath() / "dev_launch_mode.txt";
}

std::optional<DevLaunchMode> parseDevLaunchMode(std::string_view value)
{
    const std::string normalized = lowerAscii(trimAscii(std::string(value)));
    if (normalized == "base") {
        return DevLaunchMode::Base;
    }
    if (normalized == "dungeon") {
        return DevLaunchMode::Dungeon;
    }
    if (normalized == "enemy-test") {
        return DevLaunchMode::EnemyTest;
    }
    return std::nullopt;
}

std::optional<DevLaunchMode> parseDevLaunchModeCommand(const std::string& normalized)
{
    constexpr const char* Prefix = "game launch-mode ";
    constexpr std::size_t PrefixLength = std::char_traits<char>::length(Prefix);
    if (normalized.compare(0, PrefixLength, Prefix) != 0) {
        return std::nullopt;
    }
    return parseDevLaunchMode(std::string_view(normalized).substr(PrefixLength));
}

const char* devLaunchModeSaveName(DevLaunchMode mode)
{
    switch (mode) {
    case DevLaunchMode::Base: return "base";
    case DevLaunchMode::Dungeon: return "dungeon";
    case DevLaunchMode::EnemyTest: return "enemy-test";
    }
    return "base";
}

const char* devLaunchModeLogName(DevLaunchMode mode)
{
    switch (mode) {
    case DevLaunchMode::Base: return "base";
    case DevLaunchMode::Dungeon: return "dungeon";
    case DevLaunchMode::EnemyTest: return "enemy test";
    }
    return "base";
}

int devLaunchModeDropdownIndex(DevLaunchMode mode)
{
    switch (mode) {
    case DevLaunchMode::Base: return 0;
    case DevLaunchMode::Dungeon: return 1;
    case DevLaunchMode::EnemyTest: return 2;
    }
    return 0;
}

std::string devLaunchModeCommand(DevLaunchMode mode)
{
    return std::string("game launch-mode ") + devLaunchModeSaveName(mode);
}

DevLaunchMode loadDevLaunchMode()
{
    const std::filesystem::path path = devLaunchModePath();
    std::ifstream file(path);
    if (!file) {
        return DevLaunchMode::Base;
    }

    std::string value;
    std::getline(file, value);
    if (std::optional<DevLaunchMode> mode = parseDevLaunchMode(value)) {
        return *mode;
    }
    return DevLaunchMode::Base;
}

bool saveDevLaunchMode(DevLaunchMode mode, std::string& outError)
{
    const std::filesystem::path path = devLaunchModePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        outError = "Failed to create config directory: " + ec.message();
        return false;
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        outError = "Failed to open launch mode file: " + path.string();
        return false;
    }

    file << devLaunchModeSaveName(mode) << "\n";
    if (!file) {
        outError = "Failed to write launch mode file: " + path.string();
        return false;
    }

    return true;
}

bool saveDevBuildConfig(std::string_view configName, std::string& outError)
{
    const std::filesystem::path configPath = devBuildConfigPath();
    std::error_code ec;
    std::filesystem::create_directories(configPath.parent_path(), ec);
    if (ec) {
        outError = "Failed to create config directory: " + ec.message();
        return false;
    }

    std::ofstream file(configPath, std::ios::trunc);
    if (!file) {
        outError = "Failed to open config file: " + configPath.string();
        return false;
    }
    file << configName << "\n";
    if (!file) {
        outError = "Failed to write config file: " + configPath.string();
        return false;
    }

    return true;
}

bool loadDevAutoReloadBlocked()
{
    const std::filesystem::path path = devAutoReloadBlockPath();
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    std::string value;
    std::getline(file, value);
    const std::string normalized = lowerAscii(trimAscii(value));
    return normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes";
}

bool saveDevAutoReloadBlocked(bool blocked, std::string& outError)
{
    const std::filesystem::path path = devAutoReloadBlockPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        outError = "Failed to create config directory: " + ec.message();
        return false;
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file) {
        outError = "Failed to open auto-reload flag file: " + path.string();
        return false;
    }

    file << (blocked ? "1" : "0") << "\n";
    if (!file) {
        outError = "Failed to write auto-reload flag file: " + path.string();
        return false;
    }

    return true;
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
    DevLaunchMode launchMode = DevLaunchMode::Base;
    if (testPlayMode_) {
        debugConsole_.initialize();
        setLogSink([this](LogLevel level, std::string_view message) {
            debugConsole_.appendLog(level, message);
        });
        const bool autoReloadBlocked = loadDevAutoReloadBlocked();
        game_.setAutoReloadBlocked(autoReloadBlocked);
        launchMode = loadDevLaunchMode();
        debugConsole_.setDropdownSelection("launch_mode", devLaunchModeDropdownIndex(launchMode));
        logInfo(std::string("Auto reload block: ") + (autoReloadBlocked ? "ON" : "OFF"));
        logInfo(std::string("Launch mode: ") + devLaunchModeLogName(launchMode));
        logInfo("Test-play debug console enabled. Press F8 to show or hide it.");
    }
    loadAssets();
    configureAssetWatcher();
    game_.initialize(width_, height_);
    if (testPlayMode_ && launchMode != DevLaunchMode::Base) {
        game_.executeDebugCommand(devLaunchModeCommand(launchMode));
    }
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
    if (!renderer_->loadBaseMapTexture("assets/map_kyoten.png")) {
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
    if (!renderer_->loadUiLineTexture("assets/UI_line.png")) {
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
    const std::string parentPath = lowerAscii(std::filesystem::path(changedPath).parent_path().generic_string());

    if (fileName == "icon.png") {
        return renderer_->loadIconSheet("assets/icon.png");
    }
    if (fileName == "majo.png") {
        return renderer_->loadPlayerSheet("assets/majo.png");
    }
    if (fileName == "map_kyoten.png") {
        return renderer_->loadBaseMapTexture("assets/map_kyoten.png");
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
    if (fileName == "ui_line.png") {
        return renderer_->loadUiLineTexture("assets/UI_line.png");
    }
    if (extension == ".otf" || extension == ".ttf") {
        return renderer_->loadTextFont("assets/fonts/craftmincho.otf");
    }
    if (extension == ".png" &&
        fileName.rfind("obj_", 0) == 0 &&
        parentPath.find("assets/objects") != std::string::npos) {
        renderer_->invalidateImage(changedPath);
        return true;
    }
    if (extension == ".png" &&
        fileName.rfind("img_", 0) == 0 &&
        parentPath.find("assets/others") != std::string::npos) {
        renderer_->invalidateImage(changedPath);
        return true;
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
    if (std::optional<DevLaunchMode> launchMode = parseDevLaunchModeCommand(normalized)) {
        std::string error;
        if (saveDevLaunchMode(*launchMode, error)) {
            debugConsole_.setDropdownSelection("launch_mode", devLaunchModeDropdownIndex(*launchMode));
            logInfo(std::string("Launch mode saved: ") + devLaunchModeLogName(*launchMode));
        } else {
            logError("Launch mode save failed: " + error);
        }
        if (game_.executeDebugCommand(command)) {
            return;
        }
        logWarning("Unknown debug command: " + command);
        return;
    }
    if (normalized == "dev build-config debug" || normalized == "dev build debug") {
        std::string error;
        if (saveDevBuildConfig("Debug", error)) {
            logInfo("Dev build config saved: Debug (applies on next dev_auto_reload start).");
        } else {
            logError("Dev build config save failed: " + error);
        }
        return;
    }
    if (normalized == "dev build-config release" || normalized == "dev build release") {
        std::string error;
        if (saveDevBuildConfig("Release", error)) {
            logInfo("Dev build config saved: Release (applies on next dev_auto_reload start).");
        } else {
            logError("Dev build config save failed: " + error);
        }
        return;
    }
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
        if (testPlayMode_ && input_.toggleAutoReloadBlockPressed()) {
            const bool blocked = !loadDevAutoReloadBlocked();
            std::string error;
            if (saveDevAutoReloadBlocked(blocked, error)) {
                game_.setAutoReloadBlocked(blocked);
                logInfo(std::string("Auto reload block: ") + (blocked ? "ON (F2)" : "OFF (F2)"));
            } else {
                logError("Auto reload block toggle failed: " + error);
            }
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
