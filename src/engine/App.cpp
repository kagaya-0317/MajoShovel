#include "engine/App.hpp"

#include "data/GameBalance.hpp"
#include "engine/InputHelpGlyph.hpp"
#include "engine/Log.hpp"
#include "engine/Ui.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace majo {

namespace {

constexpr float AutoSimulationFixedStepSeconds = 1.0f / 60.0f;
constexpr float AutoSimulationMaxDebtSeconds = 0.75f;
constexpr int AutoSimulationMaxStepsPerFrame = 16;
constexpr int LogicalScreenWidth = balance::ScreenWidth;
constexpr int LogicalScreenHeight = balance::ScreenHeight;

InputHelpDeviceMode inputHelpDeviceModeForSetting(InputIconSetting setting)
{
    switch (setting) {
    case InputIconSetting::Auto:
        return InputHelpDeviceMode::Auto;
    case InputIconSetting::KeyboardMouse:
        return InputHelpDeviceMode::KeyboardMouse;
    case InputIconSetting::Gamepad:
        return InputHelpDeviceMode::Gamepad;
    }
    return InputHelpDeviceMode::Auto;
}

float screenBrightnessMultiplier(float brightness)
{
    return std::clamp(brightness, MinScreenBrightness, MaxScreenBrightness);
}

std::string screenBrightnessLogValue(float brightness)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << screenBrightnessMultiplier(brightness);
    return out.str();
}

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

std::string pathToUtf8(const std::filesystem::path& path)
{
#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L
    const auto encoded = path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
#else
    return path.generic_u8string();
#endif
}

std::string screenshotTimestamp()
{
    using namespace std::chrono;
    const auto now = system_clock::now();
    const auto millis = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t time = system_clock::to_time_t(now);

    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream out;
    out << std::put_time(&localTime, "%Y%m%d_%H%M%S")
        << '_' << std::setw(3) << std::setfill('0') << millis.count();
    return out.str();
}

std::string threeDigitSuffix(int value)
{
    std::ostringstream out;
    out << std::setw(3) << std::setfill('0') << value;
    return out.str();
}

std::string trimAscii(std::string text)
{
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), isSpace));
    text.erase(std::find_if_not(text.rbegin(), text.rend(), isSpace).base(), text.end());
    return text;
}

std::vector<std::string> splitAsciiWords(std::string_view text)
{
    std::vector<std::string> words;
    std::size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
            ++pos;
        }
        const std::size_t start = pos;
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) == 0) {
            ++pos;
        }
        if (start < pos) {
            words.emplace_back(text.substr(start, pos - start));
        }
    }
    return words;
}

std::optional<float> parseFloatSetting(std::string_view text)
{
    try {
        std::size_t consumed = 0;
        const float value = std::stof(std::string(text), &consumed);
        if (consumed == text.size()) {
            return value;
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

std::optional<int> parseIntSetting(std::string_view text)
{
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(std::string(text), &consumed);
        if (consumed == text.size()) {
            return value;
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

std::optional<bool> parseBoolSetting(std::string_view text)
{
    const std::string normalized = lowerAscii(std::string(text));
    if (normalized == "on" || normalized == "true" || normalized == "1") {
        return true;
    }
    if (normalized == "off" || normalized == "false" || normalized == "0") {
        return false;
    }
    return std::nullopt;
}

std::string inputBindingSummary(const std::vector<InputBinding>& bindings)
{
    if (bindings.empty()) {
        return "(none)";
    }
    std::string result;
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += inputBindingDisplayName(bindings[i]);
    }
    return result;
}

bool inputBindingMapsEqual(const InputBindingMap& lhs, const InputBindingMap& rhs)
{
    for (int action = 0; action < InputActionCount; ++action) {
        const auto& left = lhs[action];
        const auto& right = rhs[action];
        if (left.size() != right.size()) {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i) {
            if (!inputBindingEquals(left[i], right[i])) {
                return false;
            }
        }
    }
    return true;
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

constexpr std::uint64_t HotReloadPollIntervalMs = 500;

enum class DevLaunchMode {
    PreTitle,
    Base,
    Dungeon,
    EnemyTest,
    FinalBossBefore,
    FinalBossAfter,
    EndingKamishibai,
    PostEndingBase,
};

std::filesystem::path devLaunchModePath()
{
    return devSettingsRootPath() / "dev_launch_mode.txt";
}

std::optional<DevLaunchMode> parseDevLaunchMode(std::string_view value)
{
    const std::string normalized = lowerAscii(trimAscii(std::string(value)));
    if (normalized == "pre-title" || normalized == "before-title" || normalized == "opening") {
        return DevLaunchMode::PreTitle;
    }
    if (normalized == "base") {
        return DevLaunchMode::Base;
    }
    if (normalized == "dungeon") {
        return DevLaunchMode::Dungeon;
    }
    if (normalized == "enemy-test") {
        return DevLaunchMode::EnemyTest;
    }
    if (normalized == "final-boss-before" || normalized == "final-boss") {
        return DevLaunchMode::FinalBossBefore;
    }
    if (normalized == "final-boss-after") {
        return DevLaunchMode::FinalBossAfter;
    }
    if (normalized == "ending-kamishibai" || normalized == "ending-paper") {
        return DevLaunchMode::EndingKamishibai;
    }
    if (normalized == "post-ending-base" || normalized == "ending-base") {
        return DevLaunchMode::PostEndingBase;
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
    case DevLaunchMode::PreTitle: return "pre-title";
    case DevLaunchMode::Base: return "base";
    case DevLaunchMode::Dungeon: return "dungeon";
    case DevLaunchMode::EnemyTest: return "enemy-test";
    case DevLaunchMode::FinalBossBefore: return "final-boss-before";
    case DevLaunchMode::FinalBossAfter: return "final-boss-after";
    case DevLaunchMode::EndingKamishibai: return "ending-kamishibai";
    case DevLaunchMode::PostEndingBase: return "post-ending-base";
    }
    return "pre-title";
}

const char* devLaunchModeLogName(DevLaunchMode mode)
{
    switch (mode) {
    case DevLaunchMode::PreTitle: return "pre-title";
    case DevLaunchMode::Base: return "base";
    case DevLaunchMode::Dungeon: return "dungeon";
    case DevLaunchMode::EnemyTest: return "enemy test";
    case DevLaunchMode::FinalBossBefore: return "final boss before";
    case DevLaunchMode::FinalBossAfter: return "final boss after";
    case DevLaunchMode::EndingKamishibai: return "ending kamishibai";
    case DevLaunchMode::PostEndingBase: return "post-ending base";
    }
    return "pre-title";
}

int devLaunchModeDropdownIndex(DevLaunchMode mode)
{
    switch (mode) {
    case DevLaunchMode::PreTitle: return 0;
    case DevLaunchMode::Base: return 1;
    case DevLaunchMode::Dungeon: return 2;
    case DevLaunchMode::EnemyTest: return 3;
    case DevLaunchMode::FinalBossBefore: return 4;
    case DevLaunchMode::FinalBossAfter: return 5;
    case DevLaunchMode::EndingKamishibai: return 6;
    case DevLaunchMode::PostEndingBase: return 7;
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
        return DevLaunchMode::PreTitle;
    }

    std::string value;
    std::getline(file, value);
    if (std::optional<DevLaunchMode> mode = parseDevLaunchMode(value)) {
        return *mode;
    }
    return DevLaunchMode::PreTitle;
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
    if (settingsSavePending_) {
        saveSettingsNow();
    }
    setLogSink({});
    audio_.shutdown();
    debugConsole_.shutdown();
    input_.shutdown();
    delete renderer_;
    if (sdlRenderer_) {
        SDL_DestroyRenderer(sdlRenderer_);
    }
    if (window_) {
        SDL_DestroyWindow(window_);
    }
    SDL_Quit();
}

void App::applyAudioSettings()
{
    settings_ = sanitizeSettings(settings_);
    audio_.setMasterVolume(settings_.audio.masterVolume);
    audio_.setBgmVolume(settings_.audio.bgmVolume);
    audio_.setSeVolume(settings_.audio.seVolume);
}

void App::applyVideoSettings(bool notifyGameResize)
{
    settings_ = sanitizeSettings(settings_);

    if (sdlRenderer_ != nullptr) {
        if (!SDL_SetRenderVSync(sdlRenderer_, settings_.video.vsync ? 1 : 0)) {
            logWarning(std::string("SDL_SetRenderVSync failed: ") + SDL_GetError());
        }
    }

    if (window_ == nullptr) {
        width_ = settings_.video.windowWidth;
        height_ = settings_.video.windowHeight;
        return;
    }

    const int previousWidth = width_;
    const int previousHeight = height_;

    if (settings_.video.windowMode == WindowMode::BorderlessFullscreen) {
        if (!SDL_SetWindowFullscreenMode(window_, nullptr)) {
            logWarning(std::string("SDL_SetWindowFullscreenMode failed: ") + SDL_GetError());
        }
        if (!SDL_SetWindowFullscreen(window_, true)) {
            logWarning(std::string("SDL_SetWindowFullscreen failed: ") + SDL_GetError());
        }
    } else {
        if (!SDL_SetWindowFullscreen(window_, false)) {
            logWarning(std::string("SDL_SetWindowFullscreen failed: ") + SDL_GetError());
        }
        SDL_SyncWindow(window_);
        if (!SDL_SetWindowBordered(window_, true)) {
            logWarning(std::string("SDL_SetWindowBordered failed: ") + SDL_GetError());
        }
        if (!SDL_SetWindowSize(window_, settings_.video.windowWidth, settings_.video.windowHeight)) {
            logWarning(std::string("SDL_SetWindowSize failed: ") + SDL_GetError());
        }
        if (!SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED)) {
            logWarning(std::string("SDL_SetWindowPosition failed: ") + SDL_GetError());
        }
    }

    SDL_SyncWindow(window_);
    int actualWidth = width_;
    int actualHeight = height_;
    if (SDL_GetWindowSize(window_, &actualWidth, &actualHeight)) {
        width_ = actualWidth;
        height_ = actualHeight;
        if (settings_.video.windowMode == WindowMode::Windowed) {
            settings_.video.windowWidth = actualWidth;
            settings_.video.windowHeight = actualHeight;
        }
    }

    if (notifyGameResize && (width_ != previousWidth || height_ != previousHeight)) {
        game_.resize(LogicalScreenWidth, LogicalScreenHeight);
    }
}

void App::queueSettingsSave()
{
    settingsSavePending_ = true;
    settingsSaveDelaySeconds_ = 0.5f;
}

void App::updateSettingsSave(float dt)
{
    if (!settingsSavePending_) {
        return;
    }
    settingsSaveDelaySeconds_ -= dt;
    if (settingsSaveDelaySeconds_ <= 0.0f) {
        saveSettingsNow();
    }
}

bool App::saveSettingsNow()
{
    settings_ = sanitizeSettings(settings_);
    std::string error;
    if (!settingsStore_.save(settings_, &error)) {
        logError("Settings save failed: " + error);
        return false;
    }
    settingsSavePending_ = false;
    settingsSaveDelaySeconds_ = 0.0f;
    return true;
}

bool App::initialize(const char* title, int width, int height, bool testPlayMode)
{
    width_ = width;
    height_ = height;
    testPlayMode_ = testPlayMode;
    autoReloadBlocked_ = false;
    runtimeHotReloadEnabled_ = false;
    testFreezePaused_ = false;
    restartRequested_ = false;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        logError(std::string("SDL_Init failed: ") + SDL_GetError());
        return false;
    }
    const bool hadSettingsFile = settingsStore_.exists();
    std::string settingsError;
    if (!settingsStore_.load(settings_, &settingsError)) {
        logWarning("Settings load failed; using defaults: " + settingsError);
        settings_ = GameSettings{};
    }
    settings_ = sanitizeSettings(settings_);
    input_.setBindingMap(settings_.input.bindings);
    width_ = settings_.video.windowWidth;
    height_ = settings_.video.windowHeight;
    if (!hadSettingsFile) {
        saveSettingsNow();
    }
    if (!SDL_CreateWindowAndRenderer(title, width_, height_, SDL_WINDOW_RESIZABLE, &window_, &sdlRenderer_)) {
        logError(std::string("SDL_CreateWindowAndRenderer failed: ") + SDL_GetError());
        return false;
    }
    applyVideoSettings(false);
    renderer_ = new Renderer(sdlRenderer_);
    renderer_->setScreenBrightness(screenBrightnessMultiplier(settings_.presentation.brightness));
    if (!renderer_->setLogicalPresentation(LogicalScreenWidth, LogicalScreenHeight)) {
        logWarning(std::string("SDL_SetRenderLogicalPresentation failed: ") + SDL_GetError());
    }
    DevLaunchMode launchMode = DevLaunchMode::PreTitle;
    if (testPlayMode_) {
        debugConsole_.initialize();
        setLogSink([this](LogLevel level, std::string_view message) {
            debugConsole_.appendLog(level, message);
        });
        autoReloadBlocked_ = loadDevAutoReloadBlocked();
        game_.setAutoReloadBlocked(autoReloadBlocked_);
        launchMode = loadDevLaunchMode();
        debugConsole_.setDropdownSelection("launch_mode", devLaunchModeDropdownIndex(launchMode));
        logInfo(std::string("Auto reload block: ") + (autoReloadBlocked_ ? "ON" : "OFF"));
        logInfo(std::string("Launch mode: ") + devLaunchModeLogName(launchMode));
        logInfo("Test-play debug console enabled. Press F8 to show or hide it.");
    }
    startupLaunchModeCommand_ =
        testPlayMode_ && launchMode != DevLaunchMode::PreTitle
            ? devLaunchModeCommand(launchMode)
            : std::string{};
    startupStatus_ = "Starting";
    startupLoadStep_ = StartupLoadStep::FirstFrame;
    startupLoadActive_ = true;
    time_.reset();
    running_ = true;
    return true;
}

bool App::loadAssets()
{
    bool ok = true;
    if (!renderer_->loadPlayerSheet("assets/majo.png")) {
        logError(renderer_->lastAssetError());
        ok = false;
    }
    if (!renderer_->loadBaseMapTexture("assets/kyoten/map.png")) {
        logError(renderer_->lastAssetError());
        ok = false;
    }
    if (!renderer_->loadUiWindowTexture("assets/UI_window1.png")) {
        logError(renderer_->lastAssetError());
        ok = false;
    }
    if (!renderer_->loadUiMessageWindowTexture("assets/UI_messageWindow.png")) {
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
    if (!renderer_->loadUiTabTexture("assets/UI_tubs.png")) {
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

void App::setRuntimeHotReloadEnabled(bool enabled)
{
    if (runtimeHotReloadEnabled_ == enabled) {
        return;
    }

    runtimeHotReloadEnabled_ = enabled;
    nextAssetHotReloadPollTicks_ = 0;
    game_.setHotReloadEnabled(enabled);
    if (runtimeHotReloadEnabled_) {
        configureAssetWatcher();
    } else {
        assetWatcher_ = FileWatcher{};
    }
}

bool App::reloadAssetForPath(const std::string& changedPath)
{
    const std::string fileName = filenameOf(changedPath);
    const std::string extension = lowerAscii(std::filesystem::path(changedPath).extension().string());
    const std::string parentPath = lowerAscii(std::filesystem::path(changedPath).parent_path().generic_string());

    if (fileName == "majo.png") {
        return renderer_->loadPlayerSheet("assets/majo.png");
    }
    if (fileName == "map.png" && parentPath.find("assets/kyoten") != std::string::npos) {
        return renderer_->loadBaseMapTexture("assets/kyoten/map.png");
    }
    if (extension == ".png" && parentPath.find("assets/kyoten") != std::string::npos) {
        renderer_->invalidateImage(changedPath);
        return true;
    }
    if (fileName == "ui_window1.png") {
        return renderer_->loadUiWindowTexture("assets/UI_window1.png");
    }
    if (fileName == "ui_messagewindow.png") {
        return renderer_->loadUiMessageWindowTexture("assets/UI_messageWindow.png");
    }
    if (fileName == "ui_window2.png") {
        return renderer_->loadUiSubWindowTexture("assets/UI_window2.png");
    }
    if (fileName == "ui_buttons.png") {
        return renderer_->loadUiButtonTexture("assets/UI_buttons.png");
    }
    if (fileName == "ui_tubs.png") {
        return renderer_->loadUiTabTexture("assets/UI_tubs.png");
    }
    if (fileName == "ui_line.png") {
        return renderer_->loadUiLineTexture("assets/UI_line.png");
    }
    if (extension == ".otf" || extension == ".ttf") {
        return renderer_->loadTextFont("assets/fonts/craftmincho.otf");
    }
    if (fileName == "audio_manifest.tsv" ||
        parentPath.find("assets/audio") != std::string::npos ||
        extension == ".wav") {
        return audio_.reloadManifest();
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
    if (extension == ".png" &&
        fileName.size() == std::string_view("tile_1.png").size() &&
        fileName.rfind("tile_", 0) == 0 &&
        fileName[5] >= '1' &&
        fileName[5] <= '4' &&
        parentPath.find("assets/tiles") != std::string::npos) {
        renderer_->invalidateImage(changedPath);
        return true;
    }
    if (extension == ".png" &&
        fileName.rfind("op_", 0) == 0 &&
        parentPath.find("assets/opening") != std::string::npos) {
        renderer_->invalidateImage(changedPath);
        return true;
    }
    if (extension == ".png" &&
        fileName.rfind("tatie_", 0) == 0 &&
        parentPath.find("assets/taties") != std::string::npos) {
        renderer_->invalidateImage(changedPath);
        return true;
    }

    return loadAssets();
}

void App::checkAssetHotReload()
{
    if (!runtimeHotReloadEnabled_ || autoReloadBlocked_) {
        return;
    }

    const std::uint64_t now = SDL_GetTicks();
    if (now < nextAssetHotReloadPollTicks_) {
        return;
    }
    nextAssetHotReloadPollTicks_ = now + HotReloadPollIntervalMs;

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
    if (window_ == nullptr) {
        return;
    }
    if (settings_.video.windowMode == WindowMode::BorderlessFullscreen) {
        settings_.video.windowMode = WindowMode::Windowed;
    } else {
        int windowWidth = width_;
        int windowHeight = height_;
        if (SDL_GetWindowSize(window_, &windowWidth, &windowHeight)) {
            settings_.video.windowWidth = windowWidth;
            settings_.video.windowHeight = windowHeight;
        }
        settings_.video.windowMode = WindowMode::BorderlessFullscreen;
    }
    applyVideoSettings(true);
    queueSettingsSave();
}

bool App::executeSettingsDebugCommand(const std::string& normalizedCommand)
{
    const std::vector<std::string> words = splitAsciiWords(normalizedCommand);
    if (words.empty() || words[0] != "settings") {
        return false;
    }

    if (words.size() == 1 || words[1] == "show") {
        logInfo(
            "Settings: audio master=" + std::to_string(settings_.audio.masterVolume) +
            " bgm=" + std::to_string(settings_.audio.bgmVolume) +
            " se=" + std::to_string(settings_.audio.seVolume) +
            " video mode=" + windowModeName(settings_.video.windowMode) +
            " size=" + std::to_string(settings_.video.windowWidth) +
            "x" + std::to_string(settings_.video.windowHeight) +
            " vsync=" + (settings_.video.vsync ? "on" : "off") +
            " lightweight=" + (settings_.performance.lightweight ? "on" : "off") +
            " brightness=" + screenBrightnessLogValue(settings_.presentation.brightness) +
            " shake=" + screenShakeSettingName(settings_.presentation.screenShake) +
            " input_icons=" + inputIconSettingName(settings_.presentation.inputIcons) +
            " path=" + settingsStore_.path().string());
        return true;
    }

    if (words[1] == "save") {
        if (saveSettingsNow()) {
            logInfo("Settings saved: " + settingsStore_.path().string());
        }
        return true;
    }

    if (words[1] == "reload") {
        std::string error;
        GameSettings loaded;
        if (!settingsStore_.load(loaded, &error)) {
            logError("Settings reload failed: " + error);
            return true;
        }
        settings_ = sanitizeSettings(loaded);
        input_.setBindingMap(settings_.input.bindings);
        applyAudioSettings();
        applyVideoSettings(true);
        logInfo("Settings reloaded: " + settingsStore_.path().string());
        return true;
    }

    if (words[1] == "input") {
        if (words.size() == 2 || words[2] == "show") {
            if (words.size() >= 4) {
                const std::optional<InputAction> action = parseInputAction(words[3]);
                if (!action) {
                    logWarning("Unknown input action: " + words[3]);
                    return true;
                }
                logInfo(
                    std::string(inputActionName(*action)) +
                    ": " +
                    inputBindingSummary(settings_.input.bindings[inputActionIndex(*action)]));
                return true;
            }
            for (int action = 0; action < InputActionCount; ++action) {
                const auto inputAction = static_cast<InputAction>(action);
                logInfo(
                    std::string(inputActionName(inputAction)) +
                    ": " +
                    inputBindingSummary(settings_.input.bindings[action]));
            }
            return true;
        }

        if (words[2] == "reset") {
            settings_.input.bindings = defaultInputBindings();
            input_.setBindingMap(settings_.input.bindings);
            queueSettingsSave();
            logInfo("Input bindings reset to defaults.");
            return true;
        }

        if ((words[2] == "clear" || words[2] == "add") && words.size() >= 4) {
            const std::optional<InputAction> action = parseInputAction(words[3]);
            if (!action) {
                logWarning("Unknown input action: " + words[3]);
                return true;
            }

            if (words[2] == "clear") {
                settings_.input.bindings[inputActionIndex(*action)].clear();
                settings_ = sanitizeSettings(settings_);
                input_.setBindingMap(settings_.input.bindings);
                queueSettingsSave();
                logInfo(std::string(inputActionName(*action)) + " bindings cleared.");
                return true;
            }

            if (words.size() < 6) {
                logWarning(
                    "Usage: settings input add ACTION keyboard KEY | mouse BUTTON | "
                    "gamepad_button BUTTON | gamepad_axis AXIS DIRECTION [THRESHOLD]");
                return true;
            }

            const std::optional<InputBindingDevice> device = parseInputBindingDevice(words[4]);
            if (!device) {
                logWarning("Unknown input binding device: " + words[4]);
                return true;
            }

            InputBinding binding;
            binding.device = *device;
            if (*device == InputBindingDevice::Keyboard) {
                const std::optional<int> scancode = parseKeyboardScancode(words[5]);
                if (!scancode) {
                    logWarning("Unknown keyboard key: " + words[5]);
                    return true;
                }
                binding.code = *scancode;
            } else if (*device == InputBindingDevice::MouseButton) {
                const std::optional<int> button = parseMouseButton(words[5]);
                if (!button) {
                    logWarning("Unknown mouse button: " + words[5]);
                    return true;
                }
                binding.code = *button;
            } else if (*device == InputBindingDevice::GamepadButton) {
                const std::optional<int> button = parseGamepadButton(words[5]);
                if (!button) {
                    logWarning("Unknown gamepad button: " + words[5]);
                    return true;
                }
                binding.code = *button;
            } else if (*device == InputBindingDevice::GamepadAxis) {
                if (words.size() < 7) {
                    logWarning("Usage: settings input add ACTION gamepad_axis AXIS DIRECTION [THRESHOLD]");
                    return true;
                }
                const std::optional<int> axis = parseGamepadAxis(words[5]);
                const std::optional<int> direction = parseIntSetting(words[6]);
                if (!axis || !direction || *direction == 0) {
                    logWarning("Usage: settings input add ACTION gamepad_axis AXIS DIRECTION [THRESHOLD]");
                    return true;
                }
                binding.code = *axis;
                binding.direction = *direction < 0 ? -1 : 1;
                if (words.size() >= 8) {
                    binding.threshold = parseFloatSetting(words[7]).value_or(binding.threshold);
                }
            }

            auto& actionBindings = settings_.input.bindings[inputActionIndex(*action)];
            const bool alreadyBound = std::any_of(
                actionBindings.begin(),
                actionBindings.end(),
                [&](const InputBinding& existing) {
                    return inputBindingEquals(existing, binding);
                });
            if (!alreadyBound) {
                actionBindings.push_back(binding);
            }
            settings_ = sanitizeSettings(settings_);
            input_.setBindingMap(settings_.input.bindings);
            queueSettingsSave();
            logInfo(
                std::string(inputActionName(*action)) +
                ": " +
                inputBindingSummary(settings_.input.bindings[inputActionIndex(*action)]));
            return true;
        }

        logWarning(
            "Settings input usage: settings input show [ACTION], settings input reset, "
            "settings input clear ACTION, settings input add ACTION DEVICE VALUE");
        return true;
    }

    if ((words[1] == "audio" || words[1] == "volume") && words.size() == 4) {
        const std::optional<float> value = parseFloatSetting(words[3]);
        if (!value) {
            logWarning("Usage: settings audio master|bgm|se 0.0-1.0");
            return true;
        }
        if (words[2] == "master") {
            settings_.audio.masterVolume = *value;
        } else if (words[2] == "bgm") {
            settings_.audio.bgmVolume = *value;
        } else if (words[2] == "se") {
            settings_.audio.seVolume = *value;
        } else {
            logWarning("Usage: settings audio master|bgm|se 0.0-1.0");
            return true;
        }
        applyAudioSettings();
        queueSettingsSave();
        logInfo("Audio settings updated");
        return true;
    }

    if ((words[1] == "performance" || words[1] == "perf") && words.size() == 4 && words[2] == "lightweight") {
        const std::optional<bool> value = parseBoolSetting(words[3]);
        if (!value) {
            logWarning("Usage: settings performance lightweight on|off");
            return true;
        }
        settings_.performance.lightweight = *value;
        settings_ = sanitizeSettings(settings_);
        queueSettingsSave();
        logInfo(std::string("Lightweight mode: ") + (settings_.performance.lightweight ? "on" : "off"));
        return true;
    }

    if ((words[1] == "presentation" || words[1] == "display") && words.size() == 4) {
        if (words[2] == "brightness") {
            std::optional<float> value = parseFloatSetting(words[3]);
            if (!value) {
                logWarning("Usage: settings presentation brightness 0.70-1.30");
                return true;
            }
            if (*value > 2.0f) {
                *value *= 0.01f;
            }
            settings_.presentation.brightness = *value;
            settings_ = sanitizeSettings(settings_);
            queueSettingsSave();
            logInfo("Screen brightness: " + screenBrightnessLogValue(settings_.presentation.brightness));
            return true;
        }
        if (words[2] == "shake" || words[2] == "screen_shake") {
            ScreenShakeSetting value = settings_.presentation.screenShake;
            if (!parseScreenShakeSetting(words[3], value)) {
                logWarning("Usage: settings presentation shake off|low|standard");
                return true;
            }
            settings_.presentation.screenShake = value;
            settings_ = sanitizeSettings(settings_);
            queueSettingsSave();
            logInfo(std::string("Screen shake: ") + screenShakeSettingName(settings_.presentation.screenShake));
            return true;
        }
        if (words[2] == "icons" || words[2] == "input_icons") {
            InputIconSetting value = settings_.presentation.inputIcons;
            if (!parseInputIconSetting(words[3], value)) {
                logWarning("Usage: settings presentation icons auto|keyboard|gamepad");
                return true;
            }
            settings_.presentation.inputIcons = value;
            settings_ = sanitizeSettings(settings_);
            queueSettingsSave();
            logInfo(std::string("Input icons: ") + inputIconSettingName(settings_.presentation.inputIcons));
            return true;
        }
    }

    if (words[1] == "video" && words.size() >= 4 && words[2] == "vsync") {
        const std::optional<bool> value = parseBoolSetting(words[3]);
        if (!value) {
            logWarning("Usage: settings video vsync on|off");
            return true;
        }
        settings_.video.vsync = *value;
        applyVideoSettings(true);
        queueSettingsSave();
        logInfo(std::string("VSync: ") + (settings_.video.vsync ? "on" : "off"));
        return true;
    }

    if (words[1] == "video" && words.size() >= 4 && words[2] == "mode") {
        WindowMode mode = settings_.video.windowMode;
        if (!parseWindowMode(words[3], mode)) {
            logWarning("Usage: settings video mode windowed|borderless");
            return true;
        }
        if (mode == WindowMode::BorderlessFullscreen &&
            settings_.video.windowMode == WindowMode::Windowed &&
            window_ != nullptr) {
            int windowWidth = settings_.video.windowWidth;
            int windowHeight = settings_.video.windowHeight;
            if (SDL_GetWindowSize(window_, &windowWidth, &windowHeight)) {
                settings_.video.windowWidth = windowWidth;
                settings_.video.windowHeight = windowHeight;
            }
        }
        settings_.video.windowMode = mode;
        applyVideoSettings(true);
        queueSettingsSave();
        logInfo(std::string("Window mode: ") + windowModeName(settings_.video.windowMode));
        return true;
    }

    if (words[1] == "video" && words.size() >= 5 && words[2] == "size") {
        const std::optional<int> width = parseIntSetting(words[3]);
        const std::optional<int> height = parseIntSetting(words[4]);
        if (!width || !height) {
            logWarning("Usage: settings video size WIDTH HEIGHT");
            return true;
        }
        settings_.video.windowWidth = *width;
        settings_.video.windowHeight = *height;
        settings_ = sanitizeSettings(settings_);
        if (settings_.video.windowMode == WindowMode::Windowed) {
            applyVideoSettings(true);
        }
        queueSettingsSave();
        logInfo(
            "Windowed size: " +
            std::to_string(settings_.video.windowWidth) +
            "x" +
            std::to_string(settings_.video.windowHeight));
        return true;
    }

    logWarning(
        "Settings command usage: settings show|save|reload, "
        "settings audio master|bgm|se VALUE, "
        "settings video mode windowed|borderless, "
        "settings video vsync on|off, "
        "settings video size WIDTH HEIGHT, "
        "settings performance lightweight on|off, "
        "settings presentation brightness 0.70-1.30, "
        "settings presentation shake off|low|standard, "
        "settings presentation icons auto|keyboard|gamepad");
    return true;
}

void App::executeDebugCommand(const std::string& command)
{
    const std::string normalized = lowerAscii(trimAscii(command));
    if (executeSettingsDebugCommand(normalized)) {
        return;
    }
    if (normalized.rfind("autosim", 0) == 0 || normalized.rfind("auto-sim", 0) == 0) {
        if (!testPlayMode_) {
            logWarning("AutoSim commands are available only in test-play mode.");
            return;
        }
        autoSimulation_.executeCommand(normalized, game_.makeTestSnapshot());
        debugConsole_.setSliderValue("autosim_speed", autoSimulation_.speedMultiplier());
        autoSimulationStepDebtSeconds_ = 0.0f;
        return;
    }
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

void App::runAutoSimulationStep(float dt, Time& updateTime)
{
    Input effectiveInput = input_;
    const GameTestSnapshot snapshot = game_.makeTestSnapshot(autoSimulation_.snapshotOptionsForNextStep());
    autoSimulation_.update(snapshot, dt);
    while (std::optional<GameTestAction> action = autoSimulation_.consumeAction()) {
        const GameTestActionResult result = game_.applyTestAction(*action);
        autoSimulation_.recordActionResult(*action, result);
    }
    effectiveInput.applyAutomation(autoSimulation_.inputFrame());
    game_.update(effectiveInput, updateTime);
}

float App::startupLoadProgress() const
{
    constexpr float AppStepCount = 9.0f;
    switch (startupLoadStep_) {
    case StartupLoadStep::FirstFrame:
        return 0.0f / AppStepCount;
    case StartupLoadStep::InitializeAudio:
        return 1.0f / AppStepCount;
    case StartupLoadStep::LoadAudioManifest:
        return 2.0f / AppStepCount;
    case StartupLoadStep::WireGameServices:
        return 3.0f / AppStepCount;
    case StartupLoadStep::LoadAssets:
        return 4.0f / AppStepCount;
    case StartupLoadStep::BeginGameInitialize:
        return 5.0f / AppStepCount;
    case StartupLoadStep::AdvanceGameInitialize:
        return (6.0f + game_.initializeProgress()) / AppStepCount;
    case StartupLoadStep::EnableHotReload:
        return 7.0f / AppStepCount;
    case StartupLoadStep::ExecuteLaunchMode:
        return 8.0f / AppStepCount;
    case StartupLoadStep::Finish:
    case StartupLoadStep::Done:
        return 1.0f;
    }
    return 0.0f;
}

void App::renderStartupFrame()
{
    if (renderer_ == nullptr) {
        return;
    }

    renderer_->setScreenSpace();
    renderer_->clear({6, 7, 10, 255});

    const float screenW = static_cast<float>(LogicalScreenWidth);
    const float screenH = static_cast<float>(LogicalScreenHeight);

    constexpr float BarW = 280.0f;
    constexpr float TrackH = 16.0f;
    constexpr float MarginRight = 28.0f;
    constexpr float MarginBottom = 28.0f;
    const float barLeft = std::max(12.0f, screenW - MarginRight - BarW);
    const float barCenterY = std::max(20.0f, screenH - MarginBottom - TrackH * 0.5f);
    const UiRect bar{{barLeft, barCenterY - TrackH * 0.5f}, {BarW, TrackH}};
    const float progress = std::clamp(startupLoadProgress(), 0.0f, 1.0f);
    const float totalSeconds = time_.totalSeconds();
    const float pulse = 0.76f + 0.24f * std::sin(totalSeconds * 5.0f);
    UiGaugeStyle loadingGaugeStyle;
    loadingGaugeStyle.fill.start = {static_cast<unsigned char>(108.0f + 48.0f * pulse), 206, 236, 230};
    loadingGaugeStyle.fill.end = {132, 230, 250, 230};
    loadingGaugeStyle.track = {12, 16, 24, 190};
    loadingGaugeStyle.trackInner = {30, 38, 52, 220};
    loadingGaugeStyle.trackOuter = {218, 228, 244, 78};
    loadingGaugeStyle.shadow = {0, 0, 0, 105};
    loadingGaugeStyle.tick = {255, 255, 255, 32};
    loadingGaugeStyle.highlight = {255, 255, 255, 118};
    loadingGaugeStyle.capGlow = {132, 230, 250, 78};
    loadingGaugeStyle.capCore = {246, 252, 255, 225};
    loadingGaugeStyle.tickCount = 8;
    loadingGaugeStyle.shimmer = {255, 255, 255, 76};
    loadingGaugeStyle.shimmerPhase =
        std::fmod(std::max(0.0f, totalSeconds) * 116.0f, BarW + loadingGaugeStyle.shimmerWidth) /
        (BarW + loadingGaugeStyle.shimmerWidth);
    drawUiGauge(*renderer_, bar, progress, loadingGaugeStyle);

    const std::string label = "LOADING";
    const Vec2 labelSize = renderer_->measureText(label, 2);
    const Vec2 labelPos{bar.pos.x + bar.size.x - labelSize.x, bar.pos.y - labelSize.y - 9.0f};
    renderer_->drawText(labelPos + Vec2{1.0f, 1.0f}, label, {0, 0, 0, 170}, 2);
    renderer_->drawText(labelPos, label, {246, 246, 252, 230}, 2);
    renderer_->present();
}

std::filesystem::path App::screenshotDirectory() const
{
    return devSettingsRootPath() / "screenshots";
}

std::filesystem::path App::makeScreenshotPath() const
{
    const std::filesystem::path directory = screenshotDirectory();
    const std::string stem = "majo_shovel_" + screenshotTimestamp();

    for (int suffix = 0; suffix <= 999; ++suffix) {
        const std::string filename =
            suffix == 0
                ? stem + ".png"
                : stem + "_" + threeDigitSuffix(suffix) + ".png";
        const std::filesystem::path candidate = directory / filename;
        std::error_code existsError;
        if (!std::filesystem::exists(candidate, existsError)) {
            return candidate;
        }
    }

    return directory / (stem + "_999.png");
}

void App::requestScreenshot()
{
    if (!testPlayMode_ || renderer_ == nullptr) {
        return;
    }

    const std::filesystem::path path = makeScreenshotPath();
    renderer_->requestScreenshot(path);
    logInfo("Screenshot requested: " + pathToUtf8(path));
}

void App::logPendingScreenshotResult()
{
    if (renderer_ == nullptr) {
        return;
    }

    std::optional<Renderer::ScreenshotResult> result = renderer_->consumeScreenshotResult();
    if (!result) {
        return;
    }

    const std::string path = pathToUtf8(result->path);
    if (result->success) {
        logInfo("Screenshot saved: " + path);
        return;
    }

    logError("Screenshot failed: " + result->message + " (" + path + ")");
}

void App::advanceStartupLoad()
{
    switch (startupLoadStep_) {
    case StartupLoadStep::FirstFrame:
        startupStatus_ = "Starting";
        startupLoadStep_ = StartupLoadStep::InitializeAudio;
        break;
    case StartupLoadStep::InitializeAudio:
        startupStatus_ = "Initializing audio";
        if (!audio_.initialize()) {
            logWarning("Audio disabled: " + audio_.lastError());
        }
        applyAudioSettings();
        startupLoadStep_ = StartupLoadStep::LoadAudioManifest;
        break;
    case StartupLoadStep::LoadAudioManifest:
        startupStatus_ = "Loading audio manifest";
        audio_.loadManifest("assets/audio/audio_manifest.tsv");
        startupLoadStep_ = StartupLoadStep::WireGameServices;
        break;
    case StartupLoadStep::WireGameServices:
        startupStatus_ = "Wiring game services";
        game_.setAudioEngine(&audio_);
        game_.setSettingsAccessors(
            [this]() {
                return settings_;
            },
            [this](const GameSettings& settings) {
                const InputBindingMap previousBindings = settings_.input.bindings;
                settings_ = sanitizeSettings(settings);
                if (!inputBindingMapsEqual(previousBindings, settings_.input.bindings)) {
                    input_.setBindingMap(settings_.input.bindings);
                }
                applyAudioSettings();
                applyVideoSettings(true);
                queueSettingsSave();
            });
        game_.setInputBindingAccessors(
            [this]() {
                return settings_.input.bindings;
            },
            [this](const InputBindingMap& bindings) {
                settings_.input.bindings = sanitizeInputBindings(bindings);
                input_.setBindingMap(settings_.input.bindings);
                queueSettingsSave();
            });
        startupLoadStep_ = StartupLoadStep::LoadAssets;
        break;
    case StartupLoadStep::LoadAssets:
        startupStatus_ = "Loading fixed assets";
        loadAssets();
        startupLoadStep_ = StartupLoadStep::BeginGameInitialize;
        break;
    case StartupLoadStep::BeginGameInitialize:
        startupStatus_ = "Preparing game data";
        game_.beginInitialize(LogicalScreenWidth, LogicalScreenHeight, testPlayMode_);
        startupLoadStep_ = StartupLoadStep::AdvanceGameInitialize;
        break;
    case StartupLoadStep::AdvanceGameInitialize:
        startupStatus_ = game_.initializeStatusText();
        if (game_.advanceInitialize()) {
            startupLoadStep_ = StartupLoadStep::EnableHotReload;
            startupStatus_ = "Finalizing runtime";
        }
        break;
    case StartupLoadStep::EnableHotReload:
        startupStatus_ = "Configuring hot reload";
        setRuntimeHotReloadEnabled(testPlayMode_ && !autoReloadBlocked_);
        startupLoadStep_ = StartupLoadStep::ExecuteLaunchMode;
        break;
    case StartupLoadStep::ExecuteLaunchMode:
        startupStatus_ = "Applying launch mode";
        if (testPlayMode_ && !startupLaunchModeCommand_.empty()) {
            game_.executeDebugCommand(startupLaunchModeCommand_);
        }
        startupLoadStep_ = StartupLoadStep::Finish;
        break;
    case StartupLoadStep::Finish:
        startupStatus_ = "Ready";
        time_.reset();
        startupLoadStep_ = StartupLoadStep::Done;
        startupLoadActive_ = false;
        break;
    case StartupLoadStep::Done:
        startupLoadActive_ = false;
        break;
    }
}

void App::run()
{
    while (running_) {
        input_.beginFrame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            SDL_Event renderEvent = event;
            if (renderer_ != nullptr) {
                renderer_->convertEventToRenderCoordinates(renderEvent);
            }
            const bool gameConsumedEvent = !startupLoadActive_ && game_.handleEvent(renderEvent);
            if (!gameConsumedEvent) {
                input_.handleEvent(renderEvent);
            }
            if (!gameConsumedEvent && testPlayMode_ && !startupLoadActive_ &&
                event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                event.key.scancode == SDL_SCANCODE_PRINTSCREEN) {
                requestScreenshot();
            }
            if (!gameConsumedEvent && testPlayMode_ && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_F3) {
                const bool autosimRunning =
                    autoSimulation_.state() == autosim::AutoSimulationState::Running ||
                    autoSimulation_.state() == autosim::AutoSimulationState::Paused;
                autoSimulation_.executeCommand(
                    autosimRunning ? "autosim stop" : "autosim start",
                    game_.makeTestSnapshot());
            }
            if (!gameConsumedEvent && event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_F4) {
                toggleFullscreen();
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                width_ = event.window.data1;
                height_ = event.window.data2;
                if (settings_.video.windowMode == WindowMode::Windowed) {
                    settings_.video.windowWidth = width_;
                    settings_.video.windowHeight = height_;
                    queueSettingsSave();
                }
                if (!startupLoadActive_) {
                    game_.resize(LogicalScreenWidth, LogicalScreenHeight);
                }
            }
        }
        input_.update(renderer_);
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
        if (startupLoadActive_) {
            if (!running_) {
                continue;
            }
            time_.tick();
            updateSettingsSave(time_.deltaSeconds());
            audio_.update(time_.deltaSeconds());
            advanceStartupLoad();
            if (running_ && startupLoadActive_) {
                renderStartupFrame();
            }
            continue;
        }
        if (testPlayMode_ && input_.toggleAutoReloadBlockPressed()) {
            const bool blocked = !loadDevAutoReloadBlocked();
            std::string error;
            if (saveDevAutoReloadBlocked(blocked, error)) {
                autoReloadBlocked_ = blocked;
                game_.setAutoReloadBlocked(blocked);
                setRuntimeHotReloadEnabled(testPlayMode_ && !blocked);
                logInfo(std::string("Auto reload block: ") + (blocked ? "ON (F2)" : "OFF (F2)"));
            } else {
                logError("Auto reload block toggle failed: " + error);
            }
        }
        if (testPlayMode_ && input_.testFreezePressed()) {
            testFreezePaused_ = !testFreezePaused_;
            if (testFreezePaused_) {
                frozenTime_ = time_;
                logInfo("Test freeze: PAUSED (F7)");
            } else {
                logInfo("Test freeze: RESUMED (F7)");
            }
        }
        if (testPlayMode_) {
            while (std::optional<std::string> command = debugConsole_.pollCommand()) {
                executeDebugCommand(*command);
            }
        }

        checkAssetHotReload();
        time_.tick();
        updateSettingsSave(time_.deltaSeconds());
        if (!testFreezePaused_) {
            audio_.update(time_.deltaSeconds());
            if (testPlayMode_ && autoSimulation_.state() != autosim::AutoSimulationState::Idle) {
                if (!autoSimulationTimeActive_) {
                    autoSimulationTime_ = time_;
                    autoSimulationStepDebtSeconds_ = 0.0f;
                    autoSimulationTimeActive_ = true;
                }

                int simulationSteps = 0;
                if (autoSimulation_.state() == autosim::AutoSimulationState::Running &&
                    autoSimulation_.speedMultiplier() > 1) {
                    autoSimulationStepDebtSeconds_ = std::min(
                        AutoSimulationMaxDebtSeconds,
                        autoSimulationStepDebtSeconds_ +
                            time_.deltaSeconds() * static_cast<float>(autoSimulation_.speedMultiplier()));

                    while (autoSimulationStepDebtSeconds_ >= AutoSimulationFixedStepSeconds &&
                        simulationSteps < AutoSimulationMaxStepsPerFrame &&
                        autoSimulation_.state() != autosim::AutoSimulationState::Idle) {
                        autoSimulationTime_.advanceSimulation(AutoSimulationFixedStepSeconds);
                        runAutoSimulationStep(AutoSimulationFixedStepSeconds, autoSimulationTime_);
                        autoSimulationStepDebtSeconds_ -= AutoSimulationFixedStepSeconds;
                        ++simulationSteps;
                        if (game_.quitRequested()) {
                            running_ = false;
                            break;
                        }
                    }
                } else {
                    autoSimulationStepDebtSeconds_ = 0.0f;
                    autoSimulationTime_.advanceSimulation(time_.deltaSeconds());
                    runAutoSimulationStep(time_.deltaSeconds(), autoSimulationTime_);
                    simulationSteps = 1;
                }
                autoSimulation_.setSimulationStepsLastFrame(simulationSteps);
            } else {
                autoSimulationTimeActive_ = false;
                autoSimulationStepDebtSeconds_ = 0.0f;
                Input effectiveInput = input_;
                game_.update(effectiveInput, time_);
            }
            const bool autoSimulationOverlayActive =
                testPlayMode_ && autoSimulation_.state() != autosim::AutoSimulationState::Idle;
            game_.setAutoSimulationIntentOverlay(
                autoSimulationOverlayActive,
                autoSimulation_.intentHistory());
            game_.setAutoSimulationDebugOverlay(
                autoSimulationOverlayActive,
                autoSimulation_.debugSnapshot());
            if (game_.quitRequested()) {
                running_ = false;
            }
        }
        if (testFreezePaused_) {
            const bool autoSimulationOverlayActive =
                testPlayMode_ && autoSimulation_.state() != autosim::AutoSimulationState::Idle;
            game_.setAutoSimulationIntentOverlay(
                autoSimulationOverlayActive,
                autoSimulation_.intentHistory());
            game_.setAutoSimulationDebugOverlay(
                autoSimulationOverlayActive,
                autoSimulation_.debugSnapshot());
        }
        const Time& renderTime =
            !testFreezePaused_ && autoSimulationTimeActive_
                ? autoSimulationTime_
                : (testFreezePaused_ ? frozenTime_ : time_);
        if (renderer_ != nullptr) {
            renderer_->setScreenBrightness(screenBrightnessMultiplier(settings_.presentation.brightness));
        }
        setInputHelpContext(&input_);
        setInputHelpDeviceMode(inputHelpDeviceModeForSetting(settings_.presentation.inputIcons));
        game_.render(*renderer_, renderTime);
        logPendingScreenshotResult();
    }
}

}
