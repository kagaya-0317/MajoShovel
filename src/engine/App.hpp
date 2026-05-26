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
    void setRuntimeHotReloadEnabled(bool enabled);
    void checkAssetHotReload();
    bool reloadAssetForPath(const std::string& changedPath);
    void applyAudioSettings();
    void applyVideoSettings(bool notifyGameResize);
    void queueSettingsSave();
    void updateSettingsSave(float dt);
    bool saveSettingsNow();
    void toggleFullscreen();
    bool executeSettingsDebugCommand(const std::string& normalizedCommand);
    void executeDebugCommand(const std::string& command);
    void runAutoSimulationStep(float dt, Time& updateTime);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdlRenderer_ = nullptr;
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
    bool autoReloadBlocked_ = false;
    bool runtimeHotReloadEnabled_ = false;
    std::uint64_t nextAssetHotReloadPollTicks_ = 0;
    bool testFreezePaused_ = false;
    bool restartRequested_ = false;
    bool settingsSavePending_ = false;
    bool autoSimulationTimeActive_ = false;
    float settingsSaveDelaySeconds_ = 0.0f;
    float autoSimulationStepDebtSeconds_ = 0.0f;
    int width_ = 1280;
    int height_ = 720;
};

}
