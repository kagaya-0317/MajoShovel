#pragma once

#include "engine/Audio.hpp"
#include "engine/FileWatcher.hpp"
#include "engine/Input.hpp"
#include "engine/Renderer.hpp"
#include "engine/Settings.hpp"
#include "engine/Time.hpp"
#include "debug/DebugConsole.hpp"
#include "devtools/autosim/AutoSimulationController.hpp"
#include "game/Game.hpp"
#include <SDL3/SDL.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace majo {

class App {
public:
    App();
    ~App();

    bool initialize(
        const char* title,
        int width,
        int height,
        bool testPlayMode = false,
        bool devAutoReloadMode = false);
    void run();
    bool restartRequested() const { return restartRequested_; }

private:
    bool loadAssets();
    bool loadGameCursor(std::string_view path);
    void unloadGameCursor();
    void updateGameCursorPressed(bool pressed);
    void configureAssetWatcher();
    void setRuntimeHotReloadEnabled(bool enabled);
    void checkAssetHotReload();
    bool reloadAssetForPath(const std::string& changedPath);
    void checkDevBuildStatus();
    void updateDevBuildNotice(float dt);
    void updateWindowTitle();
    void requestRestart();
    void applyAudioSettings();
    void applyVideoSettings(bool notifyGameResize);
    void queueSettingsSave();
    void updateSettingsSave(float dt);
    bool saveSettingsNow();
    void toggleFullscreen();
    bool executeSettingsDebugCommand(const std::string& normalizedCommand);
    void executeDebugCommand(const std::string& command);
    void runAutoSimulationStep(float dt, Time& updateTime);
    void advanceStartupLoad();
    void renderStartupFrame();
    float startupLoadProgress() const;
    const char* startupLoadStepName() const;
    std::string crashContextSummary() const;
    void requestScreenshot();
    void logPendingScreenshotResult();
    std::filesystem::path screenshotDirectory() const;
    std::filesystem::path makeScreenshotPath() const;

    enum class StartupLoadStep {
        FirstFrame,
        InitializeAudio,
        LoadAudioManifest,
        WireGameServices,
        LoadAssets,
        BeginGameInitialize,
        AdvanceGameInitialize,
        EnableHotReload,
        ExecuteLaunchMode,
        Finish,
        Done,
    };

    enum class DevBuildState {
        None,
        Ready,
        Failed,
    };

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdlRenderer_ = nullptr;
    SDL_Cursor* gameCursor_ = nullptr;
    SDL_Cursor* gameCursorPressed_ = nullptr;
    Renderer* renderer_ = nullptr;
    AudioEngine audio_;
    SettingsStore settingsStore_;
    GameSettings settings_;
    Input input_;
    Time time_;
    Time frozenTime_;
    Time autoSimulationTime_;
    FileWatcher assetWatcher_;
    DebugConsole debugConsole_;
    Game game_;
    autosim::AutoSimulationController autoSimulation_;
    bool running_ = false;
    bool testPlayMode_ = false;
    bool devAutoReloadMode_ = false;
    bool autoReloadBlocked_ = false;
    bool runtimeHotReloadEnabled_ = false;
    DevBuildState devBuildState_ = DevBuildState::None;
    bool devBuildNoticeFailed_ = false;
    std::uint64_t nextAssetHotReloadPollTicks_ = 0;
    std::uint64_t nextDevBuildStatusPollTicks_ = 0;
    bool gameCursorPressedActive_ = false;
    bool testFreezePaused_ = false;
    bool restartRequested_ = false;
    bool settingsSavePending_ = false;
    bool autoSimulationTimeActive_ = false;
    bool startupLoadActive_ = false;
    StartupLoadStep startupLoadStep_ = StartupLoadStep::Done;
    std::string startupStatus_;
    std::string startupLaunchModeCommand_;
    std::string baseWindowTitle_;
    std::string devBuildStatusToken_;
    float settingsSaveDelaySeconds_ = 0.0f;
    float autoSimulationStepDebtSeconds_ = 0.0f;
    float devBuildNoticeTimer_ = 0.0f;
    int width_ = 1280;
    int height_ = 720;
};

std::unique_ptr<App> createApp();

}
