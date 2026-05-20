#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace majo {

enum class AudioCueType {
    Bgm,
    Se,
};

struct AudioCueOptions {
    float volume = 1.0f;
    bool loop = false;
    float cooldownSeconds = 0.0f;
};

struct AudioEngineSettings {
    int sampleRate = 48000;
    int channels = 2;
    int maxSeVoices = 32;
    bool enabled = true;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    AudioEngine(AudioEngine&&) noexcept;
    AudioEngine& operator=(AudioEngine&&) noexcept;

    bool initialize(const AudioEngineSettings& settings = {});
    void shutdown();
    void update(float dt);

    bool available() const;
    std::string lastError() const;

    bool loadManifest(const std::filesystem::path& path);
    bool reloadManifest();
    void clearCues();
    bool loadCue(
        std::string id,
        AudioCueType type,
        const std::filesystem::path& path,
        AudioCueOptions options = {});

    void playBgm(std::string_view id, float fadeSeconds = 0.0f, bool restart = false);
    void stopBgm(float fadeSeconds = 0.0f);
    void playSe(std::string_view id, float volumeScale = 1.0f);
    void stopAll();

    void setMasterVolume(float volume);
    void setBgmVolume(float volume);
    void setSeVolume(float volume);
    float masterVolume() const;
    float bgmVolume() const;
    float seVolume() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
