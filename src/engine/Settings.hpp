#pragma once

#include "engine/InputBinding.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace majo {

enum class WindowMode {
    Windowed,
    BorderlessFullscreen,
};

inline constexpr float MinScreenBrightness = 0.70f;
inline constexpr float MaxScreenBrightness = 1.30f;
inline constexpr float DefaultScreenBrightness = 1.0f;

enum class ScreenShakeSetting {
    Off,
    Low,
    Standard,
};

enum class InputIconSetting {
    Auto,
    KeyboardMouse,
    Gamepad,
};

struct AudioSettings {
    float masterVolume = 1.0f;
    float bgmVolume = 1.0f;
    float seVolume = 1.0f;
};

struct VideoSettings {
    WindowMode windowMode = WindowMode::Windowed;
    int windowWidth = 1280;
    int windowHeight = 720;
    bool vsync = true;
};

struct PerformanceSettings {
    bool lightweight = false;
};

struct PresentationSettings {
    float brightness = DefaultScreenBrightness;
    ScreenShakeSetting screenShake = ScreenShakeSetting::Standard;
    InputIconSetting inputIcons = InputIconSetting::Auto;
};

struct InputSettings {
    InputBindingMap bindings = defaultInputBindings();
};

struct GameSettings {
    int version = 1;
    AudioSettings audio;
    VideoSettings video;
    PerformanceSettings performance;
    PresentationSettings presentation;
    InputSettings input;
};

const char* windowModeName(WindowMode mode);
bool parseWindowMode(std::string_view text, WindowMode& outMode);
const char* screenShakeSettingName(ScreenShakeSetting setting);
bool parseScreenShakeSetting(std::string_view text, ScreenShakeSetting& outSetting);
const char* inputIconSettingName(InputIconSetting setting);
bool parseInputIconSetting(std::string_view text, InputIconSetting& outSetting);
GameSettings sanitizeSettings(GameSettings settings);

class SettingsStore {
public:
    SettingsStore();
    explicit SettingsStore(std::filesystem::path path);

    static std::filesystem::path defaultPath();

    const std::filesystem::path& path() const { return path_; }
    bool exists() const;
    bool load(GameSettings& outSettings, std::string* outError = nullptr) const;
    bool save(const GameSettings& settings, std::string* outError = nullptr) const;

private:
    std::filesystem::path path_;
};

}
