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

struct InputSettings {
    InputBindingMap bindings = defaultInputBindings();
};

struct GameSettings {
    int version = 1;
    AudioSettings audio;
    VideoSettings video;
    PerformanceSettings performance;
    InputSettings input;
};

const char* windowModeName(WindowMode mode);
bool parseWindowMode(std::string_view text, WindowMode& outMode);
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
